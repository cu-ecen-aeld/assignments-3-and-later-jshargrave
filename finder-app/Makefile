# Author: Joseph Hargrave
# Date: 1/17/2026
# Requirements:
	# Write a Makefile which includes:

	# A default target which builds the “writer” application

	# A clean target which removes the “writer” application and all .o files

	# Support for cross-compilation.  You should be able to generate an application for the native build platform when GNU make variable CROSS_COMPILE is not specified on the make command line.  When CROSS_COMPILE is specified with aarch64-none-linux-gnu- (note the trailing -)your makefile should compile successfully using the cross compiler installed in step 1.

CC=gcc

all: writer

writer: writer.o
	@echo 'Building writer!'
	@$(CROSS_COMPILE)$(CC) -o writer writer.o

writer.o : writer.c
	@$(CROSS_COMPILE)$(CC) -c writer.c

clean : 
	@echo 'Cleaning!'
	@rm -f writer *.o


