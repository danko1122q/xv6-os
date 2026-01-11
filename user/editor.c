#include "fcntl.h"
#include "gui.h"
#include "memlayout.h"
#include "msg.h"
#include "types.h"
#include "user.h"
#include "user_gui.h"
#include "user_handler.h"
#include "user_window.h"

// Constants
#define MAX_FILENAME_LEN 40
#define DEFAULT_WINDOW_WIDTH 450
#define DEFAULT_WINDOW_HEIGHT 350
#define BUTTON_WIDTH 50
#define BUTTON_HEIGHT 25
#define BUTTON_MARGIN 8
#define BUTTON_SPACING 5
#define EDITOR_PADDING 10
#define STATUS_BAR_HEIGHT 25

// Global state
window programWindow;
char filename[MAX_FILENAME_LEN];
int fileDescriptor = -1;
int lastMaximumOffset = 0;
int isModified = 0;
int lineCount = 1;

// Color definitions
struct RGBA createColor(int r, int g, int b, int a) {
    struct RGBA color;
    color.R = r;
    color.G = g;
    color.B = b;
    color.A = a;
    return color;
}

// Get input field widget
Widget* getInputFieldWidget() {
    int i;
    for (i = programWindow.widgetlisthead; i != -1; 
         i = programWindow.widgets[i].next) {
        if (programWindow.widgets[i].type == INPUTFIELD) {
            return &programWindow.widgets[i];
        }
    }
    return 0;
}

// Count lines in text
int countLines(char *text) {
    int count = 1;
    int i;
    for (i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            count++;
        }
    }
    return count;
}

// Save button handler
void saveButtonHandler(Widget *widget, message *msg) {
    if (msg->msg_type != M_MOUSE_LEFT_CLICK && 
        msg->msg_type != M_MOUSE_DBCLICK) {
        return;
    }

    Widget *inputWidget = getInputFieldWidget();
    if (!inputWidget) {
        return;
    }

    // Determine filename
    char *targetFile = filename[0] != '\0' ? filename : "/new.txt";
    
    // Open file for writing (create if doesn't exist)
    int fd = open(targetFile, O_RDWR | O_CREATE);
    if (fd < 0) {
        printf(1, "Error: Cannot open file %s\n", targetFile);
        return;
    }

    // Write content to file
    char *text = inputWidget->context.inputfield->text;
    int len = strlen(text);
    int written = write(fd, text, len);
    
    close(fd);

    if (written >= 0) {
        isModified = 0;
        printf(1, "File saved: %s (%d bytes)\n", targetFile, written);
    } else {
        printf(1, "Error: Failed to write to file\n");
    }
}

// New file button handler
void newButtonHandler(Widget *widget, message *msg) {
    if (msg->msg_type != M_MOUSE_LEFT_CLICK && 
        msg->msg_type != M_MOUSE_DBCLICK) {
        return;
    }

    Widget *inputWidget = getInputFieldWidget();
    if (!inputWidget) {
        return;
    }

    // Clear the text
    memset(inputWidget->context.inputfield->text, 0, MAX_LONG_STRLEN);
    inputWidget->context.inputfield->current_pos = 0;
    
    // Reset filename
    memset(filename, 0, MAX_FILENAME_LEN);
    
    isModified = 0;
    lineCount = 1;
    printf(1, "New file created\n");
}

// Input field handler with improved scrolling
void inputHandler(Widget *w, message *msg) {
    int width = w->position.xmax - w->position.xmin;
    
    if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
        inputMouseLeftClickHandler(w, msg);
    } 
    else if (msg->msg_type == M_KEY_DOWN) {
        inputFieldKeyHandler(w, msg);
        isModified = 1;
        
        // Update line count
        lineCount = countLines(w->context.inputfield->text);
        
        // Calculate new height based on content
        int widgetHeight = w->position.ymax - w->position.ymin;
        int contentLines = getMouseYFromOffset(
                            w->context.inputfield->text, width,
                            strlen(w->context.inputfield->text)) + 1;
        int newHeight = CHARACTER_HEIGHT * contentLines;
        
        // Expand widget if needed
        if (newHeight > widgetHeight) {
            w->position.ymax = w->position.ymin + newHeight;
        }
        
        // Update scroll offset only if content exceeds window
        int maximumOffset = getScrollableTotalHeight(&programWindow) - 
                            programWindow.height;
        
        // Only scroll if there's actually content that needs scrolling
        if (maximumOffset > 0) {
            // Calculate cursor position on screen
            int cursorLine = getMouseYFromOffset(
                w->context.inputfield->text, width,
                w->context.inputfield->current_pos);
            int cursorY = cursorLine * CHARACTER_HEIGHT + 
                w->position.ymin - programWindow.scrollOffsetY;
            
            // Auto-scroll to keep cursor visible
            // Scroll up if cursor is above visible area
            if (cursorY < w->position.ymin && programWindow.scrollOffsetY > 0) {
                programWindow.scrollOffsetY -= CHARACTER_HEIGHT;
            }
            // Scroll down if cursor is below visible area
            else if (cursorY >= programWindow.height - STATUS_BAR_HEIGHT - CHARACTER_HEIGHT) {
                if (programWindow.scrollOffsetY < maximumOffset) {
                    programWindow.scrollOffsetY += CHARACTER_HEIGHT;
                }
            }
            
            lastMaximumOffset = maximumOffset;
        } else {
            // Reset scroll if content fits in window
            programWindow.scrollOffsetY = 0;
            lastMaximumOffset = 0;
        }
    }
}

// Load file content
int loadFile(char *filepath, char *buffer, int maxLen) {
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    
    int bytesRead = read(fd, buffer, maxLen - 1);
    close(fd);
    
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        return bytesRead;
    }
    
    return -1;
}

// Initialize window and widgets
void initEditor(char *initialFile) {
    // Set window properties - SMALLER SIZE
    programWindow.width = DEFAULT_WINDOW_WIDTH;
    programWindow.height = DEFAULT_WINDOW_HEIGHT;
    programWindow.hasTitleBar = 1;
    
    char windowTitle[60];
    if (initialFile && initialFile[0] != '\0') {
        // Manually build title string
        int i = 0;
        char *prefix = "Editor - ";
        while (*prefix) {
            windowTitle[i++] = *prefix++;
        }
        char *fname = initialFile;
        while (*fname && i < 59) {
            windowTitle[i++] = *fname++;
        }
        windowTitle[i] = '\0';
    } else {
        strcpy(windowTitle, "Text Editor");
    }
    
    createWindow(&programWindow, windowTitle);
    
    // Background
    struct RGBA bgColor = createColor(250, 250, 250, 255);
    addColorFillWidget(&programWindow, bgColor, 0, 0, 
                       programWindow.width, programWindow.height, 
                       0, 0);
    
    // Button colors
    struct RGBA saveButtonColor = createColor(76, 175, 80, 255);
    struct RGBA newButtonColor = createColor(33, 150, 243, 255);
    struct RGBA textColor = createColor(255, 255, 255, 255);
    struct RGBA inputTextColor = createColor(33, 33, 33, 255);
    
    // Buttons - smaller and more compact
    int buttonY = BUTTON_MARGIN;
    int saveButtonX = BUTTON_MARGIN;
    int newButtonX = saveButtonX + BUTTON_WIDTH + BUTTON_SPACING;
    
    addButtonWidget(&programWindow, textColor, saveButtonColor, "Save", 
                    saveButtonX, buttonY, 
                    BUTTON_WIDTH, BUTTON_HEIGHT, 
                    0, saveButtonHandler);
    
    addButtonWidget(&programWindow, textColor, newButtonColor, "New", 
                    newButtonX, buttonY, 
                    BUTTON_WIDTH, BUTTON_HEIGHT, 
                    0, newButtonHandler);
    
    // Load file or use placeholder
    char initialText[MAX_LONG_STRLEN];
    memset(initialText, 0, MAX_LONG_STRLEN);
    
    if (initialFile && initialFile[0] != '\0') {
        strcpy(filename, initialFile);
        if (loadFile(filename, initialText, MAX_LONG_STRLEN) < 0) {
            strcpy(initialText, "");
        }
    } else {
        strcpy(initialText, "");
    }
    
    // Text input field
    int editorY = buttonY + BUTTON_HEIGHT + BUTTON_MARGIN;
    addInputFieldWidget(&programWindow, inputTextColor, initialText, 
                        EDITOR_PADDING, editorY,
                        programWindow.width - EDITOR_PADDING, 
                        programWindow.height - EDITOR_PADDING,
                        1, inputHandler);
    
    lineCount = countLines(initialText);
}

int main(int argc, char *argv[]) {
    char *initialFile = 0;
    
    // Parse command line arguments
    if (argc > 1) {
        initialFile = argv[1];
    }
    
    // Initialize editor
    initEditor(initialFile);
    
    printf(1, "Text Editor started\n");
    if (initialFile) {
        printf(1, "Loaded: %s\n", initialFile);
    }
    
    // Main event loop
    while (1) {
        updateWindow(&programWindow);
    }
    
    return 0;
}