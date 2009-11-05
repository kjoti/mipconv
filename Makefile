#
SHELL	= /bin/sh
DEBUG	= -g # -DTEST_MAIN2


CFLAGS	= $(DEBUG) -I/opt/include -I/opt/include/cdTime \
	  -Wall -pedantic -O3


LDFLAGS = $(DEBUG) -L/opt/lib -Wl,'-rpath=/opt/lib'
LIBS	= -lcmor -lnetcdf -lhdf5_hl -lhdf5 -ludunits2 -luuid \
	  -lsz -lz -lgtool3 -lm


OBJS	= main.o \
	axis.o \
	converter.o \
	cmor_supp.o \
	logging.o \
	setup.o \
	utils.o \
	var.o

TARGET	= cmip5conv

$(TARGET): $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $(OBJS) $(LIBS)

clean:
	@rm -f $(TARGET) $(OBJS)
