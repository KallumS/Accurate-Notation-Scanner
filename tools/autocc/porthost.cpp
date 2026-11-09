// The app's AutoCC port (Source/Core/AutoCC.*): argv preset dynamic; stdin "seconds on|off note velocity"; prints "sample cc value".
#include "AutoCC.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
using namespace ans::autocc;
int main (int argc, char** argv) {
  Settings s; s.preset = static_cast<Preset> (std::atoi (argv[1]));
  const int dyn = std::atoi (argv[2]); if (dyn != 0) s.dynamic = { { 0.0, dyn } };
  std::vector<KeyEvent> k; std::string line;
  while (std::getline (std::cin, line)) {
    std::istringstream is (line); double t; std::string kind; int n, v;
    if (is >> t >> kind >> n >> v) k.push_back ({ t, n, v, kind == "on" });
  }
  for (const auto& c : render (k, s)) std::printf ("%lld %d %d\n", std::llround (c.seconds * 48000), c.cc, c.value);
}
