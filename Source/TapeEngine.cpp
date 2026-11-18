#include "TapeEngine.h"

#include <algorithm>
#include <cmath>

namespace novitape
{

const char* const TapeEngine::effectNames[NumFx] = { "SPEED", "WARBLE", "SATURATE", "BRAKE", "REVERSE", "LO-FI", "MUTE", "STUTTER" };
const char* const TapeEngine::amountNames[NumFx] = { "GLIDE", "DEPTH", "DRIVE", "LENGTH", "SLEW", "AMOUNT", "DEPTH", "GATE" };
const char* const TapeEngine::variationNames[NumFx][4] = {
    { "HALF", "3/4 SLOW", "1.5X FAST", "DOUBLE" },
    { "SUBTLE", "WOBBLY", "SEASICK", "CHEWED" },
    { "TAPE", "WARM", "HOT", "CRUSH" },
    { "SHORT", "MEDIUM", "LONG", "SPIN BACK" },
    { "FLIP", "HALF", "REWIND", "SWELL" },
    { "CASSETTE", "DICTAPHONE", "8-BIT", "AM RADIO" },
    { "CUT", "DROPOUT", "CHOP 1/8", "CHOP 1/16" },
    { "1/4", "1/8", "1/16", "1/32" },
};
const float TapeEngine::defaultAmounts[NumFx] = { 0.3f, 0.5f, 0.5f, 0.5f, 0.0f, 0.6f, 1.0f, 0.85f };

namespace
{
    constexpr double kTau = 6.283185307179586;
    constexpr double kHalfPi = 1.5707963267948966;

    const double kSpeeds[4]   = { 0.5, 0.75, 1.5, 2.0 };
    const double kRevs[4]     = { 1.0, 0.5, 3.0, 1.0 };
    const double kRevSlew[4]  = { 0.004, 0.02, 0.06, 0.55 };
    const double kBrakeT[4]   = { 0.25, 0.8, 2.4, 0.7 };
    const double kStut[4]     = { 1.0, 0.5, 0.25, 0.125 };
    const double kSatDrive[4] = { 6.0, 4.0, 12.0, 22.0 };

    struct Warb { double f1, a1, f2, a2, a3, rp, rs; };
    // a* are pitch deviations (fraction of speed); converted to delay sweeps below.
    const Warb kWarb[4] = {
        { 0.55, 0.0035, 7.5, 0.0008, 0.0012, 0.02, 0.00005 },
        { 1.2,  0.011,  5.2, 0.0016, 0.003,  0.04, 0.0001  },
        { 0.32, 0.032,  0.0, 0.0,    0.008,  0.02, 0.00003 },
        { 2.6,  0.012,  11., 0.003,  0.035,  0.25, 0.0012  },
    };

    struct LoFiCfg { double mono, drive; int hold; int bits; double makeup; };
    const LoFiCfg kLoFi[4] = {
        { 0.3, 1.0, 1, 0, 1.05 },
        { 1.0, 2.2, 3, 0, 1.7  },
        { 0.0, 1.0, 1, 8, 1.0  },
        { 1.0, 1.6, 1, 0, 1.9  },
    };

    inline double clampd (double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
}

void Biquad::set (bool lowpass, double f, double q, double sampleRate)
{
    f = std::min (f, sampleRate * 0.45);
    const double w = kTau * f / sampleRate, c = std::cos (w), s = std::sin (w), al = s / (2.0 * q);
    const double a0 = 1.0 + al;
    double nb0, nb1, nb2;
    if (lowpass) { nb0 = (1 - c) / 2; nb1 = 1 - c; nb2 = (1 - c) / 2; }
    else         { nb0 = (1 + c) / 2; nb1 = -(1 + c); nb2 = (1 + c) / 2; }
    b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = (-2.0 * c) / a0; a2 = (1.0 - al) / a0;
}

void TapeEngine::prepare (double sampleRate)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    histSize = (int64_t) std::ceil (sr * 12.0);
    histL.assign ((size_t) histSize, 0.0f);
    histR.assign ((size_t) histSize, 0.0f);
    maxLag = (double) histSize - sr * 0.5;
    xfLen = std::max (64, (int) (sr * 0.008));

    wbSize = (int) std::ceil (sr * 0.12);
    wbL.assign ((size_t) wbSize, 0.0f);
    wbR.assign ((size_t) wbSize, 0.0f);
    reset();
}

void TapeEngine::reset()
{
    std::fill (histL.begin(), histL.end(), 0.0f);
    std::fill (histR.begin(), histR.end(), 0.0f);
    std::fill (wbL.begin(), wbL.end(), 0.0f);
    std::fill (wbR.begin(), wbR.end(), 0.0f);
    writeCount = 0; delay = 0; xfDelay = 0; xfActive = false; xfPos = 0; lastRate = 1;
    base = 1; tp = 1; bl = 1; slew = 0.3;
    prevIntent = prevSpeed = prevReverse = false; prevShuttle = 0; prevRevVar = prevSpeedVar = -1;
    stActive = false; stStart = stPhase = 0; stLen = 1; stMix = 0;
    wbPos = 0; ww = ph1 = ph2 = rnd = rndT = 0;
    mg = 1; drop = dropT = 0; freeBeats = 0;
    sw = sfL = sfR = dcxL = dcyL = dcxR = dcyR = 0;
    lw = hiss = hiss2 = 0; hc = 0; hl = hr = 0;
    lpL.reset(); lpR.reset(); hpL.reset(); hpR.reset();
}

void TapeEngine::readTape (double absPos, double& l, double& r) const
{
    // 4-point Hermite read, never past the newest written sample.
    const int64_t newest = writeCount - 1;
    const int64_t oldest = writeCount - histSize + 4;
    int64_t i = (int64_t) std::floor (absPos);
    const double f = absPos - (double) i;
    auto at = [&] (int64_t k, const std::vector<float>& h) -> double
    {
        k = k > newest ? newest : (k < oldest ? oldest : k);
        int64_t m = k % histSize; if (m < 0) m += histSize;
        return h[(size_t) m];
    };
    auto herm = [&] (const std::vector<float>& h)
    {
        const double xm1 = at (i - 1, h), x0 = at (i, h), x1 = at (i + 1, h), x2 = at (i + 2, h);
        const double c = (x1 - xm1) * 0.5, v = x0 - x1, w = c + v, a = w + v + (x2 - x0) * 0.5, b = w + a;
        return ((a * f - b) * f + c) * f + x0;
    };
    l = herm (histL);
    r = herm (histR);
}

void TapeEngine::startJump (double newDelay)
{
    xfDelay = delay;
    delay = newDelay;
    xfPos = 0;
    xfActive = true;
}

void TapeEngine::process (float* L, float* R, int n, const EngineInput& in)
{
    if (histSize == 0 || n <= 0) return;

    const double dt = 1.0 / sr;
    const auto& sp = in.fx[Speed];   const auto& rv = in.fx[Reverse]; const auto& br = in.fx[Brake];
    const auto& wb = in.fx[Warble];  const auto& st = in.fx[Stutter]; const auto& mu = in.fx[Mute];
    const auto& sa = in.fx[Saturate]; const auto& lf = in.fx[LoFi];
    const double bpm = clampd (in.bpm > 0 ? in.bpm : 120.0, 30.0, 300.0);
    const double beat = 60.0 / bpm * sr;

    // ----- glide time: set by whichever pitch/direction control changed last
    if (sp.on != prevSpeed || (sp.on && sp.variation != prevSpeedVar)) slew = 0.01 + sp.amount * 1.2;
    if (rv.on != prevReverse || (rv.on && rv.variation != prevRevVar)) slew = kRevSlew[rv.variation & 3] + rv.amount * 0.6;
    if ((prevSpeed && ! sp.on) || (prevReverse && ! rv.on)) slew = std::min (slew, 0.35);   // release spins back up briskly
    if (in.shuttle != prevShuttle) slew = in.shuttle != 0 ? 0.18 : 0.12;
    prevSpeed = sp.on; prevReverse = rv.on; prevShuttle = in.shuttle;
    prevSpeedVar = sp.variation; prevRevVar = rv.variation;

    // ----- target playback rate
    double target = 1.0;
    if (sp.on) target *= kSpeeds[sp.variation & 3];
    if (rv.on) target *= -kRevs[rv.variation & 3];
    if (in.shuttle < 0) target = -3.0;
    if (in.shuttle > 0) target = 2.0;
    const double kSlew = 1.0 - std::exp (-dt / std::max (0.002, slew));

    // brake
    const double bT = kBrakeT[br.variation & 3] * (0.4 + br.amount * 1.6);
    const bool spinBack = br.on && (br.variation & 3) == 3;
    const double bTarget = br.on ? (spinBack ? -1.3 : 0.0) : 1.0;
    const double bStep = br.on ? (dt / bT) * (spinBack ? 2.3 : 1.0) : dt / (bT * 0.35);

    // transport holds
    const double tTarget = (in.stopHold || in.pauseHold) ? 0.0 : 1.0;
    const double tStep = tTarget > 0.5 ? dt / 0.12 : dt / (in.pauseHold ? 0.08 : 0.5);

    // ----- read-head windows
    const bool intent = sp.on || rv.on || br.on || in.stopHold || in.pauseHold || in.shuttle != 0;
    if (prevIntent && ! intent && delay > 0.5)
    {
        // Released: snap the tape back to "now" (spin-up continues from there).
        startJump (0.0);
    }
    prevIntent = intent;

    double window = maxLag;
    if (in.shuttle < 0) window = beat;
    else if (rv.on) window = ((rv.variation & 3) == 2 ? 1.0 : 2.0) * beat;
    else if (sp.on && kSpeeds[sp.variation & 3] < 1.0) window = 2.0 * beat;
    window = std::min (window, maxLag);
    const double fastWindow = std::min (beat, maxLag);

    // ----- warble
    const Warb& W = kWarb[wb.variation & 3];
    const double wbTarget = wb.on ? 1.0 : 0.0, wDepth = wb.amount * 2.0;
    if (rand01() < W.rp) rndT = rand01() * 2.0 - 1.0;
    const double wCenter = sr * 0.03;
    const double w1 = W.f1 > 0 ? W.a1 / (kTau * W.f1) : 0.0;
    const double w2 = W.f2 > 0 ? W.a2 / (kTau * W.f2) : 0.0;

    // ----- stutter
    if (st.on)
    {
        stLen = std::max (256.0, beat * kStut[st.variation & 3]);
        if (! stActive) { stActive = true; stStart = (double) (writeCount - 1) - delay + 1.0; stPhase = 0; }
        if (stPhase >= stLen) stPhase = std::fmod (stPhase, stLen);
    }
    const double stTarget = st.on ? 1.0 : 0.0, gate = 0.25 + st.amount * 0.75;

    // ----- mute
    if (mu.on && (mu.variation & 3) == 1)
    {
        if (rand01() < 0.12f) dropT = rand01() < 0.45f ? 1.0 : rand01() * 0.5;
        else dropT *= 0.9;
    }
    const double beatsPerSample = bpm / 60.0 / sr;
    double beats = in.hostPlaying ? in.ppqAtBlockStart : freeBeats;

    // ----- smoothing coefficients
    const double kM = 1.0 - std::exp (-dt / 0.003), kF = 1.0 - std::exp (-dt / 0.004);
    const double kW = 1.0 - std::exp (-dt / 0.012), kDrop = 1.0 - std::exp (-dt / 0.015);
    const double kWarb = 1.0 - std::exp (-dt / 0.05);

    // ----- saturation
    const double drive = 1.0 + sa.amount * kSatDrive[sa.variation & 3], satComp = 0.78;

    // ----- lo-fi
    const LoFiCfg& C = kLoFi[lf.variation & 3];
    const double la = lf.amount;
    double lpf = 8000, hpf = 40, noise = 0, crackle = 0;
    int bits = 0, hold = C.hold;
    switch (lf.variation & 3)
    {
        case 0: lpf = 9000 - la * 4500; hpf = 70;  noise = 0.006 * (0.4 + la * 1.2); break;
        case 1: lpf = 4200 - la * 1800; hpf = 420; noise = 0.012 * (0.3 + la); break;
        case 2: lpf = 15000; hpf = 30; bits = (int) std::lround (12 - la * 8); hold = 1 + (int) std::lround (la * 6); break;
        default: lpf = 3200 - la * 1200; hpf = 650; noise = 0.008 * (0.3 + la); crackle = la * 0.0006; break;
    }
    lpL.set (true, lpf, 0.707, sr); lpR.set (true, lpf, 0.707, sr);
    hpL.set (false, hpf, 0.707, sr); hpR.set (false, hpf, 0.707, sr);
    const double bq = bits ? std::pow (2.0, bits - 1) : 0.0;

    double pkL = 0, pkR = 0, sum = 0, heat = 0;

    for (int i = 0; i < n; ++i)
    {
        const double inL = (double) L[i] * in.inputGain, inR = (double) R[i] * in.inputGain;

        // record onto the tape
        {
            int64_t w = writeCount % histSize;
            histL[(size_t) w] = (float) inL; histR[(size_t) w] = (float) inR;
            ++writeCount;
        }
        const double newest = (double) (writeCount - 1);

        if (! in.engaged)
        {
            const double ol = L[i], orr = R[i];
            pkL = std::max (pkL, std::abs (ol)); pkR = std::max (pkR, std::abs (orr)); sum += ol * ol + orr * orr;
            beats += beatsPerSample;
            continue;
        }

        // ----- rate ramps
        if (tp < tTarget) tp = std::min (tTarget, tp + tStep); else if (tp > tTarget) tp = std::max (tTarget, tp - tStep);
        if (bl < bTarget) bl = std::min (bTarget, bl + bStep); else if (bl > bTarget) bl = std::max (bTarget, bl - bStep);
        base += (target - base) * kSlew;
        if (std::abs (base - target) < 5e-4) base = target;   // < 1 cent: inaudible snap

        const double bm = bl >= 0 ? std::pow (bl, 1.6) : bl;
        const double tm = tp * tp * (3.0 - 2.0 * tp);
        const double rate = base * bm * tm;
        lastRate = rate;

        // ----- move the read head
        delay += 1.0 - rate;
        if (xfActive) xfDelay += 1.0 - rate;
        if (! xfActive)
        {
            if (rate > 1.0 && delay < 0.0) startJump (fastWindow);
            else if (delay > window) startJump (std::max (0.0, delay - window));
            else if (! intent && bl == 1.0 && tp == 1.0 && std::abs (base - 1.0) < 1e-3 && delay > 0.5) startJump (0.0);
        }
        delay = clampd (delay, 0.0, maxLag);
        xfDelay = clampd (xfDelay, 0.0, maxLag);
        if (! intent && bl == 1.0 && tp == 1.0 && base == 1.0 && ! xfActive && delay < 0.5) delay = 0.0;

        double xl, xr;
        if (delay == 0.0) { xl = inL; xr = inR; }
        else readTape (newest - delay, xl, xr);

        if (xfActive)
        {
            double ol, orr;
            readTape (newest - xfDelay, ol, orr);
            const double t = ((double) xfPos + 0.5) / (double) xfLen;
            const double gi = std::sin (t * kHalfPi), go = std::cos (t * kHalfPi);
            xl = ol * go + xl * gi; xr = orr * go + xr * gi;
            if (++xfPos >= xfLen) xfActive = false;
        }

        // ----- stutter (loops a slice while the tape keeps rolling underneath)
        stMix += (stTarget - stMix) * kF;
        if (stActive)
        {
            if (stMix > 1e-4)
            {
                const double ph = stPhase / stLen, fe = std::min (0.2, 0.003 * sr / stLen);
                const double env = std::max (0.0, std::min ({ 1.0, ph / fe, (gate - ph) / fe }));
                double sl, srr;
                readTape (stStart + stPhase, sl, srr);
                xl += (sl * env - xl) * stMix; xr += (srr * env - xr) * stMix;
                stPhase += rate;
                if (stPhase >= stLen) stPhase -= stLen; else if (stPhase < 0) stPhase += stLen;
            }
            else if (! st.on) stActive = false;
        }

        // tape loses level as it slows to a stop
        const double ar = std::abs (rate);
        if (ar < 0.2) { const double g = ar * 5.0; xl *= g; xr *= g; }

        // ----- warble (wow and flutter as a short modulated delay)
        ww += (wbTarget - ww) * kWarb;
        if (ww > 1e-4)
        {
            wbL[(size_t) wbPos] = (float) xl; wbR[(size_t) wbPos] = (float) xr;
            ph1 += kTau * W.f1 * dt; ph2 += kTau * W.f2 * dt;
            if (ph1 > kTau) ph1 -= kTau;
            if (ph2 > kTau) ph2 -= kTau;
            rnd += (rndT - rnd) * W.rs * 44.0;
            double offs = sr * wDepth * (w1 * std::sin (ph1) + w2 * std::sin (ph2) + W.a3 * 0.25 * rnd);
            offs = clampd (offs, -wCenter + 8.0, wCenter);
            double rp = (double) wbPos - (wCenter + offs);
            while (rp < 0) rp += wbSize;
            const int i0 = (int) rp; const double fr = rp - i0; const int i1 = (i0 + 1) % wbSize;
            const double vl = wbL[(size_t) i0] + (wbL[(size_t) i1] - wbL[(size_t) i0]) * fr;
            const double vr = wbR[(size_t) i0] + (wbR[(size_t) i1] - wbR[(size_t) i0]) * fr;
            xl += (vl - xl) * ww; xr += (vr - xr) * ww;
            if (++wbPos >= wbSize) wbPos = 0;
        }
        else if (! wb.on)
        {
            ww = 0;
        }

        // ----- mute
        double mt = 1.0;
        if (mu.on)
        {
            switch (mu.variation & 3)
            {
                case 0: mt = 1.0 - mu.amount; break;
                case 1: drop += (dropT - drop) * kDrop; mt = 1.0 - mu.amount * drop; break;
                default:
                {
                    const double div = (mu.variation & 3) == 2 ? 2.0 : 4.0;
                    const double x = beats * div;
                    mt = (x - std::floor (x)) < 0.5 ? 1.0 : 1.0 - mu.amount;
                }
            }
        }
        mg += (mt - mg) * kM;
        if (std::abs (mg - 1.0) < 1e-6) mg = 1.0;
        if (mg != 1.0) { xl *= mg; xr *= mg; }
        beats += beatsPerSample;

        // ----- saturation
        sw += ((sa.on ? 1.0 : 0.0) - sw) * kW;
        if (sw > 1e-4)
        {
            const double a = xl * drive, b = xr * drive;
            heat += std::abs (a) + std::abs (b);
            double ya, yb;
            switch (sa.variation & 3)
            {
                case 0: ya = std::tanh (a); yb = std::tanh (b); sfL += (ya - sfL) * 0.55; sfR += (yb - sfR) * 0.55; ya = sfL; yb = sfR; break;
                case 1: { const double ta = std::tanh (a), tb = std::tanh (b); ya = ta + 0.22 * ta * ta; yb = tb + 0.22 * tb * tb; break; }
                case 2: ya = 0.6366 * std::atan (a * 1.4); yb = 0.6366 * std::atan (b * 1.4); break;
                default: ya = clampd (a, -0.7, 0.7) / 0.7; yb = clampd (b, -0.7, 0.7) / 0.7; break;
            }
            // DC blocker on the saturated path only
            const double dl = ya - dcxL + 0.995 * dcyL; dcxL = ya; dcyL = dl;
            const double dr = yb - dcxR + 0.995 * dcyR; dcxR = yb; dcyR = dr;
            xl += (dl * satComp - xl) * sw; xr += (dr * satComp - xr) * sw;
        }
        else if (! sa.on) sw = 0;

        // ----- lo-fi
        lw += ((lf.on ? 1.0 : 0.0) - lw) * kW;
        if (lw > 1e-4)
        {
            double a = xl, b = xr;
            if (C.mono > 0) { const double m = (a + b) * 0.5; a += (m - a) * C.mono; b += (m - b) * C.mono; }
            if (C.drive > 1) { a = std::tanh (a * C.drive) / C.drive * 1.6; b = std::tanh (b * C.drive) / C.drive * 1.6; }
            if (hold > 1) { if (++hc >= hold) { hc = 0; hl = a; hr = b; } a = hl; b = hr; }
            if (bq > 0) { a = std::round (a * bq) / bq; b = std::round (b * bq) / bq; }
            a = hpL.process (lpL.process (a)) * C.makeup;
            b = hpR.process (lpR.process (b)) * C.makeup;
            if (noise > 0)
            {
                hiss = hiss * 0.55 + (rand01() * 2.0 - 1.0) * 0.45;
                hiss2 = hiss2 * 0.55 + (rand01() * 2.0 - 1.0) * 0.45;
                a += hiss * noise; b += (C.mono > 0.5 ? hiss : hiss2) * noise;
            }
            if (crackle > 0 && rand01() < crackle) { const double c = (rand01() * 2.0 - 1.0) * 0.35; a += c; b += c; }
            xl += (a - xl) * lw; xr += (b - xr) * lw;
        }
        else if (! lf.on) lw = 0;

        const double ol = xl * in.outputGain, orr = xr * in.outputGain;
        L[i] = (float) ol; R[i] = (float) orr;
        pkL = std::max (pkL, std::abs (ol)); pkR = std::max (pkR, std::abs (orr));
        sum += ol * ol + orr * orr;
    }

    freeBeats = beats;

    auto raise = [] (std::atomic<float>& a, float v) { float cur = a.load(); while (v > cur && ! a.compare_exchange_weak (cur, v)) {} };
    raise (meters.peakL, (float) pkL);
    raise (meters.peakR, (float) pkR);
    meters.rms.store ((float) std::sqrt (sum / (2.0 * n)));
    meters.heat.store ((float) (heat / (2.0 * n) * sw));
    meters.rate.store ((float) (in.engaged ? lastRate : 1.0));
    meters.lagSeconds.store ((float) (delay / sr));
}

} // namespace novitape
