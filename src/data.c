/** @file
 * Naïve nested lists data structure implementation
 *
 * This is pretty much stand-alone code for reading, writing and
 * manipulating nested lists, with Markdown compatible IO format.
 *
 * Also implements means of returning error information.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <string.h>
#include <wchar.h>

#include "user.h"
#include "data.h"

/** Format a result
 */
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

/** Create new entry
 *
 * New entry is safely zeroed.
 */
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

/** Parse input
 */
Result data_load(FILE *input) {
  Result ret, res;
  Entry *new, *r, *c;
  wchar_t *line, *data;
  int line_nr, current_level;

  if (!(line = calloc(LINE_MAX_LEN, sizeof(wchar_t))))
    return result_new(false, NULL, L"Couldn't allocate line buffer");
  new = r = c = NULL;
  current_level = 0;
  line_nr = 1;
  errno = 0;

  while (fgetws(line, LINE_MAX_LEN, input)) {
    new = NULL;
    int length = wcslen(line);
    if (length <= 1) continue;  // ignore empty lines
    line[--length] = L'\0';     // kill newline char

    int level = 0;
    while ((line[level] == L'\t') && (level < length))
      level++;
    if (level >= (length - 2)) {
      ret = result_new(false, NULL, L"Malformed input at line %d", line_nr);
      goto error;
    }
    if (!((line[level] == L'-') && (line[level+1] == L' '))) {
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
          ret = result_new(false, NULL, L"Ambiguous indentation at line %d", line_nr);
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

    c->length = length - level - 2;
    data = line+level+2;

    if (c->length >= 4) {
      if ((data[0] == L'~') && (data[1] == L'~') &&
          (data[c->length-1] == L'~') && (data[c->length-2] == L'~')) {
        c->crossed = true;
        data[c->length-2] = L'\0';
        c->length -= 4;
        data += 2;
      }
    }
    if (c->length >= 4) {
      if ((data[0] == L'*') && (data[1] == L'*') &&
          (data[c->length-1] == L'*') && (data[c->length-2] == L'*')) {
        c->bold = true;
        data[c->length-2] = L'\0';
        c->length -= 4;
        data += 2;
      }
    }

    wcscpy(c->text, data);

    ++line_nr;
  }

  if (errno)
    ret = result_new(false, NULL, L"File access error at line %d", line_nr);
  else {
    free(line);
    return result_new(true, r, L"Parsed %d lines", line_nr);
  }

error:
  free(line);
  if (new) free(new);
  if (r) data_unload(r);
  return ret;
}

/** Free a tree recursively
 */
void data_unload(Entry *e) {
  if (e->child)
    data_unload(e->child);
  if (e->next)
    data_unload(e->next);

  free(e->text);
  free(e);
}

/** Output data
 */
Result data_dump(Entry *e, FILE *output) {
  bool run;
  int level, line_nr, t;

  run = true;
  level = line_nr = 0;

  while (run) {
    ++line_nr;

    for (t = level; t > 0; --t)
      if (fwprintf(output, L"\t") != 1) goto error;

    if (fwprintf(output, L"- ") != 2) goto error;

    if (e->crossed)
      if (fwprintf(output, L"~~") != 2) goto error;
    if (e->bold)
      if (fwprintf(output, L"**") != 2) goto error;

    if (fwprintf(output, L"%S", e->text) != e->length) goto error;

    if (e->bold)
      if (fwprintf(output, L"**") != 2) goto error;
    if (e->crossed)
      if (fwprintf(output, L"~~") != 2) goto error;

    if (fwprintf(output, L"\n") != 1) goto error;

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
  return result_new(false, NULL, L"Error occurred. May have written %d lines", line_nr);
}

/** Insert new entry
 */
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

/** Indent entry
 *
 * In other terms change the level of the entry.
 */
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

/** Move an entry
 */
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

/** Delete an entry
 */
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
