#
# File:
#    Makefile
#
# Description:
#
# $Date$
# $Rev$
#

CROSS_COMPILE		=
CC			= $(CROSS_COMPILE)g++
AR                      = ar
RANLIB                  = ranlib
CFLAGS			= -O2 -fno-exceptions -fPIC -I/usr/include\
			-I.
LINKLIBS		= -lrt
PROGS			= petiroc_test
HEADERS			= $(wildcard *.h)
SRC			= ./petiroc_test.cc ./fpga_io.cc
OBJS			= $(SRC:.C=.o)

all: $(PROGS) $(HEADERS)

clean distclean:
	@rm -f $(PROGS) *~ *.o outlinkDef.{C,h}

%.o:	%.C Makefile
	@echo "Building $@"
	$(CC) $(CFLAGS) \
	-c $<

$(PROGS): $(OBJS) $(SRC) $(HEADERS) Makefile
	@echo "Building $@"
	$(CC) $(CFLAGS) -o $@ \
	$(LINKLIBS) \
	$(OBJS)


.PHONY: all clean distclean
