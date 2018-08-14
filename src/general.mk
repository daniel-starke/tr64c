CWFLAGS = -Wall -Wextra -Wformat -pedantic -Wshadow -Wno-format -std=c99
CFLAGS = -O2 -DNDEBUG -D_BSD_SOURCE -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE -mtune=core2 -march=core2 -mstackrealign -fomit-frame-pointer -fno-ident
LDFLAGS = -s -fno-ident
PATHS = 
LIBS = 
OBJEXT = .o
BINEXT = 

BACKEND = POSIX
