#define _GNU_SOURCE

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <string.h>
#include <wchar.h>

#include "user.h"
#include "data.h"

Result result_new(bool success, void *data, const wchar_t *fmt, ...) {
  va_list args;
  Result ret;

  ret.success = success;
  ret.data = data;

  va_start(args, fmt);
  vswprintf(ret.msg, ERR_MAX_LEN, fmt, args);
  va_end(args);

  return ret;
}

Result entry_new(int length) {
  Entry *new;
  
  if (!(new = malloc(sizeof(Entry))))
    return result_new(false, NULL, L"Couldn't allocate Entry");
  bzero(new, sizeof(Entry));

  new->text = calloc(length + 1, sizeof(wchar_t));
  if (!new->text) {
    free(new);
    return result_new(false, NULL, L"Couldn't allocate Entry text buffer");
  }
  bzero(new->text, (length + 1) * sizeof(wchar_t));
  new->length = length;
  new->size = length + 1;

  return result_new(true, new, L"Allocated new Entry with %d text buffer", length);
}

Result data_load(FILE *input) {
  Result ret, res;
  Entry *new, *r, *c;
  wchar_t *line;
  int line_nr, current_level;
  
  if (!(line = calloc(LINE_MAX_LEN, sizeof(wchar_t))))
    return result_new(false, NULL, L"Couldn't allocate line buffer");
  new = r = c = NULL;
  current_level = 0;
  line_nr = 1;

  while (fgetws(line, LINE_MAX_LEN, input)) {
    int length = wcslen(line);
    if (length <= 1) continue;  // ignore empty lines
    line[length-1] = L'\0';     // kill newline char
    --length;

    int level = 0;
    while ((line[level] == L'\t') && (level < length))
      level++;
    if (level == length) {
      ret = result_new(false, NULL, L"Malformed input at line %d", line_nr);
      goto error;
    }

    res = entry_new(length);
    if (!res.success) {
      ret = res;
      goto error;
    }

    if (!c)
      r = c = (Entry *)res.data;
    else {
      new = (Entry *)res.data;

      if (current_level < level) {
        if (level - current_level != 1) {
          ret = result_new(false, NULL, L"Ambigous indentation at line %d", line_nr);
          goto error;
        }
        new->parent = c;
        c->child = new;
        current_level = level;
      } else {
        while (c->parent && (current_level != level)) {
          c = c->parent;
          --current_level;
        }
        if (current_level != level) {
          ret = result_new(false, NULL, L"Couldn't find parent at line %d", line_nr);
          goto error;
        }
        new->parent = c->parent;
        new->prev = c;
        c->next = new;
      }
      c = new;
    }

    if (line[level] == L'~')
      c->crossed = true;

    c->length = length - level - c->crossed;
    wcscpy(c->text, line+level+c->crossed);

    ++line_nr;
  }
  free(line);

  if (errno)
    ret = result_new(false, NULL, L"File access error at line %d", line_nr);
  else
    return result_new(true, r, L"Parsed %d lines", line_nr);

error:
  free(line);
  if (new) free(new);
  if (r) data_unload(r);
  return ret;
}

void data_unload(Entry *e) {
  if (e->child)
    data_unload(e->child);
  if (e->next)
    data_unload(e->next);

  free(e->text);
  free(e);
}

Result data_dump(Entry *e, FILE *output) {
  bool run;
  int level, line_nr, t;

  run = true;
  level = line_nr = 0;

  while (run) {
    ++line_nr;

    for (t = level; t > 0; --t)
      if (fwprintf(output, L"\t") != 1) goto error;
    if (e->crossed)
      if (fwprintf(output, L"~") != 1) goto error;
    if (fwprintf(output, L"%S\n", e->text) != (e->length + 1)) goto error;

    if (e->child) {
      e = e->child;
      ++level;
    } else if (e->next)
      e = e->next;
    else {
      run = false;
      while (e->parent) {
        e = e->parent;
        --level;
        if (e->next) {
          e = e->next;
          run = true;
          break;
        }
      }
    }
  }

  return result_new(true, NULL, L"Written %d lines", line_nr);

error:
  return result_new(false, NULL, L"Error occured. May have written %d lines", line_nr);
}

Result entry_insert(Entry *e, insert_t dir, int length) {
  Result res;
  Entry *new;

  res = entry_new(length);
  if (!res.success)
    return res;

  new = (Entry *)res.data;
  new->parent = e->parent;
  switch (dir) {
    case BEFORE:
      new->next = e;
      new->prev = e->prev;
      if (e->prev)
        e->prev->next = new;
      e->prev = new;
      if (e->parent && (e->parent->child == e))
        e->parent->child = new;
      break;
    case AFTER:
      new->prev = e;
      new->next = e->next;
      if (e->next)
        e->next->prev = new;
      e->next = new;
      break;
  }

  return result_new(true, new, L"Inserted new Entry");
}

bool entry_indent(Entry *e, indent_t dir) {
  Entry *t;

  switch (dir) {
    case LEFT:
      if (!e->parent)
        return false;

      if (e->parent->child == e)
        e->parent->child = e->next;

      if (e->next)
        e->next->prev = e->prev;
      if (e->prev)
        e->prev->next = e->next;

      e->prev = e->parent;
      e->next = e->parent->next;
      e->parent->next = e;
      if (e->next)
        e->next->prev = e;
      e->parent = e->parent->parent;
      break;
    case RIGHT:
      if (!e->prev)
        return false;

      e->parent = e->prev;
      e->prev->next = e->next;
      if (e->next)
        e->next->prev = e->prev;

      if (e->prev->child) {
        t = e->prev->child;
        while (t->next)
          t = t->next;
        t->next = e;
        e->prev = t;
      } else {
        e->prev->child = e;
        e->prev = NULL;
      }
      e->next = NULL;
      break;
  }

  return true;
}

bool entry_move(Entry *e, move_t dir) {
  Entry *o;

  switch (dir) {
    case UP:
      if (!e->prev)
        return false;

      if (e->parent && (e->parent->child == e->prev))
        e->parent->child = e;

      o = e->prev;
      if (o->prev) {
        o->prev->next = e;
        e->prev = o->prev;
      } else
        e->prev = NULL;

      o->next = e->next;
      if (e->next)
        e->next->prev = o;
      o->prev = e;
      e->next = o;
      break;
    case DOWN:
      if (!e->next)
        return false;

      if (e->parent && (e->parent->child == e))
        e->parent->child = e->next;

      o = e->next;
      if (o->next) {
        o->next->prev = e;
        e->next = o->next;
      } else
        e->next = NULL;

      o->prev = e->prev;
      if (e->prev)
        e->prev->next = o;
      o->next = e;
      e->prev = o;
      break;
  }

  return true;
}

Result entry_delete(Entry *e) {
  Entry *o;

  if (e->child)
    return result_new(false, e, L"Can't delete an entry with children");
  if (!(e->prev || e->next || e->parent))
    return result_new(false, e, L"Can't delete last entry");

  o = NULL;
  if (e->parent && (e->parent->child == e)) {
    e->parent->child = e->next;
    o = e->parent;
  }

  if (e->prev)
    e->prev->next = e->next;
  if (e->next)
    e->next->prev = e->prev;

  if (!o) {
    if (e->prev)
      o = e->prev;
    else
      o = e->next;
  }

  free(e->text);
  free(e);

  if (!o)
    return result_new(false, o, L"PANIC!");
  else
    return result_new(true, o, L"Deleted entry");
}

#ifdef DEBUG
void data_debug_dump(Entry *e, FILE *output) {
  Entry *t;
  int level, c;

  t = e;
  level = 0;
  while (t->parent) {
    t = t->parent;
    ++level;
  }

  for (c = level; c > 0; --c)
    fwprintf(output, L" ");
  if (e->child)
    fwprintf(output, L"+ ");
  else
    fwprintf(output, L"- ");
  fwprintf(output, L"\"%S\"", e->text);
  fwprintf(output, L" (l:%d c:%d s:%d len:%d wlen:%d)\n",
      level, e->crossed, e->size, e->length, wcslen(e->text));
  if (e->child)
    data_debug_dump(e->child, output);
  if (e->next)
    data_debug_dump(e->next, output);
}
#endif
