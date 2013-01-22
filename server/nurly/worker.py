import multiprocessing
import os
import signal
import socket
import time

from nurly.util import NurlyStatus, NurlyResult

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
                rval.body = 'no handler found to process %s request for %s\n' % (env['REQUEST_METHOD'], env['PATH_INFO'])
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

    def reap_children(self, *args):
        os.waitpid(-1, os.WNOHANG)

    def serve_forever(self, parent, unused_pipes):
        signal.signal(signal.SIGCHLD, self.reap_children)

        try:
            for pipe in unused_pipes:
                pipe.close()

            self.parent = parent
            self.server.serve_forever(poll_interval=None)
        except KeyboardInterrupt:
            return
