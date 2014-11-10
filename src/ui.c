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

typedef struct ElmOpen {
  Entry *entry;
  bool is;

  struct ElmOpen *prev;
  struct ElmOpen *next;
} ElmOpen;

typedef struct Element {
  Entry *entry;

  int level;

  int lx, ly;
  int width, lines;

  struct ElmOpen *open;

  struct Element *prev;
  struct Element *next;
} Element;

typedef enum {BROWSE, EDIT} ui_mode_t;
typedef enum {BACKWARD, FORWARD} search_t;
typedef enum {ALL, CURRENT} update_t;
typedef enum {C_UP, C_DOWN, C_LEFT, C_RIGHT} cur_move_t;
typedef enum {D_LOAD, D_SAVE} dlg_file_path_t;

static struct Cursor {
  int x, y;
  int index;
  int lx, ex, ey;
} Cursor;

static struct Partial {
  bool is;
  bool more, less;
  int offset, limit;
} Partial;

static WINDOW *scr_main = NULL;
static ElmOpen *ElmOpenRoot = NULL;
static ElmOpen *ElmOpenLast = NULL;
static Element *Root = NULL;
static Element *Current = NULL;
static ui_mode_t Mode = BROWSE;
static int scr_width, scr_x;
static int dlg_offset = 1;
static int dlg_min;

void file_save(char *path);
void file_load(char *path);
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
void cursor_update();
void cursor_home();
void cursor_end();
void cursor_move(cur_move_t dir);
void cursor_fix();
void edit_insert(wchar_t ch);
void edit_remove(int offset);
Result elmopen_new(Entry *e);
void elmopen_set(bool to, Entry *s, Entry *e);
Result elmopen_get(Entry *e);
void elmopen_forget(Entry *e);
void elmopen_clear();
Result vitree_rebuild(Element *s, Element *e);
Element *vitree_find(Element *e, Entry *en, search_t dir);
void vitree_clear(Element *s, Element *e);
bool browse_do(int type, wchar_t input);
bool edit_do(int type, wchar_t input);
Result element_new(Entry *e);
void element_draw(Element *e);
void update(update_t mode);
Result ui_set_root(Entry *e);
Result ui_get_root();
void ui_start();
void ui_stop();
void ui_mainloop();

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

void dlg_delwin(WINDOW *win) {
  delwin(win);
  wredrawln(scr_main, LINES - dlg_offset, LINES - dlg_offset);
  if (dlg_offset > 1)
    dlg_offset--;
  wrefresh(scr_main);
}

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

void dlg_error(wchar_t *msg) {
  dlg_simple(DLG_ERROR, msg, COLOR_ERROR);
}

void dlg_info(wchar_t *msg) {
  dlg_simple(DLG_INFO, msg, COLOR_INFO);
}

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

bool dlg_save() {
  return dlg_file(DLG_SAVE, DLG_MSG_SAVE);
}

bool dlg_reload() {
  return dlg_file(DLG_RELOAD, DLG_MSG_RELOAD);
}

char *dlg_file_path(wchar_t *title, int color, dlg_file_path_t mode) {
  WINDOW *win;
  wchar_t input, *wpath, *empty;
  char *path, *dname;
  int left, type, cursor, len, start;
  size_t size;
  bool run, refresh, ok;

  wpath = empty = NULL;
  path = dname = NULL;

  win = dlg_newwin(title, color);
  left = scr_width - 2;
  if (wcslen(title) < dlg_min)
    left -= wcslen(title);
  waddwstr(win, L" ");

  if (!(path = malloc(PATH_MAX))) {
    dlg_error(L"Can't allocate path");
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
          case 127:
            if (cursor == 0)
              break;
            wmemmove(wpath+cursor-1, wpath+cursor, len - cursor);
            wpath[len-1] = L'\0';
            cursor--;
            len--;
            refresh = true;
            break;
          case L'\n':
            if ((size = wcstombs(path, wpath, PATH_MAX)) == -1) {
              dlg_error(L"Couldn't convert path");
              run = false;
            } else {
              size++;
              dname = malloc(size);
              memcpy(dname, path, size);
              dname = dirname(dname);
              if (access(dname, F_OK) == -1) {
                run = dlg_bool(title, DLG_MSG_INVALID, COLOR_ERROR);
              } else {
                switch (mode) {
                  case D_LOAD:
                    if (access(path, R_OK) == 0) {
                      ok = true;
                      run = false;
                    } else {
                      run = dlg_bool(title, DLG_MSG_ERROR, COLOR_ERROR);
                    }
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
                      } else {
                        run = dlg_bool(title, DLG_MSG_ERROR, COLOR_ERROR);
                      }
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
  free(dname);
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
  if (wpath) free(wpath);
  if (empty) free(empty);
  return NULL;
}

char *dlg_save_as() {
  return dlg_file_path(DLG_SAVEAS, COLOR_WARN, D_SAVE);
}

char *dlg_open() {
  return dlg_file_path(DLG_OPEN, COLOR_WARN, D_LOAD);
}

void dlg_info_version() {
  dlg_simple(DLG_INFO, INFO_STR, COLOR_OK);
}

void dlg_info_file() {
  if (UI_File.loaded)
    dlg_simple(DLG_INFO, (wchar_t *)UI_File.path, COLOR_OK);
  else
    dlg_simple(DLG_INFO, L"No file loaded.", COLOR_OK);
}

void cursor_update() {
  Cursor.lx = Current->lx + BULLET_WIDTH;
  Cursor.ex = Cursor.lx + (Current->entry->length % Current->width);
  Cursor.ey = Current->ly + Current->lines - 1;
}

void cursor_home() {
  Cursor.y = Current->ly;
  Cursor.x = Cursor.lx;
  Cursor.index = 0;
  cursor_fix();
}

void cursor_end() {
  Cursor.y = Cursor.ey;
  Cursor.x = Cursor.ex;
  Cursor.index = Current->entry->length;
  cursor_fix();
}

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

void cursor_recalc() {
  int index = Cursor.index > 0 ? Cursor.index : 1;
  Cursor.y = Current->ly + (index / Current->width);
}

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
    } else {
      y -= Partial.offset;
    }
  }
  wmove(scr_main, y, Cursor.x);
}

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

void elmopen_forget(Entry *e) {
  ElmOpen *t;

  t = ElmOpenRoot;
  while ((t->entry != e) && (t = t->next));
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
        case L'o':
          if (dlg_bool(DLG_OPEN, DLG_MSG_SURE, COLOR_WARN)) {
            if ((path = dlg_open()) != NULL)
              file_load(path);
          }
          break;
        case L'r':
          if (UI_File.loaded) {
            if (dlg_reload())
              file_load(UI_File.path);
          } else
            dlg_error(DLG_ERR_RELOAD);
          break;
        case L's':
          if (UI_File.loaded) {
            if (dlg_save())
              file_save(UI_File.path);
          } else
            dlg_error(DLG_ERR_SAVE);
          break;
        case L'S':
          if (dlg_bool(DLG_SAVE, DLG_MSG_SAVEAS, COLOR_WARN)) {
            if ((path = dlg_save_as()) != NULL)
              file_save(path);
          }
          break;
        case L'i':
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
        case L'\n':
          Mode = EDIT;
          update(CURRENT);
          curs_set(true); 
          cursor_update();
          cursor_end();
          wrefresh(scr_main);
          break;
        case L'Q':
          if (dlg_bool(DLG_QUIT, DLG_MSG_QUIT, COLOR_WARN))
            return false;
          break;
        case L'd':
          Current->entry->crossed = !Current->entry->crossed;
          update(CURRENT);
          break;
        case L'D':
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
            if (Current->next == Current) {
              Root->next = Root->prev = NULL;
            }
            update(ALL);
          } else {
            dlg_error(res.msg);
          }
          break;
        case L'h':
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
        case L'j':
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
        case L'k':
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
        case L'l':
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
        case L'H':
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
        case L'J':
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
        case L'K':
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
        case L'L':
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
        case L'n':
          if (Current->next) {
            Current = Current->next;
            update(ALL);
          }
          break;
        case L'm':
          if (Current->prev) {
            Current = Current->prev;
            update(ALL);
          }
          break;
        case L'c':
          new = Current;
          while (new->level != 0) {
            new = new->prev;
          }
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
        case L'e':
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

bool edit_do(int type, wchar_t input) {
  switch (type) {
    case OK:
      switch (input) {
        case L'\n':
          Mode = BROWSE;
          curs_set(false);
          update(CURRENT);
          break;
        case 127:
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
        case 263:
        case 127:
        case 8:
          edit_remove(-1);
          break;
        case KEY_DC:
          edit_remove(0);
          break;
      }
  }

  return true;
}

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
  if (en->crossed)
    wattroff(scr_main, A_BOLD | COLOR_PAIR(COLOR_CROSSED));
  if (y >= LINES) {
    if (!Partial.is)
      mvwaddwstr(scr_main, LINES - 1, scr_width - 1, TEXT_MORE);
  } else
    waddwstr(scr_main, L"\n");
}

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

Result ui_set_root(Entry *e) {
  Result res;

  elmopen_clear();

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

Result ui_get_root() {
  return result_new(true, Current->entry, L"Ok");
}

void ui_start() {
  initscr();
  start_color();
  cbreak();
  keypad(stdscr, true);
  noecho();
  curs_set(false);

  colors_init();

  if (scr_main)
    delwin(scr_main);

  scr_width = COLS < SCR_WIDTH ? (COLS - 2) : SCR_WIDTH;
  scr_x = ((COLS - scr_width) / 2) - 1;
  if (!(scr_main = newwin(LINES, scr_width, 0, scr_x))) {
    fwprintf(stderr, L"Can't make a new dialog window\n");
    exit(1);
  }
  dlg_min = scr_width - DLG_MIN_SPACE;
  if (dlg_min < 0)
    dlg_min = 0;

  clear();
  refresh();

  update(ALL);
}

void ui_stop() {
  delwin(scr_main);
  endwin();
}

void ui_refresh() {
  update(ALL);
}

void ui_mainloop() {
  bool run;
  int type;
  wchar_t input;

  run = true;
  while (run) {
    type = get_wch((wint_t *)&input);
    if (type == ERR) {
      dlg_error(L"Error reading keyboard?");
    } else {
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
}
