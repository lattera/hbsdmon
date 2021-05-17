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
SRCS+=	zfs.c

OPTIMIZATION_CFLAGS=

BINDIR?=	/usr/bin

CFLAGS+=	-I${.CURDIR} \
		-I${.CURDIR}/../../lib/libpushover \
		-I/usr/local/include


# All these CFLAGS are for ZFS, stolen from libbe's Makefile
CFLAGS+=	-DIN_BASE -DHAVE_RPC_TYPES
CFLAGS+= 	-I${SRCTOP}/sys/contrib/openzfs/include
CFLAGS+= 	-I${SRCTOP}/sys/contrib/openzfs/include/os/freebsd
CFLAGS+= 	-I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include
CFLAGS+= 	-I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/os/freebsd
CFLAGS+= 	-I${SRCTOP}/sys
CFLAGS+= 	-I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= 	-include ${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= 	-DHAVE_ISSETUGID

LDFLAGS+=	-L${.CURDIR}/../../lib/libpushover \
		-L/usr/local/lib

LDADD+=		-lcurl -lpthread -lpushover -lsbuf -lucl -lzmq -lzfs -lnvpair -lspl

#CFLAGS+=	-fPIE -flto -fvisibility=hidden -fsanitize=cfi -fsanitize=safe-stack
#LDFLAGS+=	-fPIE -pie -flto -fsanitize=cfi -fsanitize=safe-stack

.include <bsd.prog.mk>
