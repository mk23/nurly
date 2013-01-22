import socket
import wsgiref.simple_server

class WSGIServer(wsgiref.simple_server.WSGIServer):
    def __init__(self, *args):
        self.allow_reuse_address = True
        wsgiref.simple_server.WSGIServer.__init__(self, *args)

    def server_bind(self):
        self.socket.setsockopt(socket.SOL_TCP, socket.TCP_DEFER_ACCEPT, True)
        self.socket.setsockopt(socket.SOL_TCP, socket.TCP_QUICKACK,     True)
        wsgiref.simple_server.WSGIServer.server_bind(self)
