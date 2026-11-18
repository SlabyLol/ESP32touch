// HTTP Echo – GUI
using System;

public class Program {
  static void Main() {
    HomeBrew.clear();
    HomeBrew.bg("BLACK");
    HomeBrew.color("MAGENTA");
    HomeBrew.box(8, 8, 304, 45);
    HomeBrew.center(28, "HTTP Echo");

    HomeBrew.color("CYAN");
    HomeBrew.label(20, 60, "httpbin.org/get");

    HomeBrew.color("WHITE");
    HomeBrew.http("https://httpbin.org/get");

    HomeBrew.color("GREEN");
    HomeBrew.button(1, 40, 185, 100, 40, "Nochmal");
    HomeBrew.button(2, 180, 185, 100, 40, "Ende");
    HomeBrew.show();
  }
}
