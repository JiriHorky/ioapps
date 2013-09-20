PYTHON=python
INSTALL?=install
DOCDIR?=man
MANDIR?=/usr/share/man/man1/
MANPAGE=ioprofiler.1
TARGET_PATH?=/usr/bin/
DESTDIR?=/

all:
	python setup.py build

install:
	python setup.py install --root=$(DESTDIR)
	$(INSTALL) $(DOCDIR)/$(MANPAGE) $(MANDIR)


clean:
	python setup.py clean
