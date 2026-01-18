#include "defs.h"
#include "gui.h"
#include "memlayout.h"
#include "mmu.h"
#include "msg.h"
#include "param.h"
#include "proc.h"
#include "spinlock.h"
#include "types.h"
#include "x86.h"

#include "user_window.h"

#include "window_manager.h"

int min(int x, int y) { return x < y ? x : y; }
int max(int x, int y) { return x > y ? x : y; }
int clamp(int x, int l, int r) { return min(r, max(l, x)); }

struct RGBA titleBarColor;
struct RGBA dockColor;
struct RGBA closeColor;
struct RGBA txtColor;
struct RGBA minimizeColor;
struct RGBA iconColor;
struct RGBA titleTextColor;

#define MAX_WINDOW_CNT 50

// linked-list of windows
static struct {
	struct proc *proc;
	kernel_window wnd;
	int next, prev;
} windowlist[MAX_WINDOW_CNT];

static int windowlisthead, windowlisttail;
static int desktopId = -1;

static struct {
	struct proc *proc;
	kernel_window wnd;
	int caller;
} popupwindow;

static int mouseShape;

static int clickedOnTitle, clickedOnContent, clickedOnPopup;

struct spinlock wmlock;

typedef struct {
	int x, y;
} mouse_pos_t;

static mouse_pos_t wm_mouse_pos, wm_last_mouse_pos;

#define MOUSE_SPEED_X 1
#define MOUSE_SPEED_Y -1

int isInRect(int xmin, int ymin, int xmax, int ymax, int x, int y) {
	return (x >= xmin && x <= xmax && y >= ymin && y <= ymax);
}

void createRectByCoord(win_rect *rect, int xmin, int ymin, int xmax, int ymax) {
	rect->xmin = xmin;
	rect->xmax = xmax;
	rect->ymin = ymin;
	rect->ymax = ymax;
}

void createRectBySize(win_rect *rect, int xmin, int ymin, int width,
		      int height) {
	rect->xmin = xmin;
	rect->xmax = xmin + width;
	rect->ymin = ymin;
	rect->ymax = ymin + height;
}

void moveRect(win_rect *rect, int dx, int dy) {
	rect->xmin += dx;
	rect->xmax += dx;
	rect->ymin += dy;
	rect->ymax += dy;
}

int findNextAvailableWindowId() {
	for (int i = 0; i < MAX_WINDOW_CNT; i++) {
		if (windowlist[i].prev == i && windowlist[i].next == i) {
			return i;
		}
	}
	return -1;
}

void addToWindowList(int idx) {
	windowlist[idx].prev = windowlisttail;
	windowlist[idx].next = -1;
	if (windowlisttail != -1)
		windowlist[windowlisttail].next = idx;
	windowlisttail = idx;
}

void removeFromWindowList(int idx) {
	if (windowlisttail == idx)
		windowlisttail = windowlist[windowlisttail].prev;
	if (windowlist[idx].prev != -1)
		windowlist[windowlist[idx].prev].next = windowlist[idx].next;
	if (windowlist[idx].next != -1)
		windowlist[windowlist[idx].next].prev = windowlist[idx].prev;
}

void initMessageQueue(msg_buf *buf) { buf->front = buf->rear = buf->cnt = 0; }

int dispatchMessage(msg_buf *buf, message *msg) {
	if (buf->cnt >= MSG_BUF_SIZE)
		return 1;
	++buf->cnt;
	buf->data[buf->rear] = *msg;
	if ((++buf->rear) >= MSG_BUF_SIZE)
		buf->rear = 0;
	return 0;
}

int getMessage(msg_buf *buf, message *result) {
	if (buf->cnt == 0)
		return 1; // Keluar lebih awal (Guard)

	acquire(&wmlock);
	*result = buf->data[buf->front];
	buf->front = (buf->front + 1) %
		     MSG_BUF_SIZE; // Gunakan modulo agar lebih bersih
	buf->cnt--;
	release(&wmlock);

	return 0;
}

void wmInit() {
	// Modern title bar - dark blue/gray
	titleBarColor = (struct RGBA){.R = 45, .G = 52, .B = 64, .A = 255};
	dockColor = (struct RGBA){.R = 30, .G = 35, .B = 42, .A = 255};

	// Modern dock - dark
	dockColor.R = 30;
	dockColor.G = 35;
	dockColor.B = 42;
	dockColor.A = 255;

	// Modern close button - red
	closeColor.R = 239;
	closeColor.G = 83;
	closeColor.B = 80;
	closeColor.A = 255;

	// Icon color - white
	iconColor.R = 255;
	iconColor.G = 255;
	iconColor.B = 255;
	iconColor.A = 255;

	// Title text - white
	titleTextColor.R = 255;
	titleTextColor.G = 255;
	titleTextColor.B = 255;
	titleTextColor.A = 255;

	// Minimize button - gray
	minimizeColor.R = 100;
	minimizeColor.G = 108;
	minimizeColor.B = 120;
	minimizeColor.A = 255;

	txtColor.R = txtColor.G = txtColor.B = txtColor.A = 255;

	mouseShape = 0;
	wm_mouse_pos.x = SCREEN_WIDTH / 2;
	wm_mouse_pos.y = SCREEN_HEIGHT / 2;
	wm_last_mouse_pos = wm_mouse_pos;

	windowlisthead = -1;
	windowlisttail = -1;
	int i;
	for (i = 0; i < MAX_WINDOW_CNT; ++i) {
		windowlist[i].next = i;
		windowlist[i].prev = i;
	}

	popupwindow.caller = -1;

	clickedOnTitle = clickedOnContent = clickedOnPopup = 0;

	initlock(&wmlock, "wmlock");
}

void debugPrintWindowList() {
	cprintf("############################\n");
	cprintf("current Proc at %d\n", myproc());
	cprintf("current Head at %d\n", windowlisthead);
	cprintf("current Tail at %d\n", windowlisttail);
	cprintf("\n");

	int p;
	for (p = windowlisthead; p != -1; p = windowlist[p].next) {
		cprintf("current Window at %d\n", p);
		cprintf("current Window proc %d\n", windowlist[p].proc);
		cprintf("prev Window at %d\n", windowlist[p].prev);
		cprintf("next Window at %d\n", windowlist[p].next);
		cprintf("\n");
	}
}

void focusWindow(int winId) {
	if (winId == -1 || winId == windowlisttail)
		return;
	if (winId == windowlisthead) {
		int newhead = windowlist[winId].next;
		windowlist[newhead].prev = -1;
		windowlisthead = newhead;
	} else {
		int prevWin = windowlist[winId].prev;
		int nextWin = windowlist[winId].next;
		windowlist[prevWin].next = nextWin;
		windowlist[nextWin].prev = prevWin;
	}
	addToWindowList(winId);
}

void moveFocusWindow(int dx, int dy) {
	if (windowlist[windowlisttail].wnd.hasTitleBar) {
		moveRect(&windowlist[windowlisttail].wnd.position, dx, dy);
	}
}

int getWindowCount() {
	int windowCount = 0;

	for (int p = windowlisthead; p != -1; p = windowlist[p].next) {
		if (p != desktopId) {
			windowCount++;
		}
	}
	return windowCount;
}

void handleDesktopDockClick(int mouse_x, message *msg) {
	int p;
	int windowCount = getWindowCount();

	if (windowCount > 0) // handle each program dock bar click
	{
		int xStart = START_ICON_WIDTH + 5;
		int barWidth = min((SCREEN_WIDTH - START_ICON_WIDTH -
				    SHOW_DESKTOP_ICON_WIDTH) /
					   (windowCount + 1),
				   DOCK_PROGRAM_NORMAL_WIDTH);
		for (p = windowlisthead; p != -1; p = windowlist[p].next) {
			if (p != desktopId) {
				if (mouse_x > xStart + 1 &&
				    mouse_x <= xStart + barWidth - 1) {
					if (windowlist[p].wnd.minimized) {
						message newmsg;
						newmsg.msg_type =
							WM_WINDOW_MAXIMIZE;
						dispatchMessage(
							&windowlist[p]
								 .wnd.msg_buf,
							&newmsg);
					} else {
						focusWindow(p);
					}
					break;
				}
				xStart += barWidth;
			}
		}
	}
	if (mouse_x >=
	    SCREEN_WIDTH -
		    SHOW_DESKTOP_ICON_WIDTH) // handle show desktop icon click
	{
		for (p = windowlisthead; p != -1; p = windowlist[p].next) {
			if (p != desktopId) {
				if (windowlist[p].wnd.minimized == 0) {
					message newmsg;
					newmsg.msg_type = WM_WINDOW_MINIMIZE;
					dispatchMessage(
						&windowlist[p].wnd.msg_buf,
						&newmsg);
				}
			}
		}
	}
	if (mouse_x <= START_ICON_WIDTH &&
	    msg->msg_type ==
		    M_MOUSE_LEFT_CLICK) // handle start window icon click
	{
		if (popupwindow.caller != desktopId) {
			message newmsg;
			newmsg.msg_type = msg->msg_type;
			newmsg.params[0] = wm_mouse_pos.x;
			newmsg.params[1] = wm_mouse_pos.y;
			dispatchMessage(&windowlist[desktopId].wnd.msg_buf,
					&newmsg);
		}
	}
}

// handle messages from mouse and keyboard
void wmHandleMessage(message *msg) {
	acquire(&wmlock);

	message newmsg;
	switch (msg->msg_type) {
	case M_MOUSE_MOVE:
		wm_last_mouse_pos = wm_mouse_pos;
		if (msg->params[0] > -SCREEN_WIDTH &&
		    msg->params[0] < SCREEN_WIDTH) {
			if (msg->params[1] > -SCREEN_HEIGHT &&
			    msg->params[0] < SCREEN_HEIGHT) {
				wm_mouse_pos.x +=
					msg->params[0] * MOUSE_SPEED_X;
				wm_mouse_pos.y +=
					msg->params[1] * MOUSE_SPEED_Y;
			}
		}

		if (wm_mouse_pos.x > SCREEN_WIDTH)
			wm_mouse_pos.x = SCREEN_WIDTH;
		if (wm_mouse_pos.y > SCREEN_HEIGHT)
			wm_mouse_pos.y = SCREEN_HEIGHT;
		if (wm_mouse_pos.x < 0)
			wm_mouse_pos.x = 0;
		if (wm_mouse_pos.y < 0)
			wm_mouse_pos.y = 0;

		if (clickedOnTitle) // dragging the title bar
		{
			mouseShape = 1;
			moveFocusWindow(wm_mouse_pos.x - wm_last_mouse_pos.x,
					wm_mouse_pos.y - wm_last_mouse_pos.y);
		} else if (clickedOnContent) // dragging inside the window
		{
			newmsg.msg_type = msg->msg_type;
			newmsg.params[0] =
				wm_mouse_pos.x -
				windowlist[windowlisttail].wnd.position.xmin;
			newmsg.params[1] =
				wm_mouse_pos.y -
				windowlist[windowlisttail].wnd.position.ymin;
			dispatchMessage(&windowlist[windowlisttail].wnd.msg_buf,
					&newmsg);
		}
		break;
	case M_MOUSE_DOWN:
		// check if clicked on the popup first
		if (popupwindow.caller != -1 &&
		    isInRect(popupwindow.wnd.position.xmin,
			     popupwindow.wnd.position.ymin,
			     popupwindow.wnd.position.xmax,
			     popupwindow.wnd.position.ymax, wm_mouse_pos.x,
			     wm_mouse_pos.y)) {
			clickedOnPopup = 1;
		} else {
			// close the popup first
			message closePopup;
			closePopup.msg_type = WM_WINDOW_CLOSE;
			dispatchMessage(&popupwindow.wnd.msg_buf, &closePopup);
			// find the focus window
			int p = windowlisttail;
			for (; p != -1; p = windowlist[p].prev) {
				kernel_window *win = &windowlist[p].wnd;
				if (p != desktopId && win->minimized != 1 &&
				    isInRect(win->position.xmin,
					     win->position.ymin - TITLE_HEIGHT,
					     win->position.xmax,
					     win->position.ymax, wm_mouse_pos.x,
					     wm_mouse_pos.y)) {
					focusWindow(p);
					break;
				}
			}
			if (p == -1)
				focusWindow(desktopId);

			// check if clicked on content or the title bar
			kernel_window *win = &windowlist[windowlisttail].wnd;
			if (isInRect(win->position.xmin,
				     win->position.ymin - TITLE_HEIGHT,
				     win->position.xmax, win->position.ymin,
				     wm_mouse_pos.x, wm_mouse_pos.y)) {
				if (wm_mouse_pos.x + 30 > win->position.xmax) {
					newmsg.msg_type = WM_WINDOW_CLOSE;
					dispatchMessage(
						&windowlist[windowlisttail]
							 .wnd.msg_buf,
						&newmsg);
				} else if (wm_mouse_pos.x + 60 >
						   win->position.xmax &&
					   wm_mouse_pos.x + 30 <=
						   win->position.xmax) {
					newmsg.msg_type = WM_WINDOW_MINIMIZE;
					dispatchMessage(
						&windowlist[windowlisttail]
							 .wnd.msg_buf,
						&newmsg);
				} else {
					clickedOnTitle = 1;
				}
			}
			// dispatch the message
			if (isInRect(win->position.xmin, win->position.ymin,
				     win->position.xmax, win->position.ymax,
				     wm_mouse_pos.x, wm_mouse_pos.y)) {
				clickedOnContent = 1;
				newmsg.msg_type = msg->msg_type;
				newmsg.params[0] =
					wm_mouse_pos.x -
					popupwindow.wnd.position.xmin;
				newmsg.params[1] =
					wm_mouse_pos.y -
					popupwindow.wnd.position.ymin;
				dispatchMessage(
					&windowlist[windowlisttail].wnd.msg_buf,
					&newmsg);
			}
		}

		break;

	case M_MOUSE_LEFT_CLICK:
	case M_MOUSE_RIGHT_CLICK:
	case M_MOUSE_DBCLICK:
		if (clickedOnPopup) {
			clickedOnPopup = 0;
			newmsg.msg_type = msg->msg_type;
			newmsg.params[0] =
				wm_mouse_pos.x - popupwindow.wnd.position.xmin;
			newmsg.params[1] =
				wm_mouse_pos.y - popupwindow.wnd.position.ymin;
			dispatchMessage(&popupwindow.wnd.msg_buf, &newmsg);
		} else if (clickedOnContent) {
			clickedOnContent = 0;
			// check if clicked on window dock
			if (windowlisttail == desktopId &&
			    wm_mouse_pos.y >= SCREEN_HEIGHT - DOCK_HEIGHT &&
			    wm_mouse_pos.y <= SCREEN_HEIGHT) {
				handleDesktopDockClick(wm_mouse_pos.x, msg);
			} else {
				newmsg.msg_type = msg->msg_type;
				newmsg.params[0] = wm_mouse_pos.x -
						   windowlist[windowlisttail]
							   .wnd.position.xmin;
				newmsg.params[1] = wm_mouse_pos.y -
						   windowlist[windowlisttail]
							   .wnd.position.ymin;
				dispatchMessage(
					&windowlist[windowlisttail].wnd.msg_buf,
					&newmsg);
				break; // Tambahkan break agar tidak lanjut ke
				       // M_MOUSE_UP
			}
		}
	case M_MOUSE_UP:
		if (clickedOnPopup) {
			clickedOnPopup = 0;
		}
		if (clickedOnContent) {
			clickedOnContent = 0;
			newmsg.msg_type = msg->msg_type;
			newmsg.params[0] =
				wm_mouse_pos.x -
				windowlist[windowlisttail].wnd.position.xmin;
			newmsg.params[1] =
				wm_mouse_pos.y -
				windowlist[windowlisttail].wnd.position.ymin;
			dispatchMessage(&windowlist[windowlisttail].wnd.msg_buf,
					&newmsg);
		}
		if (clickedOnTitle) {
			clickedOnTitle = 0;
		}
		mouseShape = 0;
		break;
	case M_KEY_DOWN:
	case M_KEY_UP:
		dispatchMessage(&windowlist[windowlisttail].wnd.msg_buf, msg);
		break;

	default:
		break;
	}
	release(&wmlock);
}

void drawWindowBar(struct RGB *dst, kernel_window *win, struct RGBA barcolor) {
	int xmin = win->position.xmin;
	int xmax = win->position.xmax + 1;
	int ymin = win->position.ymin - TITLE_HEIGHT;
	int ymax = win->position.ymin;

	// Draw title bar background
	drawRectByCoord(dst, xmin, ymin, xmax - 2 * TITLE_HEIGHT, ymax,
			barcolor);

	// Minimize button
	drawRectByCoord(dst, xmax - 2 * TITLE_HEIGHT, ymin, xmax - TITLE_HEIGHT,
			ymax, minimizeColor);

	// Close button
	drawRectByCoord(dst, xmax - TITLE_HEIGHT, ymin, xmax, ymax, closeColor);

	// Title text - white
	drawString(dst, xmin + 8, ymin + 6, win->title, titleTextColor);

	// Icons - white
	drawIcon(dst, xmax - 2 * TITLE_HEIGHT - 1, ymin - 1, 1, iconColor);
	drawIcon(dst, xmax - TITLE_HEIGHT - 1, ymin - 1, 0, iconColor);
}

void drawWindow(kernel_window *win) {
	int width = win->position.xmax - win->position.xmin;
	int height = win->position.ymax - win->position.ymin;

	// directly flush the RGB array of window onto screen_buf
	draw24ImagePart(screen_buf, win->window_buf, win->position.xmin,
			win->position.ymin, width, height, 0, 0, width, height);

	// Window border - subtle gray
	RGB borderColor;
	borderColor.R = 60;
	borderColor.G = 68;
	borderColor.B = 82;
	drawRectBorder(screen_buf, borderColor, win->position.xmin,
		       win->position.ymin, width, height);

	if (win->hasTitleBar) {
		drawWindowBar(screen_buf, win, titleBarColor);
	}
}

void drawDesktopDock(struct RGB *dst) {
	drawRectByCoord(dst, 0, SCREEN_HEIGHT - DOCK_HEIGHT, SCREEN_WIDTH,
			SCREEN_HEIGHT, dockColor);

	int p;
	int windowCount = getWindowCount();

	// Start button - accent color
	struct RGBA startBtnColor;
	startBtnColor.R = 66;
	startBtnColor.G = 135;
	startBtnColor.B = 245;
	startBtnColor.A = 255;

	drawRectByCoord(dst, 0, SCREEN_HEIGHT - DOCK_HEIGHT, START_ICON_WIDTH,
			SCREEN_HEIGHT, startBtnColor);

	if (windowCount > 0) {
		int xStart = START_ICON_WIDTH + 5;
		int barWidth = min((SCREEN_WIDTH - START_ICON_WIDTH -
				    SHOW_DESKTOP_ICON_WIDTH) /
					   (windowCount + 1),
				   DOCK_PROGRAM_NORMAL_WIDTH);
		for (p = windowlisthead; p != -1; p = windowlist[p].next) {
			if (p != desktopId) {
				drawStringWithMaxWidth(
					dst, xStart + 5,
					SCREEN_HEIGHT - DOCK_HEIGHT * 0.7,
					barWidth - 2, windowlist[p].wnd.title,
					txtColor);
				drawRectByCoord(dst, xStart + barWidth - 2,
						SCREEN_HEIGHT - DOCK_HEIGHT,
						xStart + barWidth,
						SCREEN_HEIGHT, txtColor);
				xStart += barWidth;
			}
		}
	}

	// Show desktop button
	drawRectByCoord(dst, SCREEN_WIDTH - SHOW_DESKTOP_ICON_WIDTH,
			SCREEN_HEIGHT - DOCK_HEIGHT, SCREEN_WIDTH,
			SCREEN_HEIGHT, startBtnColor);
	drawIcon(dst, START_ICON_WIDTH / 2 - 15,
		 SCREEN_HEIGHT - DOCK_HEIGHT + 3, 2, iconColor);
}

// the only place that actually updates the screen
void updateScreen() {
	acquire(&wmlock);
	if (myproc() != windowlist[desktopId].proc) {
		panic("Update screen called by non desktop process");
		release(&wmlock);
		return;
	}

	memset(screen_buf, 255, screen_size);
	// draw desktop first
	drawWindow(&windowlist[desktopId].wnd);
	drawDesktopDock(screen_buf);

	// draw other windows in order
	int p;
	for (p = windowlisthead; p != -1; p = windowlist[p].next) {
		if (p != desktopId && windowlist[p].wnd.minimized == 0) {
			switchuvm(windowlist[p].proc);
			drawWindow(&windowlist[p].wnd);
		}
	}

	// draw popup window if there is one
	if (popupwindow.caller != -1) {
		switchuvm(popupwindow.proc);
		drawWindow(&popupwindow.wnd);
	}

	// switch back to the desktop process
	if (myproc() == 0)
		switchkvm();
	else
		switchuvm(myproc());

	// draw the mouse
	drawMouse(screen_buf, mouseShape, wm_mouse_pos.x, wm_mouse_pos.y);
	memmove(screen, screen_buf, screen_size);

	release(&wmlock);
}

// return window handler on succuss, -1 if unsuccessful
int createWindow(window_p window, char *title) {
	acquire(&wmlock);

	int winId = findNextAvailableWindowId();
	if (winId == -1) {
		release(&wmlock);
		return 1;
	}

	if (windowlisthead == -1) {
		windowlisthead = winId;
	}

	addToWindowList(winId);

	// the first window is always the desktop
	if (desktopId == -1) {
		desktopId = winId;
	}

	int xmin = window->initialPosition.xmin;
	int xmax = window->initialPosition.xmax;
	int ymin = window->initialPosition.ymin;
	int ymax = window->initialPosition.ymax;

	if (xmin >= 0 && xmax <= SCREEN_WIDTH && ymin >= 0 &&
	    ymax <= SCREEN_HEIGHT && xmax - xmin == window->width &&
	    ymax - ymin == window->height) {
		createRectByCoord(&windowlist[winId].wnd.position, xmin, ymin,
				  xmax, ymax);
	} else {
		createRectByCoord(&windowlist[winId].wnd.position,
				  SCREEN_WIDTH / 2 - window->width / 2,
				  SCREEN_HEIGHT / 2 - window->height / 2,
				  SCREEN_WIDTH / 2 + window->width / 2,
				  SCREEN_HEIGHT / 2 + window->height / 2);
	}

	windowlist[winId].wnd.window_buf = window->window_buf;
	window->handler = winId;
	windowlist[winId].proc = myproc();
	windowlist[winId].wnd.minimized = 0;
	windowlist[winId].wnd.hasTitleBar = window->hasTitleBar;

	initMessageQueue(&windowlist[winId].wnd.msg_buf);

	// Pastikan selalu ada ruang untuk null-terminator '\0'
	int len = strlen(title);
	if (len >= MAX_TITLE_LEN) {
		len = MAX_TITLE_LEN - 1;
	}
	memset(windowlist[winId].wnd.title, 0, MAX_TITLE_LEN);
	memmove(windowlist[winId].wnd.title, title, len);

	release(&wmlock);

	return 0;
}

int createPopupWindow(window_p window, int caller) {
	acquire(&wmlock);

	if (popupwindow.caller != -1) {
		release(&wmlock);
		return 1;
	}

	int xmin = window->initialPosition.xmin;
	int xmax = window->initialPosition.xmax;
	int ymin = window->initialPosition.ymin;
	int ymax = window->initialPosition.ymax;

	if (xmin >= 0 && xmax <= SCREEN_WIDTH && ymin >= 0 &&
	    ymax <= SCREEN_HEIGHT && xmax - xmin == window->width &&
	    ymax - ymin == window->height) {
		createRectByCoord(&popupwindow.wnd.position, xmin, ymin, xmax,
				  ymax);
	}

	popupwindow.wnd.window_buf = window->window_buf;
	popupwindow.caller = caller;
	window->handler = caller;
	popupwindow.proc = myproc();
	popupwindow.wnd.minimized = 0;
	popupwindow.wnd.hasTitleBar = window->hasTitleBar;
	initMessageQueue(&popupwindow.wnd.msg_buf);

	release(&wmlock);
	return 0;
}

int closePopupWindow(window_p window) {
	acquire(&wmlock);

	popupwindow.caller = -1;
	initMessageQueue(&popupwindow.wnd.msg_buf);
	window->handler = -1;
	memset(popupwindow.wnd.title, 0, MAX_TITLE_LEN);

	release(&wmlock);

	return 0;
}

int closeWindow(window_p window) {
	acquire(&wmlock);

	int winId = window->handler;
	removeFromWindowList(winId);
	windowlist[winId].prev = winId;
	windowlist[winId].next = winId;

	initMessageQueue(&windowlist[winId].wnd.msg_buf);
	memset(windowlist[winId].wnd.title, 0, MAX_TITLE_LEN);

	if (winId == windowlisttail) {
		focusWindow(windowlist[winId].prev);
	}

	window->handler = -1;

	release(&wmlock);

	return 0;
}

int minimizeWindow(window_p window) {
	acquire(&wmlock);

	int winId = window->handler;

	windowlist[winId].wnd.minimized = 1;
	if (winId == windowlisttail) {
		focusWindow(windowlist[winId].prev);
	}

	release(&wmlock);

	return 0;
}

int maximizeWindow(window_p window) {
	acquire(&wmlock);

	int winId = window->handler;

	windowlist[winId].wnd.minimized = 0;
	focusWindow(winId);

	release(&wmlock);

	return 0;
}

int turnoffScreen() {
	acquire(&wmlock);

	for (int p = windowlisthead; p != -1; p = windowlist[p].next) {
		message newmsg;
		newmsg.msg_type = WM_WINDOW_CLOSE;
		dispatchMessage(&windowlist[p].wnd.msg_buf, &newmsg);
	}
	memset(screen_buf, 255, screen_size);
	memmove(screen, screen_buf, screen_size);

	release(&wmlock);

	return 0;
}

// system calls
int sys_GUI_createPopupWindow() {
	window *wnd;
	int caller;
	argptr(0, (char **)&wnd, sizeof(window));
	argint(1, &caller);
	return createPopupWindow(wnd, caller);
}

int sys_GUI_closePopupWindow() {
	window *wnd;
	argptr(0, (char **)&wnd, sizeof(window));
	return closePopupWindow(wnd);
}

int sys_GUI_createWindow() {
	window *wnd;
	char *title;
	argptr(0, (char **)&wnd, sizeof(window));
	argstr(1, &title);
	return createWindow(wnd, title);
}

int sys_GUI_closeWindow() {
	window *wnd;
	argptr(0, (char **)&wnd, sizeof(window));
	return closeWindow(wnd);
}

int sys_GUI_minimizeWindow() {
	window *wnd;
	argptr(0, (char **)&wnd, sizeof(window));
	return minimizeWindow(wnd);
}

int sys_GUI_maximizeWindow() {
	window *wnd;
	argptr(0, (char **)&wnd, sizeof(window));
	return maximizeWindow(wnd);
}

int sys_GUI_getMessage() {
	int h;
	message *res;
	argint(0, &h);
	argptr(1, (char **)(&res), sizeof(message));
	if (myproc() != windowlist[h].proc) {
		return 1;
	}
	return getMessage(&windowlist[h].wnd.msg_buf, res);
}

int sys_GUI_getPopupMessage() {
	message *res;
	argptr(0, (char **)(&res), sizeof(message));
	if (popupwindow.caller == -1) {
		return 1;
	}
	return getMessage(&popupwindow.wnd.msg_buf, res);
}

int sys_GUI_updateScreen() {
	updateScreen();
	return 0;
}

int sys_GUI_turnoffScreen() {
	turnoffScreen();
	return 0;
}