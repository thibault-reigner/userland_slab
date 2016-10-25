C=gcc
CFLAGS=-Wall -std=gnu11 -O0 
OFLAG=-O0 -flto
VPATH=src
OBJDIR=build
BINDIR=bin

all: directories build cmdapp

build: $(OBJDIR)/main.o  $(OBJDIR)/slab.o \

cmdapp: $(BINDIR)/usr_slab

$(BINDIR)/usr_slab: $(OBJDIR)/main.o  $(OBJDIR)/slab.o 
	$(C) -o $@ $(OFLAG) $(CFLAGS) $^

directories:
	mkdir -p $(OBJDIR)
	mkdir -p $(BINDIR)

clean:
	rm -rf $(OBJDIR)/*
	rm -f $(BINDIR)/*


$(OBJDIR)/slab.o: slab.c slab.h
	$(C) -c $(CFLAGS) $(OFLAG) $< -o $@

$(OBJDIR)/main.o: main.c 
	$(C) -c $(CFLAGS) $(OFLAG) $< -o $@

