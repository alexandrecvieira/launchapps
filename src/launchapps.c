/**
 *
 * Copyright (c) 2017 Alexandre C Vieira
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <syslog.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>

#include <libfm/fm-gtk.h>
#include "lxpanel.private/dbg.h"
#include "lxpanel.private/ev.h"
#include <lxpanel/icon-grid.h>
#include <lxpanel/misc.h>
#include <lxpanel/plugin.h>

#include <wand/MagickWand.h>

#define LAPPSICON "launchapps.png"
#define BG "bglaunchapps.jpg"
#define DEFAULTBG "/usr/share/lxpanel/images/launchapps-bg-default.jpg"

typedef enum {
	LA_NONE, LA_ICONIFY
} WindowCommand;

GtkWidget *window;
gboolean running = FALSE;
/* grid[0] = rows | grid[1] = columns */
gint icon_size, s_height, s_width, grid[2];

typedef struct {
	LXPanel *panel;
	config_setting_t *settings;
	GtkWidget *icon_image;
	char *image;
	char *image_test;
	char *bg_image;
} LaunchAppsPlugin;

static void lapps_set_icons_size() {
	GdkScreen *screen = gdk_screen_get_default();
	s_height = gdk_screen_get_height(screen);
	s_width = gdk_screen_get_width(screen);
	double suggested_size = (pow(s_width * s_height, ((double) (1.0 / 3.0))) / 1.6);

	if (suggested_size < 27) {
		icon_size = 24;
	} else if (suggested_size >= 27 && suggested_size < 40) {
		icon_size = 32;
	} else if (suggested_size >= 40 && suggested_size < 56) {
		icon_size = 48;
	} else if (suggested_size >= 56) {
		icon_size = 64;
	}

	if (s_width > s_height) { // normal landscape orientation
		grid[0] = 4;
		grid[1] = 6;
	} else { // most likely a portrait orientation
		grid[0] = 6;
		grid[1] = 4;
	}
}

static void lapps_main_window_close(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	gtk_widget_destroy(window);
	running = FALSE;
}

static void lapps_item_clicked_window_close(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	gtk_widget_destroy(window);
	running = FALSE;
}

static void lapps_exec(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	GAppInfo *app = g_app_info_dup((GAppInfo *) user_data);
	g_app_info_launch(app, NULL, NULL, NULL);
}

static GdkPixbuf *lapps_application_icon(GAppInfo *appinfo) {
	GdkPixbuf *icon;
	GtkIconInfo *icon_info;
	GIcon *g_icon = g_app_info_get_icon(G_APP_INFO(appinfo));
	GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
	icon_info = gtk_icon_theme_lookup_by_gicon(icon_theme, g_icon, icon_size, GTK_ICON_LOOKUP_USE_BUILTIN);
	icon = gtk_icon_info_load_icon(icon_info, NULL);
	return icon;
}

static gboolean lapps_blur_background(LaunchAppsPlugin *lapps) {
	MagickWand *inWand, *outWand;
	size_t width, height;
	gint rowstride, row;
	guchar *pixels;
	MagickBooleanType status;

	MagickWandGenesis();
	inWand = NewMagickWand();
	status = MagickReadImage(inWand, lapps->image);
	if (status == MagickFalse) {
		inWand = DestroyMagickWand(inWand);
		MagickWandTerminus();
		return FALSE;
	}
	outWand = CloneMagickWand(inWand);
	MagickBlurImage(outWand, 0, 15);
	MagickSetImageDepth(outWand, 24);

	MagickWriteImage(outWand, lapps->bg_image);

	if (inWand)
		inWand = DestroyMagickWand(inWand);
	if (outWand)
		outWand = DestroyMagickWand(outWand);

	MagickWandTerminus();
	return TRUE;
}

static GdkPixbuf *lapps_app_name(gchar *app_name) {
	GdkPixbuf *bg_target_pix;
	MagickWand *magick_wand, *c_wand;
	DrawingWand *d_wand;
	PixelWand *p_wand;
	size_t width, height;
	gint rowstride, row;
	guchar *pixels;
	gchar *name, *target_name;
	gint i = 0;
	gint spaces = 0;

	if (strlen(app_name) > 25) {
		while (*app_name) {
			if (*app_name == ' ')
				spaces++;
			if (i > 25) {
				if (spaces > 0)
					break;
			}
			app_name++;
			i++;
		}
		name = g_strndup(app_name - i, i);
		target_name = g_strconcat(name, "...", NULL);
	} else
		target_name = g_strdup(app_name);

	MagickWandGenesis();
	magick_wand = NewMagickWand();
	d_wand = NewDrawingWand();
	p_wand = NewPixelWand();
	PixelSetColor(p_wand, "none");

	// Create a new transparent image
	MagickNewImage(magick_wand, 640, 480, p_wand);

	// Set up a 16 point white font
	PixelSetColor(p_wand, "white");
	DrawSetFillColor(d_wand, p_wand);
	DrawSetFont(d_wand, "Verdana");
	DrawSetFontSize(d_wand, 14);

	// Turn antialias on - not sure this makes a difference
	DrawSetTextAntialias(d_wand, MagickTrue);

	// Now draw the text
	DrawAnnotation(d_wand, 200, 140, target_name);

	// Draw the image on to the magick_wand
	MagickDrawImage(magick_wand, d_wand);

	// Trim the image down to include only the text
	MagickTrimImage(magick_wand, 0);

	// equivalent to the command line +repage
	MagickResetImagePage(magick_wand, "");

	// Make a copy of the text image
	c_wand = CloneMagickWand(magick_wand);

	// Set the background colour to gray for the shadow
	PixelSetColor(p_wand, "black");
	MagickSetImageBackgroundColor(magick_wand, p_wand);

	// Opacity is a real number indicating (apparently) percentage
	MagickShadowImage(magick_wand, 100, 3, 0, 0);

	// Composite the text on top of the shadow
	MagickCompositeImage(magick_wand, c_wand, OverCompositeOp, 4, 4);

	width = MagickGetImageWidth(magick_wand);
	height = MagickGetImageHeight(magick_wand);

	bg_target_pix = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width, height);

	pixels = gdk_pixbuf_get_pixels(bg_target_pix);
	rowstride = gdk_pixbuf_get_rowstride(bg_target_pix);
	MagickSetImageDepth(magick_wand, 32);

	for (row = 0; row < height; row++) {
		guchar *data = pixels + row * rowstride;
		MagickExportImagePixels(magick_wand, 0, row, width, 1, "RGBA", CharPixel, data);
	}

	/* Clean up */
	if (magick_wand)
		magick_wand = DestroyMagickWand(magick_wand);
	if (c_wand)
		c_wand = DestroyMagickWand(c_wand);
	if (d_wand)
		d_wand = DestroyDrawingWand(d_wand);
	if (p_wand)
		p_wand = DestroyPixelWand(p_wand);

	MagickWandTerminus();

	return bg_target_pix;
}

static GdkPixbuf *lapps_shadow_icons(GdkPixbuf *src_pix) {
	GdkPixbuf *bg_target_pix;
	size_t width, height;
	gint rowstride, row;
	guchar *pixels;
	gchar *buffer;
	gsize buffer_size;
	MagickWand *src_wand, *dest_wand;

	MagickWandGenesis();

	src_wand = NewMagickWand();
	gdk_pixbuf_save_to_buffer(src_pix, &buffer, &buffer_size, "png", NULL);
	MagickBooleanType read = MagickReadImageBlob(src_wand, buffer, buffer_size);
	dest_wand = CloneMagickWand(src_wand);
	PixelWand *shadow_color = NewPixelWand();
	PixelSetColor(shadow_color, "black");
	MagickWand *shadow = CloneMagickWand(dest_wand);
	MagickSetImageBackgroundColor(dest_wand, shadow_color);
	MagickShadowImage(dest_wand, 100, 2, 0, 0);
	MagickCompositeImage(dest_wand, shadow, OverCompositeOp, 3, 3);
	width = MagickGetImageWidth(dest_wand);
	height = MagickGetImageHeight(dest_wand);
	MagickSetImageDepth(dest_wand, 32);

	bg_target_pix = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width, height);

	pixels = gdk_pixbuf_get_pixels(bg_target_pix);
	rowstride = gdk_pixbuf_get_rowstride(bg_target_pix);

	for (row = 0; row < height; row++) {
		guchar *data = pixels + row * rowstride;
		MagickExportImagePixels(dest_wand, 0, row, width, 1, "RGBA", CharPixel, data);
	}

	if (shadow)
		shadow = DestroyMagickWand(shadow);
	if (src_wand)
		src_wand = DestroyMagickWand(src_wand);
	if (dest_wand)
		dest_wand = DestroyMagickWand(dest_wand);
	if (shadow_color)
		shadow_color = DestroyPixelWand(shadow_color);

	MagickWandTerminus();

	return bg_target_pix;
}

static void lapps_create_main_window(LaunchAppsPlugin *lapps) {
	GtkWidget *layout, *bg_image, *app_box, *event_box, *app_label, *table;
	GdkPixbuf *image_pix, *target_image_pix, *icon_pix, *target_icon_pix;
	GList *app_list, *test_list;
	gint pages;

	// main window
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_NORMAL);
	gtk_window_fullscreen(GTK_WINDOW(window));
	gtk_window_stick(GTK_WINDOW(window));
	gtk_window_set_keep_above(GTK_WINDOW(window), TRUE);
	gtk_window_set_title(GTK_WINDOW(window), "Launch Apps");
	gtk_widget_set_app_paintable(window, TRUE);
	gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
	gtk_window_set_icon_from_file(GTK_WINDOW(window), g_strconcat("/usr/share/lxpanel/images/", LAPPSICON, NULL),
	NULL);
	gtk_widget_add_events(window, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(G_OBJECT(window), "button-press-event", G_CALLBACK(lapps_main_window_close), NULL);
	g_signal_connect(G_OBJECT(window), "delete-event", gtk_main_quit, NULL);
	image_pix = gdk_pixbuf_new_from_file(lapps->bg_image, NULL);
	layout = gtk_layout_new(NULL, NULL);
	gtk_layout_set_size(GTK_LAYOUT(layout), s_width, s_height);
	gtk_container_add(GTK_CONTAINER(window), layout);
	target_image_pix = gdk_pixbuf_scale_simple(image_pix, s_width, s_height, GDK_INTERP_BILINEAR);
	bg_image = gtk_image_new_from_pixbuf(target_image_pix);
	gtk_layout_put(GTK_LAYOUT(layout), bg_image, 0, 0);
	g_object_unref(image_pix);
	g_object_unref(target_image_pix);

	// icons boxes and table
	table = gtk_table_new(grid[0], grid[1], TRUE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 50);
	gtk_table_set_col_spacings(GTK_TABLE(table), 0);
	app_list = g_app_info_get_all();

	pages = g_list_length(app_list) / (grid[0] * grid[1]);
	int i = 0;
	int j = 0;
	int label_width = 0;
	int greatest_width = 0;

	for (test_list = app_list; test_list != NULL; test_list = test_list->next) {
		if (g_app_info_get_icon(test_list->data) != NULL) {
			icon_pix = lapps_shadow_icons(lapps_application_icon(test_list->data));
			target_icon_pix = gdk_pixbuf_scale_simple(icon_pix, icon_size, icon_size, GDK_INTERP_BILINEAR);
			app_box = gtk_vbox_new(TRUE, 1);
			event_box = gtk_event_box_new();
			gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box), FALSE);
			gtk_container_add(GTK_CONTAINER(event_box), app_box);
			g_signal_connect(G_OBJECT(event_box), "button-press-event", G_CALLBACK(lapps_exec),
					(gpointer )test_list->data);
			g_signal_connect(G_OBJECT(event_box), "button-release-event", G_CALLBACK(lapps_item_clicked_window_close),
					NULL);
			gtk_box_pack_start(GTK_BOX(app_box), gtk_image_new_from_pixbuf(target_icon_pix), FALSE, FALSE, 0);
			app_label = gtk_image_new_from_pixbuf(lapps_app_name(g_strdup(g_app_info_get_name(test_list->data))));
			label_width = gdk_pixbuf_get_width(gtk_image_get_pixbuf(GTK_IMAGE(app_label)));
			if (label_width > greatest_width)
				greatest_width = label_width;
			greatest_width = greatest_width / 6;
			gtk_box_pack_start(GTK_BOX(app_box), app_label, FALSE, FALSE, 0);
			gtk_table_attach(GTK_TABLE(table), event_box, j, j + 1, i, i + 1, GTK_SHRINK, GTK_FILL, greatest_width, 0);
			g_object_unref(icon_pix);
			g_object_unref(target_icon_pix);
			if (j < grid[1] - 1) {
				j++;
			} else {
				j = 0;
				i++;
			}
			if (i == grid[0])
				break;
		}
	}
	gtk_container_add(GTK_CONTAINER(layout), table);
	gtk_widget_show_all(window);
	gtk_main();
}

static void lapps_button_clicked(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	LaunchAppsPlugin *lapps = (LaunchAppsPlugin *) user_data;
	if (!running) {
		running = TRUE;
		lapps_create_main_window(lapps);
	}
}

static void lapps_destructor(gpointer user_data) {
	LaunchAppsPlugin *lapps = (LaunchAppsPlugin *) user_data;
	g_free(lapps->bg_image);
	g_free(lapps->image_test);
	g_free(lapps->image);
	g_free(lapps);
}

/* Callback when the configuration dialog has recorded a configuration change. */
static gboolean lapps_apply_configuration(gpointer user_data) {
	GtkWidget *p = user_data;
	LaunchAppsPlugin *lapps = lxpanel_plugin_get_data(p);
	gchar *confdir;
	gboolean blurred;

	confdir = g_strdup(g_strconcat(g_strdup(fm_get_home_dir()), "/.config/launchapps/", NULL));
	if (!g_file_test(confdir, G_FILE_TEST_EXISTS & G_FILE_TEST_IS_DIR)) {
		g_mkdir(confdir, 0700);
	}

	lapps->bg_image = g_strdup(DEFAULTBG);

	if (lapps->image != NULL) {
		lapps->bg_image = g_strconcat(confdir, BG, NULL);
		blurred = lapps_blur_background(lapps);
		if (!blurred)
			lapps->bg_image = g_strdup(DEFAULTBG);
	}

	config_group_set_string(lapps->settings, "image", lapps->image);
	config_group_set_string(lapps->settings, "image_test", lapps->image_test);

	g_free(confdir);

	return FALSE;
}

static gboolean lapps_configuration(gpointer user_data) {
	GtkWidget *p = user_data;
	LaunchAppsPlugin *lapps = lxpanel_plugin_get_data(p);

	if (lapps->image != NULL)
		lapps->bg_image = g_strconcat(g_strdup(fm_get_home_dir()), "/.config/launchapps/", BG, NULL);

	return FALSE;
}

/* Callback when the configuration dialog is to be shown. */
static GtkWidget *lapps_configure(LXPanel *panel, GtkWidget *p) {
	LaunchAppsPlugin *lapps = lxpanel_plugin_get_data(p);
	return lxpanel_generic_config_dlg("Application Launcher", panel, lapps_apply_configuration, p, "Background image",
			&lapps->image, CONF_TYPE_FILE_ENTRY,
			NULL);
}

static GtkWidget *lapps_constructor(LXPanel *panel, config_setting_t *settings) {
	LaunchAppsPlugin *lapps = g_new0(LaunchAppsPlugin, 1);
	GtkWidget *p, *icon_box;
	int i, color_icons;
	const char *str;

	/* Load parameters from the configuration file. */
	if (config_setting_lookup_string(settings, "image", &str))
		lapps->image = g_strdup(str);
	if (config_setting_lookup_string(settings, "image_test", &str))
		lapps->image_test = g_strdup(str);

	lapps->panel = panel;
	lapps->settings = settings;

	lapps->bg_image = g_strdup(DEFAULTBG);

	g_return_val_if_fail(lapps != NULL, 0);

	p = panel_icon_grid_new(panel_get_orientation(panel), panel_get_icon_size(panel), panel_get_icon_size(panel), 1, 0,
			panel_get_height(panel));

	lxpanel_plugin_set_data(p, lapps, lapps_destructor);

	lapps_configuration(p);

	icon_box = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(p), icon_box);
	gtk_widget_show(icon_box);

	lapps->icon_image = lxpanel_image_new_for_icon(panel, LAPPSICON, -1, NULL);

	g_signal_connect(icon_box, "button_release_event", G_CALLBACK(lapps_button_clicked), (gpointer ) lapps);

	gtk_container_add(GTK_CONTAINER(icon_box), lapps->icon_image);
	gtk_widget_show(lapps->icon_image);

	lapps_set_icons_size();

	return p;
}

FM_DEFINE_MODULE(lxpanel_gtk, launchapps);

LXPanelPluginInit fm_module_init_lxpanel_gtk = {
		.name = "Application Launcher",
		.description = "Application Launcher for LXPanel",
		.new_instance = lapps_constructor,
		.config = lapps_configure,
		.one_per_system = 1
};
