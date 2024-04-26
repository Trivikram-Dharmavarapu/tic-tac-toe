CC=g++
CFLAG=-Wall -std=c++11 -pedantic
server: server.o
	$(CC) server.o -o server
server.o: server.cpp game.h
	$(CC) -c server.cpp
clean:
	rm -f a.out
	rm -f *.o
	rm -f *~
	rm -f core.*
	find . -type f -executable -exec rm '{}' \;
