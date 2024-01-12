/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Simon Kuenzer <simon.kuenzer@neclab.eu>
 *
 * Copyright (c) 2022, NEC Laboratories Europe GmbH, NEC Corporation.
 *                     All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __PROCESS_H_INTERNAL__
#define __PROCESS_H_INTERNAL__

#include <sys/types.h>
#include <uk/config.h>
#include <uk/semaphore.h>
#if CONFIG_LIBPOSIX_PROCESS_PIDS
#include <uk/thread.h>
#endif /* CONFIG_LIBPOSIX_PROCESS_PIDS */

#define UK_PID_INIT		1

#define UK_PID_WAIT_ANY		-1

#define TIDMAP_SIZE (CONFIG_LIBPOSIX_PROCESS_MAX_PID + 1)

enum posix_thread_state {
	POSIX_THREAD_RUNNING,
	POSIX_THREAD_BLOCKED_VFORK,  /* waiting for child to call execve */
	POSIX_THREAD_BLOCKED_WAIT,   /* waiting for process state change */
	POSIX_THREAD_BLOCKED_SIGNAL, /* waiting for signal */
	POSIX_THREAD_EXITED,         /* terminated normally */
	POSIX_THREAD_KILLED,         /* terminated by signal */
};

struct posix_process {
	pid_t pid;
	struct posix_process *parent;
	struct uk_list_head children; /* child processes */
	struct uk_list_head child_list_entry;
	struct uk_list_head threads;
	struct uk_alloc *_a;
#if CONFIG_LIBPOSIX_PROCESS_SIGNAL
	struct uk_signal_pdesc *signal;
#endif /* CONFIG_LIBPOSIX_PROCESS_SIGNAL */
	struct uk_semaphore wait_semaphore;
	struct uk_semaphore exit_semaphore;
	bool terminated;
	int exit_status;

	/* TODO: Mutex */
};

struct posix_thread {
	pid_t tid;
	struct posix_process *process;
	struct uk_list_head thread_list_entry;
	struct uk_thread *thread;
	struct uk_alloc *_a;
	enum posix_thread_state state;
#if CONFIG_LIBPOSIX_PROCESS_SIGNAL
	struct uk_signal_tdesc *signal;
#endif /* CONFIG_LIBPOSIX_PROCESS_SIGNAL */
	pid_t wait_pid;

	/* TODO: Mutex */
};

extern struct posix_process *pid_process[TIDMAP_SIZE];

#define uk_process_foreach(_p)						\
for (int _j = 1, _i = 0; _i != ARRAY_SIZE(pid_process);			\
	_j = !_j, _i++)							\
		for (_p = pid_process[_i]; _j; _j = !_j)		\
			if (_p)						\

#define uk_process_foreach_child(_pparent, _p)				\
uk_process_foreach(_p)							\
		if ((_p)->parent == (_pparent))				\

#define uk_process_foreach_pthread(_proc, _pthread, _pthreadn)		\
	uk_list_for_each_entry_safe(_pthread, _pthreadn,		\
				    &(_proc)->threads, thread_list_entry)

#define uk_gettid()	ukthread2tid(uk_thread_current())
#define uk_getpid()	ukthread2pid(uk_thread_current())

#if CONFIG_LIBPOSIX_PROCESS_PIDS
struct posix_process *pid2pprocess(pid_t pid);
struct uk_thread *tid2ukthread(pid_t tid);
struct posix_thread *tid2pthread(pid_t tid);
struct posix_process *tid2pprocess(pid_t tid);
pid_t ukthread2tid(struct uk_thread *thread);
pid_t ukthread2pid(struct uk_thread *thread);
#endif /* CONFIG_LIBPOSIX_PROCESS_PIDS */

/**
 * Terminate a process
 *
 * Sends the process and thread status, kills the terminating thread's
 * siblings, reparents children, and sends SIGCHLD to the parent.
 *
 * At this point the process is in zombie state and should be reaped
 * by a syscall of the wait family or uk_posix_process_wait().
 *
 * @param pthread     The thread that terminates the process.
 * @param state	      The new process state. Must be either
 *                    POSIX_THREAD_STATE_EXITED if called by exit(), or
 *                    POSIX_THREAD_KILLED if called by SIG_KILL / SIG_CORE.
 * @param exit_status If called on behalf of the current thread, does not
 *                    return. If called for a thread other than the current
 *                    thread, it returns the exit code if called by exit(),
 *                    or the signal number that caused the termination if
 *                    called via SIG_KILL / SIG_CORE.
 */
void pprocess_exit(struct uk_thread *thread, enum posix_thread_state state,
		   int exit_status);

#endif /* __PROCESS_H_INTERNAL__ */
