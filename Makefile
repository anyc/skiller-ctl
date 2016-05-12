
CFLAGS+=-Wall

CFLAGS+=$(shell pkg-config --cflags libusb-1.0) $(DEBUG)
LDLIBS+=$(shell pkg-config --libs libusb-1.0)

CTLAPP=skiller-ctl

prefix ?= /usr/local
bindir ?= $(prefix)/bin

.phony: clean install

all: $(CTLAPP)

$(CTLAPP): $(CTLAPP).c

debug:
	$(MAKE) $(MAKEFILE) DEBUG="-g -DDEBUG"

clean:
	rm -rf $(CTLAPP)

install:
	install -m 0755 $(CTLAPP) $(bindir)/