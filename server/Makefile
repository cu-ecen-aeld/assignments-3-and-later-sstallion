CC = gcc
CFLAGS = -std=gnu11 -g -pipe -Wall -Werror
CPPFLAGS = -D_GNU_SOURCE

BIN = aesdsocket

.PHONY: all
all: $(BIN)

.PHONY: clean
clean:
	$(RM) $(BIN)

.PHONY: watch
watch:
	@inotifywait -m -r -e close_write --format %f $(CURDIR) | \
	  while read file; do \
	    case "$$file" in \
	    Makefile|*.[ch]) \
	      $(MAKE) all \
	      ;; \
	    esac \
	  done

%: %.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $<
