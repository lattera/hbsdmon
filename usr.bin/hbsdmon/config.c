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

static bool parse_nodes(hbsdmon_ctx_t *, const ucl_object_t *);

hbsdmon_ctx_t *
new_ctx(void)
{
	hbsdmon_ctx_t *ctx;

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL)
		return (NULL);

	ctx->hc_psh_ctx = pushover_init_ctx(NULL);
	if (ctx->hc_psh_ctx == NULL) {
		free(ctx);
		return (NULL);
	}

	ctx->hc_kvstore = hbsdmon_new_kv_store();
	if (ctx->hc_kvstore == NULL) {
		pushover_free_ctx(&(ctx->hc_psh_ctx));
		free(ctx);
		return (NULL);
	}

	ctx->hc_zmq = zmq_ctx_new();
	if (ctx->hc_zmq == NULL) {
		hbsdmon_free_kvstore(&ctx->hc_kvstore);
		pushover_free_ctx(&(ctx->hc_psh_ctx));
		free(ctx);
		return (NULL);
	}

	SLIST_INIT(&(ctx->hc_nodes));
	SLIST_INIT(&(ctx->hc_threads));

	return (ctx);
}

pushover_ctx_t *
get_psh_ctx(hbsdmon_ctx_t *ctx)
{

	assert(ctx != NULL);
	return (ctx->hc_psh_ctx);
}

bool
parse_config(hbsdmon_ctx_t *ctx)
{
	struct ucl_parser *parser;
	const ucl_object_t *top, *obj;
	const char *str;
	bool res;

	assert(ctx != NULL);
	assert(ctx->hc_config != NULL);

	res = true;
	top = NULL;

	parser = ucl_parser_new(UCL_PARSER_KEY_LOWERCASE);
	if (parser == NULL) {
		return (false);
	}

	if (!ucl_parser_add_file(parser, ctx->hc_config)) {
		res = false;
		goto end;
	}

	top = ucl_parser_get_object(parser);
	if (top == NULL) {
		res = false;
		goto end;
	}

	obj = ucl_lookup_path(top, ".token");
	if (obj == NULL) {
		res = false;
		goto end;
	}

	str = ucl_object_tostring(obj);
	if (str == NULL) {
		res = false;
		goto end;
	}

	if (!pushover_set_token(get_psh_ctx(ctx), str)) {
		res = false;
		goto end;
	}

	obj = ucl_lookup_path(top, ".dest");
	if (obj == NULL) {
		res = false;
		goto end;
	}

	str = ucl_object_tostring(obj);
	if (str == NULL) {
		res = false;
		goto end;
	}

	ctx->hc_dest = strdup(str);
	if (ctx->hc_dest == NULL) {
		res = false;
		goto end;
	}

	res = parse_nodes(ctx, top);

end:
	if (res == false) {
		free(ctx->hc_dest);
		ctx->hc_dest = NULL;
	}
	ucl_parser_free(parser);
	return (res);
}

static bool
parse_nodes(hbsdmon_ctx_t *ctx, const ucl_object_t *top)
{
	const ucl_object_t *ucl_nodes, *ucl_node, *ucl_tmp;
	ucl_object_iter_t ucl_it, ucl_it_obj;
	hbsdmon_keyvalue_t *kv;
	hbsdmon_node_t *node;
	uint64_t kv_uint;
	const char *str;
	int64_t ucl_int;
	int port;

	ucl_nodes = ucl_lookup_path(top, ".nodes");
	if (ucl_nodes == NULL) {
		fprintf(stderr, "[-] No nodes defined.\n");
		return (false);
	}

	ucl_it = ucl_it_obj = NULL;

	while ((ucl_node = ucl_iterate_object(ucl_nodes, &ucl_it, true))) {
		node = hbsdmon_new_node();
		if (node == NULL) {
			perror("calloc");
			return (false);
		}

		ucl_tmp = ucl_lookup_path(ucl_node, ".host");
		if (ucl_tmp == NULL) {
			fprintf(stderr, "[-] Host not defined for node.\n");
			return (false);
		}

		str = ucl_object_tostring(ucl_tmp);
		if (str == NULL) {
			fprintf(stderr, "[-] Host is not a string.\n");
			return (false);
		}

		node->hn_host = strdup(str);
		if (node->hn_host == NULL) {
			perror("strdup");
			return (false);
		}

		ucl_tmp = ucl_lookup_path(ucl_node, ".method");
		if (ucl_tmp == NULL) {
			fprintf(stderr, "[-] Method not defined for host %s\n",
			    node->hn_host);
			return (false);
		}

		str = ucl_object_tostring(ucl_tmp);
		if (str == NULL) {
			fprintf(stderr, "[-] Method is not a string.\n");
			return (false);
		}

		node->hn_method = hbsdmon_str_to_method(str);
		switch (node->hn_method) {
		case METHOD_HTTP:
		case METHOD_HTTPS:
		case METHOD_TCP:
			ucl_tmp = ucl_lookup_path(ucl_node, ".port");
			if (ucl_tmp == NULL) {
				fprintf(stderr, "[-] Port not defined.\n");
				return (false);
			}
			if (!ucl_object_toint_safe(ucl_tmp, &ucl_int)) {
				fprintf(stderr, "[-] Port is not an integer.\n");
				return (false);
			}

			port = ucl_int;
			kv = hbsdmon_new_keyvalue();
			if (kv == NULL) {
				fprintf(stderr, "[-] Could not create new keyvalue object.\n");
				return (false);
			}

			if (!hbsdmon_keyvalue_store(kv, "port", &port,
			    sizeof(port))) {
				fprintf(stderr, "[-] Could not store port.\n");
				return (false);
			}

			hbsdmon_node_append_kv(node, kv);
			break;
		default:
			break;
		}

		SLIST_INSERT_HEAD(&(ctx->hc_nodes), node, hn_entry);
		hbsdmon_node_debug_print(node);
	}

	return (true);
}
