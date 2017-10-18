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
    LA_NONE,
    LA_ICONIFY
} WindowCommand;

GtkWidget *window;
gchar *wallpaper_conf_url;
gchar *lapps_wallpaper;

typedef struct {
	LXPanel *panel;
	config_setting_t *settings;
	GtkWidget *icon_image;
	gint icon_size;
	char *version;
} LaunchAppsPlugin;

static void lapps_get_wallpaper() {
	gchar ch;
	FILE *file;

	gsize length;
	gchar *content, *filename = wallpaper_conf_url, *test, **wallpaper;

	if (g_file_test(filename, G_FILE_TEST_EXISTS)) {
		if (g_file_get_contents(filename, &content, &length, NULL)) {
			test = g_strrstr(content, "wallpaper=");
			test += 10;
			wallpaper = g_strsplit(test, "\n", -1);
			lapps_wallpaper = strdup(*wallpaper);
			g_strfreev(wallpaper);
			g_free(content);
		}
	}
}

static char *lapps_get_wallpaper_conf_path() {
	gchar *homedir;
	gchar *conf_url;

	if ((homedir = g_strdup(getenv("HOME"))) == NULL) {
		homedir = g_strdup(getpwuid(getuid())->pw_dir);
	}

	int ls = system("ls ~/.config/pcmanfm/LXDE/desktop-items-0.conf");
	if (ls == 0)
		conf_url = g_strdup(g_strconcat(g_strdup(homedir), "/.config/pcmanfm/LXDE/desktop-items-0.conf", NULL));
	else
		conf_url = g_strdup(g_strconcat(g_strdup(homedir), "/.config/pcmanfm/lubuntu/desktop-items-0.conf", NULL));

	return conf_url;
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
	//Window * wlapps;
	//char * nlapps = NULL;
	if (client_list != NULL) {
		/* Loop over all windows. */
		int current_desktop = get_net_current_desktop();
		int i;
		for (i = 0; i < client_count; i++) {
			/* Get the desktop and window type properties. */
			NetWMWindowType nwwt;
			int task_desktop = get_net_wm_desktop(client_list[i]);
			get_net_wm_window_type(client_list[i], &nwwt);
			//wlapps = &client_list[i];
			/*if (wlapps != NULL) {
				nlapps = get_utf8_property(*wlapps, a_NET_WM_VISIBLE_NAME);

				if (nlapps == NULL) {
					nlapps = get_utf8_property(*wlapps, a_NET_WM_NAME);
				}

				if (nlapps == NULL) {
					nlapps = get_textproperty(*wlapps, XA_WM_NAME);
				}
			}*/
			/* If the task is visible on the current desktop and it is an ordinary window,
			 * execute the requested Iconify. */

			//openlog("launchapps", LOG_PID | LOG_CONS, LOG_USER);
			//		syslog(LOG_INFO, " ");
			//		syslog(LOG_INFO, "nlapps: %s", nlapps);
			//		syslog(LOG_INFO, " ");
			//		closelog();

			//if ((g_strcmp0(nlapps, "LaunchApps") != 0 || (g_strcmp0(nlapps, "launchapps") != 0 ))
					//&&
					if(((task_desktop == -1) || (task_desktop == current_desktop))
							&& ((!nwwt.dock) && (!nwwt.desktop) && (!nwwt.splash))) {
				if (command == LA_ICONIFY)
					XIconifyWindow(xdisplay, client_list[i], DefaultScreen(xdisplay));
				if (command == LA_NONE)
					XMapWindow(xdisplay, client_list[i]);
			}
		}
	}
	XFree(client_list);
}

static void lapps_main_window_clicked(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	//LaunchAppsPlugin *lapps = (LaunchAppsPlugin *) user_data;
	gtk_widget_destroy(window);
	GdkScreen *screen = gtk_widget_get_screen(widget);
	//lapps_iconify_execute(screen, LA_NONE);
}

static void lapps_main_window_close(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	//LaunchAppsPlugin *lapps = (LaunchAppsPlugin *) user_data;
	//gtk_widget_hide_on_delete (window);
	gtk_widget_destroy(window);
}

static void lapps_exec(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	GAppInfo *app = (GAppInfo *) user_data;
	g_app_info_launch(app, NULL, NULL, NULL);
	g_free(app);
}

static void lapps_create_main_window() {
	// main window
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_NORMAL);
	gtk_window_maximize(GTK_WINDOW(window));
	gtk_window_stick(GTK_WINDOW(window));
	gtk_window_set_skip_pager_hint(GTK_WINDOW(window), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), TRUE);
	gtk_window_set_keep_above (GTK_WINDOW(window), TRUE);
	gtk_window_set_title(GTK_WINDOW(window), "LaunchApps");
	// gtk_window_set_opacity(GTK_WINDOW(window), 0.4);
	// g_signal_connect(G_OBJECT(lapps->window), "delete-event", gtk_widget_hide_on_delete, NULL);
	gtk_widget_set_app_paintable(window, TRUE);
	gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
	gtk_widget_add_events(window, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(G_OBJECT(window), "button-press-event", G_CALLBACK(lapps_main_window_clicked), NULL);

	// background
	GtkWidget *layout, *image;
	layout = gtk_layout_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER (window), layout);
	image = gtk_image_new_from_file(lapps_wallpaper);
	gtk_layout_put(GTK_LAYOUT(layout), image, 0, 0);

	// box
	GtkWidget *box = gtk_hbox_new(TRUE, 1);

	// list
	GtkWidget *list = gtk_list_new();
	GList *app_list = g_app_info_get_all();
	GList *l;
	GtkWidget *item;
	for (l = app_list; l != NULL; l = l->next) {
		if (g_icon_to_string(g_app_info_get_icon(l->data))) {
			item = gtk_list_item_new_with_label(
					g_strconcat(g_app_info_get_id(l->data), " - ", g_app_info_get_name(l->data), " - ",
							g_app_info_get_display_name(l->data), " - ", g_icon_to_string(g_app_info_get_icon(l->data)),
							NULL));
			g_signal_connect(G_OBJECT(item), "button-press-event", G_CALLBACK(lapps_exec), (gpointer)l->data);
			g_signal_connect(G_OBJECT(item), "button-release-event", G_CALLBACK(lapps_main_window_close), NULL);
			gtk_container_add(GTK_CONTAINER(list), item);
		}
	}
	gtk_box_pack_start(GTK_BOX(box), list, 0, 0, 0);

	// add box to window
	gtk_container_add(GTK_CONTAINER(window), box);
	gtk_widget_show_all(window);
	gtk_main();

	//lapps_set_icons_size(lapps);
}

static gboolean lapps_button_clicked(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	//LaunchAppsPlugin *lapps = (LaunchAppsPlugin *) user_data;
	GdkScreen* screen = gtk_widget_get_screen(widget);
	// lapps_iconify_execute(screen, LA_NONE);
	//gtk_widget_show_all(lapps->window);
	// lapps_create_main_window();
	// lapps_iconify_execute(screen, LA_ICONIFY);
	if(wallpaper_conf_url == NULL)
			wallpaper_conf_url = g_strdup(lapps_get_wallpaper_conf_path());

	lapps_get_wallpaper();
	lapps_create_main_window();
	return FALSE;
}

static void lapps_destructor(gpointer user_data) {
	LaunchAppsPlugin *lapps = (LaunchAppsPlugin *) user_data;
	g_signal_handlers_disconnect_by_func(window, lapps_main_window_clicked, NULL);
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

	lapps->icon_image = lxpanel_image_new_for_icon(panel, "launchapps.png", -1, NULL);

	g_signal_connect(icon_box, "button_press_event", G_CALLBACK(lapps_button_clicked), NULL);

	gtk_container_add(GTK_CONTAINER(icon_box), lapps->icon_image);
	gtk_widget_show(lapps->icon_image);

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
