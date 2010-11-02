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

#ifndef _COMMON_H_
#define _COMMON_H_

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>

#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)

// debugging macros so we can pin down message provenance at a glance
#define WHERESTR  "[%s:%s:%d]: "
#define WHEREARG  __FILE__, __FUNCTION__,  __LINE__

#ifndef NDEBUG
#define DEBUGPRINT2(...)       fprintf(stderr, __VA_ARGS__)
#define DEBUGPRINTF(_fmt, ...)  DEBUGPRINT2(WHERESTR _fmt, WHEREARG, __VA_ARGS__)
#else
#define DEBUGPRINTF(_fmt, ...)  while(0) {}
#endif

#define ERRORPRINT2(...)       fprintf(stderr, __VA_ARGS__)
#define ERRORPRINTF(_fmt, ...)  ERRORPRINT2("E!!" WHERESTR _fmt, WHEREARG, __VA_ARGS__)

#define HASH_TABLE_SIZE 1019

#define MAX_STRING 512
#define MAX_LINE 512
#define MAX_DATA (1024*1024*64)
#define MAX_TIME_STRING 20
#define MAX_PARENT_IDS 20
#define VERSION "1.0"

#define SIM_MASK 0x1
#define REP_MASK 0x2

#define SHM_KEY 0x00BEEF00
#define SHM_SIZE (10*1024*1024)

#define OP_WRITE 'w'
#define OP_READ 'r'

#define OP_ACCESS 'a'
#define OP_UNKNOWN '?'
#define OP_CLOSE 'c'
#define OP_OPEN 'o'
#define OP_CREAT 'R'
#define OP_UNLINK 'u'
#define OP_LSEEK 'l'
#define OP_LLSEEK 'L'
#define OP_PIPE 'p'
#define OP_DUP 'd'
#define OP_DUP2 'D'
#define OP_DUP3 'e'
#define OP_MKDIR 'M'
#define OP_RMDIR 'i'
#define OP_MMAP 'm'
#define OP_CLONE 'C'
#define OP_SOCKET 'S'
#define OP_STAT 's'

// Timing modes
#define TIME_DIFF  0x80 ///< Try to hold the same difference between calls
#define TIME_DIFF_STR "diff"
#define TIME_EXACT 0x100 ///< Try to make calls in same time relative from start of the program
#define TIME_EXACT_STR "exact"
#define TIME_ASAP 0x40 ///< make calls as soon as possible
#define TIME_ASAP_STR "asap"
#define TIME_MASK 0x1C0

#define ACT_MASK 0x0000007F
#define ACT_CONVERT 0x1
#define ACT_SIMULATE 0x2
#define ACT_REPLICATE 0x4
#define ACT_STATS 0x8
#define ACT_CHECK 0x10
#define ACT_PREPARE 0x20

/** Our own version of struct timeval structure - the reason for it is to make sure
	it will be of equal size on both 32 and 64bit platforms. It will overflow in some
   100 years, so we don't have to worry about it. 
	Some attention should be given when using it with correct struct timeval structure.
*/


struct int32timeval {
	int32_t tv_sec;     /* seconds */
   int32_t tv_usec;    /* microseconds */
};


/** Information common to all operations */
typedef struct op_info {
	int32_t pid; ///< pid of calling process
	int32_t dur; ///< duration of operation in us
	struct int32timeval start; ///< start of operation
} op_info_t;

typedef struct read_op {
	int32_t fd;
	int64_t size;
	int64_t retval;
	op_info_t info;
} read_op_t;

typedef struct write_op {
	int32_t fd;
	int64_t size;
	int64_t retval;
	op_info_t info;
} write_op_t;

typedef struct pipe_op {
	int32_t fd1;
	int32_t fd2;	
	mode_t mode;
	int32_t retval;
	op_info_t info;
} pipe_op_t;

typedef struct mkdir_op {
	char name[MAX_STRING];
	mode_t mode;
	int32_t retval;
	op_info_t info;
} mkdir_op_t;

typedef struct rmdir_op {
	char name[MAX_STRING];
	int32_t retval;
	op_info_t info;
} rmdir_op_t;

typedef struct clone_op {
	mode_t mode;
	int32_t retval;
	op_info_t info;
} clone_op_t;

typedef struct dup_op {
	int32_t new_fd;
	int32_t old_fd;
	int32_t flags;
	int32_t retval;
	op_info_t info;
} dup_op_t;

typedef struct open_op {
	char name[MAX_STRING]; // we don't expect to have many open files, so we will not be allocating this manually
	int32_t flags;
	mode_t mode;
	int32_t retval;
	op_info_t info;
} open_op_t;

typedef struct close_op {
	int32_t fd;
	int32_t retval;
	op_info_t info;
} close_op_t;

typedef struct unlink_op {
	char name[MAX_STRING];
	int32_t retval;
	op_info_t info;
} unlink_op_t;

typedef struct llseek_op {
	int32_t fd;
	int64_t offset;
	int64_t f_offset; ///< final offset in the file
	int32_t flag;
	int64_t retval;
	op_info_t info;
} llseek_op_t;

typedef struct lseek_op {
	int32_t fd;
	int32_t flag;
	int64_t offset;
	int64_t retval; ///< this one returns final offset as return value
	op_info_t info;
} lseek_op_t;

typedef struct access_op {
	char name[MAX_STRING];
	mode_t mode;
	int32_t retval;
	op_info_t info;
} access_op_t;

typedef struct stat_op {
	char name[MAX_STRING];
	int32_t retval;
	op_info_t info;
} stat_op_t;

typedef struct socket_op {
	int32_t retval;
	op_info_t info;
} socket_op_t;

void * attach_sh_mem();

#endif

