import multiprocessing
import select
import traceback
import wsgiref.simple_server

import nurly.handlers

from nurly.util import NurlyAction, NurlyStatus
from nurly.worker import NurlyWorker
from nurly.wsgi import WSGIServer

class NurlyServer():
    def __init__(self, allow_hosts, **kwargs):
        self.environ = dict(('nurly.%s' % k, v) for k, v in kwargs.items())
        self.environ['nurly.allow_hosts'] = allow_hosts
        self.environ['nurly.action_list'] = []

        nurly.handlers.init_handlers(self)

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
            for chld in select.select(self.states.values(), [], [])[0]:
                try:
                    pid, func, args = chld.pipe.recv()
                    getattr(self, 'worker_%s' % func, lambda *a: True)(pid, *args)
                except EOFError:
                    chld.proc.worker.join()
                    del self.states[chld.proc.pid]

                    pid, status = self.create_worker()
                    self.states[pid] = status
                except:
                    traceback.print_exc()
