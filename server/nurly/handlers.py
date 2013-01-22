from nurly.util import NurlyStatus
from nurly.version import VERSION

def init_handlers(server):
    server.create_action(server_status, path='/server-status')
    server.create_action(nurly_version, path='/nurly-version')

def server_status(env, res, parent):
    stat = parent('get_status', recv=True)
    res.body = 'Busy Workers: %d\nIdle Workers: %d\nScoreboard:   %s\n' % (
        len(list(True for i in stat if i != NurlyStatus.ST_IDLE)),
        len(list(True for i in stat if i == NurlyStatus.ST_IDLE)),
        ''.join(NurlyStatus.label(i, True) for i in stat)
    )

def nurly_version(env, res, parent):
    res.body = 'Version: %s\n' % VERSION
