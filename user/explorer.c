#include "fcntl.h"
#include "fs.h"
#include "gui.h"
#include "memlayout.h"
#include "msg.h"
#include "stat.h"
#include "types.h"
#include "user.h"
#include "user_gui.h"
#include "user_handler.h"
#include "user_window.h"

window desktop;
char current_path[MAX_LONG_STRLEN];
int current_path_widget;
int toolbar_bg_widget;
int input_widget = -1;
int rename_widget = -1;
int create_btn_widget = -1;
int rename_btn_widget = -1;
int selected_widget = -1;
char selected_name[MAX_SHORT_STRLEN];
int is_selected_dir = 0;
int selected_x = 0;
int selected_y = 0;
int selected_w = 0;
int selected_h = 0;

struct RGBA textColor;
struct RGBA dirColor;
struct RGBA bgColor;
struct RGBA buttonColor;
struct RGBA whiteColor;
struct RGBA selectedColor;

int toolbarHeight = 95;
int itemStartY = 100;

char *GUI_programs[] = {"shell", "editor", "explorer", "demo"};

void gui_ls(char *path);
void refreshView();
void clearInputWidgets();
void clearRenameWidgets();
void clearSelection();

// Get file extension
char *getFileExtension(char *filename) {
	static char buf[DIRSIZ + 1];
	char *p;
	for (p = filename + strlen(filename); p >= filename && *p != '.'; p--)
		;
	p++;
	if (strlen(p) >= DIRSIZ)
		return p;
	memmove(buf, p, strlen(p));
	memset(buf + strlen(p), '\0', 1);
	return buf;
}

// Check if file is openable
int isOpenable(char *filename) {
	char *allowed[] = {"shell", "editor", "explorer", "demo"};
	for (int i = 0; i < 4; i++) {
		if (strcmp(filename, allowed[i]) == 0)
			return 1;
	}
	return 0;
}

// Check if file should be shown in explorer
int shouldShowFile(char *filename) {
	// Show text files
	if (strcmp(getFileExtension(filename), "txt") == 0)
		return 1;
	
	// Show allowed programs
	return isOpenable(filename);
}

// Check if file/folder is protected (system files)
int isProtected(char *name) {
	// Remove [D] prefix if exists
	char *actualName = name;
	if (name[0] == '[' && name[1] == 'D' && name[2] == ']' && name[3] == ' ') {
		actualName = name + 4;
	}
	
	// List of protected files/folders
	char *protected[] = {"editor", "explorer", "shell", "demo", ".", "..", 
			     "kernel", "initcode", "init", "cat", "echo", 
			     "forktest", "grep", "kill", "ln", "ls", "mkdir",
			     "rm", "sh", "stressfs", "usertests", "wc", "zombie"};
	
	for (int i = 0; i < 23; i++) {
		if (strcmp(actualName, protected[i]) == 0) {
			return 1;
		}
	}
	return 0;
}

// Get parent path
char *getparentpath(char *path) {
	static char buf[MAX_LONG_STRLEN];
	char *p;
	for (p = path + strlen(path); p >= path && *p != '/'; p--)
		;
	memmove(buf, path, p - path);
	buf[p - path] = '\0';
	if (strlen(buf) == 0)
		strcpy(buf, "/");
	return buf;
}

// Format filename
char *fmtname(char *path) {
	static char buf[DIRSIZ + 1];
	char *p;
	for (p = path + strlen(path); p >= path && *p != '/'; p--)
		;
	p++;
	if (strlen(p) >= DIRSIZ)
		return p;
	memmove(buf, p, strlen(p));
	memset(buf + strlen(p), '\0', 1);
	return buf;
}

// Clear selection
void clearSelection() {
	selected_widget = -1;
	is_selected_dir = 0;
	selected_x = 0;
	selected_y = 0;
	selected_w = 0;
	selected_h = 0;
	memset(selected_name, 0, MAX_SHORT_STRLEN);
	desktop.needsRepaint = 1;
}

// Clear input widgets
void clearInputWidgets() {
	if (input_widget != -1) {
		removeWidget(&desktop, input_widget);
		input_widget = -1;
	}
	if (create_btn_widget != -1) {
		removeWidget(&desktop, create_btn_widget);
		create_btn_widget = -1;
	}
}

// Clear rename widgets
void clearRenameWidgets() {
	if (rename_widget != -1) {
		removeWidget(&desktop, rename_widget);
		rename_widget = -1;
	}
	if (rename_btn_widget != -1) {
		removeWidget(&desktop, rename_btn_widget);
		rename_btn_widget = -1;
	}
}

// Refresh view
void refreshView() {
	clearInputWidgets();
	clearRenameWidgets();
	clearSelection();
	gui_ls(current_path);
	desktop.needsRepaint = 1;
}

// Create folder confirm handler
void createFolderConfirm(Widget *widget, message *msg) {
	if ((msg->msg_type == M_MOUSE_DBCLICK || msg->msg_type == M_MOUSE_LEFT_CLICK) && input_widget != -1) {
		char newDir[MAX_LONG_STRLEN];
		
		// Get updated text from widget
		strcpy(newDir, desktop.widgets[input_widget].context.inputfield->text);
		
		// Build full path
		char fullPath[MAX_LONG_STRLEN];
		if (strlen(current_path) > 0) {
			strcpy(fullPath, current_path);
			strcat(fullPath, "/");
			strcat(fullPath, newDir);
		} else {
			strcpy(fullPath, newDir);
		}
		
		// Create directory
		mkdir(fullPath);
		
		// Refresh view properly
		refreshView();
	}
}

// Create new folder handler
void newFolderHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_DBCLICK || msg->msg_type == M_MOUSE_LEFT_CLICK) {
		clearInputWidgets();
		clearRenameWidgets();
		
		// Add input field
		input_widget = addInputFieldWidget(&desktop, textColor, "newfolder",
						   10, 55, 200, 18, 0, inputFieldKeyHandler);
		
		// Add button
		create_btn_widget = addButtonWidget(&desktop, textColor, buttonColor, "Create", 
						    220, 53, 60, 20, 0, createFolderConfirm);
		
		desktop.needsRepaint = 1;
	}
}

// Create file confirm handler  
void createFileConfirm(Widget *widget, message *msg) {
	if ((msg->msg_type == M_MOUSE_DBCLICK || msg->msg_type == M_MOUSE_LEFT_CLICK) && input_widget != -1) {
		char newFile[MAX_LONG_STRLEN];
		
		// Get updated text from widget
		strcpy(newFile, desktop.widgets[input_widget].context.inputfield->text);
		
		// Build full path
		char fullPath[MAX_LONG_STRLEN];
		if (strlen(current_path) > 0) {
			strcpy(fullPath, current_path);
			strcat(fullPath, "/");
			strcat(fullPath, newFile);
		} else {
			strcpy(fullPath, newFile);
		}
		
		// Create file
		int fd = open(fullPath, O_CREATE | O_WRONLY);
		if (fd >= 0) {
			close(fd);
		}
		
		// Refresh view properly
		refreshView();
	}
}

// Create new file handler
void newFileHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_DBCLICK || msg->msg_type == M_MOUSE_LEFT_CLICK) {
		clearInputWidgets();
		clearRenameWidgets();
		
		// Add input field
		input_widget = addInputFieldWidget(&desktop, textColor, "newfile.txt",
						   10, 55, 200, 18, 0, inputFieldKeyHandler);
		
		// Add button
		create_btn_widget = addButtonWidget(&desktop, textColor, buttonColor, "Create", 
						    220, 53, 60, 20, 0, createFileConfirm);
		
		desktop.needsRepaint = 1;
	}
}

// Back button handler
void backHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_DBCLICK || msg->msg_type == M_MOUSE_LEFT_CLICK) {
		strcpy(current_path, getparentpath(current_path));
		if (strcmp(current_path, "/") == 0)
			strcpy(current_path, "");
		refreshView();
	}
}

// Directory click handler
void cdHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_DBCLICK) {
		// Double click = masuk folder
		char *folderName = widget->context.text->text + 4;
		
		int current_path_length = strlen(current_path);
		if (current_path_length > 0) {
			current_path[current_path_length] = '/';
			strcpy(current_path + current_path_length + 1, folderName);
		} else {
			strcpy(current_path, folderName);
		}
		
		refreshView();
	} else if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		// Single click = select untuk rename
		if (isProtected(widget->context.text->text)) {
			return;
		}
		
		clearSelection();
		clearRenameWidgets();
		
		selected_widget = findWidgetId(&desktop, widget);
		strcpy(selected_name, widget->context.text->text + 4);
		is_selected_dir = 1;
		
		selected_x = widget->position.xmin - 2;
		selected_y = widget->position.ymin - 2;
		selected_w = widget->position.xmax - widget->position.xmin + 4;
		selected_h = widget->position.ymax - widget->position.ymin + 4;
		
		desktop.needsRepaint = 1;
	}
}

// File click handler
void fileHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_DBCLICK) {
		// Double click = buka file
		char *fileName = widget->context.text->text;
		
		char fullPath[MAX_LONG_STRLEN];
		if (strlen(current_path) > 0) {
			strcpy(fullPath, current_path);
			strcat(fullPath, "/");
			strcat(fullPath, fileName);
		} else {
			strcpy(fullPath, fileName);
		}
		
		if (fork() == 0) {
			if (strcmp(getFileExtension(fileName), "txt") == 0) {
				char *argv2[] = {"editor", fullPath, 0};
				exec(argv2[0], argv2);
			} else {
				char *argv2[] = {fileName, 0};
				exec(argv2[0], argv2);
			}
			exit();
		}
	} else if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		// Single click = select untuk rename
		if (isProtected(widget->context.text->text)) {
			return;
		}
		
		clearSelection();
		clearRenameWidgets();
		
		selected_widget = findWidgetId(&desktop, widget);
		strcpy(selected_name, widget->context.text->text);
		is_selected_dir = 0;
		
		selected_x = widget->position.xmin - 2;
		selected_y = widget->position.ymin - 2;
		selected_w = widget->position.xmax - widget->position.xmin + 4;
		selected_h = widget->position.ymax - widget->position.ymin + 4;
		
		desktop.needsRepaint = 1;
	}
}

// Rename confirm handler
void renameConfirm(Widget *widget, message *msg) {
	if ((msg->msg_type == M_MOUSE_DBCLICK || msg->msg_type == M_MOUSE_LEFT_CLICK) && rename_widget != -1 && selected_widget != -1) {
		char oldPath[MAX_LONG_STRLEN];
		char newPath[MAX_LONG_STRLEN];
		
		// Get new name from widget
		char newName[MAX_SHORT_STRLEN];
		strcpy(newName, desktop.widgets[rename_widget].context.inputfield->text);
		
		// Build old path
		if (strlen(current_path) > 0) {
			strcpy(oldPath, current_path);
			strcat(oldPath, "/");
			strcat(oldPath, selected_name);
		} else {
			strcpy(oldPath, selected_name);
		}
		
		// Build new path
		if (strlen(current_path) > 0) {
			strcpy(newPath, current_path);
			strcat(newPath, "/");
			strcat(newPath, newName);
		} else {
			strcpy(newPath, newName);
		}
		
		// Perform rename
		if (link(oldPath, newPath) == 0) {
			unlink(oldPath);
		}
		
		// Refresh view properly
		refreshView();
	}
}

// Rename handler
void renameHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_DBCLICK || msg->msg_type == M_MOUSE_LEFT_CLICK) {
		if (selected_widget == -1) {
			// No file/folder selected
			return;
		}
		
		clearRenameWidgets();
		
		// Show rename input with current name
		rename_widget = addInputFieldWidget(&desktop, textColor, selected_name,
						    250, 8, 180, 18, 0, inputFieldKeyHandler);
		
		rename_btn_widget = addButtonWidget(&desktop, textColor, buttonColor, "OK", 
						    435, 6, 50, 20, 0, renameConfirm);
		desktop.needsRepaint = 1;
	}
}

// List directory contents
void gui_ls(char *path) {
	// Update path display
	strcpy(desktop.widgets[current_path_widget].context.text->text, 
	       strlen(path) > 0 ? path : "/");
	
	// Clear file/folder widgets
	int widgetsToRemove[MAX_WIDGET_SIZE];
	int removeCount = 0;
	
	for (int p = desktop.widgetlisthead; p != -1; p = desktop.widgets[p].next) {
		if ((desktop.widgets[p].type == TEXT || desktop.widgets[p].type == SHAPE) && 
		    p != current_path_widget &&
		    p != input_widget &&
		    p != rename_widget &&
		    desktop.widgets[p].scrollable == 1) {
			widgetsToRemove[removeCount++] = p;
		}
	}
	
	for (int i = 0; i < removeCount; i++) {
		removeWidget(&desktop, widgetsToRemove[i]);
	}
	
	// Use static buffer to prevent stack overflow
	static char pathBuf[MAX_LONG_STRLEN];
	char *p;
	int fd;
	struct dirent de;
	struct stat st;
	int lineCount = 0;
	
	// Open directory
	char openPath[MAX_LONG_STRLEN];
	if (strlen(path) > 0) {
		strcpy(openPath, path);
	} else {
		strcpy(openPath, ".");
	}
	
	if ((fd = open(openPath, 0)) < 0) {
		return;
	}
	
	if (fstat(fd, &st) < 0) {
		close(fd);
		return;
	}
	
	if (st.type == T_DIR) {
		if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(pathBuf)) {
			close(fd);
			return;
		}
		
		strcpy(pathBuf, path);
		p = pathBuf + strlen(pathBuf);
		if (strlen(path) > 0)
			*p++ = '/';
		
		while (read(fd, &de, sizeof(de)) == sizeof(de)) {
			if (de.inum == 0)
				continue;
			
			memmove(p, de.name, DIRSIZ);
			p[DIRSIZ] = 0;
			
			if (stat(pathBuf, &st) < 0)
				continue;
			
			char formatName[MAX_SHORT_STRLEN];
			strcpy(formatName, fmtname(pathBuf));
			
			if (st.type == T_FILE) {
				// Only show allowed files
				if (shouldShowFile(formatName)) {
					addTextWidget(&desktop, textColor, formatName, 10,
						      itemStartY + lineCount * 20, 300, 18, 1,
						      fileHandler);
					lineCount++;
				}
			} else if (st.type == T_DIR && strcmp(formatName, ".") != 0 &&
				   strcmp(formatName, "..") != 0) {
				char folderName[MAX_SHORT_STRLEN + 4];
				strcpy(folderName, "[D] ");
				strcat(folderName, formatName);
				addTextWidget(&desktop, dirColor, folderName, 10,
					      itemStartY + lineCount * 20, 300, 18, 1,
					      cdHandler);
				lineCount++;
			}
		}
	}
	
	close(fd);
}

int main(int argc, char *argv[]) {
	desktop.width = 420;
	desktop.height = 380;
	desktop.hasTitleBar = 1;
	createWindow(&desktop, "File Explorer");
	
	// Colors
	bgColor.R = 245;
	bgColor.G = 245;
	bgColor.B = 245;
	bgColor.A = 255;
	
	textColor.R = 20;
	textColor.G = 20;
	textColor.B = 20;
	textColor.A = 255;
	
	dirColor.R = 41;
	dirColor.G = 128;
	dirColor.B = 185;
	dirColor.A = 255;
	
	buttonColor.R = 52;
	buttonColor.G = 152;
	buttonColor.B = 219;
	buttonColor.A = 255;
	
	whiteColor.R = 255;
	whiteColor.G = 255;
	whiteColor.B = 255;
	whiteColor.A = 255;
	
	selectedColor.R = 255;
	selectedColor.G = 140;
	selectedColor.B = 0;
	selectedColor.A = 255;
	
	// Background
	addColorFillWidget(&desktop, bgColor, 0, 0, desktop.width,
			   desktop.height, 0, emptyHandler);
	
	// Toolbar background
	toolbar_bg_widget = addColorFillWidget(&desktop, whiteColor, 0, 0, 
					       desktop.width, toolbarHeight, 0, emptyHandler);
	
	// Toolbar buttons
	addButtonWidget(&desktop, textColor, buttonColor, "Back", 5, 5, 50, 25, 0,
			backHandler);
	addButtonWidget(&desktop, textColor, buttonColor, "NewDir", 60, 5, 55, 25, 0,
			newFolderHandler);
	addButtonWidget(&desktop, textColor, buttonColor, "NewFile", 120, 5, 60, 25, 0,
			newFileHandler);
	addButtonWidget(&desktop, textColor, buttonColor, "Rename", 185, 5, 60, 25, 0,
			renameHandler);
	
	// Path display
	memset(current_path, 0, MAX_LONG_STRLEN);
	current_path_widget = addTextWidget(&desktop, textColor, "/", 5, 35,
					    desktop.width - 10, 18, 0, emptyHandler);
	
	// Initial directory listing
	gui_ls(current_path);
	
	while (1) {
		updateWindow(&desktop);
		
		// Draw selection border after all widgets (overlay effect)
		if (selected_widget != -1 && selected_w > 0) {
			RGB borderColor;
			borderColor.R = selectedColor.R;
			borderColor.G = selectedColor.G;
			borderColor.B = selectedColor.B;
			
			int draw_y = selected_y - desktop.scrollOffsetY;
			drawRect(&desktop, borderColor, selected_x, draw_y, selected_w, selected_h);
			drawRect(&desktop, borderColor, selected_x + 1, draw_y + 1, selected_w - 2, selected_h - 2);
		}
	}
}