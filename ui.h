#ifndef UI_H
#define UI_H

#define SCR_WIDTH 80
#define BULLET_WIDTH 3
#define BULLET_CROSSED L" · "
#define BULLET_SINGLE  L" – "
#define BULLET_OPENED  L" v "
#define BULLET_CLOSED  L" > "
#define TEXT_MORE L"…"

Result ui_set_root(Entry *e);
Result ui_get_root();
void ui_start();
void ui_stop();
void ui_refresh();
void ui_mainloop();

#endif
