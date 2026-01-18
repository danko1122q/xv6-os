#ifndef APP_ICONS_H
#define APP_ICONS_H

#define APP_ICON_SIZE 48
#define APP_ICON_COUNT 4

enum {
    APP_ICON_TERMINAL = 0,
    APP_ICON_EDITOR = 1,
    APP_ICON_EXPLORER = 2,
    APP_ICON_FLOPPYBIRD = 3,
};

extern unsigned int app_icons_data[APP_ICON_COUNT][APP_ICON_SIZE * APP_ICON_SIZE];

#endif
