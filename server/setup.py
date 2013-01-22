#!/usr/bin/env python2.7

from nurly.version import VERSION
from distutils.core import setup

if __name__ == '__main__':
    setup(
        author='Max Kalika',
        author_email='max@topsy.com',

        name='nurly-server',
        version=VERSION,
        scripts=['nurly_server.py', 'check_command.py'],
        packages=['nurly'],
    )
