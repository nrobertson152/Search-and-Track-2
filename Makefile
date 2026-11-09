CC ?= cc
CFLAGS ?= -O2 -g -std=c17 -Wall -Wextra -Wpedantic
CPPFLAGS ?=
LDLIBS = -lX11 -lm
PREFIX ?= /usr/local

.PHONY: all test smoke clean install
all: build/search-and-track-2
build/search-and-track-2: src/main.c src/sim.c src/game.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ src/main.c src/sim.c $(LDLIBS)
build/test-sim: tests/test_sim.c src/sim.c src/game.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_sim.c src/sim.c -lm
test: build/test-sim
	./build/test-sim
smoke: build/search-and-track-2
	./build/search-and-track-2 --smoke
install: all
	install -Dm755 build/search-and-track-2 $(DESTDIR)$(PREFIX)/bin/search-and-track-2
clean:
	rm -rf build
