/** @file
 * User-accessible config variables
 */

#ifndef USER_H
#define USER_H

// snb.c
//#define DEFAULT_FILE    "/path/to/the/file.md"

// data.c
#define LINE_MAX_LEN    4096
#define ERR_MAX_LEN     512

// ui.c
#define SCR_WIDTH       80
#define FORCE_BLACK_BG  false
#define BOLD_ATTRS      A_BOLD

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
#define KEY_OPEN_F      L'o'
#define KEY_RELOAD_F    L'r'
#define KEY_SAVE_F      L's'
#define KEY_SAVEAS_F    L'S'
#define KEY_INSERT_E    L'i'
#define KEY_EDIT_E      L'\n'
#define KEY_QUIT        L'Q'
#define KEY_CROSS_E     L'd'
#define KEY_BOLD_E      L'f'
#define KEY_UNDO_E      L'U'
#define KEY_DELETE_E    L'D'
#define KEY_LEFT_E      L'h'
#define KEY_NEXT_E      L'j'
#define KEY_PREV_E      L'k'
#define KEY_RIGHT_E     L'l'
#define KEY_DEDENT_E    L'H'
#define KEY_MOVEUP_E    L'J'
#define KEY_MOVEDOWN_E  L'K'
#define KEY_INDENT_E    L'L'
#define KEY_NEXT_V      L'n'
#define KEY_PREV_V      L'm'
#define KEY_COLLAPSE    L'c'
#define KEY_EXPAND      L'e'
#define KEY_TOP         L'g'
#define KEY_BOTTOM      L'G'

#define DLG_YESNO       L" y/n "
#define DLG_INFO        L" INFO "
#define DLG_ERROR       L" ERROR "
#define DLG_RELOAD      L" RELOAD "
#define DLG_OPEN        L" OPEN "
#define DLG_SAVE        L" SAVE "
#define DLG_SAVEAS      L" SAVE AS "
#define DLG_QUIT        L" QUIT "

#define DLG_MSG_SAVE    L"Overwrite %s?"
#define DLG_MSG_RELOAD  L"Reload %s?"
#define DLG_MSG_SURE    L"Sure to abandon current data?"
#define DLG_MSG_SAVEAS  L"Sure to save as?"
#define DLG_MSG_EXISTS  L"Overwrite existing file?"
#define DLG_MSG_INVALID L"Invalid directory, retry?"
#define DLG_MSG_ERROR   L"File access error, retry?"
#define DLG_MSG_QUIT    L"Sure to quit?"

#define DLG_ERR_RELOAD  L"There is no file to reload."
#define DLG_ERR_SAVE    L"There is no file to save."

#endif
