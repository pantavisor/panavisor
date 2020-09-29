/*
 * Copyright (c) 2020 Pantacor Ltd.
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

#define MODULE_NAME             "state"
#define pv_log(level, msg, ...)         vlog(MODULE_NAME, level, msg, ## __VA_ARGS__)
#include "log.h"

#include "state.h"
#include "volumes.h"
#include "platforms.h"
#include "objects.h"
#include "addons.h"

struct pv_state* pv_state_init(int rev, char *spec)
{
	struct pv_state *s;

	s = calloc(1, sizeof(struct pv_state));
	if (s) {
		s->rev = rev;
		s->spec = strdup(spec);
		dl_list_init(&s->objects);
	}

	return s;
}

void pv_state_remove(struct pv_state *s)
{
	pv_log(DEBUG, "removing state with revision %d", s->rev);

	if (!s)
		return;

	if (s->spec)
		free(s->spec);
	if (s->kernel)
		free(s->kernel);
	if (s->fdt)
		free(s->fdt);
	if (s->firmware)
		free(s->firmware);
	if (s->modules)
		free(s->modules);
	if (s->initrd)
		free(s->initrd);

	pv_platforms_remove(s);
	pv_volumes_remove(s);
	pv_addons_remove(s);
	pv_objects_remove(s);

	if (s->json)
		free(s->json);

	free(s);
}

void pv_state_print(struct pv_state *s)
{
	if (!s)
		return;

	// print
	pv_log(DEBUG, "kernel: '%s'", s->kernel);
	pv_log(DEBUG, "initrd: '%s'", s->initrd);
	pv_log(DEBUG, "fdt: '%s'", s->fdt);
	struct pv_platform *p, *tmp_p;
    struct dl_list *head = &s->platforms;
    dl_list_for_each_safe(p, tmp_p, head,
            struct pv_platform, list) {
		pv_log(DEBUG, "platform: '%s'", p->name);
		pv_log(DEBUG, "  type: '%s'", p->type);
		pv_log(DEBUG, "  exec: '%s'", p->exec);
		pv_log(DEBUG, "  runlevel: '%d'", p->runlevel);
		pv_log(DEBUG, "  configs:");
		char **config = p->configs;
		while (config && *config) {
			pv_log(DEBUG, "    '%s'", *config);
			config++;
		}
    }
	struct pv_volume *v, *tmp_v;
	head = &s->volumes;
    dl_list_for_each_safe(v, tmp_v, head,
            struct pv_volume, list) {
		pv_log(DEBUG, "volume: '%s'", v->name);
		pv_log(DEBUG, "  type: '%d'", v->type);
		if (v->plat)
			pv_log(DEBUG, "  platform: '%s'", v->plat->name);
	}
	struct pv_object *curr;
	pv_objects_iter_begin(s, curr) {
		pv_log(DEBUG, "object: ");
		pv_log(DEBUG, "  name: '%s'", curr->name);
		pv_log(DEBUG, "  id: '%s'", curr->id);
	}
	pv_objects_iter_end;
}

void pv_state_validate(struct pv_state *s)
{
	if (!s)
		return;

	// remove platforms that have no loaded data
	pv_platforms_remove_not_done(s); 
	// set runlevel in all undefined platforms
	pv_platforms_default_runlevel(s);
}
