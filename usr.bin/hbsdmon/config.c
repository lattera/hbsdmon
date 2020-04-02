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

end:
	if (res == false) {
		free(ctx->hc_dest);
		ctx->hc_dest = NULL;
	}
	ucl_parser_free(parser);
	return (res);
}
