all: defrag

defrag: defrag.o
	gcc -g -Wall -o defrag defrag.o

defrag.o: defrag.c
	gcc -g -Wall -fpic -c defrag.c

clean:
	rm *o
	rm defrag
