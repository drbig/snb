#define _GNU_SOURCE

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include <string.h>
#include <wchar.h>
#include <ncurses.h>

#include "data.h"
#include "ui.h"
#include "colors.h"

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

static WINDOW *scr_main = NULL;
static ElmOpen *ElmOpenRoot = NULL;
static ElmOpen *ElmOpenLast = NULL;
static Element *Root = NULL;
static Element *Current = NULL;
static ui_mode_t Mode = BROWSE;
static int scr_width;

void update(update_t mode);
Result vitree_rebuild(Element *s, Element *e);
void vitree_clear(Element *s, Element *e);

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


bool browse_do(int type, wchar_t input) {
  Result res;
  Element *new;
  Entry *c, *o, *oo;

  new = NULL;
  o = NULL;
  c = Current->entry;
  switch (type) {
    case OK:
      switch (input) {
        case L'Q':
          return false;
          break;
        case L'd':
          Current->entry->crossed = !Current->entry->crossed;
          update(CURRENT);
          break;
        case L'D':
          res = entry_delete(c);
          if (res.success) {
            if (Current == Root) {
              free(Root);
              Root = Current->next;
              Root->prev = NULL;
              new = Root;
            } else
              new = Current->prev;
            vitree_rebuild(new, Current->next);
            Current = vitree_find(Root, (Entry *)res.data, FORWARD);
            if (Current->next == Current) {
              Root->next = Root->prev = NULL;
            }
            update(ALL);
          } else {
            // tempoarary crutch
          }
          break;
        case L'i':
          res = entry_insert(c, AFTER, 1);
          if (res.success) {
            vitree_rebuild(Current, vitree_find(Current, c->next, FORWARD));
            Current = vitree_find(Current, (Entry *)res.data, FORWARD);
            update(ALL);
          } else {
            // temporary crutch
          }
          break;
        case L'h':
          if (Current->open->is) {
            Current->open->is = false;
            if (c->next)
              o = c->next;
            else if (c->parent)
              o = c->parent->next;
            vitree_rebuild(Current, vitree_find(Current, o, FORWARD));
            new = Current;
          } else if (c->parent)
            new = vitree_find(Current, c->parent, BACKWARD);
          if (new) {
            Current = new;
            update(ALL);
          }
          break;
        case L'j':
          if (c->next)
            new = vitree_find(Current, c->next, FORWARD);
          else if (c->parent && c->parent->next)
            new = vitree_find(Current, c->parent->next, FORWARD);
          if (new) {
            Current = new;
            update(ALL);
          }
          break;
        case L'k':
          if (c->prev)
            new = vitree_find(Current, c->prev, BACKWARD);
          else if (c->parent)
            new = vitree_find(Current, c->parent, BACKWARD);
          if (new) {
            Current = new;
            update(ALL);
          }
          break;
        case L'l':
          if (Current->open->is)
            new = Current->next;
          else if (c->child) {
            Current->open->is = true;
            vitree_rebuild(Current, Current->next);
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
            vitree_rebuild(Root, vitree_find(Root, o, FORWARD));
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
            vitree_rebuild(Root, vitree_find(Current, o, FORWARD));
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
            vitree_rebuild(Root, vitree_find(Current, o, FORWARD));
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
            vitree_rebuild(new, vitree_find(Root, oo, FORWARD));
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
        case L'C':
          new = Current;
          while (new->level != 0) {
            new = new->prev;
          }
          o = new->entry;
          elmopen_set(false, NULL, NULL);
          vitree_rebuild(Root, NULL);
          Current = vitree_find(Root, o, FORWARD);
          update(ALL);
          break;
        case L'O':
          o = Current->entry;
          elmopen_set(true, NULL, NULL);
          vitree_rebuild(Root, NULL);
          Current = vitree_find(Root, o, FORWARD);
          update(ALL);
          break;
      }
      break;
    case KEY_CODE_YES:
      switch (input) {
      }
      break;
  }

  return true;
}

void element_draw(Element *e) {
  Entry *en;
  wchar_t *bullet;
  int x, y, p;

  en = e->entry;
  getyx(scr_main, y, x);
  e->ly = y;

  wattron(scr_main, A_BOLD);
  if (e == Current)
    wattron(scr_main, COLOR_PAIR(COLOR_CURRENT));
  if (e->open->is)
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
  if (e == Current)
    wattroff(scr_main, COLOR_PAIR(COLOR_CURRENT));
  wattroff(scr_main, A_BOLD);

  x = e->lx + BULLET_WIDTH;

  if (en->crossed)
    wattron(scr_main, A_BOLD | COLOR_PAIR(COLOR_CROSSED));
  mvwaddnwstr(scr_main, y, x, en->text, e->width);
  for (p = 2; p <= e->lines; p++) {
    y++;
    if (y >= LINES) break;
    mvwaddnwstr(scr_main, y, x, en->text+((p-1)*e->width), e->width);
  }
  if (en->crossed)
    wattroff(scr_main, A_BOLD | COLOR_PAIR(COLOR_CROSSED));
  if (y >= LINES)
    mvwaddwstr(scr_main, LINES - 1, scr_width - 1, TEXT_MORE);
  else
    waddwstr(scr_main, L"\n");
}

void update(update_t mode) {
  Element *e, *p;
  int y, yy;

  if (!Current)
    return;

  e = Current;
  switch (mode) {
    case ALL:
      wclear(scr_main);

      y = (LINES / 2) - (Current->lines / 2);
      if (y < 0)
        y = 0;
      else {
        if (Current != Root) {
          while (e->prev && (y - e->prev->lines >= 0)) {
            e = e->prev;
            y -= e->lines;
          }
          if ((y - 1 >= 0) && (e->prev)) {
            yy = y--;
            p = e->prev;
            while (yy >= 0) {
              mvwaddnwstr(scr_main, yy, p->lx + BULLET_WIDTH,
                  p->entry->text+((p->lines-(y-yy))*p->width), p->width);
              yy--;
            }
            mvwaddwstr(scr_main, 0, p->lx + (BULLET_WIDTH / 2), TEXT_MORE);
          }
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
  wrefresh(scr_main);
}

void ui_refresh() {
  update(ALL);
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
  }

  ElmOpenRoot = ElmOpenLast = NULL;
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
  int start_x;

  initscr();
  start_color();
  cbreak();
  keypad(stdscr, true);
  noecho();
  curs_set(0);

  colors_init();

  if (scr_main)
    delwin(scr_main);

  scr_width = COLS < SCR_WIDTH ? (COLS - 2) : SCR_WIDTH;
  start_x = ((COLS - scr_width) / 2) - 1;
  scr_main = newwin(LINES, scr_width, 0, start_x);

  clear();
  refresh();

  update(true);
}

void ui_stop() {
  delwin(scr_main);
  endwin();
}

void ui_mainloop() {
  bool run;
  int type;
  wchar_t input;

  run = true;
  while (run) {
    type = get_wch((wint_t *)&input);
    if (type == ERR) {
      exit(7); // temporary crutch
    } else {
      switch (Mode) {
        case BROWSE:
          run = browse_do(type, input);
          break;
        case EDIT:
          //run = edit_do(type, input);
          break;
      }
    }
  }
}
