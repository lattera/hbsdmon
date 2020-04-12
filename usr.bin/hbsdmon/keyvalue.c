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

hbsdmon_keyvalue_store_t *
hbsdmon_new_kv_store(void)
{
	hbsdmon_keyvalue_store_t *res;
	int err;

	res = calloc(1, sizeof(*res));
	if (res == NULL) {
		return (NULL);
	}

	SLIST_INIT(&(res->hks_store));
	err = pthread_mutex_init(&(res->hks_mtx), NULL);
	if (err) {
		free(res);
		return (NULL);
	}

	return (res);
}

hbsdmon_keyvalue_t *
hbsdmon_new_keyvalue(void) {
	hbsdmon_keyvalue_t *res;

	res = calloc(1, sizeof(*res));

	return (res);
}

int
hbsdmon_lock_kvstore(hbsdmon_keyvalue_store_t *store)
{

	return (pthread_mutex_lock(&(store->hks_mtx)));
}

int
hbsdmon_unlock_kvstore(hbsdmon_keyvalue_store_t *store)
{

	return (pthread_mutex_unlock(&(store->hks_mtx)));
}

bool
hbsdmon_keyvalue_store(hbsdmon_keyvalue_t *kv, const char *key,
    void *value, size_t len)
{

	assert(kv != NULL);
	assert(key != NULL);

	kv->hk_key = strdup(key);
	if (kv->hk_key == NULL) {
		return (false);
	}

	if (len > 0) {
		kv->hk_value = malloc(len);
		if (kv->hk_value == NULL) {
			free(kv->hk_key);
			kv->hk_key = NULL;
			return (false);
		}

		memmove(kv->hk_value, value, len);
		kv->hk_value_len = len;
	} else {
		kv->hk_value = NULL;
		kv->hk_value_len = 0;
	}

	return (true);
}

bool
hbsdmon_keyvalue_modify(hbsdmon_keyvalue_store_t *store,
    hbsdmon_keyvalue_t *kv, void *value, size_t len, bool lock)
{
	void *p;
	bool res;

	res = true;

	if (lock) {
		hbsdmon_lock_kvstore(store);
	}

	if (len == kv->hk_value_len) {
		memmove(kv->hk_value, value, len);
	} else {
		p = realloc(kv->hk_value, len);
		if (p == NULL) {
			res = false;
			goto end;
		}

		memmove(p, value, len);
		kv->hk_value = p;
	}

end:
	if (lock) {
		hbsdmon_unlock_kvstore(store);
	}

	return (res);
}

uint64_t
hbsdmon_keyvalue_to_uint64(hbsdmon_keyvalue_t *kv)
{
	uint64_t res;

	assert(kv != NULL);
	assert(kv->hk_value_len == sizeof(uint64_t));

	res = *((uint64_t *)(kv->hk_value));

	return (res);
}

int
hbsdmon_keyvalue_to_int(hbsdmon_keyvalue_t *kv)
{
	int res;

	assert(kv != NULL);
	assert(kv->hk_value_len == sizeof(int));

	res = *((int *)(kv->hk_value));

	return (res);
}

char *
hbsdmon_keyvalue_to_str(hbsdmon_keyvalue_t *kv)
{

	return (char *)(kv->hk_value);
}

time_t
hbsdmon_keyvalue_to_time(hbsdmon_keyvalue_t *kv)
{
	time_t res;

	assert(kv != NULL);
	assert(kv->hk_value != NULL);
	assert(kv->hk_value_len == sizeof(res));

	res = *((time_t *)(kv->hk_value));

	return (res);
}

void
hbsdmon_append_kv(hbsdmon_keyvalue_store_t *store,
    hbsdmon_keyvalue_t *kv)
{

	hbsdmon_lock_kvstore(store);
	SLIST_INSERT_HEAD(&(store->hks_store), kv, hk_entry);
	hbsdmon_unlock_kvstore(store);
}

hbsdmon_keyvalue_t *
hbsdmon_find_kv(hbsdmon_keyvalue_store_t *store, const char *key,
    bool icase)
{
	hbsdmon_keyvalue_t *kv, *tkv;
	bool res;

	hbsdmon_lock_kvstore(store);
	SLIST_FOREACH_SAFE(kv, &(store->hks_store), hk_entry, tkv) {
		res = icase ? strcasecmp(kv->hk_key, key) :
		    strcmp(kv->hk_key, key);
		if (!res) {
			hbsdmon_unlock_kvstore(store);
			return (kv);
		}
	}

	hbsdmon_unlock_kvstore(store);
	return (NULL);
}

void
hbsdmon_free_kv(hbsdmon_keyvalue_store_t *store,
    hbsdmon_keyvalue_t **kvp, bool instore)
{
	hbsdmon_keyvalue_t *kv;

	assert(kvp != NULL && *kvp != NULL);

	kv = *kvp;

	if (instore) {
		assert(store != NULL);
		hbsdmon_lock_kvstore(store);
		SLIST_REMOVE(&(store->hks_store), kv,
		    _hbsdmon_keyvalue, hk_entry);
		hbsdmon_unlock_kvstore(store);
	}

	free(kv->hk_key);
	free(kv->hk_value);
	free(kv);
	*kvp = NULL;
}

void
hbsdmon_free_kvstore(hbsdmon_keyvalue_store_t **storep)
{
	hbsdmon_keyvalue_store_t *store;
	hbsdmon_keyvalue_t *kv, *tkv;

	assert(storep != NULL && *storep != NULL);

	store = *storep;

	SLIST_FOREACH_SAFE(kv, &(store->hks_store), hk_entry, tkv) {
		hbsdmon_free_kv(store, &kv, true);
	}

	pthread_mutex_destroy(&(store->hks_mtx));

	free(store);
	*storep = NULL;
}
