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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <regex.h>

#include <sqlite3.h>
#include <libexif/exif-data.h>

#include "hello.h"
#include "dir_handlers.h"
#include "utils.h"

sqlite3* conn;

static int hello_getattr(const char *path, struct stat *stbuf)
{
	char* full_path;
	int ret_code = 0;
	struct stat real_stat;
	FILE* f;

	asprintf(&full_path, ".pictures/%s", path + last_index_of(path, '/'));
	f = fopen("log.txt", "a");
	fprintf(f, "path: [%s] real path: [%s]\n", path, full_path);
	fclose(f);

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if ( !regexec(&all_rx, path, 0, NULL, 0) || !regexec(&formats_rx, path, 0, NULL, 0) ||
				!regexec(&formats_ext_rx, path, 0, NULL, 0) || !regexec(&dates_rx, path, 0, NULL, 0) ||
				!regexec(&dates_year_rx, path, 0, NULL, 0) || !regexec(&dates_year_month_rx, path, 0, NULL, 0) ) {
		stbuf->st_mode = S_IFDIR | 0755;
	}else if ( !stat(full_path, &real_stat) ) {
		memcpy(stbuf, &real_stat, sizeof(real_stat));
	} else {
		ret_code = -ENOENT;
	}

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

	if ( !strcmp(path, "/") ) {
		dir_root(path, buf, filler, NULL);
	} else if ( !regexec(&all_rx, path, 0, NULL, 0) ) {
		dir_all(path, buf, filler, conn);
	} else if( !regexec(&formats_rx, path, 0, NULL, 0) ) {
		dir_formats(path, buf, filler, NULL);
	} else if ( !regexec(&formats_ext_rx, path, 0, NULL, 0) ) {
		dir_formats_ext(path, buf, filler, path + last_index_of(path, '/'), conn);
	} else if ( !regexec(&dates_rx, path, 0, NULL, 0) ) {
		dir_dates(path, buf, filler, conn);
	} else if ( !regexec(&dates_year_rx, path, 0, NULL, 0) ) {
		dir_dates_year(path, buf, filler, conn);
	} else if ( !regexec(&dates_year_month_rx, path, 0, NULL, 0) ) {
		dir_dates_year_month(path, buf, filler, conn);
	} else {
		return -ENOENT;
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
	int year = -1;
	int month = -1;

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
		        buf[7] = 0;
		        year = atoi(buf);
		        month = atoi(buf+5);
			} else {
				fprintf(f,"%s had exif data, but no date\n", full_path);
			}
		} else {
			fprintf(f,"%s had no exif data\n", full_path);
		}
		exif_data_unref(ed);
		fclose(f);

		if ( year == -1 || month == -1) {
			time_t cur_time;
			struct tm * full_time;
			time(&cur_time);
			full_time = localtime(&cur_time);
			if (year == -1)
				year = 1900 + full_time->tm_year;
			if (month == -1)
				month = full_time->tm_mon+1;
		}

		sqlite3_prepare_v2(conn, "insert into pictures values(?, ?, ?)", -1, &stmt, NULL);
		sqlite3_bind_text(stmt, 1, path + 1, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 2, year);
		sqlite3_bind_int(stmt, 3, month);
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

void *hello_init(struct fuse_conn_info *fuse_conn)
{
	sqlite3_open("pictures.db", &conn);

    regcomp(&dates_rx, dates_str, REG_EXTENDED);
    regcomp(&dates_year_rx, dates_year_str, REG_EXTENDED);
    regcomp(&dates_year_month_rx, dates_year_month_str, REG_EXTENDED);
    regcomp(&all_rx, all_str, REG_EXTENDED);
    regcomp(&formats_rx, formats_str, REG_EXTENDED);
    regcomp(&formats_ext_rx, formats_ext_str, REG_EXTENDED);

    return NULL;
}
void hello_destroy(void *userdata)
{
	sqlite3_close(conn);

	regfree(&dates_rx);
    regfree(&dates_year_rx);
    regfree(&dates_year_month_rx);
    regfree(&all_rx);
    regfree(&formats_rx);
    regfree(&formats_ext_rx);
}

static struct fuse_operations hello_oper = {
	.getattr	= hello_getattr,
	.readdir	= hello_readdir,
	.open		= hello_open,
	.read		= hello_read,
	.write		= hello_write,
	.mknod		= hello_mknod,
	.init		= hello_init,
	.destroy	= hello_destroy,
};


int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &hello_oper, NULL);
}
