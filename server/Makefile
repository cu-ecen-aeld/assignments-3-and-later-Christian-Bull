CC      ?= $(CROSS_COMPILE)gcc
CFLAGS  ?= -g -Wall -Werror
LDFLAGS ?= -lpthread -lrt
RM      = rm -f
LOG_FILE = /var/tmp/aesdsocketdata

default: all

all: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) -o aesdsocket $(INCLUDES) $(LDFLAGS) aesdsocket.c

driver: aesdsocket.c
	$(CC) $(CFLAGS) -DUSE_AESD_CHAR_DEVICE -o aesdsocket $(INCLUDES) $(LDFLAGS) aesdsocket.c

clean veryclean:
	$(RM) aesdsocket
	$(RM) $(LOG_FILE)
