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

window programWindow;
char filename[40];
int file = -1;

int lastMaximumOffset = 0;

void buttonHandler(Widget *widget, message *msg)
{
    if (msg->msg_type == M_MOUSE_LEFT_CLICK || msg->msg_type == M_MOUSE_DBCLICK)
    {
        if (file != -1)
        {
            file = open(filename, O_RDWR);
        }
        else
        {
            file = open("/new.txt", O_CREATE);
        }

        int i;
        for (i = programWindow.widgetlisthead; i != -1; i = programWindow.widgets[i].next)
        {
            if (programWindow.widgets[i].type == INPUTFIELD)
            {
                break;
            }
        }
        write(file, programWindow.widgets[i].context.inputfield->text, 512);
        close(file);
    }
}

void inputHandler(Widget *w, message *msg)
{

    int width = w->position.xmax - w->position.xmin;
    int height = w->position.ymax - w->position.ymin;

    if (msg->msg_type == M_MOUSE_LEFT_CLICK)
    {
        inputMouseLeftClickHandler(w, msg);
    }
    else if (msg->msg_type == M_KEY_DOWN)
    {
        inputFieldKeyHandler(w, msg);

        //MODIFY THE HEIGHT OF THE INPUTFIELD
        int newHeight = CHARACTER_HEIGHT * (getMouseYFromOffset(w->context.inputfield->text, width, strlen(w->context.inputfield->text)) + 1);
        if (newHeight > height)
        {
            w->position.ymax = w->position.ymin + newHeight;
        }

        //MODIFY THE SCROLL OFFSET IF IT CHANGES
        int maximumOffset = getScrollableTotalHeight(&programWindow) - programWindow.height;
        if (maximumOffset > 0 && lastMaximumOffset != maximumOffset)
        {
            programWindow.scrollOffsetY = maximumOffset;
            lastMaximumOffset = maximumOffset;
        }

        int currentHeight = getMouseYFromOffset(w->context.inputfield->text, width, w->context.inputfield->current_pos) * CHARACTER_HEIGHT+ w->position.ymin - programWindow.scrollOffsetY;

        if (currentHeight <= w->position.ymin && programWindow.scrollOffsetY>0)
            programWindow.scrollOffsetY -= CHARACTER_HEIGHT;
        if (currentHeight >= programWindow.height-1 && programWindow.scrollOffsetY<=maximumOffset)
            programWindow.scrollOffsetY += CHARACTER_HEIGHT;
            
    }
}

int main(int argc, char *argv[])
{

    struct RGBA bgColor;
    struct RGBA textColor;
    programWindow.width = 400;
    programWindow.height = 300;
    programWindow.hasTitleBar = 1;
    createWindow(&programWindow, "editor");

    bgColor.R = 255;
    bgColor.G = 255;
    bgColor.B = 255;
    bgColor.A = 250;
    addColorFillWidget(&programWindow, bgColor, 0, 0, programWindow.width, programWindow.height, 0, buttonHandler);

    textColor.R = 2;
    textColor.G = 6;
    textColor.B = 5;
    textColor.A = 255;

    if (argc > 1)
    {
        strcpy(filename, argv[1]);
        file = open(filename, O_RDWR);
    }

    struct RGBA buttonColor;
    buttonColor.R = 244;
    buttonColor.G = 180;
    buttonColor.B = 0;
    buttonColor.A = 255;

    char initial[MAX_LONG_STRLEN] = "Start typing...";

    if (file >= 0)
    {
        memset(initial, 0, MAX_LONG_STRLEN);
        read(file, initial, MAX_LONG_STRLEN);
        close(file);
    }

    addInputFieldWidget(&programWindow, textColor, initial, 10, 50, programWindow.width - 10, programWindow.height - 50, 1, inputHandler);
    addButtonWidget(&programWindow, textColor, buttonColor, "save", 10, 10, 50, 30, 0, buttonHandler);

    while (1)
    {
        updateWindow(&programWindow);
    }
}