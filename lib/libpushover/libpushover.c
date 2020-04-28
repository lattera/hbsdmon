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

#include <sys/types.h>
#include <sys/sbuf.h>

#include "libpushover.h"

static char *msg_to_str(pushover_ctx_t *, pushover_message_t *, CURL *);
static size_t pushover_curl_write_data(void *, size_t, size_t, void *);

EXPORTED_SYM
pushover_ctx_t *
pushover_init_ctx(const char *token)
{
	pushover_ctx_t *res;

	res = calloc(1, sizeof(*res));
	if (res == NULL)
		return (NULL);

	res->psh_uri = strdup(PUSHOVER_URI);
	if (res->psh_uri == NULL) {
		free(res);
		res = NULL;
		goto out;
	}

	if (token != NULL)
		res->psh_token = strdup(token);

out:
	return (res);
}

EXPORTED_SYM
void
pushover_free_ctx(pushover_ctx_t **ctx)
{
	pushover_ctx_t *ctxp;

	if (ctx == NULL || *ctx == NULL)
		return;

	ctxp = *ctx;

	free(ctxp->psh_uri);
	free(ctxp->psh_token);
	free(ctxp);
	*ctx = NULL;
}

EXPORTED_SYM
bool
pushover_set_uri(pushover_ctx_t *ctx, const char *uri)
{

	assert(ctx != NULL);
	assert(uri != NULL);

	ctx->psh_uri = strdup(uri);
	return (ctx->psh_uri != NULL);
}

EXPORTED_SYM
bool
pushover_set_token(pushover_ctx_t *ctx, const char *token)
{

	assert(ctx != NULL);
	assert(token != NULL);

	ctx->psh_token = strdup(token);
	return (ctx->psh_token != NULL);
}

EXPORTED_SYM
pushover_message_t *
pushover_init_message(pushover_message_t *msg)
{
	uint64_t flags;

	flags = 0;

	if (msg == NULL) {
		msg = calloc(1, sizeof(*msg));
		flags |= PUSHOVER_FLAGS_ALLOC;
	} else {
		memset(msg, 0, sizeof(*msg));
	}

	if (msg == NULL)
		return (NULL);

	msg->psh_flags = flags;

	return (msg);
}

EXPORTED_SYM
void
pushover_free_message(pushover_message_t **msg)
{
	pushover_message_t *msgp;

	if (msg == NULL || *msg == NULL)
		return;

	msgp = *msg;

	free(msgp->psh_dest);
	free(msgp->psh_msg);
	free(msgp->psh_title);
	free(msgp->psh_device);

	if (msgp->psh_flags & PUSHOVER_FLAGS_ALLOC) {
		free(msgp);
		*msg = NULL;
	}
}

EXPORTED_SYM
bool
pushover_message_set_msg(pushover_message_t *msg, char *data)
{

	assert(msg != NULL);
	assert(data != NULL);

	msg->psh_msg = strdup(data);
	return (msg->psh_msg != NULL);
}

EXPORTED_SYM
bool
pushover_message_set_dest(pushover_message_t *msg, char *dest)
{

	assert(msg != NULL);
	assert(dest != NULL);

	msg->psh_dest = strdup(dest);
	return (msg->psh_dest != NULL);
}

EXPORTED_SYM
bool
pushover_message_set_title(pushover_message_t *msg, char *title)
{

	assert(msg != NULL);
	assert(title != NULL);

	msg->psh_title = strdup(title);
	return (msg->psh_title != NULL);
}

EXPORTED_SYM
bool
pushover_message_set_device(pushover_message_t *msg, char *device)
{

	assert(msg != NULL);
	assert(device != NULL);

	msg->psh_device = strdup(device);
	return (msg->psh_device != NULL);
}

EXPORTED_SYM
bool
pushover_message_set_priority(pushover_message_t *msg,
    pushover_priority_t prio)
{

	assert(msg != NULL);

	if (!pushover_message_priority_sane(msg->psh_priority))
		return (false);

	msg->psh_priority = prio;

	return (true);
}

EXPORTED_SYM
bool
pushover_submit_message(pushover_ctx_t *ctx, pushover_message_t *msg)
{
	CURLcode curl_code;
	char *post_str;
	CURL *curl;
	bool res;

	assert(ctx != NULL);
	assert(msg != NULL);
	assert(ctx->psh_token != NULL);
	assert(msg->psh_dest != NULL);
	assert(msg->psh_msg != NULL);
	assert(pushover_message_priority_sane(msg->psh_priority));

	res = false;
	post_str = NULL;

	curl = curl_easy_init();
	if (curl == NULL)
		goto end;

	curl_easy_setopt(curl, CURLOPT_URL, ctx->psh_uri);

	post_str = msg_to_str(ctx, msg, curl);
	if (post_str == NULL)
		goto end;
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_str);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl, CURLOPT_STDERR, NULL);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, pushover_curl_write_data);

	curl_code = curl_easy_perform(curl);
	res = (curl_code == CURLE_OK);

end:
	curl_easy_cleanup(curl);
	free(post_str);
	return (res);
}

static char *
msg_to_str(pushover_ctx_t *ctx, pushover_message_t *msg, CURL *curl)
{
	char *p, *res;
	struct sbuf *sb;

	assert(ctx != NULL);
	assert(msg != NULL);

	sb = sbuf_new_auto();
	if (sb == NULL) {
		goto end;
	}

	res = NULL;
	if (msg->psh_device != NULL) {
		p = curl_easy_escape(curl, msg->psh_device, 0);
		if (p == NULL) {
			goto end;
		}
		if (sbuf_printf(sb, "&device=%s", p)) {
			goto end;
		}
		free(p);
	}
	if (msg->psh_msg != NULL) {
		p = curl_easy_escape(curl, msg->psh_msg, 0);
		if (p == NULL) {
			goto end;
		}
		if (sbuf_printf(sb, "&message=%s", p)) {
			goto end;
		}
		free(p);
	}
	if (msg->psh_title != NULL) {
		p = curl_easy_escape(curl, msg->psh_title, 0);
		if (p == NULL) {
			goto end;
		}
		if (sbuf_printf(sb, "&title=%s", p)) {
			goto end;
		}
		free(p);
	}
	if (ctx->psh_token != NULL) {
		p = curl_easy_escape(curl, ctx->psh_token, 0);
		if (p == NULL) {
			goto end;
		}
		if (sbuf_printf(sb, "&token=%s", p)) {
			goto end;
		}
		free(p);
	}
	if (msg->psh_dest != NULL) {
		p = curl_easy_escape(curl, msg->psh_dest, 0);
		if (p == NULL) {
			goto end;
		}
		if (sbuf_printf(sb, "&user=%s", p)) {
			goto end;
		}
		free(p);
	}

	if (sbuf_printf(sb, "&priority=%d", msg->psh_priority)) {
		goto end;
	}

end:
	if (sb != NULL) {
		if (sbuf_finish(sb)) {
			return (NULL);
		}
		res = strdup(sbuf_data(sb));
	}
	return (res);
}

static size_t
pushover_curl_write_data(void *buffer, size_t sz, size_t nmemb,
    void *usrp)
{

	return (sz * nmemb);
}

__attribute__((constructor))
static void
init_libpushover(void)
{

	curl_global_init(CURL_GLOBAL_ALL);
}

__attribute__((destructor))
static void
destroy_libpushover(void)
{

	curl_global_cleanup();
}
