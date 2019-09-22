#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <check.h>
#include <locale.h>
#include <string.h>
#include <wchar.h>

#include "../src/user.h"
#include "../src/data.h"

FILE *fp, *sink;
Result res;
Entry *root;
struct Entry *tree[16];
bool verbose;

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
  fwprintf(output, L" (l:%d c:%d b:%d s:%d len:%d wlen:%d)\n",
           level, e->crossed, e->bold, e->size, e->length, wcslen(e->text));
  if (e->child)
    data_debug_dump(e->child, output);
  if (e->next)
    data_debug_dump(e->next, output);
}

bool dump_error(Result res) {
  if (!res.success) {
    fwprintf(stderr, L"ERROR: %S.\n", res.msg);
    perror("Unix error");
    return true;
  }

  return false;
}

void dump_root() {
  if (verbose) {
    data_debug_dump(root, stderr);
    fwprintf(stderr, L"\n");
  }
}

START_TEST(test_load_dump) {
  if (!(fp = fopen("./tests/data.txt", "r"))) {
    perror("Can't open test data");
    ck_abort_msg("Can't open test data");
  }

  res = data_load(fp);
  fclose(fp);
  if (dump_error(res))
    ck_abort_msg("Parsing error");

  root = (Entry *)res.data;
  if (verbose)
    data_debug_dump(root, stderr);
  res = data_dump(root, sink);
  if (dump_error(res)) {
    data_unload(root);
    ck_abort_msg("Dumping error");
  }
  data_unload(root);
}
END_TEST

START_TEST(test_build_tree) {
  Entry *e;

  res = entry_new(8);
  if (dump_error(res))
    ck_abort_msg("Couldn't make an entry");
  root = (Entry *)res.data;
  ck_assert_int_eq(root->length, 8);
  swprintf(root->text, 8, L"0 - One");
  tree[0] = root;

  res = entry_insert(root, AFTER, 10);
  if (dump_error(res))
    ck_abort_msg("Couldn't make an entry");
  e = (Entry *)res.data;
  ck_assert_int_eq(e->length, 10);
  swprintf(e->text, 10, L"1 - Two");
  tree[1] = e;

  ck_assert(tree[0]->next == tree[1]);
  ck_assert(tree[1]->prev == tree[0]);

  res = entry_insert(e, AFTER, 16);
  if (dump_error(res))
    ck_abort_msg("Couldn't make an entry");
  e = (Entry *)res.data;
  ck_assert_int_eq(e->length, 16);
  swprintf(e->text, 16, L"2 - Three");
  tree[2] = e;

  ck_assert(tree[1]->next == tree[2]);
  ck_assert(tree[2]->prev == tree[1]);

  res = entry_insert(root, AFTER, 23);
  if (dump_error(res))
    ck_abort_msg("Couldn't make an entry");
  e = (Entry *)res.data;
  ck_assert_int_eq(e->length, 23);
  swprintf(e->text, 23, L"3 - One sub");
  tree[3] = e;

  ck_assert(tree[0]->next == tree[3]);
  ck_assert(tree[3]->prev == tree[0]);
  ck_assert(tree[1]->prev == tree[3]);

  res = entry_insert(e, BEFORE, 32);
  if (dump_error(res))
    ck_abort_msg("Couldn't make an entry");
  e = (Entry *)res.data;
  ck_assert_int_eq(e->length, 32);
  swprintf(e->text, 32, L"4 - One sub before");
  tree[4] = e;

  ck_assert(tree[0]->next == tree[4]);
  ck_assert(tree[4]->prev == tree[0]);
  ck_assert(tree[4]->next == tree[3]);

  //dump_root();

  ck_assert(entry_indent(root->next, RIGHT));
  ck_assert(entry_indent(root->next, RIGHT));

  ck_assert(tree[0]->child == tree[4]);
  ck_assert(tree[4]->next == tree[3]);
  ck_assert(tree[3]->prev == tree[4]);
  ck_assert(tree[3]->next == NULL);
  ck_assert(tree[3]->parent == root);
  ck_assert(tree[4]->parent == root);

  ck_assert(entry_indent(root->child->next, RIGHT));
  ck_assert(tree[3]->parent == tree[4]);
  ck_assert(entry_indent(root->next->next, RIGHT));
  ck_assert(entry_indent(root->next, RIGHT));
  ck_assert(!entry_indent(root, RIGHT));
  ck_assert(entry_indent(tree[1], RIGHT));

  //dump_root();

  ck_assert(entry_indent(tree[4], LEFT));
  ck_assert(entry_indent(tree[2], LEFT));
  ck_assert(entry_indent(tree[2], LEFT));
  ck_assert(!entry_indent(root, LEFT));

  //dump_root();

  ck_assert(entry_move(tree[2], UP));
  ck_assert(entry_move(tree[2], UP));
  root = tree[2];
  ck_assert(!entry_move(tree[2], UP));
  ck_assert(entry_move(tree[1], UP));

  //dump_root();

  ck_assert(entry_move(tree[0], DOWN));
  ck_assert(entry_move(tree[1], DOWN));
  ck_assert(entry_move(tree[4], DOWN));
  ck_assert(entry_move(tree[2], DOWN));
  root = tree[0];
  ck_assert(entry_move(tree[2], DOWN));
  ck_assert(!entry_move(tree[2], DOWN));
  ck_assert(!entry_move(tree[1], DOWN));

  dump_root();

  res = entry_delete(tree[4]);
  ck_assert(!res.success);
  res = entry_delete(tree[3]);
  ck_assert(res.success);
  res = entry_delete(tree[1]);
  ck_assert(res.success);
  res = entry_delete(tree[0]);
  ck_assert(res.success);
  root = tree[4];
  res = entry_delete(tree[4]);
  ck_assert(res.success);
  root = tree[2];
  res = entry_delete(tree[2]);
  ck_assert(!res.success);

  dump_root();

  data_unload(root);
}
END_TEST

Suite *data_suite(void) {
  Suite *s;
  TCase *tc;

  s = suite_create("Data");

  tc = tcase_create("Loading and dumping");
  tcase_add_test(tc, test_load_dump);
  suite_add_tcase(s, tc);

  tc = tcase_create("Manipulating");
  tcase_add_test(tc, test_build_tree);
  suite_add_tcase(s, tc);

  return s;
}

int main(int argc, char *argv[]) {
  Suite *s;
  SRunner *sr;
  char *tmp;
  int nr_failed;

  setlocale(LC_ALL, "");

  verbose = false;
  if ((tmp = getenv("CK_VERBOSITY")) && (tmp[0] == 'v'))
    verbose = true;

  if (!(sink = fopen("/dev/null", "w"))) {
    perror("Can't open /dev/null");
    return EXIT_FAILURE;
  }

  s = data_suite();
  sr = srunner_create(s);

  srunner_run_all(sr, CK_ENV);
  nr_failed = srunner_ntests_failed(sr);
  srunner_free(sr);

  fclose(sink);

  return nr_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
