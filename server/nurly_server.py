#!/usr/bin/env python2.7

import argparse
import multiprocessing
import os
import sys

from nurly.server import NurlyServer
from nurly.handlers import check_command

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='nagios service check handler', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('-a', '--allow-hosts', default=[], nargs='+',
                        help='list of allowed nagios hosts')
    parser.add_argument('-p', '--plugin-path', default=['/usr/lib/nagios/plugins'], nargs='+',
                        help='path to plugin directory')
    parser.add_argument('-t', '--mod-timeout', default=10, type=int,
                        help='module execution timeout')
    parser.add_argument('-s', '--server-port', default=1123, type=int,
                        help='server listen port')
    parser.add_argument('-n', '--num-workers', default=multiprocessing.cpu_count(), type=int,
                        help='number of handler processes')
    args = parser.parse_args()
    args.plugin_path = [os.path.realpath(i) for i in args.plugin_path]

    sys.path.extend(args.plugin_path)

    server = NurlyServer(args.allow_hosts, mod_timeout=args.mod_timeout, plugin_path=args.plugin_path)

    server.create_action(check_command, path='/check-command')
    server.simple_server(args.server_port, args.num_workers)
