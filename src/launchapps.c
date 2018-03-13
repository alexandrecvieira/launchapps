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

typedef struct {
	LXPanel *panel;
	config_setting_t *settings;
	GtkWidget *icon_image;
	char *image_path;
	char *image_path_test;
	char *version;
} LaunchAppsPlugin;

static GtkWidget *main_window = NULL;
static GtkWidget *page_table = NULL;
static GtkWidget *layout_fixed = NULL;
static GtkWidget *indicator = NULL;
static GtkWidget *indicator_fw = NULL;
static GtkWidget *indicator_rw = NULL;
static GdkPixbuf *indicator_fw_pix = NULL;
static GdkPixbuf *indicator_rw_pix = NULL;
static GdkPixbuf *indicator_fw_shaded_pix = NULL;
static GdkPixbuf *indicator_rw_shaded_pix = NULL;
static GList *all_apps_list = NULL;
static GList *system_apps_list = NULL;
static GList *recent_tmp = NULL;
static GList *pages_list = NULL;
static GHashTable *labels_list = NULL;
static gboolean filtered = FALSE;
static int page_index = 0;
static int page_count = 0;
static int app_count = 0;
static int page_last_position = 0;
static const char *confdir;
static char *bg_image_path;

// callback when window is to be closed
static void lapps_main_window_close(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	g_list_free(pages_list);
	g_list_free(recent_tmp);
	g_list_free(system_apps_list);
	g_list_free(all_apps_list);
	gtk_widget_destroy(GTK_WIDGET(main_window));
	main_window = NULL;
}

// load recent: load = TRUE | save recent: load = FALSE
static void lapps_loadsave_recent(gboolean load)
{
	FILE *conf_file;

	const char *path = g_strconcat(confdir, CONFFILE, NULL);

	if (load)
	{
		conf_file = fopen(path, "r");
		if (conf_file == NULL)
		{
			openlog("LaunchApps", LOG_PID | LOG_CONS, LOG_USER);
			syslog(LOG_INFO, "Recent Applications: Conf File Read Error");
			closelog();
		}
		else
		{
			char data[100];
			int len = 0;
			while (fgets(data, 100, conf_file) != NULL)
			{
				len = strlen(data);
				if (data[len - 1] == '\n')
					data[len - 1] = '\0';
				recent_tmp = g_list_append(recent_tmp, g_strdup(data));
			}
			fclose(conf_file);
		}
	}
	else
	{
		conf_file = fopen(path, "w");
		if (conf_file == NULL)
		{
			openlog("LaunchApps", LOG_PID | LOG_CONS, LOG_USER);
			syslog(LOG_INFO, "Recent Applications: Conf File Write Error");
			closelog();
		}
		else
		{
			GList *test_list = NULL;
			for (test_list = recent_tmp; test_list; test_list = test_list->next)
			{
				const char *app_name = g_strconcat(test_list->data, "\n", NULL);
				fputs(app_name, conf_file);
			}
			fclose(conf_file);
		}
	}
}

static void lapps_update_recent(const char *app_name)
{
	if (g_list_length(recent_tmp) == 0)
	{
		recent_tmp = g_list_prepend(recent_tmp, g_strdup(app_name));
		lapps_loadsave_recent(FALSE);
	}
	else
	{
		GList *test_list = NULL;
		int exists = 0;
		int item_position;
		for (test_list = recent_tmp; test_list; test_list = test_list->next)
		{
			if (g_strcmp0(test_list->data, app_name) == 0)
			{
				item_position = g_list_position(recent_tmp, test_list);
				exists = 1;
				break;
			}
		}
		// recent_list contains item
		if (exists == 1)
		{
			GList *item = g_list_nth(recent_tmp, item_position);
			recent_tmp = g_list_remove_link(recent_tmp, item);
			g_list_free_1(item);
			recent_tmp = g_list_prepend(recent_tmp, g_strdup(app_name));
			lapps_loadsave_recent(FALSE);
		}
		// recent_list does not contain item
		if (exists == 0)
		{
			recent_tmp = g_list_prepend(recent_tmp, g_strdup(app_name));
			lapps_loadsave_recent(FALSE);
		}
		// remove last item (s_height <= 768 = max 5 items | s_height > 768 = max 6 items)
		if (s_height < 1024)
		{
			if (g_list_length(recent_tmp) > 5)
			{
				GList *item = g_list_last(recent_tmp);
				recent_tmp = g_list_remove_link(recent_tmp, item);
				g_list_free_1(item);
				lapps_loadsave_recent(FALSE);
			}
		}
		else
		{
			if (g_list_length(recent_tmp) > 6)
			{
				GList *item = g_list_last(recent_tmp);
				recent_tmp = g_list_remove_link(recent_tmp, item);
				g_list_free_1(item);
				lapps_loadsave_recent(FALSE);
			}
		}
	}
}

// callback when application is to be run
static void lapps_exec(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	GAppInfo *app = (GAppInfo *) user_data;
	const char *app_name = g_app_info_get_name(app);
	lapps_update_recent(app_name);
	g_app_info_launch(app, NULL, NULL, NULL);
}

// create application's icon and its shadow
static GdkPixbuf *lapps_application_icon(GAppInfo *appinfo)
{
	GdkPixbuf *app_icon = NULL;
	GdkPixbuf *icon = NULL;
	GdkPixbuf *icon_tmp = NULL;
	GtkIconInfo *icon_info = NULL;
	GIcon *g_icon = NULL;
	GtkIconTheme *icon_theme = NULL;
	const char *app_name = g_app_info_get_name(appinfo);

	icon_tmp = gdk_pixbuf_new_from_file(g_strconcat(confdir, app_name, NULL), NULL);

	if (icon_tmp == NULL)
	{
		g_icon = g_app_info_get_icon(G_APP_INFO(appinfo));
		icon_theme = gtk_icon_theme_get_default();
		icon_info = gtk_icon_theme_lookup_by_gicon(icon_theme, g_icon, icon_size, GTK_ICON_LOOKUP_USE_BUILTIN);
		app_icon = gtk_icon_info_load_icon(icon_info, NULL);
		const char *icon_path = g_strconcat(confdir, app_name, NULL);
		icon_tmp = shadow_icon(app_icon, icon_path);
		icon = gdk_pixbuf_scale_simple(icon_tmp, icon_size, icon_size, GDK_INTERP_BILINEAR);
	}
	else
		icon = gdk_pixbuf_scale_simple(icon_tmp, icon_size, icon_size, GDK_INTERP_BILINEAR);

	if (app_icon)
		g_object_unref(G_OBJECT(app_icon));

	if (icon_tmp)
		g_object_unref(G_OBJECT(icon_tmp));

	if (icon_info)
		gtk_icon_info_free(icon_info);

	return icon;
}

static GdkPixbuf *lapps_label_lookup(GAppInfo *app)
{
	GdkPixbuf *label = NULL;
	const char *app_info_name = g_app_info_get_name(app);
	char *app_name = g_str_to_ascii(g_ascii_strdown(app_info_name, -1), NULL);

	label = (GdkPixbuf *) g_hash_table_lookup(labels_list, app_name);

	if (label == NULL)
	{
		label = create_app_name(app_info_name, font_size);
		g_hash_table_insert(labels_list, app_name, gdk_pixbuf_copy(label));
	}

	return label;
}

// create main table of applications
static GtkWidget *lapps_create_table()
{
	GtkWidget *app_box = NULL;
	GtkWidget *event_box = NULL;
	GtkWidget *app_label = NULL;
	GtkWidget *this_table = NULL;
	GtkWidget *icon = NULL;
	GList *test_list = NULL;
	char *bg_path = NULL;

	this_table = gtk_table_new(grid[0], grid[1], TRUE);
	gtk_table_set_row_spacings(GTK_TABLE(this_table), 50);
	gtk_table_set_col_spacings(GTK_TABLE(this_table), 40);

	int i = 0;
	int j = 0;

	for (test_list = g_list_nth(all_apps_list, page_last_position); test_list; test_list = test_list->next)
	{
		app_box = gtk_vbox_new(TRUE, 0);
		event_box = gtk_event_box_new();
		gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box), FALSE);
		gtk_container_add(GTK_CONTAINER(event_box), app_box);
		g_signal_connect(GTK_OBJECT(event_box), "button-press-event", G_CALLBACK(lapps_exec),
				(gpointer )test_list->data);
		g_signal_connect(GTK_OBJECT(event_box), "button-release-event", G_CALLBACK(lapps_main_window_close), NULL);
		bg_path = g_strconcat(confdir, g_app_info_get_name(test_list->data), NULL);
		if (g_file_test(bg_path, G_FILE_TEST_EXISTS))
			gtk_box_pack_start(GTK_BOX(app_box), gtk_image_new_from_file(bg_path), FALSE, FALSE, 0);
		else
			gtk_box_pack_start(GTK_BOX(app_box), gtk_image_new_from_pixbuf(lapps_application_icon(test_list->data)),
					FALSE, FALSE, 0);
		app_label = gtk_image_new_from_pixbuf(lapps_label_lookup(test_list->data));
		gtk_widget_set_size_request(app_label, app_label_width, app_label_height);
		gtk_box_pack_start(GTK_BOX(app_box), app_label, FALSE, FALSE, 0);
		gtk_table_attach(GTK_TABLE(this_table), event_box, j, j + 1, i, i + 1, GTK_SHRINK, GTK_FILL, 0, 0);
		if (j < grid[1] - 1)
		{
			j++;
		}
		else
		{
			j = 0;
			i++;
		}
		if (i == grid[0])
		{
			page_last_position = g_list_position(all_apps_list, test_list->next);
			break;
		}
	}

	g_free(bg_path);

	return this_table;
}

// create recent app bar
static GtkWidget *lapps_create_recent_frame(GList *recent_list)
{
	GtkWidget *app_box = NULL;
	GtkWidget *event_box = NULL;
	GtkImage *app_label = NULL;
	GtkWidget *apps_vbox = NULL;
	GtkWidget *icon = NULL;
	GList *test_list = NULL;
	char *bg_path = NULL;

	GtkWidget *recent_frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(recent_frame), GTK_SHADOW_IN);

	apps_vbox = gtk_vbox_new(TRUE, 0);

	GtkWidget *recent_frame_label = gtk_label_new(NULL);
	const char *str = "Recent Applications";
	const char *format = "<span foreground='white' size='%s'><b>\%s</b></span>";
	char *markup;
	markup = g_markup_printf_escaped(format, recent_label_font_size, str);
	gtk_label_set_markup(GTK_LABEL(recent_frame_label), markup);
	g_free(markup);
	gtk_box_pack_start(GTK_BOX(apps_vbox), recent_frame_label, TRUE, TRUE, 0);

	for (test_list = recent_list; test_list; test_list = test_list->next)
	{
		app_box = gtk_vbox_new(TRUE, 0);
		event_box = gtk_event_box_new();
		gtk_event_box_set_visible_window(GTK_EVENT_BOX(event_box), FALSE);
		gtk_container_add(GTK_CONTAINER(event_box), app_box);
		g_signal_connect(GTK_OBJECT(event_box), "button-press-event", G_CALLBACK(lapps_exec),
				(gpointer )test_list->data);
		g_signal_connect(GTK_OBJECT(event_box), "button-release-event", G_CALLBACK(lapps_main_window_close), NULL);
		bg_path = g_strconcat(confdir, g_app_info_get_name(test_list->data), NULL);
		if (g_file_test(bg_path, G_FILE_TEST_EXISTS))
			gtk_box_pack_start(GTK_BOX(app_box), gtk_image_new_from_file(bg_path), FALSE, FALSE, 0);
		else
			gtk_box_pack_start(GTK_BOX(app_box), gtk_image_new_from_pixbuf(lapps_application_icon(test_list->data)),
			FALSE, FALSE, 0);
		app_label = (GtkImage *) gtk_image_new_from_pixbuf(lapps_label_lookup(test_list->data));
		gtk_widget_set_size_request(GTK_WIDGET(app_label), app_label_width, app_label_height);
		gtk_box_pack_start(GTK_BOX(app_box), GTK_WIDGET(app_label), FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(apps_vbox), event_box, TRUE, TRUE, 0);
	}

	gtk_container_add(GTK_CONTAINER(recent_frame), apps_vbox);

	g_free(bg_path);

	return recent_frame;
}

// remove duplicates
static void lapps_app_list_remove_dup()
{
	GList *test_list;
	const char *app_info_name;
	char *app_name_old;

	int item_position = 0;
	for (test_list = all_apps_list; test_list; test_list = test_list->next)
	{
		app_info_name = g_app_info_get_name(test_list->data);
		if (g_strcmp0(app_info_name, app_name_old) == 0)
		{
			item_position = g_list_position(all_apps_list, test_list);
			GList *item = g_list_nth(all_apps_list, item_position);
			all_apps_list = g_list_remove_link(all_apps_list, item);
			g_list_free_1(item);
			app_count--;
		}
		app_name_old = g_strdup(app_info_name);
	}

	g_free(app_name_old);
}

// list the applications to then create the main table and recent application bar
static void lapps_app_list(const char *filter)
{
	GList *test_list = NULL;
	const char *app_info_name;
	const char *app_info_description;
	const char *app_info_id;
	char *name;
	char *description;
	char *id;
	const char *filter_down;

	system_apps_list = g_app_info_get_all();
	all_apps_list = NULL;

	app_count = 0;

	if (filtered && strlen(filter) > 0)
	{
		filter_down = g_str_to_ascii(g_ascii_strdown(filter, -1), NULL);

		for (test_list = system_apps_list; test_list; test_list = test_list->next)
		{
			if (g_app_info_get_icon(test_list->data) && g_app_info_get_description(test_list->data)
							&& g_app_info_should_show(test_list->data))
			{
				app_info_name = g_app_info_get_name(test_list->data);
				name = g_str_to_ascii(g_ascii_strdown(app_info_name, -1), NULL);
				app_info_description = g_app_info_get_description(test_list->data);
				description = g_str_to_ascii(g_ascii_strdown(app_info_description, -1), NULL);
				app_info_id = g_app_info_get_id(test_list->data);
				id = g_str_to_ascii(g_ascii_strdown(app_info_id, -1), NULL);
				if (g_strrstr(name, filter_down) || g_strrstr(description, filter_down) || g_strrstr(id, filter_down))
				{
					all_apps_list = g_list_insert_sorted(all_apps_list, test_list->data,
									(GCompareFunc) app_name_comparator);
					app_count++;
				}
			}
		}
	}

	if (!filtered)
	{
		for (test_list = system_apps_list; test_list; test_list = test_list->next)
		{
			if (g_app_info_get_icon(test_list->data) && g_app_info_get_description(test_list->data)
							&& g_app_info_should_show(test_list->data))
			{
				all_apps_list = g_list_insert_sorted(all_apps_list, test_list->data,
								(GCompareFunc) app_name_comparator);
				app_count++;
			}
		}
	}

	// remove duplicates
	if (app_count > 1)
		lapps_app_list_remove_dup();
}

// list the applications to then create the main table with app selected
static void lapps_app_selected_list(const char *filter)
{
	GList *test_list = NULL;
	const char *app_name;

	all_apps_list = NULL;

	app_count = 0;

	if (strlen(filter) > 0)
	{
		for (test_list = system_apps_list; test_list; test_list = test_list->next)
		{
			if (g_app_info_get_icon(test_list->data) && g_app_info_get_description(test_list->data)
							&& g_app_info_should_show(test_list->data))
			{
				app_name = g_app_info_get_name(test_list->data);
				if (g_strcmp0(g_str_to_ascii(g_ascii_strdown(app_name, -1), NULL),
								g_str_to_ascii(g_ascii_strdown(filter, -1), NULL)) == 0)
				{
					all_apps_list = g_list_prepend(all_apps_list, test_list->data);
					app_count++;
				}
			}
		}
		// remove duplicates
		if (app_count > 1)
			lapps_app_list_remove_dup();
	}
}

// calculates the number of pages
static int lapps_pages()
{
	int total_page_itens = 0;
	int pages = 0;

	total_page_itens = grid[0] * grid[1];

	pages = app_count / total_page_itens;

	if (app_count % total_page_itens > 0)
		pages++;

	return pages;
}

static void lapps_update_indicator_rw(gboolean border)
{
	char page_char[4];
	int page = page_index;

	if (GTK_IMAGE(indicator_rw))
		gtk_image_clear(GTK_IMAGE(indicator_rw));

	if (indicator_rw_pix == NULL)
		indicator_rw_pix = gdk_pixbuf_new_from_file(g_strconcat(IMAGEPATH, "go-previous.png", NULL), NULL);

	gtk_image_set_from_pixbuf(GTK_IMAGE(indicator_rw), indicator_rw_pix);

	if (page < lapps_pages())
		page++;

	sprintf(page_char, "%d", page);

	if (GTK_IMAGE(indicator))
		gtk_image_set_from_pixbuf(GTK_IMAGE(indicator), create_app_name(page_char, indicator_font_size));

	if (page > 1)
	{
		if (GTK_IMAGE(indicator_rw) && border)
		{
			if (indicator_rw_shaded_pix == NULL)
				indicator_rw_shaded_pix = shadow_icon(gtk_image_get_pixbuf(GTK_IMAGE(indicator_rw)), NULL);

			gtk_image_set_from_pixbuf(GTK_IMAGE(indicator_rw), indicator_rw_shaded_pix);
		}
	}
	else
	{
		if (GTK_IMAGE(indicator_rw))
			gtk_image_clear(GTK_IMAGE(indicator_rw));
	}
}

static void lapps_update_indicator_fw(gboolean border)
{
	char page_char[4];
	int page = page_index;

	if (GTK_IMAGE(indicator_fw))
		gtk_image_clear(GTK_IMAGE(indicator_fw));

	if (indicator_fw_pix == NULL)
		indicator_fw_pix = gdk_pixbuf_new_from_file(g_strconcat(IMAGEPATH, "go-next.png", NULL), NULL);

	gtk_image_set_from_pixbuf(GTK_IMAGE(indicator_fw), indicator_fw_pix);

	if (page < lapps_pages())
		page++;

	sprintf(page_char, "%d", page);

	if (page < lapps_pages())
	{
		if (GTK_IMAGE(indicator_fw) && border)
		{
			if (indicator_fw_shaded_pix == NULL)
				indicator_fw_shaded_pix = shadow_icon(gtk_image_get_pixbuf(GTK_IMAGE(indicator_fw)), NULL);

			gtk_image_set_from_pixbuf(GTK_IMAGE(indicator_fw), indicator_fw_shaded_pix);
		}
	}
	else
	{
		if (GTK_IMAGE(indicator_fw))
			gtk_image_clear(GTK_IMAGE(indicator_fw));
	}
}

static void lapps_indicator_rw_hover(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	if (event->type == GDK_ENTER_NOTIFY || event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE)
		lapps_update_indicator_rw(TRUE);
	else
		lapps_update_indicator_rw(FALSE);
}

static void lapps_indicator_fw_hover(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	if (event->type == GDK_ENTER_NOTIFY || event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE)
		lapps_update_indicator_fw(TRUE);
	else
		lapps_update_indicator_fw(FALSE);
}

static void lapps_clear()
{
	GList *test_list = NULL;

	gtk_widget_destroy(GTK_WIDGET(page_table));
	page_table = NULL;

	for (test_list = pages_list; test_list; test_list = test_list->next)
		gtk_widget_destroy(GTK_WIDGET(test_list->data));

	pages_list = NULL;

	gtk_widget_draw(GTK_WIDGET(main_window), NULL);
}

static void lapps_show_page(gboolean up)
{
	GList *test_list = NULL;

	int i = 0;

	if (filtered)
	{
		filtered = FALSE;
		page_last_position = 0;
		lapps_clear();
	}

	if (up && page_count < lapps_pages())
	{
		page_count++;
		page_table = lapps_create_table();
		pages_list = g_list_append(pages_list, page_table);
		gtk_fixed_put(GTK_FIXED(layout_fixed), page_table, 0, 0);
	}

	for (test_list = pages_list; test_list; test_list = test_list->next)
	{
		gtk_widget_hide_all(test_list->data);
		if (i == page_index)
			gtk_widget_show_all(test_list->data);
		i++;
	}

	lapps_update_indicator_rw(FALSE);
	lapps_update_indicator_fw(FALSE);

	gtk_widget_draw(main_window, NULL);
}

static gboolean lapps_on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	switch (event->keyval)
	{
	case GDK_KEY_Down:
		(page_index > 0) ? page_index-- : 0;
		(lapps_pages() > 1) ? lapps_show_page(FALSE) : 0;
		break;

	case GDK_KEY_Up:
		(page_index < lapps_pages()) ? page_index++ : 0;
		(page_index < lapps_pages()) ? lapps_show_page(TRUE) : 0;
		break;

	case GDK_Escape:
		lapps_main_window_close(NULL, NULL, NULL);
		break;

	default:
		return FALSE;
	}

	return FALSE;
}

static gboolean lapps_on_mouse_scroll(GtkWidget *widget, GdkEventScroll *event, gpointer user_data)
{
	switch (event->direction)
	{
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

static void lapps_indicator_rw_clicked(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	(page_index > 0) ? page_index-- : 0;
	(lapps_pages() > 1) ? lapps_show_page(FALSE) : 0;
}

static void lapps_indicator_fw_clicked(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	(page_index < lapps_pages()) ? page_index++ : 0;
	(page_index < lapps_pages()) ? lapps_show_page(TRUE) : 0;
}

static void lapps_show_default_page(gboolean app_not_found, GtkEntry *entry)
{
	if (app_not_found)
		gtk_entry_set_icon_from_stock(GTK_ENTRY(entry), GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_STOP);
	else
		gtk_entry_set_icon_from_stock(GTK_ENTRY(entry), GTK_ENTRY_ICON_PRIMARY, NULL);
	page_last_position = 0;
	filtered = FALSE;
	lapps_clear();
	lapps_app_list(NULL);
	lapps_show_page(TRUE);
}

static void lapps_search(GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEventButton *event, gpointer userdata)
{
	const char *filter = gtk_entry_get_text(entry);

	page_index = 0;
	page_count = 0;

	if (GTK_ENTRY_ICON_SECONDARY == icon_pos)
	{
		if (strlen(filter) > 0)
		{
			filtered = TRUE;
			gtk_entry_set_icon_from_stock(GTK_ENTRY(entry), GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_CLEAR);
			lapps_app_list(filter);
			if (g_list_length(all_apps_list) > 0)
				lapps_show_page(TRUE);
			else
				lapps_show_default_page(TRUE, entry);
		}
		else
			lapps_show_default_page(FALSE, entry);
	}

	if (GTK_ENTRY_ICON_PRIMARY == icon_pos)
	{
		gtk_entry_set_text(entry, "");
		lapps_show_default_page(FALSE, entry);
	}
}

static void lapps_search_(GtkEntry *entry, gpointer userdata)
{
	const char *filter = gtk_entry_get_text(entry);

	page_index = 0;
	page_count = 0;

	if (strlen(filter) > 0)
	{
		filtered = TRUE;
		gtk_entry_set_icon_from_stock(GTK_ENTRY(entry), GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_CLEAR);
		lapps_app_list(filter);
		if (g_list_length(all_apps_list) > 0)
			lapps_show_page(TRUE);
		else
			lapps_show_default_page(TRUE, entry);
	}
	else
	{
		gtk_entry_set_text(entry, "");
		lapps_show_default_page(FALSE, entry);
	}
}

static void lapps_search_selected(const char *filter, GtkEntry *entry)
{
	page_index = 0;
	page_count = 0;

	if (strlen(filter) > 0)
	{
		filtered = TRUE;
		gtk_entry_set_icon_from_stock(GTK_ENTRY(entry), GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_CLEAR);
		lapps_app_selected_list(filter);
		lapps_show_page(TRUE);
	}
}

static gboolean lapps_match_completion(GtkEntryCompletion *completion, const char *key, GtkTreeIter *iter,
				gpointer user_data)
{
	GtkTreeModel *model = gtk_entry_completion_get_model(completion);
	const char *name;
	const char *description;
	const char *id;
	gtk_tree_model_get(model, iter, 0, &name, 1, &description, 2, &id, -1);
	const char *tofind = g_str_to_ascii(g_ascii_strdown(key, -1), NULL);
	gboolean match = (g_strrstr(g_str_to_ascii(g_ascii_strdown(name, -1), NULL), tofind) == NULL ? FALSE : TRUE)
					|| (g_strrstr(g_str_to_ascii(g_ascii_strdown(description, -1), NULL), tofind) == NULL ? FALSE : TRUE)
					|| (g_strrstr(g_str_to_ascii(g_ascii_strdown(id, -1), NULL), tofind) == NULL ? FALSE : TRUE);
	return match;
}

static gboolean lapps_match_completion_selected(GtkEntryCompletion *completion, GtkTreeModel *model, GtkTreeIter *iter,
				gpointer user_data)
{
	GtkEntry *entry = (GtkEntry *) user_data;
	const char *name;
	const char *description;
	const char *id;
	gtk_tree_model_get(model, iter, 0, &name, 1, &description, 2, &id, -1);
	lapps_search_selected(name, entry);

	return FALSE;
}

static void lapps_create_main_window()
{
	page_table = NULL;
	layout_fixed = NULL;
	indicator = NULL;
	indicator_fw = NULL;
	indicator_rw = NULL;

	pages_list = NULL;

	// populates the global list of applications
	lapps_app_list(NULL);

	// main window
	main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_type_hint(GTK_WINDOW(main_window), GDK_WINDOW_TYPE_HINT_NORMAL);
	gtk_window_fullscreen(GTK_WINDOW(main_window));
	gtk_window_stick(GTK_WINDOW(main_window));
	gtk_window_set_keep_above(GTK_WINDOW(main_window), TRUE);
	gtk_window_set_title(GTK_WINDOW(main_window), LAPPSNAME);
	gtk_widget_set_app_paintable(main_window, TRUE);
	gtk_window_set_decorated(GTK_WINDOW(main_window), FALSE);
	gtk_window_set_icon_name(GTK_WINDOW(main_window), LAPPSICON);
	gtk_widget_add_events(main_window, GDK_BUTTON_PRESS_MASK);
	gtk_widget_add_events(main_window, GDK_KEY_RELEASE_MASK);
	g_signal_connect(GTK_OBJECT (main_window), "button-press-event", G_CALLBACK (lapps_main_window_close), NULL);
	g_signal_connect(GTK_OBJECT (main_window), "key-release-event", G_CALLBACK (lapps_on_key_press), NULL);
	g_signal_connect(GTK_OBJECT (main_window), "scroll-event", G_CALLBACK(lapps_on_mouse_scroll), NULL);
	gtk_widget_show(main_window);

	// window background
	GtkWidget *layout = gtk_layout_new(NULL, NULL);
	gtk_layout_set_size(GTK_LAYOUT(layout), s_width, s_height);
	gtk_container_add(GTK_CONTAINER(main_window), layout);
	gtk_widget_show(layout);
	GtkWidget *bg_image = gtk_image_new_from_file(bg_image_path);
	gtk_layout_put(GTK_LAYOUT(layout), bg_image, 0, 0);
	gtk_widget_show(bg_image);

	// main vbox for applications
	GtkWidget *main_vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_set_size_request(main_vbox, s_width, s_height);
	gtk_container_set_border_width(GTK_CONTAINER(main_vbox), main_vbox_border_width);
	gtk_container_add(GTK_CONTAINER(layout), main_vbox);
	gtk_widget_show(main_vbox);

	// search entry
	GtkWidget *entry = gtk_entry_new();
	gtk_entry_set_icon_from_stock(GTK_ENTRY(entry), GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_FIND);
	g_signal_connect(GTK_OBJECT (entry), "icon-press", G_CALLBACK (lapps_search), NULL);
	g_signal_connect(GTK_OBJECT (entry), "activate", G_CALLBACK (lapps_search_), NULL);
	gtk_widget_set_size_request(entry, 250, 30);
	GtkWidget *entry_align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
	gtk_container_add(GTK_CONTAINER(entry_align), entry);
	gtk_box_pack_start(GTK_BOX(main_vbox), entry_align, FALSE, FALSE, 0);
	gtk_widget_show_all(entry_align);
	gtk_widget_grab_focus(entry);

	page_index = 0;
	page_count = 0;
	page_last_position = 0;
	filtered = FALSE;

	// completion ******************************************************************
	GList *test_list = NULL;
	GtkEntryCompletion *completion = gtk_entry_completion_new();
	gtk_entry_completion_set_minimum_key_length(completion, 2);
	gtk_entry_completion_set_popup_set_width(completion, FALSE);
	GtkListStore *store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	for (test_list = all_apps_list; test_list; test_list = test_list->next)
	{
		GtkTreeIter it;
		gtk_list_store_append(store, &it);
		gtk_list_store_set(store, &it, 0, g_app_info_get_name(test_list->data), 1,
						g_app_info_get_description(test_list->data), 2, g_app_info_get_id(test_list->data), -1);
	}
	gtk_entry_completion_set_model(completion, (GtkTreeModel *) store);
	gtk_entry_completion_set_match_func(completion, (GtkEntryCompletionMatchFunc) lapps_match_completion, NULL, NULL);
	g_signal_connect(GTK_ENTRY_COMPLETION (completion), "match-selected", G_CALLBACK (lapps_match_completion_selected),
					(gpointer )entry);
	g_object_unref(G_OBJECT(store));
	gtk_entry_completion_set_text_column(completion, 0);
	gtk_entry_set_completion(GTK_ENTRY(entry), completion);
	gtk_entry_completion_complete(completion);
	g_object_unref(G_OBJECT(completion));

	// recent applications and table ***********************************************
	GList *recent_list = NULL;
	GList *test_recent_list = NULL;
	recent_tmp = NULL;
	lapps_loadsave_recent(TRUE);
	if (recent_tmp && g_list_length(recent_tmp) > 0)
	{
		for (test_recent_list = recent_tmp; test_recent_list; test_recent_list = test_recent_list->next)
		{
			for (test_list = all_apps_list; test_list; test_list = test_list->next)
			{
				if (g_strcmp0(g_app_info_get_name(test_list->data), test_recent_list->data) == 0)
				{
					recent_list = g_list_append(recent_list, test_list->data);
					break;
				}
			}
		}
	}
	GtkWidget *apps_hbox = gtk_hbox_new(FALSE, screen_size_relation);
	GtkWidget *frame_vbox = gtk_vbox_new(FALSE, 0);
	GtkWidget *frame = lapps_create_recent_frame(recent_list);
	GtkWidget *apps_hbox_align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
	GtkWidget *table_align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
	layout_fixed = gtk_fixed_new();
	gtk_container_add(GTK_CONTAINER(table_align), layout_fixed);
	if (g_list_length(recent_list) > 0)
	{
		gtk_widget_set_size_request(GTK_WIDGET(frame_vbox), app_label_width, apps_table_height);
		gtk_box_pack_start(GTK_BOX(frame_vbox), frame, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(apps_hbox), frame_vbox, FALSE, FALSE, 0);
	}
	else
		gtk_widget_set_size_request(GTK_WIDGET(table_align), apps_table_width, apps_table_height);
	gtk_box_pack_start(GTK_BOX(apps_hbox), table_align, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(apps_hbox_align), apps_hbox);
	gtk_box_pack_start(GTK_BOX(main_vbox), apps_hbox_align, FALSE, FALSE, 0);
	gtk_widget_show_all(apps_hbox_align);
	lapps_show_page(TRUE);
	g_list_free(recent_list);

	// indicators
	GtkWidget *indicators_hbox = gtk_hbox_new(FALSE, 0);
	GtkWidget *indicators_align = gtk_alignment_new(0.5, 0.5, 0, 0);
	GtkWidget *indicator_event_box = NULL;
	indicator_rw = gtk_image_new();
	gtk_widget_set_size_request(indicator_rw, indicator_width, indicator_height);
	indicator_event_box = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(indicator_event_box), indicator_rw);
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(indicator_event_box), FALSE);
	gtk_box_pack_start(GTK_BOX(indicators_hbox), indicator_event_box, FALSE, FALSE, 0);
	g_signal_connect(GTK_OBJECT(indicator_event_box), "button-press-event", G_CALLBACK(lapps_indicator_rw_clicked),
					NULL);
	g_signal_connect(GTK_OBJECT(indicator_event_box), "enter-notify-event", G_CALLBACK(lapps_indicator_rw_hover), NULL);
	g_signal_connect(GTK_OBJECT(indicator_event_box), "leave-notify-event", G_CALLBACK(lapps_indicator_rw_hover), NULL);
	g_signal_connect(GTK_OBJECT(indicator_event_box), "button-release-event", G_CALLBACK(lapps_indicator_rw_hover),
					NULL);
	indicator = gtk_image_new();
	gtk_widget_set_size_request(indicator, indicator_width, indicator_height);
	gtk_box_pack_start(GTK_BOX(indicators_hbox), indicator, FALSE, FALSE, 0);
	indicator_fw = gtk_image_new();
	gtk_widget_set_size_request(indicator_fw, indicator_width, indicator_height);
	indicator_event_box = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(indicator_event_box), indicator_fw);
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(indicator_event_box), FALSE);
	gtk_box_pack_start(GTK_BOX(indicators_hbox), indicator_event_box, FALSE, FALSE, 0);
	g_signal_connect(GTK_OBJECT(indicator_event_box), "button-press-event", G_CALLBACK(lapps_indicator_fw_clicked),
					NULL);
	g_signal_connect(GTK_OBJECT(indicator_event_box), "enter-notify-event", G_CALLBACK(lapps_indicator_fw_hover), NULL);
	g_signal_connect(GTK_OBJECT(indicator_event_box), "leave-notify-event", G_CALLBACK(lapps_indicator_fw_hover), NULL);
	g_signal_connect(GTK_OBJECT(indicator_event_box), "button-release-event", G_CALLBACK(lapps_indicator_fw_hover),
					NULL);
	gtk_container_add(GTK_CONTAINER(indicators_align), indicators_hbox);
	gtk_box_pack_start(GTK_BOX(main_vbox), indicators_align, FALSE, FALSE, 0);
	gtk_widget_show_all(indicators_align);
	lapps_update_indicator_rw(FALSE);
	lapps_update_indicator_fw(FALSE);
}

// load first page at plugin startup
static void lapps_tables_init()
{
	GList *test_list = NULL;

	filtered = FALSE;
	lapps_app_list(NULL);
	labels_list = g_hash_table_new(g_str_hash, g_str_equal);

	int i = 0;
	for (test_list = all_apps_list; test_list; test_list = test_list->next)
	{
		lapps_label_lookup(test_list->data);
		i++;
		if (i == (grid[0] * grid[1]))
			break;
	}
}

static gboolean lapps_button_clicked(GtkWidget *widget, GdkEventButton *event, LXPanel *panel)
{
	if (event->button != 1)
		return FALSE;

	if (main_window == NULL)
		lapps_create_main_window();

	return TRUE;
}

static void lapps_destructor(gpointer user_data)
{
	LaunchAppsPlugin *lapps = (LaunchAppsPlugin *) user_data;
	if(labels_list != NULL)
		g_hash_table_destroy(labels_list);
	if(bg_image_path != NULL)
		g_free(bg_image_path);
	if(lapps->image_path != NULL)
		g_free(lapps->image_path);
	if(lapps->image_path_test != NULL)
		g_free(lapps->image_path_test);
	if(lapps->version != NULL)
		g_free(lapps->version);
	g_free(lapps);
}

/* Callback when the configuration dialog has recorded a configuration change. */
static gboolean lapps_apply_configuration(gpointer user_data)
{
	GtkWidget *p = user_data;
	LaunchAppsPlugin *lapps = lxpanel_plugin_get_data(p);
	const char *bg_path = g_strconcat(confdir, BG, NULL);
	const char *bg_default_path = g_strconcat(IMAGEPATH, DEFAULTBG, NULL);

	// check background image
	if (!g_file_test(bg_path, G_FILE_TEST_EXISTS))
		blur_background(bg_default_path, bg_path);

	bg_image_path = g_strdup(bg_path);

	if (g_strcmp0(lapps->image_path_test, lapps->image_path) != 0)
	{
		if (lapps->image_path == NULL)
		{
			blur_background(bg_default_path, bg_path);
			lapps->image_path_test = NULL;
		}
		else
		{
			blur_background(lapps->image_path, bg_path);
			lapps->image_path_test = g_strdup(lapps->image_path);
		}
	}

	config_group_set_string(lapps->settings, "image_path", lapps->image_path);

	return TRUE;
}

/* Callback when the configuration dialog is to be shown. */
static GtkWidget *lapps_configure(LXPanel *panel, GtkWidget *p)
{
	LaunchAppsPlugin *lapps = lxpanel_plugin_get_data(p);
	return lxpanel_generic_config_dlg("Application Launcher", panel, lapps_apply_configuration, p,
			lapps->version, NULL, CONF_TYPE_TRIM,
			" Application Launcher ", NULL, CONF_TYPE_TRIM,
			"Copyright (C) 2018", NULL, CONF_TYPE_TRIM,
			" ", NULL, CONF_TYPE_TRIM,
			"Background image", &lapps->image_path, CONF_TYPE_FILE_ENTRY,
			NULL);
}

static GtkWidget *lapps_constructor(LXPanel *panel, config_setting_t *settings)
{
	LaunchAppsPlugin *lapps = g_new0(LaunchAppsPlugin, 1);
	GtkWidget *p;
	GtkWidget *icon_box;
	const char *str;

	confdir = g_strconcat(fm_get_home_dir(), CONFPATH, NULL);

	/* Load parameters from the configuration file. */
	if (config_setting_lookup_string(settings, "image_path", &str))
		lapps->image_path = g_strdup(str);

	if (lapps->image_path == NULL)
		lapps->image_path_test = NULL;
	else
		lapps->image_path_test = g_strdup(lapps->image_path);

	lapps->panel = panel;
	lapps->settings = settings;

	g_return_val_if_fail(lapps != NULL, 0);

	p = panel_icon_grid_new(panel_get_orientation(panel), panel_get_icon_size(panel), panel_get_icon_size(panel), 1, 0,
					panel_get_height(panel));

	lxpanel_plugin_set_data(p, lapps, lapps_destructor);

	icon_box = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(p), icon_box);
	gtk_widget_show(icon_box);

	lapps->icon_image = gtk_image_new_from_icon_name(LAPPSICON, lapps->panel->priv->icon_size);
	gtk_widget_set_tooltip_text(lapps->icon_image, "Click to find and run any application");

	gtk_container_add(GTK_CONTAINER(icon_box), lapps->icon_image);
	gtk_widget_show(lapps->icon_image);

	lapps->version = g_strconcat("LaunchApps ", VERSION, NULL);

	// check configuration diretory
	if (!g_file_test(confdir, G_FILE_TEST_EXISTS & G_FILE_TEST_IS_DIR))
		g_mkdir(confdir, 0700);

	set_icons_fonts_sizes();
	lapps_apply_configuration(p);

	// load first page at plugin startup
	lapps_tables_init();

	return p;
}

FM_DEFINE_MODULE(lxpanel_gtk, launchapps);

LXPanelPluginInit fm_module_init_lxpanel_gtk = {
		.name = "Application Launcher",
		.description = "Application Launcher for LXPanel",
		.new_instance = lapps_constructor,
		.config = lapps_configure,
		.button_press_event = lapps_button_clicked,
		.one_per_system = 1
};
