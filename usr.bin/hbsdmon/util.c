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


hbsdmon_method_t
hbsdmon_str_to_method(const char *method)
{
	if (!strcasecmp(method, "ICMP"))
		return (METHOD_ICMP);
	if (!strcasecmp(method, "HTTPS"))
		return (METHOD_HTTPS);
	if (!strcasecmp(method, "HTTP"))
		return (METHOD_HTTP);
	if (!strcasecmp(method, "TCP"))
		return (METHOD_TCP);
	if (!strcasecmp(method, "UDP"))
		return (METHOD_UDP);
	if (!strcasecmp(method, "ZFS"))
		return (METHOD_ZFS);

	/* Default to ICMP */
	return METHOD_ICMP;
}

const char *
hbsdmon_method_to_str(hbsdmon_method_t method)
{

	switch (method) {
	case METHOD_HTTPS:
		return ("HTTPS");
	case METHOD_HTTP:
		return ("HTTP");
	case METHOD_ICMP:
		return ("ICMP");
	case METHOD_TCP:
		return ("TCP");
	case METHOD_UDP:
		return ("UDP");
	case METHOD_ZFS:
		return ("ZFS");
	default:
		return (NULL);
	}
}

long
hbsdmon_get_interval(hbsdmon_node_t *node)
{
	hbsdmon_keyvalue_t *kv;

	kv = hbsdmon_find_kv_in_node(node, "interval", false);
	if (kv != NULL) {
		return ((long)hbsdmon_keyvalue_to_uint64(kv));
	}

	/* XXX So much indirection! */
	kv = hbsdmon_find_kv(node->hn_thread->ht_ctx->hc_kvstore,
	    "interval", false);
	if (kv != NULL) {
		return ((long)hbsdmon_keyvalue_to_uint64(kv));
	}

	/* 5 seconds seems sane */
	return (5);
}

time_t
hbsdmon_get_last_heartbeat(hbsdmon_ctx_t *ctx)
{
	hbsdmon_keyvalue_t *kv;

	kv = hbsdmon_find_kv(ctx->hc_kvstore, "heartbeat", true);
	assert(kv != NULL);

	return (hbsdmon_keyvalue_to_time(kv));
}

bool
hbsdmon_update_last_heartbeat(hbsdmon_ctx_t *ctx)
{
	hbsdmon_keyvalue_t *kv;
	time_t hbtime;

	hbtime = time(NULL);
	kv = hbsdmon_find_kv(ctx->hc_kvstore, "heartbeat", true);
	assert(kv != NULL);
	return (hbsdmon_keyvalue_modify(ctx->hc_kvstore, kv,
	    &hbtime, sizeof(hbtime), true));
}

void
hbsdmon_lock_ctx(hbsdmon_ctx_t *ctx)
{

	pthread_mutex_lock(&(ctx->hc_mtx));
}

void
hbsdmon_unlock_ctx(hbsdmon_ctx_t *ctx)
{

	pthread_mutex_unlock(&(ctx->hc_mtx));
}

void
hbsdmon_thread_lock_ctx(hbsdmon_thread_t *thread)
{

	hbsdmon_lock_ctx(thread->ht_ctx);
}

void
hbsdmon_thread_unlock_ctx(hbsdmon_thread_t *thread)
{

	hbsdmon_unlock_ctx(thread->ht_ctx);
}

void
hbsdmon_node_lock_ctx(hbsdmon_node_t *node)
{

	hbsdmon_thread_lock_ctx(node->hn_thread);
}

void
hbsdmon_node_unlock_ctx(hbsdmon_node_t *node)
{

	hbsdmon_thread_unlock_ctx(node->hn_thread);
}

void
hbsdmon_reset_stats(hbsdmon_ctx_t *ctx)
{

	memset(&(ctx->hc_stats), 0, sizeof(ctx->hc_stats));
}
