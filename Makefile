PREFIX = 
CC = $(PREFIX)gcc

SRC = \
  src/argps.c \
  src/argpus.c \
  src/bsearch.c \
  src/cvutf8.c \
  src/getopt.c \
  src/hmd5.c \
  src/http.c \
  src/parser.c \
  src/sax.c \
  src/tchar.c \
  src/tr64c.c \
  src/url.c \
  src/utf8.c

SYS := $(shell $(CC) -dumpmachine)
ifneq (, $(findstring linux, $(SYS)))
 include src/linux.mk
else
 ifneq (, $(findstring mingw, $(SYS))$(findstring windows, $(SYS)))
  include src/mingw.mk
 else
  include src/general.mk
 endif
endif

all: bin bin/tr64c$(BINEXT)

.PHONY: clean
clean:
ifeq (,$(strip $(WINDRES)))
	rm -f bin/tr64c$(BINEXT)
else
	rm -f bin/tr64c$(BINEXT) bin/version$(OBJEXT)
endif

bin:
	mkdir bin

.PHONY: bin/tr64c$(BINEXT)
bin/tr64c$(BINEXT): $(SRC)
ifeq (,$(strip $(WINDRES)))
	rm -f $@
	$(CC) $(CFLAGS) -DBACKEND_$(BACKEND) $(CWFLAGS) $(PATHS) $(LDFLAGS) -o $@ $+ $(LIBS)
else
	rm -f $@ bin/tr64c$(OBJEXT)
	$(WINDRES) -DBACKEND_$(BACKEND) src/version.rc bin/version$(OBJEXT)
	$(CC) $(CFLAGS) -DBACKEND_$(BACKEND) $(CWFLAGS) $(PATHS) $(LDFLAGS) -o $@ $+ bin/version$(OBJEXT) $(LIBS)
endif
