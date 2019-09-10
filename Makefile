BIN = longmynd
SRC = main.c nim.c ftdi.c stv0910.c stv0910_utils.c stvvglna.c stvvglna_utils.c stv6120.c stv6120_utils.c ftdi_usb.c fifo.c udp.c
OBJ = ${SRC:.c=.o}

CC = gcc
CFLAGS += -Wall -Wextra -Wunused -DVERSION=\"${VER}\"
LDFLAGS += -lusb-1.0

all: ${BIN} fake_read

debug: CFLAGS += -ggdb -fno-omit-frame-pointer
debug: all

fake_read:
	@${CC} fake_read.c -o $@

$(BIN): ${OBJ}
	@echo "  LD     "$@
	@${CC} ${CFLAGS} -o $@ ${OBJ} ${LDFLAGS}

%.o: %.c
	@echo "  CC     "$<
	@${CC} ${CFLAGS} -c -fPIC -o $@ $<

clean:
	@rm -rf ${BIN} fake_read ${OBJ}

tags:
	@ctags *

.PHONY: all clean

