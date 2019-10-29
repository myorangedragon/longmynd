BIN = longmynd
SRC = main.c nim.c ftdi.c stv0910.c stv0910_utils.c stvvglna.c stvvglna_utils.c stv6120.c stv6120_utils.c ftdi_usb.c fifo.c udp.c beep.c ts.c
OBJ = ${SRC:.c=.o}

ifndef CC
CC = gcc
endif
COPT = -O3 -march=native -mtune=native
CFLAGS += -Wall -Wextra -Wpedantic -Wunused -DVERSION=\"${VER}\" -pthread -D_GNU_SOURCE
LDFLAGS += -lusb-1.0 -lm -lasound

all: ${BIN} fake_read

debug: COPT = -Og
debug: CFLAGS += -ggdb -fno-omit-frame-pointer
debug: all

werror: CFLAGS += -Werror
werror: all

fake_read:
	@echo "  CC     "$@
	@${CC} fake_read.c -o $@

$(BIN): ${OBJ}
	@echo "  LD     "$@
	@${CC} ${COPT} ${CFLAGS} -o $@ ${OBJ} ${LDFLAGS}

%.o: %.c
	@echo "  CC     "$<
	@${CC} ${COPT} ${CFLAGS} -c -fPIC -o $@ $<

clean:
	@rm -rf ${BIN} fake_read ${OBJ}

tags:
	@ctags *

.PHONY: all clean

