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
#include <gtk/gtk.h>
#include <gio/gio.h>
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
    WC_NONE,
    WC_ICONIFY
} WindowCommand;

typedef struct {
	LXPanel *panel;
	config_setting_t *settings;
	GtkWidget *icon_image, *window;
	gint icon_size;
	char *version;
} LaunchAppsPlugin;

/*
GtkWidget *app_list = gtk_list_new();
	GList *app_names;

	int nIndex;
	GList *node;
	GList *list = lapps_applications_list();
	for (nIndex = 0; node < g_list_nth(list, nIndex) - 20; nIndex++) {
		app_names = g_list_append (app_names, (char *) g_app_info_get_name(node->data));
	}

	gtk_list_append_items(GTK_LIST(app_list), app_names);

	GtkWidget *box = gtk_vbox_new(TRUE, 1);

	gtk_container_add(GTK_CONTAINER(box), app_list);

	gtk_container_add(GTK_CONTAINER(lapps->window), box);

	gtk_widget_show_all(lapps->window);
*/

static GList * lapps_applications_list() { /* https://developer.gnome.org/gio/stable/GAppInfo.html */
	return g_app_info_get_all();
}

static void lapps_set_icons_size(LaunchAppsPlugin *lapps) {
	GdkScreen *screen = gdk_screen_get_default();
	gint s_height = gdk_screen_get_height(screen);
	gint s_width = gdk_screen_get_width(screen);
	double suggested_size = (pow(s_width * s_height, ((double) (1.0 / 3.0))) / 1.6);
	if (suggested_size < 27) {
		lapps->icon_size = 24;
	} else if (suggested_size >= 27 && suggested_size < 40) {
		lapps->icon_size = 32;
	} else if (suggested_size >= 40 && suggested_size < 56) {
		lapps->icon_size = 48;
	} else if (suggested_size >= 56) {
		lapps->icon_size = 64;
	}
}

static void lapps_iconify_execute(GdkScreen * screen, WindowCommand command) {
	/* Get the list of all windows. */
	int client_count;
	Screen * xscreen = GDK_SCREEN_XSCREEN(screen);
	Window * client_list = get_xaproperty(RootWindowOfScreen(xscreen), a_NET_CLIENT_LIST, XA_WINDOW, &client_count);
	Display *xdisplay = DisplayOfScreen(xscreen);
	Window * wlapps;
	char * nlapps = NULL;
	if (client_list != NULL) {
		/* Loop over all windows. */
		int current_desktop = get_net_current_desktop();
		int i;
		for (i = 0; i < client_count; i++) {
			/* Get the desktop and window type properties. */
			NetWMWindowType nwwt;
			int task_desktop = get_net_wm_desktop(client_list[i]);
			get_net_wm_window_type(client_list[i], &nwwt);
			wlapps = &client_list[i];
			if (wlapps != NULL) {
				nlapps = get_utf8_property(*wlapps, a_NET_WM_VISIBLE_NAME);

				if (nlapps == NULL) {
					nlapps = get_utf8_property(*wlapps, a_NET_WM_NAME);
				}

				if (nlapps == NULL) {
					nlapps = get_textproperty(*wlapps, XA_WM_NAME);
				}
			}
			/* If the task is visible on the current desktop and it is an ordinary window,
			 * execute the requested Iconify. */
			if (!(g_strcmp0(nlapps, "LaunchApps") == 0)
					&& (((task_desktop == -1) || (task_desktop == current_desktop))
							&& ((!nwwt.dock) && (!nwwt.desktop) && (!nwwt.splash)))) {
				if (command == WC_ICONIFY)
					XIconifyWindow(xdisplay, client_list[i], DefaultScreen(xdisplay));
				if (command == WC_NONE)
					XMapWindow(xdisplay, client_list[i]);
			}
		}
	}
	XFree(client_list);
}

static void clicked(GtkWindow *win, GdkEventButton *event, gpointer user_data)
{
	gtk_widget_hide_on_delete(GTK_WIDGET(win));
	GdkScreen* screen = gtk_widget_get_screen(GTK_WIDGET(win));
	lapps_iconify_execute(screen, WC_NONE);
}

static gboolean lapps_button_clicked(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	LaunchAppsPlugin *lapps = (LaunchAppsPlugin *) user_data;
	gtk_widget_show_all(lapps->window);
	GdkScreen* screen = gtk_widget_get_screen(widget);
	lapps_iconify_execute(screen, WC_ICONIFY);
	return FALSE;
}

static void lapps_destructor(gpointer user_data) {
	LaunchAppsPlugin *lapps = (LaunchAppsPlugin *) user_data;
	g_signal_handlers_disconnect_by_func(lapps->window, clicked, lapps);
	g_free(lapps);
}

static GtkWidget *lapps_constructor(LXPanel *panel, config_setting_t *settings) {
	LaunchAppsPlugin *lapps = g_new0(LaunchAppsPlugin, 1);
	GtkWidget *p, *icon_box, *button, *box;
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

	lapps->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_type_hint (GTK_WINDOW(lapps->window), GDK_WINDOW_TYPE_HINT_NORMAL);
	gtk_window_maximize (GTK_WINDOW(lapps->window));
	gtk_window_stick (GTK_WINDOW(lapps->window));
	gtk_window_set_skip_pager_hint (GTK_WINDOW(lapps->window), TRUE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW(lapps->window), TRUE);
	gtk_window_set_title (GTK_WINDOW(lapps->window), "LaunchApps");
	gtk_window_set_opacity (GTK_WINDOW(lapps->window), 0.4);
	g_signal_connect (G_OBJECT(lapps->window), "delete-event", gtk_main_quit, NULL);
	gtk_widget_set_app_paintable(lapps->window, TRUE);
	gtk_window_set_decorated (GTK_WINDOW(lapps->window), FALSE);
	gtk_widget_add_events (lapps->window, GDK_BUTTON_PRESS_MASK);
	g_signal_connect (G_OBJECT(lapps->window), "button-press-event", G_CALLBACK(clicked), (gpointer) lapps);
	button = gtk_button_new_with_label ("Hello World");
	box = gtk_hbox_new (TRUE, 1);
	gtk_box_pack_start (GTK_BOX (box), button, 0, 0, 0);
	gtk_container_add (GTK_CONTAINER (lapps->window), box);
	// gtk_widget_show (button);

	lapps_set_icons_size(lapps);

	return p;
}

FM_DEFINE_MODULE (lxpanel_gtk, launchapps);

LXPanelPluginInit fm_module_init_lxpanel_gtk =
{
    .name = "LaunchApps(Application Launcher)",
    .description = "Application Launcher for LXPanel",
    .new_instance = lapps_constructor,
    .one_per_system = 1
};
