#!/usr/bin/env python2.7

import argparse
import multiprocessing
import os
import select
import socket
import time
import traceback
import wsgiref.simple_server


class WSGIServer(wsgiref.simple_server.WSGIServer):
    def __init__(self, *args):
        self.allow_reuse_address = True
        wsgiref.simple_server.WSGIServer.__init__(self, *args)

    def server_bind(self):
        self.socket.setsockopt(socket.SOL_TCP, socket.TCP_DEFER_ACCEPT, True)
        self.socket.setsockopt(socket.SOL_TCP, socket.TCP_QUICKACK,     True)
        wsgiref.simple_server.WSGIServer.server_bind(self)


class NurlyResult():
    def __init__(self, code='200 OK', head=None, body=''):
        self.head = {} if type(head) != dict else head
        self.body = body
        self.code = code


class NurlyStatus():
    ST_IDLE = 0
    ST_BUSY = 1
    ST_STOP = 2

    ST_MAP = {
        ST_IDLE: 'IDLE',
        ST_BUSY: 'BUSY',
        ST_STOP: 'STOP',
    }

    def __init__(self, proc, pipe):
        self.proc = proc
        self.pipe = pipe

        self.count = 0
        self.state = NurlyStatus.ST_IDLE

    @staticmethod
    def label(code, short=False):
        return NurlyStatus.ST_MAP[code] if not short else NurlyStatus.ST_MAP[code][0]


class NurlyAction():
    def __init__(self, func, path='/', verb='GET'):
        self.func = func
        self.path = path
        self.verb = verb

    def __call__(self, env, res, parent):
        if env['REQUEST_METHOD'] == self.verb and env['PATH_INFO'].startswith(self.path):
            try:
                self.func(env, res, parent)
            except:
                res.code = '500 Server Error'
                res.body = traceback.format_exc()
            return True

        return False



class NurlyWorker():
    def __init__(self, parent, server, unused):
        self.server = server
        self.server.set_app(self.application)

        self.worker = multiprocessing.Process(target=self.serve_forever, args=(parent, unused))
        self.worker.start()

        self.pid = self.worker.pid

    def application(self, env, rsp):
        self.call_parent('set_status', NurlyStatus.ST_BUSY)

        init = time.time() * 1000
        rval = NurlyResult(head={'Content-type': 'text/plain'})

        allow = True
        if env['nurly.allow_hosts']:
            addr = set([env['REMOTE_ADDR']])
            try:
                host = socket.gethostbyaddr(env['REMOTE_ADDR'])[0]
                addr.add(host)
                addr.add(host.split('.')[0])
            except:
                pass

            allow = len(list(True for host in addr if host in env['nurly.allow_hosts'])) > 0

        if allow:
            for func in env['nurly.action_list']:
                if func(env, rval, self.call_parent):
                    break
            else:
                rval.code = '404 Not Found'
                rval.body = 'No handler found to process %s request for %s\n' % (env['REQUEST_METHOD'], env['PATH_INFO'])
        else:
            rval.code = '403 Forbidden'
            rval.body = '%s is forbidden access to process %s request for %s\n' % (env['REMOTE_ADDR'], env['REQUEST_METHOD'], env['PATH_INFO'])

        rval.head['X-Nurly-Pid']  = str(os.getpid())
        rval.head['X-Nurly-Time'] = '%.02fms' % (time.time() * 1000 - init)

        self.call_parent('set_status', NurlyStatus.ST_IDLE)

        rsp(rval.code, sorted(rval.head.items()))
        return [rval.body] if type(rval.body) in (str, unicode) else rval.body

    def call_parent(self, func, args=[], recv=False):
        if hasattr(self, 'parent'):
            if type(args) not in (list, tuple):
                args = [args]

            self.parent.send( (os.getpid(), func, args) )
            if recv:
                return self.parent.recv()

    def serve_forever(self, parent, unused_pipes):
        try:
            for pipe in unused_pipes:
                pipe.close()

            self.parent = parent
            self.server.serve_forever(poll_interval=None)
        except KeyboardInterrupt:
            return


class NurlyServer():
    def __init__(self, allow_hosts, **kwargs):
        self.environ = dict(('nurly.%s' % k, v) for k, v in kwargs.items())
        self.environ['nurly.allow_hosts'] = allow_hosts
        self.environ['nurly.action_list'] = []

    def create_action(self, func, **kwargs):
        self.environ['nurly.action_list'].append(NurlyAction(func=func, **kwargs))

    def simple_server(self, server_port, num_workers):
        self.unused = []
        self.server = wsgiref.simple_server.make_server('', server_port, None, server_class=WSGIServer)
        self.server.base_environ.update(self.environ)

        self.states = dict(self.create_worker() for i in xrange(num_workers))

        self.handle_events()

    def create_worker(self):
        prnt, chld = multiprocessing.Pipe(duplex=True)
        self.unused.append(prnt)

        proc = NurlyWorker(chld, self.server, self.unused)
        chld.close()

        return proc.pid, NurlyStatus(proc, prnt)

    def worker_set_status(self, pid, state):
        self.states[pid].state = state

    def worker_get_status(self, pid):
        self.states[pid].pipe.send(list(i.state for i in self.states.values()))

    def handle_events(self):
        while True:
            for pipe in select.select([i.pipe for i in self.states.values()], [], [])[0]:
                try:
                    pid, func, args = pipe.recv()
                    getattr(self, 'worker_%s' % func, lambda *a: True)(pid, *args)
                except:
                    traceback.print_exc()

def server_status(env, res, parent):
    stat = parent('get_status', recv=True)
    res.body = 'Busy Workers: %d\nIdle Workers: %d\nScoreboard:   %s\n' % (
        len(list(True for i in stat if i != NurlyStatus.ST_IDLE)),
        len(list(True for i in stat if i == NurlyStatus.ST_IDLE)),
        ''.join(NurlyStatus.label(i, True) for i in stat)
    )

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='nagios service check handler', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('-a', '--allow-hosts', default=[], nargs='+',
                        help='list of allowed nagios hosts')
    parser.add_argument('-p', '--plugin-path', default='/usr/lib/nagios/plugins',
                        help='path to plugin directory')
    parser.add_argument('-s', '--server-port', default=1123, type=int,
                        help='server listen port')
    parser.add_argument('-n', '--num-workers', default=multiprocessing.cpu_count(), type=int,
                        help='number of handler processes')
    args = parser.parse_args()

    server = NurlyServer(args.allow_hosts, plugin_path=args.plugin_path)

    server.create_action(server_status, path='/server-status')
    server.simple_server(args.server_port, args.num_workers)
