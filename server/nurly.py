#!/usr/bin/env python2.7

import argparse
import httpd
import importlib
import os
import shlex
import signal
import subprocess
import sys
import tempfile
import traceback

class check_wrapper:
    def __init__(self, stdout=sys.stdout, stderr=sys.stderr):
        self.stdout = stdout
        self.stderr = stderr

    def main(self, argv):
        sys.exit(subprocess.call(argv, stdout=self.stdout, stderr=self.stderr))

def handle_timeout(*args):
    raise RuntimeError

@httpd.GET('/check-command/(.*)')
def check_command(req, res, cmd):
    results = {
        0: (200, 'OK'),
        1: (221, 'WARNING'),
        2: (222, 'CRITICAL'),
        3: (223, 'UNKNOWN'),
    }

    out = sys.stdout
    err = sys.stderr
    cmd = shlex.split(cmd)
    tmp = tempfile.SpooledTemporaryFile()

    if os.path.dirname(os.path.realpath(cmd[0])) not in req.extra_settings.plugin_path:
        raise httpd.HTTPForbiddenError

    try:
        fun = importlib.import_module(os.path.splitext(os.path.basename(cmd[0]))[0]).main
        arg = cmd[1:]

        if 'argv' not in fun.func_code.co_varnames:
            raise AttributeError
    except (AttributeError, ImportError):
        fun = check_wrapper(tmp, tmp).main
        arg = cmd

    try:
        sys.stdout = sys.stderr = tmp

        handler = signal.getsignal(signal.SIGCHLD)
        signal.signal(signal.SIGCHLD, signal.SIG_DFL)
        signal.signal(signal.SIGALRM, handle_timeout)
        signal.alarm(req.extra_settings.mod_timeout)

        fun(arg)
    except SystemExit as e:
        res.code, res.line = results.get(e.code, results[3])
    except RuntimeError:
        res.code, res.line = results[3]
        print '%s: execution timed out after %ds' % (cmd[0], req.extra_settings.mod_timeout)
    except Exception:
        res.code, res.line = results[3]
        traceback.print_exc(file=sys.stdout)
    else:
        res.code, res.line = results[3]
        print '%s: improperly returned' % cmd[0]
    finally:
        signal.signal(signal.SIGCHLD, handler)
        signal.alarm(0)
        sys.stdout = out
        sys.stderr = err

    tmp.seek(0)
    res.body = tmp.read()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(parents=[httpd.parser], add_help=False, description='nagios service check handler', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('-l', '--plugin-path', default=['/usr/lib/nagios/plugins'], nargs='+',
                        help='path to plugin directory')
    parser.add_argument('-t', '--mod-timeout', default=10, type=int,
                        help='module execution timeout')
    args = parser.parse_args()
    args.plugin_path = [os.path.realpath(i) for i in args.plugin_path]

    sys.path.extend(args.plugin_path)
    httpd.Server(**vars(args))
