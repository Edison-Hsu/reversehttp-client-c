# Makefile 

# ---------------------------------------------------
# targets
TAR = reversehttp-client

SRC = $(wildcard *.c)
LIBS = -lcurl

# ---------------------------------------------------
# flags
CROSS = sh4-linux-

CC = $(CROSS)gcc

CFLAGS += -I./include 
CFLAGS += -DST_OSLINUX -D_GNU_SOURCE
CFLAGS += -Wall
CFLAGS += -g -O0 
CFLAGS += -L./lib

# ---------------------------------------------------
# builds

all: ${SRC}
	rm -f ${TAR}
	${CC} -o ${TAR} ${SRC} ${LIBS} ${CFLAGS} -D__DEBUG__

release: 
	rm -f ${TAR}
	${CC} -o ${TAR} ${SRC} ${LIBS} ${CFLAGS} -D__RELEASE__

gcc:
	rm -f ${TAR}	
	gcc -o ${TAR} ${SRC} ${LIBS} ${CFLAGS} -D__DEBUG__

install:
	@echo "already installed"

.PHONY: clean
clean:
	-rm -Rf *.o ${TAR}
