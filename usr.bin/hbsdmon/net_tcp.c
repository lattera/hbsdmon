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

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <ucl.h>

#include "hbsdmon.h"

static size_t hbsdmon_curl_write_data(void *, size_t,
    size_t, void *);

bool
hbsdmon_tcp_ping(hbsdmon_node_t *node)
{
	struct addrinfo hints, *servinfo, *servp;
	hbsdmon_keyvalue_t *kv;
	char buf[512];
	int port, res, sockfd;
	int addrfam;
	bool ret;

	kv = hbsdmon_find_kv_in_node(node, "port", false);
	if (kv == NULL) {
		return (false);
	}

	port = hbsdmon_keyvalue_to_int(kv);
	snprintf(buf, sizeof(buf)-1, "%d", port);

	addrfam = AF_UNSPEC;
	kv = hbsdmon_find_kv_in_node(node, "addrfam", false);
	if (kv != NULL) {
		addrfam = (int)hbsdmon_keyvalue_to_uint64(kv);
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = addrfam;
	hints.ai_socktype = SOCK_STREAM;

	servinfo = NULL;
	res = getaddrinfo(node->hn_host, buf, &hints, &servinfo);
	if (res) {
		return (false);
	}

	ret = false;
	for (servp = servinfo; servp != NULL; servp = servp->ai_next) {
		sockfd = socket(servp->ai_family, servp->ai_socktype,
		    servp->ai_protocol);
		if (sockfd == -1) {
			continue;
		}

		if (connect(sockfd, servp->ai_addr,
		    servp->ai_addrlen)) {
			close(sockfd);
			continue;
		}

		close(sockfd);
		ret = true;
		break;
	}

end:
	freeaddrinfo(servinfo);
	return (ret);
}

bool
hbsdmon_http_ping(hbsdmon_node_t *node)
{
	CURLcode curlcode;
	char url[512];
	CURL *curl;
	bool res;

	res = false;
	curl = curl_easy_init();
	if (curl == NULL) {
		return (false);
	}

	snprintf(url, sizeof(url)-1, "http://%s/", node->hn_host);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl, CURLOPT_STDERR, NULL);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
	    hbsdmon_curl_write_data);
	curlcode = curl_easy_perform(curl);

	curl_easy_cleanup(curl);
	return (curlcode == CURLE_OK);
}

static size_t hbsdmon_curl_write_data(void *buffer, size_t sz,
    size_t nmemb, void *usrp)
{

	return (sz * nmemb);
}
