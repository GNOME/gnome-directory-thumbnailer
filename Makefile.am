ACLOCAL_AMFLAGS = --install -I m4 ${ACLOCAL_FLAGS}
SUBDIRS = . po

DISTCHECK_CONFIGURE_FLAGS =
CLEANFILES =
MAINTAINERCLEANFILES =
EXTRA_DIST =
GITIGNOREFILES =
bin_PROGRAMS =

# gnome-directory-thumbnailer application
bin_PROGRAMS += src/gnome-directory-thumbnailer

src_gnome_directory_thumbnailer_SOURCES = \
	src/main.c \
	$(NULL)

src_gnome_directory_thumbnailer_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-DG_LOG_DOMAIN=\"gdt\" \
	-DLOCALE_DIR=\"$(localedir)\" \
	$(DISABLE_DEPRECATED) \
	$(AM_CPPFLAGS) \
	$(NULL)

src_gnome_directory_thumbnailer_CFLAGS = \
	$(GDT_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

src_gnome_directory_thumbnailer_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

src_gnome_directory_thumbnailer_LDADD = \
	$(GDT_LIBS) \
	$(LIBM) \
	$(CODE_COVERAGE_LIBS) \
	$(AM_LIBADD) \
	$(NULL)

# Thumbnailer file
thumbnailersdir = $(datadir)/thumbnailers
thumbnailers_DATA = src/gnome-directory-thumbnailer.thumbnailer
EXTRA_DIST += $(thumbnailers_DATA)

# Cleaning
EXTRA_DIST += \
	autogen.sh \
	HACKING \
	gnome-directory-thumbnailer.doap \
	$(NULL)

CLEANFILES += \
	`find "$(srcdir)" -type f -name .dirstamp -print` \
	$(NULL)

MAINTAINERCLEANFILES += \
	$(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
	$(GITIGNORE_MAINTAINERCLEANFILES_MAKEFILE_IN) \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_LIBTOOL) \
	build-aux/ \
	ABOUT-NLS \
	INSTALL \
	omf.make \
	config.rpath \
	`find "m4" -type f -name "*.m4" -print` \
	$(NULL)

# ChangeLog
@GENERATE_CHANGELOG_RULES@
dist-hook: dist-ChangeLog

-include $(top_srcdir)/git.mk
