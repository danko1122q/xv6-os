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
#define DOUBLE_CLICK_TIME 20 // Reduced from 50
#define DRAG_THRESHOLD 1     // Minimum movement to start drag

// Struktur untuk menyimpan data aplikasi desktop
typedef struct {
	char name[32]; // Nama yang ditampilkan
	char exec[32]; // Nama file executable
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
int isDragging = 0; // Flag to track actual drag state
int dragStartX = 0;
int dragStartY = 0;
int appOriginalX = 0;
int appOriginalY = 0;

// Handler untuk menggambar icon dummy
void drawDummyIcon(int x, int y, int iconId, RGBA bgColor) {
	RGBA iconColor;
	iconColor.A = 255;

	// Warna berbeda untuk setiap icon ID
	switch (iconId % 4) {
	case 0: // Terminal - Hitam
		iconColor.R = 40;
		iconColor.G = 44;
		iconColor.B = 52;
		break;
	case 1: // Editor - Biru
		iconColor.R = 33;
		iconColor.G = 150;
		iconColor.B = 243;
		break;
	case 2: // Explorer - Orange
		iconColor.R = 255;
		iconColor.G = 152;
		iconColor.B = 0;
		break;
	case 3: // Game - Hijau
		iconColor.R = 76;
		iconColor.G = 175;
		iconColor.B = 80;
		break;
	}

	// Gambar background icon
	drawFillRect(&desktop, bgColor, x, y, ICON_SIZE, ICON_SIZE);

	// Gambar icon dummy sebagai kotak berwarna
	int padding = 8;
	drawFillRect(&desktop, iconColor, x + padding, y + padding,
		     ICON_SIZE - padding * 2, ICON_SIZE - padding * 2);

	// Gambar border
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

		// Gambar icon
		RGBA bg = iconBgColor;
		if (draggingApp == i && isDragging) {
			bg.A = 180; // Semi-transparent saat drag
		}
		drawDummyIcon(apps[i].x, apps[i].y, apps[i].iconId, bg);

		// Gambar label - tampilkan nama lengkap
		int labelY = apps[i].y + ICON_SIZE + 5;
		int textLen = strlen(apps[i].name);
		int textWidth = textLen * 9; // CHARACTER_WIDTH = 9

		// Center text jika muat, atau align left jika tidak
		int textX = apps[i].x;
		if (textWidth <= ICON_SIZE) {
			textX = apps[i].x + (ICON_SIZE - textWidth) / 2;
		}

		// Render text dengan width yang lebih besar untuk menampung
		// nama panjang
		drawString(&desktop, apps[i].name, textColor, textX, labelY, 80,
			   LABEL_HEIGHT);
	}
}

// Cek apakah mouse di dalam area app
int findAppAtPosition(int mouseX, int mouseY) {
	// Loop dari belakang (top app first)
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

		// Handle berdasarkan message type
		if (msg.msg_type == M_MOUSE_DOWN) {
			// Mouse DOWN - cek apakah klik di app
			int appIdx = findAppAtPosition(mouseX, mouseY);

			if (appIdx != -1) {
				// Check double click
				if (appIdx == lastClickApp &&
				    (currentTime - lastClickTime) <
					    DOUBLE_CLICK_TIME) {
					// DOUBLE CLICK - Jalankan program
					if (fork() == 0) {
						// Gunakan nama executable,
						// bukan display name
						char *argv2[] = {
							apps[appIdx].exec, 0};
						exec(argv2[0], argv2);
						exit();
					}
					// Reset state
					lastClickApp = -1;
					lastClickTime = 0;
					draggingApp = -1;
					isDragging = 0;
				} else {
					// SINGLE CLICK - Store initial click
					// position
					draggingApp = appIdx;
					isDragging = 0; // Not dragging yet
					dragStartX = mouseX;
					dragStartY = mouseY;
					appOriginalX = apps[appIdx].x;
					appOriginalY = apps[appIdx].y;
					lastClickApp = appIdx;
					lastClickTime = currentTime;
				}
			}

		} else if (msg.msg_type == M_MOUSE_MOVE) {
			// Mouse MOVE - update posisi jika sedang drag
			if (draggingApp != -1) {
				int deltaX = mouseX - dragStartX;
				int deltaY = mouseY - dragStartY;

				// Check if movement exceeds threshold to start
				// dragging
				if (!isDragging) {
					if (deltaX * deltaX + deltaY * deltaY >
					    DRAG_THRESHOLD * DRAG_THRESHOLD) {
						isDragging = 1;
					}
				}

				if (isDragging) {
					// Update posisi app
					apps[draggingApp].x =
						appOriginalX + deltaX;
					apps[draggingApp].y =
						appOriginalY + deltaY;

					// Batasi agar tidak keluar layar
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
			// Mouse UP/CLICK - stop drag
			if (draggingApp != -1) {
				if (!isDragging) {
					// Was a click, not a drag - check for
					// start button
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
				// Check for start button click
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
			// Double click langsung - ignore, sudah dihandle di
			// M_MOUSE_DOWN
			draggingApp = -1;
			isDragging = 0;
			lastClickApp = -1;
		}
	} else {
		desktop.needsRepaint = 0;
	}

	// Repaint jika perlu
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

	// Tambahkan aplikasi ke desktop dengan nama tampilan dan nama
	// executable Format: addDesktopApp(display_name, executable_name,
	// iconId, x, y)
	addDesktopApp("Terminal", "terminal", 0, 20, 20);
	addDesktopApp("Editor", "editor", 1, 20, 20 + APP_TOTAL_HEIGHT + 10);
	addDesktopApp("Explorer", "explorer", 2, 20,
		      20 + (APP_TOTAL_HEIGHT + 10) * 2);
	addDesktopApp("Floppy Bird", "floppybird", 3, 20,
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