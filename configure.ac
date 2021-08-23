AC_PREREQ(2.65)

# gnome-directory-thumbnailer release version
m4_define(gdt_version_major, 0)
m4_define(gdt_version_minor, 1)
m4_define(gdt_version_micro, 12)

AC_INIT([gnome-directory-thumbnailer],[gdt_version_major.gdt_version_minor.gdt_version_micro],[https://gitlab.gnome.org/GNOME/gnome-directory-thumbnailer/-/issues/],[gnome-directory-thumbnailer],[https://wiki.gnome.org/Projects/GnomeDirectoryThumbnailer])
AX_IS_RELEASE([git-directory])

AX_CHECK_ENABLE_DEBUG([yes])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])
AC_USE_SYSTEM_EXTENSIONS

AM_INIT_AUTOMAKE([1.11 dist-xz no-dist-gzip check-news subdir-objects])
AM_SILENT_RULES([yes])

AC_PROG_CXX
AM_PROG_CC_C_O
LT_INIT([])
PKG_PROG_PKG_CONFIG

# Requirements
GLIB_REQS=2.35.0
GIO_REQS=2.22.0
GDK_PIXBUF_REQS=2.6.0
GNOME_DESKTOP_REQS=2.2.0
GTK_REQS=3.0

# gnome-directory-thumbnailer versioning
GDT_VERSION_MAJOR=gdt_version_major
GDT_VERSION_MINOR=gdt_version_minor
GDT_VERSION_MICRO=gdt_version_micro
AC_SUBST([GDT_VERSION_MAJOR])
AC_SUBST([GDT_VERSION_MINOR])
AC_SUBST([GDT_VERSION_MICRO])

# gnome-directory-thumbnailer dependencies
PKG_CHECK_MODULES(GDT, [gobject-2.0 glib-2.0 >= $GLIB_REQS gio-2.0 >= $GIO_REQS gdk-pixbuf-2.0 >= $GDK_PIXBUF_REQS gtk+-3.0 >= $GTK_REQS gnome-desktop-3.0 >= $GNOME_DESKTOP_REQS])

# Optional higher gdk-pixbuf dependency
PKG_CHECK_MODULES([GDK_PIXBUF], [gdk-pixbuf-2.0 >= 2.36.5],
                  [have_gdk_pixbuf_2_36_5=yes], [have_gdk_pixbuf_2_36_5=no])
AS_IF([test "$have_gdk_pixbuf_2_36_5" = "yes"], [
	AC_DEFINE([HAVE_GDK_PIXBUF_2_36_5], [1],
	          [Define if gdk-pixbuf >= 2.36.5 is available])
])

AC_CHECK_LIBM
AC_SUBST([LIBM])

# Internationalisation
GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],"$GETTEXT_PACKAGE",[The name of the gettext domain])
IT_PROG_INTLTOOL([0.40.0])

# General macros
AX_COMPILER_FLAGS([WARN_CFLAGS],[WARN_LDFLAGS])
AX_GENERATE_CHANGELOG

AC_CONFIG_FILES([
Makefile
po/Makefile.in
])
AC_OUTPUT
