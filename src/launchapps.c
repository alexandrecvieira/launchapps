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
#include <gdk/gdkkeysyms.h>

#include <libfm/fm-gtk.h>
#include "lxpanel.private/dbg.h"
#include "lxpanel.private/ev.h"
#include <lxpanel/icon-grid.h>
#include <lxpanel/misc.h>
#include <lxpanel/plugin.h>

#include <wand/MagickWand.h>

#define LAPPSICON "launchapps.png"
#define BG "bglaunchapps.jpg"
#define IMAGEPATH "/usr/share/lxpanel/images/"
#define DEFAULTBG "launchapps-bg-default.jpg"
#define DEFAULTFONTSIZE 16
#define INDICATORFONTSIZE 32
#define INDICATORWIDTH 30
#define INDICATORHEIGHT 30

typedef enum {
	LA_NONE, LA_ICONIFY
} WindowCommand;

GtkWidget *window;
GtkWidget *table;
GtkWidget *indicator;
GtkWidget *fixed_layout;
GList *app_list;
GList *table_list;
gboolean running = FALSE;
/* grid[0] = rows | grid[1] = columns */
int icon_size, s_height, s_width, grid[2];
int pages, total_page_itens, page_index;
int page_count;

typedef struct {
	LXPanel *panel;
	config_setting_t *settings;
	GtkWidget *icon_image;
	char *image;
	char *image_test;
	char *bg_image;
} LaunchAppsPlugin;

static void lapps_update_indicator();
static GtkWidget *lapps_table();

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
		grid[1] = 5;
	} else { // most likely a portrait orientation
		grid[0] = 5;
		grid[1] = 4;
	}
}

static void lapps_main_window_close(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	g_list_free(app_list);
	//g_list_free(indicators_list);
	g_list_free(table_list);
	gtk_widget_destroy(window);
	running = FALSE;
}

static void _lapps_main_window_close_() {
	g_list_free(app_list);
	//g_list_free(indicators_list);
	g_list_free(table_list);
	gtk_widget_destroy(window);
	running = FALSE;
}

static void lapps_item_clicked_window_close(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	g_list_free(app_list);
	//g_list_free(indicators_list);
	g_list_free(table_list);
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
	MagickSetImageDepth(outWand, 32);

	MagickWriteImage(outWand, lapps->bg_image);

	if (inWand)
		inWand = DestroyMagickWand(inWand);
	if (outWand)
		outWand = DestroyMagickWand(outWand);

	MagickWandTerminus();
	return TRUE;
}

static GdkPixbuf *lapps_app_name(gchar *app_name, double font_size) {
	GdkPixbuf *bg_target_pix;
	MagickWand *magick_wand, *c_wand;
	DrawingWand *d_wand;
	PixelWand *p_wand;
	size_t width, height;
	gint rowstride, row;
	guchar *pixels;
	gchar *name = NULL;
	gchar *name_two = NULL;
	gchar *target_name = NULL;
	int i = 0;
	int spaces = 0;

	if (strlen(app_name) > 25) {
		while (*app_name) {
			if (*app_name == ' ')
				spaces++;
			if (spaces > 2)
					break;
			app_name++;
			i++;
		}
		name = g_strndup(app_name - i, i);
		name_two = g_strdup(app_name);
		target_name = g_strconcat(name, "\n", name_two, NULL);
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
	DrawSetFont(d_wand, "Sans Serif");
	DrawSetFontSize(d_wand, font_size);

	// Turn antialias on - not sure this makes a difference
	DrawSetTextAntialias(d_wand, MagickTrue);

	// Now draw the text
	DrawAnnotation(d_wand, 0, font_size, target_name);

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

static void lapps_app_list() {
	GList *test_list, *all_app_list;
	int app_count = 0;

	all_app_list = g_app_info_get_all();

	total_page_itens = grid[0] * grid[1];

	app_list = NULL;

	for (test_list = all_app_list; test_list != NULL; test_list = test_list->next) {
		if ((g_app_info_get_icon(test_list->data) != NULL) && (g_app_info_get_description(test_list->data) != NULL)) {
			app_list = g_list_append(app_list, g_app_info_dup(test_list->data));
			app_count++;
		}
	}

	pages = app_count / total_page_itens;
	if (app_count % total_page_itens > 0)
		pages++;
}

static GtkWidget *lapps_table() {
	GtkWidget *app_box, *event_box, *app_label, *table;
	GList *test_list;
	GdkPixbuf *icon_pix, *target_icon_pix;

	table = gtk_table_new(grid[0], grid[1], TRUE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 50);
	gtk_table_set_col_spacings(GTK_TABLE(table), 40);

	int i = 0;
	int j = 0;
	for (test_list = app_list; test_list != NULL; test_list = test_list->next) {
		if (g_app_info_get_icon(test_list->data) != NULL) {
			icon_pix = lapps_shadow_icons(lapps_application_icon(test_list->data));
			target_icon_pix = gdk_pixbuf_scale_simple(icon_pix, icon_size, icon_size, GDK_INTERP_BILINEAR);
			app_box = gtk_vbox_new(TRUE, 1);
			event_box = gtk_event_box_new();
			gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box), FALSE);
			gtk_container_add(GTK_CONTAINER(event_box), app_box);
			g_signal_connect(G_OBJECT(event_box), "button-press-event", G_CALLBACK(lapps_exec),
					(gpointer )g_app_info_dup(test_list->data));
			g_signal_connect(G_OBJECT(event_box), "button-release-event", G_CALLBACK(lapps_item_clicked_window_close),
					NULL);
			gtk_box_pack_start(GTK_BOX(app_box), gtk_image_new_from_pixbuf(target_icon_pix), FALSE, FALSE, 0);
			app_label = gtk_image_new_from_pixbuf(lapps_app_name(g_strdup(g_app_info_get_name(test_list->data)), DEFAULTFONTSIZE));
			gtk_widget_set_size_request(app_label, 250, 50);
			gtk_box_pack_start(GTK_BOX(app_box), app_label, FALSE, FALSE, 0);
			gtk_table_attach(GTK_TABLE(table), event_box, j, j + 1, i, i + 1, GTK_SHRINK, GTK_FILL, 0, 0);
			g_object_unref(icon_pix);
			g_object_unref(target_icon_pix);
			if (j < grid[1] - 1) {
				j++;
			} else {
				j = 0;
				i++;
			}
			if (i == grid[0]) {
				app_list = test_list->next;
				break;
			}
		}
	}

	return table;
}

static void lapps_update_indicator(int page) {
	char page_char[4];

	page++;
	sprintf(page_char, "%d", page);
	gtk_image_set_from_pixbuf(GTK_IMAGE(indicator), lapps_app_name(g_strdup(page_char), INDICATORFONTSIZE));
}

static void lapps_show_page(gboolean up) {
	GList *test_list;
	int i = 0;

	if (up && page_count < pages) {
		page_count++;
		table = lapps_table();
		table_list = g_list_append(table_list, table);
		gtk_fixed_put(GTK_FIXED(fixed_layout), table, 250, 220);
	}

	for (test_list = table_list; test_list != NULL; test_list = test_list->next) {
		gtk_widget_hide_all(test_list->data);
		if (i == page_index)
			gtk_widget_show_all(test_list->data);
		i++;
	}

	lapps_update_indicator(page_index);

	gtk_widget_draw(window, NULL);
}

static gboolean lapps_on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
	switch (event->keyval) {
	case GDK_KEY_Down:
		(page_index > 0) ? page_index-- : 0;
		lapps_show_page(FALSE);
		break;

	case GDK_KEY_Up:
		(page_index < pages) ? page_index++ : 0;
		(page_index < pages) ? lapps_show_page(TRUE) : 0;
		break;

	case GDK_Escape:
		_lapps_main_window_close_();
		break;

	default:
		return FALSE;
	}

	return FALSE;
}

static gboolean lapps_on_mouse_scroll(GtkWidget *widget, GdkEventScroll *event, gpointer user_data) {
	switch (event->direction) {
	case GDK_SCROLL_DOWN:
		(page_index > 0) ? page_index-- : 0;
		lapps_show_page(FALSE);
		break;

	case GDK_SCROLL_UP:
		(page_index < pages) ? page_index++ : 0;
		(page_index < pages) ? lapps_show_page(TRUE) : 0;
		break;

	default:
		return FALSE;
	}

	return FALSE;
}

static void lapps_create_main_window(LaunchAppsPlugin *lapps) {
	GtkWidget *layout, *bg_image, *entry, *indicator_box;
	GdkPixbuf *image_pix, *target_image_pix;

	app_list = NULL;
	table_list = NULL;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_NORMAL);
	gtk_window_fullscreen(GTK_WINDOW(window));
	gtk_window_stick(GTK_WINDOW(window));
	gtk_window_set_keep_above(GTK_WINDOW(window), TRUE);
	gtk_window_set_title(GTK_WINDOW(window), "Launch Apps");
	gtk_widget_set_app_paintable(window, TRUE);
	gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
	gtk_window_set_icon_from_file(GTK_WINDOW(window), g_strconcat(IMAGEPATH, LAPPSICON, NULL),
	NULL);
	gtk_widget_add_events(window, GDK_BUTTON_PRESS_MASK);
	gtk_widget_add_events(window, GDK_KEY_RELEASE_MASK);
	g_signal_connect(G_OBJECT(window), "button-press-event", G_CALLBACK(lapps_main_window_close), NULL);
	g_signal_connect (G_OBJECT (window), "key-release-event", G_CALLBACK (lapps_on_key_press), NULL);
	g_signal_connect(window, "scroll-event", G_CALLBACK(lapps_on_mouse_scroll), NULL);
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

	// fixed layout
	fixed_layout = gtk_fixed_new();

	// search entry
	entry = gtk_entry_new();
	// gtk_entry_set_text( GTK_ENTRY(entry), "Search");
	// Bind the callback function to the insert text event
	//g_signal_connect(GTK_OBJECT(entry), "insert-text",
	//                     G_CALLBACK(on_insert_text), NULL);
	gtk_fixed_put(GTK_FIXED(fixed_layout), GTK_WIDGET(entry), (s_width / 2) - 125, 50);
	gtk_widget_set_size_request (entry, 250, 30);

	page_index = 0;
	page_count = 0;

	lapps_app_list();

	gtk_container_add(GTK_CONTAINER(layout), fixed_layout);

	lapps_show_page(TRUE);

	indicator_box = gtk_vbox_new(TRUE, 1);
	indicator = gtk_image_new();
	gtk_box_pack_start(GTK_BOX(indicator_box), indicator, FALSE, FALSE, 0);
	gtk_widget_set_size_request(indicator, INDICATORWIDTH, INDICATORHEIGHT);
	gtk_fixed_put(GTK_FIXED(fixed_layout), indicator_box, (s_width / 2) - INDICATORWIDTH, 1015);
	lapps_update_indicator(page_index);

	gtk_widget_show(layout);
	gtk_widget_show(bg_image);
	gtk_widget_show(fixed_layout);
	gtk_widget_show_all(indicator_box);
	gtk_widget_show(entry);
	gtk_widget_show(window);

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

	lapps->bg_image = g_strconcat(IMAGEPATH, DEFAULTBG, NULL);

	if (lapps->image != NULL) {
		lapps->bg_image = g_strconcat(confdir, BG, NULL);
		blurred = lapps_blur_background(lapps);
		if (!blurred)
			lapps->bg_image = g_strconcat(IMAGEPATH, DEFAULTBG, NULL);
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

	lapps->bg_image = g_strconcat(IMAGEPATH, DEFAULTBG, NULL);

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

	page_index = 0;

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
