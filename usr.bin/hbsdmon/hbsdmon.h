#ifndef _HBSDMON_H
#define _HBSDMON_H

#include <sys/queue.h>

#include "libpushover.h"

typedef struct _hbsdmon_keyvalue {
	char				*hk_key;
	union {
		char			*hk_charp;
		void			*hk_voidp;
		uint64_t		 hk_uint;
	};
	char				*hk_value;
	size_t				 hk_value_len;
	SLIST_ENTRY(_hbsdmon_keyvalue)	 hk_entry;
} hbsdmon_keyvalue_t;

typedef struct _hbsdmon_node {
	char				*hn_host;
	char				*hn_method;
	SLIST_HEAD(, _hbsdmon_keyvalue)	 hn_kvstore;
	SLIST_ENTRY(_hbsdmon_node)	 hn_entry;
} hbsdmon_node_t;

typedef struct _hbsdmon_ctx {
	char				*hc_config;
	char				*hc_dest;
	pushover_ctx_t			*hc_psh_ctx;
	SLIST_HEAD(, _hbsdmon_node)	 hc_nodes;
} hbsdmon_ctx_t;

hbsdmon_ctx_t *new_ctx(void);
pushover_ctx_t *get_psh_ctx(hbsdmon_ctx_t *);
bool parse_config(hbsdmon_ctx_t *);

#endif /* !_HBSDMON_H */