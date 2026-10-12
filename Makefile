PREFIX ?= $(HOME)/.local
CC     ?= clang
CFLAGS ?= -O2 -Wall

cscreen: cscreen-arm.c
	$(CC) $(CFLAGS) -framework ApplicationServices -o $@ $<

install: cscreen
	install -d $(PREFIX)/bin
	install -m 755 cscreen $(PREFIX)/bin/cscreen

uninstall:
	rm -f $(PREFIX)/bin/cscreen

clean:
	rm -f cscreen

.PHONY: install uninstall clean
