CCOPTS= -Wall -g -std=gnu99 -Wstrict-prototypes
LIBS= 
CC=gcc
AR=ar


BINS= simplefs_test simplefs_shell test

#add here your object files
OBJS = disk_driver.o\
	utils.o\
	simplefs.o

HEADERS = disk_driver.h\
	simplefs.h\
	utils.h

%.o:	%.c $(HEADERS)
	$(CC) $(CCOPTS) -c -o $@  $<

.phony: clean all


all:	$(BINS) 

so_game: simplefs_test.c $(OBJS) 
	$(CC) $(CCOPTS)  -o $@ $^ $(LIBS)

test:	simplefs_test.c $(OBJS) 
	$(CC) $(CCOPTS)  -o $@ $^ $(LIBS) -lm
	rm test.fs

shell:	simplefs_shell.c  $(OBJS) 
	$(CC) $(CCOPTS)  -o $@ $^ $(LIBS) -lm

clean:
	rm -rf *.o *~  $(BINS)
