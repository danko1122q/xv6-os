#include "fcntl.h"
#include "gui.h"
#include "memlayout.h"
#include "types.h"
#include "user.h"
#include "user_gui.h"
#include "user_window.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define MENU_WIDTH 220
#define MENU_HEIGHT 420
#define BUTTON_HEIGHT 45
#define BUTTON_SPACING 5
#define BUTTON_PADDING_X 10 // Kurangi padding jika tombol terasa penuh
#define BUTTON_PADDING_Y 10
#define SECTION_SPACING 15
#define SCROLLBAR_WIDTH 8
#define HEADER_HEIGHT 50

window startWindow;
int scrollOffset = 0;
int maxScrollOffset = 0;
int contentHeight = 0;

// Program list
typedef struct {
	char *name;    // Executable name (for exec)
	char *display; // Display name (for UI)
	int isSystem;
} ProgramItem;

ProgramItem programs[] = {{"terminal", "Terminal", 0},
			  {"editor", "Text Editor", 0},
			  {"explorer", "File Explorer", 0},
			  {"floppybird", "Flappy Bird", 0}};

#define PROGRAM_COUNT 4

// Color helper
struct RGBA createColor(int r, int g, int b, int a) {
	struct RGBA color;
	color.R = r;
	color.G = g;
	color.B = b;
	color.A = a;
	return color;
}

// Find program by display name
char *findProgramName(char *displayName) {
	int i;
	for (i = 0; i < PROGRAM_COUNT; i++) {
		if (strcmp(programs[i].display, displayName) == 0) {
			return programs[i].name;
		}
	}
	return 0;
}

// Program launcher
void startProgramHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		char *displayName = widget->context.button->text;
		char *execName = findProgramName(displayName);

		if (!execName) {
			printf(1, "Error: Program not found: %s\n",
			       displayName);
			return;
		}

		if (fork() == 0) {
			printf(1, "Starting: %s (%s)\n", displayName, execName);
			char *argv2[] = {execName, 0};
			exec(execName, argv2);
			printf(1, "Error: Failed to exec %s\n", execName);
			exit();
		}
	}
}

// Restart handler
void rebootHandler(Widget *widget, message *msg) {
	(void)widget;
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		printf(1, "System restarting...\n");
		reboot();
	}
}

// Shutdown handler
void shutdownHandler(Widget *widget, message *msg) {
	(void)widget;
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		printf(1, "System shutting down...\n");
		halt();
	}
}

// Scroll handler - keyboard only
void scrollHandler(Widget *widget, message *msg) {
	(void)widget;

	if (msg->msg_type == M_KEY_DOWN) {
		int key = msg->params[0];
		int oldScroll = scrollOffset;

		// Up arrow or numpad 8
		if (key == KEY_UP || key == '8') {
			scrollOffset -= 20;
		}
		// Down arrow or numpad 2
		else if (key == KEY_DN || key == '2') {
			scrollOffset += 20;
		}
		// Page Up
		else if (key == KEY_PGUP) {
			scrollOffset -= MENU_HEIGHT / 2;
		}
		// Page Down
		else if (key == KEY_PGDN) {
			scrollOffset += MENU_HEIGHT / 2;
		}
		// Home - scroll to top
		else if (key == KEY_HOME) {
			scrollOffset = 0;
		}
		// End - scroll to bottom
		else if (key == KEY_END) {
			scrollOffset = maxScrollOffset;
		}

		// Clamp scroll offset
		if (scrollOffset < 0)
			scrollOffset = 0;
		if (scrollOffset > maxScrollOffset)
			scrollOffset = maxScrollOffset;

		// Repaint if scrolled
		if (scrollOffset != oldScroll) {
			startWindow.needsRepaint = 1;
			printf(1, "Scroll: %d/%d\n", scrollOffset,
			       maxScrollOffset);
		}
	}
}

// Render widgets at proper scroll position
void renderContent(void) {
	int i;
	int currentY;
	struct RGBA bgColor;
	struct RGBA appBgColor;
	struct RGBA textColor;
	struct RGBA dangerColor;
	struct RGBA dividerColor;
	struct RGBA scrollbarBg;
	struct RGBA scrollbarThumb;

	// Colors
	bgColor = createColor(45, 45, 48, 255);
	appBgColor = createColor(60, 60, 65, 255);
	textColor = createColor(240, 240, 240, 255);
	dangerColor = createColor(232, 17, 35, 255);
	dividerColor = createColor(80, 80, 85, 255);
	scrollbarBg = createColor(50, 50, 52, 255);
	scrollbarThumb = createColor(100, 100, 105, 255);

	// Main scrollable background
	addColorFillWidget(&startWindow, bgColor, 0, HEADER_HEIGHT, MENU_WIDTH,
			   MENU_HEIGHT - HEADER_HEIGHT, 0, scrollHandler);

	// Start content with scroll offset
	currentY = HEADER_HEIGHT + 5 - scrollOffset;

	// Applications section
	for (i = 0; i < PROGRAM_COUNT; i++) {
		// Only render if in visible area
		if (currentY + BUTTON_HEIGHT >= HEADER_HEIGHT &&
		    currentY < MENU_HEIGHT) {
			addButtonWidget(&startWindow, textColor, appBgColor,
					programs[i].display, BUTTON_PADDING_X,
					currentY,
					MENU_WIDTH - (BUTTON_PADDING_X * 2) -
						SCROLLBAR_WIDTH - 5,
					BUTTON_HEIGHT, 0, startProgramHandler);
		}
		currentY += BUTTON_HEIGHT + BUTTON_SPACING;
	}

	currentY += SECTION_SPACING;

	// Divider
	if (currentY >= HEADER_HEIGHT && currentY < MENU_HEIGHT) {
		addColorFillWidget(&startWindow, dividerColor, BUTTON_PADDING_X,
				   currentY,
				   MENU_WIDTH - (BUTTON_PADDING_X * 2) -
					   SCROLLBAR_WIDTH - 5,
				   2, 0, emptyHandler);
	}

	currentY += SECTION_SPACING;

	// System label
	if (currentY >= HEADER_HEIGHT && currentY < MENU_HEIGHT) {
		addButtonWidget(&startWindow, textColor, bgColor, "System",
				BUTTON_PADDING_X, currentY, 80, 25, 0,
				emptyHandler);
	}

	currentY += 35;

	// System buttons
	if (currentY >= HEADER_HEIGHT &&
	    currentY + BUTTON_HEIGHT < MENU_HEIGHT) {
		int btnWidth = (MENU_WIDTH - (BUTTON_PADDING_X * 3) -
				SCROLLBAR_WIDTH - 5) /
			       2;

		addButtonWidget(&startWindow, textColor, dangerColor, "Restart",
				BUTTON_PADDING_X, currentY, btnWidth,
				BUTTON_HEIGHT, 0, rebootHandler);

		addButtonWidget(
			&startWindow, textColor, dangerColor, "Shutdown",
			BUTTON_PADDING_X + btnWidth + BUTTON_PADDING_X,
			currentY, btnWidth, BUTTON_HEIGHT, 0, shutdownHandler);
	}

	// Scrollbar background
	addColorFillWidget(&startWindow, scrollbarBg,
			   MENU_WIDTH - SCROLLBAR_WIDTH - 2, HEADER_HEIGHT,
			   SCROLLBAR_WIDTH, MENU_HEIGHT - HEADER_HEIGHT, 0,
			   emptyHandler);

	// Scrollbar thumb (if scrollable)
	if (maxScrollOffset > 0) {
		int visibleHeight = MENU_HEIGHT - HEADER_HEIGHT;
		int thumbHeight =
			(visibleHeight * visibleHeight) / contentHeight;
		if (thumbHeight < 20)
			thumbHeight = 20;

		int thumbPosition =
			(scrollOffset * (visibleHeight - thumbHeight)) /
			maxScrollOffset;

		addColorFillWidget(&startWindow, scrollbarThumb,
				   MENU_WIDTH - SCROLLBAR_WIDTH - 2,
				   HEADER_HEIGHT + thumbPosition,
				   SCROLLBAR_WIDTH, thumbHeight, 0,
				   emptyHandler);
	}
}

int main(int argc, char *argv[]) {
	int caller;
	struct RGBA headerBg;
	struct RGBA textColor;

	(void)argc;
	caller = (int)argv[1];

	// Window setup
	startWindow.width = MENU_WIDTH;
	startWindow.height = MENU_HEIGHT;
	startWindow.initialPosition.xmin = 0;
	startWindow.initialPosition.xmax = startWindow.width;
	startWindow.initialPosition.ymin =
		SCREEN_HEIGHT - DOCK_HEIGHT - startWindow.height;
	startWindow.initialPosition.ymax = SCREEN_HEIGHT - DOCK_HEIGHT;
	startWindow.hasTitleBar = 0;

	createPopupWindow(&startWindow, caller);

	// Calculate scrollable content
	contentHeight = HEADER_HEIGHT +
			(PROGRAM_COUNT * (BUTTON_HEIGHT + BUTTON_SPACING)) +
			SECTION_SPACING + 2 + SECTION_SPACING + 25 + 35 +
			BUTTON_HEIGHT + 20;

	maxScrollOffset = contentHeight - MENU_HEIGHT;
	if (maxScrollOffset < 0)
		maxScrollOffset = 0;

	printf(1, "Start Menu: content=%d, max_scroll=%d\n", contentHeight,
	       maxScrollOffset);

	// Colors
	headerBg = createColor(30, 30, 32, 255);
	textColor = createColor(240, 240, 240, 255);

	// Fixed header (always on top)
	addColorFillWidget(&startWindow, headerBg, 0, 0, MENU_WIDTH,
			   HEADER_HEIGHT, 0, emptyHandler);

	addButtonWidget(&startWindow, textColor, headerBg, "Programs",
			BUTTON_PADDING_X, BUTTON_PADDING_Y, 100, 30, 0,
			emptyHandler);

	// Render content
	renderContent();

	printf(1, "Start Menu ready!\n");
	printf(1, "Programs: terminal, editor, explorer, floppybird\n");
	if (maxScrollOffset > 0) {
		printf(1, "Use arrows/PgUp/PgDn/Home/End to scroll\n");
	}

	while (1) {
		updatePopupWindow(&startWindow);

		// Re-render if scroll changed (widget positions changed)
		if (startWindow.needsRepaint) {
			// Clear widgets except header
			int i;
			for (i = startWindow.widgetlisthead; i != -1;) {
				int next = startWindow.widgets[i].next;
				// Keep only first 2 widgets (header bg and
				// title)
				if (i > 1) {
					removeWidget(&startWindow, i);
				}
				i = next;
			}
			renderContent();
		}
	}

	return 0;
}