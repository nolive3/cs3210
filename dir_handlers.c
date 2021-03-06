#define FUSE_USE_VERSION 26
#define _GNU_SOURCE

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glob.h>
#include "sqlite3.h"

#include "dir_handlers.h"
#include "utils.h"

void dir_root(const char *path, void *buf, fuse_fill_dir_t filler, sqlite3* conn)
{
	filler(buf, "All", NULL, 0);
	filler(buf, "Formats", NULL, 0);
	filler(buf, "Dates", NULL, 0);	
	filler(buf, "Search", NULL, 0);
}

void dir_dates(const char *path, void *buf, fuse_fill_dir_t filler, sqlite3* conn)
{
	sqlite3_stmt *stmt;
	int rc;

	sqlite3_prepare_v2(conn, "SELECT DISTINCT year FROM pictures", -1, &stmt, NULL);
	do {
		const char* year;
		rc = sqlite3_step(stmt);
		switch( rc ){
			case SQLITE_DONE:
				break;
			case SQLITE_ROW:
				year = (const char*) sqlite3_column_text(stmt,0);
				filler(buf, year, NULL, 0);
				break;
			default:
				fprintf(stderr, "Error: %d : %s\n",  rc, sqlite3_errmsg(conn));
				break;
		}
	} while (rc == SQLITE_ROW);
	sqlite3_finalize(stmt);
}

void dir_dates_year(const char *path, void *buf, fuse_fill_dir_t filler, sqlite3* conn)
{
	sqlite3_stmt *stmt;
	int rc;
	int year;
	char* path_tok = malloc(strlen(path) + 1);
	char* tok;
	strcpy(path_tok, path);

	strtok(path_tok, "/");
	tok = strtok(NULL, "/");
	year = atoi(tok);
	free(path_tok);

	sqlite3_prepare_v2(conn, "SELECT DISTINCT month FROM pictures where year=?", -1, &stmt, NULL);
	sqlite3_bind_int(stmt, 1, year);
	do {
		int month;
		rc = sqlite3_step(stmt);
		switch( rc ){
			case SQLITE_DONE:
				break;
			case SQLITE_ROW:
				// print results for this row
				month = sqlite3_column_int(stmt,0);
				filler(buf, itom(month), NULL, 0);
				break;
			default:
				fprintf(stderr, "Error: %d : %s\n",  rc, sqlite3_errmsg(conn));
				break;
		}
	} while (rc == SQLITE_ROW);
	sqlite3_finalize(stmt);
}

void dir_dates_year_month(const char *path, void *buf, fuse_fill_dir_t filler, sqlite3* conn)
{
	sqlite3_stmt *stmt;
	int rc;
	int year;
	int month;
	char* path_tok = malloc(strlen(path) + 1);
	char* tok;

	strcpy(path_tok, path);

	strtok(path_tok, "/");
	tok = strtok(NULL, "/");
	year = atoi(tok);
	tok = strtok(NULL, "/");
	month = mtoi(tok);
	free(path_tok);

	sqlite3_prepare_v2(conn, "SELECT filename FROM pictures WHERE year=? AND month=?", -1, &stmt, NULL);
	sqlite3_bind_int(stmt, 1, year);
	sqlite3_bind_int(stmt, 2, month);
	do {
		const char* txt;
		rc = sqlite3_step(stmt);
		switch( rc ){
			case SQLITE_DONE:
				break;
			case SQLITE_ROW:
				// print results for this row
				txt = (const char *) sqlite3_column_text(stmt,0);
				filler(buf, txt ? txt : "NULL", NULL, 0);
				break;
			default:
				fprintf(stderr, "Error: %d : %s\n",  rc, sqlite3_errmsg(conn));
				break;
		}
	} while (rc == SQLITE_ROW);
	sqlite3_finalize(stmt);


}

void dir_all(const char *path, void *buf, fuse_fill_dir_t filler, sqlite3* conn)
{
	sqlite3_stmt *stmt;
	int rc;

	sqlite3_prepare_v2(conn, "select * from pictures", -1, &stmt, NULL);
	do {
		const char *txt;
		rc = sqlite3_step(stmt);
		switch( rc ){
			case SQLITE_DONE:
				break;
			case SQLITE_ROW:
				// print results for this row
				txt = (const char*)sqlite3_column_text(stmt,0);
				filler(buf, txt ? txt : "NULL", NULL, 0);
				break;
			default:
				fprintf(stderr, "Error: %d : %s\n",  rc, sqlite3_errmsg(conn));
				break;
		}
	} while (rc == SQLITE_ROW);
	sqlite3_finalize(stmt);
}

void dir_formats(const char *path, void *buf, fuse_fill_dir_t filler, sqlite3* conn)
{
	filler(buf, "jpg", NULL, 0);
	filler(buf, "png", NULL, 0);
	filler(buf, "bmp", NULL, 0);
}

void dir_formats_ext(const char *path, void *buf, fuse_fill_dir_t filler, const char* file_ext, sqlite3* conn)
{	
	sqlite3_stmt *stmt;
	int rc;
	char insert[6];

	insert[0] = '%';
	insert[1] = '.';
	strncpy(insert+2, file_ext, 3);
	insert[5] = '\0';

	sqlite3_prepare_v2(conn, "select * from pictures where filename like ?", -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, insert, -1, SQLITE_TRANSIENT);
	do {
		const char *txt;
		rc = sqlite3_step(stmt);
		switch( rc ){
			case SQLITE_DONE:
				break;
			case SQLITE_ROW:
				// print results for this row
				txt = (const char*)sqlite3_column_text(stmt,0);
				filler(buf, txt ? txt : "NULL", NULL, 0);
				break;
			default:
				fprintf(stderr, "Error: %d : %s\n",  rc, sqlite3_errmsg(conn));
				break;
		}
	} while (rc == SQLITE_ROW);
	sqlite3_finalize(stmt);

}

void dir_search_term(const char *path, void *buf, fuse_fill_dir_t filler, sqlite3* conn) {
	sqlite3_stmt *stmt;
	int rc;
	char insert[13];
	const char* term = path + last_index_of(path, '/');

	insert[0] = '%';
	strncpy(insert + 1, term, strlen(term));
	insert[strlen(term) + 1] = '%';
	insert[strlen(term) + 2] = '\0';

	sqlite3_prepare_v2(conn, "select * from pictures where filename like ?", -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, insert, -1, SQLITE_TRANSIENT);
	do {
		const char *txt;
		rc = sqlite3_step(stmt);
		switch( rc ){
			case SQLITE_DONE:
				break;
			case SQLITE_ROW:
				// print results for this row
				txt = (const char*)sqlite3_column_text(stmt,0);
				filler(buf, txt ? txt : "NULL", NULL, 0);
				break;
			default:
				fprintf(stderr, "Error: %d : %s\n",  rc, sqlite3_errmsg(conn));
				break;
		}
	} while (rc == SQLITE_ROW);
	sqlite3_finalize(stmt);
}