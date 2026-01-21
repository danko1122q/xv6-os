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
int sidebar_bg_widget;
int content_bg_widget;
int input_widget = -1;
int rename_widget = -1;
int create_btn_widget = -1;
int rename_btn_widget = -1;
int cancel_btn_widget = -1;
int selected_widget = -1;
char selected_name[MAX_SHORT_STRLEN];
int is_selected_dir = 0;
int selected_x = 0;
int selected_y = 0;
int selected_w = 0;
int selected_h = 0;
int selected_index = -1;
int total_items = 0;
int input_mode = 0; // 0=none, 1=newfolder, 2=newfile, 3=rename

struct RGBA textColor;
struct RGBA dirColor;
struct RGBA bgColor;
struct RGBA buttonColor;
struct RGBA whiteColor;
struct RGBA selectedColor;
struct RGBA sidebarColor;
struct RGBA toolbarColor;
struct RGBA contentBgColor;
struct RGBA borderColor;
struct RGBA inputBgColor;

int toolbarHeight = 45;
int sidebarWidth = 140;
int itemStartY = 50;
int itemStartX = 150;
int inputPanelY = 350; // Area untuk input di bawah

void gui_ls(char *path);
void refreshView();
void clearInputWidgets();
void clearRenameWidgets();
void clearSelection();
int canOpenWithEditor(char *filename);
void selectItemByIndex(int index);
void explorerKeyHandler(Widget *widget, message *msg);

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

int isOpenable(char *filename) {
	char *allowed[] = {"terminal", "editor", "explorer", "floppybird"};
	for (int i = 0; i < 4; i++) {
		if (strcmp(filename, allowed[i]) == 0)
			return 1;
	}
	return 0;
}

int shouldShowFile(char *filename) {
	char *guiApps[] = {"terminal", "editor", "explorer", "floppybird"};
	for (int i = 0; i < 4; i++) {
		if (strcmp(filename, guiApps[i]) == 0)
			return 1;
	}

	if (strcmp(getFileExtension(filename), "txt") == 0)
		return 1;

	char *ext = getFileExtension(filename);
	if (strcmp(ext, "md") == 0)
		return 1;

	int hasUpperCase = 0;
	for (int i = 0; filename[i] != '\0'; i++) {
		if (filename[i] >= 'A' && filename[i] <= 'Z') {
			hasUpperCase = 1;
			break;
		}
	}
	if (hasUpperCase)
		return 1;

	return 0;
}

int canOpenWithEditor(char *filename) {
	char *ext = getFileExtension(filename);

	if (strcmp(ext, "txt") == 0)
		return 1;

	if (strcmp(ext, "md") == 0)
		return 1;

	for (int i = 0; filename[i] != '\0'; i++) {
		if (filename[i] >= 'A' && filename[i] <= 'Z') {
			return 1;
		}
	}

	return 0;
}

int isProtected(char *name) {
	char *actualName = name;
	if (name[0] == '[' && name[1] == 'D' && name[2] == ']' &&
	    name[3] == ' ') {
		actualName = name + 4;
	}

	char *protected[] = {"editor",	  "explorer", "terminal", "floppybird",
			     ".",	  "..",	      "kernel",	  "initcode",
			     "init",	  "cat",      "echo",	  "forktest",
			     "grep",	  "kill",     "ln",	  "ls",
			     "mkdir",	  "rm",	      "sh",	  "stressfs",
			     "usertests", "wc",	      "zombie"};

	for (int i = 0; i < 23; i++) {
		if (strcmp(actualName, protected[i]) == 0) {
			return 1;
		}
	}
	return 0;
}

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

void clearSelection() {
	selected_widget = -1;
	is_selected_dir = 0;
	selected_x = 0;
	selected_y = 0;
	selected_w = 0;
	selected_h = 0;
	selected_index = -1;
	memset(selected_name, 0, MAX_SHORT_STRLEN);
	desktop.needsRepaint = 1;
}

void clearInputWidgets() {
	if (input_widget != -1) {
		removeWidget(&desktop, input_widget);
		input_widget = -1;
	}
	if (create_btn_widget != -1) {
		removeWidget(&desktop, create_btn_widget);
		create_btn_widget = -1;
	}
	if (cancel_btn_widget != -1) {
		removeWidget(&desktop, cancel_btn_widget);
		cancel_btn_widget = -1;
	}
	input_mode = 0;
	desktop.needsRepaint = 1;
}

void clearRenameWidgets() {
	if (rename_widget != -1) {
		removeWidget(&desktop, rename_widget);
		rename_widget = -1;
	}
	if (rename_btn_widget != -1) {
		removeWidget(&desktop, rename_btn_widget);
		rename_btn_widget = -1;
	}
	if (cancel_btn_widget != -1) {
		removeWidget(&desktop, cancel_btn_widget);
		cancel_btn_widget = -1;
	}
	input_mode = 0;
	desktop.needsRepaint = 1;
}

void refreshView() {
	clearInputWidgets();
	clearRenameWidgets();
	clearSelection();
	gui_ls(current_path);
	desktop.needsRepaint = 1;
}

void selectItemByIndex(int index) {
	if (index < 0 || index >= total_items)
		return;

	clearRenameWidgets();
	clearInputWidgets();

	int currentIndex = 0;
	for (int p = desktop.widgetlisthead; p != -1;
	     p = desktop.widgets[p].next) {
		if (desktop.widgets[p].scrollable == 1 &&
		    desktop.widgets[p].type == TEXT &&
		    p != current_path_widget) {

			if (currentIndex == index) {
				selected_widget = p;
				selected_index = index;

				if (desktop.widgets[p].context.text->text[0] ==
					    '[' &&
				    desktop.widgets[p].context.text->text[1] ==
					    'D') {
					is_selected_dir = 1;
					strcpy(selected_name,
					       desktop.widgets[p]
							       .context.text
							       ->text +
						       4);
				} else {
					is_selected_dir = 0;
					strcpy(selected_name,
					       desktop.widgets[p]
							       .context.text
							       ->text +
						       2);
				}

				selected_x =
					desktop.widgets[p].position.xmin - 5;
				selected_y =
					desktop.widgets[p].position.ymin - 3;
				selected_w = desktop.widgets[p].position.xmax -
					     desktop.widgets[p].position.xmin +
					     10;
				selected_h = desktop.widgets[p].position.ymax -
					     desktop.widgets[p].position.ymin +
					     6;

				int item_top = desktop.widgets[p].position.ymin;
				int item_bottom =
					desktop.widgets[p].position.ymax;
				int visible_top = itemStartY;
				int visible_bottom = inputPanelY - 10;

				if (item_bottom - desktop.scrollOffsetY >
				    visible_bottom) {
					desktop.scrollOffsetY = item_bottom -
								visible_bottom +
								20;
				}
				if (item_top - desktop.scrollOffsetY <
				    visible_top) {
					desktop.scrollOffsetY =
						item_top - visible_top - 20;
					if (desktop.scrollOffsetY < 0)
						desktop.scrollOffsetY = 0;
				}

				desktop.needsRepaint = 1;
				break;
			}
			currentIndex++;
		}
	}
}

void explorerKeyHandler(Widget *widget, message *msg) {
	if (msg->msg_type != M_KEY_DOWN)
		return;

	int key = msg->params[0];

	if (key == KEY_DN) {
		if (selected_index < total_items - 1) {
			selectItemByIndex(selected_index + 1);
		} else if (selected_index == -1 && total_items > 0) {
			selectItemByIndex(0);
		}
	} else if (key == KEY_UP) {
		if (selected_index > 0) {
			selectItemByIndex(selected_index - 1);
		} else if (selected_index == -1 && total_items > 0) {
			selectItemByIndex(0);
		}
	} else if (key == '\n') {
		if (selected_widget != -1) {
			if (is_selected_dir) {
				int current_path_length = strlen(current_path);
				if (current_path_length > 0) {
					current_path[current_path_length] = '/';
					strcpy(current_path +
						       current_path_length + 1,
					       selected_name);
				} else {
					strcpy(current_path, selected_name);
				}
				refreshView();
			} else {
				char fullPath[MAX_LONG_STRLEN];
				if (strlen(current_path) > 0) {
					strcpy(fullPath, current_path);
					strcat(fullPath, "/");
					strcat(fullPath, selected_name);
				} else {
					strcpy(fullPath, selected_name);
				}

				if (fork() == 0) {
					if (canOpenWithEditor(selected_name)) {
						char *argv2[] = {"editor",
								 fullPath, 0};
						exec(argv2[0], argv2);
					} else {
						char *argv2[] = {selected_name,
								 0};
						exec(argv2[0], argv2);
					}
					exit();
				}
			}
		}
	}
}

void cancelInput(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		clearInputWidgets();
		clearRenameWidgets();
		desktop.needsRepaint = 1;
	}
}

void createFolderConfirm(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK && input_widget != -1) {
		char newDir[MAX_LONG_STRLEN];
		strcpy(newDir,
		       desktop.widgets[input_widget].context.inputfield->text);

		if (strlen(newDir) == 0) {
			clearInputWidgets();
			return;
		}

		char fullPath[MAX_LONG_STRLEN];
		if (strlen(current_path) > 0) {
			strcpy(fullPath, current_path);
			strcat(fullPath, "/");
			strcat(fullPath, newDir);
		} else {
			strcpy(fullPath, newDir);
		}

		mkdir(fullPath);
		clearInputWidgets();
		refreshView();
	}
}

void newFolderHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		clearInputWidgets();
		clearRenameWidgets();

		input_mode = 1;

		// Background panel untuk input
		addColorFillWidget(&desktop, inputBgColor, sidebarWidth,
				   inputPanelY, desktop.width - sidebarWidth,
				   70, 0, emptyHandler);

		addTextWidget(&desktop, textColor,
			      "New Folder:", sidebarWidth + 10,
			      inputPanelY + 10, 120, 20, 0, emptyHandler);

		input_widget = addInputFieldWidget(
			&desktop, textColor, "newfolder", sidebarWidth + 10,
			inputPanelY + 35, 300, 25, 0, inputFieldKeyHandler);

		create_btn_widget = addButtonWidget(
			&desktop, whiteColor, buttonColor, "Create",
			sidebarWidth + 320, inputPanelY + 35, 70, 25, 0,
			createFolderConfirm);

		cancel_btn_widget =
			addButtonWidget(&desktop, textColor, bgColor, "Cancel",
					sidebarWidth + 395, inputPanelY + 35,
					70, 25, 0, cancelInput);

		desktop.keyfocus = input_widget;
		desktop.needsRepaint = 1;
	}
}

void createFileConfirm(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK && input_widget != -1) {
		char newFile[MAX_LONG_STRLEN];
		strcpy(newFile,
		       desktop.widgets[input_widget].context.inputfield->text);

		if (strlen(newFile) == 0) {
			clearInputWidgets();
			return;
		}

		char fullPath[MAX_LONG_STRLEN];
		if (strlen(current_path) > 0) {
			strcpy(fullPath, current_path);
			strcat(fullPath, "/");
			strcat(fullPath, newFile);
		} else {
			strcpy(fullPath, newFile);
		}

		int fd = open(fullPath, O_CREATE | O_WRONLY);
		if (fd >= 0) {
			close(fd);
		}

		clearInputWidgets();
		refreshView();
	}
}

void newFileHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		clearInputWidgets();
		clearRenameWidgets();

		input_mode = 2;

		// Background panel untuk input
		addColorFillWidget(&desktop, inputBgColor, sidebarWidth,
				   inputPanelY, desktop.width - sidebarWidth,
				   70, 0, emptyHandler);

		addTextWidget(&desktop, textColor,
			      "New File:", sidebarWidth + 10, inputPanelY + 10,
			      120, 20, 0, emptyHandler);

		input_widget = addInputFieldWidget(
			&desktop, textColor, "newfile.txt", sidebarWidth + 10,
			inputPanelY + 35, 300, 25, 0, inputFieldKeyHandler);

		create_btn_widget = addButtonWidget(
			&desktop, whiteColor, buttonColor, "Create",
			sidebarWidth + 320, inputPanelY + 35, 70, 25, 0,
			createFileConfirm);

		cancel_btn_widget =
			addButtonWidget(&desktop, textColor, bgColor, "Cancel",
					sidebarWidth + 395, inputPanelY + 35,
					70, 25, 0, cancelInput);

		desktop.keyfocus = input_widget;
		desktop.needsRepaint = 1;
	}
}

void backHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		strcpy(current_path, getparentpath(current_path));
		if (strcmp(current_path, "/") == 0)
			strcpy(current_path, "");
		refreshView();
	}
}

void homeHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		strcpy(current_path, "");
		refreshView();
	}
}

void cdHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_DBCLICK) {
		char *folderName = widget->context.text->text + 4;

		int current_path_length = strlen(current_path);
		if (current_path_length > 0) {
			current_path[current_path_length] = '/';
			strcpy(current_path + current_path_length + 1,
			       folderName);
		} else {
			strcpy(current_path, folderName);
		}

		refreshView();
	} else if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		clearSelection();
		clearRenameWidgets();
		clearInputWidgets();

		selected_widget = findWidgetId(&desktop, widget);
		strcpy(selected_name, widget->context.text->text + 4);
		is_selected_dir = 1;

		int currentIndex = 0;
		for (int p = desktop.widgetlisthead; p != -1;
		     p = desktop.widgets[p].next) {
			if (desktop.widgets[p].scrollable == 1 &&
			    desktop.widgets[p].type == TEXT &&
			    p != current_path_widget) {
				if (p == selected_widget) {
					selected_index = currentIndex;
					break;
				}
				currentIndex++;
			}
		}

		selected_x = widget->position.xmin - 5;
		selected_y = widget->position.ymin - 3;
		selected_w = widget->position.xmax - widget->position.xmin + 10;
		selected_h = widget->position.ymax - widget->position.ymin + 6;

		desktop.needsRepaint = 1;
	}
}

void fileHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_DBCLICK) {
		char *fileName = widget->context.text->text + 2;

		char fullPath[MAX_LONG_STRLEN];
		if (strlen(current_path) > 0) {
			strcpy(fullPath, current_path);
			strcat(fullPath, "/");
			strcat(fullPath, fileName);
		} else {
			strcpy(fullPath, fileName);
		}

		if (fork() == 0) {
			if (canOpenWithEditor(fileName)) {
				char *argv2[] = {"editor", fullPath, 0};
				exec(argv2[0], argv2);
			} else {
				char *argv2[] = {fileName, 0};
				exec(argv2[0], argv2);
			}
			exit();
		}
	} else if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		clearSelection();
		clearRenameWidgets();
		clearInputWidgets();

		selected_widget = findWidgetId(&desktop, widget);
		strcpy(selected_name, widget->context.text->text + 2);
		is_selected_dir = 0;

		int currentIndex = 0;
		for (int p = desktop.widgetlisthead; p != -1;
		     p = desktop.widgets[p].next) {
			if (desktop.widgets[p].scrollable == 1 &&
			    desktop.widgets[p].type == TEXT &&
			    p != current_path_widget) {
				if (p == selected_widget) {
					selected_index = currentIndex;
					break;
				}
				currentIndex++;
			}
		}

		selected_x = widget->position.xmin - 5;
		selected_y = widget->position.ymin - 3;
		selected_w = widget->position.xmax - widget->position.xmin + 10;
		selected_h = widget->position.ymax - widget->position.ymin + 6;

		desktop.needsRepaint = 1;
	}
}

void renameConfirm(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK && rename_widget != -1 &&
	    selected_widget != -1) {
		char oldPath[MAX_LONG_STRLEN];
		char newPath[MAX_LONG_STRLEN];

		char newName[MAX_SHORT_STRLEN];
		strcpy(newName,
		       desktop.widgets[rename_widget].context.inputfield->text);

		if (strlen(newName) == 0 ||
		    strcmp(newName, selected_name) == 0) {
			clearRenameWidgets();
			return;
		}

		if (strlen(current_path) > 0) {
			strcpy(oldPath, current_path);
			strcat(oldPath, "/");
			strcat(oldPath, selected_name);
		} else {
			strcpy(oldPath, selected_name);
		}

		if (strlen(current_path) > 0) {
			strcpy(newPath, current_path);
			strcat(newPath, "/");
			strcat(newPath, newName);
		} else {
			strcpy(newPath, newName);
		}

		if (link(oldPath, newPath) == 0) {
			unlink(oldPath);
		}

		clearRenameWidgets();
		refreshView();
	}
}

void renameHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		if (selected_widget == -1) {
			return;
		}

		char checkName[MAX_SHORT_STRLEN];
		if (is_selected_dir) {
			strcpy(checkName, "[D] ");
			strcat(checkName, selected_name);
		} else {
			strcpy(checkName, selected_name);
		}

		if (isProtected(checkName)) {
			return;
		}

		clearRenameWidgets();
		clearInputWidgets();

		input_mode = 3;

		// Background panel untuk rename
		addColorFillWidget(&desktop, inputBgColor, sidebarWidth,
				   inputPanelY, desktop.width - sidebarWidth,
				   70, 0, emptyHandler);

		addTextWidget(&desktop, textColor,
			      "Rename to:", sidebarWidth + 10, inputPanelY + 10,
			      120, 20, 0, emptyHandler);

		rename_widget = addInputFieldWidget(
			&desktop, textColor, selected_name, sidebarWidth + 10,
			inputPanelY + 35, 300, 25, 0, inputFieldKeyHandler);

		rename_btn_widget = addButtonWidget(
			&desktop, whiteColor, buttonColor, "Rename",
			sidebarWidth + 320, inputPanelY + 35, 70, 25, 0,
			renameConfirm);

		cancel_btn_widget =
			addButtonWidget(&desktop, textColor, bgColor, "Cancel",
					sidebarWidth + 395, inputPanelY + 35,
					70, 25, 0, cancelInput);

		desktop.keyfocus = rename_widget;
		desktop.needsRepaint = 1;
	}
}

void gui_ls(char *path) {
	strcpy(desktop.widgets[current_path_widget].context.text->text,
	       strlen(path) > 0 ? path : "/");

	int widgetsToRemove[MAX_WIDGET_SIZE];
	int removeCount = 0;

	// Collect widgets to remove
	for (int p = desktop.widgetlisthead; p != -1;
	     p = desktop.widgets[p].next) {
		if (desktop.widgets[p].scrollable == 1 &&
		    (desktop.widgets[p].type == TEXT ||
		     desktop.widgets[p].type == SHAPE) &&
		    p != current_path_widget) {
			widgetsToRemove[removeCount++] = p;
		}
	}

	// Remove old items
	for (int i = 0; i < removeCount; i++) {
		removeWidget(&desktop, widgetsToRemove[i]);
	}

	static char pathBuf[MAX_LONG_STRLEN];
	char *p;
	int fd;
	struct dirent de;
	struct stat st;
	int lineCount = 0;

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
				if (shouldShowFile(formatName)) {
					char displayName[MAX_SHORT_STRLEN + 5];
					strcpy(displayName, "F ");
					strcat(displayName, formatName);
					addTextWidget(&desktop, textColor,
						      displayName, itemStartX,
						      itemStartY +
							      lineCount * 26,
						      340, 22, 1, fileHandler);
					lineCount++;
				}
			} else if (st.type == T_DIR &&
				   strcmp(formatName, ".") != 0 &&
				   strcmp(formatName, "..") != 0) {
				char displayName[MAX_SHORT_STRLEN + 5];
				strcpy(displayName, "[D] ");
				strcat(displayName, formatName);
				addTextWidget(&desktop, dirColor, displayName,
					      itemStartX,
					      itemStartY + lineCount * 26, 340,
					      22, 1, cdHandler);
				lineCount++;
			}
		}
	}

	close(fd);
	total_items = lineCount;
}

int main(int argc, char *argv[]) {
	desktop.width = 620;
	desktop.height = 450;
	desktop.hasTitleBar = 1;
	createWindow(&desktop, "Files");

	// Modern color scheme
	bgColor.R = 250;
	bgColor.G = 250;
	bgColor.B = 250;
	bgColor.A = 255;

	textColor.R = 30;
	textColor.G = 30;
	textColor.B = 30;
	textColor.A = 255;

	dirColor.R = 52;
	dirColor.G = 101;
	dirColor.B = 164;
	dirColor.A = 255;

	buttonColor.R = 53;
	buttonColor.G = 132;
	buttonColor.B = 228;
	buttonColor.A = 255;

	whiteColor.R = 255;
	whiteColor.G = 255;
	whiteColor.B = 255;
	whiteColor.A = 255;

	selectedColor.R = 66;
	selectedColor.G = 135;
	selectedColor.B = 245;
	selectedColor.A = 80;

	sidebarColor.R = 242;
	sidebarColor.G = 242;
	sidebarColor.B = 242;
	sidebarColor.A = 255;

	toolbarColor.R = 252;
	toolbarColor.G = 252;
	toolbarColor.B = 252;
	toolbarColor.A = 255;

	contentBgColor.R = 255;
	contentBgColor.G = 255;
	contentBgColor.B = 255;
	contentBgColor.A = 255;

	borderColor.R = 220;
	borderColor.G = 220;
	borderColor.B = 220;
	borderColor.A = 255;

	inputBgColor.R = 245;
	inputBgColor.G = 247;
	inputBgColor.B = 250;
	inputBgColor.A = 255;

	// Background
	addColorFillWidget(&desktop, bgColor, 0, 0, desktop.width,
			   desktop.height, 0, emptyHandler);

	// Sidebar
	sidebar_bg_widget =
		addColorFillWidget(&desktop, sidebarColor, 0, 0, sidebarWidth,
				   desktop.height, 0, emptyHandler);

	// Sidebar items
	addTextWidget(&desktop, dirColor, "< Home", 10, 10, 120, 22, 0,
		      homeHandler);

	// Toolbar
	toolbar_bg_widget = addColorFillWidget(
		&desktop, toolbarColor, sidebarWidth, 0,
		desktop.width - sidebarWidth, toolbarHeight, 0, emptyHandler);

	// Toolbar buttons - single click only
	addButtonWidget(&desktop, textColor, buttonColor, "<", sidebarWidth + 5,
			10, 30, 25, 0, backHandler);
	addButtonWidget(&desktop, textColor, buttonColor, "+Dir",
			sidebarWidth + 40, 10, 45, 25, 0, newFolderHandler);
	addButtonWidget(&desktop, textColor, buttonColor, "+File",
			sidebarWidth + 90, 10, 45, 25, 0, newFileHandler);
	addButtonWidget(&desktop, textColor, buttonColor, "Ren",
			sidebarWidth + 140, 10, 40, 25, 0, renameHandler);

	// Content area background
	content_bg_widget = addColorFillWidget(
		&desktop, contentBgColor, sidebarWidth, toolbarHeight,
		desktop.width - sidebarWidth, inputPanelY - toolbarHeight, 0,
		emptyHandler);

	// Path display
	memset(current_path, 0, MAX_LONG_STRLEN);
	current_path_widget = addTextWidget(
		&desktop, dirColor, "/", itemStartX, itemStartY - 25,
		desktop.width - itemStartX - 10, 18, 0, emptyHandler);

	gui_ls(current_path);

	addColorFillWidget(&desktop, bgColor, 0, 0, 0, 0, 0,
			   explorerKeyHandler);
	desktop.keyfocus = desktop.widgetlisttail;

	while (1) {
		updateWindow(&desktop);

		// Draw selection highlight - only once per frame
		if (selected_widget != -1 && selected_w > 0 &&
		    input_mode == 0) {
			int draw_y = selected_y - desktop.scrollOffsetY;

			// Only draw if visible in content area
			if (draw_y + selected_h >= itemStartY &&
			    draw_y < inputPanelY) {
				// Draw filled selection background
				drawFillRect(&desktop, selectedColor,
					     selected_x, draw_y, selected_w,
					     selected_h);

				// Draw border
				RGB borderHighlight;
				borderHighlight.R = 66;
				borderHighlight.G = 135;
				borderHighlight.B = 245;
				drawRect(&desktop, borderHighlight, selected_x,
					 draw_y, selected_w, selected_h);
			}
		}
	}
}