# dwm - dynamic window manager
# See LICENSE file for copyright and license details.

.SILENT:

include config.mk

SRC = drw.c dwm.c util.c
OBJ = ${SRC:.c=.o}

all: options dwm

options:
	@echo dwm build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.mk
dwm.o: config.h drw.h util.h movestack.h switcher.c
drw.o: drw.h util.h
util.o: util.h

dwm: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f dwm ${OBJ} dwm-${VERSION}.tar.gz

dist: clean
	mkdir -p dwm-${VERSION}
	cp -R LICENSE Makefile README.md config.mk\
		dwm.1 drw.h util.h config.h movestack.h switcher.c ${SRC} transient.c tests dwm-${VERSION}
	tar -cf dwm-${VERSION}.tar dwm-${VERSION}
	gzip dwm-${VERSION}.tar
	rm -rf dwm-${VERSION}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f dwm ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/dwm
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < dwm.1 > ${DESTDIR}${MANPREFIX}/man1/dwm.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/dwm.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/dwm\
		${DESTDIR}${MANPREFIX}/man1/dwm.1

indent:
	indent --blank-lines-after-procedures --brace-indent0 --indent-level4 \
		--no-space-after-casts --no-space-after-function-call-names \
		--dont-break-procedure-type --format-all-comments \
		--line-length100 --comment-line-length100 --tab-size4 *.c *.h

check-indentation:
	status=0; for file in *.c *.h; do \
		indent -st -bap -bli0 -i4 -ncs -npcs -npsl -fca -l100 -lc100 -ts4 "$$file" \
			| diff - "$$file" || status=1; \
	done; exit $$status

check:
	sh tests/run.sh

.PHONY: all options clean dist install uninstall indent check-indentation check
