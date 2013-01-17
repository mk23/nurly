#!/usr/bin/env python2.7

import argparse
import multiprocessing
import os
import select
import socket
import wsgiref.simple_server

class WSGIServer(wsgiref.simple_server.WSGIServer):
    def __init__(self, *args):
        self.allow_reuse_address = True
        wsgiref.simple_server.WSGIServer.__init__(self, *args)

    def server_bind(self):
        print 'setting tcp options'
        self.socket.setsockopt(socket.SOL_TCP, socket.TCP_DEFER_ACCEPT, True)
        self.socket.setsockopt(socket.SOL_TCP, socket.TCP_QUICKACK,     True)
        wsgiref.simple_server.WSGIServer.server_bind(self)


class NurlyServer():
    def __init__(self, allow_hosts, plugin_path):
        self.allow_hosts = allow_hosts
        self.plugin_path = plugin_path

    def wsgi_app(self, env, rsp):
        self.worker_action('set_status', 'B')

        if env['PATH_INFO'] == '/server-status':
            stat = self.worker_action('get_status', recv=True)
            body = 'Busy Workers: %d\nIdle Workers: %d\nScoreboard:   %s\n' % (
                len(list(True for i in stat if i != 'I')),
                len(list(True for i in stat if i == 'I')),
                ''.join(stat)
            )
        else:
            body = 'handled by %d\n' % os.getpid()

        rsp('200 OK', [])

        self.worker_action('set_status', 'I')
        return [body]

    def worker_action(self, func, args=[], recv=False):
        if hasattr(self, 'pipe'):
            if type(args) not in (list, tuple):
                args = [args]

            self.pipe.send( (self.name, func, args) )
            if recv:
                return self.pipe.recv()

    def action_set_status(self, name, stat):
        self.workers[name].stat = stat

    def action_get_status(self, name):
        self.workers[name].pipe.send(list(i.stat for i in self.workers))

    def simple_server(self, server_port, num_workers):
        self.httpsrv = wsgiref.simple_server.make_server('', server_port, self.wsgi_app, server_class=WSGIServer)
        self.workers = [self.create_worker(i) for i in xrange(num_workers)]

        while True:
            for pipe in select.select([i.pipe for i in self.workers], [], [])[0]:
                try:
                    name, func, args = pipe.recv()
                    getattr(self, 'action_%s' % func, lambda *a: True)(name, *args)
                except:
                    pass

    def create_worker(self, name):
        p, c = multiprocessing.Pipe(duplex=True)
        proc = multiprocessing.Process(target=self.serve_forever, args=(c, name))
        proc.pipe = p
        proc.stat = 'I'
        proc.start()
        return proc

    def serve_forever(self, pipe, name):
        try:
            self.pipe = pipe
            self.name = name
            self.httpsrv.serve_forever()
        except KeyboardInterrupt:
            return


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

    server = NurlyServer(args.allow_hosts, args.plugin_path)
    server.simple_server(args.server_port, args.num_workers)
