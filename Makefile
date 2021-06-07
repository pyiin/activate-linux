
SRC = activate_linux.c
CC = cc
INC = /usr/include/freetype2
LDFLAGS = -lX11 -lX11-xcb -lxcb -lxcb-res -lXrender -lfontconfig -lXft

activate_linux:
	${CC} -o $@ ${SRC} -I${INC} ${LDFLAGS}

run: activate_linux
	./activate_linux

install: activate_linux
	mv -f activate_linux ~/.local/bin/
	killall activate_linux
	activate_linux & disown

.PHONY: run activate_linux
