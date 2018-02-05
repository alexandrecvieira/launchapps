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

#include "lappsutil.h"

#include <config.h>

#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <gdk/gdkkeysyms.h>

#include <libfm/fm-gtk.h>
#include "lxpanel.private/dbg.h"
#include "lxpanel.private/ev.h"
#include "lxpanel.private/private.h"
#include <lxpanel/icon-grid.h>
#include <lxpanel/misc.h>
#include <lxpanel/plugin.h>

#define LAPPSICON "system-run"
#define LAPPSNAME "LaunchApps"
#define BG "bglaunchapps.jpg"
#define IMAGEPATH "/usr/share/lxpanel/images/"
#define CONFPATH "/.config/launchapps/"
#define CONFFILE "launchapps.recent"
#define DEFAULTBG "launchapps-bg-default.jpg"

typedef enum {
	LA_NONE, LA_ICONIFY
} WindowCommand;

GtkWidget *window;
GtkWidget *table;
GtkWidget *table_fixed;
GtkWidget *indicator;
GtkWidget *indicator_fw;
GtkWidget *indicator_rw;
GList *app_list;
GList *recent_list;
GList *recent_tmp;
GList *table_list;
gboolean running;
gboolean filtered;
int pages;
int page_index;
int page_count;
int app_count;

typedef struct {
	LXPanel *panel;
	config_setting_t *settings;
	GtkWidget *icon_image;
	char *image;
	char *image_test;
	char *bg_image;
	char *version;
} LaunchAppsPlugin;

static void lapps_main_window_close(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	g_list_free(app_list);
	g_list_free(table_list);
	gtk_widget_destroy(window);
	running = FALSE;
}

static void _lapps_main_window_close_() {
	g_list_free(app_list);
	g_list_free(table_list);
	gtk_widget_destroy(window);
	running = FALSE;
}

static void lapps_item_clicked_window_close(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	g_list_free(app_list);
	g_list_free(table_list);
	gtk_widget_destroy(window);
	running = FALSE;
}

// load recent TRUE | save recent FALSE
static void lapps_loadsave_recent(gboolean read) {
	GList *test_list = NULL;
	GList *tmp = NULL;
	FILE *conf_file;
	char *app_name = NULL;
	char data[100];
	char path[100];
	int len = 0;
	strcpy(path, g_strdup(g_strconcat(g_strdup(fm_get_home_dir()), CONFPATH, NULL)));
	strcat(path, CONFFILE);

	if (read) {
		conf_file = fopen(path, "r");
		if (conf_file == NULL) {
			openlog("LaunchApps", LOG_PID | LOG_CONS, LOG_USER);
			syslog(LOG_INFO, "Recent Applications: Conf File Read Error");
			closelog();
		} else {
			while (fgets(data, 100, conf_file) != NULL) {
				len = strlen(data);
				if (data[len - 1] == '\n')
					data[len - 1] = '\0';
				recent_tmp = g_list_append(recent_tmp, g_strdup(data));
			}
			fclose(conf_file);
		}
	} else {
		conf_file = fopen(path, "w");
		if (conf_file == NULL) {
			openlog("LaunchApps", LOG_PID | LOG_CONS, LOG_USER);
			syslog(LOG_INFO, "Recent Applications: Conf File Write Error");
			closelog();
		} else {
			for (test_list = recent_list; test_list != NULL; test_list = test_list->next) {
				app_name = strcat(g_strdup(g_app_info_get_name(test_list->data)), "\n");
				fputs(app_name, conf_file);
			}
			fclose(conf_file);
		}
	}
}

static void lapps_exec(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	GAppInfo *app = g_app_info_dup((GAppInfo *) user_data);
	GList *test_list = NULL;
	int exists = 0;
	int item_position;

	if (g_list_length(recent_list) == 0) {
		recent_list = g_list_prepend(recent_list, app);
		lapps_loadsave_recent(FALSE);
	} else {
		for (test_list = recent_list; test_list != NULL; test_list = test_list->next) {
			if (g_strcmp0(g_app_info_get_name(test_list->data), g_app_info_get_name(app)) == 0) {
				item_position = g_list_position(recent_list, test_list);
				exists = 1;
				break;
			}
		}

		// recent_list contains item
		if (exists == 1) {
			GList *item = g_list_nth(recent_list, item_position);
			recent_list = g_list_remove_link(recent_list, item);
			g_list_free(item);
			recent_list = g_list_prepend(recent_list, app);
			lapps_loadsave_recent(FALSE);
		}

		// recent_list does not contain item
		if (exists == 0) {
			recent_list = g_list_prepend(recent_list, app);
			lapps_loadsave_recent(FALSE);
		}

		// remove last item (max 6 items)
		if (g_list_length(recent_list) > 6) {
			GList *item = g_list_last(recent_list);
			recent_list = g_list_remove_link(recent_list, item);
			g_list_free(item);
			lapps_loadsave_recent(FALSE);
		}
	}
	g_app_info_launch(app, NULL, NULL, NULL);
}

static GdkPixbuf *lapps_application_icon(GAppInfo *appinfo) {
	GdkPixbuf *icon = NULL;
	GtkIconInfo *icon_info = NULL;
	GIcon *g_icon = g_app_info_get_icon(G_APP_INFO(appinfo));
	GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
	icon_info = gtk_icon_theme_lookup_by_gicon(icon_theme, g_icon, icon_size, GTK_ICON_LOOKUP_USE_BUILTIN);
	icon = gtk_icon_info_load_icon(icon_info, NULL);
	return icon;
}

static GtkWidget *lapps_create_table() {
	GtkWidget *app_box = NULL;
	GtkWidget *event_box = NULL;
	GtkWidget *app_label = NULL;
	GtkWidget *this_table = NULL;
	GList *test_list = NULL;
	GdkPixbuf *icon_pix = NULL;
	GdkPixbuf *target_icon_pix = NULL;
	gchar *app_name = NULL;
	gchar *app_name_old = NULL;

	this_table = gtk_table_new(grid[0], grid[1], TRUE);
	gtk_table_set_row_spacings(GTK_TABLE(this_table), 50);
	gtk_table_set_col_spacings(GTK_TABLE(this_table), 40);

	int i = 0;
	int j = 0;

	for (test_list = app_list; test_list != NULL; test_list = test_list->next) {
		app_name = g_strdup(g_app_info_get_name(test_list->data));
		if (g_strcmp0(app_name, app_name_old) == 0) {
			app_count--;
		} else {
			icon_pix = shadow_icon(lapps_application_icon(test_list->data));
			target_icon_pix = gdk_pixbuf_scale_simple(icon_pix, icon_size, icon_size, GDK_INTERP_BILINEAR);
			app_box = gtk_vbox_new(TRUE, 0);
			event_box = gtk_event_box_new();
			gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box), FALSE);
			gtk_container_add(GTK_CONTAINER(event_box), app_box);
			g_signal_connect(G_OBJECT(event_box), "button-press-event", G_CALLBACK(lapps_exec),
					(gpointer )g_app_info_dup(test_list->data));
			g_signal_connect(G_OBJECT(event_box), "button-release-event", G_CALLBACK(lapps_item_clicked_window_close),
					NULL);
			gtk_box_pack_start(GTK_BOX(app_box), gtk_image_new_from_pixbuf(target_icon_pix), FALSE, FALSE, 0);
			app_label = gtk_image_new_from_pixbuf(create_app_name(app_name, font_size));
			gtk_widget_set_size_request(app_label, app_label_width, app_label_height);
			gtk_box_pack_start(GTK_BOX(app_box), app_label, FALSE, FALSE, 0);
			gtk_table_attach(GTK_TABLE(this_table), event_box, j, j + 1, i, i + 1, GTK_SHRINK, GTK_FILL, 0, 0);
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
		app_name_old = g_strdup(app_name);
	}

	g_free(app_name);

	return this_table;
}

static GtkWidget *lapps_create_recent_frame() {
	GtkWidget *app_box = NULL;
	GtkWidget *event_box = NULL;
	GtkWidget *app_label = NULL;
	GtkWidget *apps_vbox = NULL;
	GList *test_list = NULL;
	GdkPixbuf *icon_pix = NULL;
	GdkPixbuf *target_icon_pix = NULL;
	gchar *app_name = NULL;

	GtkWidget *recent_frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(recent_frame), GTK_SHADOW_IN);

	apps_vbox = gtk_vbox_new(TRUE, 0);

	GtkWidget *recent_frame_label = gtk_label_new(NULL);
	const gchar *str = "Recent Applications";
	const gchar *format = "<span foreground='white' size='medium'><b>\%s</b></span>";
	gchar *markup;
	markup = g_markup_printf_escaped(format, str);
	gtk_label_set_markup(GTK_LABEL(recent_frame_label), markup);
	g_free(markup);
	gtk_box_pack_start(GTK_BOX(apps_vbox), recent_frame_label, TRUE, TRUE, 0);

	for (test_list = recent_list; test_list != NULL; test_list = test_list->next) {
		app_name = g_strdup(g_app_info_get_name(test_list->data));
		icon_pix = shadow_icon(lapps_application_icon(test_list->data));
		target_icon_pix = gdk_pixbuf_scale_simple(icon_pix, icon_size, icon_size, GDK_INTERP_BILINEAR);
		app_box = gtk_vbox_new(TRUE, 0);
		event_box = gtk_event_box_new();
		gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box), FALSE);
		gtk_container_add(GTK_CONTAINER(event_box), app_box);
		g_signal_connect(G_OBJECT(event_box), "button-press-event", G_CALLBACK(lapps_exec),
				(gpointer )g_app_info_dup(test_list->data));
		g_signal_connect(G_OBJECT(event_box), "button-release-event", G_CALLBACK(lapps_item_clicked_window_close),
				NULL);
		gtk_box_pack_start(GTK_BOX(app_box), gtk_image_new_from_pixbuf(target_icon_pix), FALSE, FALSE, 0);
		app_label = gtk_image_new_from_pixbuf(create_app_name(app_name, font_size));
		gtk_widget_set_size_request(app_label, app_label_width, app_label_height);
		gtk_box_pack_start(GTK_BOX(app_box), app_label, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(apps_vbox), event_box, TRUE, TRUE, 0);
		g_object_unref(icon_pix);
		g_object_unref(target_icon_pix);
	}

	gtk_container_add(GTK_CONTAINER(recent_frame), apps_vbox);

	g_free(app_name);

	return recent_frame;
}

static void lapps_app_list(gchar *filter) {
	GList *test_list = NULL;
	GList *all_app_list = NULL;

	all_app_list = g_app_info_get_all();

	app_list = NULL;
	app_count = 0;

	if (filtered && strlen(filter) > 0) {
		for (test_list = all_app_list; test_list != NULL; test_list = test_list->next) {
			if ((g_app_info_get_icon(test_list->data) != NULL) && (g_app_info_get_description(test_list->data) != NULL)
					&& g_app_info_should_show(test_list->data)) {
				gchar *name = g_str_to_ascii(g_ascii_strdown(g_app_info_get_name(test_list->data), -1), NULL);
				gchar *description = g_str_to_ascii(g_ascii_strdown(g_app_info_get_description(test_list->data), -1),
						NULL);
				gchar *id = g_str_to_ascii(g_ascii_strdown(g_app_info_get_id(test_list->data), -1), NULL);
				if (g_strrstr(name, filter) || g_strrstr(description, filter) || g_strrstr(id, filter)) {
					app_list = g_list_insert_sorted(app_list, g_app_info_dup(test_list->data),
							(GCompareFunc) app_name_comparator);
					app_count++;
				}
			}
		}
	}

	if (!filtered) {
		for (test_list = all_app_list; test_list != NULL; test_list = test_list->next) {
			if ((g_app_info_get_icon(test_list->data) != NULL) && (g_app_info_get_description(test_list->data) != NULL)
					&& g_app_info_should_show(test_list->data)) {
				app_list = g_list_insert_sorted(app_list, g_app_info_dup(test_list->data),
						(GCompareFunc) app_name_comparator);
				app_count++;
			}
		}
	}

	g_list_free(all_app_list);
}

static gint lapps_pages() {
	int total_page_itens = 0;
	int pages = 0;

	total_page_itens = grid[0] * grid[1];

	pages = app_count / total_page_itens;

	if (app_count % total_page_itens > 0)
		pages++;

	return pages;
}

static void lapps_update_indicator_rw(gboolean border) {
	char page_char[4];
	int page = page_index;

	if (GTK_IMAGE(indicator_rw))
		gtk_image_clear(GTK_IMAGE(indicator_rw));

	gtk_image_set_from_file(GTK_IMAGE(indicator_rw), g_strconcat(IMAGEPATH, "go-previous.png", NULL));

	if (page < lapps_pages())
		page++;

	sprintf(page_char, "%d", page);

	if (GTK_IMAGE(indicator))
		gtk_image_set_from_pixbuf(GTK_IMAGE(indicator), create_app_name(g_strdup(page_char), indicator_font_size));

	if (page > 1) {
		if (GTK_IMAGE(indicator_rw) && border)
			gtk_image_set_from_pixbuf(GTK_IMAGE(indicator_rw),
					shadow_icon(gtk_image_get_pixbuf(GTK_IMAGE(indicator_rw))));
	} else {
		if (GTK_IMAGE(indicator_rw))
			gtk_image_clear(GTK_IMAGE(indicator_rw));
	}
}

static void lapps_update_indicator_fw(gboolean border) {
	char page_char[4];
	int page = page_index;

	if (GTK_IMAGE(indicator_fw))
		gtk_image_clear(GTK_IMAGE(indicator_fw));

	gtk_image_set_from_file(GTK_IMAGE(indicator_fw), g_strconcat(IMAGEPATH, "go-next.png", NULL));

	if (page < lapps_pages())
		page++;

	sprintf(page_char, "%d", page);

	if (page < lapps_pages()) {
		if (GTK_IMAGE(indicator_fw) && border)
			gtk_image_set_from_pixbuf(GTK_IMAGE(indicator_fw),
					shadow_icon(gtk_image_get_pixbuf(GTK_IMAGE(indicator_fw))));
	} else {
		if (GTK_IMAGE(indicator_fw))
			gtk_image_clear(GTK_IMAGE(indicator_fw));
	}
}

static void lapps_indicator_rw_hover(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	if (event->type == GDK_ENTER_NOTIFY || event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE)
		lapps_update_indicator_rw(TRUE);
	else
		lapps_update_indicator_rw(FALSE);
}

static void lapps_indicator_fw_hover(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	if (event->type == GDK_ENTER_NOTIFY || event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE)
		lapps_update_indicator_fw(TRUE);
	else
		lapps_update_indicator_fw(FALSE);
}

static void lapps_clear() {
	GList *test_list = NULL;

	table = NULL;
	for (test_list = table_list; test_list != NULL; test_list = test_list->next) {
		gtk_widget_hide_all(test_list->data);
	}

	table_list = NULL;
	gtk_widget_draw(window, NULL);
}

static void lapps_show_page(gboolean up) {
	GList *test_list = NULL;

	int i = 0;

	if (filtered) {
		filtered = FALSE;
		lapps_clear();
	}

	if (up && page_count < lapps_pages()) {
		page_count++;
		table = lapps_create_table();
		table_list = g_list_append(table_list, table);
		gtk_fixed_put(GTK_FIXED(table_fixed), table, 0, 0);
	}

	for (test_list = table_list; test_list != NULL; test_list = test_list->next) {
		gtk_widget_hide_all(test_list->data);
		if (i == page_index)
			gtk_widget_show_all(test_list->data);
		i++;
	}

	lapps_update_indicator_rw(FALSE);
	lapps_update_indicator_fw(FALSE);

	gtk_widget_draw(window, NULL);
}

static gboolean lapps_on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
	switch (event->keyval) {
	case GDK_KEY_Down:
		(page_index > 0) ? page_index-- : 0;
		(lapps_pages() > 1) ? lapps_show_page(FALSE) : 0;
		break;

	case GDK_KEY_Up:
		(page_index < lapps_pages()) ? page_index++ : 0;
		(page_index < lapps_pages()) ? lapps_show_page(TRUE) : 0;
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
		(lapps_pages() > 1) ? lapps_show_page(FALSE) : 0;
		break;

	case GDK_SCROLL_UP:
		(page_index < lapps_pages()) ? page_index++ : 0;
		(page_index < lapps_pages()) ? lapps_show_page(TRUE) : 0;
		break;

	default:
		return FALSE;
	}

	return FALSE;
}

static void lapps_indicator_rw_clicked(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	(page_index > 0) ? page_index-- : 0;
	(lapps_pages() > 1) ? lapps_show_page(FALSE) : 0;
}

static void lapps_indicator_fw_clicked(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
	(page_index < lapps_pages()) ? page_index++ : 0;
	(page_index < lapps_pages()) ? lapps_show_page(TRUE) : 0;
}

static void lapps_search(GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEventButton *event, gpointer userdata) {
	const gchar *filter = gtk_entry_get_text(entry);

	page_index = 0;
	page_count = 0;

	if (GTK_ENTRY_ICON_SECONDARY == icon_pos) {
		if (strlen(filter) > 0) {
			filtered = TRUE;
			gtk_entry_set_icon_from_stock(GTK_ENTRY(entry), GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_CLEAR);
			lapps_app_list(g_ascii_strdown(filter, -1));
			lapps_show_page(TRUE);
			return;
		} else {
			filtered = FALSE;
			lapps_clear();
			lapps_app_list(NULL);
			lapps_show_page(TRUE);
		}
	}

	if (GTK_ENTRY_ICON_PRIMARY == icon_pos) {
		gtk_entry_set_text(entry, "");
		gtk_entry_set_icon_from_stock(GTK_ENTRY(entry), GTK_ENTRY_ICON_PRIMARY, NULL);
		lapps_clear();
		lapps_app_list(NULL);
		lapps_show_page(TRUE);
	}
}

static void lapps_search_(GtkEntry *entry, gpointer userdata) {
	const gchar *filter = g_ascii_strdown(gtk_entry_get_text(entry), -1);

	page_index = 0;
	page_count = 0;

	if (strlen(filter) > 0) {
		filtered = TRUE;
		gtk_entry_set_icon_from_stock(GTK_ENTRY(entry), GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_CLEAR);
		lapps_app_list(g_str_to_ascii(filter, NULL));
		lapps_show_page(TRUE);
		return;
	} else {
		gtk_entry_set_text(entry, "");
		gtk_entry_set_icon_from_stock(GTK_ENTRY(entry), GTK_ENTRY_ICON_PRIMARY, NULL);
		filtered = FALSE;
		lapps_clear();
		lapps_app_list(NULL);
		lapps_show_page(TRUE);
	}
}

static gboolean lapps_match_completion(GtkEntryCompletion *completion, const gchar *key, GtkTreeIter *iter,
		gpointer user_data) {
	GtkTreeModel *model = gtk_entry_completion_get_model(completion);
	gchar *name;
	gchar *description;
	gchar *id;
	gtk_tree_model_get(model, iter, 0, &name, 1, &description, 2, &id, -1);
	gchar *tofind = g_str_to_ascii(g_ascii_strdown(key, -1), NULL);
	gboolean match = (g_strrstr(g_str_to_ascii(g_ascii_strdown(name, -1), NULL), tofind) == NULL ? FALSE : TRUE)
			|| (g_strrstr(g_str_to_ascii(g_ascii_strdown(description, -1), NULL), key) == NULL ? FALSE : TRUE)
			|| (g_strrstr(g_str_to_ascii(g_ascii_strdown(id, -1), NULL), key) == NULL ? FALSE : TRUE);
	g_free(name);
	g_free(description);
	g_free(id);
	return match;
}

static gboolean lapps_match_completion_selected(GtkEntryCompletion *widget, GtkTreeModel *model, GtkTreeIter *iter,
		gpointer user_data) {
	GtkEntry *entry = (GtkEntry *) user_data;
	lapps_search_(entry, NULL);
	return FALSE;
}

static void lapps_create_main_window(LaunchAppsPlugin *lapps) {
	indicator = NULL;
	indicator_rw = NULL;
	indicator_fw = NULL;
	table = NULL;
	app_list = NULL;
	table_list = NULL;

	// window(global var)
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_NORMAL);
	gtk_window_fullscreen(GTK_WINDOW(window));
	gtk_window_stick(GTK_WINDOW(window));
	gtk_window_set_keep_above(GTK_WINDOW(window), TRUE);
	gtk_window_set_title(GTK_WINDOW(window), LAPPSNAME);
	gtk_widget_set_app_paintable(window, TRUE);
	gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
	gtk_window_set_icon_name(GTK_WINDOW(window), LAPPSICON);
	gtk_widget_add_events(window, GDK_BUTTON_PRESS_MASK);
	gtk_widget_add_events(window, GDK_KEY_RELEASE_MASK);
	g_signal_connect(G_OBJECT(window), "button-press-event", G_CALLBACK(lapps_main_window_close), NULL);
	g_signal_connect(G_OBJECT (window), "key-release-event", G_CALLBACK (lapps_on_key_press), NULL);
	g_signal_connect(window, "scroll-event", G_CALLBACK(lapps_on_mouse_scroll), NULL);
	g_signal_connect(G_OBJECT(window), "delete-event", gtk_main_quit, NULL);
	gtk_widget_show(window);

	// window background
	GdkPixbuf *image_pix = gdk_pixbuf_new_from_file(g_strdup(lapps->bg_image), NULL);
	GtkWidget *layout = gtk_layout_new(NULL, NULL);
	gtk_layout_set_size(GTK_LAYOUT(layout), s_width, s_height);
	gtk_container_add(GTK_CONTAINER(window), layout);
	GdkPixbuf *target_image_pix = gdk_pixbuf_scale_simple(image_pix, s_width, s_height, GDK_INTERP_BILINEAR);
	gtk_widget_show(layout);
	GtkWidget *bg_image = gtk_image_new_from_pixbuf(target_image_pix);
	gtk_layout_put(GTK_LAYOUT(layout), GTK_WIDGET(bg_image), 0, 0);
	gtk_widget_show(bg_image);
	g_object_unref(image_pix);
	g_object_unref(target_image_pix);

	// main vbox for applications
	GtkWidget *main_vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_set_size_request(main_vbox, s_width, s_height);
	gtk_container_set_border_width(GTK_CONTAINER(main_vbox), (screen_size_relation / 2));
	gtk_container_add(GTK_CONTAINER(layout), main_vbox);
	gtk_widget_show(main_vbox);

	// search entry
	GtkWidget *entry = gtk_entry_new();
	gtk_entry_set_icon_from_stock(GTK_ENTRY(entry), GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_FIND);
	g_signal_connect(GTK_OBJECT(entry), "icon-press", G_CALLBACK(lapps_search), NULL);
	g_signal_connect(GTK_OBJECT(entry), "activate", G_CALLBACK(lapps_search_), NULL);
	gtk_widget_set_size_request(entry, 250, 30);
	GtkWidget *entry_align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
	gtk_container_add(GTK_CONTAINER(entry_align), entry);
	gtk_box_pack_start(GTK_BOX(main_vbox), entry_align, FALSE, FALSE, 0);
	gtk_widget_show_all(entry_align);
	gtk_widget_grab_focus(entry);

	page_index = 0;
	page_count = 0;
	filtered = FALSE;

	lapps_app_list(NULL);

	// completion ******************************************************************
	GtkEntryCompletion *completion = gtk_entry_completion_new();
	gtk_entry_completion_set_minimum_key_length(completion, 2);
	gtk_entry_completion_set_popup_set_width(completion, FALSE);
	GtkListStore *store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	GList *test_list = NULL;
	for (test_list = app_list; test_list != NULL; test_list = test_list->next) {
		GtkTreeIter it;
		gtk_list_store_append(store, &it);
		gtk_list_store_set(store, &it, 0, g_strdup(g_app_info_get_name(test_list->data)), 1,
				g_strdup(g_app_info_get_description(test_list->data)), 2, g_strdup(g_app_info_get_id(test_list->data)),
				-1);
	}
	gtk_entry_completion_set_model(completion, (GtkTreeModel*) store);
	gtk_entry_completion_set_match_func(completion, (GtkEntryCompletionMatchFunc) lapps_match_completion, NULL, NULL);
	g_signal_connect(GTK_OBJECT(completion), "match-selected", G_CALLBACK(lapps_match_completion_selected),
			(gpointer )entry);
	g_object_unref(store);
	gtk_entry_completion_set_text_column(completion, 0);
	gtk_entry_set_completion(GTK_ENTRY(entry), completion);
	gtk_entry_completion_complete(completion);
	g_object_unref(completion);
	g_list_free(test_list);

	// recent applications and table ***********************************************
	if (g_list_length(recent_tmp) > 0) {
		GList *test_list = NULL;
		GList *test_recent_list = NULL;
		gchar *app_name = NULL;
		for (test_recent_list = recent_tmp; test_recent_list != NULL; test_recent_list = test_recent_list->next) {
			for (test_list = app_list; test_list != NULL; test_list = test_list->next) {
				app_name = g_strdup(g_app_info_get_name(test_list->data));
				if (g_strcmp0(app_name, test_recent_list->data) == 0) {
					recent_list = g_list_append(recent_list, g_app_info_dup(test_list->data));
					break;
				}
			}
		}
		g_free(app_name);
		g_list_free(test_recent_list);
	}
	recent_tmp = NULL;
	GtkWidget *apps_hbox = gtk_hbox_new(FALSE, screen_size_relation);
	GtkWidget *frame_vbox = gtk_vbox_new(FALSE, 0);
	GtkWidget *frame = lapps_create_recent_frame();
	GtkWidget *apps_hbox_align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
	GtkWidget *table_align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
	table_fixed = gtk_fixed_new();
	gtk_container_add(GTK_CONTAINER(table_align), table_fixed);
	if (g_list_length(recent_list) > 0) {
		gtk_widget_set_size_request(GTK_WIDGET(frame_vbox), app_label_width,
				(s_height - (screen_size_relation * 2)));
		gtk_box_pack_start(GTK_BOX(frame_vbox), frame, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(apps_hbox), frame_vbox, FALSE, FALSE, 0);
	} else
		gtk_widget_set_size_request(GTK_WIDGET(table_align), (s_width - screen_size_relation),
				(s_height - (screen_size_relation * 2)));
	gtk_box_pack_start(GTK_BOX(apps_hbox), table_align, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(apps_hbox_align), apps_hbox);
	gtk_box_pack_start(GTK_BOX(main_vbox), apps_hbox_align, FALSE, FALSE, 0);
	gtk_widget_show_all(apps_hbox_align);

	lapps_show_page(TRUE);

	// indicators
	GtkWidget *indicators_hbox = gtk_hbox_new(FALSE, 0);
	GtkWidget *indicators_align = gtk_alignment_new(0.5, 0.5, 0, 0);
	GtkWidget *indicator_event_box = NULL;
	indicator_rw = gtk_image_new();
	gtk_widget_set_size_request(GTK_WIDGET(indicator_rw), indicator_width, indicator_height);
	indicator_event_box = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(indicator_event_box), GTK_WIDGET(indicator_rw));
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(indicator_event_box), FALSE);
	gtk_box_pack_start(GTK_BOX(indicators_hbox), indicator_event_box, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(indicator_event_box), "button-press-event", G_CALLBACK(lapps_indicator_rw_clicked), NULL);
	g_signal_connect(G_OBJECT(indicator_event_box), "enter-notify-event", G_CALLBACK(lapps_indicator_rw_hover), NULL);
	g_signal_connect(G_OBJECT(indicator_event_box), "leave-notify-event", G_CALLBACK(lapps_indicator_rw_hover), NULL);
	g_signal_connect(G_OBJECT(indicator_event_box), "button-release-event", G_CALLBACK(lapps_indicator_rw_hover), NULL);
	indicator = gtk_image_new();
	gtk_widget_set_size_request(indicator, indicator_width, indicator_height);
	gtk_box_pack_start(GTK_BOX(indicators_hbox), indicator, FALSE, FALSE, 0);
	indicator_fw = gtk_image_new();
	gtk_widget_set_size_request(GTK_WIDGET(indicator_fw), indicator_width, indicator_height);
	indicator_event_box = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(indicator_event_box), GTK_WIDGET(indicator_fw));
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(indicator_event_box), FALSE);
	gtk_box_pack_start(GTK_BOX(indicators_hbox), indicator_event_box, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(indicator_event_box), "button-press-event", G_CALLBACK(lapps_indicator_fw_clicked), NULL);
	g_signal_connect(G_OBJECT(indicator_event_box), "enter-notify-event", G_CALLBACK(lapps_indicator_fw_hover), NULL);
	g_signal_connect(G_OBJECT(indicator_event_box), "leave-notify-event", G_CALLBACK(lapps_indicator_fw_hover), NULL);
	g_signal_connect(G_OBJECT(indicator_event_box), "button-release-event", G_CALLBACK(lapps_indicator_fw_hover), NULL);

	gtk_container_add(GTK_CONTAINER(indicators_align), indicators_hbox);
	gtk_box_pack_start(GTK_BOX(main_vbox), indicators_align, FALSE, FALSE, 0);
	gtk_widget_show_all(indicators_align);

	lapps_update_indicator_rw(FALSE);
	lapps_update_indicator_fw(FALSE);

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
	gchar *confdir = NULL;
	gboolean blurred;

	confdir = g_strdup(g_strconcat(g_strdup(fm_get_home_dir()), CONFPATH, NULL));

	lapps->bg_image = g_strconcat(IMAGEPATH, DEFAULTBG, NULL);

	if (lapps->image != NULL) {
		lapps->bg_image = g_strconcat(confdir, BG, NULL);
		blurred = blur_background(g_strdup(lapps->image), g_strdup(lapps->bg_image));
		if (!blurred)
			lapps->bg_image = g_strconcat(IMAGEPATH, DEFAULTBG, NULL);
	}

	config_group_set_string(lapps->settings, "image", lapps->image);
	config_group_set_string(lapps->settings, "image_test", lapps->image_test);

	return FALSE;
}

static void lapps_configuration(gpointer user_data) {
	GtkWidget *p = user_data;
	LaunchAppsPlugin *lapps = lxpanel_plugin_get_data(p);

	if (lapps->image != NULL)
		lapps->bg_image = g_strconcat(g_strdup(fm_get_home_dir()), CONFPATH, BG, NULL);
}

/* Callback when the configuration dialog is to be shown. */
static GtkWidget *lapps_configure(LXPanel *panel, GtkWidget *p) {
	LaunchAppsPlugin *lapps = lxpanel_plugin_get_data(p);
	return lxpanel_generic_config_dlg("Application Launcher", panel, lapps_apply_configuration, p,
			g_strconcat("LaunchApps ", g_strdup(lapps->version), NULL), NULL, CONF_TYPE_TRIM, " Application Launcher ",
			NULL, CONF_TYPE_TRIM, "Copyright (C) 2017", NULL, CONF_TYPE_TRIM, " ", NULL, CONF_TYPE_TRIM,
			"Background image", &lapps->image, CONF_TYPE_FILE_ENTRY,
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

	lapps->icon_image = gtk_image_new_from_icon_name(LAPPSICON, lapps->panel->priv->icon_size);
	gtk_widget_set_tooltip_text(lapps->icon_image, LAPPSNAME);

	g_signal_connect(icon_box, "button_release_event", G_CALLBACK(lapps_button_clicked), (gpointer ) lapps);

	gtk_container_add(GTK_CONTAINER(icon_box), lapps->icon_image);
	gtk_widget_show(lapps->icon_image);

	lapps->version = g_strdup(VERSION);

	set_icons_fonts_sizes();

	page_index = 0;

	running = FALSE;

	recent_list = NULL;

	// check configuration diretory
	gchar *confdir = g_strdup(g_strconcat(g_strdup(fm_get_home_dir()), CONFPATH, NULL));
	if (!g_file_test(confdir, G_FILE_TEST_EXISTS & G_FILE_TEST_IS_DIR)) {
		g_mkdir(confdir, 0700);
	}

	lapps_loadsave_recent(TRUE);

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
