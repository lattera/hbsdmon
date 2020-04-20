#ifndef _HBSDMON_H
#define _HBSDMON_H

#include <pthread.h>
#include <sys/queue.h>
#include <time.h>

#include <zmq.h>

#include "libpushover.h"

struct _hbsdmon_ctx;
struct _hbsdmon_thread;

typedef enum _hbsdmon_method {
	METHOD_HTTP,
	METHOD_HTTPS,
	METHOD_ICMP,
	METHOD_SSH,
	METHOD_TCP,
	METHOD_TOR,
} hbsdmon_method_t;

typedef enum _hbsdmon_thread_msg_verb {
	VERB_INIT,
	VERB_FINI,
	VERB_HEARTBEAT,
	VERB_TERM,
} hbsdmon_thread_msg_verb_t;

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
	struct _hbsdmon_thread		*hn_thread;
	hbsdmon_method_t		 hn_method;
	hbsdmon_keyvalue_store_t	*hn_kvstore;
	SLIST_ENTRY(_hbsdmon_node)	 hn_entry;
} hbsdmon_node_t;

typedef struct _hbsdmon_thread {
	uint64_t			 ht_flags;
	pthread_t			 ht_tid;
	void				*ht_zmqsock;
	void				*ht_zmqtsock;
	char				*ht_sockname;
	struct _hbsdmon_ctx		*ht_ctx;
	hbsdmon_node_t			*ht_node;
	SLIST_ENTRY(_hbsdmon_thread)	 ht_entry;
} hbsdmon_thread_t;

typedef struct _hbsdmon_thread_msg {
	hbsdmon_thread_msg_verb_t	 htm_verb;

	union {
		void			*htm_void;
		hbsdmon_node_t		*htm_node;
		hbsdmon_thread_t	*htm_thread;
		uint64_t		 htm_uint64;
	};
} hbsdmon_thread_msg_t;

typedef struct _hbsdmon_ctx {
	char				*hc_config;
	char				*hc_dest;
	pushover_ctx_t			*hc_psh_ctx;
	hbsdmon_keyvalue_store_t	*hc_kvstore;
	void				*hc_zmq;
	size_t				 hc_nthreads;
	size_t				 hc_nnodes;
	pthread_mutex_t			 hc_mtx;
	SLIST_HEAD(, _hbsdmon_node)	 hc_nodes;
	SLIST_HEAD(, _hbsdmon_thread)	 hc_threads;
} hbsdmon_ctx_t;

hbsdmon_ctx_t *new_ctx(void);
pushover_ctx_t *get_psh_ctx(hbsdmon_ctx_t *);
bool parse_config(hbsdmon_ctx_t *);
hbsdmon_method_t hbsdmon_str_to_method(const char *);
const char *hbsdmon_method_to_str(hbsdmon_method_t);
long hbsdmon_get_interval(hbsdmon_node_t *);

hbsdmon_node_t *hbsdmon_new_node(void);
hbsdmon_keyvalue_store_t *hbsdmon_node_kv(hbsdmon_node_t *);
void hbsdmon_node_append_kv(hbsdmon_node_t *, hbsdmon_keyvalue_t *);
void hbsdmon_node_debug_print(hbsdmon_node_t *);
hbsdmon_keyvalue_t *hbsdmon_find_kv_in_node(hbsdmon_node_t *,
    const char *, bool);
void hbsdmon_node_thread_init(hbsdmon_thread_t *);
bool hbsdmon_node_thread_run(hbsdmon_thread_t *);
hbsdmon_node_t *hbsdmon_find_node_by_zmqsock(hbsdmon_ctx_t *, void *);

hbsdmon_keyvalue_t *hbsdmon_new_keyvalue(void);
bool hbsdmon_keyvalue_store(hbsdmon_keyvalue_t *, const char *,
    void *, size_t);
bool hbsdmon_keyvalue_modify(hbsdmon_keyvalue_store_t *,
    hbsdmon_keyvalue_t *, void *, size_t, bool);
uint64_t hbsdmon_keyvalue_to_uint64(hbsdmon_keyvalue_t *);
int hbsdmon_keyvalue_to_int(hbsdmon_keyvalue_t *);
char *hbsdmon_keyvalue_to_str(hbsdmon_keyvalue_t *);
time_t hbsdmon_keyvalue_to_time(hbsdmon_keyvalue_t *);
void hbsdmon_append_kv(hbsdmon_keyvalue_store_t *,
    hbsdmon_keyvalue_t *);
hbsdmon_keyvalue_t *hbsdmon_find_kv(hbsdmon_keyvalue_store_t *,
    const char *, bool);
void hbsdmon_free_kv(hbsdmon_keyvalue_store_t *,
    hbsdmon_keyvalue_t **, bool);
void hbsdmon_free_kvstore(hbsdmon_keyvalue_store_t **);

hbsdmon_keyvalue_store_t *hbsdmon_new_kv_store(void);
int hbsdmon_lock_kvstore(hbsdmon_keyvalue_store_t *);
int hbsdmon_unlock_kvstore(hbsdmon_keyvalue_store_t *);

bool hbsdmon_tcp_ping(hbsdmon_node_t *);
bool hbsdmon_http_ping(hbsdmon_node_t *);

bool hbsdmon_thread_init(hbsdmon_ctx_t *);
void hbsdmon_node_cleanup(hbsdmon_node_t *);

#endif /* !_HBSDMON_H */
