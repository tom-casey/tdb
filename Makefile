CC=gcc
CFLAGS=-I.
DEPS = tdb.h breakpoint.h
OBJ = tdb.o breakpoint.o utils.o

%.o: %.c $(DEPS)
		$(CC) -c -o $@ $< $(CFLAGS)

hellomake: $(OBJ)
		gcc -o $@ $^ $(CFLAGS)
