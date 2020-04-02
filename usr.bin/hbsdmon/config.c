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

	SLIST_INIT(&(ctx->hc_nodes));

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

static hbsdmon_method_t
parse_method(const char *method)
{
	if (!strcasecmp(method, "ICMP"))
		return METHOD_ICMP;
	if (!strcasecmp(method, "HTTPS"))
		return METHOD_HTTPS;
	if (!strcasecmp(method, "HTTP"))
		return METHOD_HTTP;

	/* Default to ICMP */
	return METHOD_ICMP;
}

static bool
parse_nodes(hbsdmon_ctx_t *ctx, const ucl_object_t *top)
{
	const ucl_object_t *ucl_nodes, *ucl_node, *ucl_tmp;
	ucl_object_iter_t ucl_it, ucl_it_obj;
	hbsdmon_keyvalue_t *kv;
	hbsdmon_node_t *node;
	const char *str;

	ucl_nodes = ucl_lookup_path(top, ".nodes");
	if (ucl_nodes == NULL) {
		fprintf(stderr, "[-] No nodes defined.\n");
		return (false);
	}

	ucl_it = ucl_it_obj = NULL;

	while ((ucl_node = ucl_iterate_object(ucl_nodes, &ucl_it, true))) {
		node = calloc(1, sizeof(*node));
		if (node == NULL) {
			perror("calloc");
			return (false);
		}

		SLIST_INIT(&(node->hn_kvstore));

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

		node->hn_method = parse_method(str);

		SLIST_INSERT_HEAD(&(ctx->hc_nodes), node, hn_entry);
	}

	return (true);
}
