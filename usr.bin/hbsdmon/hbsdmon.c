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
#include <unistd.h>

#include "hbsdmon.h"

static void main_loop(hbsdmon_ctx_t *);
static bool main_handle_message(hbsdmon_ctx_t *, hbsdmon_node_t *,
    hbsdmon_thread_msg_t *);

int
main(int argc, char *argv[])
{
	hbsdmon_node_t *node, *tnode;
	hbsdmon_ctx_t *ctx;
	int ch, res;

	ctx = new_ctx();
	if (ctx == NULL)
		return (1);

	while ((ch = getopt(argc, argv, "c:")) != -1) {
		switch (ch) {
		case 'c':
			ctx->hc_config = strdup(optarg);
			break;
		}
	}

	if (parse_config(ctx) == false) {
		fprintf(stderr, "[-] Configuration parsing failed. Bailing.\n");
		return (1);
	}

	res = 0;

	if (hbsdmon_thread_init(ctx) == false) {
		res = 1;
	}

	/*
	 * In the current design, the number of threads must equal the
	 * number of nodes. Our threading model is 1:1 between threads
	 * and nodes.
	 */
	assert(ctx->hc_nthreads == ctx->hc_nnodes);

	main_loop(ctx);
	pushover_free_ctx(&(ctx->hc_psh_ctx));

	return (res);
}

static void
main_loop(hbsdmon_ctx_t *ctx)
{
	hbsdmon_thread_t *thread, *tmpthread;
	zmq_pollitem_t *pollitems;
	hbsdmon_thread_msg_t msg;
	hbsdmon_node_t *node;
	int i, nitems;

	pollitems = calloc(ctx->hc_nnodes, sizeof(*pollitems));
	if (pollitems == NULL) {
		return;
	}

	while (true) {
		/*
		 * XXX I really dislike that ZeroMQ went with signed 
		 * integers.
		 */
		memset(pollitems, 0, ctx->hc_nthreads * sizeof(*pollitems));

		nitems = 0;
		SLIST_FOREACH_SAFE(thread, &(ctx->hc_threads), ht_entry,
		    tmpthread) {
			pollitems[nitems].socket = thread->ht_zmqsock;
			pollitems[nitems].events = ZMQ_POLLIN;
			nitems++;
		}

		nitems = zmq_poll(pollitems, nitems, -1);
		if (nitems == -1) {
			break;
		}
		if (nitems == 0) {
			continue;
		}

		for (i = 0; i < ctx->hc_nthreads; i++) {
			if (pollitems[i].revents & ZMQ_POLLIN) {
				node = hbsdmon_find_node_by_zmqsock(
				    ctx, pollitems[i].socket);
				memset(&msg, 0, sizeof(msg));
				nitems = zmq_recv(pollitems[i].socket,
				    &msg, sizeof(msg), 0);
				assert(nitems == sizeof(msg));
				if (main_handle_message(ctx, node,
				    &msg) == false) {
					fprintf(stderr, "Main thread:"
					    " Unable to handle message"
					    " from node %s (method %s)",
					    node->hn_host,
					    hbsdmon_method_to_str(node->hn_method));
					return;
				}
			}
		}
	}
}

static bool
main_handle_message(hbsdmon_ctx_t *ctx, hbsdmon_node_t *node,
    hbsdmon_thread_msg_t *msg)
{

	switch (msg->htm_verb) {
	case VERB_HEARTBEAT:
		printf("Main: Got heartbeat from %s (method %s)\n",
		    node->hn_host,
		    hbsdmon_method_to_str(node->hn_method));
		break;
	default:
		printf("Main: Got unknown message from %s"
		    " (method %s)\n",
		    node->hn_host,
		    hbsdmon_method_to_str(node->hn_method));
	}

	return (true);
}
