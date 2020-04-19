TARGETS=yapu

ifdef D
	CPERF=-g
else
	CPERF=-O2
endif

CC=gcc

SRCDIR=src
INCLUDES=include
OBJDIR=obj

CFLAGS=-Wall -Werror $(CPERF) -I $(INCLUDES)


## Dependencies

all: $(TARGETS)

yapu: $(OBJDIR)/main.o

$(OBJDIR)/main.o: $(INCLUDES)/defs.h


## Compilation

$(TARGETS):
	$(CC) $^ -o $@

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

