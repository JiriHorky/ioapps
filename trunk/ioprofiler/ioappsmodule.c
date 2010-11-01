/* IOtool, IO profiler and IO traces replayer

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

#include <Python.h>
#include <ctype.h>
#include "../common.h"
#include "../adt/list.h"
#include "../simulate.h"
#include "../replicate.h"
#include "../in_binary.h"
#include "../in_strace.h"

list_t * list_g = NULL;
PyObject * read_dict_g = NULL;
PyObject * write_dict_g = NULL;

/* The module doc string */
PyDoc_STRVAR(ioapps__doc__,
"Repio module. It benefits from ioapps C program (loading, normalizing functions).");

/* The function doc strings */
PyDoc_STRVAR(init_items_bin__doc__,
		"Arguments: filename. Loads syscalls from file in binary form. Sets context for other calls.");

PyDoc_STRVAR(init_items_strace__doc__,
		"Arguments: filename. Loads syscalls from file in strace form. Sets context for other calls.");

PyDoc_STRVAR(free_items__doc__,
		"Arguments: none. Remove previously allocated items.");

static PyObject * init_items_bin(PyObject *self, PyObject *args) {
	char * filename;

	if (! list_g) {
		list_g = malloc(sizeof(list_t));
		list_init(list_g);
	} else {
		PyErr_SetString(PyExc_ValueError, "List of syscalls already initialized!");
		return NULL;
	}

	/* The ':init_items_binary' is for error messages */
	if (!PyArg_ParseTuple(args, "s:init_items_binary", &filename))
		return NULL;
	
	/* Call the C function */
	if (bin_get_items(filename, list_g)) {
		PyErr_SetString(PyExc_ValueError, "Error loading list of syscalls.");
		return NULL;
	}
	
	return Py_None;
}

static PyObject * init_items_strace(PyObject *self, PyObject *args) {
	char * filename;

	if (! list_g) {
		list_g = malloc(sizeof(list_t));
		list_init(list_g);
	} else {
		PyErr_SetString(PyExc_ValueError, "List of syscalls already initialized!");
		return NULL;
	}

	/* The ':init_items_strace' is for error messages */
	if (!PyArg_ParseTuple(args, "s:init_items_strace", &filename))
		return NULL;
	
	/* Call the C function */
	if (strace_get_items(filename, list_g, 0)) {
		PyErr_SetString(PyExc_ValueError, "Error loading list of syscalls.");
		return NULL;
	}
	
	return Py_None;
}

static PyObject * simulate(PyObject *self, PyObject *args) {
	if (! list_g) {
		PyErr_SetString(PyExc_ValueError, "List of syscalls not initialized!");
		return NULL;
	}

	simulate_init(ACT_SIMULATE);
	/* Call the C function */
	if (replicate(list_g, 0, 1, SIM_MASK, NULL, NULL)) {
		PyErr_SetString(PyExc_ValueError, "Error simulating of syscalls.");
		return NULL;
	}
	return Py_None;
}

PyObject *  make_pylist_from_list(list_t * list) {
	PyObject * tuple;
	rw_op_t * rw_op;
	item_t * cur = list->head;
	PyObject * pylist;
	double starttime, dur;
	double size;
	double offset;
		
	if ( (pylist = PyList_New(0)) == NULL) {
		PyErr_SetString(PyExc_ValueError, "Cannot create python list!");
		return NULL;
	}

	int i = 0;
	while(cur) {
		rw_op = list_entry(cur, rw_op_t, item);
		starttime = rw_op->start.tv_sec;
		starttime += rw_op->start.tv_usec / 1000000.0L;
		dur = rw_op->dur / 1000.0L;
		offset = rw_op->offset / (1024.0);
		size = rw_op->size / (1024.0);
		tuple = Py_BuildValue("(dddd)", offset, size, starttime, dur);
		if ( PyList_Append(pylist, tuple) ) {
			return NULL;
		}
		cur = cur->next;
		i++;
	}

	return pylist;
}

void make_keyval_from_item_read(item_t * item) {
	PyObject * tuple;
	if (PyErr_Occurred())
		return;

	sim_item_t * sim_item = hash_table_entry(item, sim_item_t, item);
	PyObject * list = make_pylist_from_list(&sim_item->list);
	
	if (list) {
		double time_open = sim_item->time_open.tv_sec;
		time_open += sim_item->time_open.tv_usec / 1000000.L;
		if ( (tuple = Py_BuildValue("(dO)", time_open, list)) == NULL )
			return;

		PyDict_SetItemString(read_dict_g, sim_item->name, tuple);
	}
}

void make_keyval_from_item_write(item_t * item) {
	PyObject * tuple;
	if (PyErr_Occurred())
		return;

	sim_item_t * sim_item = hash_table_entry(item, sim_item_t, item);
	PyObject * list = make_pylist_from_list(&sim_item->list);

	if (list) {
		double time_open = sim_item->time_open.tv_sec;
		time_open += sim_item->time_open.tv_usec / 1000000.L;
		if ( (tuple = Py_BuildValue("(dO)", time_open, list)) == NULL )
			return;

		PyDict_SetItemString(write_dict_g, sim_item->name, tuple);
	}
}

static PyObject * get_reads(PyObject *self, PyObject *args) {
	hash_table_t * ht;

	/* Call the C function */
	if ( (ht = simulate_get_map_read()) == NULL) {
		PyErr_SetString(PyExc_ValueError, "Error getting simulation data, did you forget to call simulate?");
		return NULL;
	}
	
	if ( ! read_dict_g ) {
		read_dict_g = PyDict_New();
	} else {
		return read_dict_g;
	}

	hash_table_apply(ht, make_keyval_from_item_read);
	if (PyErr_Occurred()) {
		return NULL;
	} else {
		return read_dict_g;
	}
}

static PyObject * get_writes(PyObject *self, PyObject *args) {
	hash_table_t * ht;


	/* Call the C function */
	if ((ht = simulate_get_map_write()) == NULL) {
		PyErr_SetString(PyExc_ValueError, "Error getting simulation data, did you forget to call simulate?");
		return NULL;
	}
	
	if ( ! write_dict_g ) {
		write_dict_g = PyDict_New();
	} else {
		return write_dict_g;
	}

	hash_table_apply(ht, make_keyval_from_item_write);
	if (PyErr_Occurred()) {
		return NULL;
	} else {		
		return write_dict_g;
	}
}

static PyObject * free_items(PyObject *self, PyObject *args) {
	/* Call the C function */
	if (remove_items(list_g)) {
		PyErr_SetString(PyExc_ValueError, "Error removing list of syscalls.");
		return NULL;
	}
	free(list_g);
	list_g = NULL;

	return Py_None;
}

/*
void finish() {
	simulate_finish();
}
*/

/* A list of all the methods defined by this module. */
/* "iterate_point" is the name seen inside of Python */
/* "py_iterate_point" is the name of the C function handling the Python call */
/* "METH_VARGS" tells Python how to call the handler */
/* The {NULL, NULL} entry indicates the end of the method definitions */
static PyMethodDef ioapps_methods[] = {
	{"init_items_bin",  init_items_bin, METH_VARARGS, init_items_bin__doc__},
	{"init_items_strace",  init_items_strace, METH_VARARGS, init_items_strace__doc__},
	{"free_items",  free_items, METH_VARARGS, free_items__doc__},
	{"simulate",  simulate, METH_VARARGS, free_items__doc__},
	{"get_reads",  get_reads, METH_VARARGS, free_items__doc__},
	{"get_writes",  get_writes, METH_VARARGS, free_items__doc__},
	{NULL, NULL}      /* sentinel */
};

/* When Python imports a C module named 'X' it loads the module */
/* then looks for a method named "init"+X and calls it.  Hence */
/* for the module "mandelbrot" the initialization function is */
/* "initmandelbrot".  The PyMODINIT_FUNC helps with portability */
/* across operating systems and between C and C++ compilers */

PyMODINIT_FUNC initioapps(void) {
	/* There have been several InitModule functions over time */
	Py_InitModule3("ioapps", ioapps_methods, ioapps__doc__);
}
