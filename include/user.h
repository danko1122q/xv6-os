#ifndef USER_H
#define USER_H

#include "types.h"

struct stat;
struct rtcdate;
struct RGBA;
struct RGB;
struct message;
struct Widget;
struct window;

typedef void (*Handler)(struct Widget *, struct message *);

// system calls
int fork(void);
int exit(void) __attribute__((noreturn));
int wait(void);
int pipe(int *);
int write(int, const void *, int);
int read(int, void *, int);
int close(int);
int kill(int);
int exec(char *, char **);
int open(const char *, int);
int mknod(const char *, short, short);
int unlink(const char *);
int fstat(int fd, struct stat *);
int link(const char *, const char *);
int mkdir(const char *);
int chdir(const char *);
int dup(int);
int getpid(void);
char *sbrk(int);
int sleep(int);
int uptime(void);
int GUI_createWindow(struct window *, const char *);
int GUI_closeWindow(struct window *);
int GUI_getMessage(int, struct message *);
int GUI_getPopupMessage(struct message *);
int GUI_updateScreen(void);
int GUI_turnoffScreen(void);
int GUI_maximizeWindow(struct window *);
int GUI_minimizeWindow(struct window *);
int GUI_createPopupWindow(struct window *, int);
int GUI_closePopupWindow(struct window *);
int halt(void);
int reboot(void);

// ulib.c
int stat(const char *, struct stat *);
char *strcpy(char *, const char *);
char *strcat(char *, const char *);
void *memmove(void *, const void *, int);
char *strchr(const char *, char c);
int strcmp(const char *, const char *);
void printf(int, const char *, ...);
char *gets(char *, int max);
uint strlen(const char *);
void *memset(void *, int, uint);
void *malloc(uint);
void free(void *);
int atoi(const char *);

// user_window.c
void debugPrintWidgetList(struct window *win);
void createPopupWindow(struct window *, int);
void closePopupWindow(struct window *);
void createWindow(struct window *, const char *);
void closeWindow(struct window *);
void updateWindow(struct window *);
void updatePopupWindow(struct window *);
int addButtonWidget(struct window *win, struct RGBA c, struct RGBA bc,
		    char *text, int x, int y, int w, int h, int,
		    Handler handler);
int addTextWidget(struct window *win, struct RGBA c, char *text, int x, int y,
		  int w, int h, int, Handler handler);
int addInputFieldWidget(struct window *win, struct RGBA c, char *text, int x,
			int y, int w, int h, int, Handler handler);
int addColorFillWidget(struct window *win, struct RGBA c, int x, int y, int w,
		       int h, int, Handler handler);
int addRectangleWidget(struct window *win, struct RGBA c,
		       struct RGBA filledColor, int filled, int x, int y, int w,
		       int h, int scrollable, Handler handler);
int removeWidget(struct window *win, int index);
int setWidgetHandler(struct window *win, int index, Handler handler);
int findWidgetId(struct window *win, struct Widget *widget);

// user_gui.c
void fillRect(struct RGB *buf, int x, int y, int width, int height, int max_x,
	      int max_y, struct RGBA fill);
void drawRect(struct window *win, struct RGB color, int x, int y, int width,
	      int height);
void drawFillRect(struct window *win, struct RGBA color, int x, int y,
		  int width, int height);
void drawString(struct window *win, char *str, struct RGBA color, int x, int y,
		int width, int height);
void draw24Image(struct window *win, struct RGB *img, int x, int y, int width,
		 int height);
void drawIcon(struct window *win, int icon, struct RGBA color, int x, int y,
	      int width, int height);

// user_handler.c
void emptyHandler(struct Widget *w, struct message *msg);
int getInputOffsetFromMousePosition(char *str, int width, int mouse_x,
				    int mouse_y);
int getMouseXFromOffset(char *str, int width, int offset);
int getMouseYFromOffset(char *str, int width, int offset);
void inputMouseLeftClickHandler(struct Widget *w, struct message *msg);
void inputFieldKeyHandler(struct Widget *w, struct message *msg);
int getScrollableTotalHeight(struct window *win);
int addScrollBarWidget(struct window *window, struct RGBA color,
		       Handler handler);

#endif // USER_H