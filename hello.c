/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall hello.c `pkg-config fuse --cflags --libs` -o hello
*/

#define FUSE_USE_VERSION 26
#define _GNU_SOURCE

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sqlite3.h>
#include <libexif/exif-data.h>

#include "dir_handlers.h"
#include "utils.h"

sqlite3* conn;

static int hello_getattr(const char *path, struct stat *stbuf)
{
	char* full_path;
	int ret_code = 0;
	struct stat real_stat;

	asprintf(&full_path, ".pictures/%s", path + last_index_of(path, '/'));

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if ( strcmp(path, "/All") == 0 || strcmp(path, "/Formats") == 0 ||
				strcmp(path, "/Formats/jpg") == 0 || strcmp(path, "/Formats/png") == 0 ||
				strcmp(path, "/Formats/bmp") == 0 || strcmp(path, "/Dates") == 0 ||
				strncmp(path, "/Dates/", 7) == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
	}else if ( !stat(full_path, &real_stat) ) {
		stbuf->st_mode = real_stat.st_mode;
		stbuf->st_nlink = real_stat.st_nlink;
		stbuf->st_size = real_stat.st_size;
	} else
		ret_code = -ENOENT;

	free(full_path);
	return ret_code;
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	if (strcmp(path, "/") == 0) {
		dir_root(path, buf, filler, NULL);
	} else if (strcmp(path, "/All") == 0) {
		dir_all(path, buf, filler, conn);
	} else if( strcmp(path, "/Formats") == 0) {
		dir_formats(path, buf, filler, NULL);
	} else if (strcmp(path, "/Dates") == 0) {
		dir_dates(path, buf, filler, conn);
	} else if ( strcmp(path, "/Formats/jpg") == 0 ||
				strcmp(path, "/Formats/png") == 0 ||
				strcmp(path, "/Formats/bmp") == 0) {
		dir_format(path, buf, filler, path + last_index_of(path, '/'), conn);
	}
	
	return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi)
{
	char* full_path;
	FILE* f;
	int ret_code;

	asprintf(&full_path, ".pictures/%s", path + last_index_of(path, '/'));
	
	if ( (f = fopen(full_path, "r")) ) {
		ret_code = 0;
	} else {
		ret_code = -ENOENT;
	}

	//if ((fi->flags & 3) != O_RDONLY)
	//	return -EACCES;
	
	free(full_path);
	return ret_code;
}

static int hello_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	char* full_path;
	FILE* f;
	(void) fi;
	struct stat real_stat;
	size_t real_size;

	asprintf(&full_path, ".pictures/%s", path + last_index_of(path, '/'));

	if( !(f = fopen(full_path, "r")) ) {
		free(full_path);	
		return -ENOENT;
	}

	stat(full_path, &real_stat);
	real_size = real_stat.st_size;

	if (offset < real_size) {
		if (offset + size > real_size)
			size = real_size - offset;
		fseek(f, offset, SEEK_SET);
		fread(buf, sizeof(char), size, f);
	} else {
		size = 0;
	}

	fclose(f);
	free(full_path);
	return size;
}

static int hello_write(const char *path, const char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	char* full_path;
	FILE* f;
	ExifData *ed;
	ExifEntry *entry;
	(void) fi;
	sqlite3_stmt *stmt;
	int year = 1969;

	asprintf(&full_path, ".pictures/%s", path + last_index_of(path, '/'));

	if( !(f = fopen(full_path, "a")) ) {
		free(full_path);
		return -ENOENT;
	}

	fwrite(buf, sizeof(char), size, f);
	fclose(f);

	if (offset == 0) {

		f = fopen("log.txt", "a");
		ed = exif_data_new_from_file(full_path);
		if (ed) {
			entry = exif_content_get_entry(ed->ifd[EXIF_IFD_EXIF], EXIF_TAG_DATE_TIME_ORIGINAL);
			if (entry) {
				char buf[1024];
		        exif_entry_get_value(entry, buf, sizeof(buf));
		        fprintf(f, "%s had date %s\n", full_path, buf);
		        buf[4] = 0;
		        year = atoi(buf);
			} else {
				fprintf(f,"%s had exif data, but no date\n", full_path);
			}
		} else {
			fprintf(f,"%s had no exif data\n", full_path);
		}
		exif_data_unref(ed);
		fclose(f);

		sqlite3_prepare_v2(conn, "insert into pictures values(?, ?)", -1, &stmt, NULL);
		sqlite3_bind_text(stmt, 1, path + 1, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 2, year);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);

	}
	
	free(full_path);
	return size;
}

static int hello_mknod(const char *path, mode_t mode, dev_t rdev) {

	int ret_code;
	char* full_path;

	asprintf(&full_path, ".pictures/%s", path + last_index_of(path, '/'));
	
	ret_code = mknod(full_path, mode, rdev);

	free(full_path);
	return ret_code;
}

static struct fuse_operations hello_oper = {
	.getattr	= hello_getattr,
	.readdir	= hello_readdir,
	.open		= hello_open,
	.read		= hello_read,
	.write		= hello_write,
	.mknod		= hello_mknod,
};

int main(int argc, char *argv[])
{
	sqlite3_open("pictures.db", &conn);
	return fuse_main(argc, argv, &hello_oper, NULL);
}
