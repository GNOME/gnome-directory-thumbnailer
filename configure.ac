AC_PREREQ(2.65)

# gnome-directory-thumbnailer release version
m4_define(gdt_version_major, 0)
m4_define(gdt_version_minor, 1)
m4_define(gdt_version_micro, 0)

AC_INIT([gnome-directory-thumbnailer],[gdt_version_major.gdt_version_minor.gdt_version_micro],[https://bugzilla.gnome.org/enter_bug.cgi?product=gnome-directory-thumbnailer],[gnome-directory-thumbnailer],[https://wiki.gnome.org/GnomeDirectoryThumbnailer])

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

# gnome-directory-thumbnailer versioning
GDT_VERSION_MAJOR=gdt_version_major
GDT_VERSION_MINOR=gdt_version_minor
GDT_VERSION_MICRO=gdt_version_micro
AC_SUBST([GDT_VERSION_MAJOR])
AC_SUBST([GDT_VERSION_MINOR])
AC_SUBST([GDT_VERSION_MICRO])

# gnome-directory-thumbnailer dependencies
PKG_CHECK_MODULES(GDT, [gobject-2.0 glib-2.0 >= $GLIB_REQS gio-2.0 >= $GIO_REQS gdk-pixbuf-2.0 >= $GDK_PIXBUF_REQS gnome-desktop-3.0 >= $GNOME_DESKTOP_REQS])

# Internationalisation
GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],"$GETTEXT_PACKAGE",[The name of the gettext domain])
IT_PROG_INTLTOOL([0.40.0])

# General GNOME macros
GNOME_DEBUG_CHECK
GNOME_COMPILE_WARNINGS([maximum])
GNOME_MAINTAINER_MODE_DEFINES

AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_CXXFLAGS])
AC_SUBST([AM_LDFLAGS])

AC_CONFIG_FILES([
Makefile
po/Makefile.in
])
AC_OUTPUT
