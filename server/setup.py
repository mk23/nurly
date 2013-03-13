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
        author_email='max.kalika+projects@gmail.com',
        url='https://github.com/mk23/nurly',

        name='nurly-server',
        version=VERSION,
        scripts=['nurly_server.py'],
        packages=['nurly', 'nurly.handlers'],
    )
