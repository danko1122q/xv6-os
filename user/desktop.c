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
#define DOUBLE_CLICK_TIME                                                      \
	20		 // Time threshold for double-click detection (in ticks)
#define DRAG_THRESHOLD 1 // Minimum pixel movement to trigger drag

/**
 * Structure to store desktop application data
 * Each app has a name, executable path, icon ID, position, and active state
 */
typedef struct {
	char name[32]; // Display name of the application
	char exec[32]; // Executable file name
	int iconId;    // Icon ID from app_icons_data array
	int x, y;      // Screen position coordinates
	int isActive;  // Whether this slot is occupied (1) or empty (0)
} DesktopApp;

// Desktop window object
window desktop;

// Color definitions for UI elements
struct RGBA desktopColorTop;	// Top gradient color for background
struct RGBA desktopColorBottom; // Bottom gradient color for background
struct RGBA buttonColor;	// Start button background color
struct RGBA textColor;		// Text color for labels
struct RGBA iconBgColor;	// Icon background color (unused with PNG icons)

// Array to store all desktop applications
DesktopApp apps[MAX_APPS];

// Mouse state tracking variables for click and drag detection
int lastClickTime = 0; // Timestamp of last click
int lastClickApp = -1; // Index of last clicked app
int draggingApp = -1;  // Index of app being dragged (-1 if none)
int isDragging = 0;    // Flag indicating if drag is in progress
int dragStartX = 0;    // Mouse X position when drag started
int dragStartY = 0;    // Mouse Y position when drag started
int appOriginalX = 0;  // App's X position before drag started
int appOriginalY = 0;  // App's Y position before drag started

/**
 * Draw an application icon from the PNG icon array
 * @param x         X coordinate on screen
 * @param y         Y coordinate on screen
 * @param iconId    Icon ID from APP_ICON_* enum
 */
void drawAppIcon(int x, int y, int iconId) {
	// Validate icon ID is within valid range
	if (iconId < 0 || iconId >= APP_ICON_COUNT) {
		// Fallback to dummy gray icon if ID is invalid
		RGBA iconColor;
		iconColor.R = 128;
		iconColor.G = 128;
		iconColor.B = 128;
		iconColor.A = 255;

		drawFillRect(&desktop, iconColor, x, y, ICON_SIZE, ICON_SIZE);
		return;
	}

	// Render icon pixel by pixel from app_icons_data array
	for (int iy = 0; iy < APP_ICON_SIZE; iy++) {
		for (int ix = 0; ix < APP_ICON_SIZE; ix++) {
			// Calculate array index for current pixel
			int idx = iy * APP_ICON_SIZE + ix;
			unsigned int pixel = app_icons_data[iconId][idx];

			// Skip transparent pixels (marked with 0xFF000000)
			if (pixel == 0xFF000000) {
				continue;
			}

			// Extract RGB components from pixel data (format:
			// 0x00RRGGBB)
			RGBA color;
			color.R = (pixel >> 16) & 0xFF; // Red channel
			color.G = (pixel >> 8) & 0xFF;	// Green channel
			color.B = pixel & 0xFF;		// Blue channel
			color.A = 255;			// Full opacity

			// Draw the pixel at the correct screen position
			drawFillRect(&desktop, color, x + ix, y + iy, 1, 1);
		}
	}
}

/**
 * Draw a dummy icon (fallback for testing/debugging)
 * Creates a simple colored square with border
 * @param x         X coordinate on screen
 * @param y         Y coordinate on screen
 * @param iconId    Icon ID (used to determine color)
 * @param bgColor   Background color for the icon
 */
void drawDummyIcon(int x, int y, int iconId, RGBA bgColor) {
	RGBA iconColor;
	iconColor.A = 255;

	// Choose color based on iconId modulo 4
	switch (iconId % 4) {
	case 0: // Dark gray
		iconColor.R = 40;
		iconColor.G = 44;
		iconColor.B = 52;
		break;
	case 1: // Blue
		iconColor.R = 33;
		iconColor.G = 150;
		iconColor.B = 243;
		break;
	case 2: // Orange
		iconColor.R = 255;
		iconColor.G = 152;
		iconColor.B = 0;
		break;
	case 3: // Green
		iconColor.R = 76;
		iconColor.G = 175;
		iconColor.B = 80;
		break;
	}

	// Draw background
	drawFillRect(&desktop, bgColor, x, y, ICON_SIZE, ICON_SIZE);

	// Draw inner colored rectangle with padding
	int padding = 8;
	drawFillRect(&desktop, iconColor, x + padding, y + padding,
		     ICON_SIZE - padding * 2, ICON_SIZE - padding * 2);

	// Draw border
	RGB borderColor;
	borderColor.R = 200;
	borderColor.G = 200;
	borderColor.B = 200;
	drawRect(&desktop, borderColor, x, y, ICON_SIZE, ICON_SIZE);
}

/**
 * Render all active desktop applications
 * Draws both icons and text labels for each app
 */
void renderAllApps() {
	for (int i = 0; i < MAX_APPS; i++) {
		// Skip inactive apps
		if (!apps[i].isActive)
			continue;

		// Draw the PNG icon (without background, transparency
		// supported)
		drawAppIcon(apps[i].x, apps[i].y, apps[i].iconId);

		// Draw the text label below the icon
		int labelY = apps[i].y + ICON_SIZE +
			     5; // 5px gap between icon and label
		int textLen = strlen(apps[i].name);
		int textWidth = textLen * 9; // Approximate character width

		// Center the text horizontally under the icon
		int textX = apps[i].x;
		if (textWidth <= ICON_SIZE) {
			textX = apps[i].x + (ICON_SIZE - textWidth) / 2;
		}

		// Render the text label
		drawString(&desktop, apps[i].name, textColor, textX, labelY, 80,
			   LABEL_HEIGHT);
	}
}

/**
 * Find which application is at the given mouse position
 * @param mouseX    Mouse X coordinate
 * @param mouseY    Mouse Y coordinate
 * @return          App index if found, -1 if no app at position
 */
int findAppAtPosition(int mouseX, int mouseY) {
	// Iterate backwards so top apps are checked first
	for (int i = MAX_APPS - 1; i >= 0; i--) {
		if (!apps[i].isActive)
			continue;

		// Define the clickable area for this app
		int x = apps[i].x;
		int y = apps[i].y;
		int w = ICON_SIZE;
		int h = APP_TOTAL_HEIGHT; // Icon + label height

		// Check if mouse is within the app's bounds
		if (mouseX >= x && mouseX <= x + w && mouseY >= y &&
		    mouseY <= y + h) {
			return i;
		}
	}
	return -1; // No app found at this position
}

/**
 * Add a new application to the desktop
 * @param name      Display name of the app
 * @param exec      Executable file name
 * @param iconId    Icon ID from APP_ICON_* enum
 * @param x         Initial X position
 * @param y         Initial Y position
 * @return          Index of added app, or -1 if desktop is full
 */
int addDesktopApp(char *name, char *exec, int iconId, int x, int y) {
	// Find first available slot
	int idx = -1;
	for (int i = 0; i < MAX_APPS; i++) {
		if (!apps[i].isActive) {
			idx = i;
			break;
		}
	}

	// Return error if desktop is full
	if (idx == -1)
		return -1;

	// Initialize the app data
	strcpy(apps[idx].name, name);
	strcpy(apps[idx].exec, exec);
	apps[idx].iconId = iconId;
	apps[idx].x = x;
	apps[idx].y = y;
	apps[idx].isActive = 1;

	return idx;
}

/**
 * Handler for the start button widget
 * Launches the start window when clicked
 */
void startWindowHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		// Fork a new process to run the start window
		if (fork() == 0) {
			char *argv2[] = {"startWindow", (char *)desktop.handler,
					 0};
			exec(argv2[0], argv2);
			exit();
		}
	}
}

/**
 * Custom window update function
 * Handles all mouse events, drag & drop, double-click detection
 */
void customUpdateWindow() {
	message msg;

	// Get the next message from the message queue
	if (GUI_getMessage(desktop.handler, &msg) == 0) {

		// Handle window close event
		if (msg.msg_type == WM_WINDOW_CLOSE) {
			closeWindow(&desktop);
		}

		// Extract mouse coordinates from message
		int mouseX = msg.params[0];
		int mouseY = msg.params[1];
		int currentTime = uptime();

		// Handle mouse button press
		if (msg.msg_type == M_MOUSE_DOWN) {
			int appIdx = findAppAtPosition(mouseX, mouseY);

			if (appIdx != -1) {
				// Check for double-click (same app clicked
				// within time threshold)
				if (appIdx == lastClickApp &&
				    (currentTime - lastClickTime) <
					    DOUBLE_CLICK_TIME) {
					// Double-click detected - launch the
					// application
					if (fork() == 0) {
						char *argv2[] = {
							apps[appIdx].exec, 0};
						exec(argv2[0], argv2);
						exit();
					}
					// Reset click tracking
					lastClickApp = -1;
					lastClickTime = 0;
					draggingApp = -1;
					isDragging = 0;
				} else {
					// Single click - prepare for potential
					// drag
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

			// Handle mouse movement
		} else if (msg.msg_type == M_MOUSE_MOVE) {
			if (draggingApp != -1) {
				// Calculate mouse movement delta
				int deltaX = mouseX - dragStartX;
				int deltaY = mouseY - dragStartY;

				// Check if movement exceeds drag threshold
				if (!isDragging) {
					if (deltaX * deltaX + deltaY * deltaY >
					    DRAG_THRESHOLD * DRAG_THRESHOLD) {
						isDragging =
							1; // Start dragging
					}
				}

				// Update app position while dragging
				if (isDragging) {
					apps[draggingApp].x =
						appOriginalX + deltaX;
					apps[draggingApp].y =
						appOriginalY + deltaY;

					// Clamp position to screen bounds
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

					// Mark for repaint
					desktop.needsRepaint = 1;
				}
			}

			// Handle mouse button release or click
		} else if (msg.msg_type == M_MOUSE_LEFT_CLICK ||
			   msg.msg_type == M_MOUSE_UP) {
			if (draggingApp != -1) {
				// If it was a click (not a drag), handle widget
				// clicks
				if (!isDragging) {
					// Iterate through widgets in reverse
					// order (top to bottom)
					for (int p = desktop.widgetlisttail;
					     p != -1;
					     p = desktop.widgets[p].prev) {
						Widget *w = &desktop.widgets[p];
						// Check if click is within
						// widget bounds
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
				// Reset drag state
				draggingApp = -1;
				isDragging = 0;
				desktop.needsRepaint = 1;
			} else {
				// No drag in progress, handle widget clicks
				// normally
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

			// Handle explicit double-click event
		} else if (msg.msg_type == M_MOUSE_DBCLICK) {
			// Reset drag state on double-click
			draggingApp = -1;
			isDragging = 0;
			lastClickApp = -1;
		}
	} else {
		// No messages to process
		desktop.needsRepaint = 0;
	}

	// Repaint the screen if needed
	if (desktop.needsRepaint) {
		// Re-render gradient background (top to bottom)
		for (int y = 0; y < desktop.height; y++) {
			// Calculate interpolation factor (0.0 to 1.0)
			float factor = (float)y / desktop.height;

			// Interpolate between top and bottom colors
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

			// Draw horizontal line across the screen
			fillRect(desktop.window_buf, 0, y, desktop.width, 1,
				 desktop.width, desktop.height, currentColor);
		}

		// Render all desktop applications
		renderAllApps();

		// Render widgets (currently only the start button)
		for (int p = desktop.widgetlisthead; p != -1;
		     p = desktop.widgets[p].next) {
			if (desktop.widgets[p].type == BUTTON) {
				// Define black color for button border
				RGB black;
				black.R = 0;
				black.G = 0;
				black.B = 0;

				Widget *w = &desktop.widgets[p];
				int width = w->position.xmax - w->position.xmin;
				int height =
					w->position.ymax - w->position.ymin;

				// Center text vertically and horizontally
				int textYOffset = (height - 18) / 2;
				int textXOffset =
					(width -
					 strlen(w->context.button->text) * 9) /
					2;

				// Draw button background
				drawFillRect(&desktop,
					     w->context.button->bg_color,
					     w->position.xmin, w->position.ymin,
					     width, height);

				// Draw button border
				drawRect(&desktop, black, w->position.xmin,
					 w->position.ymin, width, height);

				// Draw button text
				drawString(&desktop, w->context.button->text,
					   w->context.button->color,
					   w->position.xmin + textXOffset,
					   w->position.ymin + textYOffset,
					   width, height);
			}
		}
	}
}

/**
 * Main entry point for the desktop application
 */
int main(int argc, char *argv[]) {
	// Initialize desktop window properties
	desktop.width = SCREEN_WIDTH;
	desktop.height = SCREEN_HEIGHT;
	desktop.initialPosition.xmin = 0;
	desktop.initialPosition.xmax = SCREEN_WIDTH;
	desktop.initialPosition.ymin = 0;
	desktop.initialPosition.ymax = SCREEN_HEIGHT;
	desktop.hasTitleBar = 0; // Desktop has no title bar

	// Create the desktop window
	createWindow(&desktop, "desktop");

	// Define gradient background colors (blue theme)
	desktopColorTop.R = 30;
	desktopColorTop.G = 60;
	desktopColorTop.B = 114;
	desktopColorTop.A = 255;

	desktopColorBottom.R = 72;
	desktopColorBottom.G = 140;
	desktopColorBottom.B = 203;
	desktopColorBottom.A = 255;

	// Create initial gradient background
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

	// Define UI element colors
	buttonColor.R = 41; // Blue button
	buttonColor.G = 128;
	buttonColor.B = 185;
	buttonColor.A = 255;

	textColor.R = 255; // White text
	textColor.G = 255;
	textColor.B = 255;
	textColor.A = 255;

	iconBgColor.R = 255; // White icon background (unused with PNG icons)
	iconBgColor.G = 255;
	iconBgColor.B = 255;
	iconBgColor.A = 200;

	// Initialize all app slots as inactive
	for (int i = 0; i < MAX_APPS; i++) {
		apps[i].isActive = 0;
	}

	// Add desktop applications with custom PNG icons
	// Format: addDesktopApp(display_name, executable_name, APP_ICON_xxx, x,
	// y)
	addDesktopApp("Terminal", "terminal", APP_ICON_TERMINAL, 20, 20);
	addDesktopApp("Editor", "editor", APP_ICON_EDITOR, 20,
		      20 + APP_TOTAL_HEIGHT + 10);
	addDesktopApp("Explorer", "explorer", APP_ICON_EXPLORER, 20,
		      20 + (APP_TOTAL_HEIGHT + 10) * 2);
	addDesktopApp("Floppy Bird", "floppybird", APP_ICON_FLOPPYBIRD, 20,
		      20 + (APP_TOTAL_HEIGHT + 10) * 3);

	// Render all apps for the first time
	renderAllApps();

	// Add the start button at bottom-left corner
	addButtonWidget(&desktop, textColor, buttonColor, "start", 5,
			SCREEN_HEIGHT - 36, 72, 36, 0, startWindowHandler);

	// Mark desktop for initial paint
	desktop.needsRepaint = 1;

	// Main event loop
	while (1) {
		customUpdateWindow(); // Process events and update window
		GUI_updateScreen();   // Refresh the screen display
	}
}