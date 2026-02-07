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

int min(int a, int b);
int max(int a, int b);
void startWindowHandler(Widget *w, message *msg);

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

#define MAX_APPS 10
#define ICON_SIZE 48
#define LABEL_HEIGHT 20
#define APP_TOTAL_HEIGHT (ICON_SIZE + LABEL_HEIGHT + 5)

#define DRAG_THRESHOLD 5
#define SELECTION_UPDATE_THRESHOLD 3

typedef struct {
	char name[32];
	char exec[32];
	int iconId;
	int x, y;
	int isActive;
	int isSelected;
} DesktopApp;

window desktop;

struct RGBA desktopColorTop;
struct RGBA desktopColorBottom;
struct RGBA buttonColor;
struct RGBA textColor;
struct RGBA iconBgColor;

DesktopApp apps[MAX_APPS];

int draggingApp = -1;
int isDragging = 0;
int dragStartX = 0;
int dragStartY = 0;
int appOriginalX = 0;
int appOriginalY = 0;

int isSelecting = 0;
int selectionStartX = 0;
int selectionStartY = 0;
int selectionCurrentX = 0;
int selectionCurrentY = 0;
int lastSelectionUpdateX = 0;
int lastSelectionUpdateY = 0;

RGB *backgroundCache = 0;
int backgroundCached = 0;

static inline int abs_int(int x) { return x < 0 ? -x : x; }

void clearAllSelections() {
	for (int i = 0; i < MAX_APPS; i++) {
		apps[i].isSelected = 0;
	}
}

int isAppInSelectionBox(int appIdx, int boxX1, int boxY1, int boxX2,
			int boxY2) {
	if (!apps[appIdx].isActive)
		return 0;

	int iconX1 = apps[appIdx].x;
	int iconY1 = apps[appIdx].y;
	int iconX2 = iconX1 + ICON_SIZE;
	int iconY2 = iconY1 + APP_TOTAL_HEIGHT;

	return !(iconX2 < boxX1 || iconX1 > boxX2 || iconY2 < boxY1 ||
		 iconY1 > boxY2);
}

void updateSelection(int boxX1, int boxY1, int boxX2, int boxY2) {
	for (int i = 0; i < MAX_APPS; i++) {
		if (!apps[i].isActive)
			continue;
		apps[i].isSelected =
			isAppInSelectionBox(i, boxX1, boxY1, boxX2, boxY2);
	}
}

void drawSelectionBoxFast(int x1, int y1, int x2, int y2) {
	int minX = min(x1, x2);
	int minY = min(y1, y2);
	int maxX = max(x1, x2);
	int maxY = max(y1, y2);
	int width = maxX - minX;
	int height = maxY - minY;

	if (width <= 2 || height <= 2)
		return;

	if (minX < 0)
		minX = 0;
	if (minY < 0)
		minY = 0;
	if (maxX > desktop.width)
		maxX = desktop.width;
	if (maxY > desktop.height)
		maxY = desktop.height;
	width = maxX - minX;
	height = maxY - minY;

	RGB *buf = desktop.window_buf;
	int bufWidth = desktop.width;

	RGB fillColor;
	fillColor.R = 190;
	fillColor.G = 210;
	fillColor.B = 255;

	RGB borderColor;
	borderColor.R = 50;
	borderColor.G = 100;
	borderColor.B = 200;

	if (minY >= 0 && minY < desktop.height) {
		RGB *scanline = buf + minY * bufWidth + minX;
		for (int x = 0; x < width; x++) {
			scanline[x] = borderColor;
		}
	}

	if (maxY - 1 >= 0 && maxY - 1 < desktop.height) {
		RGB *scanline = buf + (maxY - 1) * bufWidth + minX;
		for (int x = 0; x < width; x++) {
			scanline[x] = borderColor;
		}
	}

	for (int y = minY; y < maxY; y++) {
		if (y >= 0 && y < desktop.height && minX >= 0 &&
		    minX < bufWidth) {
			buf[y * bufWidth + minX] = borderColor;
		}
	}

	for (int y = minY; y < maxY; y++) {
		if (y >= 0 && y < desktop.height && maxX - 1 >= 0 &&
		    maxX - 1 < bufWidth) {
			buf[y * bufWidth + maxX - 1] = borderColor;
		}
	}

	int area = width * height;
	int skipFactor = 2;

	if (area < 10000) {
		skipFactor = 2;
	} else if (area < 50000) {
		skipFactor = 2;
	} else {
		skipFactor = 3;
	}

	for (int y = minY + 1; y < maxY - 1; y++) {
		if (y < 0 || y >= desktop.height)
			continue;

		RGB *scanline = buf + y * bufWidth + minX + 1;

		int startOffset = (y % skipFactor);
		for (int x = startOffset; x < width - 2; x += skipFactor) {
			if (minX + 1 + x >= 0 && minX + 1 + x < bufWidth) {
				RGB *pixel = &scanline[x];

				pixel->R = (pixel->R + fillColor.R) >> 1;
				pixel->G = (pixel->G + fillColor.G) >> 1;
				pixel->B = (pixel->B + fillColor.B) >> 1;
			}
		}
	}
}

void drawSelectionHighlight(int x, int y) {
	RGBA highlightColor;
	highlightColor.R = 255;
	highlightColor.G = 220;
	highlightColor.B = 0;
	highlightColor.A = 100;

	int offset = 2;
	drawFillRect(&desktop, highlightColor, x - offset, y - offset,
		     ICON_SIZE + offset * 2, offset);
	drawFillRect(&desktop, highlightColor, x - offset, y + ICON_SIZE,
		     ICON_SIZE + offset * 2, offset);
	drawFillRect(&desktop, highlightColor, x - offset, y, offset,
		     ICON_SIZE);
	drawFillRect(&desktop, highlightColor, x + ICON_SIZE, y, offset,
		     ICON_SIZE);
}

void drawAppIconFast(int x, int y, int iconId) {
	if (iconId < 0 || iconId >= APP_ICON_COUNT) {
		RGBA iconColor = {255, 128, 128, 128};
		drawFillRect(&desktop, iconColor, x, y, ICON_SIZE, ICON_SIZE);
		return;
	}

	RGB *buf = desktop.window_buf;
	int bufWidth = desktop.width;

	for (int iy = 0; iy < APP_ICON_SIZE; iy++) {
		int screenY = y + iy;
		if (screenY < 0 || screenY >= desktop.height)
			continue;

		RGB *scanline = buf + screenY * bufWidth + x;

		for (int ix = 0; ix < APP_ICON_SIZE; ix++) {
			int screenX = x + ix;
			if (screenX < 0 || screenX >= bufWidth)
				continue;

			int idx = iy * APP_ICON_SIZE + ix;
			unsigned int pixel = app_icons_data[iconId][idx];

			if (pixel == 0xFF000000)
				continue;

			scanline[ix].R = (pixel >> 16) & 0xFF;
			scanline[ix].G = (pixel >> 8) & 0xFF;
			scanline[ix].B = pixel & 0xFF;
		}
	}
}

void renderAllApps() {
	for (int i = 0; i < MAX_APPS; i++) {
		if (!apps[i].isActive)
			continue;

		if (apps[i].isSelected) {
			drawSelectionHighlight(apps[i].x, apps[i].y);
		}

		drawAppIconFast(apps[i].x, apps[i].y, apps[i].iconId);

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
	int foundIdx = -1;
	int topMostZ = -1;

	for (int i = 0; i < MAX_APPS; i++) {
		if (!apps[i].isActive)
			continue;

		int x = apps[i].x;
		int y = apps[i].y;

		if (mouseX >= x && mouseX < x + ICON_SIZE && mouseY >= y &&
		    mouseY < y + APP_TOTAL_HEIGHT) {
			if (i > topMostZ) {
				topMostZ = i;
				foundIdx = i;
			}
		}
	}

	return foundIdx;
}

void addDesktopApp(char *name, char *exec, int iconId, int x, int y) {
	for (int i = 0; i < MAX_APPS; i++) {
		if (!apps[i].isActive) {
			strcpy(apps[i].name, name);
			strcpy(apps[i].exec, exec);
			apps[i].iconId = iconId;
			apps[i].x = x;
			apps[i].y = y;
			apps[i].isActive = 1;
			apps[i].isSelected = 0;
			return;
		}
	}
}

void restoreBackground() {
	if (!backgroundCached)
		return;

	RGB *dst = desktop.window_buf;
	RGB *src = backgroundCache;
	int totalPixels = desktop.width * desktop.height;

	for (int i = 0; i < totalPixels; i++) {
		dst[i] = src[i];
	}
}

void createBackgroundCache() {
	if (backgroundCache != 0)
		return;

	int cacheSize = desktop.width * desktop.height * sizeof(RGB);
	backgroundCache = (RGB *)malloc(cacheSize);

	if (backgroundCache == 0) {
		backgroundCached = 0;
		return;
	}

	for (int y = 0; y < desktop.height; y++) {
		int factor_256 = (y * 256) / desktop.height;

		RGB currentColor;
		currentColor.R = desktopColorTop.R +
				 (((desktopColorBottom.R - desktopColorTop.R) *
				   factor_256) >>
				  8);
		currentColor.G = desktopColorTop.G +
				 (((desktopColorBottom.G - desktopColorTop.G) *
				   factor_256) >>
				  8);
		currentColor.B = desktopColorTop.B +
				 (((desktopColorBottom.B - desktopColorTop.B) *
				   factor_256) >>
				  8);

		RGB *scanline = backgroundCache + y * desktop.width;
		for (int x = 0; x < desktop.width; x++) {
			scanline[x] = currentColor;
		}
	}

	backgroundCached = 1;
}

void customUpdateWindow() {
	message msg;

	if (GUI_getMessage(desktop.handler, &msg) == 0) {

		if (msg.msg_type == WM_WINDOW_CLOSE) {
			closeWindow(&desktop);
		}

		int mouseX = msg.params[0];
		int mouseY = msg.params[1];

		if (msg.msg_type == M_MOUSE_DOWN) {
			int appIdx = findAppAtPosition(mouseX, mouseY);

			if (appIdx != -1) {
				draggingApp = appIdx;
				isDragging = 0;
				dragStartX = mouseX;
				dragStartY = mouseY;
				appOriginalX = apps[appIdx].x;
				appOriginalY = apps[appIdx].y;
				isSelecting = 0;
			} else {
				isSelecting = 1;
				selectionStartX = mouseX;
				selectionStartY = mouseY;
				selectionCurrentX = mouseX;
				selectionCurrentY = mouseY;
				lastSelectionUpdateX = mouseX;
				lastSelectionUpdateY = mouseY;
				clearAllSelections();
				draggingApp = -1;
				isDragging = 0;
			}
		}

		else if (msg.msg_type == M_MOUSE_MOVE) {
			if (draggingApp != -1 && !isSelecting) {
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

			if (isSelecting) {
				selectionCurrentX = mouseX;
				selectionCurrentY = mouseY;
				desktop.needsRepaint = 1;

				int deltaX =
					abs_int(mouseX - lastSelectionUpdateX);
				int deltaY =
					abs_int(mouseY - lastSelectionUpdateY);

				if (deltaX >= SELECTION_UPDATE_THRESHOLD ||
				    deltaY >= SELECTION_UPDATE_THRESHOLD) {

					int minX = min(selectionStartX,
						       selectionCurrentX);
					int minY = min(selectionStartY,
						       selectionCurrentY);
					int maxX = max(selectionStartX,
						       selectionCurrentX);
					int maxY = max(selectionStartY,
						       selectionCurrentY);

					updateSelection(minX, minY, maxX, maxY);
					lastSelectionUpdateX = mouseX;
					lastSelectionUpdateY = mouseY;
				}
			}
		}

		else if (msg.msg_type == M_MOUSE_UP) {
			if (draggingApp != -1) {
				draggingApp = -1;
				isDragging = 0;
				desktop.needsRepaint = 1;
			}

			if (isSelecting) {
				int minX =
					min(selectionStartX, selectionCurrentX);
				int minY =
					min(selectionStartY, selectionCurrentY);
				int maxX =
					max(selectionStartX, selectionCurrentX);
				int maxY =
					max(selectionStartY, selectionCurrentY);

				updateSelection(minX, minY, maxX, maxY);
				isSelecting = 0;
				desktop.needsRepaint = 1;
			}
		}

		else if (msg.msg_type == M_MOUSE_LEFT_CLICK) {
			int appIdx = findAppAtPosition(mouseX, mouseY);

			if (appIdx == -1) {
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
		}

		else if (msg.msg_type == M_MOUSE_DBCLICK) {
			int appIdx = findAppAtPosition(mouseX, mouseY);

			if (appIdx != -1) {
				if (fork() == 0) {
					char *argv2[] = {apps[appIdx].exec, 0};
					exec(argv2[0], argv2);
					exit();
				}
			}

			draggingApp = -1;
			isDragging = 0;
			isSelecting = 0;
		}

	} else {
		desktop.needsRepaint = 0;
	}

	if (desktop.needsRepaint) {
		if (backgroundCached) {
			restoreBackground();
		} else {
			for (int y = 0; y < desktop.height; y++) {
				int factor_256 = (y * 256) / desktop.height;
				RGBA currentColor;
				currentColor.R = desktopColorTop.R +
						 (((desktopColorBottom.R -
						    desktopColorTop.R) *
						   factor_256) >>
						  8);
				currentColor.G = desktopColorTop.G +
						 (((desktopColorBottom.G -
						    desktopColorTop.G) *
						   factor_256) >>
						  8);
				currentColor.B = desktopColorTop.B +
						 (((desktopColorBottom.B -
						    desktopColorTop.B) *
						   factor_256) >>
						  8);
				currentColor.A = 255;
				fillRect(desktop.window_buf, 0, y,
					 desktop.width, 1, desktop.width,
					 desktop.height, currentColor);
			}
		}

		renderAllApps();

		if (isSelecting) {
			drawSelectionBoxFast(selectionStartX, selectionStartY,
					     selectionCurrentX,
					     selectionCurrentY);
		}

		for (int p = desktop.widgetlisthead; p != -1;
		     p = desktop.widgets[p].next) {
			if (desktop.widgets[p].type == BUTTON) {
				RGB black = {0, 0, 0};
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

void startWindowHandler(Widget *w, message *msg) {
	(void)w;

	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		printf(1, "Start button clicked!\n");

		if (fork() == 0) {
			// Child: run startWindow
			char *argv2[] = {"startWindow", 0}; // Tanpa argument
			exec(argv2[0], argv2);
			printf(1, "Failed to start startWindow\n");
			exit();
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
		int factor_256 = (y * 256) / desktop.height;
		RGBA currentColor;
		currentColor.R = desktopColorTop.R +
				 (((desktopColorBottom.R - desktopColorTop.R) *
				   factor_256) >>
				  8);
		currentColor.G = desktopColorTop.G +
				 (((desktopColorBottom.G - desktopColorTop.G) *
				   factor_256) >>
				  8);
		currentColor.B = desktopColorTop.B +
				 (((desktopColorBottom.B - desktopColorTop.B) *
				   factor_256) >>
				  8);
		currentColor.A = 255;
		fillRect(desktop.window_buf, 0, y, desktop.width, 1,
			 desktop.width, desktop.height, currentColor);
	}

	createBackgroundCache();

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
		apps[i].isSelected = 0;
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