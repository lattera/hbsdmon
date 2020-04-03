PROG=	hbsdmon
MAN=

SRCS+=	config.c
SRCS+=	hbsdmon.c
SRCS+=	keyvalue.c
SRCS+=	util.c

BINDIR?=	/usr/bin

CFLAGS+=	-I${.CURDIR} \
		-I${.CURDIR}/../../lib/libpushover \
		-I/usr/local/include

LDFLAGS+=	-L${.CURDIR}/../../lib/libpushover \
		-L/usr/local/lib

LDADD+=		-lcurl -lpushover -lucl

.include <bsd.prog.mk>
