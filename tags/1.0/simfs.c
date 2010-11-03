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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "simfs.h"
#include "in_common.h"
#include "ioreplay.h"

trie_t * fs;
int simfs_mask;
void (* simfs_apply_function)(simfs_t * simfs);
void (* simfs_apply_function_full)(simfs_t * simfs, char * full_name);

trie_node_t * simfs_new_trie_node() {
	simfs_t * simfs = malloc(sizeof(simfs_t));
	simfs->physical = 0;
	simfs->phys_size = -1;
	simfs->virt_size = 0;
	simfs->created = 0;
	return & simfs->node;
}

trie_node_t * simfs_new_trie_node_phys() {
	simfs_t * simfs = malloc(sizeof(simfs_t));
	simfs->physical = 1;
	simfs->phys_size = 0;
	simfs->virt_size = 0;
	simfs->created = 0;
	return & simfs->node;
}

void simfs_delete_trie_node(trie_node_t * node) {
	simfs_t * simfs = trie_get_instance(node, simfs_t, node);
	free(node->key);
	free(simfs);
}


/** Polish path name, ie. it destroys all '..' and '.' in path and also make sure the path is absolute,
 * ie. current directory is prepended to the path. 
 * @arg name path name to be polished
 * @return absolute path name of the name without any ".." in it.
 */

void simfs_absolute_name(const char * name, char * buff, int size) {
	char * s;

	strcpy(buff,name);

	if (name[0] != '/') {//not absolute path
		if ( getcwd(buff, size) == NULL) {
			ERRORPRINTF("Current path dir exceeds compiled maximum of %d bytes. Recompile with bigger limit.\n",
					size);
			exit(0);
		}
		if (strlen(buff) + strlen(name) + 1 + 1 > size) {
			ERRORPRINTF("Current path name+ access path name exceeds compiled maximum of %d bytes. Recompile with bigger limit.\n",
					size);
			exit(0);
		}
		strcat(buff, "/");
		strcat(buff, name);
	} else {
		strcpy(buff, name);
	}

	char * ss;
	int off;
	while ((s = strstr(buff, "/..")) != NULL ) { //directory name can't have "/.." in its name
		off = (int)(s - buff);
		if (off == 0) { //beggining of the string
				s = buff + 3;
				int i = 0;
				while (*s) {
					buff[i] = *s;
					i++; s++;
				}
				buff[i] = 0;
		} else { // get rid of it
			ss = s+3; //one char after "/.."
			*s = 0;
			s = rindex(buff, '/'); 
			assert(s); //there must be one...
			while (*ss) {
				*s = *ss;
				s++; ss++;
			}
			*s = 0;
		}
	}

	while ( (s = strstr(buff, "/.")) != NULL ) { //directory name can't have "/." in its name
		if (s == buff) { //beggining of the string
			if ( strlen(buff) == 2 ) { //only this
				strcpy(buff, "/");
			} else {
				s = buff + 2;
				int i = 0;
				while (*s) {
					buff[i] = *s;
					i++; s++;
				}
				buff[i] = 0;
			}
		} else { // get rid of it
			ss = s+2; // end od/beggining of '/.'
			while(*ss) {
				*s = *ss;
				s++; ss++;
			}
			*s = 0;
		}
	}
}

/** Checks whether all @a missing directories/files exists on disks starting from path @a existing.
 * @arg existing where to start checking
 * @arg missing missing part to check
 * @return 1 if all exists, 0 if not.
 */

int simfs_populate(char * existing, char * missing) {
	int rv;
	char * strtmp;
	char * saveptr;
	trie_node_t * n;
	struct stat st;
	char * s;
	char * c;
	int i;
	short slash_added = 0;
	//char name[MAX_LINE];
	strtmp = strdup(missing);
	s = strtok_r(strtmp, "/", &saveptr);
	int mis_off = 0;
	rv = 1;


	while(s) {
		int l = strlen(existing);
		c = existing + l; //end of buff string
		slash_added = 0;
		if (! *existing || existing[l-1] != '/') { //if it is empty string or there isn't ending slash character
			strcat(existing, "/");			
			slash_added = 1;
		}
		strcat(existing, s);
		if ( access(existing, F_OK) == 0) { //it exists on the disk
			n = trie_insert2(fs, existing, simfs_new_trie_node_phys);
			simfs_t * simfs = trie_get_instance(n, simfs_t, node);
			stat(existing, &st);
			simfs->phys_size = st.st_size;
		} else {
			*c = 0; //cut off last non-existing part
			rv = 0;
			break;
		}
		mis_off += slash_added + strlen(s);
		s = strtok_r(NULL, "/", &saveptr);
	}

	//polish also the missing string
	i = 0;
	if (mis_off != 0) { 
		while(missing[mis_off + i]) {
			missing[i] = missing[mis_off+i];
			i++;
		}
		missing[i] = 0;
	}

	free(strtmp);
	return rv;
}

/** Inits all structures needed by simfs. You have to call this function before using other simfs_* functions.
 */

void simfs_init(int mask) {
	simfs_t * simfs;
	fs = malloc(sizeof(trie_t));
	trie_init(fs, '/', simfs_new_trie_node, simfs_delete_trie_node);
	simfs = trie_get_instance(fs->root, simfs_t, node);
	simfs->physical = 1;
	simfs_mask = mask;
}

/** Frees all structure used by SimFs. Call this function when you will not access SimFS any longer.
*/

void simfs_finish() {
	///< @todo destroy trie
	trie_destroy(fs);
	simfs_mask = 0;
	free(fs);
}


/** Simulates access(2) system call on the SimFS. It takes appropriate actions in to fix fs, if needed.
 *
 * @arg access_op structure with all information about the call. It contains also return code that is expected. If
 *       zero, access should succeed, if non-zero, the file should not exists. In fact, there many other reasons
 *       for non-zero return value, but we are just not that smart (yet?).
 * @return zero, if everything went ok. SIMFS_ENOENT if the @a name doesn't exists, SIMFS_EENT if the @a name exists but 
			it should not.
 */

int simfs_access(access_op_t * access_op) {
	trie_node_t * node;
	simfs_t * simfs;
	int rv;
	int i = 0;
	char * missing;
	char name_buff[MAX_LINE];
	char * name;

	simfs_absolute_name(access_op->name, name_buff, MAX_LINE); 
	name = name_buff;
	char * buff = strdup(name);
	missing = strdup(name);
	
	node = trie_longest_prefix(fs, name, buff);
	simfs = trie_get_instance(node, simfs_t, node);

	if (strcmp(name, buff) == 0) { //the file/directory is there
		if (access_op->retval == 0) { //and it was expected
			rv = 0;
		} else {
			if (simfs->physical) {
				ERRORPRINTF("Previous access call to %s failed, but we would succeed. Delete the file %s.\n",
					name, name);
				trie_delete(fs, name);
				if (simfs_mask & ACT_PREPARE) {
					///< @todo really delete it 
				}
				rv = SIMFS_EENT;
			} else { // this is really strange, it only exists in virtual space, i.e. we created the file and now it should 
				      // exist there...source strace file corrupted?
				ERRORPRINTF("Previous access call to %s failed but the file was created by replicating. Corrupted source .strace file?\n",
						name);
				rv = SIMFS_EENT;
			}
		}
	} else { //the file is not there
		while (buff[i] && buff[i] == name[i]) //skip common part
			i++;

		strcpy(missing, name + i);

		int exists = simfs_populate(buff, missing);
		if (access_op->retval != 0 )  {//previous access call failed
			if ( exists ) { //would we fail?
				ERRORPRINTF("Previous access call to %s failed but we would succeed. Delete the file %s.\n",
					name, name);
				trie_delete(fs,name); // delete last node
				rv = SIMFS_EENT;
			} else {
				rv = 0;
			}
		} else { 
			if ( ! exists) {
				ERRORPRINTF("2File %s doesn't exist, only '%s' exists, create missing entries (%s): %s\n", name, buff, missing, strerror(errno));
				trie_insert(fs, name);
				if (simfs_mask & ACT_PREPARE) {
					///< @todo really create it
				}
			} else {
				rv = 0;
			}
		}
	}
	free(buff);
	return rv;
}

/** Simulates stat(2) system call on the SimFS. It takes appropriate actions in to fix fs, if needed.
 *
 * @arg stat_op structure with all information about the call. It contains also return code that is expected. If
 *       zero, stat should succeed, if non-zero, the file should not exists. In fact, there many other reasons
 *       for non-zero return value, but we are just not that smart (yet?).
 * @return zero, if everything went ok. SIMFS_ENOENT if the @a name doesn't exists, SIMFS_EENT if the @a name exists but 
			it should not.
 */

int simfs_stat(stat_op_t * stat_op) {
	trie_node_t * node;
	simfs_t * simfs;
	int rv;
	int i = 0;
	char * missing;
	char name_buff[MAX_LINE];
	char * name;

	simfs_absolute_name(stat_op->name, name_buff, MAX_LINE); 
	name = name_buff;
	char * buff = strdup(name);
	missing = strdup(name);
	
	node = trie_longest_prefix(fs, name, buff);
	simfs = trie_get_instance(node, simfs_t, node);

	if (strcmp(name, buff) == 0) { //the file/directory is there
		if (stat_op->retval == 0) { //and it was expected
			rv = 0;
		} else {
			if (simfs->physical) {
				ERRORPRINTF("Previous stat call to %s failed, but we would succeed. Delete the file %s.\n",
					name, name);
				trie_delete(fs, name);
				if (simfs_mask & ACT_PREPARE) {
					///< @todo really delete it 
				}
				rv = SIMFS_EENT;
			} else { // this is really strange, it only exists in virtual space, i.e. we created the file and now it should 
				      // exist there...source strace file corrupted?
				ERRORPRINTF("Previous stat call to %s failed but the file was created by replicating. Corrupted source .strace file?\n",
						name);
				rv = SIMFS_EENT;
			}
		}
	} else { //the file is not there
		while (buff[i] && buff[i] == name[i]) //skip common part
			i++;

		strcpy(missing, name + i);

		int exists = simfs_populate(buff, missing);
		if (stat_op->retval != 0 )  {//previous stat call failed
			if ( exists ) { //would we fail?
				ERRORPRINTF("Previous stat call to %s failed but we would succeed. Delete the file %s.\n",
					name, name);
				trie_delete(fs,name); // delete last node
				rv = SIMFS_EENT;
			} else {
				rv = 0;
			}
		} else { 
			if ( ! exists) {
				ERRORPRINTF("2File %s doesn't exist, only '%s' exists, create missing entries (%s): %s\n", name, buff, missing, strerror(errno));
				trie_insert(fs, name);
				if (simfs_mask & ACT_PREPARE) {
					///< @todo really create it
				}
			} else {
				rv = 0;
			}
		}
	}
	free(buff);
	return rv;
}

/** Simulates mkdir(2) system call on the SimFS. It takes appropriate actions to fix the fs, if needed.
 * @arg mkdir_op structure with all information about the call.
 * 
 * @return zero, if successful, SIMFS_ENOENT if component in path doesn't exist or SIMFS_EENT if the dir itself exists.
 */

int simfs_mkdir(mkdir_op_t * mkdir_op) {
	trie_node_t * node;
	simfs_t * simfs;
	int rv;
	int i = 0;
	char * missing;
	char name_buff[MAX_LINE];
	char * name;

	simfs_absolute_name(mkdir_op->name, name_buff, MAX_LINE); 
	name = name_buff;
	char * buff = strdup(name);
	missing = strdup(name);
	
	node = trie_longest_prefix(fs, name, buff);
	simfs = trie_get_instance(node, simfs_t, node);

	while (buff[i] && buff[i] == name[i]) //skip common part
		i++;

	strcpy(missing, name + i);

	simfs_populate(buff, missing); //make sure that buff and missing are reflecting current situation

	if (strcmp(name, buff) == 0) { //the directory is already there
		if (mkdir_op->retval != 0) { //and previous mkdir failed (maybe because it was there already)
			rv = 0;
		} else { //but previous mkdir succeeded
			if (simfs->physical) {
				ERRORPRINTF("Previous mkdir call of %s succeeded. But the dir already exists. Delete it.\n",
						name);
				if (simfs_mask & ACT_PREPARE) {
					///< @todo really delete it 
				}
				rv = SIMFS_EENT;
			} else { // this is really strange, it only exists in virtual space, i.e. we created the directory but it
				      // shouldn't...
				ERRORPRINTF("Previous mkdir call of %s succeeded but the dir already exists and was created by us. Corrupted source .strace file?\n",
						name);
				rv = SIMFS_EENT;
			}
		}
	} else { //dir is not there

		char would_succ; ///< whether the missing part is only new directory itself
		int len = strlen(missing);

		if ( missing[len-1] == '/' ) {
			missing[len-1] = 0; //delete last '/' char
		}

		if ( missing[0] == '/' ) {
				missing += 1;
		}
		int count = strccount(missing, '/');

		//this is really simplistic, not taking permissions into account...
		if (count == 0 ) {
			would_succ = 1;
		} else {
			would_succ = 0;
		}

		if (mkdir_op->retval != 0 )  {//previous mkdir call failed
			if ( would_succ == 0) { // and so we would...
				rv = 0;
			} else {
				if (simfs->physical) {
					ERRORPRINTF("Previous mkdir call to %s failed but we would succeed.\n",
						name);
				} else {
					ERRORPRINTF("Previous mkdir call to %s failed but we would succeed and it was me who created the path. Corrupted source .strace file?\n",
						name);
				}
				trie_delete(fs, name);
				rv = SIMFS_EENT;
			}
		} else {  //previous mkdir call succeeded
			if ( would_succ) {
				rv = 0;
			} else { //would_succ == 0
				ERRORPRINTF("Mkdir can't succeed as the path is not ready. Only '%s' exists, create missing entry for mkdir of (%s)\n", buff, name);
				if (simfs_mask & ACT_PREPARE) {
					///< @todo really create it
				}
				rv = SIMFS_ENOENT;
			}
			trie_node_t * n = trie_insert(fs, name);
			simfs_t * simfs = trie_get_instance(n, simfs_t, node);
			simfs->created = 1;
		}
	}
	free(buff);
	return rv;
}

/** Simulates unlink(2) system call on the SimFS. It takes appropriate actions to fix the fs, if needed.
 * @arg unlink_op structure with all information about the call.
 * @return zero if successful, SIMFS_ENOENT in case of missing file/dir and SIMFS_EENT in case of previously failed 
 *         unlink operation that would now succeed.
 */

int simfs_unlink(unlink_op_t * unlink_op) {
	trie_node_t * node;
	simfs_t * simfs;
	int rv;
	int i = 0;
	char * missing;
	char name_buff[MAX_LINE];
	char * name;

	simfs_absolute_name(unlink_op->name, name_buff, MAX_LINE); 
	name = name_buff;
	char * buff = strdup(name);
	missing = strdup(name);
	
	node = trie_longest_prefix(fs, name, buff);
	simfs = trie_get_instance(node, simfs_t, node);

	if (strcmp(name, buff) == 0) { //the file/directory is there
		if (unlink_op->retval == 0) { //and it was expected
			trie_delete(fs, name);
			rv = 0;
		} else {
			if (simfs->physical) {
				ERRORPRINTF("Previous unlink call to %s failed but we would (probably) succeed. Delete the file.\n", name);
				if (simfs_mask & ACT_PREPARE) {
					///< @todo really delete it 
				}
				rv = SIMFS_EENT;
			} else { // this is really strange, it only exists in virtual space, i.e. we created the file and now it should 
				      // exist there...source strace file corrupted?
				ERRORPRINTF("Previous unlink call to %s failed but the file was created by replicating. Corrupted source .strace file?\n",
						name);
				rv = SIMFS_EENT;
			}
			trie_delete(fs, name);
		}
	} else { //the file is not there
		while (buff[i] && buff[i] == name[i]) //skip common part
			i++;

		strcpy(missing, name + i);

		int exists = simfs_populate(buff, missing);

		if (unlink_op->retval != 0 )  {//previous unlink call failed, and probably we would fail too...
			if ( exists ) { //would we really fail?
				ERRORPRINTF("Previous unlink call to %s failed, but we would succeed. Delete the file %s.\n",
					name, name);
				trie_delete(fs, name);
				rv = SIMFS_EENT;
			} else {
				rv = 0;
			}
		} else {  //unlink succeeded
			if ( ! exists ) { // we created the longest prefix --> longer path cannot exist
				ERRORPRINTF("1File %s doesn't exist, only '%s' exists, create missing entries (%s)\n", name, buff, missing);
				trie_insert(fs, name); //create all missing entries
				trie_delete(fs, name);
				if (simfs_mask & ACT_PREPARE) {
					///< @todo really create it
				}
				rv = SIMFS_ENOENT;
			} else { //shorter path existed, maybe we have it on disk?
				ERRORPRINTF("2File %s doesn't exist, only '%s' exists, create missing entries (%s): %s\n", name, buff, missing, strerror(errno));
				trie_insert(fs, name);
				trie_delete(fs, name); //it was created in simfs so delete it
				if (simfs_mask & ACT_PREPARE) {
					///< @todo really create it
				}
				rv = SIMFS_ENOENT;
			}
		}
	}
	free(buff);
	return rv;
}

/** Simulates rmdir(2) system call on the SimFS. It takes appropriate actions to fix the fs, if needed.
 * @arg rmdir_op structure with all information about the call. The implementation is VERY simplistic.
 * @return zero if successful, SIMFS_ENOENT in case of missing file/dir and SIMFS_EENT in case of previously failed 
 *         rmdir operation that would now succeed.
 */

int simfs_rmdir(rmdir_op_t * rmdir_op) {
///< @todo rewrite. For now it is sufficient as we don't have information whether given node is a directory or a file,
//but this is really nasty. 
	int rv;
	unlink_op_t * unlink_op = malloc(sizeof(unlink_op_t));
	unlink_op->retval = rmdir_op->retval;
	strcpy(unlink_op->name, rmdir_op->name);
	unlink_op->info = rmdir_op->info;
	rv = simfs_unlink(unlink_op);
	free(unlink_op);
	return rv;
}



/** Simulates open(2)/creat(2) system call on the SimFS. It takes appropriate actions to fix the fs, if needed.
 * @arg open_op structure with all information about the call.
 * @return zero if successful, ENOENT in case of error.
 */

int simfs_creat(open_op_t * open_op) {
	trie_node_t * node;
	simfs_t * simfs;
	int rv;
	int i = 0;
	char * missing;
	char name_buff[MAX_LINE];
	char * name;

	simfs_absolute_name(open_op->name, name_buff, MAX_LINE); 
	name = name_buff;
	char * buff = strdup(name);
	missing = strdup(name);
	
	node = trie_longest_prefix(fs, name, buff);
	simfs = trie_get_instance(node, simfs_t, node);

	while (buff[i] && buff[i] == name[i]) //skip common part
		i++;

	strcpy(missing, name + i);

	simfs_populate(buff, missing); //make sure buff and missing reflect actual situation on the disk

	if (strcmp(name, buff) == 0) { //the file is already there
		if (open_op->flags & O_CREAT) {
			if ( open_op->flags & O_EXCL) { 
				if (open_op->retval == -1) { //and previous open failed (probably because it was there already)
					rv = 0;
				} else { //but previous open succeeded
					if (simfs->physical) {
						ERRORPRINTF("Previous open call (with O_EXCL) of %s succeeded. But the file already exists. Delete it.\n", name);
						if (simfs_mask & ACT_PREPARE) {
							///< @todo really delete it 
						}
						rv = SIMFS_EENT;
					} else { // this is really strange, it only exists in virtual space, i.e. we created the directory but it
						// shouldn't...
						ERRORPRINTF("Previous open call (with O_EXCL) of %s suceeded. But the file was created by replicating. Corrupted source .strace file?\n",
								name);
						rv = SIMFS_EENT;
					}
				}
			} else {	//Creating new file, without O_EXCL, file is there --> everything ok.							
				rv = 0;
			}
		} else { //Not O_CREAT 
			if (open_op->retval == -1) { //and previous open failed
				if (simfs->physical) {
					ERRORPRINTF("Previous open call of %s failed. But we would succeed. Delete the file?.\n",	name);
					trie_delete(fs, name);
					if (simfs_mask & ACT_PREPARE) {
						///< @todo really delete it 
					}
					rv = SIMFS_EENT;
				} else { // this is really strange, it only exists in virtual space, i.e. we created the directory but it
							// shouldn't...
					ERRORPRINTF("Previous open call to %s failed but the file was created by replicating. Corrupted source .strace file?\n",
							name);
					trie_delete(fs, name);
					rv = SIMFS_EENT;
				}
			} else { //previous open succeeded
				rv = 0;
			}
		}
	} else { //file is not yet there
		char would_succ; ///< whether the missing part is only new file itself

		if ( missing[0] == '/' ) {
			missing += 1; // "delete"  '/' char
		}

		int count = strccount(missing, '/');

		//this is really simplistic, not taking permissions into account...
		if (count == 0) {
			would_succ = 1;
		} else {
			would_succ = 0;
		}

		if ( ! (open_op->flags & O_CREAT)) { //not creating new file --> fail
			if (open_op->retval == -1)  {//previous open call failed
				rv = 0;
			} else {
				ERRORPRINTF("Open can't succeed as the file is not there. Only '%s' exists, create missing entry for open of (%s)\n", buff, name);
				trie_insert(fs, name);
				rv = SIMFS_ENOENT;
			}
		} else { // creating new file 
			if (open_op->retval == -1)  {//previous open call failed
				if ( would_succ == 0) { // and so we would...
					rv = 0;
				} else {
					if (simfs->physical) {
						ERRORPRINTF("Previous creatcall to %s failed, but we would succeed.\n",	name);
					} else {
						ERRORPRINTF("Previous creat call to %s failed, but we would succeed and it was me who created the path. Corrupted source .strace file?\n",
							name);
					}
					trie_delete(fs, name);
					rv = SIMFS_EENT;
				}
			} else {  //previous open call succeeded
				if ( would_succ) {
					rv = 0;
				} else { //would_succ == 0
					ERRORPRINTF("Creat can't succeed as the path is not ready. Only '%s' exists, create missing entry for creat of (%s)\n", buff, name);
					if (simfs_mask & ACT_PREPARE) {
						///< @todo really create it
					}
					rv = SIMFS_ENOENT;
				}
				trie_node_t * n = trie_insert(fs, name);
				simfs_t * simfs = trie_get_instance(n, simfs_t, node);
				simfs->created = 1;
			}
		}
	}
	free(buff);
	return rv;
}



/** Checks whether given file @a name exists in virutal filesystem and if it was only virtually created or it
 * exists on the disk too.
 *
 * @arg name filename to be check
 * @return SIMFS_PHYS if file physically existed, SIFMS_VIRT if it was created during simulation or SIMFS_ENOENT if 
 *         file doesn't exist 
 */

int simfs_has_file(const char * name) {
	trie_node_t * node;
	simfs_t * simfs;
	int rv;
	char * buff = strdup(name);
	node = trie_longest_prefix(fs, name, buff);
	simfs = trie_get_instance(node, simfs_t, node);

	if (strcmp(name, buff) == 0) { //the file is already there
		if (simfs->physical) {
			rv = SIMFS_PHYS;
		} else {
			rv = SIMFS_VIRT;
		}
	} else {
		rv = SIMFS_ENOENT;
	}
	free(buff);
	return rv;
}

/** Returns whether given simfs item @a simfs is file or not. It is quite silly, it only checks if it has
 * some children or not. @todo make it more clever...
 *
 * @arg simfs pointer to item to be checked
 * @return true if argument is file, false otherwise
 */

int simfs_is_file(simfs_t * simfs) {
	return trie_is_leaf(&simfs->node);
}


simfs_t * simfs_find(const char * name) {
	char name_buff[MAX_LINE];

	simfs_absolute_name(name, name_buff, MAX_LINE); 
	trie_node_t * node = trie_find(fs, name_buff);
	simfs_t * simfs;

	if (!node) {
		return NULL;
	} else {
		simfs = trie_get_instance(node, simfs_t, node);
		return simfs;
	}
}

void simfs_apply_wrapper_full(trie_node_t * node, char * full_name) {
	simfs_t * simfs = trie_get_instance(node, simfs_t, node);
	simfs_apply_function_full(simfs, full_name);
}

void simfs_apply_wrapper(trie_node_t * node) {
	simfs_t * simfs = trie_get_instance(node, simfs_t, node);
	simfs_apply_function(simfs);
}


void simfs_apply(void (* function)(simfs_t * item)) {
	simfs_apply_function = function;

	trie_apply(fs, simfs_apply_wrapper);
}

void simfs_apply_full_name(void (* function_full)(simfs_t * item, char * full_name)) {
	simfs_apply_function_full = function_full;

	trie_apply_full(fs, simfs_apply_wrapper_full);
}

void simfs_dump() {
	trie_dump(fs);
}

