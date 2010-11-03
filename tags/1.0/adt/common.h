/**
 * @file common.h
 *
 * Common container functions.
 *
 */

#ifndef CONTAINER_COMMON_H_
#define CONTAINER_COMMON_H_

/**
 * These container macros are shamelessly borrowed from
 * the linux kernel.
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
