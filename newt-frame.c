/*
 * Copyright © 2018 Keith Packard <keithp@keithp.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "newt.h"

typedef struct newt_frame {
	newt_offset_t	prev;
	newt_offset_t	variables;
} newt_frame_t;

newt_frame_t	*newt_globals;
newt_frame_t	*newt_locals;

static newt_variable_t *
newt_variable_lookup(newt_frame_t *frame, newt_offset_t name, bool insert)
{
	newt_variable_t	*v;

	for (v = newt_pool_ref(frame->variables); v; v = newt_pool_ref(v->next)) {
		if (v->name == name)
			return v;
	}
	if (insert) {
		newt_name_stash(name);
		v = newt_alloc(sizeof (newt_variable_t));
		v->name = newt_name_fetch();
		v->next = frame->variables;
		frame->variables = newt_pool_offset(v);
	}
}

static newt_frame_t *
newt_frame_globals(newt_offset_t *name)
{
	if (!newt_globals) {
		newt_name_stash(*name);
		newt_globals = newt_alloc(sizeof (newt_frame_t));
		*name = newt_name_fetch();
	}
	return newt_globals;
}

newt_variable_t *
newt_frame_lookup(newt_offset_t name, bool insert)
{
	newt_variable_t	*v;

	if (newt_locals && (v = newt_variable_lookup(newt_locals, name, false)))
		return v;
	if (newt_globals && (v = newt_variable_lookup(newt_globals, name, false)))
		return v;
	if (!insert)
		return NULL;
	if (newt_locals)
		v = newt_variable_lookup(newt_locals, name, true);
	else {
		newt_frame_t *g = newt_frame_globals(&name);
		v = newt_variable_lookup(g, name, true);
	}
	return v;
}

bool
newt_frame_mark_global(newt_offset_t name)
{
	if (newt_locals) {
		newt_variable_t *v;

		v = newt_variable_lookup(newt_locals, name, true);
		if (v)
			v->value = NEWT_GLOBAL;
	}
}

bool
newt_frame_push(void)
{
	newt_frame_t *f;

	f = newt_alloc(sizeof (newt_frame_t));
	if (!f)
		return false;
	f->prev = newt_pool_offset(newt_locals);
	newt_locals = f;
}

static int
newt_variable_size(void *addr)
{
	return sizeof (newt_variable_t);
}

static void
newt_variable_mark(void *addr)
{
	newt_variable_t *v = addr;

	for (;;) {
		newt_variable_t *next = newt_pool_ref(v->next);

		newt_poly_mark(v->value, 1);
		if (!next)
			break;
		newt_mark_memory(&newt_variable_mem, next);
		v = next;
	}
}

static void
newt_variable_move(void *addr)
{
	newt_variable_t *v = addr;

	for (;;) {
		int ret;

		(void) newt_poly_move(&v->value, true);
		(void) newt_move_offset(&newt_name_mem, &v->name);

		newt_variable_t *next = newt_pool_ref(v->next);

		ret = newt_move_memory(&newt_variable_mem, (void **) &next);
		if (next != newt_pool_ref(v->next))
			v->next = newt_pool_offset(next);
		if (ret)
			break;

		v = next;
	}
}

const newt_mem_t newt_variable_mem = {
	.size = newt_variable_size,
	.mark = newt_variable_mark,
	.move = newt_variable_move,
	.name = "variable"
};

static int
newt_frame_size(void *addr)
{
	(void) addr;
	return sizeof (newt_frame_t);
}

static void
newt_frame_mark(void *addr)
{
	newt_frame_t *f = addr;

	for (;;) {
		newt_mark(&newt_variable_mem, newt_pool_ref(f->variables));

		newt_frame_t *prev = newt_pool_ref(f->prev);
		if (!prev)
			break;
		newt_mark_memory(&newt_frame_mem, prev);
		f = prev;
	}
}

static void
newt_frame_move(void *addr)
{
	newt_frame_t *f = addr;

	for (;;) {
		newt_variable_t *v = newt_pool_ref(f->variables);
		newt_move(&newt_variable_mem, (void **) &v);
		if (v != net_pool_ref(f->variables))
			f->variables = newt_pool_offset(v);
		newt_frame_t *prev = newt_pool_ref(f->prev);
		if (!prev)
			break;
		int ret = newt_move_memory(&newt_frame_mem, &prev);
		if (prev != newt_pool_ref(f->prev))
			f->prev = newt_pool_offset(prev);
		if (ret)
			break;
		f = prev;
	}
}

const newt_mem_t newt_frame_mem = {
	.size = newt_frame_size,
	.mark = newt_frame_mark,
	.move = newt_frame_move,
	.name = "frame"
};
