PREFIX          = /usr/local
TARGETS		= cmcheckwave
OBJ_TARGETS	= cmcheckwave.o tclist.o

LANG=C
CC		= gcc
CFLAGS		= -g
LDFLAGS		=
LIBS		=

.c.o:
	${CC} ${CFLAGS} -c $<

all:		${TARGETS}


${TARGETS}:	${OBJ_TARGETS}
		${CC} ${CFLAGS} ${OBJ_TARGETS} -o $@ ${LDFLAGS} ${LIBS}

${OBJ_TARGETS}:	${HEDDERDEPEND}

clean:
		rm -f core ${TARGETS} *.o

install:
	install -m 755 ${TARGETS} ${PREFIX}/bin
