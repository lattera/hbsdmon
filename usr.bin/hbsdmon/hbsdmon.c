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

#include <pthread_np.h>

#include <signal.h>
#include <sys/types.h>
#include <sys/sbuf.h>

#include "hbsdmon.h"

#define	APPFLAG_NONE	 0
#define	APPFLAG_TERM	 1
#define	APPFLAG_INFO	 2

static uint64_t appflags = 0;

static void sighandler(int);

static void main_loop(hbsdmon_ctx_t *);
static bool main_handle_message(hbsdmon_ctx_t *, hbsdmon_node_t *,
    hbsdmon_thread_msg_t *);
static void dispatch_signal(hbsdmon_ctx_t *);
static void dispatch_term(hbsdmon_ctx_t *);
static void dispatch_info(hbsdmon_ctx_t *);
static void hbsdmon_heartbeat(hbsdmon_ctx_t *);
static char *hbsdmon_stats_to_str(hbsdmon_ctx_t *);

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

	pthread_mutex_init(&(ctx->hc_mtx), NULL);

	res = 0;

	signal(SIGINT, sighandler);
	signal(SIGINFO, sighandler);
	signal(SIGTERM, sighandler);

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
	bool breakout;
	int i, nitems;

	pollitems = calloc(ctx->hc_nnodes, sizeof(*pollitems));
	if (pollitems == NULL) {
		return;
	}

	breakout = false;
	while (true) {
		hbsdmon_heartbeat(ctx);
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

		nitems = zmq_poll(pollitems, nitems, 1000);

		if (appflags) {
			if ((appflags & APPFLAG_TERM)  ==
			    APPFLAG_TERM) {
				breakout = true;
			}
			dispatch_signal(ctx);
			hbsdmon_lock_ctx(ctx);
			appflags = 0;
			hbsdmon_unlock_ctx(ctx);

			if (breakout) {
				break;
			}
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

static void
hbsdmon_heartbeat(hbsdmon_ctx_t *ctx)
{
	char sndbuf[512], timebuf[32];
	pushover_message_t *pmsg;
	hbsdmon_keyvalue_t *kv;
	time_t lasthb, curtime;
	struct tm localt;

	lasthb = hbsdmon_get_last_heartbeat(ctx);
	curtime = time(NULL);
	if (curtime - lasthb < ctx->hc_heartbeat - 1) {
		return;
	}

	memset(timebuf, 0, sizeof(timebuf));
	memset(&localt, 0, sizeof(localt));
	localtime_r(&curtime, &localt);
	asctime_r(&localt, timebuf);

	memset(sndbuf, 0, sizeof(sndbuf));
	pmsg = pushover_init_message(NULL);
	if (pmsg == NULL) {
		return;
	}
	pushover_message_set_title(pmsg, "MONITOR HEARTBEAT");
	snprintf(sndbuf, sizeof(sndbuf)-1, "Heartbeat at %s\n",
	    timebuf);
	pushover_message_set_msg(pmsg, sndbuf);
	pushover_message_set_dest(pmsg, ctx->hc_dest);
	pushover_submit_message(ctx->hc_psh_ctx, pmsg);
	pushover_free_message(&pmsg);

	hbsdmon_lock_ctx(ctx);
	ctx->hc_stats.hs_nheartbeats++;
	hbsdmon_unlock_ctx(ctx);

	assert(hbsdmon_update_last_heartbeat(ctx) == true);
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
	case VERB_TERM:
		pthread_join(node->hn_thread->ht_tid, NULL);
		break;
	default:
		printf("Main: Got unknown message from %s"
		    " (method %s)\n",
		    node->hn_host,
		    hbsdmon_method_to_str(node->hn_method));
	}

	return (true);
}

static void
sighandler(int signo)
{

	printf("[*] Main thread: Got signal (%d)\n", signo);
	switch (signo) {
	case SIGINT:
	case SIGTERM:
		appflags |= APPFLAG_TERM;
		break;
	case SIGINFO:
		appflags |= APPFLAG_INFO;
		break;
	}
}

static void
dispatch_signal(hbsdmon_ctx_t *ctx)
{

	if ((appflags & APPFLAG_TERM)  == APPFLAG_TERM) {
		dispatch_term(ctx);
	}

	if ((appflags & APPFLAG_INFO) == APPFLAG_INFO) {
		dispatch_info(ctx);
	}
}

static void
dispatch_term(hbsdmon_ctx_t *ctx)
{
	hbsdmon_thread_t *thread, *tthread;
	hbsdmon_thread_msg_t msg;
	zmq_pollitem_t zmqpoll;
	int res;

	printf("[*] Main thread: dispatching term.\n");

	SLIST_FOREACH_SAFE(thread, &(ctx->hc_threads), ht_entry,
	   tthread) {
		memset(&msg, 0, sizeof(msg));
		msg.htm_verb = VERB_TERM;
		res = zmq_send(thread->ht_zmqsock, &msg,
		    sizeof(msg), 0);
		assert(res == sizeof(msg));

		/* Allow the thread one second to clean up. */
		memset(&zmqpoll, 0, sizeof(zmqpoll));
		zmqpoll.socket = thread->ht_zmqsock;
		zmqpoll.events |= ZMQ_POLLIN;
		zmq_poll(&zmqpoll, 1, 1000);

		memset(&msg, 0, sizeof(msg));
		res = zmq_recv(thread->ht_zmqsock, &msg,
		    sizeof(msg), ZMQ_DONTWAIT);
		if (res != sizeof(msg)) {
			fprintf(stderr, "[*] Worker thread failed to"
			    " shutdown gracefully. Terminating.\n");
			pthread_cancel(thread->ht_tid);
		}
		pthread_peekjoin_np(thread->ht_tid, NULL);
		pthread_cancel(thread->ht_tid);
	}
}

static void
dispatch_info(hbsdmon_ctx_t *ctx)
{
	pushover_message_t *pmsg;
	char *stats_str;


	hbsdmon_lock_ctx(ctx);
	stats_str = hbsdmon_stats_to_str(ctx);
	hbsdmon_reset_stats(ctx);
	hbsdmon_unlock_ctx(ctx);

	if (stats_str == NULL) {
		return;
	}

	pmsg = pushover_init_message(NULL);
	if (pmsg == NULL) {
		free(stats_str);
		return;
	}
	pushover_message_set_title(pmsg, "MONITOR STATS");
	pushover_message_set_msg(pmsg, stats_str);
	pushover_message_set_dest(pmsg, ctx->hc_dest);
	pushover_submit_message(ctx->hc_psh_ctx, pmsg);
	pushover_free_message(&pmsg);
	free(stats_str);
}

static char *
hbsdmon_stats_to_str(hbsdmon_ctx_t *ctx)
{
	struct tm localt;
	char timebuf[32];
	time_t heartbeat;
	struct sbuf *sb;
	char *ret;

	sb = sbuf_new_auto();
	if (sb == NULL) {
		return (NULL);
	}

	memset(&localt, 0, sizeof(localt));
	memset(timebuf, 0, sizeof(timebuf));
	heartbeat = hbsdmon_get_last_heartbeat(ctx);
	localtime_r(&heartbeat, &localt);
	asctime_r(&localt, timebuf);

	sbuf_printf(sb, "Last heartbeat: %s\n", timebuf);

	sbuf_printf(sb, "Nodes: %zu\n", ctx->hc_nnodes);

	sbuf_printf(sb,
	    "Heartbeats: %zu\n"
	    "Errors: %zu\n"
	    "Successes: %zu\n"
	    "Poll failures: %zu\n",
	    ctx->hc_stats.hs_nheartbeats,
	    ctx->hc_stats.hs_nerrors,
	    ctx->hc_stats.hs_nsuccess,
	    ctx->hc_stats.hs_npollfails);

	if (sbuf_finish(sb)) {
		sbuf_delete(sb);
		return (NULL);
	}

	ret = strdup(sbuf_data(sb));
	sbuf_delete(sb);

	return (ret);
}
