#
SHELL	= /bin/sh
DEBUG	= -g #-DTEST_MAIN2

CFLAGS	= $(DEBUG) -I/opt/include -I/opt/include/cdTime \
	  -Wall -pedantic

LDFLAGS = $(DEBUG) -L/opt/lib -Wl,'-rpath=/opt/lib'
LIBS	= -lcmor -lnetcdf -lhdf5_hl -lhdf5 -ludunits2 -luuid \
	  -lsz -lz -lgtool3 -lm

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
	timeaxis.o \
	utils.o \
	unit.o \
	var.o \
	version.o \
	zfactor.o

SRCS	= $(OBJS:%.o=%.c)

TARGET	= mipconv

$(TARGET): $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $(OBJS) $(LIBS)

tags:
	etags $(SRCS)

clean:
	@rm -f $(TARGET) $(OBJS) cmor.log*
