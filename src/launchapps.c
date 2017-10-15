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
#include <unistd.h>
#include <sys/types.h>
#include <gtk/gtk.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "lxpanel.private/dbg.h"
#include "lxpanel.private/ev.h"
#include <lxpanel/icon-grid.h>
#include <lxpanel/misc.h>
#include <lxpanel/plugin.h>

typedef struct {
	LXPanel *panel;
	config_setting_t *settings;
	GtkWidget *icon_image;
	char *version;
} LaunchAppsPlugin;


static void clicked(GtkWindow *win, GdkEventButton *event, gpointer user_data)
{
	gtk_widget_hide_on_delete (GTK_WIDGET(win));
}

static gboolean lapps_button_clicked(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_maximize (GTK_WINDOW(window));
	gtk_window_stick (GTK_WINDOW(window));
	gtk_window_set_title(GTK_WINDOW(window), "LaunchApps");
	gtk_window_set_opacity(GTK_WINDOW(window), 0.5);
	g_signal_connect(G_OBJECT(window), "delete-event", gtk_main_quit, NULL);

	gtk_widget_set_app_paintable(window, TRUE);

	gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
	gtk_widget_add_events(window, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(G_OBJECT(window), "button-press-event", G_CALLBACK(clicked), NULL);

	gtk_widget_show_all(window);
	gtk_main();

	return 0;
}

static void lapps_destructor(gpointer user_data) {
	LaunchAppsPlugin *lapps = (LaunchAppsPlugin *) user_data;
	g_free(lapps);
}

static GtkWidget *lapps_constructor(LXPanel *panel, config_setting_t *settings) {
	LaunchAppsPlugin *lapps = g_new0(LaunchAppsPlugin, 1);
	GtkWidget *p;
	GtkWidget *icon_box;
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

	lapps->icon_image = lxpanel_image_new_for_icon(panel, "launchapps.png", -1, NULL);

	g_signal_connect(icon_box, "button_press_event", G_CALLBACK(lapps_button_clicked), (gpointer) lapps);

	gtk_container_add(GTK_CONTAINER(icon_box), lapps->icon_image);
	gtk_widget_show(lapps->icon_image);

	return p;
}

FM_DEFINE_MODULE (lxpanel_gtk, launchapps);

LXPanelPluginInit fm_module_init_lxpanel_gtk =
{
    .name = "LaunchApps-Application Launcher",
    .description = "Application Launcher for LXPanel",
    .new_instance = lapps_constructor,
    .one_per_system = 1
};
