# cwm - c window manager
# See LICENSE file for copyright and license details

include /etc/make.conf

PREFIX = /usr/local
SRC = cwm.c

LIBS = -lxcb -lxcb-icccm -lxcb-randr
CFLAGS += -Wall
LDFLAGS = ${LIBS}

CC = gcc


all: options cwm

options:
	@echo "# cwm build options:"
	@echo "CFLAGS	= ${CFLAGS}"
	@echo "LDFLAGS	= ${LDFLAGS}"
	@echo "CC	= ${CC}"

cwm:
	@echo "# CC -o $@"
	@${CC} ${SRC} ${LDFLAGS} -o $@ > cc.log

clean:
	@echo "# cleaning"
	@rm -f cwm
	@rm -f cc.log

install:
	@echo "# installing executable file to ${DESTDIR}${PREFIX}/bin"
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f cwm ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/cwm

uninstall:
	@echo "# removing executable file from ${DESTDIR}/bin"
	@rm -f ${DESTDIR}${PREFIX}/bin/cwm

.PHONY: all options install clean uninstall
