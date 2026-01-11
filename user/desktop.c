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

window desktop;
struct RGBA desktopColor;
struct RGBA buttonColor;
struct RGBA textColor;

char *GUI_programs[] = {"shell", "editor", "explorer", "demo"};

void startProgramHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_DBCLICK) {
		if (fork() == 0) {
			char *argv2[] = {widget->context.button->text, 0};
			exec(argv2[0], argv2);
			exit();
		}
	}
}

void startWindowHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		if (fork() == 0) {
			char *argv2[] = {"startWindow", (char *)desktop.handler, 0};
			exec(argv2[0], argv2);
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
	
	// Gradient background
	struct RGBA desktopColorTop;
	desktopColorTop.R = 30;
	desktopColorTop.G = 60;
	desktopColorTop.B = 114;
	desktopColorTop.A = 255;
	
	struct RGBA desktopColorBottom;
	desktopColorBottom.R = 72;
	desktopColorBottom.G = 140;
	desktopColorBottom.B = 203;
	desktopColorBottom.A = 255;
	
	// Create gradient
	for (int y = 0; y < desktop.height; y++) {
		float factor = (float)y / desktop.height;
		struct RGBA currentColor;
		currentColor.R = desktopColorTop.R + 
				 (desktopColorBottom.R - desktopColorTop.R) * factor;
		currentColor.G = desktopColorTop.G + 
				 (desktopColorBottom.G - desktopColorTop.G) * factor;
		currentColor.B = desktopColorTop.B + 
				 (desktopColorBottom.B - desktopColorTop.B) * factor;
		currentColor.A = 255;
		fillRect(desktop.window_buf, 0, y, desktop.width, 1, 
			 desktop.width, desktop.height, currentColor);
	}
	
	// Modern button colors
	buttonColor.R = 41;
	buttonColor.G = 128;
	buttonColor.B = 185;
	buttonColor.A = 255;
	
	textColor.R = 255;
	textColor.G = 255;
	textColor.B = 255;
	textColor.A = 255;
	
	// Program buttons - JANGAN pakai Widget *btn =
	for (int i = 0; i < 4; i++) {
		addButtonWidget(&desktop, textColor, buttonColor,
				GUI_programs[i], 25, 20 + 50 * i, 100, 40, 0,
				startProgramHandler);
	}
	
	// Start button
	addButtonWidget(&desktop, textColor, buttonColor, "start", 5,
			SCREEN_HEIGHT - 36, 72, 36, 0, startWindowHandler);
	
	int lastTime = 0;
	while (1) {
		updateWindow(&desktop);
		int currentTime = uptime();
		if (currentTime - lastTime >= 2) {
			GUI_updateScreen();
			lastTime = currentTime;
		}
	}
}