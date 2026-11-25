#
# mipconv -- A CMOR client for MIROC outputs
#
# https://github.com/kjoti/mipconv
#
# Required libraries:
#   * CMOR (for miroc)   https://github.com/kjoti/cmor_for_miroc
#   * netCDF             https://www.unidata.ucar.edu/software/netcdf/
#   * HDF5               https://www.hdfgroup.org/
#   * udunits2           https://www.unidata.ucar.edu/software/udunits
#   * libuuid            https://sourceforge.net/projects/libuuid/files/
#   * json-c             https://github.com/json-c/json-c
#   * libgtool3          https://github.com/kjoti/libgtool3
#
# Configurable variables:
#   * CMOR_PREFIX: A directory where CMOR is installed
#   * PREFIX: A directory where the other libraries are installed
#
SHELL	= /bin/sh

## CMOR
CMOR_PREFIX = $(HOME)/.local
CFLAGS	= -I$(CMOR_PREFIX)/include -I$(CMOR_PREFIX)/include/cdTime
LDFLAGS	= -L$(CMOR_PREFIX)/lib
LIBS	= -lcmor

## netCDF, HDF5, udunits2, uuid, json-c, ..., libgtool3
PREFIX	= $(HOME)/.local
NC_CONFIG = $(PREFIX)/bin/nc-config
CC	= `$(NC_CONFIG) --cc`
CFLAGS	+= `$(NC_CONFIG) --cflags`
LIBS	+= `$(NC_CONFIG) --libs`
LDFLAGS	+= -Wl,-rpath=$(PREFIX)/lib

## and some libraries...
LIBS	+= -ludunits2 -luuid -ljson-c
LIBS	+= -lgtool3 -lm

#
CFLAGS	+= -O2 -Wall -pedantic
CPPFLAGS =

## strlcpy/strlcat
#CPPFLAGS += -DHAVE_STRLCPY

## strcasecmp
#CPPFLAGS += -DHAVE_STRCASECMP

## test code
#CPPFLAGS += -DTEST_MAIN2

PROGRAM	= mipconv
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
	fskim.o \
	get_ints.o \
	iarray.o \
	logging.o \
	logicline.o \
	rotated_pole.o \
	sdb.o \
	seq.o \
	setup.o \
	site.o \
	split.o \
	split2.o \
	startswith.o \
	strcase.o \
	strcasecmp.o \
	strlcpy.o \
	tables.o \
	timeaxis.o \
	tripolar.o \
	unit.o \
	var.o \
	version.o \
	zfactor.o

SRCS	= $(OBJS:%.o=%.c)

all: $(PROGRAM)

$(PROGRAM): main.o $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $^ $(LIBS)

mipconv_test: main_test.o $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $^ $(LIBS)

tags: $(SRCS)
	etags $(SRCS)

clean:
	@rm -f $(PROGRAMS) $(OBJS) *.o

distclean: clean
	@rm -f TAGS *.log
