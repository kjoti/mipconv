#
SHELL	= /bin/sh

## CMOR directory
cmordir = ../cmor

## netCDF, HDF5, udunits, and so on ...
PREFIX	= /usr/local

## strlcpy/strlcat
#CPPFLAGS += -DHAVE_STRLCPY

## test code
#CPPFLAGS += -DTEST_MAIN2

## GCC
CC	= gcc
CFLAGS	= -std=c99 -Wall -pedantic -O2 \
	-I$(cmordir)/include \
	-I$(cmordir)/include/json-c \
	-I$(cmordir)/include/cdTime \
	-I$(PREFIX)/include

LDFLAGS = -L$(PREFIX)/lib -Wl,'-rpath=$(PREFIX)/lib'

## -g option
#CFLAGS += -g
#LDFLAGS += -g

PROGRAMS = mipconv mipconv_test

OBJS	= \
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

LIBS	= $(cmordir)/libcmor.a \
	-lnetcdf -lhdf5_hl -lhdf5 \
	-ludunits2 -luuid \
	-lgtool3 -lz -lm

SRCS	= $(OBJS:%.o=%.c)

mipconv: main.o $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $^ $(LIBS)

mipconv_test: main_test.o $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $^ $(LIBS)

tags: $(SRCS)
	etags $(SRCS)

clean:
	@rm -f $(PROGRAMS) $(OBJS) *.o

distclean: clean
	@rm -f TAGS *.log
