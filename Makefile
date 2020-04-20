TARGETS=yapu

ifdef D
	CPERF=-g
else
	CPERF=-O3
endif

CC=gcc

SRCDIR=src
INCLUDES=include
OBJDIR=obj

CFLAGS=-Wall -Werror $(CPERF) -I $(INCLUDES)
LDFLAGS=-lpthread


## Dependencies

all: $(TARGETS)

yapu: $(OBJDIR)/main.o $(OBJDIR)/ping.o $(OBJDIR)/cbuf.o

$(OBJDIR)/main.o: $(INCLUDES)/defs.h $(INCLUDES)/ping.h

$(OBJDIR)/ping.o: $(INCLUDES)/defs.h $(INCLUDES)/cbuf.h $(INCLUDES)/ping.h

$(OBJDIR)/cbuf.o: $(INCLUDES)/cbuf.h

## Compilation

$(TARGETS):
	$(CC) $^ $(LDFLAGS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $@


## Miscellaneous
# Universal Ctags
.PHONY: tags
tags:
	ctags -R --languages=C *

clean:
	rm -rf $(OBJDIR) $(TARGETS)

