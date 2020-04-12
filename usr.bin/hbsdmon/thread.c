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
static bool hbsdmon_create_node_thread(hbsdmon_ctx_t *,
    hbsdmon_node_t *);

bool
hbsdmon_thread_init(hbsdmon_ctx_t *ctx) {
	hbsdmon_node_t *node, *tnode;

	SLIST_FOREACH_SAFE(node, &(ctx->hc_nodes), hn_entry, tnode) {
		if (hbsdmon_create_node_thread(ctx, node) == false) {
			return (false);
		}
	}

	return (true);
}

static bool
hbsdmon_create_node_thread(hbsdmon_ctx_t *ctx, hbsdmon_node_t *node)
{
	hbsdmon_thread_t *thread;
	hbsdmon_thread_msg_t msg;
	char sockname[512];

	thread = calloc(1, sizeof(*thread));
	if (thread == NULL) {
		return (false);
	}

	thread->ht_ctx = ctx;
	thread->ht_node = node;

	thread->ht_zmqsock = zmq_socket(ctx->hc_zmq, ZMQ_PAIR);
	if (thread->ht_zmqsock == NULL) {
		free(thread);
		return (false);
	}

	snprintf(sockname, sizeof(sockname)-1, "inproc://thread_%zu",
	    ++(ctx->hc_nthreads));

	thread->ht_sockname = strdup(sockname);
	if (thread->ht_sockname == NULL) {
		zmq_close(thread->ht_zmqsock);
		free(thread);
		return (false);
	}

	if (zmq_bind(thread->ht_zmqsock, sockname)) {
		zmq_close(thread->ht_zmqsock);
		free(thread->ht_sockname);
		free(thread);
		return (false);
	}

	if (pthread_create(&(thread->ht_tid), NULL,
	    hbsdmon_thread_start, thread)) {
		zmq_close(thread->ht_zmqsock);
		free(thread->ht_sockname);
		free(thread);
		return (false);
	}

	/* Wait for the new thread to tell us it's ready */
	zmq_recv(thread->ht_zmqsock, NULL, 0, 0);

	/* Send init message */
	memset(&msg, 0, sizeof(msg));
	msg.htm_verb = VERB_INIT;
	zmq_send(thread->ht_zmqsock, &msg, sizeof(msg), 0);

	SLIST_INSERT_HEAD(&(ctx->hc_threads), thread, ht_entry);

	return (true);
}

static void *
hbsdmon_thread_start(void *argp)
{
	hbsdmon_thread_t *thread;
	hbsdmon_thread_msg_t msg;
	pushover_message_t *pmsg;
	char sendbuf[512];
	void *zmqsock;
	int nrecv;

	assert(argp != NULL);

	thread = argp;

	zmqsock = zmq_socket(thread->ht_ctx->hc_zmq, ZMQ_PAIR);
	if (zmqsock == NULL) {
		return (argp);
	}

	if (zmq_connect(zmqsock, thread->ht_sockname)) {
		goto end;
	}

	thread->ht_zmqtsock = zmqsock;

	/* Tell the main thread we're ready */
	zmq_send(zmqsock, NULL, 0, 0);

	/* Wait for main thread to send us our first message */
	if (zmq_recv(thread->ht_zmqtsock, &msg,
	   sizeof(msg), 0) == -1) {
		/* Unrecoverable error */
		return (false);
	}

	switch (msg.htm_verb) {
	case VERB_INIT:
		hbsdmon_node_thread_init(thread);
		break;
	case VERB_FINI:
		return (argp);
	default:
		return (argp);
	}

	hbsdmon_node_thread_run(thread);

end:
	zmq_close(zmqsock);
	return (argp);
}
