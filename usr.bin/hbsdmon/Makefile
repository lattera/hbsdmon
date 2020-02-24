PROG=	hbsdmon
SRCS=	hbsdmon.c
MAN=

BINDIR?=	/usr/bin

CFLAGS+=	-I${.CURDIR} \
		-I${.CURDIR}/../../lib/libpushover \
		-I/usr/local/include

LDFLAGS+=	-L${.CURDIR}/../../lib/libpushover \
		-L/usr/local/lib

LDADD+=		-lcurl -lpushover

.include <bsd.prog.mk>
