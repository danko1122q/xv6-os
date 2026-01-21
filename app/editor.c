#include "fcntl.h"
#include "character.h"
#include "gui.h"
#include "msg.h"
#include "types.h"
#include "user.h"
#include "user_gui.h"
#include "user_handler.h"
#include "user_window.h"
#include "kbd.h"

// Constants
#define MAX_FILENAME_LEN 40
#define WINDOW_WIDTH 700
#define WINDOW_HEIGHT 550
#define BUTTON_WIDTH 50
#define BUTTON_HEIGHT 25
#define BUTTON_MARGIN 8
#define TOOLBAR_HEIGHT 40
#define EDITOR_X 10
#define EDITOR_Y 50

// Global state
window editorWindow;
char filename[MAX_FILENAME_LEN];
int isModified = 0;

// Colors
struct RGBA createColor(int r, int g, int b, int a) {
	struct RGBA color;
	color.R = r;
	color.G = g;
	color.B = b;
	color.A = a;
	return color;
}

// Get input widget
Widget *getInputWidget() {
	int i;
	for (i = editorWindow.widgetlisthead; i != -1;
	     i = editorWindow.widgets[i].next) {
		if (editorWindow.widgets[i].type == INPUTFIELD) {
			return &editorWindow.widgets[i];
		}
	}
	return 0;
}

// Simple safe key handler - avoids ALL buggy functions
void safeKeyHandler(Widget *w, message *msg) {
	if (msg->msg_type != M_KEY_DOWN)
		return;

	int key = msg->params[0];
	char *text = w->context.inputfield->text;
	int *pos = &w->context.inputfield->current_pos;
	int len = strlen(text);

	// Keep position in bounds
	if (*pos < 0)
		*pos = 0;
	if (*pos > len)
		*pos = len;

	// Printable characters and newline
	if ((key >= ' ' && key <= '~') || key == '\n') {
		if (len < MAX_LONG_STRLEN - 1) {
			// Shift text right
			int i;
			for (i = len; i >= *pos; i--) {
				text[i + 1] = text[i];
			}
			text[*pos] = key;
			(*pos)++;
			isModified = 1;
		}
	}
	// Backspace
	else if (key == '\b' && *pos > 0) {
		// Shift text left
		int i;
		for (i = *pos - 1; i < len; i++) {
			text[i] = text[i + 1];
		}
		(*pos)--;
		isModified = 1;
	}
	// Delete key - delete character at cursor
	else if (key == KEY_DEL && *pos < len) {
		int i;
		for (i = *pos; i < len; i++) {
			text[i] = text[i + 1];
		}
		isModified = 1;
	}
	// Left arrow (main keyboard or numpad 4)
	else if ((key == KEY_LF || key == KEY_KP_4 || key == '4') && *pos > 0) {
		(*pos)--;
	}
	// Right arrow (main keyboard or numpad 6)
	else if ((key == KEY_RT || key == KEY_KP_6 || key == '6') &&
		 *pos < len) {
		(*pos)++;
	}
	// Up arrow (main keyboard or numpad 8) - move to previous line
	else if (key == KEY_UP || key == KEY_KP_8 || key == '8') {
		// Find start of current line
		int lineStart = *pos;
		while (lineStart > 0 && text[lineStart - 1] != '\n') {
			lineStart--;
		}

		if (lineStart > 0) {
			// There is a previous line
			int prevLineEnd =
				lineStart -
				1; // Position of \n before current line
			int prevLineStart = prevLineEnd;
			while (prevLineStart > 0 &&
			       text[prevLineStart - 1] != '\n') {
				prevLineStart--;
			}

			// Calculate column in current line
			int col = *pos - lineStart;

			// Try to go to same column in previous line
			int prevLineLen = prevLineEnd - prevLineStart;
			if (col > prevLineLen)
				col = prevLineLen;

			*pos = prevLineStart + col;
		} else {
			// Already on first line, go to start
			*pos = 0;
		}
	}
	// Down arrow (main keyboard or numpad 2) - move to next line
	else if (key == KEY_DN || key == KEY_KP_2 || key == '2') {
		// Find start of current line
		int lineStart = *pos;
		while (lineStart > 0 && text[lineStart - 1] != '\n') {
			lineStart--;
		}

		// Find end of current line
		int lineEnd = *pos;
		while (lineEnd < len && text[lineEnd] != '\n') {
			lineEnd++;
		}

		if (lineEnd < len) {
			// There is a next line
			int nextLineStart = lineEnd + 1;

			// Calculate column in current line
			int col = *pos - lineStart;

			// Find end of next line
			int nextLineEnd = nextLineStart;
			while (nextLineEnd < len && text[nextLineEnd] != '\n') {
				nextLineEnd++;
			}

			// Try to go to same column in next line
			int nextLineLen = nextLineEnd - nextLineStart;
			if (col > nextLineLen)
				col = nextLineLen;

			*pos = nextLineStart + col;
		} else {
			// Already on last line, go to end
			*pos = len;
		}
	}
	// Home - go to start of line
	else if (key == KEY_HOME || key == KEY_KP_7) {
		while (*pos > 0 && text[*pos - 1] != '\n') {
			(*pos)--;
		}
	}
	// End - go to end of line
	else if (key == KEY_END || key == KEY_KP_1) {
		while (*pos < len && text[*pos] != '\n') {
			(*pos)++;
		}
	}
	// Page Up - move up several lines
	else if (key == KEY_PGUP || key == KEY_KP_9) {
		int i;
		for (i = 0; i < 10; i++) {
			// Simulate up arrow
			message upMsg = *msg;
			upMsg.params[0] = KEY_UP;
			safeKeyHandler(w, &upMsg);
		}
		return; // Position already updated
	}
	// Page Down - move down several lines
	else if (key == KEY_PGDN || key == KEY_KP_3) {
		int i;
		for (i = 0; i < 10; i++) {
			// Simulate down arrow
			message downMsg = *msg;
			downMsg.params[0] = KEY_DN;
			safeKeyHandler(w, &downMsg);
		}
		return; // Position already updated
	}

	// Keep position valid
	len = strlen(text); // Recalculate in case text changed
	if (*pos < 0)
		*pos = 0;
	if (*pos > len)
		*pos = len;

	editorWindow.needsRepaint = 1;
}

// Input handler wrapper
void editorInputHandler(Widget *w, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		// Simple click handling - set cursor to approximate position
		int mouse_x = msg->params[0];
		int mouse_y = msg->params[1];

		// Calculate approximate character position
		int char_x = (mouse_x - w->position.xmin) / CHARACTER_WIDTH;
		int char_y = (mouse_y - w->position.ymin) / CHARACTER_HEIGHT;

		if (char_x < 0)
			char_x = 0;
		if (char_y < 0)
			char_y = 0;

		// Find position in text
		char *text = w->context.inputfield->text;
		int pos = 0;
		int current_line = 0;
		int current_col = 0;
		int len = strlen(text);

		while (pos < len) {
			if (current_line == char_y && current_col >= char_x) {
				break;
			}

			if (text[pos] == '\n') {
				if (current_line == char_y) {
					// Clicked at end of this line
					break;
				}
				current_line++;
				current_col = 0;
			} else {
				current_col++;
			}
			pos++;
		}

		w->context.inputfield->current_pos = pos;
		editorWindow.needsRepaint = 1;
	} else if (msg->msg_type == M_KEY_DOWN) {
		safeKeyHandler(w, msg);
	}
}

// Save button
void saveHandler(Widget *widget, message *msg) {
	if (msg->msg_type != M_MOUSE_LEFT_CLICK &&
	    msg->msg_type != M_MOUSE_DBCLICK) {
		return;
	}

	Widget *input = getInputWidget();
	if (!input) {
		printf(1, "Error: Input widget not found\n");
		return;
	}

	char *file = filename[0] != '\0' ? filename : "untitled.txt";

	int fd = open(file, O_RDWR | O_CREATE);
	if (fd < 0) {
		printf(1, "Error: Cannot save file '%s'\n", file);
		return;
	}

	char *text = input->context.inputfield->text;
	int textlen = strlen(text);
	int written = write(fd, text, textlen);
	close(fd);

	if (written != textlen) {
		printf(1, "Warning: File may not be saved completely\n");
	}

	isModified = 0;
	printf(1, "Saved: %s (%d bytes)\n", file, textlen);
}

// New button
void newHandler(Widget *widget, message *msg) {
	if (msg->msg_type != M_MOUSE_LEFT_CLICK &&
	    msg->msg_type != M_MOUSE_DBCLICK) {
		return;
	}

	// Confirm if modified
	if (isModified) {
		printf(1, "Warning: Unsaved changes will be lost\n");
	}

	Widget *input = getInputWidget();
	if (!input)
		return;

	memset(input->context.inputfield->text, 0, MAX_LONG_STRLEN);
	input->context.inputfield->current_pos = 0;
	memset(filename, 0, MAX_FILENAME_LEN);
	isModified = 0;
	editorWindow.needsRepaint = 1;
	printf(1, "New file created\n");
}

// Load file
int loadFile(char *path, char *buffer, int maxLen) {
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		printf(1, "Error: Cannot open file '%s'\n", path);
		return -1;
	}

	int n = read(fd, buffer, maxLen - 1);
	close(fd);

	if (n >= 0) {
		buffer[n] = '\0';
		printf(1, "Loaded: %s (%d bytes)\n", path, n);
		return n;
	}

	printf(1, "Error: Cannot read file '%s'\n", path);
	return -1;
}

// Initialize
void initEditor(char *file) {
	editorWindow.width = WINDOW_WIDTH;
	editorWindow.height = WINDOW_HEIGHT;
	editorWindow.hasTitleBar = 1;
	editorWindow.scrollOffsetY = 0;
	editorWindow.scrollOffsetX = 0;
	editorWindow.needsRepaint = 1;

	char title[60];
	if (file && file[0] != '\0') {
		strcpy(title, "Editor - ");
		strcat(title, file);
		strcpy(filename, file);
	} else {
		strcpy(title, "Text Editor");
		memset(filename, 0, MAX_FILENAME_LEN);
	}

	createWindow(&editorWindow, title);

	// Background
	addColorFillWidget(&editorWindow, createColor(250, 250, 250, 255), 0, 0,
			   WINDOW_WIDTH, WINDOW_HEIGHT, 0, emptyHandler);

	// Toolbar background
	addColorFillWidget(&editorWindow, createColor(240, 240, 240, 255), 0, 0,
			   WINDOW_WIDTH, TOOLBAR_HEIGHT, 0, emptyHandler);

	// Load text
	char text[MAX_LONG_STRLEN];
	memset(text, 0, MAX_LONG_STRLEN);

	if (file && file[0] != '\0') {
		if (loadFile(filename, text, MAX_LONG_STRLEN) < 0) {
			printf(1, "Starting with empty file\n");
		}
	}

	// Input field - NOT SCROLLABLE
	int editorHeight = WINDOW_HEIGHT - EDITOR_Y - 10;
	addInputFieldWidget(&editorWindow, createColor(40, 40, 40, 255), text,
			    EDITOR_X, EDITOR_Y, WINDOW_WIDTH - EDITOR_X * 2,
			    editorHeight, 0, editorInputHandler);

	// Save button
	addButtonWidget(&editorWindow, createColor(255, 255, 255, 255),
			createColor(76, 175, 80, 255), "Save", BUTTON_MARGIN,
			BUTTON_MARGIN, BUTTON_WIDTH, BUTTON_HEIGHT, 0,
			saveHandler);

	// New button
	addButtonWidget(&editorWindow, createColor(255, 255, 255, 255),
			createColor(33, 150, 243, 255), "New",
			BUTTON_MARGIN + BUTTON_WIDTH + 8, BUTTON_MARGIN,
			BUTTON_WIDTH, BUTTON_HEIGHT, 0, newHandler);

	printf(1, "Text Editor initialized\n");
	printf(1, "Features: Save, New, Arrow keys, Home/End, PgUp/PgDn\n");
}

int main(int argc, char *argv[]) {
	char *file = 0;

	if (argc > 1) {
		file = argv[1];
	}

	printf(1, "Starting Text Editor...\n");
	initEditor(file);

	while (1) {
		updateWindow(&editorWindow);
	}

	return 0;
}
