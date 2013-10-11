/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * gnome-directory-thumbnailer
 * Copyright (C) 2013 Collabora Ltd.
 *
 * gnome-directory-thumbnailer is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * gnome-directory-thumbnailer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gnome-directory-thumbnailer.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  • Philip Withnall <philip.withnall@collabora.co.uk>
 */

#include "config.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <locale.h>
#include <math.h>

/* GnomeDesktopThumbnail is unstable. */
#define GNOME_DESKTOP_USE_UNSTABLE_API 1
#include <libgnome-desktop/gnome-desktop-thumbnail.h>

/**
 * gnome-directory-thumbnailer:
 *
 * Given a directory to thumbnail, the basic approach taken by the program is to enumerate all of the directory’s children (files, directories, symlinks, etc.)
 * and rank them according to their ‘interestingness’ score, which indicates how good each child is likely to be as a thumbnail representing the entire
 * directory. The thumbnail for the most interesting child is then generated or looked up and used as the thumbnail for the directory.
 *
 * If thumbnailing the most interesting child fails, there is no fallback, and the directory will end up with no thumbnail.
 *
 * Note that child symlinks to other directories are always ignored to eliminate the possibility of entering an endless loop of directory symlinks which would
 * result in a fork bomb.
 *
 * Feel free to modify the heuristics in calculate_file_interestingness() to improve the thumbnails for directories. There are many possibilities for
 * improvement, such as identifying common directory structures and choosing a well-known file within them to represent the directory. (For example, a directory
 * structure which looks like a Sugar Activity could be represented by its Activity icon.)
 */


/* Command line options. */
static gint output_size = -1; /* pixels */
static gchar **filenames = NULL; /* needs to be freed with g_strfreev() */

#define MAX_FILE_INTERESTINGNESS 21

/**
 * calculate_file_interestingness:
 * @file_info: information about the file
 * @file: pointer to the file
 * @factory: global thumbnail factory
 *
 * Calculate an ‘interestingness’ score for the given @file, in terms of how interesting it would be as a thumbnail to represent the directory containing it.
 * The score is a positive integer, with larger numbers meaning the file is more interesting. The maximum possible score is %MAX_FILE_INTERESTINGNESS.
 *
 * If using new #GFileInfo attributes in this function, don’t forget to update the g_file_enumerate_children() call in pick_interesting_file_for_directory() below.
 * Also don’t forget to update %MAX_FILE_INTERESTINGNESS. It must be calculated manually every time you change this function.
 *
 * Return value: interestingness score for @file
 */
static guint
calculate_file_interestingness (GFileInfo *file_info, GFile *file, GnomeDesktopThumbnailFactory *factory)
{
	guint interestingness = 1;
	GTimeVal file_mtime;
	gchar *file_uri, *file_mime_type;

#define INC(I) interestingness = CLAMP (((gint64) interestingness) + (I), 1, G_MAXUINT)
#define DEC(I) INC(-(I))

	/* Weight subdirectories and special files lower than normal files. Treat symlinks and shortcuts as normal files. */
	switch (g_file_info_get_file_type (file_info)) {
		case G_FILE_TYPE_REGULAR:
		case G_FILE_TYPE_SYMBOLIC_LINK:
		case G_FILE_TYPE_SHORTCUT:
			INC (20);
			break;
		case G_FILE_TYPE_DIRECTORY:
		case G_FILE_TYPE_SPECIAL:
		case G_FILE_TYPE_MOUNTABLE:
			INC (10);
			break;
		case G_FILE_TYPE_UNKNOWN:
		default:
			/* Do nothing. */
			break;
	}

	/* Weight backup and hidden files less. */
	if (g_file_info_get_is_hidden (file_info) == TRUE || g_file_info_get_is_backup (file_info) == TRUE) {
		DEC (5);
	}

	/* Weight un-thumbnailable files a lot less. */
	file_uri = g_file_get_uri (file);
	g_file_info_get_modification_time (file_info, &file_mtime);
	file_mime_type = g_content_type_get_mime_type (g_file_info_get_content_type (file_info));

	if (gnome_desktop_thumbnail_factory_can_thumbnail (factory, file_uri, file_mime_type, file_mtime.tv_sec) == FALSE) {
		DEC (10);
	}

	g_free (file_uri);
	g_free (file_mime_type);

#undef DEC
#undef INC

	g_assert (interestingness > 0);

	return interestingness;
}

/**
 * pick_interesting_file_for_directory:
 * @input_directory: directory to pick a child from
 * @file_info_out: (out) (allow-none) (transfer full): return location for a #GFileInfo for the chosen child, or %NULL
 * @factory: global thumbnail factory
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Pick an interesting file which will represent the directory. This child may be a file, a symlink, a sub-directory, etc. If the @input_directory
 * is empty, %NULL will be returned (and @error will not be set). If @file_info_out is non-%NULL, a #GFileInfo object for the chosen file will be
 * returned, and must be unreffed with g_object_unref().
 *
 * On error, %NULL will be returned and @file_info_out will be set to %NULL. An error will be returned if @input_directory is not a directory or does not exist.
 *
 * Return value: (transfer full) (allow-none): chosen interesting child for the given @input_directory, or %NULL if the directory is empty or on error; unref with g_object_unref()
 */
static GFile *
pick_interesting_file_for_directory (GFile *input_directory, GFileInfo **file_info_out, GnomeDesktopThumbnailFactory *factory, GError **error)
{
	GFileEnumerator *enumerator = NULL;
	GFile *interesting_file = NULL;
	GFileInfo *file_info, *interesting_file_info = NULL;
	guint interesting_file_interestingness = 0;
	GError *child_error = NULL;

	/* Enumerate all the children of the directory and choose the most interesting one. */
	enumerator = g_file_enumerate_children (input_directory,
	                                        G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE ","
	                                        G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," G_FILE_ATTRIBUTE_TIME_MODIFIED ","
	                                        G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP "," G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN ","
	                                        G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
	                                        G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, &child_error);
	if (child_error != NULL) {
		g_assert (enumerator == NULL);
		goto done;
	}

	while ((file_info = g_file_enumerator_next_file (enumerator, NULL, &child_error)) != NULL) {
		guint file_interestingness;
		GFile *file;

		file = g_file_enumerator_get_child (enumerator, file_info);

		/* Completely ignore symbolic links to directories, so that we avoid potentially infinite loops of symlinks. */
		if (g_file_info_get_file_type (file_info) == G_FILE_TYPE_SYMBOLIC_LINK) {
			const gchar *symlink_target;
			GFile *symlink_target_file;

			symlink_target = g_file_info_get_symlink_target (file_info);
			symlink_target_file = g_file_get_child (input_directory, symlink_target);

			g_debug ("Checking target ‘%s’ for symlink ‘%s’.", symlink_target, g_file_info_get_name (file_info));

			if (g_file_query_file_type (symlink_target_file, G_FILE_QUERY_INFO_NONE, NULL) == G_FILE_TYPE_DIRECTORY) {
				g_debug ("Skipping file ‘%s’ as it’s a symlink to a directory, and could cause an infinite loop.", g_file_info_get_name (file_info));
				g_object_unref (symlink_target_file);
				g_object_unref (file);
				continue;
			}

			g_object_unref (symlink_target_file);
		}

		/* Is this file more interesting than the most interesting one we've seen so far? */
		file_interestingness = calculate_file_interestingness (file_info, file, factory);

		g_debug ("Examining file ‘%s’ with interestingness %u", g_file_info_get_name (file_info), file_interestingness);

		if (file_interestingness > interesting_file_interestingness) {
			g_clear_object (&interesting_file_info);
			g_clear_object (&interesting_file);

			interesting_file_info = g_object_ref (file_info);
			interesting_file = g_object_ref (file);
			interesting_file_interestingness = file_interestingness;

			g_debug ("Updating most interesting file to ‘%s’ with interestingness %u.", g_file_get_path (interesting_file), interesting_file_interestingness);

			/* If this is the most fantastic, interesting, amazing file we can possibly encounter, bail. */
			if (interesting_file_interestingness >= MAX_FILE_INTERESTINGNESS) {
				g_debug ("Interestingness reached maximum of %u. Breaking out with most interesting file ‘%s’.", MAX_FILE_INTERESTINGNESS, g_file_get_path (interesting_file));

				g_object_unref (file_info);
				g_object_unref (file_info);

				break;
			}
		}

		g_object_unref (file_info);
		g_object_unref (file);
	}

	g_file_enumerator_close (enumerator, NULL, NULL);  /* ignore errors from this */

	/* Did we break out of the loop because of an error? */
	if (child_error != NULL) {
		goto done;
	}

done:
	g_assert ((interesting_file == NULL) == (interesting_file_info == NULL));
	g_assert (child_error == NULL || interesting_file_info == NULL);

	if (child_error != NULL) {
		g_propagate_error (error, child_error);
	}

	g_clear_object (&enumerator);

	if (file_info_out != NULL) {
		*file_info_out = interesting_file_info;  /* transfer ownership */
	} else {
		g_clear_object (&interesting_file_info);
	}

	return interesting_file;
}

/**
 * copy_thumbnail_from_file:
 * @factory: global thumbnail factory
 * @file_uri: URI of the file whose thumbnail should be copied
 * @file_mtime: modification time of the file whose thumbnail should be copied
 * @file_mime_type: MIME type of the file whose thumbnail should be copied
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Generate or look up the thumbnail for the given file. This may fail if generating the thumbnail fails (e.g. due to no thumbnailer being available for
 * the given MIME type). The thumbnail for the file will be returned as a #GdkPixbuf.
 *
 * In case of error, @error will be set to a %G_FILE_ERROR or %GDK_PIXBUF_ERROR and %NULL will be returned.
 *
 * Return value: pixbuf representing the thumbnail for the given file, or %NULL on error
 */
static GdkPixbuf *
copy_thumbnail_from_file (GnomeDesktopThumbnailFactory *factory, const gchar *file_uri, const GTimeVal *file_mtime, const gchar *file_mime_type, GError **error)
{
	gchar *thumbnail_path;
	GdkPixbuf *pixbuf;

	thumbnail_path = gnome_desktop_thumbnail_factory_lookup (factory, file_uri, file_mtime->tv_sec);

	g_debug ("Getting thumbnail for file ‘%s’ from path ‘%s’.", file_uri, thumbnail_path);

	if (thumbnail_path == NULL) {
		/* No thumbnail exists for the file. Try and generate one. */
		if (gnome_desktop_thumbnail_factory_can_thumbnail (factory, file_uri, file_mime_type, file_mtime->tv_sec) == TRUE) {
			pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (factory, file_uri, file_mime_type);
			if (pixbuf == NULL) {
				/* gnome-desktop doesn't set an error so we have to. */
				g_debug ("Error generating thumbnail.");
				g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT, _("Error generating thumbnail for file ‘%s’."), file_uri);
			}
		} else {
			/* Can't generate a thumbnail for this type of file. gnome-desktop doesn't set an error so we have to. */
			g_debug ("Couldn’t generate thumbnail (because MIME type ‘%s’ is unsupported by the thumbnail factory).", file_mime_type);
			g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT, _("Error generating thumbnail for file ‘%s’: MIME type ‘%s’ is unsupported."), file_uri, file_mime_type);
			pixbuf = NULL;
		}

		return pixbuf;
	}

	/* Otherwise, load up the existing thumbnail. */
	pixbuf = gdk_pixbuf_new_from_file (thumbnail_path, error);

	g_free (thumbnail_path);

	return pixbuf;
}

/**
 * create_thumbnail_for_directory:
 * @factory: global thumbnail factory
 * @input_directory: the directory to create a thumbnail for
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Create a thumbnail representing the given @input_directory, which should be a #GFile representing an existing directory. The thumbnail
 * will be returned as a #GdkPixbuf and must be unreffed using g_object_unref().
 *
 * On error (e.g. if @input_directory doesn’t exist, isn’t a directory or is empty), %NULL will be returned and @error will be set to a %G_FILE_ERROR.
 *
 * Return value: (transfer full): a #GdkPixbuf representing the thumbnail for the directory, or %NULL on error
 */
static GdkPixbuf *
create_thumbnail_for_directory (GnomeDesktopThumbnailFactory *factory, GFile *input_directory, GError **error)
{
	GFile *interesting_file = NULL;
	GFileInfo *interesting_file_info = NULL;
	gchar *interesting_file_uri = NULL, *interesting_file_mime_type = NULL;
	GTimeVal interesting_file_mtime;
	GdkPixbuf *pixbuf = NULL;
	GError *child_error = NULL;

	interesting_file = pick_interesting_file_for_directory (input_directory, &interesting_file_info, factory, &child_error);
	if (child_error != NULL) {
		goto done;
	} else if (interesting_file == NULL) {
		/* No error, but the directory was empty. */
		g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, _("Directory is empty."));
		goto done;
	}

	interesting_file_uri = g_file_get_uri (interesting_file);
	g_file_info_get_modification_time (interesting_file_info, &interesting_file_mtime);
	interesting_file_mime_type = g_content_type_get_mime_type (g_file_info_get_content_type (interesting_file_info));
	pixbuf = copy_thumbnail_from_file (factory, interesting_file_uri, &interesting_file_mtime, interesting_file_mime_type, &child_error);

done:
	g_free (interesting_file_uri);
	g_free (interesting_file_mime_type);
	g_clear_object (&interesting_file_info);
	g_clear_object (&interesting_file);

	if (child_error != NULL) {
		g_propagate_error (error, child_error);
		g_assert (pixbuf == NULL);
	}

	return pixbuf;
}

/**
 * save_pixbuf:
 * @pixbuf: #GdkPixbuf to save
 * @output_file: location to save the pixbuf to
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Save the given @pixbuf in the location given by @output_file. This will overwrite any existing file at that location.
 *
 * On error, @error will be set.
 */
static void
save_pixbuf (GdkPixbuf *pixbuf, GFile *output_file, GError **error)
{
	gchar *output_filename;

	output_filename = g_file_get_path (output_file);
	g_debug ("Saving thumbnail to file ‘%s’.", output_filename);

	gdk_pixbuf_save (pixbuf, output_filename, "png", error, NULL);

	g_free (output_filename);
}

/* Command line options. */
static const GOptionEntry entries[] = {
	{ "size", 's', 0, G_OPTION_ARG_INT, &output_size, N_("Maximum size of the thumbnail in pixels (maximum width or height)"), NULL },
	{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("[INPUT FILE] [OUTPUT FILE]") },
	{ NULL },
};

/* main() return statuses. */
enum {
	STATUS_SUCCESS = 0,
	STATUS_INVALID_OPTIONS = 1,
	STATUS_ERROR_GENERATING_THUMBNAIL = 2,
	STATUS_ERROR_GENERATING_THUMBNAIL_EMPTY_DIRECTORY = 3,
	STATUS_ERROR_SAVING_THUMBNAIL = 4,
};

int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GError *child_error = NULL;
	int status = 0;
	GFile *input_directory = NULL, *output_file = NULL;
	GdkPixbuf *pixbuf = NULL;
	GnomeDesktopThumbnailFactory *factory = NULL;
	GnomeDesktopThumbnailSize thumbnail_size;

	/* Localisation */
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_set_application_name ("gnome-directory-thumbnailer");

	/* Handle the command line options. */
	/* Translators: This is the command line description of what the application does. Please keep the em-dash (or an equivalent). */
	context = g_option_context_new (_("— Generate thumbnails for directories"));
	g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

	if (g_option_context_parse (context, &argc, &argv, &child_error) == FALSE) {
		g_printerr (_("Couldn’t parse command line options: %s\n"), child_error->message);
		g_error_free (child_error);

		return STATUS_INVALID_OPTIONS;
	}

	/* Check both an input and an output filename were provided. Check the output size is sensible. */
	if (filenames == NULL || g_strv_length (filenames) != 2 ||
	    output_size < -1 || output_size == 0) {
		gchar *help = g_option_context_get_help (context, FALSE, NULL);
		g_print ("%s", help);
		g_free (help);

		status = STATUS_INVALID_OPTIONS;
		goto done;
	}

	/* Turn them into GFiles because GFiles are nice. */
	input_directory = g_file_new_for_commandline_arg (filenames[0]);
	output_file = g_file_new_for_commandline_arg (filenames[1]);

	/* Build a thumbnail factory. Match the factory's size to the requested thumbnail size.
	 *  • GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL is up to 128px
	 *  • GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE is up to 256px
	 */
	if (output_size == -1 || output_size <= 128) {
		thumbnail_size = GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL;
	} else {
		thumbnail_size = GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE;
	}

	factory = gnome_desktop_thumbnail_factory_new (thumbnail_size);

	/* Create the thumbnail. */
	pixbuf = create_thumbnail_for_directory (factory, input_directory, &child_error);
	if (child_error != NULL) {
		gchar *input_directory_path = g_file_get_path (input_directory);
		g_printerr (_("Couldn’t generate thumbnail for directory ‘%s’: %s\n"), input_directory_path, child_error->message);
		g_free (input_directory_path);
		g_error_free (child_error);

		status = (g_error_matches (child_error, G_FILE_ERROR, G_FILE_ERROR_FAILED) == TRUE) ? STATUS_ERROR_GENERATING_THUMBNAIL_EMPTY_DIRECTORY : STATUS_ERROR_GENERATING_THUMBNAIL;
		goto done;
	}

	/* Scale the pixbuf down if necessary. */
	if (output_size != -1) {
		gint original_width, original_height, scaled_width, scaled_height;
		gdouble scale;

		original_width = gdk_pixbuf_get_width (pixbuf);
		original_height = gdk_pixbuf_get_height (pixbuf);

		scale = (gdouble) output_size / (gdouble) MAX (original_width, original_height);

		scaled_width = round ((gdouble) original_width * scale);
		scaled_height = round ((gdouble) original_height * scale);

		g_debug ("Calculated scaling factor %f.", scale);

		/* Only do the scaling if it will be a strictly downscaling operation. */
		if (scale < 1.0) {
			GdkPixbuf *scaled_pixbuf;

			g_debug ("Scaling thumbnail from %u×%u to %u×%u with for output size %i with scaling factor %f.",
			         original_width, original_height, scaled_width, scaled_height, output_size, scale);

			scaled_pixbuf = gnome_desktop_thumbnail_scale_down_pixbuf (pixbuf, scaled_width, scaled_height);
			g_object_unref (pixbuf);
			pixbuf = scaled_pixbuf;  /* transfer ownership */
		}
	}

	/* Save it. */
	save_pixbuf (pixbuf, output_file, &child_error);
	if (child_error != NULL) {
		gchar *output_file_path = g_file_get_path (output_file);
		g_printerr (_("Couldn’t save thumbnail to ‘%s’: %s\n"), output_file_path, child_error->message);
		g_free (output_file_path);
		g_error_free (child_error);

		status = STATUS_ERROR_SAVING_THUMBNAIL;
		goto done;
	}

done:
	g_strfreev (filenames);
	g_clear_object (&input_directory);
	g_clear_object (&output_file);
	g_clear_object (&pixbuf);
	g_clear_object (&factory);

	g_debug ("Exiting with status %i.", status);

	return status;
}
