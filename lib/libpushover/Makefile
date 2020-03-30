SHLIB=	pushover
SHLIB_MAJOR=	0
SRCS=	libpushover.c sanity.c
INCS=	libpushover.h

CFLAGS+=	-I${.CURDIR} -I/usr/local/include
LDFLAGS+=	-L/usr/local/lib

LDADD=		-lcurl

.include <bsd.lib.mk>
