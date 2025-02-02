CC = gcc
CFLAGS = -g -pipe -Wall -Werror

all: writer

clean:
	$(RM) writer

%: %.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -o $@ $<
