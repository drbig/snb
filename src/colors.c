/** @file
 * Colours definition and setup function
 */

#define _GNU_SOURCE

#include <ncurses.h>

#include "colors.h"

void colors_init() {
  init_pair(COLOR_KEY    , COLOR_RED,   COLOR_WHITE );
  init_pair(COLOR_OK     , COLOR_BLACK, COLOR_GREEN );
  init_pair(COLOR_INFO   , COLOR_WHITE, COLOR_BLUE  );
  init_pair(COLOR_WARN   , COLOR_BLACK, COLOR_YELLOW);
  init_pair(COLOR_ERROR  , COLOR_BLACK, COLOR_RED   );
  init_pair(COLOR_CURRENT, COLOR_RED,   COLOR_BLACK );
  init_pair(COLOR_CROSSED, COLOR_BLACK, COLOR_BLACK );
}
