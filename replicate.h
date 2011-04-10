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

#ifndef _REPLICATE_H_
#define _REPLICATE_H_

#include <adt/list.h>
#include "common.h"
#include "in_common.h"

//ignore this operation - do not replicate it
#define O_IGNORE 020000000000  //31st bit
#define S_IFIGNORE ((1 << 30)-1) // first 30 bits are 1

int replicate(list_t * list, int cpu, double scale, int sim_mode, char * ifile, char * mfile);
void replicate_clone(clone_item_t * op_it, int op_mask);
void replicate_open(open_item_t * op_it, int op_mask);
#endif
