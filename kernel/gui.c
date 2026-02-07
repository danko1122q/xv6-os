#include "gui.h"
#include "character.h"
#include "defs.h"
#include "icons.h"
#include "memlayout.h"
#include "mmu.h"
#include "mouse_shape.h"
#include "param.h"
#include "proc.h"
#include "spinlock.h"
#include "types.h"
#include "x86.h"

ushort SCREEN_WIDTH;
ushort SCREEN_HEIGHT;
int screen_size;

RGB *screen;
RGB *screen_buf;

void initGUI() {
	uint GraphicMem = KERNBASE + 0x1028;

	uint baseAdd = *((uint *)GraphicMem);
	screen = (RGB *)baseAdd;

	SCREEN_WIDTH = *((ushort *)(KERNBASE + 0x1012));
	SCREEN_HEIGHT = *((ushort *)(KERNBASE + 0x1014));

	screen_size = (SCREEN_WIDTH * SCREEN_HEIGHT) * 3;

	screen_buf = (RGB *)(baseAdd + screen_size);

	mouse_color[0].G = 0;
	mouse_color[0].B = 0;
	mouse_color[0].R = 0;
	mouse_color[1].G = 200;
	mouse_color[1].B = 200;
	mouse_color[1].R = 200;

	cprintf("KERNEL BASE ADDRESS: %x\n", KERNBASE);
	cprintf("SCREEN PHYSICAL ADDRESS: %x\n", baseAdd);
	cprintf("@Screen Width:   %d\n", SCREEN_WIDTH);
	cprintf("@Screen Height:  %d\n", SCREEN_HEIGHT);
	cprintf("@Video card drivers initialized successfully.\n");

	wmInit();
}

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

int drawCharacter(RGB *buf, int x, int y, char ch, RGBA color) {
	int i, j;
	RGB *t;
	int ord = ch - 0x20;

	if (ord < 0 || ord >= (CHARACTER_NUMBER - 1))
		return -1;

	for (i = 0; i < CHARACTER_HEIGHT; i++) {
		if (y + i >= SCREEN_HEIGHT || y + i < 0)
			continue;

		for (j = 0; j < CHARACTER_WIDTH; j++) {
			uchar font_alpha = character[ord][i][j];

			if (font_alpha > 0) {
				if (x + j >= SCREEN_WIDTH || x + j < 0)
					continue;

				t = buf + (y + i) * SCREEN_WIDTH + x + j;

				RGBA smooth_color = color;
				smooth_color.A = (color.A * font_alpha) >> 8;

				drawPointAlpha(t, smooth_color);
			}
		}
	}
	return CHARACTER_WIDTH;
}

int drawIcon(RGB *buf, int x, int y, int icon, RGBA color) {
	int i, j;
	RGB *t;

	if (icon < 0 || icon >= ICON_NUMBER) {
		return -1;
	}

	for (i = 0; i < ICON_SIZE; i++) {
		if (y + i >= SCREEN_HEIGHT || y + i < 0)
			continue;

		for (j = 0; j < ICON_SIZE; j++) {
			if (x + j >= SCREEN_WIDTH || x + j < 0)
				continue;

			unsigned int raw_color = icons_data[icon][i * ICON_SIZE + j];

			if (raw_color == 0xFF000000 ||
			    raw_color == 0x00000000 ||
			    raw_color == ICON_TRANSPARENT) {
				continue;
			}

			t = buf + (y + i) * SCREEN_WIDTH + (x + j);

			RGBA pixel_color;
			pixel_color.R = (raw_color >> 16) & 0xFF;
			pixel_color.G = (raw_color >> 8) & 0xFF;
			pixel_color.B = raw_color & 0xFF;
			pixel_color.A = 255;

			drawPointAlpha(t, pixel_color);
		}
	}
	return ICON_SIZE;
}

void drawString(RGB *buf, int x, int y, char *str, RGBA color) {
	int offset_x = 0;
	while (*str != '\0') {
		offset_x += drawCharacter(buf, x + offset_x, y, *str, color);
		str++;
	}
}

void drawStringWithMaxWidth(RGB *buf, int x, int y, int width, char *str,
			    RGBA color) {
	int offset_x = 0;
	while (*str != '\0' && offset_x + CHARACTER_WIDTH <= width) {
		offset_x += drawCharacter(buf, x + offset_x, y, *str, color);
		str++;
	}
}

void drawImage(RGB *buf, RGBA *img, int x, int y, int width, int height,
	       int max_x, int max_y) {
	int i, j;
	RGB *t;
	RGBA *o;
	for (i = 0; i < height; i++) {
		if (y + i >= max_y)
			break;
		if (y + i < 0)
			continue;
		for (j = 0; j < width; j++) {
			if (x + j >= max_x)
				break;
			if (x + j < 0)
				continue;

			t = buf + (y + i) * SCREEN_WIDTH + x + j;
			o = img + (height - i) * width + j;
			drawPointAlpha(t, *o);
		}
	}
}

void draw24Image(RGB *buf, RGB *img, int x, int y, int width, int height,
		 int max_x, int max_y) {
	int i;
	RGB *t;
	RGB *o;
	int max_line = (max_x - x) < width ? (max_x - x) : width;
	for (i = 0; i < height; i++) {
		if (y + i >= max_y)
			break;
		if (y + i < 0)
			continue;

		t = buf + (y + i) * SCREEN_WIDTH + x;
		o = img + (height - i) * width;
		memmove(t, o, max_line * 3);
	}
}

void draw24ImagePart(RGB *buf, RGB *img, int x, int y, int width, int height,
		     int subx, int suby, int subw, int subh) {
	if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT)
		return;

	int minj = x < 0 ? -x : 0;
	int maxj = x + subw > SCREEN_WIDTH ? SCREEN_WIDTH - x : subw;
	if (minj >= maxj)
		return;

	int i;
	RGB *t;
	RGB *o;
	for (i = 0; i < subh; i++) {
		if (y + i < 0)
			continue;
		if (y + i >= SCREEN_HEIGHT)
			break;

		t = buf + (y + i) * SCREEN_WIDTH + minj + x;
		o = img + (i + suby) * width + subx + minj;
		memmove(t, o, (maxj - minj) * 3);
	}
}

void drawRectBound(RGB *buf, int x, int y, int width, int height, RGBA fill,
		   int max_x, int max_y) {
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
			t = buf + i * SCREEN_WIDTH + start_x;
			for (j = start_x; j < end_x; j++) {
				*t++ = solid_color;
			}
		}
		return;
	}
	
	for (i = start_y; i < end_y; i++) {
		for (j = start_x; j < end_x; j++) {
			t = buf + i * SCREEN_WIDTH + j;
			drawPointAlpha(t, fill);
		}
	}
}

void drawRectBorder(RGB *buf, RGB color, int x, int y, int width, int height) {
	if (x >= SCREEN_WIDTH || x + width < 0 || y >= SCREEN_HEIGHT ||
	    y + height < 0 || width < 0 || height < 0)
		return;

	int i;
	RGB *t = buf + y * SCREEN_WIDTH + x;

	if (y > 0) {
		for (i = 0; i < width; i++) {
			if (x + i > 0 && x + i < SCREEN_WIDTH)
				*(t + i) = color;
		}
	}
	if (y + height < SCREEN_HEIGHT) {
		RGB *o = t + height * SCREEN_WIDTH;
		for (i = 0; i < width; i++) {
			if (y > 0 && x + i > 0 && x + i < SCREEN_WIDTH)
				*(o + i) = color;
		}
	}
	if (x > 0) {
		for (i = 0; i < height; i++) {
			if (y + i > 0 && y + i < SCREEN_HEIGHT)
				*(t + i * SCREEN_WIDTH) = color;
		}
	}
	if (x + width < SCREEN_WIDTH) {
		RGB *o = t + width;
		for (i = 0; i < height; i++) {
			if (y + i > 0 && y + i < SCREEN_HEIGHT)
				*(o + i * SCREEN_WIDTH) = color;
		}
	}
}

void drawRect(RGB *buf, int x, int y, int width, int height, RGBA fill) {
	drawRectBound(buf, x, y, width, height, fill, SCREEN_WIDTH,
		      SCREEN_HEIGHT);
}

void drawRectByCoord(RGB *buf, int xmin, int ymin, int xmax, int ymax,
		     RGBA fill) {
	drawRect(buf, xmin, ymin, xmax - xmin, ymax - ymin, fill);
}

void clearRect(RGB *buf, RGB *temp_buf, int x, int y, int width, int height) {
	if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT || x + width <= 0 || y + height <= 0)
		return;
	
	int start_y = y < 0 ? 0 : y;
	int end_y = (y + height > SCREEN_HEIGHT) ? SCREEN_HEIGHT : y + height;
	int start_x = x < 0 ? 0 : x;
	int copy_width = (x + width > SCREEN_WIDTH) ? SCREEN_WIDTH - x : width;
	
	if (start_x < 0) {
		copy_width += start_x;
		start_x = 0;
	}
	
	if (copy_width <= 0)
		return;
	
	RGB *t;
	RGB *o;
	int bytes_to_copy = copy_width * 3;
	
	for (int i = start_y; i < end_y; i++) {
		t = buf + i * SCREEN_WIDTH + start_x;
		o = temp_buf + i * SCREEN_WIDTH + start_x;
		memmove(t, o, bytes_to_copy);
	}
}

void clearRectByCoord(RGB *buf, RGB *temp_buf, int xmin, int ymin, int xmax,
		      int ymax) {
	clearRect(buf, temp_buf, xmin, ymin, xmax - xmin, ymax - ymin);
}

void drawMouse(RGB *buf, int mode, int x, int y) {
	int i, j;
	RGB *t;
	for (i = 0; i < MOUSE_HEIGHT; i++) {
		if (y + i >= SCREEN_HEIGHT)
			break;
		if (y + i < 0)
			continue;
		for (j = 0; j < MOUSE_WIDTH; j++) {
			if (x + j >= SCREEN_WIDTH)
				break;
			if (x + j < 0)
				continue;

			uchar temp = mouse_pointer[mode][i][j];
			if (temp) {
				t = buf + (y + i) * SCREEN_WIDTH + x + j;
				drawPoint(t, mouse_color[temp - 1]);
			}
		}
	}
}

void clearMouse(RGB *buf, RGB *temp_buf, int x, int y) {
	clearRect(buf, temp_buf, x, y, MOUSE_WIDTH, MOUSE_HEIGHT);
}