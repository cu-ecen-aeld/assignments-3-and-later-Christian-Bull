CC      = gcc
CFLAGS  = -g
RM      = rm -f

default: all

all: writer

writer: writer.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -o writer writer.c

clean veryclean:
	$(RM) writer