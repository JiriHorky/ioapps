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
#include <getopt.h>
#include <fcntl.h>
#include <libgen.h>
#include "common.h"
#include "ioreplay.h"
#include "replicate.h"
#include "stats.h"
#include "simulate.h"
#include "in_strace.h"
#include "in_binary.h"

#define FORMAT_STRACE "strace"
#define FORMAT_BIN "bin"

static struct option ioreplay_options[] = {
   /* name        has_arg flag  value */
   { "bind",			1,		NULL,	'b' },
   { "convert",			0,		NULL,	'c' },
   { "file",			1,		NULL,	'f' },
   { "format",			1,		NULL,	'F' },
   { "help",			0,		NULL,	'h' },
   { "ignore",			1,		NULL,	'i' },
   { "map",				0,		NULL,	'm' },
   { "output",			1,		NULL,	'o' },
   { "replicate",		0,		NULL,	'r' },
   { "prepare",		0,		NULL,	'p' },
   { "scale",			1,		NULL,	's' },
   { "stats",			1,		NULL,	'S' },
   { "timing",			1,		NULL,	't' },
   { "verbose",		0,		NULL,	'v' },
   { "version",		0,		NULL,	'V' },
   { NULL,				0,		NULL,	0 }
};

struct timeval global_start;

void help(char * name) {
	printf("Replicates all IO syscalls defined in file by -f option.\n\n");
printf("Usage: %s [OPERATION] -f <file> [OPTIONS]\n\n", name);
printf("%s is primary used to replicate recorded IO system calls.\n\
In order to do that, several other helper functionality exists.\n\n", name);

printf("Usage: %s -c -f <file> [-F <format>] [-o <out>] [-v]\n", name);
printf("   converts <file> in format <format> to binary form into file <out>\n\n");
printf("Usage: %s -S -f <file> [-v]\n", name);
printf("   displays some statistics about syscalls recorded in <file> (must be in " FORMAT_STRACE " format)\n\n");
printf("Usage: %s -C -f <file> [-F <format>] [-i <file>] [-m <file>] [-v]\n", name);
printf("   checks whether local enviroment is ready for replaying traces recorded in <file>.\n\n");
printf("Usage: %s -r -f <file> [-F <format>] [-t <mode>] [-s <factor>] [-b <number>] [-i <file>] [-m <file>] [-v]\n", name);
printf("   replicates traces recorded in <file>. Use -C prior to running this.\n");
printf("\n\
 -b --bind <number>  bind replicating process to processor number <number>\n\
 -c --convert        file to binary form, see also -o\n\
 -C --check          checks that all operations recorded in the file specied by -f will\n\
                     succeed (ie. will result in same return code).\n\
                     It takes -i and -m into account. See also -p.\n\
 -f --file <file>    sets filename to <file>\n\
 -F --format <fmt>   specifies input format of the file.\n\
                     Options: " FORMAT_STRACE ", " FORMAT_BIN ".\n\
                     Check README for details. Default is " FORMAT_STRACE ".\n\
 -h --help           prints this message\n\
 -i --ignore <file>  sets file containing names which we should not touch during\n\
                     replaying. I.e. no syscall operation will be performed on given file.\n\
 -m --map <file>     sets containing file names mapping. When opening file,\n\
                     if there is mapping for it, it will open mapped file instead.\n\
                     See README for more information.\n\
 -o --output <file>  output filename when converting. Default: strace.bin.\n\
 -p --prepare        will prepare all files accesses recorded in file specified by -f,\n\
                     so every IO operation will return with same exit code as in original\n\
                     application. See also -i and/or -m parameters.\n\
                     Do nothing at the moment.\n\
 -r --replicate      will replicate every operation stored in file specified by -f\n\
 -s --scale <factor> scales delays between calls by the factor <factor>. Used with -r.\n\
 -S --stats          generate stats when processing the file. Can be combined with other\n\
                     options.\n\
 -t --timing         sets timing mode for replication. Options available:\n\
                      asap  - default mode, makes calls one just after another.\n\
                      diff  - makes sure that gaps between calls are the same as in the original run.\n\
                      exact - makes sure that calls are (approximately) done in the same time as in the original run\n\
                              (relative from start of the application)\n\
 -v --verbose be more verbose (do nothing at the moment)\n\
 -V --version prints version and exits.\n");
}

void print_version() {
	printf("ioreplay v%s, part of IOapps\n\
\n\
Copyright (C) 2010 Jiri Horky <jiri.horky@gmail.com>\n\
\n\
License GPLv2: GNU GPL version 2 <http://gnu.org/licenses/gpl.html>\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
\n\
Written by Jiri Horky, 2010\n", VERSION);
}


int main(int argc, char * * argv) {
	char format[MAX_STRING] = FORMAT_STRACE;
	char filename[MAX_STRING] = "strace.file";
	char output[MAX_STRING] = "strace.bin";
	char ignorefile[MAX_STRING] = "";
	char mapfile[MAX_STRING] = "";
	list_t * list;
	int retval;
	int action = 0;
	int verbose = 0;
	char c;
	int cpu;
	double scale = 1.0;
#ifdef _SC_NPROCESSORS_ONLN	 //we can determine how many processors we have
	srandom((int)time(NULL));
	cpu = random() % sysconf(_SC_NPROCESSORS_ONLN);
#else
	cpu = 0;
#endif

	gettimeofday(&global_start, NULL);

	/* Parse parameters */
	while ((c = getopt_long (argc, argv, "b:cCf:F:hi:m:o:rs:St:vV", ioreplay_options, NULL)) != -1 ) {
		switch (c) {
			case 'b':
				cpu = atoi(optarg);
				break;
			case 'c':
				action |= ACT_CONVERT;
				break;
			case 'C':
				action |= ACT_CHECK;
				break;
			case 'f':
				strncpy(filename, optarg, MAX_STRING);
				break;
			case 'F':
				strncpy(format, optarg, MAX_STRING);
				break;
			case 'h':
				help(basename(argv[0]));
				return 0;
				break;
			case 'i':
				strncpy(ignorefile, optarg, MAX_STRING);
				break;
			case 'm':
				strncpy(mapfile, optarg, MAX_STRING);
				break;
			case 'o':
				strncpy(output, optarg, MAX_STRING);
				break;
			case 'r':
				action |= ACT_REPLICATE;
				break;
			case 'p':
				action |= ACT_PREPARE;
				break;
			case 's':
				scale = atof(optarg);
				if (scale == 0) {
					fprintf(stderr, "Error parsing scale parameter\n");
					exit(-1);
				}
				break;
			case 'S':
				action |= ACT_STATS;
				break;
			case 't':
				if ( ! strcmp(TIME_DIFF_STR, optarg) ) {
					action |= TIME_DIFF;
				} else if ( ! strcmp(TIME_EXACT_STR, optarg) ) {
					action |= TIME_EXACT;
				} else if ( ! strcmp(TIME_ASAP_STR, optarg) ) {
					action |= TIME_ASAP;
				} else {
					fprintf(stderr, "Unknown timemode specified.\n");
					exit(-1);
				}
				break;
			case 'v':
				verbose = 1;
				break;
			case 'V':
				print_version();
				return 0;
				break;
			default:
				fprintf(stderr, "Unknown parameter: %s\n", argv[optind-1]);
				return -1;
				break;
		}
	}
	
	list = (list_t *)malloc(sizeof(list_t));
	list_init(list);

	if ( !strcmp(format, FORMAT_STRACE)) {
		if ( (retval = strace_get_items(filename, list, action & ACT_STATS)) != 0) {
			DEBUGPRINTF("Error parsing file %s, exiting\n", filename);
			return retval;
		}
	} else if ( !strcmp(format, FORMAT_BIN)) {
		if ( (retval = bin_get_items(filename, list)) != 0) {
			DEBUGPRINTF("Error parsing file %s, exiting\n", filename);
			return retval;
		}
	} else {
		ERRORPRINTF("Unknown format identifier: %s\n", format);
		return -1;
	}

	DEBUGPRINTF("Loading done, %zd items loaded.\n", list_length(list));
	char * ifilename = ignorefile;
	if (strlen(ignorefile) == 0) {
		ifilename = NULL;
	}
	char * mfilename = mapfile;
	if (strlen(mapfile) == 0) {
		mfilename = NULL;
	}

	if (action & ACT_CONVERT) {
		DEBUGPRINTF("Saving in binary form...%s", "\n");
		bin_save_items(output, list);
	} else if (action & ACT_SIMULATE) {
		simulate_init(ACT_SIMULATE);
		if (replicate(list, cpu, scale, SIM_MASK, ifilename, mfilename) != 0) {
			ERRORPRINTF("An error occurred during replicating.%s", "\n");
		}
		simulate_finish();
	} else if (action & ACT_CHECK) {
		simulate_init(ACT_CHECK);
		if (replicate(list, cpu, scale, SIM_MASK, ifilename, mfilename) != 0) {
			ERRORPRINTF("An error occurred during replicating.%s", "\n");
		}
		simulate_check_files();
		simulate_finish();
	} else if (action & ACT_PREPARE) {
		simulate_init(ACT_PREPARE);
		if (replicate(list, cpu, scale, SIM_MASK, ifilename, mfilename) != 0) {
			ERRORPRINTF("An error occurred during replicating.%s", "\n");
		}
		///< @todo to change
		//simulate_prepare_files();
		simulate_finish();
	} else if (action & ACT_REPLICATE) {
		DEBUGPRINTF("Starting of replicating...%s", "\n");
		/// < @todo to change
		if (replicate(list, cpu, scale, REP_MASK | (action & TIME_MASK), ifilename, mfilename) != 0) {
			ERRORPRINTF("An error occurred during replicating.%s", "\n");
		}
	} else {
		ERRORPRINTF("No action specified!%s", "\n");
	}

	remove_items(list);
	free(list);
	return 0;
}
