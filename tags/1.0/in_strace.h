#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "in_common.h"
#include "adt/list.h"

typedef struct isyscall {
	item_t item;
	int32_t pid;
	char line[MAX_STRING];
} isyscall_t;

int strace_get_items(char * filename, list_t * list, int stats);
inline int strace_process_line(char * line, list_t * list, hash_table_t * ht, int stats);
