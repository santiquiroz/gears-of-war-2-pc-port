#include "gow2_menu_escape_back.h"
#include <cstdio>
int main() {
  using rex::input::gow2_menu_escape::Route;
  const unsigned short start = 0x10, back = 0x2000, a = 0x1000;
  int failures = 0;
  failures += Route(start, true, true, true, true, start, back) != back;
  failures += Route(start | a, true, true, true, true, start, back) != (back | a);
  failures += Route(start, false, true, true, true, start, back) != start;
  failures += Route(start, true, false, true, true, start, back) != start;
  failures += Route(start, true, true, false, true, start, back) != start;
  failures += Route(0, true, true, true, true, start, back) != back;
  failures += Route(back, true, true, true, true, start, back) != back;
  failures += Route(start, true, true, true, false, start, back) != start;
  std::printf("8 menu routing cases, %d failures\n", failures);
  return failures ? 1 : 0;
}
