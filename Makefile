.PHONY : all clean

DISTFILES=ioreplay
SOURCES=ioreplay.c in_common.c in_strace.c in_binary.c replicate.c simulate.c stats.c fdmap.c namemap.c simfs.c adt/list.c adt/hash_table.c adt/fs_trie.c
CFLAGS=-c -g -Wall -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -I.
OBJFILES=$(subst .c,.o,$(SOURCES))
DEPFILES=$(subst .c,.d,$(SOURCES))

all: $(DISTFILES) man

man:

clean:
	rm -f $(DISTFILES) $(OBJFILES) $(DEPFILES)

$(DISTFILES): $(subst .c,.o,$(SOURCES))
	$(CC) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

include $(subst .c,.d,$(SOURCES))

%.d: %.c
	$(CC) -MM -MT "$*.o" $(CPPFLAGS) $< > $*.d
	
