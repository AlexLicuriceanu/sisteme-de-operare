CC = gcc
CFLAGS = -fPIC -Wall
LDFLAGS = -m32

.PHONY: build
build: libscheduler.so

libscheduler.so: scheduler.o
	$(CC) -shared -o $@ $^

scheduler.o: so_scheduler.c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	-rm -f scheduler.o libscheduler.so
