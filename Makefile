#
SHELL	= /bin/sh

DEBUG	= -g -DTEST_MAIN2
prefix	= /opt

DEFS	= -DHAVE_STRLCPY

## gcc
CC	= gcc
CFLAGS	= -std=c99 $(DEBUG) -Wall -pedantic -O2 \
	$(DEFS) \
	-I$(prefix)/include \
	-I$(prefix)/include/json-c \
	-I$(prefix)/include/cdTime

LDFLAGS = $(DEBUG) -L$(prefix)/lib -Wl,'-rpath=$(prefix)/lib'

OBJS	= main.o \
	axis.o \
	bipolar.o \
	calculator.o \
	cmor_supp.o \
	converter.o \
	coord.o \
	date.o \
	editheader.o \
	fileiter.o \
	get_ints.o \
	iarray.o \
	logging.o \
	logicline.o \
	rotated_pole.o \
	sdb.o \
	seq.o \
	setup.o \
	split.o \
	split2.o \
	startswith.o \
	strcase.o \
	strlcpy.o \
	tables.o \
	timeaxis.o \
	tripolar.o \
	unit.o \
	var.o \
	version.o \
	zfactor.o

LIBS	= -lcmor -lnetcdf -lhdf5_hl -lhdf5 -ludunits2 -luuid \
	  -lz -lgtool3 -lm

SRCS	= $(OBJS:%.o=%.c)

TARGET	= mipconv

$(TARGET): $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $(OBJS) $(LIBS)

tags:
	etags $(SRCS)

clean:
	@rm -f $(TARGET) $(OBJS) cmor.log*
