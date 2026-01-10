#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "memlayout.h"
#include "user_gui.h"
#include "user_window.h"
#include "gui.h"

#define columnPairs 3

window programWindow;
int birdId;
int startGameButtonId;
int columnIds[2 * columnPairs];

int gravity = 1;
int birdVelocity = 0;
int maxVelocity=15;
int columnWidth = 60;
int columnSeparation = 200;
int birdWidth = 9 * CHARACTER_WIDTH;
int birdHeight = 4 * CHARACTER_HEIGHT;

int seed = 0;
int gameOver = 1;

struct RGBA buttonColor, textColor, birdColor, backgroundColor, columnColor;

int collisionDetection(win_rect *rec1, win_rect *rec2)
{
    if (rec1->xmin >= rec2->xmax || rec2->xmin >= rec1->xmax)
    {
        return 0;
    }
    if (rec1->ymin >= rec2->ymax || rec2->ymin >= rec1->ymax)
    {
        return 0;
    }
    return 1;
}

void initGame()
{
    birdVelocity = 0;
    win_rect *bird_position = &programWindow.widgets[birdId].position;
    bird_position->xmin = programWindow.width / 2 - birdWidth / 2;
    bird_position->ymin = programWindow.height / 2 - birdHeight / 2;
    bird_position->xmax = programWindow.width / 2 + birdWidth / 2;
    bird_position->ymax = programWindow.height / 2 + birdHeight / 2;
    for (int i = 0; i < columnPairs; i++)
    {
        int yend = ((1000 * i) % 31) * 4 + 50;
        int ystart = yend + columnSeparation;
        win_rect *up_position = &programWindow.widgets[columnIds[i]].position;
        win_rect *down_position = &programWindow.widgets[columnIds[columnPairs + i]].position;
        up_position->xmin = programWindow.width * 0.9 + i * 200;
        up_position->xmax = up_position->xmin + columnWidth;
        up_position->ymin = 0;
        up_position->ymax = yend;

        down_position->xmin = programWindow.width * 0.9 + i * 200;
        down_position->xmax = up_position->xmin + columnWidth;
        down_position->ymin = ystart;
        down_position->ymax = programWindow.height;
    }
}

void buttonHandler(Widget *w, message *msg)
{
    //int width = w->position.xmax - w->position.xmin;
    int height = w->position.ymax - w->position.ymin;

    if (msg->msg_type == M_MOUSE_DBCLICK)
    {
        gameOver = 0;
        w->position.ymin = 1000;
        w->position.ymin = 1000 + height;

        initGame();
    }
}

void jumpHandler(Widget *w, message *msg)
{
    //int width = w->position.xmax - w->position.xmin;
    //int height = w->position.ymax - w->position.ymin;

    if (msg->msg_type == M_KEY_DOWN)
    {
        if (msg->params[0] == ' ')
        {

            if (birdVelocity > 0)
            {
                birdVelocity = -birdVelocity / 2 - 5;
            }
            else
            {
                birdVelocity -= 10;
            }
        }
    }
}

int main()
{

    programWindow.width = 540;
    programWindow.height = 400;
    programWindow.hasTitleBar = 1;
    createWindow(&programWindow, "demo");

    buttonColor.R = 244;
    buttonColor.G = 180;
    buttonColor.B = 0;
    buttonColor.A = 255;

    backgroundColor.R = 113;
    backgroundColor.G = 197;
    backgroundColor.B = 207;
    backgroundColor.A = 250;

    textColor.R = 0;
    textColor.G = 0;
    textColor.B = 0;
    textColor.A = 255;

    birdColor.R = 212;
    birdColor.G = 68;
    birdColor.B = 55;
    birdColor.A = 255;

    columnColor.R = 113;
    columnColor.G = 191;
    columnColor.B = 46;
    columnColor.A = 255;

    addColorFillWidget(&programWindow, backgroundColor, 0, 0, programWindow.width, programWindow.height, 0, emptyHandler);

    birdId = addTextWidget(&programWindow, birdColor,
                           "    __\n___( o)>\n\\ <_. ) \n `---\'  ", 0,0,0,0, 0, jumpHandler);
    programWindow.keyfocus = birdId;

    for (int i = 0; i < columnPairs; i++)
    {
        columnIds[i] = addButtonWidget(&programWindow, columnColor, columnColor, "", 0,0,0,0, 0, emptyHandler);
        columnIds[columnPairs + i] = addButtonWidget(&programWindow, columnColor, columnColor, "", 0,0,0,0, 0, emptyHandler);
    }
    initGame();

    startGameButtonId = addButtonWidget(&programWindow, textColor, buttonColor, "start", programWindow.width / 2 - 30, programWindow.height / 2 - 20, 60, 40, 0, buttonHandler);

    int startTime = uptime();

    while (1)
    {
        if (!gameOver && uptime() - startTime > 2)
        {
            for (int i = 0; i < columnPairs; i++)
            {
                win_rect *up_position = &programWindow.widgets[columnIds[i]].position;
                win_rect *down_position = &programWindow.widgets[columnIds[columnPairs + i]].position;
                up_position->xmin -= 2;
                up_position->xmax -= 2;

                down_position->xmin -= 2;
                down_position->xmax -= 2;

                if (up_position->xmax <= 0)
                {
                    up_position->xmin = programWindow.width;
                    up_position->xmax = programWindow.width + columnWidth;

                    down_position->xmin = programWindow.width;
                    down_position->xmax = programWindow.width + columnWidth;

                    seed = (739 * seed + 24) % 97;
                    int yend = seed * 2 + 20;
                    int ystart = yend + columnSeparation;
                    if (yend > programWindow.height - columnSeparation)
                        yend = programWindow.height - columnSeparation - 20;
                    if (ystart > programWindow.height)
                        ystart = programWindow.height - 20;
                    up_position->ymax = yend;
                    down_position->ymin = ystart;
                }

                if (collisionDetection(up_position, &programWindow.widgets[birdId].position) ||
                    collisionDetection(down_position, &programWindow.widgets[birdId].position))
                {
                    gameOver = 1;
                }
            }

            birdVelocity += gravity;
            if (birdVelocity > maxVelocity)
                birdVelocity = maxVelocity;
            if (birdVelocity < -maxVelocity)
                birdVelocity = -maxVelocity;
            programWindow.widgets[birdId].position.ymax += birdVelocity;
            programWindow.widgets[birdId].position.ymin += birdVelocity;
            if (programWindow.widgets[birdId].position.ymax < 0 || programWindow.widgets[birdId].position.ymin > programWindow.height)
            {
                gameOver = 1;
            }

            if (gameOver == 1)
            {
                programWindow.widgets[startGameButtonId].position.ymin = programWindow.height / 2 - 20;
                programWindow.widgets[startGameButtonId].position.ymax = programWindow.height / 2 + 20;
            }

            programWindow.needsRepaint = 1;
            startTime = uptime();
        }
        updateWindow(&programWindow);
    }
}