PYTHON=python
INSTALL=install
TARGET_PATH=/usr/bin/

all:
	python setup.py build

install:
	python setup.py install
	$(INSTALL) ioprofiler.py $(TARGET_PATH)/ioprofiler

clean:
	python setup.py clean
