// Public IP – echtes C++-API-Style, wird zu .hb transpiliert.
#include <HomeBrew.h>

void main() {
  clear();
  color("ORANGE");
  center(25, "Public IP");
  color("YELLOW");
  center(55, "api.ipify.org");
  color("WHITE");
  http("https://api.ipify.org");
  color("GREEN");
  center(190, "Touch = Zurueck");
  wait_touch();
}
