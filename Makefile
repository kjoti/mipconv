#
SHELL	= /bin/sh

DEBUG	= -g -DTEST_MAIN2
prefix	= /opt

## gcc
CC	= gcc
CFLAGS	= $(DEBUG) -Wall -pedantic \
	-I$(prefix)/include \
	-I$(prefix)/include/cdTime

LDFLAGS = $(DEBUG) -L$(prefix)/lib -Wl,'-rpath=$(prefix)/lib'

OBJS	= main.o \
	axis.o \
	cmor_supp.o \
	calculator.o \
	converter.o \
	get_ints.o \
	logging.o \
	logicline.o \
	seq.o \
	setup.o \
	split.o \
	startswith.o \
	timeaxis.o \
	utils.o \
	unit.o \
	var.o \
	version.o \
	zfactor.o

LIBS	= -lcmor -lnetcdf -lhdf5_hl -lhdf5 -ludunits2 -luuid \
	  -lsz -lz -lgtool3 -lm

SRCS	= $(OBJS:%.o=%.c)

TARGET	= mipconv

$(TARGET): $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $(OBJS) $(LIBS)

tags:
	etags $(SRCS)

clean:
	@rm -f $(TARGET) $(OBJS) cmor.log*
