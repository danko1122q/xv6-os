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
	float alpha;
	if (origin.A == 255) {
		color->R = origin.R;
		color->G = origin.G;
		color->B = origin.B;
		return;
	}
	if (origin.A == 0) {
		return;
	}
	alpha = (float)origin.A / 255;
	color->R = color->R * (1 - alpha) + origin.R * alpha;
	color->G = color->G * (1 - alpha) + origin.G * alpha;
	color->B = color->B * (1 - alpha) + origin.B * alpha;
}

void fillRect(RGB *buf, int x, int y, int width, int height, int max_x,
	      int max_y, RGBA fill) {
	int i, j;
	RGB *t;
	for (i = 0; i < height; i++) {
		if (y + i < 0)
			continue;
		if (y + i >= max_y)
			break;
		for (j = 0; j < width; j++) {
			if (x + j < 0)
				continue;
			if (x + j >= max_x)
				break;
			t = buf + (y + i) * max_x + x + j;
			drawPointAlpha(t, fill);
		}
	}
}


int drawCharacter(window *win, int x, int y, char ch, RGBA color) {
    int i, j;
    RGB *t;
    int ord = ch - 0x20;
    if (ord < 0 || ord >= (CHARACTER_NUMBER - 1)) {
        return -1;
    }
    for (i = 0; i < CHARACTER_HEIGHT; i++) {
        if (y + i >= win->height || y + i < 0) continue;
        
        for (j = 0; j < CHARACTER_WIDTH; j++) {
            // Jika bitmap bernilai 1, gambar pixel tersebut
            if (character[ord][i][j] == 1) {
                if (x + j >= win->width || x + j < 0) continue;
                
                t = win->window_buf + (y + i) * win->width + (x + j);
                drawPointAlpha(t, color);
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
		if (offset_y > height)
			break;
		if (*str != '\n') {
			if (x + offset_x >= 0 && x + offset_x <= win->width &&
			    y + offset_y >= 0 && y + offset_y <= win->height) {
				drawCharacter(win, x + offset_x, y + offset_y,
					      *str, color);
			}

			offset_x += CHARACTER_WIDTH;
			if (offset_x > width) {
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

	for (i = 0; i < height; i++) {
		if (y + i >= win->height) {
			break;
		}
		if (y + i < 0) {
			continue;
		}
		for (j = 0; j < width; j++) {
			if (x + j >= win->width) {
				break;
			}
			if (x + j < 0) {
				continue;
			}
			t = win->window_buf + (y + i) * win->width + x + j;
			o = img + (height - i - 1) * width + j;
			drawPointAlpha(t, *o);
		}
	}
}

void draw24Image(window *win, RGB *img, int x, int y, int width, int height) {
	int i;
	RGB *t;
	RGB *o;
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
	// int max_line = (SCREEN_WIDTH - x) < width ? (SCREEN_WIDTH - x) :
	// width;
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

// user_gui.c
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

			// JIKA pixel bernilai 0 (hitam murni di area
			// transparan), kita lompati (skip) Ini akan membuat
			// warna biru di bawahnya tetap terlihat
			if (p == 0) {
				continue;
			}

			t = win->window_buf + (y + i) * win->width + (x + j);

			// Gunakan drawPointAlpha agar pixel menyatu dengan
			// background
			RGBA pixel_color;
			pixel_color.R = (p >> 16) & 0xFF;
			pixel_color.G = (p >> 8) & 0xFF;
			pixel_color.B = p & 0xFF;
			pixel_color.A = 255; // Kita anggap bagian yang tidak 0
					     // adalah solid

			drawPointAlpha(t, pixel_color);
		}
	}
}