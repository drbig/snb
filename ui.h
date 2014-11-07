#ifndef UI_H
#define UI_H

#define MIN_DLG_SPACE 20

Result ui_set_root(Entry *e);
Result ui_get_root();
void ui_start();
void ui_stop();
void ui_refresh();
void ui_mainloop();

#endif
