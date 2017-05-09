/*
 * Copyright (c) 2017 Pantacor Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MODULE_NAME			"objects"
#define sc_log(level, msg, ...)		vlog(MODULE_NAME, level, msg, ## __VA_ARGS__)
#include "log.h"

#include "utils.h"
#include "objects.h"

struct sc_object* sc_objects_add(struct sc_state *s, char *filename, char *id, char *c)
{
	struct sc_object *this = calloc(1, sizeof(struct sc_object));
	struct sc_object *add = s->objects;
	int size;

	while (add && add->next) {
		add = add->next;
	}

	if (!add) {
		s->objects = add = this;
	} else {
		add->next = this;
	}

	this->name = strdup(filename);
	this->id = strdup(id);

	size = sizeof(RELPATH_FMT) + strlen(c) +
		strlen(filename) + get_digit_count(s->rev);

	this->relpath = calloc(1, size * sizeof(char));
	sprintf(this->relpath , RELPATH_FMT, c, s->rev, filename);

	size = sizeof(OBJPATH_FMT) + strlen(c) + strlen(id);

	this->objpath = calloc(1, size * sizeof(char));
	sprintf(this->objpath, OBJPATH_FMT, c, id);

	return this;
}

struct sc_object* sc_objects_get_by_name(struct sc_state *s, char *name)
{
	struct sc_object *o = s->objects;

	while (o) {
		if (!strcmp(o->name, name))
			return o;
		o = o->next;
	}

	return NULL;
}

void sc_objects_remove_all(struct sc_state *s)
{
	struct sc_object *o = s->objects;
	struct sc_object *t;

	while (o) {
		if (o->name)
			free(o->name);
		if (o->id)
			free(o->id);
		t = o->next;
		free(o);
		o = t;
	}
}
