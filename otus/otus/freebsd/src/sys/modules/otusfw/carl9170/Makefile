# $FreeBSD$

KMOD=	otusfw
FIRMWS=	otusfw:otusfw:1

CLEANFILES=	otusfw

otusfw: ${.CURDIR}/../../../contrib/dev/otus/carl9170-1.fw
	cp ${.CURDIR}/../../../contrib/dev/otus/carl9170-1.fw ${.TARGET}

.include <bsd.kmod.mk>
