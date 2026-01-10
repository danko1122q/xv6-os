#include "types.h"
#include "stat.h"
#include "msg.h"
#include "user.h"
#include "fcntl.h"
#include "user_window.h"
#include "fs.h"
#include "gui.h"

void drawColorFillWidget(window *win, Widget *w);
void drawButtonWidget(window *win, Widget *w);
void drawTextWidget(window *win, Widget *w);
void drawInputFieldWidget(window *win, Widget *w);
void drawShapeWidget(window *win, Widget *w);
int freeWidget(window *win, int index);

void debugPrintWidgetList(window *win)
{

    printf(1, "############################\n");
    printf(1, "current Head at %d\n", win->widgetlisthead);
    printf(1, "current Tail at %d\n", win->widgetlisttail);
    printf(1, "current scrollOffset is %d\n", win->scrollOffsetY);
    printf(1, "\n");

    int p;
    for (p = win->widgetlisthead; p != -1; p = win->widgets[p].next)
    {
        printf(1, "current Widget at %d, type: %d, scrollable: %d\n", p, win->widgets[p].type, win->widgets[p].scrollable);
        printf(1, "current Widget position %d, %d, %d, %d\n", win->widgets[p].position.xmin, win->widgets[p].position.ymin, win->widgets[p].position.xmax, win->widgets[p].position.ymax);
        printf(1, "prev Window at %d\n", win->widgets[p].prev);
        printf(1, "next Window at %d\n", win->widgets[p].next);
        //cprintf("current Window width %d\n", windowlist[p].wnd->position.xmax - windowlist[p].wnd->position.xmin);
        printf(1, "\n");
    }
}

int isInRect(int xmin, int ymin, int xmax, int ymax, int x, int y)
{
    return (x >= xmin && x <= xmax && y >= ymin && y <= ymax);
}

void createPopupWindow(window *win, int caller)
{

    int width = win->width;
    int height = win->height;

    win->window_buf = malloc(width * height * 3);
    if (!win->window_buf)
    {
        return;
    }
    memset(win->window_buf, 255, height * width * 3);
    win->widgetlisthead = -1;
    win->widgetlisttail = -1;
    int i;
    for (i = 0; i < MAX_WIDGET_SIZE; ++i)
    {
        win->widgets[i].next = i;
        win->widgets[i].prev = i;
    }
    win->needsRepaint = 1;
    win->hasTitleBar = 0;
    win->scrollOffsetX = 0;
    win->scrollOffsetY = 0;
    GUI_createPopupWindow(win, caller);
}

void closePopupWindow(window *win)
{
    free(win->window_buf);
    for (int p = win->widgetlisthead; p != -1; p = win->widgets[p].next)
    {
        freeWidget(win, p);
    }
    GUI_closePopupWindow(win);
    exit();
}

void createWindow(window *win, const char *title)
{

    int width = win->width;
    int height = win->height;

    win->window_buf = malloc(width * height * 3);
    if (!win->window_buf)
    {
        return;
    }
    memset(win->window_buf, 255, height * width * 3);

    win->keyfocus = -1;
    win->scrollOffsetX = 0;
    win->scrollOffsetY = 0;
    win->widgetlisthead = -1;
    win->widgetlisttail = -1;
    int i;
    for (i = 0; i < MAX_WIDGET_SIZE; ++i)
    {
        win->widgets[i].next = i;
        win->widgets[i].prev = i;
    }
    win->needsRepaint = 1;
    if (win->hasTitleBar != 0)
    {
        win->hasTitleBar = 1;
    }

    GUI_createWindow(win, title);
}

void closeWindow(window *win)
{
    free(win->window_buf);
    for (int p = win->widgetlisthead; p != -1; p = win->widgets[p].next)
    {
        freeWidget(win, p);
    }
    GUI_closeWindow(win);
    exit();
}

void repaintWindow(window *win)
{
    if (win->needsRepaint)
    {
        //memset(win->window_buf, 255, win->height * win->width * 3);
        for (int p = win->widgetlisthead; p != -1; p = win->widgets[p].next)
        {
            //don't draw widget that is invisible
            if ((!win->widgets[p].scrollable && (win->widgets[p].position.xmin > win->width ||
                                                 win->widgets[p].position.xmax < 0 ||
                                                 win->widgets[p].position.ymin > win->height ||
                                                 win->widgets[p].position.ymax < 0)) ||
                (win->widgets[p].scrollable && (win->widgets[p].position.xmin - win->scrollOffsetX > win->width ||
                                                win->widgets[p].position.xmax - win->scrollOffsetX < 0 ||
                                                win->widgets[p].position.ymin - win->scrollOffsetY > win->height ||
                                                win->widgets[p].position.ymax - win->scrollOffsetY < 0)))
            {
                continue;
            }

            switch (win->widgets[p].type)
            {
            case COLORFILL:
                drawColorFillWidget(win, &win->widgets[p]);
                break;
            case BUTTON:
                drawButtonWidget(win, &win->widgets[p]);
                break;
            case TEXT:
                drawTextWidget(win, &win->widgets[p]);
                break;
            case INPUTFIELD:
                drawInputFieldWidget(win, &win->widgets[p]);
                break;
            case SHAPE:
                drawShapeWidget(win, &win->widgets[p]);
                break;

            default:
                break;
            }
        }
    }
}

void updateWindow(window *win)
{
    repaintWindow(win);
    message msg;

    if (GUI_getMessage(win->handler, &msg) == 0)
    {
        win->needsRepaint = 1;

        printf(2, "", msg.msg_type, msg.msg_type);

        if (msg.msg_type == WM_WINDOW_CLOSE)
        {
            closeWindow(win);
        }
        else if (msg.msg_type == WM_WINDOW_MINIMIZE)
        {
            GUI_minimizeWindow(win);
        }
        else if (msg.msg_type == WM_WINDOW_MAXIMIZE)
        {
            GUI_maximizeWindow(win);
        }
        else if (win->keyfocus != -1 && (msg.msg_type == M_KEY_DOWN || msg.msg_type == M_KEY_UP))
        {
            win->widgets[win->keyfocus].handler(&win->widgets[win->keyfocus], &msg);
        }
        else
        {
            int mouse_x = msg.params[0];
            int mouse_y = msg.params[1];

            for (int p = win->widgetlisttail; p != -1; p = win->widgets[p].prev)
            {

                if ((!win->widgets[p].scrollable && isInRect(win->widgets[p].position.xmin, win->widgets[p].position.ymin, win->widgets[p].position.xmax, win->widgets[p].position.ymax, mouse_x, mouse_y)) ||
                    (win->widgets[p].scrollable && isInRect(win->widgets[p].position.xmin - win->scrollOffsetX, win->widgets[p].position.ymin - win->scrollOffsetY, win->widgets[p].position.xmax - win->scrollOffsetX, win->widgets[p].position.ymax - win->scrollOffsetY, mouse_x, mouse_y)))
                {
                    if (!win->widgets[p].scrollable)
                    {
                        win->widgets[p].handler(&win->widgets[p], &msg);
                    }
                    else
                    {
                        message newmsg;
                        newmsg.msg_type = msg.msg_type;
                        newmsg.params[0] = mouse_x + win->scrollOffsetX;
                        newmsg.params[1] = mouse_y + win->scrollOffsetY;
                        win->widgets[p].handler(&win->widgets[p], &newmsg);
                    }

                    if (win->widgets[p].type == INPUTFIELD)
                    {
                        win->keyfocus = p;
                    }

                    break;
                }
            }
        }
    }
    else
    {
        win->needsRepaint = 0;
    }
    return;
}

//TODO: this function remains a update
void updatePopupWindow(window *win)
{
    repaintWindow(win);

    message msg;
    if (GUI_getPopupMessage(&msg) == 0)
    {
        win->needsRepaint = 1;

        //deleting this printing seems to make popup window unable to open other programs
        printf(2, "", msg.msg_type, msg.msg_type);

        if (msg.msg_type == WM_WINDOW_CLOSE)
        {
            closePopupWindow(win);
        }
        else
        {
            if (msg.msg_type == M_KEY_DOWN || msg.msg_type == M_KEY_UP)
            {
                win->widgets[win->keyfocus].handler(&win->widgets[win->keyfocus], &msg);
            }
            else
            {
                int mouse_x = msg.params[0];
                int mouse_y = msg.params[1];
                for (int p = win->widgetlisttail; p != -1; p = win->widgets[p].prev)
                {
                    if (isInRect(win->widgets[p].position.xmin, win->widgets[p].position.ymin, win->widgets[p].position.xmax, win->widgets[p].position.ymax, mouse_x, mouse_y))
                    {
                        win->widgets[p].handler(&win->widgets[p], &msg);

                        if (win->widgets[p].type == INPUTFIELD)
                        {
                            win->keyfocus = p;
                        }
                        break;
                    }
                }
            }
        }
    }
    else
    {
        win->needsRepaint = 0;
    }
    return;
}

void setWidgetSize(Widget *widget, int x, int y, int w, int h)
{
    widget->position.xmin = x;
    widget->position.ymin = y;
    widget->position.xmax = x + w;
    widget->position.ymax = y + h;
}

int findNextAvailable(window *win)
{

    for (int i = 0; i < MAX_WIDGET_SIZE; i++)
    {
        if (win->widgets[i].prev == i && win->widgets[i].next == i)
        {
            return i;
        }
    }
    return -1;
}

int findWidgetId(window *win, Widget *widget)
{

    for (int i = 0; i < MAX_WIDGET_SIZE; i++)
    {
        if (&win->widgets[i] == widget)
        {
            return i;
        }
    }
    return -1;
}

void addToWidgetListTail(window *win, int idx)
{
    win->widgets[idx].prev = win->widgetlisttail;
    win->widgets[idx].next = -1;
    if (win->widgetlisttail != -1)
        win->widgets[win->widgetlisttail].next = idx;
    win->widgetlisttail = idx;
}

void removeFromWidgetList(window *win, int idx)
{
    if (win->widgetlisthead == idx)
        win->widgetlisthead = win->widgets[win->widgetlisttail].next;
    if (win->widgetlisttail == idx)
        win->widgetlisttail = win->widgets[win->widgetlisttail].prev;
    if (win->widgets[idx].prev != -1)
        win->widgets[win->widgets[idx].prev].next = win->widgets[idx].next;
    if (win->widgets[idx].next != -1)
        win->widgets[win->widgets[idx].next].prev = win->widgets[idx].prev;
    win->widgets[idx].prev = idx;
    win->widgets[idx].next = idx;
}

int addWidget(window *win)
{
    int widgetId = findNextAvailable(win);
    if (widgetId == -1)
        return -1;

    if (win->widgetlisthead == -1)
    {
        win->widgetlisthead = widgetId;
    }

    addToWidgetListTail(win, widgetId);
    return widgetId;
}

int freeWidget(window *win, int index)
{
    switch (win->widgets[index].type)
    {
    case COLORFILL:
        free(win->widgets[index].context.colorfill);
        break;
    case BUTTON:
        free(win->widgets[index].context.button);
        break;
    case TEXT:
        free(win->widgets[index].context.text);
        break;
    case INPUTFIELD:
        free(win->widgets[index].context.inputfield);
        break;

    default:
        break;
    }
    return 0;
}

int removeWidget(window *win, int index)
{
    if (win->widgets[index].prev == index && win->widgets[index].next == index)
    {
        return -1;
    }
    freeWidget(win, index);
    removeFromWidgetList(win, index);
    return 0;
}

int setWidgetHandler(window *win, int index, Handler handler)
{
    win->widgets[index].handler = handler;
    return 0;
}

int addRectangleWidget(window *win, RGBA c, RGBA filledColor, int filled, int x, int y, int w, int h, int scrollable, Handler handler)
{

    int widgetId = addWidget(win);
    if (widgetId == -1)
        return -1;
    Shape *s = malloc(sizeof(Shape));
    s->shape = RECTANGLE;
    s->params[0] = x;
    s->params[1] = y;
    s->params[2] = w;
    s->params[3] = h;
    s->filled = filled;
    s->color = c;
    s->filledColor = filledColor;

    Widget *widget = &win->widgets[widgetId];
    widget->context.shape = s;
    widget->type = SHAPE;
    widget->handler = handler;
    widget->scrollable = scrollable;
    setWidgetSize(widget, x, y, w, h);

    return widgetId;
}

int addColorFillWidget(window *win, RGBA c, int x, int y, int w, int h, int scrollable, Handler handler)
{

    int widgetId = addWidget(win);
    if (widgetId == -1)
        return -1;
    ColorFill *b = malloc(sizeof(ColorFill));
    b->buf = malloc(w * h * 3);
    fillRect(b->buf, 0, 0, w, h, w, h, c);

    Widget *widget = &win->widgets[widgetId];
    widget->context.colorfill = b;
    widget->type = COLORFILL;
    widget->handler = handler;
    widget->scrollable = scrollable;
    setWidgetSize(widget, x, y, w, h);

    return widgetId;
}

int addButtonWidget(window *win, RGBA c, RGBA bc, char *text, int x, int y, int w, int h, int scrollable, Handler handler)
{

    int widgetId = addWidget(win);
    if (widgetId == -1)
        return -1;
    Button *b = malloc(sizeof(Button));
    b->bg_color = bc;
    b->color = c;
    strcpy(b->text, text);

    Widget *widget = &win->widgets[widgetId];
    widget->context.button = b;
    widget->type = BUTTON;
    widget->handler = handler;
    widget->scrollable = scrollable;
    setWidgetSize(widget, x, y, w, h);

    return widgetId;
}

int addTextWidget(window *win, RGBA c, char *text, int x, int y, int w, int h, int scrollable, Handler handler)
{

    int widgetId = addWidget(win);
    if (widgetId == -1)
        return -1;
    Text *t = malloc(sizeof(Text));
    t->color = c;
    strcpy(t->text, text);

    Widget *widget = &win->widgets[widgetId];
    widget->context.text = t;
    widget->type = TEXT;
    widget->handler = handler;
    widget->scrollable = scrollable;
    setWidgetSize(widget, x, y, w, h);

    return widgetId;
}

int addInputFieldWidget(window *win, RGBA c, char *text, int x, int y, int w, int h, int scrollable, Handler handler)
{

    int widgetId = addWidget(win);
    if (widgetId == -1)
        return -1;
    InputField *t = malloc(sizeof(InputField));
    t->color = c;
    strcpy(t->text, text);
    t->current_pos = strlen(text);

    Widget *widget = &win->widgets[widgetId];
    widget->context.inputfield = t;
    widget->type = INPUTFIELD;
    widget->handler = handler;
    widget->scrollable = scrollable;
    setWidgetSize(widget, x, y, w, h);

    win->keyfocus = widgetId;

    return widgetId;
}

void drawColorFillWidget(window *win, Widget *w)
{
    int width = w->position.xmax - w->position.xmin;
    int height = w->position.ymax - w->position.ymin;
    int xmin = w->position.xmin, ymin = w->position.ymin;
    if (w->scrollable)
    {
        xmin = w->position.xmin - win->scrollOffsetX;
        ymin = w->position.ymin - win->scrollOffsetY;
    }

    draw24Image(win, w->context.colorfill->buf, xmin, ymin, width, height);
}

void drawButtonWidget(window *win, Widget *w)
{
    RGB black;
    black.R = 0;
    black.G = 0;
    black.B = 0;
    int xmin = w->position.xmin, ymin = w->position.ymin;
    if (w->scrollable)
    {
        xmin = w->position.xmin - win->scrollOffsetX;
        ymin = w->position.ymin - win->scrollOffsetY;
    }
    int width = w->position.xmax - w->position.xmin;
    int height = w->position.ymax - w->position.ymin;
    int textYOffset = (height - CHARACTER_HEIGHT) / 2;
    int textXOffset = 2;
    if (width > strlen(w->context.button->text) * CHARACTER_WIDTH)
    {
        textXOffset = (width - strlen(w->context.button->text) * CHARACTER_WIDTH) / 2;
    }

    drawFillRect(win, w->context.button->bg_color, xmin, ymin, width, height);
    drawRect(win, black, xmin, ymin, width, height);
    drawString(win, w->context.button->text, w->context.button->color, xmin + textXOffset, ymin + textYOffset, width, height);
}

//TODO: shape is not actually used in current version
void drawShapeWidget(window *win, Widget *w)
{
    Shape *widget = w->context.shape;
    int shape = widget->shape;
    switch (shape)
    {
    case RECTANGLE:;
        RGB color;
        color.R = widget->color.R;
        color.G = widget->color.G;
        color.B = widget->color.B;
        drawRect(win, color, widget->params[0], widget->params[1], widget->params[2], widget->params[3]);
        break;

    default:
        break;
    }
}

void drawTextWidget(window *win, Widget *w)
{
    int xmin = w->position.xmin, ymin = w->position.ymin;
    if (w->scrollable)
    {
        xmin = w->position.xmin - win->scrollOffsetX;
        ymin = w->position.ymin - win->scrollOffsetY;
    }
    int width = w->position.xmax - w->position.xmin;
    int height = w->position.ymax - w->position.ymin;

    drawString(win, w->context.text->text, w->context.text->color, xmin, ymin, width, height);
}

void drawInputFieldWidget(window *win, Widget *w)
{
    int xmin = w->position.xmin, ymin = w->position.ymin;
    if (w->scrollable)
    {
        xmin = w->position.xmin - win->scrollOffsetX;
        ymin = w->position.ymin - win->scrollOffsetY;
    }
    int width = w->position.xmax - w->position.xmin;
    int height = w->position.ymax - w->position.ymin;

    drawString(win, w->context.inputfield->text, w->context.inputfield->color, xmin, ymin, width, height);

    //draw the text cursor
    int offset_x = 0;
    int offset_y = 0;
    int iter = 0;
    while (iter < w->context.inputfield->current_pos)
    {
        if (offset_y > height)
            break;
        if (w->context.inputfield->text[iter] != '\n')
        {
            offset_x += CHARACTER_WIDTH;
            if (offset_x > width)
            {
                offset_x = 0;
                offset_y += CHARACTER_HEIGHT;
            }
        }
        else
        {
            offset_x = 0;
            offset_y += CHARACTER_HEIGHT;
        }
        iter++;
    }
    RGBA black;
    black.R = 0;
    black.G = 0;
    black.B = 0;
    black.A = 255;

    if (offset_y < height)
    {
        drawFillRect(win, black, xmin + offset_x, ymin + offset_y + 1, 1, CHARACTER_HEIGHT - 4);
    }
}