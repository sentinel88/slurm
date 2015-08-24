#
# Makefile for slurmirmd

AUTOMAKE_OPTIONS = foreign
CLEANFILES = core.*

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src/common 
AM_CXXFLAGS = -fexceptions

slurmirmd_SOURCES =     \
	controller.c  	\
        rsrc_offer.c    \
	feedback_agent.c \
	protocol_testing.c \
	slurmirmd.h

sbin_PROGRAMS = slurmirmd

slurmirmd_LDADD = 				    \
	$(top_builddir)/src/api/libslurm.o $(DL_LIBS)
slurmirmd_LDFLAGS = -export-dynamic $(CMD_LDFLAGS)

force:
$(slurmctld_LDADD) : force
	@cd `dirname $@` && $(MAKE) `basename $@`
