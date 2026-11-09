// AutoCC's engine, ported from AutoCC.jsfx (@init's envelope functions and
// @block's event handling). Time runs in samples at 48 kHz so the 5 ms ticks
// fall exactly where the JSFX's do.

#include "AutoCC.h"

#include <algorithm>
#include <cmath>

namespace ans::autocc
{

namespace
{
constexpr int CN = 33;                  // curve table resolution
constexpr double SRATE = 48000.0;
constexpr long long STEP = 240;         // max(8, floor(srate * 5 ms))
constexpr double RESTART_GAP = 0.4;     // seconds: closer than this continues the line

double clampv (double v, double a, double b) { return std::max (a, std::min (b, v)); }

struct Curve
{
    double table[CN];
    void make (double e)
    {
        e = std::max (0.08, std::min (8.0, e));
        for (int i = 0; i < CN; ++i) table[i] = std::pow (static_cast<double> (i) / (CN - 1), e);
    }
    double at (double p) const
    {
        p = std::max (0.0, std::min (1.0, p));
        const double x = p * (CN - 1);
        const int i0 = static_cast<int> (x);
        if (i0 >= CN - 1) return table[CN - 1];
        const double f = x - i0;
        return table[i0] * (1 - f) + table[i0 + 1] * f;
    }
};

struct Slot
{
    Lane lane;
    Curve attack, release;
    int stage = 0;              // 0 idle, 1 attack, 2 decay, 3 sustain, 4 release, 5 carry
    double t = 0, val = 0, start = 0, relst = 0, vsc = 1;
    int last = -1;
};
} // namespace

std::array<Lane, 4> preset (Preset p)
{
    auto L = [] (int cc, int fl, int pk, int su, double atk, double dcy, double rel, int vel, double ae, double re) {
        Lane l;
        l.cc = cc; l.floor = fl; l.peak = pk; l.sustain = su;
        l.attackMs = atk; l.decayMs = dcy; l.releaseMs = rel;
        l.velocityPercent = vel; l.attackShape = ae; l.releaseShape = re;
        return l;
    };
    switch (p)
    {
        case Preset::Strings:   // start at 85, swell +31 over 2.2 s, hold it
            return { L (1, 85, 116, 116, 2200, 0, 1000, 10, 0.66, 1.00), L (11, 94, 118, 118, 1800, 0, 1000, 0, 0.54, 1.00),
                     L (7, 94, 94, 94, 0, 0, 0, 0, 1.00, 1.00), L (21, 92, 92, 92, 0, 0, 0, 0, 1.00, 1.00) };
        case Preset::Brass:     // start high, a quick +7 bloom, ease back by half
            return { L (1, 102, 109, 106, 1800, 200, 1000, 10, 0.22, 1.00), L (11, 78, 85, 82, 1800, 400, 1000, 0, 0.23, 1.00),
                     L (7, 94, 94, 94, 0, 0, 0, 0, 1.00, 1.00), L (21, 92, 92, 92, 0, 0, 0, 0, 1.00, 1.00) };
        case Preset::Woodwinds: // start low, swell +17 within a second, hold it
            return { L (1, 46, 63, 63, 800, 0, 1000, 10, 0.66, 1.00), L (11, 69, 88, 84, 1200, 800, 1000, 0, 0.66, 1.00),
                     L (7, 94, 94, 94, 0, 0, 0, 0, 1.00, 1.00), L (21, 92, 92, 92, 0, 0, 0, 0, 1.00, 1.00) };
        case Preset::Default:   // every family together: start at 91, swell +24 over 2.6 s
        default:
            return { L (1, 91, 115, 115, 2600, 0, 1000, 10, 0.54, 1.00), L (11, 85, 104, 104, 2200, 0, 1000, 0, 0.47, 1.00),
                     L (7, 94, 94, 94, 0, 0, 0, 0, 1.00, 1.00), L (21, 92, 92, 92, 0, 0, 0, 0, 1.00, 1.00) };
    }
}

std::vector<Control> render (std::vector<KeyEvent> keys, const Settings& settings)
{
    std::vector<Control> out;
    std::array<Slot, 4> slots;
    const auto lanes = preset (settings.preset);
    for (size_t s = 0; s < slots.size(); ++s)
    {
        slots[s].lane = lanes[s];
        slots[s].attack.make (lanes[s].attackShape);
        slots[s].release.make (lanes[s].releaseShape);
    }

    // key-ups before key-downs at the same moment: a line played legato
    std::stable_sort (keys.begin(), keys.end(), [] (const KeyEvent& a, const KeyEvent& b) {
        if (a.seconds != b.seconds) return a.seconds < b.seconds;
        return ! a.on && b.on;
    });

    long long pos = 0, phase = 0;
    auto dynAt = [&] (long long sample) {
        int d = 0;
        for (const auto& [at, v] : settings.dynamic)
            if (std::llround (at * SRATE) <= sample) d = v;
        return d;
    };
    int dyn = dynAt (0);
    auto dynOf = [&] (size_t s) { return s < 2 ? dyn : 0; };
    auto fl = [&] (size_t s) { return clampv (slots[s].lane.floor + dynOf (s), 0, 127); };
    auto pk = [&] (size_t s) { return clampv (slots[s].lane.peak + dynOf (s), 0, 127); };
    auto su = [&] (size_t s) { return clampv (slots[s].lane.sustain + dynOf (s), 0, 127); };

    auto emit = [&] (long long at, size_t s, double v) {
        const int iv = static_cast<int> (clampv (std::floor (v + 0.5), 0, 127));
        if (iv == slots[s].last) return;
        slots[s].last = iv;
        out.push_back ({ static_cast<double> (at) / SRATE, slots[s].lane.cc, iv });
    };

    auto tick = [&] (double dt, long long at) {
        dyn = dynAt (at);
        for (size_t i = 0; i < slots.size(); ++i)
        {
            auto& sl = slots[i];
            int st = sl.stage;
            const double f = fl (i);
            const double peak = f + (pk (i) - f) * sl.vsc;
            const double sus = f + (su (i) - f) * sl.vsc;
            double t = sl.t + dt;
            double v = sl.val;
            for (bool done = false; ! done;)
            {
                if (st == 1)
                {
                    const double dur = sl.lane.attackMs * 0.001;
                    if (t >= dur) { t -= dur; st = 2; }
                    else { v = sl.start + (peak - sl.start) * sl.attack.at (t / dur); done = true; }
                }
                else if (st == 2)
                {
                    const double dur = sl.lane.decayMs * 0.001;
                    if (t >= dur) { t -= dur; st = 3; }
                    else { v = peak + (sus - peak) * (t / dur); done = true; }
                }
                else if (st == 3) { v = sus; done = true; }
                else if (st == 5) { v = clampv (f + sl.relst, 0, 127); done = true; }
                else if (st == 4)
                {
                    const double dur = sl.lane.releaseMs * 0.001;
                    if (t >= dur) { st = 0; t = 0; }
                    else { v = sl.relst + (f - sl.relst) * sl.release.at (t / dur); done = true; }
                }
                else { v = f; t = 0; done = true; }
            }
            sl.stage = st; sl.t = t; sl.val = v;
            emit (at, i, v);
        }
    };
    auto advance = [&] (long long target) {
        while (pos < target)
        {
            const long long n = std::min (STEP - phase, target - pos);
            pos += n;
            phase += n;
            if (phase >= STEP)
            {
                phase -= STEP;
                tick (static_cast<double> (STEP) / SRATE, pos);
            }
        }
    };
    auto idle = [&] {
        for (const auto& sl : slots) if (sl.stage != 0) return false;
        return true;
    };

    // primed: every lane at its resting level
    for (size_t i = 0; i < slots.size(); ++i)
    {
        slots[i].val = slots[i].start = slots[i].relst = fl (i);
        emit (0, i, fl (i));
    }

    bool held[128] = {};
    int count = 0;
    long long keyUp = -1000000000LL;
    for (const auto& k : keys)
    {
        const long long at = std::max (0LL, std::llround (k.seconds * SRATE));
        advance (at);
        dyn = dynAt (at);
        const int note = std::clamp (k.note, 0, 127);
        if (k.on && k.velocity > 0)
        {
            if (! held[note]) { held[note] = true; ++count; }
            if (count == 1 || idle())
            {
                if (count == 1 && ! idle() && static_cast<double> (at - keyUp) < RESTART_GAP * SRATE)
                {
                    // carry the line on: a lane already falling holds where it is
                    for (size_t i = 0; i < slots.size(); ++i)
                        if (slots[i].stage == 4) { slots[i].relst = slots[i].val - fl (i); slots[i].stage = 5; slots[i].t = 0; }
                }
                else
                {
                    for (size_t i = 0; i < slots.size(); ++i)
                    {
                        auto& sl = slots[i];
                        sl.start = sl.stage == 0 ? fl (i) : sl.val;
                        sl.val = sl.start;
                        sl.stage = 1;
                        sl.t = 0;
                        const double va = sl.lane.velocityPercent * 0.01;
                        sl.vsc = 1 - va + va * (k.velocity / 127.0);
                        emit (at, i, sl.start);
                    }
                }
            }
        }
        else
        {
            if (held[note]) { held[note] = false; count = std::max (0, count - 1); }
            if (count == 0)
            {
                keyUp = at;
                for (auto& sl : slots)
                    if (sl.stage != 0) { sl.relst = sl.val; sl.stage = 4; sl.t = 0; }
            }
        }
    }
    // let the last arc fall back
    while (! idle()) advance (pos + STEP);
    advance (pos + STEP);
    return out;
}

} // namespace ans::autocc
