#!/usr/bin/env python2.7

import BaseHTTPServer

import argparse
import collections
import multiprocessing
import os
import re
import select
import signal
import socket
import sys
import time
import traceback
import urllib
import urlparse

VERSION = '1.2.4'

STATUS_IDLE, STATUS_BUSY, STATUS_STOP = 'IBS'

class HTTPError(Exception):
    code = 500
    def __init__(self, exception=None):
        self.line, self.body = Handler.responses[self.code]
        if exception is not None:
            self.body = ''.join(traceback.format_exception(*exception))

class HTTPForbiddenError(HTTPError):
    code = 403

class HTTPNotFoundError(HTTPError):
    code = 404

class Response():
    def __init__(self, **kwargs):
        self.__dict__.update({
            'code': 200,
            'line': 'OK',
            'head': collections.OrderedDict(),
            'body': '',
        })

        for key, val in kwargs.items():
            if key == 'head':
                self.head.update(kwargs['head'])
            else:
                setattr(self, key, val)

    def __setattr__(self, key, val):
        if key.startswith('_') or key == 'head' or key not in self.__dict__:
            raise AttributeError('setting attribute %s is not supported' % key)
        if key == 'code' and type(val) != int:
            raise AttributeError('setting attribute %s requires an integer' % key)

        self.__dict__[key] = val

class Worker(multiprocessing.Process):
    def __init__(self, server, worker_pipe, parent_pipe, unused_pipes):
        self.channel = parent_pipe
        self.server = server
        self.fileno = self.channel.fileno

        unused_pipes.append(parent_pipe)

        multiprocessing.Process.__init__(self, target=self.start_server, args=(worker_pipe, unused_pipes))
        self.start()

    def reap_children(self, *args):
        os.waitpid(-1, os.WNOHANG)

    def send_channel(self, func, args=[], recv=False):
        if type(args) not in (list, tuple):
            args = [args]
        self.channel.send( (func, args) )

        if recv:
            return self.channel.recv()

    def start_server(self, worker_pipe, unused_pipes):
        signal.signal(signal.SIGTERM, signal.SIG_DFL)
        signal.signal(signal.SIGCHLD, self.reap_children)

        self.channel = worker_pipe
        self.fileno = self.channel.fileno

        try:
            for pipe in unused_pipes:
                pipe.close()

            self.server.worker = self
            self.server.serve_forever(poll_interval=None)
        except KeyboardInterrupt:
            return

class Server(BaseHTTPServer.HTTPServer):
    allow_reuse_address = True

    def __init__(self, server_port, server_addr='', num_workers=1, allowed_ips=None, disable_log=False, version=None, **kwargs):
        BaseHTTPServer.HTTPServer.__init__(self, (server_addr, server_port), Handler)

        self.unused_pipes = []
        self.worker_state = {}
        self.ip_whitelist = set(socket.gethostbyname(i) for i in allowed_ips)

        if disable_log:
            Handler.log_message = lambda *args: True

        Handler.extra_settings  = collections.namedtuple('extra_settings', kwargs.keys())(*kwargs.values())
        Handler.server_version += ' %s/%s' % (os.path.basename(sys.modules[__name__].__file__), VERSION)
        if version is not None:
            Handler.server_version += ' %s' % version

        self.create_server(num_workers)

    def server_bind(self):
        self.socket.setsockopt(socket.SOL_TCP, socket.TCP_DEFER_ACCEPT, True)
        self.socket.setsockopt(socket.SOL_TCP, socket.TCP_QUICKACK,     True)
        BaseHTTPServer.HTTPServer.server_bind(self)

    def create_server(self, workers):
        signal.signal(signal.SIGTERM, self.clean_workers)

        self.worker_state.update(self.create_worker() for i in xrange(workers))
        self.handle_events()

    def create_worker(self):
        parent_pipe, worker_pipe = multiprocessing.Pipe(duplex=True)

        worker_proc = Worker(self, worker_pipe, parent_pipe, self.unused_pipes)
        worker_proc.status = STATUS_IDLE
        worker_pipe.close()

        return worker_proc.pid, worker_proc

    def clean_workers(self, *args):
        for pid in self.worker_state.keys():
            os.kill(pid, signal.SIGTERM)

        sys.exit(0)

    def ch_set_status(self, worker, status):
        worker.status = status

    def ch_get_status(self, worker):
        worker.channel.send(list(i.status for i in self.worker_state.values()))

    def handle_events(self):
        while True:
            for worker in select.select(self.worker_state.values(), [], [])[0]:
                try:
                    func, args = worker.channel.recv()
                    getattr(self, 'ch_%s' % func, lambda *a: True)(worker, *args)
                except EOFError:
                    worker.join()
                    del self.worker_state[worker.pid]

                    pid, proc = self.create_worker()
                    self.worker_state[pid] = proc
                except Exception:
                    traceback.print_exc()


class Handler(BaseHTTPServer.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    request_handlers = {
        'GET': collections.OrderedDict()
    }

    default_headers = {
        'Connection': 'close',
    }

    def route_request(self, method):
        self.start_time  = time.time() * 1000

        self.server.worker.send_channel('set_status', STATUS_BUSY)

        self.nurly_url   = urlparse.urlparse(self.path)
        self.nurly_path  = urllib.unquote(self.nurly_url.path)
        self.nurly_query = urlparse.parse_qs(self.nurly_url.query)

        response = Response(head=self.default_headers)

        try:
            if self.server.ip_whitelist and self.client_address[0] not in self.server.ip_whitelist:
                raise HTTPForbiddenError

            for patt, func in self.request_handlers[method].items():
                find = patt.match(self.nurly_path)
                if find:
                    response = Response(head=self.default_headers)
                    func(self, response, *(find.groups()))
                    break
            else:
                raise HTTPNotFoundError

        except Exception as e:
            if not isinstance(e, HTTPError):
                e = HTTPError(sys.exc_info())
            response.code = e.code
            response.line = e.line
            response.body = e.body

        self.send_response(response.code, response.line)
        for key, val in response.head.items():
            if key.lower() != 'content-length':
                self.send_header(key, val)
        self.send_header('Content-length', len(response.body))
        self.send_header('X-Handler-Time', '%.02fms' % (time.time() * 1000 - self.start_time))
        self.send_header('X-Handler-Pid', os.getpid())
        self.end_headers()
        self.wfile.write(response.body)

        self.server.worker.send_channel('set_status', STATUS_IDLE)

    def do_GET(self):
        self.route_request('GET')

def GET(path=None):
    def wrapper(func):
        patt = r'/%s(?:/|$)' % (path.strip('/') if path is not None else func.func_name.replace('_', '-'))
        Handler.request_handlers['GET'][re.compile(patt)] = func
    return wrapper


@GET()
def server_status(req, res):
    status = req.server.worker.send_channel('get_status', recv=True)
    res.body = 'Busy Workers: %d\nIdle Workers: %d\nScoreboard:   %s\n' % (
        status.count(STATUS_BUSY), status.count(STATUS_IDLE), ''.join(status)
    )

@GET()
def server_version(req, res):
    res.body = req.server_version + '\r\n'

parser = argparse.ArgumentParser(description='python http server library', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument('-a', '--server-addr', default='',
                    help='local listening interface')
parser.add_argument('-p', '--server-port', default=1123, type=int,
                    help='local listening port')
parser.add_argument('-n', '--num-workers', default=multiprocessing.cpu_count(), type=int,
                    help='number of worker processes')
parser.add_argument('-i', '--allowed-ips', default=[], nargs='+',
                    help='allowed host whitelist, permits all when empty')
parser.add_argument('-q', '--disable-log', default=False, action='store_true',
                    help='disable access logs')

if __name__ == '__main__':
    args = parser.parse_args()

    Server(**vars(args))
