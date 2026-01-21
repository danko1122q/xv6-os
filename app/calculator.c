#include "fcntl.h"
#include "gui.h"
#include "msg.h"
#include "types.h"
#include "user.h"
#include "user_gui.h"
#include "user_handler.h"
#include "user_window.h"

// Window dimensions
#define WINDOW_WIDTH 320
#define WINDOW_HEIGHT 480

// Display dimensions
#define DISPLAY_HEIGHT 80
#define DISPLAY_MARGIN 10

// Button dimensions
#define BUTTON_SIZE 70
#define BUTTON_MARGIN 5
#define BUTTON_START_Y (DISPLAY_HEIGHT + DISPLAY_MARGIN * 2)

// Calculator state
typedef struct {
	char display[32];
	char operation;
	double currentValue;
	double storedValue;
	int newNumber;
	int hasDecimal;
} CalcState;

window calcWindow;
CalcState state;

// Color definitions
RGBA displayBg;
RGBA displayText;
RGBA buttonBg;
RGBA buttonText;
RGBA operatorBg;
RGBA operatorText;
RGBA equalBg;
RGBA clearBg;

// Widget IDs
int displayWidget;
int buttonWidgets[20];

// Helper functions
void initState() {
	strcpy(state.display, "0");
	state.operation = 0;
	state.currentValue = 0.0;
	state.storedValue = 0.0;
	state.newNumber = 1;
	state.hasDecimal = 0;
}

double stringToDouble(char *str) {
	double result = 0.0;
	double fraction = 0.0;
	int sign = 1;
	int afterDecimal = 0;
	int decimalPlaces = 0;

	if (*str == '-') {
		sign = -1;
		str++;
	}

	while (*str) {
		if (*str == '.') {
			afterDecimal = 1;
		} else if (*str >= '0' && *str <= '9') {
			if (afterDecimal) {
				fraction = fraction * 10 + (*str - '0');
				decimalPlaces++;
			} else {
				result = result * 10 + (*str - '0');
			}
		}
		str++;
	}

	for (int i = 0; i < decimalPlaces; i++) {
		fraction /= 10.0;
	}

	return sign * (result + fraction);
}

void doubleToString(double value, char *buffer) {
	int intPart = (int)value;
	double fracPart = value - intPart;
	
	if (fracPart < 0) fracPart = -fracPart;

	// Handle negative
	int pos = 0;
	if (value < 0 && intPart == 0) {
		buffer[pos++] = '-';
	}

	// Integer part
	char temp[16];
	int tempPos = 0;
	int num = intPart;
	
	if (num < 0) num = -num;
	
	if (num == 0) {
		temp[tempPos++] = '0';
	} else {
		while (num > 0) {
			temp[tempPos++] = '0' + (num % 10);
			num /= 10;
		}
		if (intPart < 0) {
			temp[tempPos++] = '-';
		}
	}

	// Reverse
	for (int i = tempPos - 1; i >= 0; i--) {
		buffer[pos++] = temp[i];
	}

	// Fractional part
	if (fracPart > 0.0000001) {
		buffer[pos++] = '.';
		for (int i = 0; i < 6; i++) {
			fracPart *= 10;
			int digit = (int)fracPart;
			buffer[pos++] = '0' + digit;
			fracPart -= digit;
			if (fracPart < 0.0000001) break;
		}
	}

	buffer[pos] = '\0';
}

void updateDisplay() {
	strcpy(calcWindow.widgets[displayWidget].context.text->text,
	       state.display);
	calcWindow.needsRepaint = 1;
}

void performOperation() {
	double current = stringToDouble(state.display);
	double result = state.storedValue;

	switch (state.operation) {
	case '+':
		result = state.storedValue + current;
		break;
	case '-':
		result = state.storedValue - current;
		break;
	case '*':
		result = state.storedValue * current;
		break;
	case '/':
		if (current != 0) {
			result = state.storedValue / current;
		} else {
			strcpy(state.display, "Error");
			updateDisplay();
			return;
		}
		break;
	default:
		result = current;
	}

	state.currentValue = result;
	state.storedValue = result;
	doubleToString(result, state.display);
	updateDisplay();
}

// Button handlers
void digitHandler(Widget *w, message *msg) {
	if (msg->msg_type != M_MOUSE_LEFT_CLICK)
		return;

	char *digit = w->context.button->text;

	if (state.newNumber) {
		strcpy(state.display, digit);
		state.newNumber = 0;
		state.hasDecimal = 0;
	} else {
		if (strlen(state.display) < 15) {
			strcat(state.display, digit);
		}
	}
	updateDisplay();
}

void decimalHandler(Widget *w, message *msg) {
	if (msg->msg_type != M_MOUSE_LEFT_CLICK)
		return;

	if (state.newNumber) {
		strcpy(state.display, "0.");
		state.newNumber = 0;
		state.hasDecimal = 1;
	} else if (!state.hasDecimal) {
		strcat(state.display, ".");
		state.hasDecimal = 1;
	}
	updateDisplay();
}

void operatorHandler(Widget *w, message *msg) {
	if (msg->msg_type != M_MOUSE_LEFT_CLICK)
		return;

	if (state.operation && !state.newNumber) {
		performOperation();
	} else {
		state.storedValue = stringToDouble(state.display);
	}

	state.operation = w->context.button->text[0];
	state.newNumber = 1;
	state.hasDecimal = 0;
}

void equalHandler(Widget *w, message *msg) {
	if (msg->msg_type != M_MOUSE_LEFT_CLICK)
		return;

	if (state.operation) {
		performOperation();
		state.operation = 0;
	}
	state.newNumber = 1;
	state.hasDecimal = 0;
}

void clearHandler(Widget *w, message *msg) {
	if (msg->msg_type != M_MOUSE_LEFT_CLICK)
		return;

	initState();
	updateDisplay();
}

void backspaceHandler(Widget *w, message *msg) {
	if (msg->msg_type != M_MOUSE_LEFT_CLICK)
		return;

	int len = strlen(state.display);
	if (len > 1) {
		if (state.display[len - 1] == '.') {
			state.hasDecimal = 0;
		}
		state.display[len - 1] = '\0';
	} else {
		strcpy(state.display, "0");
		state.newNumber = 1;
	}
	updateDisplay();
}

void negateHandler(Widget *w, message *msg) {
	if (msg->msg_type != M_MOUSE_LEFT_CLICK)
		return;

	double value = stringToDouble(state.display);
	value = -value;
	doubleToString(value, state.display);
	updateDisplay();
}

void initColors() {
	// Dark theme colors
	displayBg.R = 40;
	displayBg.G = 44;
	displayBg.B = 52;
	displayBg.A = 255;

	displayText.R = 255;
	displayText.G = 255;
	displayText.B = 255;
	displayText.A = 255;

	buttonBg.R = 60;
	buttonBg.G = 63;
	buttonBg.B = 68;
	buttonBg.A = 255;

	buttonText.R = 255;
	buttonText.G = 255;
	buttonText.B = 255;
	buttonText.A = 255;

	operatorBg.R = 255;
	operatorBg.G = 149;
	operatorBg.B = 0;
	operatorBg.A = 255;

	operatorText.R = 255;
	operatorText.G = 255;
	operatorText.B = 255;
	operatorText.A = 255;

	equalBg.R = 76;
	equalBg.G = 175;
	equalBg.B = 80;
	equalBg.A = 255;

	clearBg.R = 244;
	clearBg.G = 67;
	clearBg.B = 54;
	clearBg.A = 255;
}

int main(int argc, char *argv[]) {
	calcWindow.width = WINDOW_WIDTH;
	calcWindow.height = WINDOW_HEIGHT;
	calcWindow.initialPosition.xmin = 100;
	calcWindow.initialPosition.xmax = 100 + WINDOW_WIDTH;
	calcWindow.initialPosition.ymin = 50;
	calcWindow.initialPosition.ymax = 50 + WINDOW_HEIGHT;
	calcWindow.hasTitleBar = 1;

	createWindow(&calcWindow, "Calculator");
	initColors();
	initState();

	// Create display
	displayWidget = addTextWidget(&calcWindow, displayText, state.display,
				      DISPLAY_MARGIN, DISPLAY_MARGIN,
				      WINDOW_WIDTH - DISPLAY_MARGIN * 2,
				      DISPLAY_HEIGHT, 0, emptyHandler);

	// Draw display background
	addColorFillWidget(&calcWindow, displayBg, DISPLAY_MARGIN,
			   DISPLAY_MARGIN, WINDOW_WIDTH - DISPLAY_MARGIN * 2,
			   DISPLAY_HEIGHT, 0, emptyHandler);

	// Re-add display on top
	displayWidget = addTextWidget(&calcWindow, displayText, state.display,
				      DISPLAY_MARGIN + 5, DISPLAY_MARGIN + 25,
				      WINDOW_WIDTH - DISPLAY_MARGIN * 2 - 10,
				      DISPLAY_HEIGHT - 30, 0, emptyHandler);

	int startX = DISPLAY_MARGIN;
	int currentY = BUTTON_START_Y;

	// Row 1: C, ←, ±, ÷
	addButtonWidget(&calcWindow, buttonText, clearBg, "C", startX,
			currentY, BUTTON_SIZE, BUTTON_SIZE, 0, clearHandler);
	addButtonWidget(&calcWindow, buttonText, buttonBg, "<",
			startX + (BUTTON_SIZE + BUTTON_MARGIN), currentY,
			BUTTON_SIZE, BUTTON_SIZE, 0, backspaceHandler);
	addButtonWidget(&calcWindow, buttonText, buttonBg, "+/-",
			startX + (BUTTON_SIZE + BUTTON_MARGIN) * 2, currentY,
			BUTTON_SIZE, BUTTON_SIZE, 0, negateHandler);
	addButtonWidget(&calcWindow, operatorText, operatorBg, "/",
			startX + (BUTTON_SIZE + BUTTON_MARGIN) * 3, currentY,
			BUTTON_SIZE, BUTTON_SIZE, 0, operatorHandler);

	currentY += BUTTON_SIZE + BUTTON_MARGIN;

	// Row 2: 7, 8, 9, ×
	addButtonWidget(&calcWindow, buttonText, buttonBg, "7", startX,
			currentY, BUTTON_SIZE, BUTTON_SIZE, 0, digitHandler);
	addButtonWidget(&calcWindow, buttonText, buttonBg, "8",
			startX + (BUTTON_SIZE + BUTTON_MARGIN), currentY,
			BUTTON_SIZE, BUTTON_SIZE, 0, digitHandler);
	addButtonWidget(&calcWindow, buttonText, buttonBg, "9",
			startX + (BUTTON_SIZE + BUTTON_MARGIN) * 2, currentY,
			BUTTON_SIZE, BUTTON_SIZE, 0, digitHandler);
	addButtonWidget(&calcWindow, operatorText, operatorBg, "*",
			startX + (BUTTON_SIZE + BUTTON_MARGIN) * 3, currentY,
			BUTTON_SIZE, BUTTON_SIZE, 0, operatorHandler);

	currentY += BUTTON_SIZE + BUTTON_MARGIN;

	// Row 3: 4, 5, 6, -
	addButtonWidget(&calcWindow, buttonText, buttonBg, "4", startX,
			currentY, BUTTON_SIZE, BUTTON_SIZE, 0, digitHandler);
	addButtonWidget(&calcWindow, buttonText, buttonBg, "5",
			startX + (BUTTON_SIZE + BUTTON_MARGIN), currentY,
			BUTTON_SIZE, BUTTON_SIZE, 0, digitHandler);
	addButtonWidget(&calcWindow, buttonText, buttonBg, "6",
			startX + (BUTTON_SIZE + BUTTON_MARGIN) * 2, currentY,
			BUTTON_SIZE, BUTTON_SIZE, 0, digitHandler);
	addButtonWidget(&calcWindow, operatorText, operatorBg, "-",
			startX + (BUTTON_SIZE + BUTTON_MARGIN) * 3, currentY,
			BUTTON_SIZE, BUTTON_SIZE, 0, operatorHandler);

	currentY += BUTTON_SIZE + BUTTON_MARGIN;

	// Row 4: 1, 2, 3, +
	addButtonWidget(&calcWindow, buttonText, buttonBg, "1", startX,
			currentY, BUTTON_SIZE, BUTTON_SIZE, 0, digitHandler);
	addButtonWidget(&calcWindow, buttonText, buttonBg, "2",
			startX + (BUTTON_SIZE + BUTTON_MARGIN), currentY,
			BUTTON_SIZE, BUTTON_SIZE, 0, digitHandler);
	addButtonWidget(&calcWindow, buttonText, buttonBg, "3",
			startX + (BUTTON_SIZE + BUTTON_MARGIN) * 2, currentY,
			BUTTON_SIZE, BUTTON_SIZE, 0, digitHandler);
	addButtonWidget(&calcWindow, operatorText, operatorBg, "+",
			startX + (BUTTON_SIZE + BUTTON_MARGIN) * 3, currentY,
			BUTTON_SIZE, BUTTON_SIZE, 0, operatorHandler);

	currentY += BUTTON_SIZE + BUTTON_MARGIN;

	// Row 5: 0 (double width), ., =
	addButtonWidget(&calcWindow, buttonText, buttonBg, "0", startX,
			currentY, BUTTON_SIZE * 2 + BUTTON_MARGIN, BUTTON_SIZE,
			0, digitHandler);
	addButtonWidget(&calcWindow, buttonText, buttonBg, ".",
			startX + (BUTTON_SIZE + BUTTON_MARGIN) * 2, currentY,
			BUTTON_SIZE, BUTTON_SIZE, 0, decimalHandler);
	addButtonWidget(&calcWindow, operatorText, equalBg, "=",
			startX + (BUTTON_SIZE + BUTTON_MARGIN) * 3, currentY,
			BUTTON_SIZE, BUTTON_SIZE, 0, equalHandler);

	calcWindow.needsRepaint = 1;

	while (1) {
		updateWindow(&calcWindow);
		GUI_updateScreen();
	}

	return 0;
}