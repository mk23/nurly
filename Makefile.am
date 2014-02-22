EXTRA_DIST = nagios
ACLOCAL_AMFLAGS = -Im4

dist_doc_DATA = README.md LICENSE.txt

SUBDIRS = src

dist-hook:
	(cd server ; python setup.py sdist --dist-dir ..)
