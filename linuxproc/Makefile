CC=gcc

meres: meres.o prog.o
	$(CC) -o $@ $^

meres.o: meres.c
	$(CC) -c $^

prog.o: prog.c
	$(CC) -c $^