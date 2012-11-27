#include <string.h>

int last_index_of(const char* path, char c) {
	int pos = strlen(path);
	while (path[pos] != c) {
		pos--;
		if (pos == -1)
			return pos;
	}
	return ++pos;
} 
 
