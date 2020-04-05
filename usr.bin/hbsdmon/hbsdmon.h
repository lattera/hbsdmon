#ifndef _HBSDMON_H
#define _HBSDMON_H

#include <pthread.h>
#include <sys/queue.h>

#include "libpushover.h"

typedef enum _hbsdmon_method {
	METHOD_HTTP,
	METHOD_HTTPS,
	METHOD_ICMP,
	METHOD_TCP,
} hbsdmon_method_t;

typedef struct _hbsdmon_keyvalue {
	char				*hk_key;
	void				*hk_value;
	size_t				 hk_value_len;
	SLIST_ENTRY(_hbsdmon_keyvalue)	 hk_entry;
} hbsdmon_keyvalue_t;

typedef struct _hbsdmon_keyvalue_store {
	pthread_mutex_t			 hks_mtx;
	SLIST_HEAD(, _hbsdmon_keyvalue)	 hks_store;
} hbsdmon_keyvalue_store_t;

typedef struct _hbsdmon_node {
	char				*hn_host;
	hbsdmon_method_t		 hn_method;
	hbsdmon_keyvalue_store_t	*hn_kvstore;
	SLIST_ENTRY(_hbsdmon_node)	 hn_entry;
} hbsdmon_node_t;

typedef struct _hbsdmon_ctx {
	char				*hc_config;
	char				*hc_dest;
	pushover_ctx_t			*hc_psh_ctx;
	hbsdmon_keyvalue_store_t	*hc_kvstore;
	SLIST_HEAD(, _hbsdmon_node)	 hc_nodes;
} hbsdmon_ctx_t;

hbsdmon_ctx_t *new_ctx(void);
pushover_ctx_t *get_psh_ctx(hbsdmon_ctx_t *);
bool parse_config(hbsdmon_ctx_t *);
hbsdmon_method_t hbsdmon_str_to_method(const char *);
const char *hbsdmon_method_to_str(hbsdmon_method_t);

hbsdmon_node_t *hbsdmon_new_node(void);
hbsdmon_keyvalue_store_t *hbsdmon_node_kv(hbsdmon_node_t *);
void hbsdmon_node_append_kv(hbsdmon_node_t *, hbsdmon_keyvalue_t *);
void hbsdmon_node_debug_print(hbsdmon_node_t *);
hbsdmon_keyvalue_t *hbsdmon_find_kv_in_node(hbsdmon_node_t *,
    const char *, bool);

hbsdmon_keyvalue_t *hbsdmon_new_keyvalue(void);
bool hbsdmon_keyvalue_store(hbsdmon_keyvalue_t *, const char *,
    void *, size_t);
bool hbsdmon_keyvalue_modify(hbsdmon_keyvalue_store_t *,
    hbsdmon_keyvalue_t *, void *, size_t, bool);
uint64_t hbsdmon_keyvalue_to_uint64(hbsdmon_keyvalue_t *);
int hbsdmon_keyvalue_to_int(hbsdmon_keyvalue_t *);
void hbsdmon_append_kv(hbsdmon_keyvalue_store_t *,
    hbsdmon_keyvalue_t *);
hbsdmon_keyvalue_t *hbsdmon_find_kv(hbsdmon_keyvalue_store_t *,
    const char *, bool);

hbsdmon_keyvalue_store_t *hbsdmon_new_kv_store(void);
int hbsdmon_lock_kvstore(hbsdmon_keyvalue_store_t *);
int hbsdmon_unlock_kvstore(hbsdmon_keyvalue_store_t *);

bool hbsdmon_tcp_ping(hbsdmon_node_t *);

#endif /* !_HBSDMON_H */