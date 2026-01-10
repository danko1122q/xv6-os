#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "memlayout.h"
#include "user_gui.h"
#include "user_window.h"
#include "user_handler.h"
#include "gui.h"
#include "msg.h"
//#include "defs.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
window desktop;

struct RGBA desktopColor;
struct RGBA buttonColor;
struct RGBA textColor;
char *GUI_programs[] = {"shell", "editor", "explorer", "demo"};

void startProgramHandler(Widget *widget, message *msg)
{
    if (msg->msg_type == M_MOUSE_DBCLICK)
    {
        if (fork() == 0)
        {
            //printf(1, "fork new process\n");
            char *argv2[] = {widget->context.button->text};
            exec(argv2[0], argv2);
            exit();
        }
    }
}

void startWindowHandler(Widget *widget, message *msg)
{
    int mouse_x = msg->params[0];
    int mouse_y = msg->params[1];
    if (msg->msg_type == M_MOUSE_LEFT_CLICK && mouse_x < START_ICON_WIDTH && mouse_y > SCREEN_HEIGHT - DOCK_HEIGHT)
    {

        if (fork() == 0)
        {
            char *argv2[] = {"startWindow", (char *)desktop.handler};
            exec(argv2[0], argv2);
            exit();
        }
    }
}

int main(int argc, char *argv[])
{

    desktop.width = SCREEN_WIDTH;
    desktop.height = SCREEN_HEIGHT;
    desktop.initialPosition.xmin = 0;
    desktop.initialPosition.xmax = SCREEN_WIDTH;
    desktop.initialPosition.ymin = 0;
    desktop.initialPosition.ymax = SCREEN_HEIGHT;
    desktop.hasTitleBar = 0;
    createWindow(&desktop, "desktop");

    desktopColor.R = 66;
    desktopColor.G = 130;
    desktopColor.B = 244;
    desktopColor.A = 250;
    addColorFillWidget(&desktop, desktopColor, 0, 0, desktop.width, desktop.height, 0, emptyHandler);

    buttonColor.R = 244;
    buttonColor.G = 180;
    buttonColor.B = 0;
    buttonColor.A = 255;

    textColor.R = 0;
    textColor.G = 0;
    textColor.B = 0;
    textColor.A = 255;

    for(int i=0; i<4; i++) {
       addButtonWidget(&desktop, textColor, buttonColor, GUI_programs[i], 20, 20 + 50*i, 80, 30, 0, startProgramHandler); 
    }

    addButtonWidget(&desktop, textColor, buttonColor, "start", 5, SCREEN_HEIGHT - 36, 72, 36, 0, startWindowHandler);

    int lastTime = 0;
    while (1)
    {
        updateWindow(&desktop);
        int currentTime = uptime();
        if (currentTime - lastTime >= 2)
        {
            GUI_updateScreen();
            lastTime = currentTime;
        }
    }
}