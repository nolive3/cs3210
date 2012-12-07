opfs_base = opfs /opfs/pictures.db /opfs/.pictures mnt /opfs/encode /opfs/decode


all: run

build: opfs

run: $(opfs_base)
	./opfs mnt/

debug: $(opfs_base)
	./opfs -d mnt/

share: $(opfs_base)
	./opfs -oallow_other mnt/

share-debug: $(opfs_base)
	./opfs -oallow_other -d mnt/

/opfs/.pictures:/opfs
	mkdir -p /opfs/.pictures
	
/opfs/decode:/opfs
	cp decode /opfs/decode
	
/opfs/encode:/opfs
	cp encode /opfs/encode
	
/opfs:
	mkdir -p /opfs

mnt:
	mkdir -p mnt
	

clean:
	fusermount -q -u mnt || true
	rm -f *.o
	rm -f opfs
	rm -rf /opfs
	rm -rf mnt

/opfs/pictures.db:/opfs
	./build_db.sh

tar:
	tar -cf fuse.tar.gz Makefile tables *.c *.h *.sh test_pictures/*
	
opfs: opfs.o dir_handlers.o utils.o
	gcc opfs.o dir_handlers.o utils.o -o opfs `pkg-config fuse --cflags --libs` -lsqlite3 -lexif

opfs.o: opfs.c opfs.h dir_handlers.h utils.h
	gcc -Wall -c opfs.c `pkg-config fuse --cflags --libs` -lsqlite3 -lexif

dir_handlers.o: dir_handlers.c dir_handlers.h utils.h
	gcc -Wall -c dir_handlers.c `pkg-config fuse --cflags --libs` -lsqlite3

utils.o: utils.h utils.c
	gcc -Wall -c utils.c -lsqlite3
