// NoviTape engine test (no JUCE needed).
// Build: cmake --build build --target EngineTest && ./build/EngineTest
#include "../Source/TapeEngine.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace novitape;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("FAIL: %s\n", msg); ++failures; } } while (0)

static void fillSignal (std::vector<float>& l, std::vector<float>& r, long& t, double sr)
{
    for (size_t i = 0; i < l.size(); ++i, ++t)
    {
        const double x = (double) t / sr;
        l[i] = (float) (0.4 * std::sin (6.2831853 * 220.0 * x) + 0.2 * std::sin (6.2831853 * 331.0 * x));
        r[i] = (float) (0.4 * std::sin (6.2831853 * 277.0 * x));
    }
}

int main()
{
    const double sr = 48000.0;
    const int block = 512;
    TapeEngine e;
    e.prepare (sr);
    EngineInput in;
    in.bpm = 90.0;

    std::vector<float> l (block), r (block), l0 (block), r0 (block);
    long t = 0;

    auto runBlocks = [&] (int blocks, bool& finite, double& peak, bool& identical)
    {
        finite = true; peak = 0; identical = true;
        for (int b = 0; b < blocks; ++b)
        {
            fillSignal (l, r, t, sr);
            l0 = l; r0 = r;
            e.process (l.data(), r.data(), block, in);
            for (int i = 0; i < block; ++i)
            {
                if (! std::isfinite (l[i]) || ! std::isfinite (r[i])) finite = false;
                peak = std::max (peak, (double) std::abs (l[i]));
                if (l[i] != l0[i] || r[i] != r0[i]) identical = false;
            }
        }
    };

    bool finite, identical; double peak;

    // 1. Nothing on: bit-exact pass-through, zero latency.
    runBlocks (50, finite, peak, identical);
    CHECK (finite && identical, "clean pass-through is bit-exact");
    CHECK (e.isLive(), "read head is live at rest");

    // 2. Every effect and variation: finite output, sane level, and back to live after release.
    for (int fx = 0; fx < NumFx; ++fx)
    {
        for (int v = 0; v < 4; ++v)
        {
            in.fx[(size_t) fx].on = true;
            in.fx[(size_t) fx].variation = v;
            in.fx[(size_t) fx].amount = TapeEngine::defaultAmounts[fx];
            runBlocks (120, finite, peak, identical);
            char msg[128];
            std::snprintf (msg, sizeof msg, "%s %s produces finite audio", TapeEngine::effectNames[fx], TapeEngine::variationNames[fx][v]);
            CHECK (finite, msg);
            std::snprintf (msg, sizeof msg, "%s %s level is sane (peak %.3f)", TapeEngine::effectNames[fx], TapeEngine::variationNames[fx][v], peak);
            CHECK (peak < 2.0, msg);
            std::printf ("%-9s %-11s peak %.3f  lag %.3fs\n", TapeEngine::effectNames[fx], TapeEngine::variationNames[fx][v], peak, e.meters.lagSeconds.load());

            in.fx[(size_t) fx].on = false;
            runBlocks (400, finite, peak, identical);   // ~4 s to settle (long brake release, warble fade)
            std::snprintf (msg, sizeof msg, "%s %s returns to live tape", TapeEngine::effectNames[fx], TapeEngine::variationNames[fx][v]);
            CHECK (e.isLive(), msg);
            runBlocks (4, finite, peak, identical);
            std::snprintf (msg, sizeof msg, "%s %s is bit-exact again after release", TapeEngine::effectNames[fx], TapeEngine::variationNames[fx][v]);
            CHECK (identical, msg);
        }
    }

    // 3. Transport holds.
    in.stopHold = true; runBlocks (100, finite, peak, identical); CHECK (finite, "tape stop finite");
    CHECK (std::abs (e.getRate()) < 1e-6, "tape stop reaches zero speed");
    in.stopHold = false; runBlocks (300, finite, peak, identical); CHECK (e.isLive(), "tape stop resyncs");
    in.shuttle = -1; runBlocks (100, finite, peak, identical); CHECK (finite && e.getRate() < -2.5, "rewind runs backwards");
    in.shuttle = 0; runBlocks (300, finite, peak, identical); CHECK (e.isLive(), "rewind resyncs");
    in.shuttle = 1; runBlocks (100, finite, peak, identical); CHECK (finite && e.getRate() > 1.5, "cue runs fast");
    in.shuttle = 0; runBlocks (300, finite, peak, identical); CHECK (e.isLive(), "cue resyncs");

    // 4. Bypass is exact even with effects on.
    in.engaged = false; in.fx[Saturate].on = true; in.fx[LoFi].on = true;
    runBlocks (20, finite, peak, identical); CHECK (identical, "bypass is bit-exact");

    std::printf (failures == 0 ? "All engine tests passed\n" : "%d engine test(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}
