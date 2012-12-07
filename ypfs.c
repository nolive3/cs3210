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
#include <sys/time.h>

#include <sqlite3.h>
#include <libexif/exif-data.h>

#include "ypfs.h"
#include "dir_handlers.h"
#include "utils.h"
#undef DEBUG
sqlite3* conn;

char* path_prefix = ".pictures/%s";

char* build_path(const char* path) {
	char* full_path;
	asprintf(&full_path, ".pictures/%s", path + last_index_of(path, '/'));
	return full_path;
}

static int ypfs_getattr(const char *path, struct stat *stbuf)
{
	char* full_path;
	int ret_code = 0;
	struct stat real_stat;
	FILE* f;

#ifdef DEBUG
	if(strcmp(path, "/debug") == 0) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = 10;
		return 0;
	}
#endif

	full_path = build_path(path);

	f = fopen("log.txt", "a");
	fprintf(f, "path: [%s] real path: [%s]\n", path, full_path);
	fclose(f);

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0777;
		stbuf->st_nlink = 2;
	} else if ( !regexec(&all_rx, path, 0, NULL, 0) || !regexec(&formats_rx, path, 0, NULL, 0) ||
				!regexec(&formats_ext_rx, path, 0, NULL, 0) || !regexec(&dates_rx, path, 0, NULL, 0) ||
				!regexec(&dates_year_rx, path, 0, NULL, 0) || !regexec(&dates_year_month_rx, path, 0, NULL, 0) ||
				!regexec(&search_rx, path, 0, NULL, 0) || !regexec(&search_term_rx, path, 0, NULL, 0) ) {
		stbuf->st_mode = S_IFDIR | 0777;
	}else if ( !stat(full_path, &real_stat) ) {
		memcpy(stbuf, &real_stat, sizeof(real_stat));
	} else {
		ret_code = -ENOENT;
	}

	free(full_path);
	return ret_code;
}

static int ypfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

#ifdef DEBUG
	if(!strcmp(path,"/"))
		filler(buf, "debug", NULL, 0);
#endif

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
	} else if ( !regexec(&search_rx, path, 0, NULL, 0) ) {
		return 0;
	} else if ( !regexec(&search_term_rx, path, 0, NULL, 0) ) {
		dir_search_term(path, buf, filler, conn);
	} else {
		return -ENOENT;
	}

	return 0;
}

static int ypfs_open(const char *path, struct fuse_file_info *fi)
{
	char* full_path;
	FILE* f;
	int ret_code;
#ifdef DEBUG
	if(!strcmp(path,"/debug")){
		return 0;
	}
#endif
	full_path = build_path(path);
	
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

static int ypfs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	char* full_path;
	FILE* f;
	(void) fi;
	struct stat real_stat;
	size_t real_size;

#ifdef DEBUG
	if(!strcmp(path,"/debug")){
		size_t len;
		char* debugstr=NULL;
		len = asprintf(&debugstr, "%010u\n", 0);
		if (len == (size_t)-1)
			return 0;
		if (offset < len) {
			if (offset + size > len)
				size = len - offset;
			memcpy(buf, debugstr + offset, size);
		}else
			size = 0;
		free(debugstr);
		return size;
	}
#endif

	full_path = build_path(path);

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

static int ypfs_write(const char *path, const char *buf, size_t size, off_t offset,
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

	full_path = build_path(path);

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

		sqlite3_prepare_v2(conn, "insert into pictures values(?, ?, ?, ?)", -1, &stmt, NULL);
		sqlite3_bind_text(stmt, 1, path + last_index_of(path, '/'), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 2, year);
		sqlite3_bind_int(stmt, 3, month);
		sqlite3_bind_int(stmt, 4, fuse_get_context()->uid);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);

	}
	
	free(full_path);
	return size;
}

static int ypfs_mknod(const char *path, mode_t mode, dev_t rdev) {

	int ret_code;
	char* full_path;

	full_path = build_path(path);
	
	ret_code = mknod(full_path, mode, rdev);

	free(full_path);
	return ret_code;
}

/** Change the permission bits of a file */
int ypfs_chmod(const char *path, mode_t mode)
{
    int ret_code = 0;
    char* full_path;

    full_path = build_path(path);
    
    ret_code = chmod(full_path, mode);

    free(full_path);
    return ret_code;
}

/** Change the owner and group of a file */
int ypfs_chown(const char *path, uid_t uid, gid_t gid)
  
{
	int ret_code = 0;
    char* full_path;

    full_path = build_path(path);
    
    ret_code = chown(full_path, uid, gid);

    free(full_path);
    return ret_code;
}

/** Change the access and/or modification times of a file */
/* note -- I'll want to change this as soon as 2.6 is in debian testing */
int ypfs_utime(const char *path, struct utimbuf *ubuf)
{
	int ret_code = 0;
    char* full_path;

    full_path = build_path(path);
    
    ret_code = utime(full_path, ubuf);

    free(full_path);
    return ret_code;
}

int ypfs_utimens(const char * path, const struct timespec ts[2]) {
	int ret_code = 0;
    char* full_path;
	struct timeval tv[2];

    full_path = build_path(path);

	tv[0].tv_sec = ts[0].tv_sec;
	tv[0].tv_usec = ts[0].tv_nsec / 1000;
	tv[1].tv_sec = ts[1].tv_sec;
	tv[1].tv_usec = ts[1].tv_nsec / 1000;

    ret_code = utimes(full_path, tv);

    free(full_path);
    return ret_code;
}

/** Change the size of a file */
int ypfs_truncate(const char *path, off_t newsize)
{
	int ret_code = 0;
    char* full_path;

    full_path = build_path(path);
    
    ret_code = truncate(full_path, newsize);

    free(full_path);
    return ret_code;
}

int ypfs_rename(const char *path, const char *newpath)
{
    int ret_code = 0;
    char* full_path;
    char* new_full_path;
	sqlite3_stmt *stmt;

    full_path = build_path(path);
    new_full_path = build_path(newpath);
    
    if (!strstr(path, "+private") && strstr(newpath, "+private")) {
    	char* command;
    	asprintf(&command, "./encode \"%s\" \"%s\" \"%d\"", full_path, new_full_path, fuse_get_context()->uid);
    	ret_code = system(command);
    	if ( WEXITSTATUS(ret_code) )
    		ret_code = -EACCES;
    	free(command);
    } else if (strstr(path, "+private") && !strstr(newpath, "+private")) {
    	char* command;
    	asprintf(&command, "./decode \"%s\" \"%s\" \"%d\"", new_full_path, full_path, fuse_get_context()->uid);
    	ret_code = system(command);
    	if ( WEXITSTATUS(ret_code) )
    		ret_code = -EACCES;
    	free(command);
    } else {
    	ret_code = rename(full_path, new_full_path);
    }
    
	if (!ret_code) {
		sqlite3_prepare_v2(conn, "UPDATE pictures SET filename=? WHERE filename=?", -1, &stmt, NULL);
		sqlite3_bind_text(stmt, 2, path + last_index_of(path, '/'), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 1, newpath + last_index_of(newpath, '/'), -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

 	free(full_path);
    free(new_full_path);

    return ret_code;
}

/** Remove a file */
int ypfs_unlink(const char *path)
{
    int ret_code = 0;
    char* full_path;
	sqlite3_stmt *stmt;
	
	/*	Add check for uid and don't allow a delete from another user
	sqlite3_prepare_v2(conn, "SELECT * FROM pictures WHERE filename=? AND uid=?", -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, path + last_index_of(path, '/'), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 2, fuse_get_context()->uid);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	*/

    full_path = build_path(path);

    ret_code = unlink(full_path);

	if (!ret_code) {
		sqlite3_prepare_v2(conn, "DELETE FROM pictures WHERE filename=?", -1, &stmt, NULL);
		sqlite3_bind_text(stmt, 1, path + last_index_of(path, '/'), -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

    return ret_code;
}

void *ypfs_init(struct fuse_conn_info *fuse_conn)
{
	sqlite3_open("pictures.db", &conn);

    regcomp(&dates_rx, dates_str, REG_EXTENDED);
    regcomp(&dates_year_rx, dates_year_str, REG_EXTENDED);
    regcomp(&dates_year_month_rx, dates_year_month_str, REG_EXTENDED);
    regcomp(&all_rx, all_str, REG_EXTENDED);
    regcomp(&formats_rx, formats_str, REG_EXTENDED);
    regcomp(&formats_ext_rx, formats_ext_str, REG_EXTENDED);
    regcomp(&search_rx, search_str, REG_EXTENDED);
    regcomp(&search_term_rx, search_term_str, REG_EXTENDED);

    return NULL;
}
void ypfs_destroy(void *userdata)
{
	sqlite3_close(conn);

	regfree(&dates_rx);
    regfree(&dates_year_rx);
    regfree(&dates_year_month_rx);
    regfree(&all_rx);
    regfree(&formats_rx);
    regfree(&formats_ext_rx);
    regfree(&search_rx);
    regfree(&search_term_rx);
}

static struct fuse_operations ypfs_oper = {
	.getattr	= ypfs_getattr,
	.readdir	= ypfs_readdir,
	.open		= ypfs_open,
	.read		= ypfs_read,
	.write		= ypfs_write,
	.mknod		= ypfs_mknod,
	.init		= ypfs_init,
	.destroy	= ypfs_destroy,
	.chown		= ypfs_chown,
	.chmod		= ypfs_chmod,
	.utimens	= ypfs_utimens,
	.truncate	= ypfs_truncate,
	.rename		= ypfs_rename,
	.unlink		= ypfs_unlink,
};


int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &ypfs_oper, NULL);
}
