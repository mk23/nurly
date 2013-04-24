#!/usr/bin/env python2.7

import sys

from distutils.core import setup
from httpd import VERSION

if __name__ == '__main__':
    setup(
        author='Max Kalika',
        author_email='max.kalika+projects@gmail.com',
        url='https://github.com/mk23/nurly',

        name='nurly-server',
        version=VERSION,
        scripts=['nurly.py', 'httpd.py'],
    )
