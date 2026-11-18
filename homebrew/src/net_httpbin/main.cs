// HTTP Echo – echtes C#-API-Style, wird zu .hb transpiliert.
using System;

public class Program {
  static void Main() {
    HomeBrew.clear();
    HomeBrew.color("MAGENTA");
    HomeBrew.center(25, "HTTP Echo");
    HomeBrew.color("CYAN");
    HomeBrew.center(55, "httpbin.org/get");
    HomeBrew.color("WHITE");
    HomeBrew.http("https://httpbin.org/get");
    HomeBrew.color("GREEN");
    HomeBrew.center(190, "Touch = Zurueck");
    HomeBrew.wait_touch();
  }
}
