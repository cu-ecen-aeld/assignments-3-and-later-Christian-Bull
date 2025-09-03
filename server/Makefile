CC      = gcc
CFLAGS  = -g
RM      = rm -f

default: all

all: aesdsocket

aesdsocket: aesdsocket.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -o aesdsocket aesdsocket.c

clean veryclean:
	$(RM) aesdsocket