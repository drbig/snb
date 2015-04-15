/** @file
 * Whole UI code in one place
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <ncurses.h>

#include "user.h"
#include "data.h"
#include "ui.h"
#include "colors.h"
#include "snb.h"

// Element open cache
typedef struct ElmOpen {
  Entry *entry;
  bool is;

  struct ElmOpen *prev;
  struct ElmOpen *next;
} ElmOpen;

// Visual tree element
typedef struct Element {
  Entry *entry;

  int level;

  int lx, ly;
  int width, lines;

  struct ElmOpen *open;

  struct Element *prev;
  struct Element *next;
} Element;

// Enums to make life a bit saner
typedef enum {BROWSE, EDIT} ui_mode_t;
typedef enum {BACKWARD, FORWARD} search_t;
typedef enum {ALL, CURRENT} update_t;
typedef enum {C_UP, C_DOWN, C_LEFT, C_RIGHT} cur_move_t;
typedef enum {D_LOAD, D_SAVE} dlg_file_path_t;

// Cursor position and internal data
static struct Cursor {
  int x, y;
  int index;
  int lx, ex, ey;
} Cursor;

// Partial drawing internal data
static struct Partial {
  bool is;
  bool more, less;
  int offset, limit;
} Partial;

// Undo holds last deleted entry data
static struct Undo {
  wchar_t *text;
  int size;
  bool crossed;
  bool present;
  bool root;

  insert_t dir;
  struct Entry *other;
} Undo;

// UI global variables
static WINDOW *scr_main = NULL;
static ElmOpen *ElmOpenRoot = NULL;
static ElmOpen *ElmOpenLast = NULL;
static Element *Root = NULL;
static Element *Current = NULL;
static ui_mode_t Mode = BROWSE;
static int scr_width, scr_x;
static int dlg_offset = 1;
static int dlg_min;

// File loading and saving
void file_save(char *path);
void file_load(char *path);

// Dialog windows
WINDOW *dlg_newwin(wchar_t *title, int color);
void dlg_delwin(WINDOW *win);
void dlg_simple(wchar_t *title, wchar_t *msg, int color);
void dlg_error(wchar_t *msg);
void dlg_info(wchar_t *msg);
bool dlg_bool(wchar_t *title, wchar_t *msg, int color);
bool dlg_file(wchar_t *title, wchar_t *fmt);
bool dlg_save();
bool dlg_reload();
char *dlg_file_path(wchar_t *title, int color, dlg_file_path_t mode);
char *dlg_save_as();
char *dlg_open();
void dlg_info_version();
void dlg_info_file();

// Cursor for editing mode
void cursor_update();
void cursor_home();
void cursor_end();
void cursor_move(cur_move_t dir);
void cursor_fix();

// Editing helpers
void edit_insert(wchar_t ch);
void edit_remove(int offset);

// Element open cache
Result elmopen_new(Entry *e);
void elmopen_set(bool to, Entry *s, Entry *e);
Result elmopen_get(Entry *e);
void elmopen_forget(Entry *e);
void elmopen_clear();

// Visible elements tree
Result vitree_rebuild(Element *s, Element *e);
Element *vitree_find(Element *e, Entry *en, search_t dir);
void vitree_clear(Element *s, Element *e);

// Main modes key handling
bool browse_do(int type, wchar_t input);
bool edit_do(int type, wchar_t input);

// Element operations
Result element_new(Entry *e);

// Drawing
void element_draw(Element *e);
void update(update_t mode);

/** Save current tree to file
 *
 * This will update UI_File as needed.
 *
 * @param path Absolute path to a file (MBS)
 */
void file_save(char *path) {
  Result res;
  wchar_t *msg;
  FILE *fp;

  if (!(msg = calloc(scr_width, sizeof(wchar_t)))) {
    dlg_error(L"Can't allocate msg");
    return;
  }
  if (!(fp = fopen(path, "w"))) {
    swprintf(msg, scr_width, L"%s", strerror(errno));
    dlg_error(msg);
  } else {
    res = data_dump(Root->entry, fp);
    if (res.success) {
      if (UI_File.path && (UI_File.path != path))
        free(UI_File.path);
      UI_File.path = path;
      UI_File.loaded = true;
    } else {
      swprintf(msg, scr_width, L"%S", res.msg);
      dlg_error(msg);
    }
    fclose(fp);
  }
  free(msg);
}

/** Load tree from file
 *
 * This will also discard current tree and update UI_File as needed.
 *
 * @param path Absolute path to a file (MBS)
 */
void file_load(char *path) {
  Result res;
  wchar_t *msg;
  FILE *fp;

  if (!(msg = calloc(scr_width, sizeof(wchar_t)))) {
    dlg_error(L"Can't allocate msg");
    return;
  }
  if (!(fp = fopen(path, "r"))) {
    swprintf(msg, scr_width, L"%s", strerror(errno));
    dlg_error(msg);
  } else {
    res = data_load(fp);
    if (res.success) {
      res = ui_set_root((Entry *)res.data);
      if (res.success) {
        if (UI_File.path && (UI_File.path != path))
          free(UI_File.path);
        UI_File.path = path;
        UI_File.loaded = true;
      } else {
        swprintf(msg, scr_width, L"%S", res.msg);
        dlg_error(msg);
      }
      ui_refresh();
    } else {
      swprintf(msg, scr_width, L"%S", res.msg);
      dlg_error(msg);
    }
    fclose(fp);
  }
  free(msg);
}

/** Setup new dialog window
 */
WINDOW *dlg_newwin(wchar_t *title, int color) {
  WINDOW *win;

  if (!(win = newwin(1, scr_width, LINES - dlg_offset, scr_x))) {
    fwprintf(stderr, L"Can't make a new dialog window\n");
    exit(1);
  }
  if (dlg_offset < LINES)
    dlg_offset++;
  wclear(win);
  wbkgd(win, COLOR_PAIR(color));
  if (wcslen(title) < dlg_min) {
    wattron(win, A_BOLD | A_REVERSE);
    waddwstr(win, title);
    wattroff(win, A_BOLD | A_REVERSE);
  }

  return win;
}

/** Clean up after a dialog window
 *
 * This will also refresh the main screen.
 */
void dlg_delwin(WINDOW *win) {
  delwin(win);
  wredrawln(scr_main, LINES - dlg_offset, LINES - dlg_offset);
  if (dlg_offset > 1)
    dlg_offset--;
  wrefresh(scr_main);
}

/** Show a simple dialog
 *
 * Simple dialog shows a single static string and can be
 * dismissed with any key.
 */
void dlg_simple(wchar_t *title, wchar_t *msg, int color) {
  WINDOW *win;
  int left;

  win = dlg_newwin(title, color);
  left = scr_width - 2;
  if (wcslen(title) < dlg_min)
    left -= wcslen(title);
  waddwstr(win, L" ");
  waddnwstr(win, msg, left);
  if (wcslen(msg) > left)
    mvwaddwstr(win, 0, scr_width - 2, TEXT_MORE);

  wrefresh(win);
  getch();
  dlg_delwin(win);
}

/** Show an error dialog
 */
void dlg_error(wchar_t *msg) {
  dlg_simple(DLG_ERROR, msg, COLOR_ERROR);
}

/** Show an info dialog
 */
void dlg_info(wchar_t *msg) {
  dlg_simple(DLG_INFO, msg, COLOR_INFO);
}

/** Show a yes/no dialog
 */
bool dlg_bool(wchar_t *title, wchar_t *msg, int color) {
  WINDOW *win;
  bool answer;
  wchar_t input;
  int left, type;

  win = dlg_newwin(title, color);
  left = scr_width - wcslen(DLG_YESNO) - 3;
  if (wcslen(title) < dlg_min)
    left -= wcslen(title);
  waddwstr(win, L" ");
  waddnwstr(win, msg, left);
  if (wcslen(msg) > left)
    mvwaddwstr(win, 0, scr_width - wcslen(DLG_YESNO) - 2, TEXT_MORE);
  wattron(win, COLOR_PAIR(COLOR_KEY));
  mvwaddwstr(win, 0, scr_width - wcslen(DLG_YESNO), DLG_YESNO);
  wattroff(win, COLOR_PAIR(COLOR_KEY));

  wrefresh(win);
  while (true) {
    type = get_wch((wint_t *)&input);
    if (type == KEY_TYPE) {
      if (input == KEY_YES) {
        answer = true;
        break;
      } else if (input == KEY_NO) {
        answer = false;
        break;
      }
    }
  }
  dlg_delwin(win);

  return answer;
}

/** Show a dialog related to currently loaded file
 *
 * fmt is expected to contain a single "%s" that will be replaced
 * with the basename of the currently loaded file.
 */
bool dlg_file(wchar_t *title, wchar_t *fmt) {
  wchar_t *msg;
  char *fname;
  bool answer;

  msg = calloc(scr_width, sizeof(wchar_t));
  fname = basename(UI_File.path);
  swprintf(msg, scr_width, fmt, fname);
  answer = dlg_bool(title, msg, COLOR_WARN);
  free(msg);

  return answer;
}

/** Show save current file dialog
 */
bool dlg_save() {
  return dlg_file(DLG_SAVE, DLG_MSG_SAVE);
}

/** Show reload current file dialog
 */
bool dlg_reload() {
  return dlg_file(DLG_RELOAD, DLG_MSG_RELOAD);
}

/** Show a dialog asking for a valid file path
 *
 * This has two modes depending on dlg_file_path_t, one for saving
 * a file and one for loading a file, where the only difference is how
 * file access errors are handled. E.g. it's ok for a path to point to
 * a non-existent file if we're saving, but it's not ok if we are loading.
 *
 * The user can bail out if he gives a bad path.
 *
 * @return Selected file path (MBS)
 */
char *dlg_file_path(wchar_t *title, int color, dlg_file_path_t mode) {
  WINDOW *win;
  wchar_t input, *wpath, *empty;
  char *path, *tpath, *dname;
  int left, type, cursor, len, start;
  bool run, refresh, ok;

  wpath = empty = NULL;
  path = tpath = dname = NULL;

  win = dlg_newwin(title, color);
  left = scr_width - 2;
  if (wcslen(title) < dlg_min)
    left -= wcslen(title);
  waddwstr(win, L" ");

  if (!(path = malloc(PATH_MAX))) {
    dlg_error(L"Can't allocate path");
    goto error;
  }

  if (!(tpath = malloc(PATH_MAX))) {
    dlg_error(L"Can't allocate tpath");
    goto error;
  }

  if (!(wpath = calloc(PATH_MAX, sizeof(wchar_t)))) {
    dlg_error(L"Can't allocate wpath");
    goto error;
  }

  if (!(empty = calloc(left + 2, sizeof(wchar_t)))) {
    dlg_error(L"Can't allocate empty");
    goto error;
  }

  for (cursor = 0; cursor <= left; cursor++)
    empty[cursor] = L' ';
  empty[left+1] = L'\0';

  if (UI_File.loaded)
    swprintf(wpath, PATH_MAX, L"%s", UI_File.path);
  else {
    if (!getcwd(path, PATH_MAX))
      sprintf(path, "/home/");
    swprintf(wpath, PATH_MAX, L"%s", path);
  }

  start = scr_width - left - 1;
  cursor = len = wcslen(wpath);
  run = refresh = true;
  ok = false;
  curs_set(true);

  while (run) {
    if (refresh) {
      mvwaddwstr(win, 0, start, empty);
      mvwaddnwstr(win, 0, start, wpath+(cursor-(cursor % left)), left);
      redrawwin(win);
      refresh = false;
    }
    wmove(win, 0, start + (cursor % left));
    wrefresh(win);

    type = get_wch((wint_t *)&input);
    switch (type) {
      case KEY_CODE_YES:
        switch (input) {
          case KEY_LEFT:
            if (cursor % left == 0)
              refresh = true;
            cursor--;
            if (cursor < 0)
              cursor = 0;
            break;
          case KEY_RIGHT:
            cursor++;
            if (cursor % left == 0)
              refresh = true;
            if (cursor > len)
              cursor = len;
            break;
          case KEY_HOME:
            cursor = 0;
            refresh = true;
            break;
          case KEY_END:
            cursor = len;
            refresh = true;
            break;
          case KEY_DC:
            if (cursor == len)
              break;
            wmemmove(wpath+cursor, wpath+cursor+1, len - cursor - 1);
            wpath[len-1] = L'\0';
            len--;
            refresh = true;
            break;
          case 263:
          case 127:
          case 8:
            if (cursor == 0)
              break;
            wmemmove(wpath+cursor-1, wpath+cursor, len - cursor);
            wpath[len-1] = L'\0';
            cursor--;
            len--;
            refresh = true;
            break;
        }
        break;
      case OK:
        switch (input) {
          case 2: // Ctrl-B
            if (cursor % left == 0)
              refresh = true;
            cursor--;
            if (cursor < 0)
              cursor = 0;
            break;
          case 6: // Ctrl-F
            cursor++;
            if (cursor % left == 0)
              refresh = true;
            if (cursor > len)
              cursor = len;
            break;
          case 127:
          case 8:
            if (cursor == 0)
              break;
            wmemmove(wpath+cursor-1, wpath+cursor, len - cursor);
            wpath[len-1] = L'\0';
            cursor--;
            len--;
            refresh = true;
            break;
          case L'\n':
            if (wcstombs(path, wpath, PATH_MAX) == -1) {
              dlg_error(L"Couldn't convert path");
              run = false;
            } else {
              memcpy(tpath, path, PATH_MAX);
              dname = dirname(tpath);
              if (access(dname, F_OK) == -1)
                run = dlg_bool(title, DLG_MSG_INVALID, COLOR_ERROR);
              else {
                switch (mode) {
                  case D_LOAD:
                    if (access(path, R_OK) == 0) {
                      ok = true;
                      run = false;
                    } else
                      run = dlg_bool(title, DLG_MSG_ERROR, COLOR_ERROR);
                    break;
                  case D_SAVE:
                    if (access(path, F_OK) == 0) {
                      run = !dlg_bool(title, DLG_MSG_EXISTS, COLOR_ERROR);
                      if (!run)
                        ok = true;
                    } else {
                      if ((access(path, W_OK) == 0) || (errno == ENOENT)) {
                        ok = true;
                        run = false;
                      } else
                        run = dlg_bool(title, DLG_MSG_ERROR, COLOR_ERROR);
                    }
                    break;
                }
              }
            }
            refresh = true;
            break;
          default:
            if (iswprint(input)) {
              if (len + 1 == PATH_MAX) {
                dlg_error(L"Too long path");
                break;
              }
              wmemmove(wpath+cursor+1, wpath+cursor, len - cursor);
              len++;
              wpath[cursor] = input;
              wpath[len] = L'\0';
              cursor++;
              refresh = true;
            }
            break;
        }
        break;
    }
  }
  free(tpath);
  free(wpath);
  free(empty);
  curs_set(false);
  dlg_delwin(win);

  if (ok)
    return path;
  else {
    free(path);
    return NULL;
  }

error:
  if (path) free(path);
  if (tpath) free(tpath);
  if (wpath) free(wpath);
  if (empty) free(empty);
  return NULL;
}

/** Show a save as dialog
 */
char *dlg_save_as() {
  return dlg_file_path(DLG_SAVEAS, COLOR_WARN, D_SAVE);
}

/** Show a open file dialog
 */
char *dlg_open() {
  return dlg_file_path(DLG_OPEN, COLOR_WARN, D_LOAD);
}

/** Show a version and legal stuff dialog
 */
void dlg_info_version() {
  dlg_simple(DLG_INFO, INFO_STR, COLOR_OK);
}

/** Show current file path dialog
 */
void dlg_info_file() {
  wchar_t *wpath;
  int len;

  if (UI_File.loaded) {
    len = strlen(UI_File.path);
    wpath = calloc(len, sizeof(wchar_t));
    mbstowcs(wpath, UI_File.path, len);
    dlg_simple(DLG_INFO, wpath, COLOR_OK);
    free(wpath);
  } else
    dlg_simple(DLG_INFO, L"No file loaded.", COLOR_OK);
}

/** Update cursor internal data
 *
 * This update cursor internal data.
 */
void cursor_update() {
  Cursor.lx = Current->lx + BULLET_WIDTH;
  Cursor.ex = Cursor.lx + (Current->entry->length % Current->width);
  Cursor.ey = Current->ly + Current->lines - 1;
}

/** Move cursor home
 */
void cursor_home() {
  Cursor.y = Current->ly;
  Cursor.x = Cursor.lx;
  Cursor.index = 0;
  cursor_fix();
}

/** Move cursor end
 */
void cursor_end() {
  Cursor.y = Cursor.ey;
  Cursor.x = Cursor.ex;
  Cursor.index = Current->entry->length;
  cursor_fix();
}

/** Advance cursor in given direction
 *
 * This basically implements 'the arrow keys'.
 */
void cursor_move(cur_move_t dir) {
  switch (dir) {
    case C_UP:
      if (Cursor.y > Current->ly) {
        Cursor.y--;
        Cursor.index -= Current->width;
      }
      break;
    case C_DOWN:
      if (Cursor.y < Cursor.ey) {
        Cursor.y++;
        if ((Cursor.y == Cursor.ey) && (Cursor.x > Cursor.ex)) {
          Cursor.x = Cursor.ex;
          Cursor.index = Current->entry->length;
        } else
          Cursor.index += Current->width;
      }
      break;
    case C_LEFT:
      if (Cursor.index > 0) {
        Cursor.x--;
        if (Cursor.x < Cursor.lx) {
          Cursor.x = scr_width - 1;
          if (Cursor.y > Current->ly)
            Cursor.y--;
        }
        Cursor.index--;
      }
      break;
    case C_RIGHT:
      if (Cursor.index < Current->entry->length) {
        Cursor.x++;
        if (Cursor.x == scr_width) {
          if (Cursor.y < Cursor.ey) {
            Cursor.y++;
            Cursor.x = Cursor.lx;
          }
        }
        Cursor.index++;
      }
      break;
  }
  cursor_fix();
}

/** Recalculate cursor index from current position
 */
void cursor_recalc() {
  int index = Cursor.index > 0 ? Cursor.index : 1;
  Cursor.y = Current->ly + (index / Current->width);
}

/** Fix cursor in case we're in partial view
 *
 * This is a bolted-on solution for editing when the current
 * entry doesn't fit on the screen (hence partial).
 *
 * Basically if needed we fix the display offset and the y coord
 * of the cursor.
 */
void cursor_fix() {
  int y;

  y = Cursor.y;
  if (Partial.is) {
    if (Cursor.y < Partial.offset) {
      Partial.offset = Cursor.y;
      Partial.less = true;
      Partial.more = true;
      if (Partial.offset == 0) {
        Partial.less = false;
        Partial.more = true;
      }
      update(CURRENT);
      y = 0;
    } else if (Cursor.y > (Partial.offset + (LINES - 1))) {
      Partial.offset = Cursor.y - (LINES - 1);
      Partial.less = true;
      Partial.more = true;
      if (Partial.offset == Partial.limit) {
        Partial.less = true;
        Partial.more = false;
      }
      update(CURRENT);
      y = LINES - 1;
    } else
      y -= Partial.offset;
  }
  wmove(scr_main, y, Cursor.x);
}

/** Handle character entry
 *
 * This also updates and refreshes the screen.
 */
void edit_insert(wchar_t ch) {
  Entry *e;
  wchar_t *new;

  e = Current->entry;

  if ((e->length + 2) > e->size) {
    e->size += scr_width;
    if (!(new = realloc(e->text, e->size * sizeof(wchar_t)))) {
      dlg_error(L"Couldn't realloc Entry text buffer");
      return;
    }
    e->text = new;
  }
  wmemmove(e->text+Cursor.index+1, e->text+Cursor.index, e->length - Cursor.index);
  e->length++;
  e->text[Cursor.index] = ch;
  e->text[e->length] = L'\0';

  if (Cursor.ex + 1 == scr_width) {
    Current->lines++;
    if (Partial.is) {
      Partial.limit++;
      if (Cursor.x == scr_width)
        Partial.offset++;
    }
    update(ALL);
    cursor_update();
    cursor_recalc();
    cursor_move(C_RIGHT);
    wrefresh(scr_main);
  } else {
    update(CURRENT);
    cursor_update();
    cursor_move(C_RIGHT);
    wrefresh(scr_main);
  }
}

/** Handle character deletion
 *
 * The offset parameter differentiates between the behaviours
 * of delete and backspace keys. Using other values will probably
 * blow the whole thing up.
 *
 * This also updates and refreshes the screen.
 *
 * @param offset `0` for delete, `-1` for backspace
 */
void edit_remove(int offset) {
  Entry *e;

  e = Current->entry;

  if (e->length == 0)
    return;

  if (Cursor.index + offset < 0)
    return;

  if ((offset == 0) && (Cursor.index == e->length))
    return;

  wmemmove(e->text+Cursor.index+offset, e->text+Cursor.index+offset+1,
           e->length - Cursor.index);
  e->length--;
  e->text[e->length] = L'\0';

  if (Cursor.ex == Cursor.lx) {
    Current->lines--;
    if (Partial.is) {
      Partial.limit--;
      if (Partial.offset > 0) {
        Partial.offset--;
        if (Partial.offset == 0)
          Partial.less = false;
      }
    }
    update(ALL);
    cursor_update();
    cursor_recalc();
    if (offset == -1) {
      if (Cursor.index == e->length)
        Cursor.index++;
      cursor_move(C_LEFT);
    } else
      cursor_fix();
    wrefresh(scr_main);
  } else {
    update(CURRENT);
    cursor_update();
    if (offset == -1) {
      if (Cursor.index == e->length)
        Cursor.index++;
      cursor_move(C_LEFT);
    } else
      cursor_fix();
    wrefresh(scr_main);
  }
}

/** Add element open cache item
 *
 * @param e Entry for which to add the cache element
 */
Result elmopen_new(Entry *e) {
  ElmOpen *new;

  new = malloc(sizeof(ElmOpen));
  if (!new)
    return result_new(false, NULL, L"Couldn't allocate ElmOpen");
  new->is = false;
  new->entry = e;
  new->next = NULL;
  new->prev = ElmOpenLast;
  if (new->prev)
    new->prev->next = new;

  ElmOpenLast = new;
  if (!ElmOpenRoot)
    ElmOpenRoot = new;

  return result_new(true, new, L"Allocated new ElmOpen");
}

/** Set element open cache items
 *
 * Use this only on ranges.
 *
 * @param to Value to set to
 * @param s Start entry (can't be NULL)
 * @param e End entry (can be NULL)
 */
void elmopen_set(bool to, Entry *s, Entry *e) {
  bool act;
  ElmOpen *t;

  act = s ? false : true;
  t = ElmOpenRoot;
  do {
    if (e && (t->entry == e)) break;
    if (act && (t->entry->child))
      t->is = to;
    else if (t->entry == s)
      act = true;
  } while ((t = t->next));
}

/** Find an element open cache item for an entry
 */
Result elmopen_get(Entry *e) {
  ElmOpen *t;

  t = ElmOpenRoot;
  while (t) {
    if (t->entry == e) break;
    t = t->next;
  }

  if (!t)
    return elmopen_new(e);

  return result_new(true, t, L"Found ElemOpen in cache");
}

/** Remove an element open cache item for an entry
 */
void elmopen_forget(Entry *e) {
  ElmOpen *t;

  t = ElmOpenRoot;
  while ((t->entry != e) && (t = t->next));
  if (!t)
    return;
  if (t->prev)
    t->prev->next = t->next;
  else
    ElmOpenRoot = t->next;
  if (t->next)
    t->next->prev = t->prev;
  else
    ElmOpenLast = t->prev;
  free(t);
}

/** Clear element open cache
 */
void elmopen_clear() {
  ElmOpen *t, *n;

  if (!ElmOpenRoot)
    return;

  t = ElmOpenRoot;
  while (true) {
    n = t->next;
    free(t);
    if (!n) break;
    t = n;
  }

  ElmOpenRoot = ElmOpenLast = NULL;
}

/** Backup given element, for simple undo
 */
Result undo_set(Entry *e) {
  wchar_t *buffer;
  int size;

  size = sizeof(wchar_t) * (e->length + 1);
  if (Undo.size < size) {
    buffer = realloc(Undo.text, size);
    if (!buffer)
      return result_new(false, NULL, L"Couldn't allocate text buffer for Undo");
    Undo.text = buffer;
    Undo.size = size;
  }
  wcscpy(Undo.text, e->text);
  if (e->next) {
    Undo.other = e->next;
    Undo.dir = BEFORE;
  } else if (e->prev) {
    Undo.other = e->prev;
    Undo.dir = AFTER;
  } else if (e->parent) {
    Undo.other = e->parent;
    Undo.dir = AFTER;
  } else
    Undo.other = NULL;
  Undo.crossed = e->crossed;
  if (e == Root->entry)
    Undo.root = true;
  else
    Undo.root = false;
  Undo.present = true;

  return result_new(true, NULL, L"Backed up entry");
}

/** Restore backed up element
 *
 * Assumes the caller has checked if there is an element to restore.
 */
Result undo_restore() {
  Result res;
  Entry *n;

  if (Undo.other) {
    res = entry_insert(Undo.other, Undo.dir, Undo.size);
    if (!res.success)
      return res;
    n = (Entry *)res.data;
  } else
    n = Root->entry;
  wcscpy(n->text, Undo.text);
  n->length = wcslen(n->text);
  n->crossed = Undo.crossed;
  if (Undo.root) {
    res = element_new(n);
    if (!res.success)
      return res;
    vitree_clear(Root, NULL);
    Root = (Element *)res.data;
  }
  Undo.present = false;

  return result_new(true, n, L"Restored entry");
}

/** Rebuild visual tree
 *
 * Not that this will not remove the starting element.
 * In other words you have to handle Root element on
 * your own and with care.
 *
 * @param s Start element
 * @param e End element
 */
Result vitree_rebuild(Element *s, Element *e) {
  Result res;
  Element *new;
  Entry *nx, *last;
  bool run;
  int level;

  last = NULL;
  run = true;
  level = s->level;

  if (s->next)
    vitree_clear(s->next, e);
  if (e)
    last = e->entry;

  while (run) {
    s->level = level;
    s->lx = level * BULLET_WIDTH;
    s->width = scr_width - (level + 1) * BULLET_WIDTH;
    s->lines = s->entry->length / s->width;
    if (s->entry->length % s->width > 0)
      s->lines++;
    if (s->lines < 1)
      s->lines++;

    if (s->open->is && !s->entry->child)
      s->open->is = false;

    if (s->open->is) {
      nx = s->entry->child;
      level++;
    } else if (s->entry->next)
      nx = s->entry->next;
    else {
      run = false;
      s->next = NULL;
      nx = s->entry;
      while (nx->parent) {
        nx = nx->parent;
        --level;
        if (nx->next) {
          nx = nx->next;
          run = true;
          break;
        }
      }
    }

    if (last && (nx == last)) {
      s->next = e;
      e->prev = s;
      run = false;
    }

    if (!run) break;

    res = element_new(nx);
    if (!res.success)
      return res;

    new = (Element *)res.data;
    new->prev = s;
    s->next = new;
    s = new;
  }

  return result_new(true, s, L"Cache rebuilt");
}

/** Find a visual tree element for an entry
 *
 * As the visual tree is being rebuild on changes the pointer
 * you may currently hold is probably invalid.
 *
 * @param e Start element
 * @param en Entry to find an element for
 * @param dir Direction of search relative to e
 * @return Will return NULL if not found
 */
Element *vitree_find(Element *e, Entry *en, search_t dir) {
  if (!en)
    return NULL;

  switch (dir) {
    case BACKWARD:
      do {
        if (e->entry == en)
          return e;
      } while ((e = e->prev));
      break;
    case FORWARD:
      do {
        if (e->entry == en)
          return e;
      } while ((e = e->next));
      break;
  }

  return NULL;
}

/** Remove visual tree elements
 *
 * This won't do anything if `s == e`.
 *
 * @param s Start element
 * @param e End element
 */
void vitree_clear(Element *s, Element *e) {
  Element *n;

  if (s == e)
    return;

  while (true) {
    n = s->next;
    free(s);

    if (!n) break;
    if (e && (n == e)) break;
    s = n;
  }
}

/** Handle browse mode input
 */
bool browse_do(int type, wchar_t input) {
  Result res, r;
  Element *new;
  Entry *c, *o, *oo;
  char *path;

  new = NULL;
  o = NULL;
  c = Current->entry;
  switch (type) {
    case OK:
      switch (input) {
        case KEY_OPEN_F:
          if (dlg_bool(DLG_OPEN, DLG_MSG_SURE, COLOR_WARN)) {
            if ((path = dlg_open()) != NULL)
              file_load(path);
          }
          break;
        case KEY_RELOAD_F:
          if (UI_File.loaded) {
            if (dlg_reload())
              file_load(UI_File.path);
          } else
            dlg_error(DLG_ERR_RELOAD);
          break;
        case KEY_SAVE_F:
          if (UI_File.loaded) {
            if (dlg_save())
              file_save(UI_File.path);
          } else
            dlg_error(DLG_ERR_SAVE);
          break;
        case KEY_SAVEAS_F:
          if (dlg_bool(DLG_SAVE, DLG_MSG_SAVEAS, COLOR_WARN)) {
            if ((path = dlg_save_as()) != NULL)
              file_save(path);
          }
          break;
        case KEY_INSERT_E:
          res = entry_insert(c, AFTER, scr_width);
          if (res.success) {
            o = (Entry *)res.data;
            o->length = 0;
            r = vitree_rebuild(Current, vitree_find(Current, c->next, FORWARD));
            if (!r.success) {
              dlg_error(r.msg);
              break;
            }
            Current = vitree_find(Current, o, FORWARD);
            update(ALL);
          } else {
            dlg_error(res.msg);
            break;
          }
        case KEY_EDIT_E:
          Mode = EDIT;
          update(CURRENT);
          curs_set(true);
          cursor_update();
          cursor_end();
          wrefresh(scr_main);
          break;
        case KEY_QUIT:
          if (dlg_bool(DLG_QUIT, DLG_MSG_QUIT, COLOR_WARN))
            return false;
          break;
        case KEY_CROSS_E:
          Current->entry->crossed = !Current->entry->crossed;
          update(CURRENT);
          break;
        case KEY_BOLD_E:
          Current->entry->bold = !Current->entry->bold;
          update(CURRENT);
          break;
        case KEY_UNDO_E:
          if (Undo.present) {
            res = undo_restore();
            if (!res.success)
              dlg_error(res.msg);
            else {
              o = (Entry *)res.data;
              res = vitree_rebuild(Root, NULL);
              if (!res.success) {
                dlg_error(res.msg);
                break;
              }
              Current = vitree_find(Root, o, FORWARD);
              update(ALL);
            }
          }
          break;
        case KEY_DELETE_E:
          res = undo_set(Current->entry);
          if (!res.success)
            dlg_error(res.msg);
          res = entry_delete(c);
          if (res.success) {
            elmopen_forget(c);
            if (Current == Root) {
              new = Current->next;
              new->prev = NULL;
              free(Root);
              Root = Current = new;
            } else
              new = Current->prev;
            r = vitree_rebuild(new, Current->next);
            if (!r.success) {
              dlg_error(r.msg);
              break;
            }
            Current = vitree_find(Root, (Entry *)res.data, FORWARD);
            if (Current->next == Current)
              Root->next = Root->prev = NULL;
            update(ALL);
          } else {
            Undo.present = false;
            dlg_error(res.msg);
          }
          break;
        case KEY_LEFT_E:
          if (Current->open->is) {
            Current->open->is = false;
            if (c->next)
              o = c->next;
            else if (c->parent)
              o = c->parent->next;
            r = vitree_rebuild(Current, vitree_find(Current, o, FORWARD));
            if (!r.success) {
              dlg_error(r.msg);
              break;
            }
            new = Current;
          } else if (c->parent)
            new = vitree_find(Current, c->parent, BACKWARD);
          if (new) {
            Current = new;
            update(ALL);
          }
          break;
        case KEY_NEXT_E:
          if (Partial.is && Partial.more) {
            Partial.offset++;
            if (Partial.offset == Partial.limit)
              Partial.more = false;
            Partial.less = true;
            update(CURRENT);
          } else {
            if (c->next)
              new = vitree_find(Current, c->next, FORWARD);
            else if (c->parent && c->parent->next)
              new = vitree_find(Current, c->parent->next, FORWARD);
            if (new) {
              Current = new;
              update(ALL);
            }
          }
          break;
        case KEY_PREV_E:
          if (Partial.is && Partial.less) {
            Partial.offset--;
            if (Partial.offset == 0)
              Partial.less = false;
            Partial.more = true;
            update(CURRENT);
          } else {
            if (c->prev)
              new = vitree_find(Current, c->prev, BACKWARD);
            else if (c->parent)
              new = vitree_find(Current, c->parent, BACKWARD);
            if (new) {
              Current = new;
              update(ALL);
            }
          }
          break;
        case KEY_RIGHT_E:
          if (Current->open->is)
            new = Current->next;
          else if (c->child) {
            Current->open->is = true;
            r = vitree_rebuild(Current, Current->next);
            if (!r.success) {
              dlg_error(r.msg);
              break;
            }
            new = Current;
          }
          if (new) {
            Current = new;
            update(ALL);
          }
          break;
        case KEY_DEDENT_E:
          if (c->parent)
            o = c->parent->next;
          if (o)
            o = o->next;
          if (entry_indent(c, LEFT)) {
            r = vitree_rebuild(Root, vitree_find(Root, o, FORWARD));
            if (!r.success) {
              dlg_error(r.msg);
              break;
            }
            Current = vitree_find(Root, c, FORWARD);
            update(ALL);
          }
          break;
        case KEY_MOVEUP_E:
          o = c->next;
          if (entry_move(c, DOWN)) {
            if (Current == Root) {
              new = Root;
              Root = vitree_find(Root, o, FORWARD);
              Root->prev = NULL;
              free(new);
            }
            if (c->parent && c->parent->next)
              o = c->parent->next;
            else
              o = c->next;
            r = vitree_rebuild(Root, vitree_find(Current, o, FORWARD));
            if (!r.success) {
              dlg_error(r.msg);
              break;
            }
            Current = vitree_find(Root, c, FORWARD);
            update(ALL);
          }
          break;
        case KEY_MOVEDOWN_E:
          o = c->prev;
          if (entry_move(c, UP)) {
            if (o && (o == Root->entry)) {
              free(Root);
              Root = Current;
              Root->prev = NULL;
            }
            if (c->parent && c->parent->next)
              o = c->parent->next;
            else
              o = c->next;
            r = vitree_rebuild(Root, vitree_find(Current, o, FORWARD));
            if (!r.success) {
              dlg_error(r.msg);
              break;
            }
            Current = vitree_find(Root, c, FORWARD);
            update(ALL);
          }
          break;
        case KEY_INDENT_E:
          o = c->prev;
          if (c->parent && c->parent->next)
            oo = c->parent->next;
          else
            oo = c->next;
          if (entry_indent(c, RIGHT)) {
            new = vitree_find(Root, o, FORWARD);
            new->open->is = true;
            r = vitree_rebuild(new, vitree_find(Root, oo, FORWARD));
            if (!r.success) {
              dlg_error(r.msg);
              break;
            }
            Current = vitree_find(Root, c, FORWARD);
            update(ALL);
          }
          break;
        case KEY_NEXT_V:
          if (Current->next) {
            Current = Current->next;
            update(ALL);
          }
          break;
        case KEY_PREV_V:
          if (Current->prev) {
            Current = Current->prev;
            update(ALL);
          }
          break;
        case KEY_COLLAPSE:
          new = Current;
          while (new->level != 0)
            new = new->prev;
          o = new->entry;
          elmopen_set(false, NULL, NULL);
          r = vitree_rebuild(Root, NULL);
          if (!r.success) {
            dlg_error(r.msg);
            break;
          }
          Current = vitree_find(Root, o, FORWARD);
          update(ALL);
          break;
        case KEY_EXPAND:
          o = Current->entry;
          elmopen_set(true, NULL, NULL);
          r = vitree_rebuild(Root, NULL);
          if (!r.success) {
            dlg_error(r.msg);
            break;
          }
          Current = vitree_find(Root, o, FORWARD);
          update(ALL);
          break;
        case KEY_TOP:
          Current = Root;
          update(ALL);
          break;
        case KEY_BOTTOM:
          o = Root->entry;
          while (o->next)
            o = o->next;
          Current = vitree_find(Current, o, FORWARD);
          update(ALL);
          break;
      }
      break;
    case KEY_CODE_YES:
      switch (input) {
        case KEY_RESIZE:
          ui_stop();
          ui_start();
          break;
        case KEY_F(1):
          dlg_info_file();
          break;
        case KEY_F(2):
          dlg_info_version();
          break;
      }
      break;
  }

  return true;
}

/** Handle edit mode input
 */
bool edit_do(int type, wchar_t input) {
  switch (type) {
    case OK:
      switch (input) {
        case L'\n':
          Mode = BROWSE;
          curs_set(false);
          update(CURRENT);
          break;
        case 2: // Ctrl-B
          cursor_move(C_LEFT);
          wrefresh(scr_main);
          break;
        case 6: // Ctrl-F
          cursor_move(C_RIGHT);
          wrefresh(scr_main);
          break;
        case 127: // Ctrl-H, Backspace
        case 8:
          edit_remove(-1);
          break;
        default:
          if (iswprint(input))
            edit_insert(input);
          break;
      }
      break;
    case KEY_CODE_YES:
      switch (input) {
        case KEY_HOME:
          cursor_home();
          wrefresh(scr_main);
          break;
        case KEY_END:
          cursor_end();
          wrefresh(scr_main);
          break;
        case KEY_UP:
          cursor_move(C_UP);
          wrefresh(scr_main);
          break;
        case KEY_DOWN:
          cursor_move(C_DOWN);
          wrefresh(scr_main);
          break;
        case KEY_LEFT:
          cursor_move(C_LEFT);
          wrefresh(scr_main);
          break;
        case KEY_RIGHT:
          cursor_move(C_RIGHT);
          wrefresh(scr_main);
          break;
        case 263: // Yeah, that's probably not the best way...
        case 127: // Ctrl-H, Backspace
        case 8:
          edit_remove(-1);
          break;
        case KEY_DC: // Delete
          edit_remove(0);
          break;
      }
  }

  return true;
}

/** Insert new element
 *
 * This setups a visual tree and element open cache items.
 *
 * @param e Entry to make an element for
 */
Result element_new(Entry *e) {
  Result res;
  Element *new;

  new = malloc(sizeof(Element));
  if (!new)
    return result_new(false, NULL, L"Couldn't allocate Element");
  res = elmopen_get(e);
  if (!res.success) {
    free(new);
    return res;
  }
  bzero(new, sizeof(Element));
  new->entry = e;
  new->open = (ElmOpen *)res.data;

  return result_new(true, new, L"Allocated new Element");
}

/** Draw a single element
 */
void element_draw(Element *e) {
  Entry *en;
  wchar_t *bullet;
  int x, y, p;
  int offset;

  en = e->entry;
  getyx(scr_main, y, x);
  e->ly = y;

  wattron(scr_main, A_BOLD);
  if (e == Current) {
    wattron(scr_main, COLOR_PAIR(COLOR_CURRENT));
    if (Mode == EDIT)
      wattron(scr_main, A_REVERSE);
  }
  if ((e == Current) && Partial.is)
    bullet = BULLET_PARTIAL;
  else if (e->open->is)
    bullet = BULLET_OPENED;
  else {
    if (en->child)
      bullet = BULLET_CLOSED;
    else {
      if (en->crossed)
        bullet = BULLET_CROSSED;
      else
        bullet = BULLET_SINGLE;
    }
  }
  mvwaddwstr(scr_main, e->ly, e->lx, bullet);
  if (Partial.is && (e->ly + 1 < LINES)) {
    if (Partial.more && Partial.less)
      bullet = BULLET_ML;
    else if (Partial.more)
      bullet = BULLET_MORE;
    else
      bullet = BULLET_LESS;
    mvwaddwstr(scr_main, e->ly + 1, e->lx, bullet);
  }
  if (e == Current) {
    wattroff(scr_main, COLOR_PAIR(COLOR_CURRENT));
    if (Mode == EDIT)
      wattroff(scr_main, A_REVERSE);
  }
  wattroff(scr_main, A_BOLD);

  x = e->lx + BULLET_WIDTH;

  if (en->crossed)
    wattron(scr_main, A_BOLD | COLOR_PAIR(COLOR_CROSSED));
  if (en->bold)
    wattron(scr_main, BOLD_ATTRS);
  if ((e == Current) && Partial.is) {
    offset = Partial.offset * e->width;
    p = 2 + Partial.offset;
  } else {
    offset = 0;
    p = 2;
  }
  mvwaddnwstr(scr_main, y, x, en->text+offset, e->width);
  for (; p <= e->lines; p++) {
    y++;
    if (y >= LINES) break;
    mvwaddnwstr(scr_main, y, x, en->text+((p-1)*e->width), e->width);
  }
  if (en->bold)
    wattroff(scr_main, BOLD_ATTRS);
  if (en->crossed)
    wattroff(scr_main, A_BOLD | COLOR_PAIR(COLOR_CROSSED));
  if (y >= LINES) {
    if (!Partial.is)
      mvwaddwstr(scr_main, LINES - 1, scr_width - 1, TEXT_MORE);
  } else
    waddwstr(scr_main, L"\n");
}

/** Update screen
 *
 * This works in either single item update or whole screen update,
 * unless the current item doesn't fit on the screen.
 */
void update(update_t mode) {
  Element *e, *p;
  int y, yy;

  if (!Current)
    return;

  if (Current->lines > LINES) {
    if (!Partial.is) {
      Partial.is = true;
      Partial.less = false;
      Partial.more = true;
      Partial.offset = 0;
      Partial.limit = Current->lines - LINES;
    }

    if (mode == ALL)
      wclear(scr_main);
    wmove(scr_main, 0, Current->lx);
    element_draw(Current);
  } else {
    Partial.is = false;
    y = (LINES / 2) - (Current->lines / 2);

    switch (mode) {
      case ALL:
        wclear(scr_main);

        e = Current;
        if (Current != Root) {
          while (e->prev && (y - e->prev->lines >= 0)) {
            e = e->prev;
            y -= e->lines;
          }
          if ((y - 1 >= 0) && (e->prev)) {
            yy = y - 1;
            p = e->prev;
            while (yy >= 0) {
              mvwaddnwstr(scr_main, yy, p->lx + BULLET_WIDTH,
                          p->entry->text+((p->lines-(y-yy))*p->width), p->width);
              yy--;
            }
            mvwaddwstr(scr_main, 0, p->lx + (BULLET_WIDTH / 2), TEXT_MORE);
          }
        }

        wmove(scr_main, y, 0);
        while (y < LINES) {
          element_draw(e);
          y += e->lines;

          if (e->next)
            e = e->next;
          else
            break;
        }
        break;
      case CURRENT:
        wmove(scr_main, Current->ly, Current->lx);
        element_draw(Current);
        break;
    }
  }

  wrefresh(scr_main);
}

/** Set root item for the UI
 */
Result ui_set_root(Entry *e) {
  Result res;

  elmopen_clear();
  Undo.text = NULL;
  Undo.size = 0;

  if (Root)
    vitree_clear(Root, NULL);

  res = element_new(e);
  if (!res.success)
    return res;

  Root = Current = (Element *)res.data;
  res = vitree_rebuild(Root, NULL);
  if (!res.success)
    return res;

  return result_new(true, Root, L"Set root");
}

/** Get root item of the UI
 */
Result ui_get_root() {
  return result_new(true, Current->entry, L"Ok");
}

/** Start the UI
 */
void ui_start() {
  initscr();
  start_color();
  BG_COLOR = COLOR_BLACK;
  if (use_term_colors && (use_default_colors() == OK))
    BG_COLOR = -1;
  cbreak();
  keypad(stdscr, true);
  noecho();
  curs_set(false);

  colors_init();

  if (scr_main)
    delwin(scr_main);

  if (ui_scr_width)
    scr_width = COLS < ui_scr_width ? (COLS - 2) : ui_scr_width;
  else
    scr_width = COLS;
  scr_x = ((COLS - scr_width) / 2) - 1;
  scr_x = scr_x < 0 ? 0 : scr_x;
  if (!(scr_main = newwin(LINES, scr_width, 0, scr_x))) {
    endwin();
    fwprintf(stderr, L"Can't make a new main window\n");
    exit(1);
  }
  dlg_min = scr_width - DLG_MIN_SPACE;
  if (dlg_min < 0)
    dlg_min = 0;

  clear();
  refresh();

  update(ALL);
}

/** Stop the UI
 */
void ui_stop() {
  delwin(scr_main);
  endwin();
}

/** Update screen
 *
 * Only needed once in the main binary after the root has been set.
 */
void ui_refresh() {
  update(ALL);
}

/** Main input loop
 *
 * Reads a key and dispatches based on mode.
 */
void ui_mainloop() {
  bool run;
  int type;
  wchar_t input;

  run = true;
  while (run) {
    type = get_wch((wint_t *)&input);
    if (type == ERR)
      dlg_error(L"Error reading keyboard?");
    else {
      switch (Mode) {
        case BROWSE:
          run = browse_do(type, input);
          break;
        case EDIT:
          run = edit_do(type, input);
          break;
      }
    }
  }
  if (Undo.text)
    free(Undo.text);
}
