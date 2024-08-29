/*
 * Copyright (c) 2024 Pantacor Ltd.
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

#ifndef PHLOGGER_DAEMON_H
#define PHLOGGER_DAEMON_H

#include <sys/types.h>

#define PHLOGGER_SERVICE_FLAG_STOP (1 << 0)

typedef enum {
	PHLOGGER_SERVICE_DAEMON,
	PHLOGGER_SERVICE_ONE_SHOT
} phlogger_service_type_t;

struct phlogger_service {
	const char *name;
	phlogger_service_type_t type;
	pid_t pid;
	int flags;
	char *rev;
	int (*init)(void);
	void (*proc)(void);
};

int phlogger_service_start(struct phlogger_service *srv, const char *rev);
void phlogger_service_stop_lenient(struct phlogger_service *srv);
void phlogger_service_stop_force(struct phlogger_service *srv);

#endif
