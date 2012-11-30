all: run

build: ypfs

run: ypfs db
	mkdir -p .pictures
	mkdir -p mnt
	./ypfs -d mnt/

clean:
	rm -f *.o
	rm -f ypfs
	rm -f log.txt
	rm -f pictures.db
	rm -rf .pictures
	rm -rf mnt

db:
	./build_db.sh

tar:
	tar -cf fuse.tar.gz Makefile tables *.c *.h *.sh test_pictures/*
	
ypfs: ypfs.o dir_handlers.o utils.o
	gcc ypfs.o dir_handlers.o utils.o -o ypfs `pkg-config fuse --cflags --libs` -lsqlite3 -lexif

ypfs.o: ypfs.c ypfs.h dir_handlers.h utils.h
	gcc -Wall -c ypfs.c `pkg-config fuse --cflags --libs` -lsqlite3 -lexif

dir_handlers.o: dir_handlers.c dir_handlers.h utils.h
	gcc -Wall -c dir_handlers.c `pkg-config fuse --cflags --libs` -lsqlite3

utils.o: utils.h utils.c
	gcc -Wall -c utils.c
