target ?= gbcamextract
VERSION_STRING= 1.1
objects := $(patsubst %.c,%.o,$(wildcard *.c))

LDLIBS += -lpng

CFLAGS  += -std=gnu99 -Os -ggdb -D__progversion=\"${VERSION_STRING}\" -D__progname=\"${target}\"

#EXTRAS += -fsanitize=undefined -fsanitize=null -fcf-protection=full -fstack-protector-all -fstack-check -Wimplicit-fallthrough -fanalyzer -Wall
EXTRAS += -fanalyzer -Wall -flto

CFLAGS += ${EXTRAS}
LDFLAGS += ${EXTRAS}

.PHONY: all
all:	$(target)

.PHONY: clean
clean:
	rm -f $(target) $(target).exe $(objects)

.PHONY: install
install:
	cp $(target) /usr/local/bin/$(target)

$(target): $(objects)
