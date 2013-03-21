#!/bin/bash -e

cd $(dirname $(readlink -e $0))
/usr/bin/curl -s https://raw.github.com/mk23/sandbox/master/misc/release.py | exec /usr/bin/env python2.7 - --commit -e configure.ac 'nurly-module, {version},' -e server/nurly/version.py 'VERSION = "{version}"' $@
