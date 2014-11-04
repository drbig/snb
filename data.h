#ifndef DATA_H
#define DATA_H

#define LINE_MAX_LEN 4096
#define ERR_MAX_LEN 512

typedef char bool;
#define true 1
#define false 0

typedef struct Entry {
  wchar_t *text;
  int length;
  bool crossed;

  struct Entry *prev;
  struct Entry *next;
  struct Entry *parent;
  struct Entry *child;
} Entry;

typedef struct Result {
  bool success;
  wchar_t msg[ERR_MAX_LEN];
  void *data;
} Result;

typedef enum {BEFORE, AFTER} insert_t;
typedef enum {LEFT, RIGHT} indent_t;
typedef enum {UP, DOWN} move_t;

Result result_new(bool success, void *data, const wchar_t *fmt, ...);
Result entry_new(int length);
Result data_load(FILE *input);
void data_unload(Entry *e);
Result data_dump(Entry *e, FILE *output);
Result entry_insert(Entry *e, insert_t dir, int length);
bool entry_indent(Entry *e, indent_t dir);
bool entry_move(Entry *e, move_t dir);
Result entry_delete(Entry *e);

#ifdef DEBUG
void data_debug_dump(Entry *e, FILE *output);
#endif

#endif
