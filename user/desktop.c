#include "app_icons.h" // Tambahkan include ini
#include "fcntl.h"
#include "gui.h"
#include "memlayout.h"
#include "msg.h"
#include "types.h"
#include "user.h"
#include "user_gui.h"
#include "user_handler.h"
#include "user_window.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define MAX_APPS 10
#define ICON_SIZE 48
#define LABEL_HEIGHT 20
#define APP_TOTAL_HEIGHT (ICON_SIZE + LABEL_HEIGHT + 5)
#define DOUBLE_CLICK_TIME 20
#define DRAG_THRESHOLD 1

// Struktur untuk menyimpan data aplikasi desktop
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

// Mouse state tracking
int lastClickTime = 0;
int lastClickApp = -1;
int draggingApp = -1;
int isDragging = 0;
int dragStartX = 0;
int dragStartY = 0;
int appOriginalX = 0;
int appOriginalY = 0;

// Handler untuk menggambar icon PNG dari array
void drawAppIcon(int x, int y, int iconId) {
	if (iconId < 0 || iconId >= APP_ICON_COUNT) {
		// Fallback ke dummy icon jika ID tidak valid
		RGBA iconColor;
		iconColor.R = 128;
		iconColor.G = 128;
		iconColor.B = 128;
		iconColor.A = 255;

		drawFillRect(&desktop, iconColor, x, y, ICON_SIZE, ICON_SIZE);
		return;
	}

	// Render icon dari array app_icons_data
	for (int iy = 0; iy < APP_ICON_SIZE; iy++) {
		for (int ix = 0; ix < APP_ICON_SIZE; ix++) {
			int idx = iy * APP_ICON_SIZE + ix;
			unsigned int pixel = app_icons_data[iconId][idx];

			// Skip pixel transparan (marker 0xFF000000)
			if (pixel == 0xFF000000) {
				continue;
			}

			// Extract RGB dari pixel (format: 0x00RRGGBB)
			RGBA color;
			color.R = (pixel >> 16) & 0xFF;
			color.G = (pixel >> 8) & 0xFF;
			color.B = pixel & 0xFF;
			color.A = 255;

			// Gambar pixel di posisi yang tepat
			drawFillRect(&desktop, color, x + ix, y + iy, 1, 1);
		}
	}
}

// Handler untuk menggambar icon dummy (fallback)
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

// Render semua aplikasi
void renderAllApps() {
	for (int i = 0; i < MAX_APPS; i++) {
		if (!apps[i].isActive)
			continue;

		// Gambar icon PNG dari array (tanpa background)
		drawAppIcon(apps[i].x, apps[i].y, apps[i].iconId);

		// Gambar label
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

// Cek apakah mouse di dalam area app
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

// Fungsi untuk menambah aplikasi baru ke desktop
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

// Custom updateWindow
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
				if (appIdx == lastClickApp &&
				    (currentTime - lastClickTime) <
					    DOUBLE_CLICK_TIME) {
					if (fork() == 0) {
						char *argv2[] = {
							apps[appIdx].exec, 0};
						exec(argv2[0], argv2);
						exit();
					}
					lastClickApp = -1;
					lastClickTime = 0;
					draggingApp = -1;
					isDragging = 0;
				} else {
					draggingApp = appIdx;
					isDragging = 0;
					dragStartX = mouseX;
					dragStartY = mouseY;
					appOriginalX = apps[appIdx].x;
					appOriginalY = apps[appIdx].y;
					lastClickApp = appIdx;
					lastClickTime = currentTime;
				}
			}

		} else if (msg.msg_type == M_MOUSE_MOVE) {
			if (draggingApp != -1) {
				int deltaX = mouseX - dragStartX;
				int deltaY = mouseY - dragStartY;

				if (!isDragging) {
					if (deltaX * deltaX + deltaY * deltaY >
					    DRAG_THRESHOLD * DRAG_THRESHOLD) {
						isDragging = 1;
					}
				}

				if (isDragging) {
					apps[draggingApp].x =
						appOriginalX + deltaX;
					apps[draggingApp].y =
						appOriginalY + deltaY;

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
			if (draggingApp != -1) {
				if (!isDragging) {
					for (int p = desktop.widgetlisttail;
					     p != -1;
					     p = desktop.widgets[p].prev) {
						Widget *w = &desktop.widgets[p];
						if (mouseX >=
							    w->position.xmin &&
						    mouseX <=
							    w->position.xmax &&
						    mouseY >=
							    w->position.ymin &&
						    mouseY <=
							    w->position.ymax) {
							w->handler(w, &msg);
							break;
						}
					}
				}
				draggingApp = -1;
				isDragging = 0;
				desktop.needsRepaint = 1;
			} else {
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

		} else if (msg.msg_type == M_MOUSE_DBCLICK) {
			draggingApp = -1;
			isDragging = 0;
			lastClickApp = -1;
		}
	} else {
		desktop.needsRepaint = 0;
	}

	if (desktop.needsRepaint) {
		// Re-render gradient background
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

		// Render semua apps
		renderAllApps();

		// Render widgets (start button)
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

	// Gradient background colors
	desktopColorTop.R = 30;
	desktopColorTop.G = 60;
	desktopColorTop.B = 114;
	desktopColorTop.A = 255;

	desktopColorBottom.R = 72;
	desktopColorBottom.G = 140;
	desktopColorBottom.B = 203;
	desktopColorBottom.A = 255;

	// Create initial gradient
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

	// UI colors
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

	// Inisialisasi apps
	for (int i = 0; i < MAX_APPS; i++) {
		apps[i].isActive = 0;
	}

	// Tambahkan aplikasi dengan icon PNG custom
	// Format: addDesktopApp(display_name, executable_name, APP_ICON_xxx, x,
	// y)
	addDesktopApp("Terminal", "terminal", APP_ICON_TERMINAL, 20, 20);
	addDesktopApp("Editor", "editor", APP_ICON_EDITOR, 20,
		      20 + APP_TOTAL_HEIGHT + 10);
	addDesktopApp("Explorer", "explorer", APP_ICON_EXPLORER, 20,
		      20 + (APP_TOTAL_HEIGHT + 10) * 2);
	addDesktopApp("Floppy Bird", "floppybird", APP_ICON_FLOPPYBIRD, 20,
		      20 + (APP_TOTAL_HEIGHT + 10) * 3);

	// Render apps pertama kali
	renderAllApps();

	// Start button
	addButtonWidget(&desktop, textColor, buttonColor, "start", 5,
			SCREEN_HEIGHT - 36, 72, 36, 0, startWindowHandler);

	desktop.needsRepaint = 1;

	while (1) {
		customUpdateWindow();
		GUI_updateScreen();
	}
}