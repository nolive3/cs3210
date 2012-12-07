int last_index_of(const char* path, char c);
int last_index_of_pos(const char* path, char c, int pos);
const char* itom(int month);
int mtoi(const char*  month);
int hasPermission(uid_t uid, const char * path, sqlite3* conn);