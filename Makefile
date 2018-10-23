#
SHELL	= /bin/sh

## CMOR directory
cmordir = ../cmor

## netCDF, HDF5, udunits, and so on ...
PREFIX	= /usr/local

##
DEBUG	=
DEFS	=

## strcpy/strcat
#DEFS	+= -DHAVE_STRLCPY

#DEBUG	= -g -DTEST_MAIN2

## gcc
CC	= gcc
CFLAGS	= -std=c99 $(DEBUG) -Wall -pedantic -O2 \
	$(DEFS) \
	-I$(cmordir)/include \
	-I$(cmordir)/include/json-c \
	-I$(cmordir)/include/cdTime \
	-I$(PREFIX)/include

LDFLAGS = $(DEBUG) -L$(PREFIX)/lib -Wl,'-rpath=$(PREFIX)/lib'

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

LIBS	= $(cmordir)/libcmor.a \
	-lnetcdf -lhdf5_hl -lhdf5 \
	-ludunits2 -luuid \
	-lgtool3 -lz -lm

SRCS	= $(OBJS:%.o=%.c)

TARGET	= mipconv

$(TARGET): $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $(OBJS) $(LIBS)

tags:
	etags $(SRCS)

clean:
	@rm -f $(TARGET) $(OBJS) cmor.log*
