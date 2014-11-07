#ifndef USER_H
#define USER_H

// data.c
#define LINE_MAX_LEN    4096
#define ERR_MAX_LEN     512

// ui.c
#define SCR_WIDTH       80

#define BULLET_WIDTH    3
#define BULLET_CROSSED  L" · "
#define BULLET_SINGLE   L" – "
#define BULLET_OPENED   L" v "
#define BULLET_CLOSED   L" > "
#define BULLET_PARTIAL  L" … "
#define BULLET_MORE     L" ↓ "
#define BULLET_LESS     L" ↑ "
#define BULLET_ML       L" ⇅ "
#define TEXT_MORE       L"…"

#define KEY_TYPE        OK
#define KEY_YES         L'y'
#define KEY_NO          L'n'

#define DLG_YESNO       L" y/n "
#define DLG_INFO        L" INFO "
#define DLG_ERROR       L" ERROR "
#define DLG_LOAD        L" LOAD "
#define DLG_SAVE        L" SAVE "
#define DLG_SAVEAS      L" SAVE AS "

#define DLG_MSG_SAVE        L"Overwrite %s?"
#define DLG_MSG_LOAD        L"Reload %s?"

#endif
