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

hbsdmon_node_t *
hbsdmon_new_node(void)
{
	hbsdmon_node_t *res;

	res = calloc(1, sizeof(*res));
	if (res == NULL) {
		return (NULL);
	}

	res->hn_kvstore = calloc(1, sizeof(*(res->hn_kvstore)));
	if (res->hn_kvstore == NULL) {
		free(res);
		return (NULL);
	}

	SLIST_INIT(&(res->hn_kvstore->hks_store));

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
