#ifndef __WINDOW_MANAGER_H__
#define __WINDOW_MANAGER_H__

#define MSG_BUF_SIZE 50
#define MAX_TITLE_LEN 50
#define TITLE_HEIGHT 28
#define DOCK_HEIGHT 36
#define DOCK_PROGRAM_NORMAL_WIDTH 160
#define START_ICON_WIDTH 72
#define SHOW_DESKTOP_ICON_WIDTH 10

#include "msg.h"

typedef struct win_rect
{
	int xmin, xmax, ymin, ymax;
} win_rect;

typedef struct msg_buf
{
	message data[MSG_BUF_SIZE];
	int front, rear, cnt;
} msg_buf;


typedef struct kernel_window
{
	win_rect position;
	struct RGB *window_buf;
	struct msg_buf msg_buf;
	char title[MAX_TITLE_LEN];
	int minimized;
	int hasTitleBar;

} kernel_window;

#endif