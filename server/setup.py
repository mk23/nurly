#!/usr/bin/env python2.7

import sys
from distutils.core import setup

if __name__ == '__main__':
    if 'install' in sys.argv or 'sdist' in sys.argv:
        from nurly.version import VERSION
    else:
        VERSION = None

    setup(
        author='Max Kalika',
        author_email='max@topsy.com',
        url='http://potato.georx.net/cgit/cgit.cgi/nurly/',

        name='nurly-server',
        version=VERSION,
        scripts=['nurly_server.py'],
        packages=['nurly', 'nurly.handlers'],
    )
