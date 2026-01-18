#include "types.h"
#include "user.h"
#include "character.h"
#include "user_window.h"

void emptyHandler(Widget *w, message *msg) { return; }

int getInputOffsetFromMousePosition(char *str, int width, int mouse_x,
				    int mouse_y) {

	// printf(1, "mouse int at %d, %d\n", mouse_x, mouse_y);
	int charPerLine = width / CHARACTER_WIDTH;
	int offset_x = 0;
	int offset_y = 0;
	int current_pos = 0;
	int first_y_match_pos = 0;
	if (mouse_y < 0)
		return 0;
	while (*str != '\0') {
		if (mouse_y == offset_y) {
			if (mouse_x == offset_x) {
				return current_pos;
			}
			first_y_match_pos = current_pos;
		}
		if (*str != '\n') {
			offset_x += 1;
			if (offset_x > charPerLine) {
				offset_x = 0;
				offset_y += 1;
			}
		} else {
			offset_x = 0;
			offset_y += 1;
		}

		str++;
		current_pos++;
	}
	if (first_y_match_pos > 0) {
		return first_y_match_pos;
	}
	return current_pos;
}

int getMouseXFromOffset(char *str, int width, int offset) {

	int charPerLine = width / CHARACTER_WIDTH;
	int offset_x = 0;
	int offset_y = 0;
	int iter = 0;
	while (iter < offset) {
		if (str[iter] != '\n') {
			offset_x += 1;
			if (offset_x > charPerLine) {
				offset_x = 0;
				offset_y += 1;
			}
		} else {
			offset_x = 0;
			offset_y += 1;
		}
		iter++;
	}
	return offset_x;
}

int getMouseYFromOffset(char *str, int width, int offset) {

	int charPerLine = width / CHARACTER_WIDTH;
	int offset_x = 0;
	int offset_y = 0;
	int iter = 0;
	while (iter < offset) {
		if (str[iter] != '\n') {
			offset_x += 1;
			if (offset_x > charPerLine) {
				offset_x = 0;
				offset_y += 1;
			}
		} else {
			offset_x = 0;
			offset_y += 1;
		}
		iter++;
	}
	return offset_y;
}

int getScrollableTotalHeight(window *win) {

	int totalHeight = 0;
	int p;
	for (p = win->widgetlisthead; p != -1; p = win->widgets[p].next) {
		if (win->widgets[p].scrollable &&
		    win->widgets[p].position.ymax > totalHeight) {
			totalHeight = win->widgets[p].position.ymax;
		}
	}
	return totalHeight;
	;
}

// scrollbar needs more implementation
int addScrollBarWidget(window *window, RGBA color, Handler scrollBarHandler) {
	// int totalHeight = getScrollableTotalHeight(window);
	// int scrollableHeight = ((float)window->height / totalHeight) *
	// window->height;
	return addColorFillWidget(window, color, window->width - 20, 0,
				  window->width, window->height, 0,
				  scrollBarHandler);
}

// change text cursor from mouse click
void inputMouseLeftClickHandler(Widget *w, message *msg) {
	if (msg->msg_type != M_MOUSE_LEFT_CLICK)
		return;

	int mouse_x = msg->params[0];
	int mouse_y = msg->params[1];
	int width = w->position.xmax - w->position.xmin;

	int mouse_char_y = (mouse_y - w->position.ymin) / CHARACTER_HEIGHT;
	int mouse_char_x = (mouse_x - w->position.xmin) / CHARACTER_WIDTH;
	int new_pos = getInputOffsetFromMousePosition(
		w->context.inputfield->text, width, mouse_char_x, mouse_char_y);

	w->context.inputfield->current_pos = new_pos;
}

void inputFieldKeyHandler(Widget *w, message *msg) {
	if (msg->msg_type != M_KEY_DOWN)
		return;
	int width = w->position.xmax - w->position.xmin;
	;
	int charCount = strlen(w->context.inputfield->text);
	int newChar = msg->params[0];

	// currently supported ASCII characters
	if ((newChar >= ' ' && newChar <= '~') || newChar == '\n') {
		char temp[MAX_LONG_STRLEN];
		strcpy(temp, w->context.inputfield->text +
				     w->context.inputfield->current_pos);
		w->context.inputfield
			->text[w->context.inputfield->current_pos++] = newChar;
		strcpy(w->context.inputfield->text +
			       w->context.inputfield->current_pos,
		       temp);
	}
	// handle arrow keys to change text cursor (both main keyboard and
	// numpad) Left arrow: KEY_LF or numpad 4
	if ((newChar == KEY_LF || newChar == '4') &&
	    w->context.inputfield->current_pos > 0) {
		w->context.inputfield->current_pos--;
	}
	// Right arrow: KEY_RT or numpad 6
	if ((newChar == KEY_RT || newChar == '6') &&
	    w->context.inputfield->current_pos < charCount) {
		w->context.inputfield->current_pos++;
	}

	int current_pos_mouse_X =
		getMouseXFromOffset(w->context.inputfield->text, width,
				    w->context.inputfield->current_pos);
	int current_pos_mouse_Y =
		getMouseYFromOffset(w->context.inputfield->text, width,
				    w->context.inputfield->current_pos);
	// Up arrow: KEY_UP or numpad 8
	if (newChar == KEY_UP || newChar == '8') {
		w->context.inputfield->current_pos =
			getInputOffsetFromMousePosition(
				w->context.inputfield->text, width,
				current_pos_mouse_X, current_pos_mouse_Y - 1);
	}
	// Down arrow: KEY_DN or numpad 2
	if (newChar == KEY_DN || newChar == '2') {
		w->context.inputfield->current_pos =
			getInputOffsetFromMousePosition(
				w->context.inputfield->text, width,
				current_pos_mouse_X, current_pos_mouse_Y + 1);
	}
	// handle delete key
	if (newChar == '\b' && w->context.inputfield->current_pos > 0) {
		char temp[MAX_LONG_STRLEN];
		strcpy(temp, w->context.inputfield->text +
				     w->context.inputfield->current_pos);
		strcpy(w->context.inputfield->text +
			       w->context.inputfield->current_pos - 1,
		       temp);
		w->context.inputfield->current_pos--;
	}
}