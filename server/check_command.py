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
        sys.exit(subprocess.Popen(argv, stdout=self.stdout, stderr=self.stderr).wait())

def handle_timeout(*args):
    raise RuntimeError

def check_command(env, res, parent):
    ERRORS = {
        0: '200 OK',
        1: '221 WARNING',
        2: '222 CRITICAL',
        3: '223 UNKNOWN',
    }

    tmp = tempfile.SpooledTemporaryFile()
    out = sys.stdout
    err = sys.stderr

    cmd = shlex.split(env['PATH_INFO'][15:])
    if os.path.dirname(os.path.realpath(cmd[0])) not in env['nurly.plugin_path']:
        res.code = '403 Forbidden'
        res.body = '%s: plugin location not allowed by server configuration\n' % cmd[0]
        return
    else:
        try:
            mod = importlib.import_module(os.path.splitext(os.path.basename(cmd[0]))[0])
            arg = cmd[1:]
        except ImportError:
            mod = check_wrapper(tmp, tmp)
            arg = cmd

    try:
        sys.stdout = sys.stderr = tmp

        signal.signal(signal.SIGALRM, handle_timeout)
        signal.alarm(env['nurly.mod_timeout'])

        mod.main(arg)

        raise NotImplementedError
    except SystemExit as e:
        res.code = ERRORS.get(e.code, ERRORS[3])
    except NotImplementedError:
        res.code = ERRORS[3]
        print '%s: improperly returned' % cmd[0]
    except RuntimeError:
        res.code = ERRORS[3]
        print '%s: execution timed out after %ds' % (cmd[0], env['nurly.mod_timeout'])
    except Exception:
        res.code = ERRORS[3]
        traceback.print_exc()

    signal.alarm(0)
    sys.stdout = out
    sys.stderr = err

    tmp.seek(0)
    res.body = tmp.read()
