PROG=	hbsdmon
MAN=

SRCS+=	config.c
SRCS+=	hbsdmon.c
SRCS+=	keyvalue.c
SRCS+=	net_tcp.c
SRCS+=	net_udp.c
SRCS+=	node.c
SRCS+=	thread.c
SRCS+=	util.c

BINDIR?=	/usr/bin

CFLAGS+=	-I${.CURDIR} \
		-I${.CURDIR}/../../lib/libpushover \
		-I/usr/local/include

LDFLAGS+=	-L${.CURDIR}/../../lib/libpushover \
		-L/usr/local/lib

LDADD+=		-lcurl -lpthread -lpushover -lsbuf -lucl -lzmq

CFLAGS+=	-fPIE -flto -fvisibility=hidden -fsanitize=cfi -fsanitize=safe-stack
LDFLAGS+=	-fPIE -pie -flto -fsanitize=cfi -fsanitize=safe-stack

.include <bsd.prog.mk>
