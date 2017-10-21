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

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "lxpanel.private/dbg.h"
#include "lxpanel.private/ev.h"
#include <lxpanel/icon-grid.h>
#include <lxpanel/misc.h>
#include <lxpanel/plugin.h>

typedef enum {
	LA_NONE, LA_ICONIFY
} WindowCommand;

GtkWidget *window;
gchar *lapps_icon = "launchapps.png", *wallpaper_conf_url, *lapps_wallpaper;
gboolean running = FALSE;
// grid[0] = rows | grid[1] = columns
gint icon_size, s_height, s_width, grid[2];

typedef struct {
	LXPanel *panel;
	config_setting_t *settings;
	GtkWidget *icon_image;
} LaunchAppsPlugin;

static void lapps_get_wallpaper() {
	gsize length;
	gchar *content, *test, **wallpaper;

	if (g_file_test(wallpaper_conf_url, G_FILE_TEST_EXISTS)) {
		if (g_file_get_contents(wallpaper_conf_url, &content, &length, NULL)) {
			test = g_strrstr(g_strdup(content), "wallpaper=");
			test += 10;
			wallpaper = g_strsplit(test, "\n", -1);
			lapps_wallpaper = strdup(*wallpaper);
			g_strfreev(wallpaper);
			g_free(content);
		}
	}
}

static void lapps_get_wallpaper_conf_path() {
	gchar *homedir, *conf_url;

	if ((homedir = g_strdup(getenv("HOME"))) == NULL) {
		homedir = g_strdup(getpwuid(getuid())->pw_dir);
	}

	int ls = system("ls ~/.config/pcmanfm/LXDE/desktop-items-0.conf");
	if (ls == 0)
		wallpaper_conf_url = g_strdup(
				g_strconcat(g_strdup(homedir), "/.config/pcmanfm/LXDE/desktop-items-0.conf", NULL));
	else
		wallpaper_conf_url = g_strdup(
				g_strconcat(g_strdup(homedir), "/.config/pcmanfm/lubuntu/desktop-items-0.conf", NULL));

	g_free(homedir);
}

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

static void lapps_create_main_window() {
	GtkWidget *layout, *image, *box, *event_box, *app_label;
	GdkPixbuf *image_pix, *target_image_pix, *icon_pix, *target_icon_pix;
	GtkWidget *table;
	GList *app_list, *test_list;

	// main window
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_NORMAL);
	gtk_window_fullscreen(GTK_WINDOW(window));
	gtk_window_stick(GTK_WINDOW(window));
	gtk_window_set_keep_above(GTK_WINDOW(window), TRUE);
	gtk_window_set_title(GTK_WINDOW(window), "Launch Apps");
	gtk_widget_set_app_paintable(window, TRUE);
	gtk_window_set_opacity(GTK_WINDOW(window), 0.9);
	gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
	gtk_window_set_icon_from_file(GTK_WINDOW(window), g_strconcat("/usr/share/lxpanel/images/", lapps_icon, NULL),
			NULL);
	gtk_widget_add_events(window, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(G_OBJECT(window), "button-press-event", G_CALLBACK(lapps_main_window_close), NULL);
	g_signal_connect(G_OBJECT(window), "delete-event", gtk_main_quit, NULL);

	// background
	image_pix = gdk_pixbuf_new_from_file(lapps_wallpaper, NULL);
	layout = gtk_layout_new(NULL, NULL);
	gtk_layout_set_size (GTK_LAYOUT(layout), s_width, s_height);
	gtk_container_add(GTK_CONTAINER(window), layout);
	target_image_pix = gdk_pixbuf_scale_simple(image_pix, s_width, s_height, GDK_INTERP_BILINEAR);
	image = gtk_image_new_from_pixbuf(target_image_pix);
	gtk_layout_put(GTK_LAYOUT(layout), image, 0, 0);
	g_object_unref(image_pix);
	g_object_unref(target_image_pix);

	// icons boxes and table
	table = gtk_table_new(grid[0], grid[1], TRUE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 50);
	gtk_table_set_col_spacings(GTK_TABLE(table), 0);
	app_list = g_app_info_get_all();

	int i = 0;
	int j = 0;

	for (test_list = app_list; test_list != NULL; test_list = test_list->next) {
		if (g_app_info_get_icon(test_list->data) != NULL) {
			icon_pix = lapps_application_icon(test_list->data);
			target_icon_pix = gdk_pixbuf_scale_simple(icon_pix, icon_size, icon_size, GDK_INTERP_BILINEAR);
			box = gtk_vbox_new(TRUE, 1);
			event_box = gtk_event_box_new();
			gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box), FALSE);
			gtk_container_add(GTK_CONTAINER(event_box), box);
			g_signal_connect(G_OBJECT(event_box), "button-press-event", G_CALLBACK(lapps_exec), (gpointer )test_list->data);
			g_signal_connect(G_OBJECT(event_box), "button-release-event", G_CALLBACK(lapps_item_clicked_window_close),
					NULL);
			gtk_box_pack_start(GTK_BOX(box), gtk_image_new_from_pixbuf(target_icon_pix), 0, 0, 0);
			app_label = gtk_label_new(NULL);
			gtk_label_set_markup(GTK_LABEL(app_label),
					g_strconcat("<span color=\"white\"><b>", g_strdup(g_app_info_get_name(test_list->data)), "</b></span>",
					NULL));
			gtk_box_pack_start(GTK_BOX(box), app_label, 0, 0, 0);
			gtk_table_attach_defaults(GTK_TABLE(table), event_box, j, j + 1, i, i + 1);
			g_object_unref(icon_pix);
			g_object_unref(target_icon_pix);
			if (j < grid[1]) {
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
	if (!running) {
		running = TRUE;
		lapps_get_wallpaper();
		lapps_create_main_window();
	}
}

static void lapps_destructor(gpointer user_data) {
	LaunchAppsPlugin *lapps = (LaunchAppsPlugin *) user_data;
	g_free(lapps);
}

static GtkWidget *lapps_constructor(LXPanel *panel, config_setting_t *settings) {
	LaunchAppsPlugin *lapps = g_new0(LaunchAppsPlugin, 1);
	GtkWidget *p, *icon_box;
	int i, color_icons;

	lapps->panel = panel;
	lapps->settings = settings;

	g_return_val_if_fail(lapps != NULL, 0);

	p = panel_icon_grid_new(panel_get_orientation(panel), panel_get_icon_size(panel), panel_get_icon_size(panel), 1, 0,
			panel_get_height(panel));

	lxpanel_plugin_set_data(p, lapps, lapps_destructor);

	icon_box = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(p), icon_box);
	gtk_widget_show(icon_box);

	lapps->icon_image = lxpanel_image_new_for_icon(panel, lapps_icon, -1, NULL);

	g_signal_connect(icon_box, "button_press_event", G_CALLBACK(lapps_button_clicked), (gpointer ) lapps);

	gtk_container_add(GTK_CONTAINER(icon_box), lapps->icon_image);
	gtk_widget_show(lapps->icon_image);

	if (wallpaper_conf_url == NULL)
		lapps_get_wallpaper_conf_path();

	lapps_set_icons_size();

	return p;
}

FM_DEFINE_MODULE(lxpanel_gtk, launchapps);

LXPanelPluginInit fm_module_init_lxpanel_gtk = {
		.name = "LaunchApps(Application Launcher)",
		.description = "Application Launcher for LXPanel",
		.new_instance = lapps_constructor,
		.one_per_system = 1
};
