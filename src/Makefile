AXIS_USABLE_LIBS = UCLIBC GLIBC
AXIS_AUTO_DEPEND = yes
include $(AXIS_TOP_DIR)/tools/build/Rules.axis
#include ".depend"
INSTDIR    = $(prefix)/usr/local/sbin/
CONFDIR   = $(prefix)/etc/
INSTMODE   = 0755
INSTOTHER  = 0644
INSTOWNER  = root
INSTGROUP  = root

INCDIR     = $(prefix)/include

PROGS      = autoexposure temperature

SRCS = autoexposure.c globalsinit.c aexp_utils.c aexp_corr.c white_balance.c hdr_control.c temperature.c

OBJS = autoexposure.o globalsinit.o aexp_utils.o aexp_corr.o white_balance.o hdr_control.o temperature.o

CFLAGS   += -Wall -I$(INCDIR) -I$(AXIS_KERNEL_DIR)/include 
#CFLAGS   += -Wall -I$(INCDIR) -I$(AXIS_KERNEL_DIR)/include -save-temps -dA -dP

all: $(PROGS)

autoexposure: autoexposure.o globalsinit.o aexp_utils.o aexp_corr.o white_balance.o hdr_control.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
	cris-strip -s $@
temperature: temperature.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
	cris-strip -s $@
install: $(PROGS)
	$(INSTALL) -d $(INSTDIR)
	$(INSTALL) -m $(INSTMODE) -o $(INSTOWNER) -g $(INSTGROUP) $(PROGS) $(INSTDIR)
clean:
	rm -rf $(PROGS) *.o core
configsubs:
