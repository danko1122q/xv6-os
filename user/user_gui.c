#include "stat.h"
#include "types.h"
#include "character.h"
#include "fcntl.h"
#include "fs.h"
#include "gui.h"
#include "icons.h"
#include "msg.h"
#include "user.h"
#include "user_window.h"

int min(int x, int y) { return x < y ? x : y; }
int max(int x, int y) { return x > y ? x : y; }

void drawPoint(RGB *color, RGB origin) {
	color->R = origin.R;
	color->G = origin.G;
	color->B = origin.B;
}

void drawPointAlpha(RGB *color, RGBA origin) {
	uint alpha, inv_alpha;

	if (origin.A == 255) {
		color->R = origin.R;
		color->G = origin.G;
		color->B = origin.B;
		return;
	}
	if (origin.A == 0) {
		return;
	}

	alpha = origin.A;
	inv_alpha = 255 - alpha;

	color->R = (color->R * inv_alpha + origin.R * alpha) >> 8;
	color->G = (color->G * inv_alpha + origin.G * alpha) >> 8;
	color->B = (color->B * inv_alpha + origin.B * alpha) >> 8;
}

void fillRect(RGB *buf, int x, int y, int width, int height, int max_x,
	      int max_y, RGBA fill) {
	int i, j;
	RGB *t;

	if (x >= max_x || y >= max_y || x + width <= 0 || y + height <= 0)
		return;

	int start_x = x < 0 ? 0 : x;
	int start_y = y < 0 ? 0 : y;
	int end_x = (x + width > max_x) ? max_x : x + width;
	int end_y = (y + height > max_y) ? max_y : y + height;

	if (fill.A == 255) {
		RGB solid_color;
		solid_color.R = fill.R;
		solid_color.G = fill.G;
		solid_color.B = fill.B;

		for (i = start_y; i < end_y; i++) {
			t = buf + i * max_x + start_x;
			for (j = start_x; j < end_x; j++) {
				*t++ = solid_color;
			}
		}
		return;
	}

	for (i = start_y; i < end_y; i++) {
		for (j = start_x; j < end_x; j++) {
			t = buf + i * max_x + j;
			drawPointAlpha(t, fill);
		}
	}
}

int drawCharacter(RGB *buf, int x, int y, char ch, RGBA color, int win_width,
		  int win_height) {
	int i, j;
	RGB *t;
	int ord = ch - 0x20;

	if (ord < 0 || ord >= (CHARACTER_NUMBER - 1))
		return -1;

	if (x >= win_width || y >= win_height || x + CHARACTER_WIDTH <= 0 ||
	    y + CHARACTER_HEIGHT <= 0)
		return CHARACTER_WIDTH;

	for (i = 0; i < CHARACTER_HEIGHT; i++) {
		if (y + i >= win_height || y + i < 0)
			continue;

		for (j = 0; j < CHARACTER_WIDTH; j++) {
			if (x + j >= win_width || x + j < 0)
				continue;

			uchar font_alpha = character[ord][i][j];

			if (font_alpha > 0) {
				t = buf + (y + i) * win_width + (x + j);

				RGBA smooth_color = color;
				smooth_color.A = (color.A * font_alpha) >> 8;

				drawPointAlpha(t, smooth_color);
			}
		}
	}
	return CHARACTER_WIDTH;
}

void drawString(window *win, char *str, RGBA color, int x, int y, int width,
		int height) {
	int offset_x = 0;
	int offset_y = 0;

	while (*str != '\0') {
		if (offset_y + CHARACTER_HEIGHT > height)
			break;

		if (*str != '\n') {
			if (offset_x + CHARACTER_WIDTH <= width) {
				drawCharacter(win->window_buf, x + offset_x,
					      y + offset_y, *str, color,
					      win->width, win->height);
			}

			offset_x += CHARACTER_WIDTH;

			if (offset_x + CHARACTER_WIDTH > width) {
				offset_x = 0;
				offset_y += CHARACTER_HEIGHT;
			}
		} else {
			offset_x = 0;
			offset_y += CHARACTER_HEIGHT;
		}

		str++;
	}
}

void drawImage(window *win, RGBA *img, int x, int y, int width, int height) {
	int i, j;
	RGB *t;
	RGBA *o;

	if (x >= win->width || y >= win->height || x + width <= 0 ||
	    y + height <= 0)
		return;

	int start_y = (y < 0) ? -y : 0;
	int start_x = (x < 0) ? -x : 0;
	int end_y = (y + height > win->height) ? win->height - y : height;
	int end_x = (x + width > win->width) ? win->width - x : width;

	for (i = start_y; i < end_y; i++) {
		for (j = start_x; j < end_x; j++) {
			t = win->window_buf + (y + i) * win->width + (x + j);
			o = img + (height - i - 1) * width + j;
			drawPointAlpha(t, *o);
		}
	}
}

void draw24Image(window *win, RGB *img, int x, int y, int width, int height) {
	int i;
	RGB *t;
	RGB *o;

	if (x >= win->width || y >= win->height || x < 0 || y < 0)
		return;

	int max_line = (win->width - x) < width ? (win->width - x) : width;

	for (i = 0; i < height; i++) {
		if (y + i >= win->height) {
			break;
		}
		if (y + i < 0) {
			continue;
		}
		t = win->window_buf + (y + i) * win->width + x;
		o = img + (height - i - 1) * width;
		memmove(t, o, max_line * 3);
	}
}

void drawRect(window *win, RGB color, int x, int y, int width, int height) {
	int screen_width = win->width;
	int screen_height = win->height;

	if (x >= screen_width || x + width < 0 || y >= screen_height ||
	    y + height < 0 || width < 0 || height < 0) {
		return;
	}
	int i;
	RGB *t = win->window_buf + y * screen_width + x;

	if (y >= 0) {
		for (i = 0; i < width; i++) {
			if (x + i >= 0 && x + i < screen_width) {
				*(t + i) = color;
			}
		}
	}
	if (y + height <= screen_height) {
		RGB *o = t + height * screen_width;
		for (i = 0; i < width; i++) {
			if (y >= 0 && x + i > 0 && x + i < screen_width) {
				*(o + i) = color;
			}
		}
	}
	if (x >= 0) {
		for (i = 0; i < height; i++) {
			if (y + i >= 0 && y + i < screen_height) {
				*(t + i * screen_width) = color;
			}
		}
	}

	if (x + width <= screen_width) {
		RGB *o = t + width;
		for (i = 0; i < height; i++) {
			if (y + i >= 0 && y + i < screen_height) {
				*(o + i * screen_width) = color;
			}
		}
	}
}

void drawFillRect(window *win, RGBA color, int x, int y, int width,
		  int height) {
	int screen_width = win->width;
	int screen_height = win->height;

	if (x >= screen_width || x + width < 0 || y >= screen_height ||
	    y + height < 0 || width < 0 || height < 0) {
		return;
	}

	if (x < 0) {
		width = width + x;
		x = 0;
	}
	if (y < 0) {
		height = height + y;
		y = 0;
	}
	if (x + width > screen_width) {
		width = screen_width - x;
	}
	if (y + height > screen_height) {
		height = screen_height - y;
	}

	if (color.A == 255) {
		RGB solid_color;
		solid_color.R = color.R;
		solid_color.G = color.G;
		solid_color.B = color.B;

		int i, j;
		RGB *t;
		for (i = 0; i < height; i++) {
			t = win->window_buf + (y + i) * win->width + x;
			for (j = 0; j < width; j++) {
				*t++ = solid_color;
			}
		}
		return;
	}

	int i, j;
	RGB *t;
	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			if (x + j > 0 && x + j < screen_width && y + i > 0 &&
			    y + i < screen_height) {
				t = win->window_buf + (y + i) * win->width +
				    (x + j);
				drawPointAlpha(t, color);
			}
		}
	}
}

void draw24FillRect(window *win, RGB color, int x, int y, int width,
		    int height) {
	if (x >= win->width || x + width < 0 || y >= win->height ||
	    y + height < 0 || x < 0 || y < 0 || width < 0 || height < 0) {
		return;
	}
	int i, j;
	int max_line = (win->width - x) < width ? (win->width - x) : width;
	RGB *t, *o;
	t = win->window_buf + y * win->width + x;
	for (i = 0; i < height; i++) {
		if (y + i >= win->height) {
			break;
		}
		if (y + i < 0) {
			continue;
		}
		if (i == 0) {
			for (j = 0; j < max_line; j++) {
				*(t + j) = color;
			}
		} else {
			o = win->window_buf + (y + i) * win->width + x;
			memmove(o, t, max_line * 3);
		}
	}
}

void drawIcon(window *win, int icon, RGBA color, int x, int y, int width,
	      int height) {
	int i, j;
	RGB *t;
	unsigned int p;

	if (icon < 0 || icon >= ICON_NUMBER)
		return;

	for (i = 0; i < ICON_SIZE; i++) {
		if (y + i >= win->height || y + i < 0)
			break;

		for (j = 0; j < ICON_SIZE; j++) {
			if (x + j >= win->width || x + j < 0)
				break;

			p = icons_data[icon][i * ICON_SIZE + j];

			if (p == 0) {
				continue;
			}

			t = win->window_buf + (y + i) * win->width + (x + j);

			RGBA pixel_color;
			pixel_color.R = (p >> 16) & 0xFF;
			pixel_color.G = (p >> 8) & 0xFF;
			pixel_color.B = p & 0xFF;
			pixel_color.A = 255;

			drawPointAlpha(t, pixel_color);
		}
	}
}