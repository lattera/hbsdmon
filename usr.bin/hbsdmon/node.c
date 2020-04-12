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

static bool hbsdmon_node_ping(hbsdmon_ctx_t *, hbsdmon_node_t *);
static void hbsdmon_node_fail(hbsdmon_thread_t *);

hbsdmon_node_t *
hbsdmon_new_node(void)
{
	hbsdmon_node_t *res;

	res = calloc(1, sizeof(*res));
	if (res == NULL) {
		return (NULL);
	}

	res->hn_kvstore = hbsdmon_new_kv_store();
	if (res->hn_kvstore == NULL) {
		free(res);
		return (NULL);
	}

	return (res);
}

hbsdmon_keyvalue_store_t *
hbsdmon_node_kv(hbsdmon_node_t *node)
{

	return (node->hn_kvstore);
}

void
hbsdmon_node_append_kv(hbsdmon_node_t *node, hbsdmon_keyvalue_t *kv)
{
	hbsdmon_keyvalue_store_t *store;

	store = hbsdmon_node_kv(node);
	hbsdmon_append_kv(store, kv);
}

hbsdmon_keyvalue_t *
hbsdmon_find_kv_in_node(hbsdmon_node_t *node, const char *key,
    bool icase)
{

	return (hbsdmon_find_kv(hbsdmon_node_kv(node), key, icase));
}

bool
hbsdmon_node_thread_run(hbsdmon_thread_t *thread)
{
	hbsdmon_thread_msg_t tmsg;
	pushover_message_t *pmsg;
	zmq_pollitem_t pollitem;
	hbsdmon_keyvalue_t *kv;
	char sndbuf[512];
	long timeout;
	int nevents, res;

	while (true) {
		timeout = 5000;
		kv = hbsdmon_find_kv_in_node(thread->ht_node,
		    "interval", false);
		if (kv != NULL) {
			timeout = (long)hbsdmon_keyvalue_to_uint64(kv);
		}

		memset(&pollitem, 0, sizeof(pollitem));
		pollitem.socket = thread->ht_zmqtsock;
		pollitem.events = ZMQ_POLLIN;

		nevents = zmq_poll(&pollitem, 1, timeout);
		if (nevents > 0) {
			if (zmq_recv(thread->ht_zmqtsock, &tmsg,
			    sizeof(tmsg), 0) == -1) {
				/* Unrecoverable error */
				return (false);
			}

			switch (tmsg.htm_verb) {
			case VERB_INIT:
				break;
			case VERB_FINI:
				return (true);
			default:
				return (true);
			}
		}

		if (nevents == 0) {
			/*
			 * XXX properly calculate ping time based on
			 * last ping.
			 */
			res = hbsdmon_node_ping(thread->ht_ctx,
			    thread->ht_node);
			if (res == true) {
				/* Ping successful, continue on */
				continue;
			}

			hbsdmon_node_fail(thread);
			continue;
		}
	}

	return (true);
}

void
hbsdmon_node_debug_print(hbsdmon_node_t *node)
{
	hbsdmon_keyvalue_store_t *store;
	hbsdmon_keyvalue_t *kv, *tkv;

	store = hbsdmon_node_kv(node);

	printf("Node: %s\n", node->hn_host);

	SLIST_FOREACH_SAFE(kv, &(store->hks_store), hk_entry, tkv) {
		printf("    Key: %s\n", kv->hk_key);
		printf("    &Value: %p\n", kv->hk_value);
	}
}

static bool
hbsdmon_node_ping(hbsdmon_ctx_t *ctx, hbsdmon_node_t *node)
{

	return (false);
}

static void
hbsdmon_node_fail(hbsdmon_thread_t *thread)
{
	pushover_message_t *pmsg;
	char sndbuf[512];

	memset(sndbuf, 0, sizeof(sndbuf));
	pmsg = pushover_init_message(NULL);
	if (pmsg == NULL) {
		return;
	}

	pushover_message_set_user(pmsg, thread->ht_ctx->hc_dest);
	snprintf(sndbuf, sizeof(sndbuf)-1, "Node %s unresponsive",
		thread->ht_node->hn_host);
	pushover_message_set_title(pmsg, sndbuf);
	snprintf(sndbuf, sizeof(sndbuf)-1,
		"%s stopped responding to pings",
		thread->ht_node->hn_host);
	pushover_message_set_msg(pmsg, sndbuf);
	pushover_submit_message( thread->ht_ctx->hc_psh_ctx, pmsg);
	pushover_free_message(&pmsg);
}

void
hbsdmon_node_thread_init(hbsdmon_thread_t *thread)
{
	pushover_message_t *pmsg;
	char sndbuf[512];

	memset(sndbuf, 0, sizeof(sndbuf));
	pmsg = pushover_init_message(NULL);
	if (pmsg == NULL) {
		return;
	}

	pushover_message_set_user(pmsg, thread->ht_ctx->hc_dest);
	snprintf(sndbuf, sizeof(sndbuf)-1, "MONITOR INIT");
	pushover_message_set_title(pmsg, sndbuf);
	snprintf(sndbuf, sizeof(sndbuf)-1,
		"Initializing monitor for %s",
		thread->ht_node->hn_host);
	pushover_message_set_msg(pmsg, sndbuf);
	pushover_submit_message( thread->ht_ctx->hc_psh_ctx, pmsg);
	pushover_free_message(&pmsg);
}
