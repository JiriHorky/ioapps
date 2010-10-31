/* IOapps, IO profiler and IO traces replayer

    Copyright (C) 2010 Jiri Horky <jiri.horky@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdlib.h>
#include <ctype.h>
#include <fnmatch.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "namemap.h"


hash_table_t ht_map;
list_t l_igns;

static int ht_compare_namemap(key_t *key, item_t *item) {
	namemap_item_t * namemap_item;

	namemap_item = hash_table_entry(item, namemap_item_t, item);
	return ! strncmp(namemap_item->old_name, (char *) key, MAX_STRING);
}

static inline void ht_remove_callback_namemap(item_t * item) {
	namemap_item_t * namemap_item = hash_table_entry(item, namemap_item_t, item);	
	free(namemap_item);
	return;
}

/** hash table operations. */
static hash_table_operations_t ht_ops_namemap = {
	.hash = ht_hash_str,
	.compare = ht_compare_namemap,
	.remove_callback = ht_remove_callback_namemap
};

char * namemap_load_item(char * line, char * name, ssize_t size) {
	int i = 0;

	if ( line[0] == '"' ) {
		i = 1;
		while (line[i] && line[i] != '"' && i < size) {
			name[i-1] = line[i];
			i++;
		}
		if (! line[i]) { //we reached end of the line
			ERRORPRINTF("Error loading mapping from file: Missing '\"' character.%s", "\n");
			return NULL;
		} else if ( i >= MAX_STRING ) {
			ERRORPRINTF("Error loading mapping from file: Missing '\"' character or path name too long %s", "\n");
			return NULL;
		}
		name[i-1] = 0;
		return line + (i+1);
	} else if ( isalnum(line[0]) || line[0] == '/' ) {
		i = 0;
		while (line[i] && line[i] != ' ' && i < size) {
			name[i] = line[i];
			i++;
		}
		if ( i >= MAX_STRING ) {
			ERRORPRINTF("Error loading mapping from file: Missing ' ' character or path name too long on line: %s", "\n");
			return NULL;
		}
		name[i] = 0;
		return line + i;
	} else {
		ERRORPRINTF("Error loading mapping from file: First character on the line unrecognized: '%c'(%d)\n", line[0], line[0]);
		return NULL;
	}
}

int namemap_load_items(char * line, char * old_name, char * new_name, ssize_t size) {
	char * c;
	if ( (c = namemap_load_item(line, old_name, size)) == NULL) {
		return -1;
	} else {
		if ( (c = namemap_load_item(c+1, new_name, size)) == NULL) {
			return -1;
		} else {
			if ( *c != 0 ) {
				ERRORPRINTF("Error loading mapping from file: not whole line was read!%s", "\n");
				return -1;
			} else {
				return 0;
			}
		}
	}
}

int namemap_init(char * ifilename, char * mfilename) {
	FILE * ifile = NULL;
	FILE * mfile = NULL;	
	char line[2*MAX_STRING + 2];
	int linenum = 1;
	namemap_item_t * nm_item;

	hash_table_init(&ht_map, HASH_TABLE_SIZE, &ht_ops_namemap );
	list_init(&l_igns);

	if (ifilename) {
		if ( (ifile = fopen(ifilename, "r"))  == NULL) {
			ERRORPRINTF("Cannot open ignore file %s: %s. Ignoring it.\n", ifilename, strerror(errno));
			return -1;
		}
	}

	if (mfilename) {
		if ( (mfile = fopen(mfilename, "r"))  == NULL) {
			ERRORPRINTF("Cannot open mapping file %s: %s. Ignoring it.\n", mfilename, strerror(errno));
			return -1;
		}
	}

	if (ifile) {
		while(fgets(line, MAX_STRING, ifile) != NULL) {
			if ( line[0] == '#' ) //comment
				continue;

			nm_item = malloc(sizeof(namemap_item_t));
			item_init(&nm_item->item);			

			int i = 0;
			int found_nl = 0;
			while ( i < MAX_STRING && line[i] != 0) {
				if (line[i] == '\n') {
					line[i] = 0;
					found_nl = 1;
					break;
				}
				i++;
			}
			if ( ! found_nl) {
				ERRORPRINTF("Error loading ignored file names from %s: line %d too long. \n", ifilename, linenum);
				free(nm_item);
				return -1;
			}

			strncpy(nm_item->old_name, line, MAX_STRING);
			nm_item->new_name[0] = 0;
			list_append(&l_igns, &nm_item->item);
			linenum++;
		}
	} 

	linenum = 1;
	if (mfile) {
		while(fgets(line, 2*MAX_STRING+2, mfile) != NULL) {			
			if ( line[0] == '#' ) //comment
				continue;

			int i = 0;
			int found_nl = 0;
			while ( i < MAX_STRING && line[i] != 0) {
				if (line[i] == '\n') {
					line[i] = 0;
					found_nl = 1;
					break;
				}
				i++;
			}
			if ( ! found_nl) {
				ERRORPRINTF("Error loading ignored file names from %s: line %d too long. \n", ifilename, linenum);
				free(nm_item);
				return -1;
			}

			nm_item = malloc(sizeof(namemap_item_t));
			item_init(&nm_item->item);
			
			if ( namemap_load_items(line, nm_item->old_name, nm_item->new_name, MAX_STRING) != 0) {
				ERRORPRINTF("Error occurred reading file %s on line %d.\n", mfilename, linenum);
				free(nm_item);
				return -1;
			}			
			hash_table_insert(&ht_map, (key_t *) nm_item->old_name, &nm_item->item);
			linenum++;
		}
	} 
	return 0;
}

/** Deallocates structures used by this file.
 * */

void namemap_finish() {
	hash_table_destroy(&ht_map);
	namemap_item_t * nm_item;
	item_t * item = l_igns.head;

	while (item) {
		nm_item = list_entry(item, namemap_item_t, item);
		item = item->next;
		free(nm_item);
	}
}

/** Searches for mapping for given filename. It returns NULL if the file should be ignored,
 * mapped file name in case mapping was found or simply @a name if no change is necessary.
 *
 * @arg name file name for which to find mapping
 * @return old/new filename or NULL if the file should be ignored
 */

char * namemap_get_name(char * name) {
	namemap_item_t * nm_item;
	item_t * item = l_igns.head;
	int rv;

	while (item) {
		nm_item = list_entry(item, namemap_item_t, item);
		rv = fnmatch(nm_item->old_name, name, 0);
		if (rv == 0) {
			return NULL;
		} else if ( rv != FNM_NOMATCH ) {
			ERRORPRINTF("Error occured during matching name %s to string %s.\n", name, nm_item->old_name);
			return NULL; // it will be best to ignore this file...
		}
		item = item->next;
	}

	//it was not in ignore items, maybe it is in mapped items
	item = hash_table_find(&ht_map, (key_t *) name);

	if (item) {
		nm_item = list_entry(item, namemap_item_t, item);
		return nm_item->new_name;
	} else {
		//no change necessary:
		return name;
	}
}
