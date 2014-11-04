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

typedef struct Element {
  Entry *entry;

  int level;
  bool open;

  int lx, ly;
  int width, lines;

  struct Element *prev;
  struct Element *next;
} Element;

typedef enum {BROWSE, EDIT} ui_mode_t;
typedef enum {BACKWARD, FORWARD} search_t;

static WINDOW *scr_main = NULL;
static Element *Root = NULL;
static Element *Current = NULL;
static ui_mode_t mode = BROWSE;
static int scr_width;

void update(bool all);
Result cache_rebuild(Element *s, Element *e);

Element *cache_find(Element *e, Entry *en, search_t dir) {
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

bool browse_do(int type, wchar_t input) {
  Element *new;
  Entry *c;

  new = NULL;
  c = Current->entry;
  switch (type) {
    case OK:
      switch (input) {
        case L'Q':
          return false;
          break;
        case L'h':
          break;
        case L'j':
          if (c->next)
            new = cache_find(Current, c->next, FORWARD);
          else if (c->parent && c->parent->next)
            new = cache_find(Current, c->parent->next, FORWARD);
          if (new) {
            Current = new;
            update(true);
          }
          break;
        case L'k':
          if (c->prev)
            new = cache_find(Current, c->prev, BACKWARD);
          else if (c->parent)
            new = cache_find(Current, c->parent, BACKWARD);
          if (new) {
            Current = new;
            update(true);
          }
          break;
        case L'l':
          if (Current->open)
            new = Current->next;
          else if (c->child) {
            Current->open = true;
            cache_rebuild(Current, Current->next);
            new = Current;
          }
          if (new) {
            Current = new;
            update(true);
          }
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
  if (e->open)
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

void update(bool all) {
  Element *e, *p;
  int y, yy;

  if (!Current)
    return;

  e = Current;
  if (all) {
    wclear(scr_main);

    y = (LINES / 2) - (Current->lines / 2);
    if (y <= 0)
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
            mvwaddnwstr(scr_main, yy, p->lx, p->entry->text+((p->lines-(y-yy))*p->width), p->width);
            yy--;
          }
          mvwaddwstr(scr_main, 0, p->lx-(BULLET_WIDTH/2+1), TEXT_MORE);
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

    wrefresh(scr_main);
  } else {
  }
}

void ui_refresh() {
  update(true);
}

Result element_new(Entry *e) {
  Element *new;

  new = malloc(sizeof(Element));
  if (!new)
    return result_new(false, NULL, L"Couldn't allocate Element");
  bzero(new, sizeof(Element));
  new->entry = e;

  return result_new(true, new, L"Allocated new Element");
}

void cache_clear(Element *s, Element *e) {
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

Result cache_rebuild(Element *s, Element *e) {
  Result res;
  Element *new;
  Entry *nx, *last;
  bool run;
  int level;

  if (s->next)
    cache_clear(s->next, e);
  if (e)
    last = e->entry;

  run = true;
  level = s->level;

  while (run) {
    s->level = level;
    s->lx = level * BULLET_WIDTH;
    s->width = scr_width - (level + 1) * BULLET_WIDTH;
    s->lines = s->entry->length / s->width;
    if (s->entry->length % s->width > 0)
      s->lines++;

    if (s->open) {
      nx = s->entry->child;
      level++;
    } else if (s->entry->next)
      nx = s->entry->next;
    else {
      run = false;
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

  if (Root)
    cache_clear(Root, NULL);

  res = element_new(e);
  if (!res.success)
    return res;

  Root = Current = (Element *)res.data;
  res = cache_rebuild(Root, NULL);
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
      switch (mode) {
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
