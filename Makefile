target ?= gbcamextract
VERSION_STRING= 1.1
objects := $(patsubst %.c,%.o,$(wildcard *.c))

ifdef $(__MINGW32__)
LDLIBS += -l:libpng.a -l:libz.a
else
LDLIBS += -lpng
endif


CFLAGS  += -std=gnu99 -Os -ggdb -D__progversion=\"${VERSION_STRING}\" -D__progname=\"${target}\" -Wall -fanalyzer

.PHONY: all
all:	$(target)

.PHONY: clean
clean:
	rm -f $(target) $(target).exe $(objects)

.PHONY: install
install:
	cp $(target) /usr/local/bin/$(target)

$(target): $(objects)
