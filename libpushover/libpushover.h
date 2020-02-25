#ifndef _LIBPUSHOVER_H
#define _LIBPUSHOVER_H

#include <stdbool.h>

#include <curl/curl.h>

#define EXPORTED_SYM	 __attribute__((visibility("default")))
#define PUSHOVER_URI	 "https://api.pushover.net/1/messages.json"

typedef enum _pushover_priority {
	PSH_PRIO_NONE = -2,
	PSH_PRIO_QUIET = -1,
	PSH_PRIO_DEF = 0,
	PSH_PRIO_HIGH = 1,
	PSH_PRIO_CONFIRM = 2
} pushover_priority_t;

typedef struct _pushover_ctx {
	char		*psh_token;
	char		*psh_uri;
} pushover_ctx_t;

typedef struct _pushover_message {
	char			*psh_user;
	char			*psh_msg;
	char			*psh_title;
	char			*psh_device;
	pushover_priority_t	 psh_priority;
} pushover_message_t;

pushover_ctx_t *pushover_init_ctx(const char *);
bool pushover_set_uri(pushover_ctx_t *, const char *);
bool pushover_set_token(pushover_ctx_t *, const char *);
pushover_message_t *pushover_init_message(pushover_message_t *);
bool pushover_message_set_msg(pushover_message_t *, char *);
bool pushover_message_set_user(pushover_message_t *, char *);
bool pushover_message_set_title(pushover_message_t *, char *);
bool pushover_message_set_device(pushover_message_t *, char *);
bool pushover_message_set_priority(pushover_message_t *,
    pushover_priority_t);
bool pushover_submit_message(pushover_ctx_t *, pushover_message_t *);

#endif /* !_LIBPUSHOVER_H */
