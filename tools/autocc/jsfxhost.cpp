// Runs the real AutoCC.jsfx through ysfx: argv fx.jsfx preset dynamic; stdin "seconds on|off note velocity"
// prints every CC the plug-in sends as "sample cc value".
#include "ysfx.h"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
static void logr(intptr_t, ysfx_log_level l, const char *m) { fprintf(stderr, "[%s] %s\n", ysfx_log_level_string(l), m); }
struct Ev { long long at; std::vector<uint8_t> msg; };
int main(int argc, char **argv) {
  ysfx_config_t *c = ysfx_config_new(); ysfx_set_log_reporter(c, &logr);
  ysfx_guess_file_roots(c, argv[1]);
  ysfx_t *fx = ysfx_new(c); ysfx_config_free(c);
  if (!ysfx_load_file(fx, argv[1], 0) || !ysfx_compile(fx, 0)) { fprintf(stderr, "load/compile failed\n"); return 2; }
  const int B = 256;
  ysfx_set_sample_rate(fx, 48000); ysfx_set_block_size(fx, B); ysfx_init(fx);
  ysfx_slider_set_value(fx, 0, std::atof(argv[2]), true);    // slider1: preset
  ysfx_slider_set_value(fx, 19, std::atof(argv[3]), true);   // slider20: Dynamic
  std::vector<Ev> evs; std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream is(line); double s; std::string kind; int n, v; if (!(is >> s >> kind >> n >> v)) continue;
    long long at = std::llround(s * 48000);
    evs.push_back({at, {(uint8_t)(kind == "on" ? 0x90 : 0x80), (uint8_t)n, (uint8_t)(kind == "on" ? v : 0)}});
  }
  std::stable_sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) { if (a.at != b.at) return a.at < b.at; return a.msg[0] == 0x80 && b.msg[0] == 0x90; });
  long long end = (evs.empty() ? 0 : evs.back().at) + 48000 * 3;
  float z[B] = {0}, o1[B], o2[B];
  size_t k = 0;
  for (long long pos = 0; pos < end; pos += B) {
    while (k < evs.size() && evs[k].at < pos + B) {
      ysfx_midi_event_t me; me.bus = 0; me.offset = (uint32_t)(evs[k].at - pos); me.size = 3; me.data = evs[k].msg.data(); ysfx_send_midi(fx, &me); ++k;
    }
    const float *ins[2] = {z, z}; float *outs[2] = {o1, o2};
    ysfx_process_float(fx, ins, outs, 2, 2, B);
    ysfx_midi_event_t mo;
    while (ysfx_receive_midi(fx, &mo))
      if (mo.size >= 3 && (mo.data[0] & 0xF0) == 0xB0) printf("%lld %d %d\n", pos + mo.offset, mo.data[1], mo.data[2]);
  }
  ysfx_free(fx);
}
