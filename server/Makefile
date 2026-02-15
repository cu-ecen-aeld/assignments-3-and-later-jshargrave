# Author: Joseph Hargrave
# Date: 2/15/2026

CC=gcc

all: aesdsocket

aesdsocket: aesdsocket.o
	@echo 'Building aesdsocket!'
	@$(CROSS_COMPILE)$(CC) -o aesdsocket aesdsocket.o

aesdsocket.o : aesdsocket.c
	@$(CROSS_COMPILE)$(CC) -c aesdsocket.c

clean : 
	@echo 'Cleaning!'
	@rm -f aesdsocket *.o


