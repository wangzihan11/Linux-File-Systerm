CC      = gcc 
CFLAGS	=  -Wall -D_FILE_OFFSET_BITS=64 -g
TARGET1 = ufs
TARGET2 = init
OBJS1	= ufs.o op.o
OBJS2   = init.o
INC	=  -pthread -lfuse -lrt -ldl

all:init ufs
init:$(OBJS2)
	$(CC)   $(OBJS2) -o $(TARGET2)
ufs:$(OBJS1)
	$(CC)   $(CFLAGS) $(OBJS1) -o $(TARGET1) $(INC)
ufs.o:ufs.h 
op.o:op.h
init.o:ufs.h

.PHONY : all
clean :
	rm  -f $(TARGET1) $(TARGET2) $(OBJS1) $(OBJS2)









