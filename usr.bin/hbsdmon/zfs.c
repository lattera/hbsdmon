/*-
 * Copyright (c) 2021 HardenedBSD Foundation Corp.
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

#include <sys/param.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <sys/queue.h>
#include <sys/stat.h>

#include <stdint.h>
#include <sys/types.h>

#include <libzfs.h>
#include <libzfs_core.h>

#include <ucl.h>

#include "hbsdmon.h"

bool
hbsdmon_zfs_init(hbsdmon_node_t *node)
{
	libzfs_handle_t *zfshandle;
	zpool_handle_t *poolhandle;
	hbsdmon_keyvalue_t *kv;
	char *pool;

	kv = hbsdmon_find_kv_in_node(node, "pool", false);
	if (kv == NULL) {
		return (true);
	}

	zfshandle = libzfs_init();
	if (zfshandle == NULL) {
		fprintf(stderr, "[-] libzfs_init failed.\n");
		return (false);
	}

	pool = hbsdmon_keyvalue_to_str(kv);
	poolhandle = zpool_open(zfshandle, pool);
	if (poolhandle == NULL) {
		fprintf(stderr, "[-] zpool_open(%s) failed.\n", pool);
		return (false);
	}

	kv = hbsdmon_new_keyvalue();
	if (kv == NULL) {
		return (false);
	}

	if (!hbsdmon_keyvalue_store(kv, "zfshandle", &zfshandle,
	    sizeof(zfshandle))) {
		return (false);
	}
	hbsdmon_node_append_kv(node, kv);

	kv = hbsdmon_new_keyvalue();
	if (kv == NULL) {
		return (false);
	}

	if (!hbsdmon_keyvalue_store(kv, "poolhandle", &poolhandle,
	    sizeof(poolhandle))) {
		return (false);
	}
	hbsdmon_node_append_kv(node, kv);

	return (true);
}

bool
hbsdmon_zfs_status(hbsdmon_node_t *node)
{
	zpool_handle_t *poolhandle;
	hbsdmon_keyvalue_t *kv;
	zpool_errata_t errata;
	zpool_status_t reason;
	char *msgid;

	kv = hbsdmon_find_kv_in_node(node, "poolhandle", false);
	if (kv == NULL) {
		fprintf(stderr, "[-] poolhandle not set!\n");
		return (true);
	}

	poolhandle = *((zpool_handle_t **)kv->hk_value);

	msgid = NULL;
	errata = 0;
	reason = 0;
	reason = zpool_get_status(poolhandle, &msgid, &errata);
	switch (reason) {
	case ZPOOL_STATUS_OK:
	case ZPOOL_STATUS_VERSION_OLDER:
	case ZPOOL_STATUS_FEAT_DISABLED:
	case ZPOOL_STATUS_COMPATIBILITY_ERR:
		return (true);
	default:
		return (false);
	}
}
