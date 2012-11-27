void dir_root(const char *path, void *buf, fuse_fill_dir_t filler, sqlite3* conn);
void dir_dates(const char *path, void *buf, fuse_fill_dir_t filler, sqlite3* conn);
void dir_all(const char *path, void *buf, fuse_fill_dir_t filler, sqlite3* conn);
void dir_formats(const char *path, void *buf, fuse_fill_dir_t filler, sqlite3* conn);
void dir_format(const char *path, void *buf, fuse_fill_dir_t filler, const char* fmt, sqlite3* conn);
