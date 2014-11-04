#define _GNU_SOURCE

#include <ncurses.h>

#include "colors.h"

void colors_init() {
  init_pair(COLOR_CURRENT, COLOR_RED, COLOR_BLACK);
  init_pair(COLOR_CROSSED, COLOR_BLACK, COLOR_BLACK);
}
