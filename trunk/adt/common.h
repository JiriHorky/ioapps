/**
 * @file common.h
 *
 * Common container functions.
 *
 * These container macros are shamelessly borrowed from
 * the linux kernel.
 *
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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef CONTAINER_COMMON_H_
#define CONTAINER_COMMON_H_

/**
 */

#undef offsetof
#ifdef __compiler_offsetof
	#define offsetof(TYPE,MEMBER) __compiler_offsetof(TYPE,MEMBER)
#else
	#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

/** Cast a member of a structure out to the containing structure.
 * The result is NULL if the pointer is NULL.
 * @param ptr		the pointer to the member.
 * @param type		the type of the container struct this is embedded in.
 * @param member	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ((ptr) == NULL ? NULL : ({	\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );}))

#endif
