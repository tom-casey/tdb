CC=gcc
CFLAGS=-lexplain -I. -I/usr/include/libdwarf/ -g
DEPS = tdb.h breakpoint.h
OBJ = tdb.o breakpoint.o utils.o

%.o: %.c $(DEPS)
		$(CC) -c -o $@ $< $(CFLAGS)

tdb: $(OBJ)
		gcc -o $@ $^ $(CFLAGS)
