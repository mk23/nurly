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

from version import VERSION

STATUS_IDLE, STATUS_BUSY, STATUS_STOP = 'IBS'

class HTTPError(Exception):
    code = 500
    line = 'Internal Server Error'
    body = 'internal server error\r\n'
    def __init__(self, exception=None):
        if exception is not None:
            self.body = ''.join(traceback.format_exception(*exception))

class HTTPNotFoundError(HTTPError):
    code = 404
    line = 'Not Found'
    body = 'not found\r\n'
    def __init__(self, item=None):
        if item is not None:
            self.body = '%s %s' % (item, self.body)

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

    def server_bind(self):
        self.socket.setsockopt(socket.SOL_TCP, socket.TCP_DEFER_ACCEPT, True)
        self.socket.setsockopt(socket.SOL_TCP, socket.TCP_QUICKACK,     True)
        BaseHTTPServer.HTTPServer.server_bind(self)

    def create_server(self, workers=multiprocessing.cpu_count(), version=None):
        Handler.server_version += ' %s/%s' % (os.path.basename(sys.modules[__name__].__file__), VERSION)
        if version is not None:
            Handler.server_version += ' %s' % version

        self.unused_pipes = []
        self.worker_state = dict(self.create_worker() for i in xrange(workers))

        self.handle_events()

    def create_worker(self):
        parent_pipe, worker_pipe = multiprocessing.Pipe(duplex=True)

        worker_proc = Worker(self, worker_pipe, parent_pipe, self.unused_pipes)
        worker_proc.status = STATUS_IDLE
        worker_pipe.close()

        return worker_proc.pid, worker_proc

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
            for patt, func in self.request_handlers[method].items():
                find = patt.match(urllib.unquote(self.nurly_path))
                if find:
                    response = Response(head=self.default_headers)
                    func(self, response, *(find.groups()))
                    break
            else:
                raise HTTPNotFoundError(self.path)

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

parser = argparse.ArgumentParser(description='python http server library')
parser.add_argument('-p', '--server-port', default=1123, type=int,
                    help='local listening port')
parser.add_argument('-n', '--num-workers', default=multiprocessing.cpu_count(), type=int,
                    help='number of worker processes')

if __name__ == '__main__':
    args = parser.parse_args()

    Server(('', args.server_port), Handler).create_server(args.num_workers)
