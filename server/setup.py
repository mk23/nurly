#!/usr/bin/env python2.7

import os

from setuptools import setup
from httpd import VERSION

if __name__ == '__main__':
    # dirty hack to allow symlink targets to work
    delattr(os, 'link')

    setup(
        author='Max Kalika',
        author_email='max.kalika+projects@gmail.com',
        url='https://github.com/mk23/nurly',

        name='nurly-server',
        version=VERSION,
	license='../LICENSE.txt',
	scripts=['nurly.py', 'httpd.py']
    )
