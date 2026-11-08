CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra

build: build/stilllife build/crosscheck

build/stilllife: src/stilllife.c
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $<

build/crosscheck: src/crosscheck.c
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $<

test: build
	sh tests/run.sh

figures:
	python3 scripts/make_figures.py

clean:
	rm -rf build

.PHONY: build test figures clean
