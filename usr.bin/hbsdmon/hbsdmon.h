#ifndef _HBSDMON_H
#define _HBSDMON_H

#include "libpushover.h"

typedef struct _hbsdmon_ctx {
	char		*hc_config;
	pushover_ctx_t	*hc_psh_ctx;
} hbsdmon_ctx_t;

hbsdmon_ctx_t *new_ctx(void);
pushover_ctx_t *get_psh_ctx(hbsdmon_ctx_t *);
bool parse_config(hbsdmon_ctx_t *);

#endif /* !_HBSDMON_H */
