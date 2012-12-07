#include <string.h>
#include <sqlite3.h>
#include <sys/types.h>
#include <unistd.h>
#include "utils.h"

int hasPermission(uid_t uid, const char * path, sqlite3* conn) {
	sqlite3_stmt *stmt;
	int ret_code = 0;
	int db_code = 0;
	if ( !strstr(path, "+private") ) {
		ret_code = 1;
	} else {
		sqlite3_prepare_v2(conn, "SELECT uid FROM pictures WHERE filename=?", -1, &stmt, NULL);
		sqlite3_bind_text(stmt, 1, path + last_index_of(path, '/'), -1, SQLITE_TRANSIENT);
		db_code = sqlite3_step(stmt);

		if (db_code == SQLITE_ROW) {
			int db_uid = sqlite3_column_int(stmt,0);
			if (db_uid == uid) {
				ret_code = 1;
			}
		} else {
			ret_code = 1;
		}

		sqlite3_finalize(stmt);
	}
	return ret_code;
}

int last_index_of(const char* path, char c) {
	return last_index_of_pos(path, c, strlen(path));
}

int last_index_of_pos(const char* path, char c, int start) {
	int pos = start;
	while (path[pos] != c) {
		pos--;
		if (pos == -1)
			return pos;
	}
	return ++pos;
} 
 
const char* itom(int month) {
	switch(month) {
		case 1: return "January";
		case 2: return "February";
		case 3: return "March";
		case 4: return "April";
		case 5: return "May";
		case 6: return "June";
		case 7: return "July";
		case 8: return "August";
		case 9: return "September";
		case 10: return "October";
		case 11: return "November";
		case 12: return "December";
		default: return "NULL";
	}
}

int mtoi(const char* month) {
	if ( !strcmp(month, "January") ) return 1;
	else if ( !strcmp(month, "February") ) return 2;
	else if ( !strcmp(month, "March") ) return 3;
	else if ( !strcmp(month, "April") ) return 4;
	else if ( !strcmp(month, "May") ) return 5;
	else if ( !strcmp(month, "June") ) return 6;
	else if ( !strcmp(month, "July") ) return 7;
	else if ( !strcmp(month, "August") ) return 8;
	else if ( !strcmp(month, "September") ) return 9;
	else if ( !strcmp(month, "October") ) return 10;
	else if ( !strcmp(month, "November") ) return 11;
	else if ( !strcmp(month, "December") ) return 12;
	else return -1;
}