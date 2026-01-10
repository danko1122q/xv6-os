#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "memlayout.h"
#include "user_gui.h"
#include "user_window.h"
#include "user_handler.h"
#include "gui.h"
#include "msg.h"
#include "fs.h"
#include "stat.h"
//#include "defs.h"

window desktop;
char current_path[MAX_LONG_STRLEN];
int current_path_widget;
char newDir[MAX_SHORT_STRLEN];
char buf[MAX_LONG_STRLEN];
struct RGBA textColor;
struct RGBA dirColor;

int statusBarHeight = 50;

char *GUI_programs[] = {"shell", "editor", "explorer", "demo"};

void gui_ls(char *path);

char* getFileExtension(char *filename)
{
    static char buf[DIRSIZ + 1];
    char *p;

    // Find first character after last .
    for (p = filename + strlen(filename); p >= filename && *p != '.'; p--)
        ;
    p++;

    // Return blank-padded name.
    if (strlen(p) >= DIRSIZ)
        return p;
    memmove(buf, p, strlen(p));
    memset(buf + strlen(p), '\0', 1);
    //memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
    return buf;
}

int isOpenable(char *filename) {
    int isOpenable=0;
    for(int i=0; i<4; i++) {
       if(strcmp(filename, GUI_programs[i])==0) isOpenable=1;
    }
    return isOpenable;
}

char *
getparentpath(char *path)
{
    static char buf[DIRSIZ + 1];
    char *p;

    // Find first character after last slash.
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    //p++;

    memmove(buf, path, p - path);
    buf[p - path] = '\0';
    return buf;
}

void mkdirHandler(Widget *widget, message *msg)
{
    if (msg->msg_type == M_MOUSE_DBCLICK)
    {
        memset(newDir, 0, MAX_SHORT_STRLEN);
        strcpy(newDir, current_path);
        memset(newDir + strlen(current_path), '/', 1);
        strcpy(newDir + strlen(current_path) + 1, "temp");


        if (fork() == 0)
        {
            //even if I put "temp" here, the error persists
            char *argv2[] = {"mkdir", newDir};
            exec(argv2[0], argv2);
            exit();
        }
        wait();
        gui_ls(current_path);
    }
}

void backHandler(Widget *widget, message *msg)
{
    if (msg->msg_type == M_MOUSE_DBCLICK)
    {
        strcpy(current_path, getparentpath(current_path));
        gui_ls(current_path);
    }
}

void cdHandler(Widget *widget, message *msg)
{
    if (msg->msg_type == M_MOUSE_DBCLICK)
    {
        int current_path_length = strlen(current_path);
        current_path[current_path_length] = '/';
        strcpy(current_path + current_path_length + 1, widget->context.text->text);
        gui_ls(current_path);
    }
}

void buttonHandler(Widget *widget, message *msg)
{
    if (msg->msg_type == M_MOUSE_DBCLICK)
    {
        if (fork() == 0)
        {
            //printf(1, "fork new process\n");
            char *fileName = widget->context.text->text;
            //printf(1, "extension is: ");
            //printf(1, getFileExtension(fileName));
            if (strcmp(getFileExtension(fileName), "txt") == 0)
            {
                //printf(1, "\nopen editor\n");
                char *argv2[] = {"editor", widget->context.text->text};
                exec(argv2[0], argv2);
                exit();
            }
            else
            {
                char *argv2[] = {widget->context.text->text};
                exec(argv2[0], argv2);
                exit();
            }

            
        }
    }
}

char *
fmtname(char *path)
{
    static char buf[DIRSIZ + 1];
    char *p;

    // Find first character after last slash.
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;

    // Return blank-padded name.
    if (strlen(p) >= DIRSIZ)
        return p;
    memmove(buf, p, strlen(p));
    memset(buf + strlen(p), '\0', 1);
    //memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
    return buf;
}

void gui_ls(char *path)
{
    strcpy(desktop.widgets[current_path_widget].context.text->text, path);
    printf(1, desktop.widgets[current_path_widget].context.text->text);

    while (1)
    {
        int p;
        for (p = desktop.widgetlisthead; p != -1; p = desktop.widgets[p].next)
        {
            if (desktop.widgets[p].type == TEXT && current_path_widget!=p)
            {
                removeWidget(&desktop, p);
                break;
            }
        }
        if (p == -1)
        {
            break;
        }
    }

    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    int lineCount = 0;

    if ((fd = open(path, 0)) < 0)
    {
        printf(2, "ls: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0)
    {
        printf(2, "ls: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type)
    {
    case T_FILE:
        addTextWidget(&desktop, textColor, fmtname(path), 10, statusBarHeight + lineCount * CHARACTER_HEIGHT, 200, CHARACTER_HEIGHT, 1, emptyHandler);
        lineCount++;
        //printf(1, "%s %d %d %d\n", fmtname(path), st.type, st.ino, st.size);
        break;

    case T_DIR:
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
        {
            printf(1, "ls: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0)
                continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if (stat(buf, &st) < 0)
            {
                printf(1, "ls: cannot stat %s\n", buf);
                continue;
            }
            //printf(1, "%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
            char formatName[MAX_SHORT_STRLEN];
            strcpy(formatName, fmtname(buf));
            if (st.type == T_FILE && (isOpenable(formatName) || strcmp(getFileExtension(formatName), "txt") == 0))
            {
                addTextWidget(&desktop, textColor, formatName, 10, statusBarHeight + lineCount * CHARACTER_HEIGHT, 200, CHARACTER_HEIGHT, 1, buttonHandler);
                lineCount++;
            }
            if (st.type == T_DIR && strcmp(formatName, ".") != 0 && strcmp(formatName, "..") != 0)
            {
                addTextWidget(&desktop, dirColor, formatName, 10, statusBarHeight + lineCount * CHARACTER_HEIGHT, 200, CHARACTER_HEIGHT, 1, cdHandler);
                lineCount++;
            }
        }
        break;
    }
    close(fd);
}

int main(int argc, char *argv[])
{

    struct RGBA bgColor;

    desktop.width = 400;
    desktop.height = 400;
    desktop.hasTitleBar = 1;
    createWindow(&desktop, "explorer");

    bgColor.R = 255;
    bgColor.G = 255;
    bgColor.B = 255;
    bgColor.A = 250;
    textColor.R = 2;
    textColor.G = 6;
    textColor.B = 5;
    textColor.A = 255;
    dirColor.R = 66;
    dirColor.G = 130;
    dirColor.B = 245;
    dirColor.A = 255;
    addColorFillWidget(&desktop, bgColor, 0, 0, desktop.width, desktop.height, 0, emptyHandler);

    strcpy(current_path, "");

    gui_ls(current_path);

        struct RGBA buttonColor;
    buttonColor.R = 244;
    buttonColor.G = 180;
    buttonColor.B = 0;
    buttonColor.A = 255;
    addButtonWidget(&desktop, textColor, buttonColor, "mkdir", 80, 10, 50, 30, 0, mkdirHandler);
    addButtonWidget(&desktop, textColor, buttonColor, "back", 10, 10, 50, 30, 0, backHandler);
    current_path_widget=addTextWidget(&desktop, textColor, current_path, 160, 10+6, 200, CHARACTER_HEIGHT, 0, emptyHandler);

    while (1)
    {
        updateWindow(&desktop);
    }
}