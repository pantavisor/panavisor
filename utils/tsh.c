/*
 * Copyright (c) 2017-2021 Pantacor Ltd.
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
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <signal.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "tsh.h"
#include "timer.h"

#define TSH_MAX_LENGTH 32
#define TSH_DELIM " \t\r\n\a"

static char **_tsh_split_cmd(char *cmd)
{
	int pos = 0;
	char **ts = malloc(TSH_MAX_LENGTH * sizeof(char *));
	char *t;

	if (!ts)
		return NULL;

	t = strtok(cmd, TSH_DELIM);
	while (t != NULL) {
		ts[pos] = t;
		pos++;

		if (pos >= TSH_MAX_LENGTH)
			break;

		t = strtok(NULL, TSH_DELIM);
	}
	ts[pos] = NULL;

	return ts;
}

static pid_t _tsh_exec(char **argv, int wait, int *status, int stdin_p[],
		       int stdout_p[], int stderr_p[])
{
	int pid = -1;
	sigset_t blocked_sig, old_sigset;
	int ret = 0;

	if (wait) {
		sigemptyset(&blocked_sig);
		sigaddset(&blocked_sig, SIGCHLD);
		/*
		 * Block SIGCHLD while we want to wait on this child.
		 * */
		ret = sigprocmask(SIG_BLOCK, &blocked_sig, &old_sigset);
	}
	pid = fork();

	if (pid == -1) {
		if ((ret == 0) && wait)
			sigprocmask(SIG_SETMASK, &old_sigset, NULL);
		return -1;
	} else if (pid > 0) {
		// In parent
		if (wait) {
			if (ret == 0) {
				/*wait only if we blocked SIGCHLD*/
				waitpid(pid, status, 0);
				sigprocmask(SIG_SETMASK, &old_sigset, NULL);
			}
		}
		free(argv);
	} else {
		ret = 0;
		// closed all unused fds right away ..
		if (stdin_p) // close writing end for stdin dup
			close(stdin_p[1]);
		if (stdout_p) // close reading ends for out and err dup
			close(stdout_p[0]);
		if (stderr_p)
			close(stderr_p[0]);

		// dup2 things
		while (stdin_p &&
		       ((ret = dup2(stdin_p[0], STDIN_FILENO)) == -1) &&
		       (errno == EINTR)) {
		}
		if (ret == -1)
			goto exit_failure;
		while (stdout_p &&
		       ((ret = dup2(stdout_p[1], STDOUT_FILENO)) == -1) &&
		       (errno == EINTR)) {
		}
		if (ret == -1)
			goto exit_failure;
		while (stderr_p &&
		       ((ret = dup2(stderr_p[1], STDERR_FILENO)) == -1) &&
		       (errno == EINTR)) {
		}
		if (ret == -1)
			goto exit_failure;

		// close all the duped ones now too
		if (stdin_p) // close reading end for stdin dup
			close(stdin_p[0]);
		if (stdout_p) // close writing ends for out and err dup
			close(stdout_p[1]);
		if (stderr_p)
			close(stderr_p[1]);

		// now we let it flow ...
		setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin", 1);
		execvp(argv[0], argv);
	exit_failure:
		exit(EXIT_FAILURE);
	}

	return pid;
}

// Run command, either built-in or exec
pid_t tsh_run(char *cmd, int wait, int *status)
{
	return tsh_run_io(cmd, wait, status, NULL, NULL, NULL);
}

// Run command, either built-in or exec
pid_t tsh_run_io(char *cmd, int wait, int *status, int stdin_p[],
		 int stdout_p[], int stderr_p[])
{
	pid_t pid;
	char **args;
	char *vcmd;

	vcmd = malloc(strlen(cmd) + 1);
	if (!vcmd)
		return -1;

	strcpy(vcmd, cmd);

	args = _tsh_split_cmd(vcmd);
	if (!args)
		return -1;

	pid = _tsh_exec(args, wait, status, stdin_p, stdout_p, stderr_p);
	free(vcmd);

	if (pid < 0)
		printf("Cannot run \"%s\"\n", cmd);

	return pid;
}

static int safe_fd_set(int fd, fd_set *fds, int *max_fd)
{
	FD_SET(fd, fds);
	if (fd > *max_fd) {
		*max_fd = fd;
	}
	return 0;
}

int tsh_run_output(const char *cmd, int timeout_s, char *out_buf, int out_size,
		   char *err_buf, int err_size)
{
	int ret = -1, max_fd = -1, res, out_i = 0, err_i = 0;
	pid_t pid = -1;
	char **args;
	char *vcmd = NULL;
	fd_set master;
	int outfd[2], errfd[2];
	struct timespec ts;
	struct timeval tv;
	sighandler_t oldsig;
	sigset_t mask;
	sigset_t orig_mask;

	memset(outfd, -1, sizeof(outfd));
	memset(errfd, -1, sizeof(errfd));

	vcmd = malloc(strlen(cmd) + 1);
	if (!vcmd)
		goto out;

	strcpy(vcmd, cmd);

	args = _tsh_split_cmd(vcmd);
	if (!args)
		goto out;

	// pipes for communication between main process and command process
	if (pipe(outfd) < 0)
		goto out;
	if (pipe(errfd) < 0)
		goto out;

	// set SIGCHLD mask for timeout on waitpid()
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &mask, &orig_mask);

	pid = fork();
	if (pid < 0)
		goto out;
	else if (pid == 0) {
		// redirect out and err of command to pipe
		dup2(outfd[1], STDOUT_FILENO);
		dup2(errfd[1], STDERR_FILENO);
		close(outfd[0]);
		close(errfd[0]);
		// uncomment below to try how child that ignores SIGTERM
		// also gets reaped
		// signal(SIGTERM, SIG_IGN);
		execvp(args[0], args);
		goto out;
	} else {
		int killed = 0;
		close(outfd[1]);
		close(errfd[1]);
		ts.tv_sec = timeout_s;
		ts.tv_nsec = 0;
		tv.tv_sec = timeout_s;
		tv.tv_usec = 0;

		oldsig = signal(SIGCHLD, SIG_DFL);
		while (errfd[0] != -1 || outfd[0] != -1) {
			int ret;
			max_fd = -1;
			FD_ZERO(&master);
			if (outfd[0] != -1)
				safe_fd_set(outfd[0], &master, &max_fd);
			if (errfd[0] != -1)
				safe_fd_set(errfd[0], &master, &max_fd);

			if ((ret = pselect(max_fd + 1, &master, NULL, NULL, &ts,
					   &orig_mask)) < 0) {
				ret = -1;
				printf("signal exit\n");
				break;
			}
			if (!ret) {
				// if we timed out, we send a nice SIGTERM
				// and break ....
				if (!killed) {
					kill(pid, SIGTERM);
					killed = 1;
					printf("killing TERM\n");
					continue;
				} else if (killed == 1) {
					kill(pid, SIGKILL);
					killed++;
					printf("killing KILL\n");
					continue;
				}
				break;
			}
			if (FD_ISSET(outfd[0], &master)) {
				res = read(outfd[0], &out_buf[out_i], out_size);
				if (res > 0) {
					out_size -= res;
					out_i += res;
				} else if (res < 0 && errno != EAGAIN) {
					ret = -1;
					break;
				}
				if (res == 0) {
					close(outfd[0]);
					outfd[0] = -1;
				}
			}

			if (FD_ISSET(errfd[0], &master)) {
				res = read(errfd[0], &err_buf[err_i], err_size);
				if (res > 0) {
					err_size -= res;
					err_i += res;
				} else if (res < 0 && errno != EAGAIN) {
					ret = -1;
					break;
				} else if (res == 0) {
					close(errfd[0]);
					errfd[0] = -1;
				}
			}
		}
		signal(SIGCHLD, oldsig);
	}

out:
	if (pid == 0) {
		close(outfd[1]);
		close(errfd[1]);
		exit(127);
	} else {
		int rv;
		if ((rv = waitpid(pid, &ret, 0)) > 0) {
			if (WIFEXITED(ret)) {
				ret = WEXITSTATUS(ret);
			} else if (WIFSIGNALED(ret)) {
				ret = WTERMSIG(ret);
				errno = EINTR;
			} else {
				printf("unexpected waitpid code path... treating it as exited ... %d\n",
				       ret);
			}
		} else if (!rv) {
			printf("WARNING: pid was not yet reapable: pid=%d",
			       pid);
			kill(pid, SIGKILL);
			errno = EINTR;
		} else {
			printf("ERROR: pid was not yet reapable: rv=%d,pid=%d error=%s",
			       rv, pid, strerror(errno));
		}

		sigprocmask(SIG_SETMASK, &orig_mask, NULL);
	}

	if (vcmd)
		free(vcmd);

	return ret;
}
