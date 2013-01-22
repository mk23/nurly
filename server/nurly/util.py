import traceback

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

        self.fileno = self.pipe.fileno

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
