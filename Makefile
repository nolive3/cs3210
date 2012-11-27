all: run

build: hello

run: hello db
	mkdir -p .pictures
	mkdir -p mnt
	./hello -d mnt/

clean:
	rm -f *.o
	rm -f hello
	rm -f log.txt
	rm -f pictures.db
	rm -rf .pictures
	rm -rf mnt

db:
	./build_db.sh

tar:
	tar -cf fuse.tar.gz Makefile tables *.c *.h *.sh test_pictures/*
	
hello: hello.o dir_handlers.o utils.o
	gcc hello.o dir_handlers.o utils.o -o hello `pkg-config fuse --cflags --libs` -lsqlite3 -lexif

hello.o: hello.c dir_handlers.h utils.h
	gcc -Wall -c hello.c `pkg-config fuse --cflags --libs` -lsqlite3 -lexif

dir_handlers.o: dir_handlers.c dir_handlers.h utils.h
	gcc -Wall -c dir_handlers.c `pkg-config fuse --cflags --libs` -lsqlite3

utils.o: utils.h utils.c
	gcc -Wall -c utils.c
