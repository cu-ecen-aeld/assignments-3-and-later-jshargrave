# Author: Joseph Hargrave
# Date: 2/15/2026

CC ?= gcc
CFLAGS ?= -Wall -Werror
EXTRA_CFLAGS ?=
LDFLAGS ?=
EXTRA_LDFLAGS ?=

TARGET := aesdsocket
OBJS := aesdsocket.o

all: aesdsocket

aesdsocket: aesdsocket.o
	@echo 'Building aesdsocket!'
	@$(CC) -o aesdsocket aesdsocket.o $(LDFLAGS) ${EXTRA_LDFLAGS}

aesdsocket.o : aesdsocket.c aesdsocket.h
	@$(CC) -c aesdsocket.c $(CFLAGS) ${EXTRA_CFLAGS}

clean : 
	@echo 'Cleaning!'
	@rm -f aesdsocket *.o


