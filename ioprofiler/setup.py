#!/usr/bin/python
#    IOapps, IO profiler and IO traces replayer
#
#    Copyright (C) 2010 Jiri Horky <jiri.horky@gmail.com>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA


from distutils.core import setup, Extension

setup(name="ioprofiler",
		version="1.4-r1",
		description="IO profiler, part of IOApps",
		author="Jiri Horky",
		author_email="jiri.horky@gmail.com",
		url="http://code.google.com/p/ioapps/",
		package_data = [ "README", "../COPYING" ],
		ext_modules = [Extension("ioapps",
										define_macros = [('_GNU_SOURCE', None), ('_FILE_OFFSET_BITS',64), ('PY_MODULE', None)],
										extra_compile_args = ["-g"],
										sources = ["ioappsmodule.c", "../in_common.c", "../in_binary.c", "../in_strace.c", "../adt/list.c", 
											"../adt/hash_table.c", "../namemap.c", "../simulate.c", "../replicate.c", "../fdmap.c", "../stats.c", "../simfs.c", "../adt/fs_trie.c"],
										include_dirs = ['../'])],
		py_modules = [ 'grapher' ],
		scripts=['ioprofiler']
		)
