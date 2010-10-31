#!/usr/bin/python

from distutils.core import setup, Extension

setup(name="ioprofiler",
		version="1.0b",
		description="IO profiler, part of IOApps",
		author="Jiri Horky",
		author_email="jiri.horky@gmail.com",
		url="http://code.google.com/p/ioapps/",
		package_date = [ "README", "../COPYING" ],
		ext_modules = [Extension("ioprofiler",
										define_macros = [('_GNU_SOURCE', None), ('_FILE_OFFSET_BITS',64), ('PY_MODULE', None)],
										extra_compile_args = ["-g"],
										sources = ["ioprofilermodule.c", "../in_common.c", "../in_binary.c", "../in_strace.c", "../adt/list.c", 
											"../adt/hash_table.c", "../namemap.c", "../simulate.c", "../replicate.c", "../fdmap.c", "../stats.c", "../simfs.c", "../adt/fs_trie.c"],
										include_dirs = ['../'])],
		py_modules = [ 'grapher', 'ioprofiler' ]
		)
