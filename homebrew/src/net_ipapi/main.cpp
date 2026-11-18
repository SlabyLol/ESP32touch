// Public IP – GUI
#include <HomeBrew.h>

void main() {
  clear();
  bg("BLACK");
  color("ORANGE");
  box(8, 8, 304, 45);
  center(28, "Public IP");

  color("YELLOW");
  label(20, 60, "api.ipify.org");

  color("WHITE");
  http("https://api.ipify.org");

  color("GREEN");
  button(1, 100, 185, 120, 40, "OK");
  show();
}
