# Makefile for ischeduler scheduler plugin

AUTOMAKE_OPTIONS = foreign

AM_CXXFLAGS = -fexceptions

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src/common

pkglib_LTLIBRARIES = sched_ischeduler.la

sched_ischeduler_la_SOURCES = \
			ischeduler_wrapper.c	\
			ischeduler.h
sched_ischeduler_la_LDFLAGS = $(SO_LDFLAGS) $(PLUGIN_FLAGS)
