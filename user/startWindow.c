#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "memlayout.h"
#include "user_gui.h"
#include "user_window.h"
#include "gui.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

window startWindow;

char *GUI_programs[] = {"shell", "editor", "explorer", "demo"};

// Handler untuk menjalankan program (Double Click)
void startProgramHandler(Widget *widget, message *msg)
{
    if (msg->msg_type == M_MOUSE_DBCLICK)
    {
        if (fork() == 0)
        {
            printf(1, "fork new process\n");
            char *argv2[] = {widget->context.button->text};
            exec(argv2[0], argv2);
            exit();
        }
    }
}

// Handler untuk Restart (M_MOUSE_DOWN agar sekali klik langsung jalan)
void rebootHandler(Widget *widget, message *msg)
{
    if (msg->msg_type == M_MOUSE_DOWN || msg->msg_type == M_MOUSE_DBCLICK)
    {
        reboot();
    }
}

// Handler untuk Shutdown (M_MOUSE_DOWN agar sekali klik langsung jalan)
void shutdownHandler(Widget *widget, message *msg)
{
    if (msg->msg_type == M_MOUSE_DOWN || msg->msg_type == M_MOUSE_DBCLICK)
    {
        halt();
    }
}

int main(int argc, char *argv[])
{
    int caller = (int)argv[1];

    startWindow.width = 3 * START_ICON_WIDTH;
    startWindow.height = SCREEN_HEIGHT / 2;
    startWindow.initialPosition.xmin = 0;
    startWindow.initialPosition.xmax = startWindow.width;
    startWindow.initialPosition.ymin = SCREEN_HEIGHT - DOCK_HEIGHT - startWindow.height;
    startWindow.initialPosition.ymax = SCREEN_HEIGHT - DOCK_HEIGHT;
    startWindow.hasTitleBar = 0;
    createPopupWindow(&startWindow, caller);

    struct RGBA buttonColor;
    struct RGBA textColor;
    struct RGBA dangerColor; // Warna merah untuk shutdown/restart

    // Warna Kuning (Default)
    buttonColor.R = 244;
    buttonColor.G = 180;
    buttonColor.B = 0;
    buttonColor.A = 255;

    // Warna Merah (Shutdown/Restart)
    dangerColor.R = 211;
    dangerColor.G = 47;
    dangerColor.B = 47;
    dangerColor.A = 255;

    // Warna Teks Hitam
    textColor.R = 0;
    textColor.G = 0;
    textColor.B = 0;
    textColor.A = 255;

    // Tambahkan tombol program aplikasi (shell, editor, dll)
    int i;
    for (i = 0; i < 4; i++)
    {
        addButtonWidget(&startWindow, textColor, buttonColor, GUI_programs[i], 20, 20 + 50 * i, 80, 30, 0, startProgramHandler);
    }

    // Tambahkan tombol Restart di bawah daftar aplikasi
    addButtonWidget(&startWindow, textColor, dangerColor, "reboot", 20, 20 + 50 * i, 80, 30, 0, rebootHandler);
    
    // Tambahkan tombol Shutdown di bawah Restart
    i++;
    addButtonWidget(&startWindow, textColor, dangerColor, "halt", 20, 20 + 50 * i, 80, 30, 0, shutdownHandler);

    while (1)
    {
        updatePopupWindow(&startWindow);
    }
}