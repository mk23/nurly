EXTRA_DIST = nagios
ACLOCAL_AMFLAGS = -Im4

SUBDIRS = src

dist-hook:
	(cd server ; python setup.py sdist --dist-dir ..)
