
CPPFLAGS ?=

CFLAGS ?= -O3 -ggdb3 -std=gnu99 -Wall -pedantic

minimalloc_test: minimalloc_test.o minimalloc.o
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

minimalloc_test: LDFLAGS += $(shell pkg-config --libs libbsd check)

minimalloc.o: CFLAGS += $(shell pkg-config --cflags libbsd)

minimalloc_test.o: CFLAGS += $(shell pkg-config --cflags check)

clean:
	rm -rf minimalloc_test minimalloc.o minimalloc_test.o

check test: minimalloc_test
	./minimalloc_test
