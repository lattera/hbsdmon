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
#include <unistd.h>

#include "hbsdmon.h"

int
main(int argc, char *argv[])
{
	pushover_message_t *msg;
	hbsdmon_ctx_t *ctx;
	pushover_ctx_t *pctx;
	int ch, res;

	ctx = new_ctx();
	if (ctx == NULL)
		return (1);

	msg = pushover_init_message(NULL);
	if (msg == NULL)
		return (1);

	while ((ch = getopt(argc, argv, "c:u:m:p:t:")) != -1) {
		switch (ch) {
		case 'c':
			ctx->hc_config = strdup(optarg);
			break;
		case 'u':
			pushover_message_set_user(msg, optarg);
			break;
		case 'm':
			pushover_message_set_msg(msg, optarg);
			break;
		case 'p':
			break;
		case 't':
			pushover_message_set_title(msg, optarg);
			break;
		}
	}

	if (parse_config(ctx) == false) {
		fprintf(stderr, "[-] Configuration parsing failed. Bailing.\n");
		return (1);
	}

	pctx = get_psh_ctx(ctx);
#if 0
	res = pushover_submit_message(pctx, msg);
#else
	res = 0;
#endif

	pushover_free_message(&msg);
	pushover_free_ctx(&(ctx->hc_psh_ctx));

	return (res);
}
