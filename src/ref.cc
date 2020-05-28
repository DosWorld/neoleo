/*
 * $Id: ref.c,v 1.15 2001/02/13 23:38:06 danny Exp $
 *
 * Copyright (c) 1990, 1992, 1993, 2001 Free Software Foundation, Inc.
 * 
 * This file is part of Oleo, the GNU Spreadsheet.
 * 
 * Oleo is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * Oleo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Oleo; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>

#include <iostream>
#include <map>
#include <math.h>
#include <string>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <signal.h>


#include "global.h"
#include "io-abstract.h"
#include "io-generic.h"
#include "ref.h"
#include "cmd.h"
#include "sheet.h"
#include "logging.h"

using std::cout;
using std::endl;
//using std::vector;

static void add_ref_fm (struct ref_fm **where, CELLREF r, CELLREF c);
static void flush_ref_fm (struct ref_fm **, CELLREF, CELLREF);
static void flush_range_ref (struct rng *, CELLREF, CELLREF);
extern void shift_formula (int r, int c, int dn, int ov);
static void flush_ref_to (struct ref_to **);
static void flush_fm_ref (struct ref_fm *);

/* More tunable paramaters */

#define FIFO_START	40
#define FIFO_INC	*=2

#define TO_MAGIC(row,col)	(((long)(row)<<BITS_PER_CELLREF)|(col))
#define MAGIC_ROW(magic)	(((magic)>>BITS_PER_CELLREF)&CELLREF_MASK)
#define MAGIC_COL(magic)	((magic)&CELLREF_MASK)

#define BETWEEN(mid,lo,hi)	((mid>=lo)&&(mid<=hi))

static VOIDSTAR moving;

int timer_active = 0;
struct ref_fm *timer_cells;

CELL *my_cell;

#ifdef TEST
extern int debug;
#endif

/* Functions for dealing exclusively with variables */
std::map<std::string, struct var>the_vars_1;


/* For the fifo-buffer */
struct pos {
	CELLREF row;
	CELLREF col;
};

struct cell_buf {
	unsigned int size;
	struct pos *buf;
	struct pos *push_to_here;
	struct pos *pop_frm_here;
};


/* Set the cell ROW,COL to STRING, parsing string as needed */
	void
set_cell (CELLREF row, CELLREF col, const std::string& in_string)
{
	unsigned char *ret;

	cur_row = row;
	cur_col = col;

	std::string s2{in_string};
	while(s2.size() > 0 && s2[0] == ' ') s2.erase(0, 1);

	my_cell = find_cell (cur_row, cur_col);
	my_cell = find_or_make_cell(cur_row, cur_col);

}

extern int default_lock;

/* new_value() calls set_cell, but refuses to change locked cells, and
   updates and prints the results.  It returns an error msg on error. . .
   */

	char *
new_value (CELLREF row, CELLREF col, const char *string)
{
	CELL *cp;

	cp = find_cell (row, col);
	if (((!cp || GET_LCK (cp) == LCK_DEF) && default_lock == LCK_LCK) || (cp && GET_LCK (cp) == LCK_LCK))
	{
		return (char *) "cell is locked";
	}

	set_cell(row, col, string);
	if (my_cell)
	{
		my_cell->update_cell();
		io_pr_cell (row, col, my_cell);
		my_cell = 0;
	}
	Global->modified = 1;
	return 0;
}


/* --------- Routines for dealing with cell references to other cells ------ */


/* like add_ref, except over a range of arguments and with memory
 * management weirdness. 
 */
void add_range_ref (struct rng *rng)
{

	make_cells_in_range (rng);

}

	static void
flush_range_ref (struct rng *rng, CELLREF rr, CELLREF cc)
{
}

#define FM_HASH_NUM 503
#define TO_HASH_NUM 29
#ifdef TEST
static int fm_misses = 0;
static int to_misses = 0;
#endif

static struct ref_fm *fm_list[FM_HASH_NUM];
static struct ref_fm *fm_tmp_ref;
static unsigned fm_tmp_ref_alloc;

static struct ref_to *to_list[TO_HASH_NUM];
static struct ref_to *to_tmp_ref;
static unsigned to_tmp_ref_alloc;






	void
add_ref_to (cell* cp, int whereto)
{
}



/* ------------- Routines for dealing with moving cells -------------------- */

static struct rng *shift_fm;
static int shift_ov;
static int shift_dn;




#define RIGHT	8
#define LEFT	4
#define BOTTOM	2
#define TOP	1



/* ---------- Routines and vars for dealing with the eval FIFO ------------ */
static struct cell_buf cell_buffer;


/* Push the cells in REF onto the FIFO.  This calls push_cell to do the
   actual work. . . */
void push_refs (cell *cp)
{
}

/* Push a cell onto the FIFO of cells to evaluate, checking for cells
   that are already on the FIFO, etc.

   This does not implement best-order recalculation, since there may be
   intersecting branches in the dependency tree, however, it's close enough
   for most people.
   */
static void cell_buffer_contents (FILE *fp);

	void
push_cell (CELLREF row, CELLREF col)
{
	ASSERT_UNCALLED();
}

/* Pop a cell off CELL_BUFFER, and evaluate it, displaying the result. . .
   This returns 0 if there are no more cells to update, or if it gets
   an error. */

	int
eval_next_cell (void)
{
	CELL *cp;
	static int loop_counter = 40;

	if (cell_buffer.pop_frm_here == cell_buffer.push_to_here)
		return 0;

	cur_row = cell_buffer.pop_frm_here->row;
	cur_col = cell_buffer.pop_frm_here->col;
	cell_buffer.pop_frm_here++;
	if (cell_buffer.pop_frm_here == cell_buffer.buf + cell_buffer.size)
		cell_buffer.pop_frm_here = cell_buffer.buf;

	if (!(cp = find_cell(cur_row, cur_col)))
		return 0;


	cp->update_cell();
	io_pr_cell (cur_row, cur_col, cp);
	return loop_counter;
}



/* This sets the variable V_NAME to V_NEWVAL
 * It returns error msg, or 0 on success.
 * all the appropriate cells have their ref_fm arrays adjusted appropriately
 * This could be smarter; when changing a range var, only the cells that
 * were in the old value but not in the new one need their references flushed,
 * and only the cells that are new need references added.
 * This might also be changed to use add_range_ref()?
 */

	char *
new_var_value (char *v_name, int v_namelen, struct rng *rng)
{
	struct var *var;
	int n = 0;
	int newflag = 0;

	cur_row = MIN_ROW;
	cur_col = MIN_COL;

	//newflag = ((ROWREL | COLREL) == (R_CELL | ROWREL | COLREL)) ? VAR_CELL : VAR_RANGE;

	var = find_or_make_var (v_name, v_namelen);

	if (var->var_ref_fm)
	{
		if (var->var_flags != VAR_UNDEF)
		{
			for (n = 0; n < var->var_ref_fm->refs_used; n++)
			{
				flush_range_ref (&(var->v_rng),
						var->var_ref_fm->fm_refs[n].ref_row,
						var->var_ref_fm->fm_refs[n].ref_col);
			}
		}
		var->v_rng = *rng;

		if (var->v_rng.lr != NON_ROW)
		{
			for (n = 0; n < var->var_ref_fm->refs_used; n++)
			{
				cur_row = var->var_ref_fm->fm_refs[n].ref_row;
				cur_col = var->var_ref_fm->fm_refs[n].ref_col;
				add_range_ref (&(var->v_rng));
			}
		}
		for (n = 0; n < var->var_ref_fm->refs_used; n++)
			push_cell (var->var_ref_fm->fm_refs[n].ref_row,
					var->var_ref_fm->fm_refs[n].ref_col);
	}
	else
		var->v_rng = *rng;

	var->var_flags = newflag;

	return 0;
}

	void
for_all_vars (void (*func) (const char *, struct var *))
{
	for(auto it = the_vars_1.begin(); it != the_vars_1.end() ; ++it) {
		auto s1{it->first};
		const char* s3 = s1.c_str();
		func(s3, &(it->second));
	}
}

struct var *find_var_1(const char* str)
{
	auto it = the_vars_1.find(str);
	if(it != the_vars_1.end())
		return &(it ->second);
	else
		return nullptr;
}

/* Find a variable in the list of variables, or create it if it doesn't
   exist.  Takes a name and a length so the name doesn't have to be
   null-terminated
   */
	struct var *
find_or_make_var(const char *string, int len)
{
	log_debug("find_or_make_var called");
	assert(strlen(string) >= len);
	std::string varname;
	for(int i=0; i<len; ++i) varname += string[i];

	struct var *ret = find_var_1(varname.c_str());
	if(ret) return ret;

	struct var new_var;
	new_var.var_name = varname;
	the_vars_1[varname] = new_var;
	return find_var_1(varname.c_str());
}




/* Free up all the variables, and (if SPLIT_REFS) the ref_fm structure
   associated with each variable.  Note that this does not get rid of
   the struct var *s in cell expressions, so it can only be used when all
   the cells are being freed also
   */
	void
flush_variables (void)
{
	the_vars_1.clear();
}
