#######################################
# The list of libraries we are building seperated by spaces.
# The 'lib_' indicates that these build products will be installed
# in the $(libdir) directory. For example /usr/lib
lib_LTLIBRARIES = launchapps.la

#######################################
# Build information for each library

# Sources for launchapps
launchapps_la_SOURCES = launchapps.c
# noinst_HEADERS = i18n.h

# localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

# Linker options libTestProgram
launchapps_la_LDFLAGS = -module -avoid-version -shared $(LTLIBINTL)

# Compiler options. Here we are adding the include directory
# to be searched for headers included in the source code.
launchapps_la_CPPFLAGS = -I$(top_srcdir)/include $(GTK2_CFLAGS)

install-exec-hook:
		cd $(DESTDIR)$(libdir) && $(RM) $(lib_LTLIBRARIES) \
		&& chmod -x launchapps.so && sudo lxpanelctl restart
