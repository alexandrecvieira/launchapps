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

#ifndef LAPPSUTIL_H
#define LAPPSUTIL_H

#include <string.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>

#include <syslog.h>

#include <wand/MagickWand.h>

extern int icon_size, s_height, s_width, grid[2];

void set_icons_size();
gboolean blur_background(gchar *image, gchar *bg_image);
GdkPixbuf *create_app_name(gchar *app_name, double font_size);
GdkPixbuf *shadow_icon(GdkPixbuf *src_pix);
gint app_name_comparator(GAppInfo *item1, GAppInfo *item2);

#endif /* LAPPSUTIL_H */
