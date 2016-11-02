PROGS      = autoexposure temperature

SRCS = autoexposure.c globalsinit.c aexp_utils.c aexp_corr.c white_balance.c hdr_control.c temperature.c
OBJS = autoexposure.o globalsinit.o aexp_utils.o aexp_corr.o white_balance.o hdr_control.o temperature.o

CFLAGS   += -Wall -I$(STAGING_DIR_HOST)/usr/include-uapi 

INSTALL    = install
INSTMODE   = 0755
#INSTDOCS   = 0644
OWN = -o root -g root
BINDIR     = /usr/bin


all: $(PROGS)

autoexposure: autoexposure.o globalsinit.o aexp_utils.o aexp_corr.o white_balance.o hdr_control.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

temperature: temperature.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
	
install: $(PROGS)
	$(INSTALL) $(OWN) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m $(INSTMODE) $(OWN) $(PROGS) $(DESTDIR)$(BINDIR)
clean:
	rm -rf $(PROGS) *.o core  .depend

depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	$(CC) $(CFLAGS) -MM $^ > ./.depend;

include .depend
