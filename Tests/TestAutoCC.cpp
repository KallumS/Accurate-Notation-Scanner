// AutoCC's engine. Each expectation is a number AutoCC's README or preset table
// states (KallumS/AutoCC: README "The CC lanes", "Where the numbers come from";
// AutoCC.jsfx load_preset), not anything this code printed.

#include "Check.h"
#include "../Source/Core/AutoCC.h"

#include <algorithm>
#include <vector>

using namespace ans::autocc;

namespace
{
// The value a lane holds at a moment: the last one sent at or before it.
int valueAt (const std::vector<Control>& c, int cc, double seconds)
{
    int v = -1;
    for (const auto& x : c)
        if (x.cc == cc && x.seconds <= seconds + 1e-9) v = x.value;
    return v;
}
int sent (const std::vector<Control>& c, int cc)
{
    return static_cast<int> (std::count_if (c.begin(), c.end(), [cc] (const Control& x) { return x.cc == cc; }));
}
std::vector<KeyEvent> note (double on, double off, int pitch = 60, int velocity = 127)
{
    return { { on, pitch, velocity, true }, { off, pitch, 0, false } };
}
std::vector<Control> play (Preset p, std::vector<KeyEvent> k, int dynamic = 0)
{
    Settings s;
    s.preset = p;
    if (dynamic != 0) s.dynamic = { { 0.0, dynamic } };
    return render (k, s);
}
} // namespace

TEST ("AutoCC strings: start at 85, swell +31 over 2.2 s, hold it, fall back after the key")
{
    const auto c = play (Preset::Strings, note (0.0, 4.0));
    CHECK_EQ (valueAt (c, 1, 0.0), 85);      // CC1 starts at the dynamic
    CHECK_EQ (valueAt (c, 1, 2.25), 116);    // peak by 2.2 s
    CHECK_EQ (valueAt (c, 1, 3.9), 116);     // held while the key is
    CHECK_EQ (valueAt (c, 11, 0.0), 94);     // CC11 floor
    CHECK_EQ (valueAt (c, 11, 1.85), 118);   // CC11 peak by 1.8 s
    CHECK (valueAt (c, 1, 1.0) > 85 && valueAt (c, 1, 1.0) < 116);   // rising in between
    CHECK_EQ (valueAt (c, 1, 5.1), 85);      // a plain 1 s fall
    CHECK_EQ (valueAt (c, 11, 5.1), 94);
}

TEST ("AutoCC: CC7 and CC21 are levels, held, not gestures")
{
    const auto c = play (Preset::Strings, note (0.0, 3.0));
    CHECK_EQ (valueAt (c, 7, 1.0), 94);
    CHECK_EQ (valueAt (c, 21, 1.0), 92);
    CHECK_EQ (sent (c, 7), 1);               // sent once, never moved
    CHECK_EQ (sent (c, 21), 1);
}

TEST ("AutoCC brass: start high, a quick +7 bloom, ease back")
{
    const auto c = play (Preset::Brass, note (0.0, 4.0));
    CHECK_EQ (valueAt (c, 1, 0.0), 102);
    // the peak, reached at the end of the 1.8 s rise; the 0.2 s settle starts
    // straight after it (this first read the value at 1.85 s, already 50 ms
    // into the settle - corrected 2026-11-09)
    int peak = 0;
    for (const auto& x : c) if (x.cc == 1) peak = std::max (peak, x.value);
    CHECK_EQ (peak, 109);
    CHECK_EQ (valueAt (c, 1, 1.8), 109);
    CHECK_EQ (valueAt (c, 1, 3.0), 106);     // settled back to sustain
    CHECK (valueAt (c, 1, 0.3) >= 106);      // the bloom comes almost at once (shape 0.22)
}

TEST ("AutoCC woodwinds: start low, swell +17 within a second")
{
    const auto c = play (Preset::Woodwinds, note (0.0, 3.0));
    CHECK_EQ (valueAt (c, 1, 0.0), 46);
    CHECK_EQ (valueAt (c, 1, 0.85), 63);
}

TEST ("AutoCC: a note within 0.4 s of the last key-up carries the line on; later, it swells again")
{
    // second note 0.2 s after the first ends: no new swell, the lane holds
    auto close = note (0.0, 3.0);
    for (auto e : note (3.2, 6.0)) close.push_back (e);
    const auto c = play (Preset::Strings, close);
    const int at = valueAt (c, 1, 3.2);
    CHECK (at < 116);                         // it had begun to fall
    CHECK_EQ (valueAt (c, 1, 5.9), at);       // and holds there, no swell
    // one second later: a fresh swell, back up to the peak
    auto apart = note (0.0, 3.0);
    for (auto e : note (4.0, 7.0)) apart.push_back (e);
    const auto d = play (Preset::Strings, apart);
    CHECK_EQ (valueAt (d, 1, 6.9), 116);
}

TEST ("AutoCC: velocity scales CC1's swell by only 10%, and leaves CC11 alone")
{
    const auto quiet = play (Preset::Strings, note (0.0, 4.0, 60, 0 + 1));
    const auto loud = play (Preset::Strings, note (0.0, 4.0, 60, 127));
    // swell 31 at full velocity; at the bottom 90% of it: 85 + 27.9 = 113
    CHECK_EQ (valueAt (loud, 1, 3.0), 116);
    CHECK_EQ (valueAt (quiet, 1, 3.0), 113);
    CHECK_EQ (valueAt (quiet, 11, 3.0), valueAt (loud, 11, 3.0));
}

TEST ("AutoCC: Dynamic moves CC1 and CC11's start, keeping the swell's size; CC7 stays")
{
    const auto c = play (Preset::Strings, note (0.0, 4.0), 10);
    CHECK_EQ (valueAt (c, 1, 0.0), 95);
    CHECK_EQ (valueAt (c, 1, 3.0), 126);
    CHECK_EQ (valueAt (c, 11, 0.0), 104);
    CHECK_EQ (valueAt (c, 7, 0.0), 94);
}
