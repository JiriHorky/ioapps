MANPAGE=ioreplay.1
MANDIR=/usr/share/man/man1/
INSTALL=install

all: $(MANPAGE)

$(MANPAGE):
	help2man --name "IO traces replayer" -s1 -m"User Commands" --output=$(MANPAGE) ../ioreplay -N -i static.txt

clean:
	rm -f $(MANPAGE)

install:
	$(INSTALL) $(MANPAGE) $(MANDIR)
