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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ucl.h>

#include "hbsdmon.h"

#include <sys/types.h>
#include <sys/sbuf.h>

static bool hbsdmon_node_ping(hbsdmon_ctx_t *, hbsdmon_node_t *);
static void hbsdmon_node_fail(hbsdmon_thread_t *);
static void hbsdmon_node_success(hbsdmon_thread_t *);
static void hbsdmon_node_notify(hbsdmon_node_t *,
    hbsdmon_thread_msg_t *);
static char *hbsdmon_node_port(hbsdmon_node_t *);

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

bool
hbsdmon_node_init(hbsdmon_node_t *node)
{

	switch (node->hn_method) {
	case METHOD_ZFS:
		if (!hbsdmon_zfs_init(node)) {
			return (false);
		}
	default:
		break;
	}

	return (true);
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
		timeout = hbsdmon_get_interval(thread->ht_node) * 1000;

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
			case VERB_TERM:
			case VERB_FINI:
			default:
				return (true);
			}
		}

		if (nevents < 0) {
			hbsdmon_thread_lock_ctx(thread);
			thread->ht_ctx->hc_stats.hs_npollfails++;
			hbsdmon_thread_unlock_ctx(thread);

			if (errno == ETERM) {
				break;
			}

			continue;
		}

		res = hbsdmon_node_ping(thread->ht_ctx,
			thread->ht_node);
		if (res == false) {
			hbsdmon_node_fail(thread);
			continue;
		}

		hbsdmon_thread_lock_ctx(thread);
		thread->ht_ctx->hc_stats.hs_nsuccess++;
		hbsdmon_thread_unlock_ctx(thread);

		/*
		* Ping successful. Clear the last
		* fail time if it exists.
		*/
		kv = hbsdmon_find_kv_in_node( thread->ht_node,
		    "lastfail", true);
		if (kv != NULL) {
			hbsdmon_node_success(thread);
			hbsdmon_free_kv(hbsdmon_node_kv(
			    thread->ht_node), &kv, true);
		}
	}

	return (true);
}

hbsdmon_node_t *
hbsdmon_find_node_by_zmqsock(hbsdmon_ctx_t *ctx, void *sock)
{
	hbsdmon_thread_t *thread, *tmpthread;

	SLIST_FOREACH_SAFE(thread, &(ctx->hc_threads), ht_entry,
	    tmpthread) {
		if (thread->ht_zmqsock == sock) {
			return (thread->ht_node);
		}
	}

	/* XXX should we assert instead? */
	return (NULL);
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

	switch (node->hn_method) {
	case METHOD_HTTP:
		return (hbsdmon_http_ping(node));
	case METHOD_TCP:
		return (hbsdmon_tcp_ping(node));
	case METHOD_UDP:
		return (hbsdmon_udp_ping(node));
	case METHOD_ZFS:
		return (hbsdmon_zfs_status(node));
	default:
		return (true);
	}
}

static void
hbsdmon_node_fail(hbsdmon_thread_t *thread)
{
	time_t lastfail, tlastfail;
	hbsdmon_thread_msg_t tmsg;
	pushover_message_t *pmsg;
	hbsdmon_keyvalue_t *kv;
	struct sbuf *sb;
	char *nodestr;
	long interval;

	/*
	 * This is used for determining the next ping. Value should be
	 * one less than the primary interval to properly determine
	 * next ping time.
	 */
	interval = hbsdmon_get_interval(thread->ht_node) - 1;

	kv = hbsdmon_find_kv_in_node(thread->ht_node,
	    "lastfail", true);

	lastfail = 0;
	if (kv != NULL) {
		lastfail = hbsdmon_keyvalue_to_time(kv);
		/* XXX make this dynamic */
		lastfail = time(NULL) - lastfail;
		if (lastfail > interval && lastfail < 7200) {
			return;
		}
		hbsdmon_free_kv(hbsdmon_node_kv(thread->ht_node),
		    &kv, true);
	}

	lastfail = time(NULL);
	kv = hbsdmon_new_keyvalue();
	if (kv == NULL) {
		return;
	}
	hbsdmon_keyvalue_store(kv, "lastfail",
		&lastfail, sizeof(lastfail));
	hbsdmon_node_append_kv(thread->ht_node, kv);

	sb = sbuf_new_auto();
	if (sb == NULL) {
		return;
	}

	nodestr = hbsdmon_node_to_str(thread->ht_node);
	if (nodestr == NULL) {
		goto end;
	}

	if (sbuf_cat(sb, nodestr)) {
		goto end;
	}

	kv = hbsdmon_find_kv_in_node(thread->ht_node,
	    "failmsg", true);

	if (kv != NULL) {
		sbuf_printf(sb, "\n%s", hbsdmon_keyvalue_to_str(kv));
	}

	pmsg = pushover_init_message(NULL);
	if (pmsg == NULL) {
		goto end;
	}

	if (sbuf_finish(sb)) {
		goto end;
	}

	pushover_message_set_dest(pmsg, thread->ht_ctx->hc_dest);
	pushover_message_set_title(pmsg, "NODE FAILURE");
	pushover_message_set_msg(pmsg, sbuf_data(sb));
	pushover_submit_message(thread->ht_ctx->hc_psh_ctx, pmsg);

end:

	hbsdmon_thread_lock_ctx(thread);
	thread->ht_ctx->hc_stats.hs_nerrors++;
	hbsdmon_thread_unlock_ctx(thread);

	sbuf_delete(sb);
	free(nodestr);
	if (pmsg != NULL) {
		pushover_free_message(&pmsg);
	}
}

static void
hbsdmon_node_success(hbsdmon_thread_t *thread)
{
	pushover_message_t *pmsg;
	char *nodestr;

	pmsg = pushover_init_message(NULL);
	if (pmsg == NULL) {
		return;
	}

	nodestr = hbsdmon_node_to_str(thread->ht_node);
	if (nodestr == NULL) {
		pushover_free_message(&pmsg);
		return;
	}

	pushover_message_set_dest(pmsg, thread->ht_ctx->hc_dest);
	pushover_message_set_title(pmsg, "NODE ONLINE");
	pushover_message_set_msg(pmsg, nodestr);
	pushover_submit_message(thread->ht_ctx->hc_psh_ctx, pmsg);

	pushover_free_message(&pmsg);
	free(nodestr);
}

bool
hbsdmon_node_thread_init(hbsdmon_thread_t *thread)
{
	pushover_message_t *pmsg;
	char *nodestr;

	if (!hbsdmon_node_init(thread->ht_node)) {
		return (false);
	}

	pmsg = pushover_init_message(NULL);
	if (pmsg == NULL) {
		return (false);
	}

	nodestr = hbsdmon_node_to_str(thread->ht_node);
	if (nodestr == NULL) {
		pushover_free_message(&pmsg);
		return (false);
	}

	/* XXX check for errors */
	pushover_message_set_dest(pmsg, thread->ht_ctx->hc_dest);
	pushover_message_set_title(pmsg, "MONITOR INIT");
	pushover_message_set_msg(pmsg, nodestr);
	pushover_submit_message( thread->ht_ctx->hc_psh_ctx, pmsg);

	pushover_free_message(&pmsg);
	free(nodestr);

	return (true);
}

void
hbsdmon_node_cleanup(hbsdmon_node_t *node)
{

	hbsdmon_free_kvstore(&(node->hn_kvstore));
	free(node->hn_host);
	node->hn_host = NULL;
}

char *
hbsdmon_node_to_str(hbsdmon_node_t *node)
{
	hbsdmon_keyvalue_t *kv;
	char *port, *ret;
	struct sbuf *sb;
	int addrfam;

	sb = sbuf_new_auto();
	if (sb == NULL) {
		return (NULL);
	}

	port = hbsdmon_node_port(node);
	assert(port != NULL);

	if (sbuf_printf(sb,
	    "Monitor name:	%s\n"
	    "Host:		%s\n"
	    "Method:		%s\n"
	    "Port:		%s\n",
	    node->hn_thread->ht_ctx->hc_name,
	    node->hn_host,
	    hbsdmon_method_to_str(node->hn_method),
	    port)) {
		free(port);
		return (NULL);
	}

	free(port);

	switch (node->hn_method) {
	case METHOD_TCP:
	case METHOD_UDP:
		kv = hbsdmon_find_kv(hbsdmon_node_kv(node),
		    "addrfam", false);
		if (kv == NULL) {
			break;
		}
		addrfam = (int)hbsdmon_keyvalue_to_uint64(kv);
		if (sbuf_printf(sb, "Address family: %s\n",
		    (addrfam == PF_INET) ? "IPv4" : "IPv6")) {
			sbuf_delete(sb);
			return (NULL);
		}
		break;
	case METHOD_ZFS:
		kv = hbsdmon_find_kv(hbsdmon_node_kv(node),
		    "pool", false);
		if (kv == NULL) {
			break;
		}
		if (sbuf_printf(sb, "Pool: %s\n",
		    hbsdmon_keyvalue_to_str(kv))) {
			sbuf_delete(sb);
			return (NULL);
		}
		break;
	default:
		break;
	}

	if (sbuf_finish(sb)) {
		sbuf_delete(sb);
		return (NULL);
	}

	ret = strdup(sbuf_data(sb));

	sbuf_delete(sb);

	return (ret);
}

static char *
hbsdmon_node_port(hbsdmon_node_t *node)
{
	hbsdmon_keyvalue_t *kv;
	char *ret;
	int port;

	switch (node->hn_method) {
	case METHOD_UDP:
	case METHOD_TCP:
		ret = NULL;
		kv = hbsdmon_find_kv(hbsdmon_node_kv(node),
		    "port", true);
		assert(kv != NULL);
		port = hbsdmon_keyvalue_to_int(kv);
		asprintf(&ret, "%d", port);
		return (ret);
	case METHOD_HTTP:
		return (strdup("80"));
	default:
		return (strdup("N/A"));
	}
}
