# Author: Joseph Hargrave
# Date: 2/15/2026

CC ?= gcc
CFLAGS ?= -Wall -Werror
LDFLAGS ?=

TARGET := aesdsocket
OBJS := aesdsocket.o

all: aesdsocket

aesdsocket: aesdsocket.o
	@echo 'Building aesdsocket!'
	@$(CC) -o aesdsocket aesdsocket.o $(LDFLAGS)

aesdsocket.o : aesdsocket.c aesdsocket.h
	@$(CC) -c aesdsocket.c $(FLAGS)

clean : 
	@echo 'Cleaning!'
	@rm -f aesdsocket *.o


