#pragma once

// NoviTape DSP engine. Plain C++ (no JUCE) so it can be unit-tested on its own.
//
// Live audio is written into a "tape" history buffer. When no time effect is
// active the read head sits exactly on the write head, so audio passes through
// untouched with zero latency. Speed, Reverse, Brake and the transport holds move
// the read head away from "now"; when they are released it snaps back to the
// beat with a short crossfade, the way a performance tape effect should.

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace novitape
{

enum FxId { Speed = 0, Warble, Saturate, Brake, Reverse, LoFi, Mute, Stutter, NumFx };

struct FxSetting
{
    bool on = false;
    int variation = 0;     // 0..3
    float amount = 0.5f;   // 0..1
};

struct EngineInput
{
    std::array<FxSetting, NumFx> fx {};
    float inputGain = 1.0f;    // linear
    float outputGain = 1.0f;   // linear
    bool engaged = true;       // false = clean bypass
    bool stopHold = false;     // tape stop while held
    bool pauseHold = false;    // quick pause while held
    int shuttle = 0;           // -1 rewind, +1 cue, 0 none
    double bpm = 120.0;
    bool hostPlaying = false;
    double ppqAtBlockStart = 0.0;
};

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set (bool lowpass, double f, double q, double sr);
    inline double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }
};

class TapeEngine
{
public:
    void prepare (double sampleRate);
    void reset();

    // In-place stereo processing.
    void process (float* left, float* right, int numSamples, const EngineInput& in);

    struct Meters
    {
        std::atomic<float> peakL { 0 }, peakR { 0 }, rms { 0 }, heat { 0 }, rate { 1 }, lagSeconds { 0 };
    } meters;

    bool isLive() const noexcept { return delay == 0.0 && ! xfActive; }
    double getDelaySamples() const noexcept { return delay; }
    double getRate() const noexcept { return lastRate; }

    static const char* const effectNames[NumFx];
    static const char* const amountNames[NumFx];
    static const char* const variationNames[NumFx][4];
    static const float defaultAmounts[NumFx];

private:
    void readTape (double absPos, double& l, double& r) const;
    void startJump (double newDelay);
    float rand01() noexcept
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (rng & 0xffffff) / 16777216.0f;
    }

    double sr = 44100.0;
    std::vector<float> histL, histR;
    int64_t histSize = 0, writeCount = 0;
    double maxLag = 0;

    // read head
    double delay = 0, xfDelay = 0, lastRate = 1;
    int xfPos = 0, xfLen = 480;
    bool xfActive = false;

    // rate model
    double base = 1, tp = 1, bl = 1, slew = 0.3;
    bool prevIntent = false, prevSpeed = false, prevReverse = false;
    int prevShuttle = 0, prevRevVar = -1, prevSpeedVar = -1;

    // stutter
    bool stActive = false;
    double stStart = 0, stPhase = 0, stLen = 1, stMix = 0;

    // warble (post vibrato)
    std::vector<float> wbL, wbR;
    int wbSize = 1, wbPos = 0;
    double ww = 0, ph1 = 0, ph2 = 0, rnd = 0, rndT = 0;

    // mute
    double mg = 1, drop = 0, dropT = 0, freeBeats = 0;

    // saturation
    double sw = 0, sfL = 0, sfR = 0, dcxL = 0, dcyL = 0, dcxR = 0, dcyR = 0;

    // lo-fi
    double lw = 0, hiss = 0, hiss2 = 0;
    Biquad lpL, lpR, hpL, hpR;
    int hc = 0;
    double hl = 0, hr = 0;

    uint32_t rng = 0x9e3779b9u;
};

} // namespace novitape
