/*-
 * Copyright (c) 2020 HardenedBSD Foundation Corp.
 * Author: Shawn Webb <shawn.webb@hardenedbsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ucl.h>

#include "hbsdmon.h"

static void *hbsdmon_thread_start(void *);

hbsdmon_thread_t *
hbsdmon_thread_init(hbsdmon_ctx_t *ctx) {
	hbsdmon_thread_t *thread;
	char sockname[512];

	thread = calloc(1, sizeof(*thread));
	if (thread == NULL) {
		return (NULL);
	}

	thread->ht_ctx = ctx;

	thread->ht_zmqsock = zmq_socket(ctx->hc_zmq, ZMQ_PAIR);
	if (thread->ht_zmqsock == NULL) {
		free(thread);
		return (NULL);
	}

	snprintf(sockname, sizeof(sockname)-1, "inproc://thread_%zu",
	    ++(ctx->hc_nthreads));

	thread->ht_sockname = strdup(sockname);
	if (thread->ht_sockname == NULL) {
		zmq_close(thread->ht_zmqsock);
		free(thread);
		return (NULL);
	}

	if (zmq_bind(thread->ht_zmqsock, sockname)) {
		zmq_close(thread->ht_zmqsock);
		free(thread->ht_sockname);
		free(thread);
		return (NULL);
	}

	if (pthread_create(&(thread->ht_tid), NULL,
	    hbsdmon_thread_start, thread)) {
		zmq_close(thread->ht_zmqsock);
		free(thread->ht_sockname);
		free(thread);
		return (NULL);
	}

	SLIST_INSERT_HEAD(&(ctx->hc_threads), thread, ht_entry);

	return (thread);
}

static void *
hbsdmon_thread_start(void *argp)
{
	hbsdmon_thread_t *thread;

	assert(argp != NULL);

	thread = argp;

	if (zmq_connect(thread->ht_zmqsock, thread->ht_sockname)) {
		return (argp);
	}

	return (argp);
}
