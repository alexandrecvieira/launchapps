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

int icon_size, font_size, app_label_width, app_label_height;
int indicator_font_size, indicator_width, indicator_height;
int s_height, s_width, grid[2];
double screen_size_relation;
char *recent_label_font_size;

gboolean blur_background(const char *image, const char *bg_image) {
	MagickWand *inWand = NULL;
	MagickWand *outWand = NULL;
	MagickBooleanType status;

	MagickWandGenesis();
	inWand = NewMagickWand();
	status = MagickReadImage(inWand, image);

	if (status == MagickFalse) {
		inWand = DestroyMagickWand(inWand);
		MagickWandTerminus();
		return FALSE;
	}

	outWand = CloneMagickWand(inWand);
	MagickBlurImage(outWand, 0, 15);
	MagickSetImageDepth(outWand, 32);

	MagickWriteImage(outWand, bg_image);

	if (inWand)
		inWand = DestroyMagickWand(inWand);
	if (outWand)
		outWand = DestroyMagickWand(outWand);

	MagickWandTerminus();

	return TRUE;
}

GdkPixbuf *create_app_name(const char *app_name, double font_size) {
	GdkPixbuf *bg_target_pix = NULL;
	MagickWand *magick_wand = NULL;
	MagickWand *c_wand = NULL;
	DrawingWand *d_wand = NULL;
	PixelWand *p_wand = NULL;
	size_t width, height;
	int rowstride, row;
	guchar *pixels = NULL;
	char *target_name = NULL;

	char **app_name_splited = g_strsplit(app_name, " ", -1);
	char **ptr = NULL;
	int i = 1;
	for (ptr = app_name_splited; *ptr; ptr++) {
		if (i == 1)
			target_name = g_strconcat(*ptr, " ", NULL);
		if (i > 1 && (i % 2) == 0)
			target_name = g_strconcat(target_name, *ptr, "\n ", NULL);
		if (i > 1 && (i % 2) > 0)
			target_name = g_strconcat(target_name, *ptr, " ", NULL);
		i++;
	}

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

	g_free(target_name);

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

GdkPixbuf *shadow_icon(GdkPixbuf *src_pix, const char *path) {
	GdkPixbuf *bg_target_pix = NULL;
	size_t width, height;
	int rowstride, row;
	guchar *pixels = NULL;
	char *buffer = NULL;
	gsize buffer_size;
	MagickWand *src_wand = NULL;
	MagickWand *dest_wand = NULL;

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

	if(path != NULL)
		MagickWriteImage(dest_wand, path);

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

void set_icons_fonts_sizes() {
	GdkScreen *screen = gdk_screen_get_default();
	s_height = gdk_screen_get_height(screen);
	s_width = gdk_screen_get_width(screen);
	screen_size_relation = (pow(s_width * s_height, ((double) (1.0 / 3.0))) / 1.6);

	/*openlog("LaunchApps", LOG_PID | LOG_CONS, LOG_USER);
	syslog(LOG_INFO, "Screen relation: w-%d x h-%d -> %f", s_width, s_height, screen_size_relation);
	closelog();*/

	// common screen size resolution = suggested_size (s_width / s_height)
	// 1024x768 = 57.689983 (~1.33) | 1280x800 = 62.996052 (1.6)   | 1280x1024 = 68.399038 (~1.24) | 1366x768  = 63.506375 (~1.77)
	// 1440x900 = 68.142022 (1.6)   | 1600x900 = 70.577702 (~1.77) | 1680x1050 = 75.517258 (1.6)   | 1920x1080 = 79.699393 (~1.77)

	if (screen_size_relation < 68) {
		icon_size = 48;
		font_size = 12;
		recent_label_font_size = "x-small";
	} else if (screen_size_relation >= 68 && screen_size_relation < 79) {
		icon_size = 48;
		font_size = 14;
		recent_label_font_size = "small";
	} else if (screen_size_relation >= 79) {
		icon_size = 64;
		font_size = 16;
		recent_label_font_size = "medium";
	}

	if(screen_size_relation < 75)
		app_label_height = 58;
	else
		app_label_height = 68;

	if (screen_size_relation < 62)
		app_label_width = 120;
	else if (screen_size_relation >= 62 && screen_size_relation < 79)
		app_label_width = 160;
	else if (screen_size_relation >= 79)
		app_label_width = 250;

	indicator_font_size = font_size * 2;
	indicator_width = font_size * 2;
	indicator_height = (font_size * 2) + (font_size / 2);

	/* grid[0] = rows | grid[1] = columns */
	if (s_width > s_height) { // normal landscape orientation
		grid[0] = 4;
		grid[1] = 5;
	} else { // most likely a portrait orientation
		grid[0] = 5;
		grid[1] = 4;
	}
}

int app_name_comparator(GAppInfo *item1, GAppInfo *item2) {
	return g_ascii_strcasecmp(g_app_info_get_name(item1), g_app_info_get_name(item2));
}

