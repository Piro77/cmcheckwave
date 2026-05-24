PREFIX          = /usr/local
TARGETS		= ${TARGET1} ${TARGET2}
TARGET1		= cmcheckwave
OBJ_TARGET1	= cmcheckwave.o tclist.o
TARGET2		= fixass
OBJ_TARGET2	= fixass.o tclist.o

LANG=C
CC		= clang
CFLAGS		= -g
LDFLAGS		=
LIBS		=

.c.o:
	${CC} ${CFLAGS} -c $<

all:		${TARGETS}


${TARGET1}:	${OBJ_TARGET1}
		${CC} ${CFLAGS} ${OBJ_TARGET1} -o $@ ${LDFLAGS} ${LIBS}

${TARGET2}:	${OBJ_TARGET2}
		${CC} ${CFLAGS} ${OBJ_TARGET2} -o $@ ${LDFLAGS} ${LIBS}

${OBJ_TARGET1}:	${HEDDERDEPEND}

${OBJ_TARGET2}:	${HEDDERDEPEND}

clean:
		rm -f core ${TARGETS} *.o

install:
	install -m 755 ${TARGETS} ${PREFIX}/bin
