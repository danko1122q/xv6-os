#ifndef USER_HANDLER_H
#define USER_HANDLER_H

struct Widget;
struct message;
struct window;
struct RGBA;

// Forward declarations for common handlers
void emptyHandler(struct Widget *w, struct message *msg);
void inputMouseLeftClickHandler(struct Widget *w, struct message *msg);
void inputFieldKeyHandler(struct Widget *w, struct message *msg);

// Utility functions for text positioning
int getInputOffsetFromMousePosition(char *str, int width, int mouse_x, int mouse_y);
int getMouseXFromOffset(char *str, int width, int offset);
int getMouseYFromOffset(char *str, int width, int offset);
int getScrollableTotalHeight(struct window *win);

// Widget creation helpers
int addScrollBarWidget(struct window *window, struct RGBA color, void (*scrollBarHandler)(struct Widget *, struct message *));

#endif // USER_HANDLER_H
