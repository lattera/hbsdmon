PROG=	hbsdmon
MAN=

SRCS+=	config.c
SRCS+=	hbsdmon.c
SRCS+=	keyvalue.c
SRCS+=	net_tcp.c
SRCS+=	node.c
SRCS+=	util.c

BINDIR?=	/usr/bin

CFLAGS+=	-I${.CURDIR} \
		-I${.CURDIR}/../../lib/libpushover \
		-I/usr/local/include

LDFLAGS+=	-L${.CURDIR}/../../lib/libpushover \
		-L/usr/local/lib

LDADD+=		-lcurl -lpushover -lucl -lzmq

.include <bsd.prog.mk>
