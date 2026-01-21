#include "app_icons.h"
#include "fcntl.h"
#include "gui.h"
#include "memlayout.h"
#include "msg.h"
#include "types.h"
#include "user.h"
#include "user_gui.h"
#include "user_handler.h"
#include "user_window.h"

// Screen dimensions
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

// Desktop application settings
#define MAX_APPS 10
#define ICON_SIZE 48
#define LABEL_HEIGHT 20
#define APP_TOTAL_HEIGHT (ICON_SIZE + LABEL_HEIGHT + 5)

// Mouse interaction settings
#define DOUBLE_CLICK_TIME 20
#define DRAG_THRESHOLD                                                         \
	5 // BUG FIX #1: Threshold terlalu kecil (1 pixel), ubah ke 5

typedef struct {
	char name[32];
	char exec[32];
	int iconId;
	int x, y;
	int isActive;
} DesktopApp;

window desktop;

struct RGBA desktopColorTop;
struct RGBA desktopColorBottom;
struct RGBA buttonColor;
struct RGBA textColor;
struct RGBA iconBgColor;

DesktopApp apps[MAX_APPS];

int lastClickTime = 0;
int lastClickApp = -1;
int draggingApp = -1;
int isDragging = 0;
int dragStartX = 0;
int dragStartY = 0;
int appOriginalX = 0;
int appOriginalY = 0;

void drawAppIcon(int x, int y, int iconId) {
	if (iconId < 0 || iconId >= APP_ICON_COUNT) {
		RGBA iconColor;
		iconColor.R = 128;
		iconColor.G = 128;
		iconColor.B = 128;
		iconColor.A = 255;
		drawFillRect(&desktop, iconColor, x, y, ICON_SIZE, ICON_SIZE);
		return;
	}

	for (int iy = 0; iy < APP_ICON_SIZE; iy++) {
		for (int ix = 0; ix < APP_ICON_SIZE; ix++) {
			int idx = iy * APP_ICON_SIZE + ix;
			unsigned int pixel = app_icons_data[iconId][idx];

			if (pixel == 0xFF000000) {
				continue;
			}

			RGBA color;
			color.R = (pixel >> 16) & 0xFF;
			color.G = (pixel >> 8) & 0xFF;
			color.B = pixel & 0xFF;
			color.A = 255;

			drawFillRect(&desktop, color, x + ix, y + iy, 1, 1);
		}
	}
}

void drawDummyIcon(int x, int y, int iconId, RGBA bgColor) {
	RGBA iconColor;
	iconColor.A = 255;

	switch (iconId % 4) {
	case 0:
		iconColor.R = 40;
		iconColor.G = 44;
		iconColor.B = 52;
		break;
	case 1:
		iconColor.R = 33;
		iconColor.G = 150;
		iconColor.B = 243;
		break;
	case 2:
		iconColor.R = 255;
		iconColor.G = 152;
		iconColor.B = 0;
		break;
	case 3:
		iconColor.R = 76;
		iconColor.G = 175;
		iconColor.B = 80;
		break;
	}

	drawFillRect(&desktop, bgColor, x, y, ICON_SIZE, ICON_SIZE);

	int padding = 8;
	drawFillRect(&desktop, iconColor, x + padding, y + padding,
		     ICON_SIZE - padding * 2, ICON_SIZE - padding * 2);

	RGB borderColor;
	borderColor.R = 200;
	borderColor.G = 200;
	borderColor.B = 200;
	drawRect(&desktop, borderColor, x, y, ICON_SIZE, ICON_SIZE);
}

void renderAllApps() {
	for (int i = 0; i < MAX_APPS; i++) {
		if (!apps[i].isActive)
			continue;

		drawAppIcon(apps[i].x, apps[i].y, apps[i].iconId);

		int labelY = apps[i].y + ICON_SIZE + 5;
		int textLen = strlen(apps[i].name);
		int textWidth = textLen * 9;

		int textX = apps[i].x;
		if (textWidth <= ICON_SIZE) {
			textX = apps[i].x + (ICON_SIZE - textWidth) / 2;
		}

		drawString(&desktop, apps[i].name, textColor, textX, labelY, 80,
			   LABEL_HEIGHT);
	}
}

int findAppAtPosition(int mouseX, int mouseY) {
	for (int i = MAX_APPS - 1; i >= 0; i--) {
		if (!apps[i].isActive)
			continue;

		int x = apps[i].x;
		int y = apps[i].y;
		int w = ICON_SIZE;
		int h = APP_TOTAL_HEIGHT;

		if (mouseX >= x && mouseX <= x + w && mouseY >= y &&
		    mouseY <= y + h) {
			return i;
		}
	}
	return -1;
}

int addDesktopApp(char *name, char *exec, int iconId, int x, int y) {
	int idx = -1;
	for (int i = 0; i < MAX_APPS; i++) {
		if (!apps[i].isActive) {
			idx = i;
			break;
		}
	}

	if (idx == -1)
		return -1;

	strcpy(apps[idx].name, name);
	strcpy(apps[idx].exec, exec);
	apps[idx].iconId = iconId;
	apps[idx].x = x;
	apps[idx].y = y;
	apps[idx].isActive = 1;

	return idx;
}

void startWindowHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		if (fork() == 0) {
			char *argv2[] = {"startWindow", (char *)desktop.handler,
					 0};
			exec(argv2[0], argv2);
			exit();
		}
	}
}

void customUpdateWindow() {
	message msg;

	if (GUI_getMessage(desktop.handler, &msg) == 0) {

		if (msg.msg_type == WM_WINDOW_CLOSE) {
			closeWindow(&desktop);
		}

		int mouseX = msg.params[0];
		int mouseY = msg.params[1];
		int currentTime = uptime();

		if (msg.msg_type == M_MOUSE_DOWN) {
			int appIdx = findAppAtPosition(mouseX, mouseY);

			if (appIdx != -1) {
				// PERBAIKAN: Hapus pengecekan double-click
				// manual di sini Langsung setup drag saja
				draggingApp = appIdx;
				isDragging = 0;
				dragStartX = mouseX;
				dragStartY = mouseY;
				appOriginalX = apps[appIdx].x;
				appOriginalY = apps[appIdx].y;

				// Simpan state klik untuk keperluan internal
				// jika perlu, tapi jangan panggil fork() di
				// sini.
				lastClickApp = appIdx;
				lastClickTime = currentTime;
			} else {
				lastClickApp = -1;
				draggingApp = -1;
				isDragging = 0;
			}

		} else if (msg.msg_type == M_MOUSE_MOVE) {
			if (draggingApp != -1) {
				int deltaX = mouseX - dragStartX;
				int deltaY = mouseY - dragStartY;

				if (!isDragging) {
					if (deltaX * deltaX + deltaY * deltaY >
					    DRAG_THRESHOLD * DRAG_THRESHOLD) {
						isDragging = 1;
						// BUG FIX #5: Reset
						// double-click saat mulai drag
						lastClickApp = -1;
						lastClickTime = 0;
					}
				}

				if (isDragging) {
					apps[draggingApp].x =
						appOriginalX + deltaX;
					apps[draggingApp].y =
						appOriginalY + deltaY;

					// Clamp bounds
					if (apps[draggingApp].x < 0)
						apps[draggingApp].x = 0;
					if (apps[draggingApp].y < 0)
						apps[draggingApp].y = 0;
					if (apps[draggingApp].x + ICON_SIZE >
					    SCREEN_WIDTH)
						apps[draggingApp].x =
							SCREEN_WIDTH -
							ICON_SIZE;
					if (apps[draggingApp].y +
						    APP_TOTAL_HEIGHT >
					    SCREEN_HEIGHT - 40)
						apps[draggingApp].y =
							SCREEN_HEIGHT - 40 -
							APP_TOTAL_HEIGHT;

					desktop.needsRepaint = 1;
				}
			}

		} else if (msg.msg_type == M_MOUSE_LEFT_CLICK ||
			   msg.msg_type == M_MOUSE_UP) {
			// BUG FIX #6: Handle widget click hanya jika TIDAK
			// sedang drag
			if (draggingApp != -1 && !isDragging) {
				// Click tanpa drag - check widget
				for (int p = desktop.widgetlisttail; p != -1;
				     p = desktop.widgets[p].prev) {
					Widget *w = &desktop.widgets[p];
					if (mouseX >= w->position.xmin &&
					    mouseX <= w->position.xmax &&
					    mouseY >= w->position.ymin &&
					    mouseY <= w->position.ymax) {
						w->handler(w, &msg);
						break;
					}
				}
			} else if (draggingApp == -1) {
				// No drag in progress, handle widget clicks
				for (int p = desktop.widgetlisttail; p != -1;
				     p = desktop.widgets[p].prev) {
					Widget *w = &desktop.widgets[p];
					if (mouseX >= w->position.xmin &&
					    mouseX <= w->position.xmax &&
					    mouseY >= w->position.ymin &&
					    mouseY <= w->position.ymax) {
						w->handler(w, &msg);
						break;
					}
				}
			}

			// Reset drag state
			draggingApp = -1;
			isDragging = 0;
			desktop.needsRepaint = 1;

		} else if (msg.msg_type == M_MOUSE_DBCLICK) {
			// PERBAIKAN: Gunakan blok ini sebagai satu-satunya
			// pemicu launch aplikasi
			int appIdx = findAppAtPosition(mouseX, mouseY);
			if (appIdx != -1) {
				if (fork() == 0) {
					char *argv2[] = {apps[appIdx].exec, 0};
					exec(argv2[0], argv2);
					exit();
				}
			}
			// Reset semua state agar bersih
			draggingApp = -1;
			isDragging = 0;
			lastClickApp = -1;
			lastClickTime = 0;
		}
	} else {
		desktop.needsRepaint = 0;
	}

	if (desktop.needsRepaint) {
		// Render gradient background
		for (int y = 0; y < desktop.height; y++) {
			float factor = (float)y / desktop.height;

			RGBA currentColor;
			currentColor.R =
				desktopColorTop.R +
				(desktopColorBottom.R - desktopColorTop.R) *
					factor;
			currentColor.G =
				desktopColorTop.G +
				(desktopColorBottom.G - desktopColorTop.G) *
					factor;
			currentColor.B =
				desktopColorTop.B +
				(desktopColorBottom.B - desktopColorTop.B) *
					factor;
			currentColor.A = 255;

			fillRect(desktop.window_buf, 0, y, desktop.width, 1,
				 desktop.width, desktop.height, currentColor);
		}

		renderAllApps();

		// Render widgets
		for (int p = desktop.widgetlisthead; p != -1;
		     p = desktop.widgets[p].next) {
			if (desktop.widgets[p].type == BUTTON) {
				RGB black;
				black.R = 0;
				black.G = 0;
				black.B = 0;

				Widget *w = &desktop.widgets[p];
				int width = w->position.xmax - w->position.xmin;
				int height =
					w->position.ymax - w->position.ymin;

				int textYOffset = (height - 18) / 2;
				int textXOffset =
					(width -
					 strlen(w->context.button->text) * 9) /
					2;

				drawFillRect(&desktop,
					     w->context.button->bg_color,
					     w->position.xmin, w->position.ymin,
					     width, height);

				drawRect(&desktop, black, w->position.xmin,
					 w->position.ymin, width, height);

				drawString(&desktop, w->context.button->text,
					   w->context.button->color,
					   w->position.xmin + textXOffset,
					   w->position.ymin + textYOffset,
					   width, height);
			}
		}
	}
}

int main(int argc, char *argv[]) {
	desktop.width = SCREEN_WIDTH;
	desktop.height = SCREEN_HEIGHT;
	desktop.initialPosition.xmin = 0;
	desktop.initialPosition.xmax = SCREEN_WIDTH;
	desktop.initialPosition.ymin = 0;
	desktop.initialPosition.ymax = SCREEN_HEIGHT;
	desktop.hasTitleBar = 0;

	createWindow(&desktop, "desktop");

	desktopColorTop.R = 30;
	desktopColorTop.G = 60;
	desktopColorTop.B = 114;
	desktopColorTop.A = 255;

	desktopColorBottom.R = 72;
	desktopColorBottom.G = 140;
	desktopColorBottom.B = 203;
	desktopColorBottom.A = 255;

	for (int y = 0; y < desktop.height; y++) {
		float factor = (float)y / desktop.height;
		RGBA currentColor;
		currentColor.R =
			desktopColorTop.R +
			(desktopColorBottom.R - desktopColorTop.R) * factor;
		currentColor.G =
			desktopColorTop.G +
			(desktopColorBottom.G - desktopColorTop.G) * factor;
		currentColor.B =
			desktopColorTop.B +
			(desktopColorBottom.B - desktopColorTop.B) * factor;
		currentColor.A = 255;
		fillRect(desktop.window_buf, 0, y, desktop.width, 1,
			 desktop.width, desktop.height, currentColor);
	}

	buttonColor.R = 41;
	buttonColor.G = 128;
	buttonColor.B = 185;
	buttonColor.A = 255;

	textColor.R = 255;
	textColor.G = 255;
	textColor.B = 255;
	textColor.A = 255;

	iconBgColor.R = 255;
	iconBgColor.G = 255;
	iconBgColor.B = 255;
	iconBgColor.A = 200;

	for (int i = 0; i < MAX_APPS; i++) {
		apps[i].isActive = 0;
	}

	addDesktopApp("Terminal", "terminal", APP_ICON_TERMINAL, 20, 20);
	addDesktopApp("Editor", "editor", APP_ICON_EDITOR, 20,
		      20 + APP_TOTAL_HEIGHT + 10);
	addDesktopApp("Explorer", "explorer", APP_ICON_EXPLORER, 20,
		      20 + (APP_TOTAL_HEIGHT + 10) * 2);
	addDesktopApp("Floppy Bird", "floppybird", APP_ICON_FLOPPYBIRD, 20,
		      20 + (APP_TOTAL_HEIGHT + 10) * 3);

	renderAllApps();

	addButtonWidget(&desktop, textColor, buttonColor, "start", 5,
			SCREEN_HEIGHT - 36, 72, 36, 0, startWindowHandler);

	desktop.needsRepaint = 1;

	while (1) {
		customUpdateWindow();
		GUI_updateScreen();
	}
}