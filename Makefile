PROG= gbcamextract
VERSION_STRING= 1.0

ifdef $(__MINGW32__)
LDLIBS += -l:libpng.a -l:libz.a
else
LDLIBS += -lpng
endif

include Makefile.in
