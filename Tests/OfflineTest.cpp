// Offline checks for the BOTHER engine. Build with -DBOTHER_BUILD_TESTS=ON.

#include "Engine.h"

#include <chrono>
#include <cstdio>
#include <functional>
#include <memory>
#include <vector>

static constexpr double sr = 48000.0;
static constexpr int block = 256;
static int failures = 0;

static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (! ok) ++failures;
}

struct Stereo { std::vector<float> l, r; };

using ParamFn = std::function<bother::Params (double)>;

static Stereo run (const std::vector<float>& inL, const std::vector<float>& inR, ParamFn params,
                   std::function<void (bother::Engine&)> after = {})
{
    auto e = std::make_unique<bother::Engine>();
    e->prepare (sr, 1234);
    Stereo out { inL, inR };
    for (size_t s = 0; s < inL.size(); s += block)
    {
        const int n = (int) std::min<size_t> (block, inL.size() - s);
        e->process (out.l.data() + s, out.r.data() + s, n, params ((double) s / sr));
        if (after) after (*e);
    }
    return out;
}

static Stereo run (const std::vector<float>& in, ParamFn params, std::function<void (bother::Engine&)> after = {})
{
    return run (in, in, params, after);
}

static std::vector<float> constant (double secs, float v) { return std::vector<float> ((size_t) (secs * sr), v); }

static std::vector<float> sine (double secs, double hz, float amp)
{
    std::vector<float> x ((size_t) (secs * sr));
    for (size_t i = 0; i < x.size(); ++i) x[i] = amp * (float) std::sin (2.0 * M_PI * hz * (double) i / sr);
    return x;
}

static bool finite (const std::vector<float>& x) { for (float v : x) if (! std::isfinite (v)) return false; return true; }
static float peak (const std::vector<float>& x) { float m = 0; for (float v : x) m = std::max (m, std::abs (v)); return m; }

static float rms (const std::vector<float>& x, double t0, double t1)
{
    const size_t a = (size_t) (t0 * sr), b = std::min (x.size(), (size_t) (t1 * sr));
    double acc = 0.0;
    for (size_t i = a; i < b; ++i) acc += (double) x[i] * x[i];
    return (float) std::sqrt (acc / (double) std::max<size_t> (1, b - a));
}

// fraction of samples (after t0) whose gain sits within 5 % of fully on or fully off
static float nearExtremes (const std::vector<float>& g, double t0, float lo, float hi)
{
    size_t count = 0, total = 0;
    const float band = 0.05f * (hi - lo);
    for (size_t i = (size_t) (t0 * sr); i < g.size(); ++i, ++total)
        if (g[i] > hi - band || g[i] < lo + band) ++count;
    return (float) count / (float) std::max<size_t> (1, total);
}

static float maxStep (const std::vector<float>& g, double t0)
{
    float m = 0.0f;
    for (size_t i = (size_t) (t0 * sr) + 1; i < g.size(); ++i) m = std::max (m, std::abs (g[i] - g[i - 1]));
    return m;
}

static std::pair<float, float> range (const std::vector<float>& g, double t0)
{
    float lo = 1e9f, hi = -1e9f;
    for (size_t i = (size_t) (t0 * sr); i < g.size(); ++i) { lo = std::min (lo, g[i]); hi = std::max (hi, g[i]); }
    return { lo, hi };
}

// counts upward crossings of the midpoint, to measure the wobble rate
static float wobbleHz (const std::vector<float>& g, double t0, double t1)
{
    const auto [lo, hi] = range (g, t0);
    const float mid = 0.5f * (lo + hi);
    int ups = 0;
    for (size_t i = (size_t) (t0 * sr) + 1; i < std::min (g.size(), (size_t) (t1 * sr)); ++i)
        if (g[i - 1] < mid && g[i] >= mid) ++ups;
    return (float) ups / (float) (t1 - t0);
}

// Goertzel power at one frequency over a span
static double tone (const std::vector<float>& x, double t0, double t1, double hz)
{
    const double w = 2.0 * M_PI * hz / sr, c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    const size_t a = (size_t) (t0 * sr), b = std::min (x.size(), (size_t) (t1 * sr));
    for (size_t i = a; i < b; ++i) { const double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return (s1 * s1 + s2 * s2 - c * s1 * s2) / (double) (b - a);
}

static bother::Params clean()
{
    bother::Params p;
    p.drive = 0.0f; p.grit = 0.0f; p.tone = 0.0f;
    return p;
}

int main()
{
    using bother::Shape;
    std::printf ("BOTHER engine tests\n");

    // With a steady DC input and no dirt, the output is the tremolo gain itself.
    const auto dc = constant (6.0, 0.5f);
    auto gainOf = [&] (ParamFn f) { auto o = run (dc, f); for (auto& v : o.l) v *= 2.0f; for (auto& v : o.r) v *= 2.0f; return o; };

    {
        std::printf ("THE WOBBLE\n");
        auto flat = run (sine (2.0, 440.0, 0.3f), [] (double) { auto p = clean(); p.depth = 0.0f; return p; });
        float diff = 0.0f;
        const auto ref = sine (2.0, 440.0, 0.3f);
        for (size_t i = 0; i < ref.size(); ++i) diff = std::max (diff, std::abs (flat.l[i] - ref[i]));
        check (diff < 1.0e-5f, "DEPTH 0 with no dirt is a clean wire");

        auto s = gainOf ([] (double) { auto p = clean(); p.depth = 1.0f; p.rateHz = 5.0f; return p; });
        const auto [lo, hi] = range (s.l, 1.0);
        const float hz = wobbleHz (s.l, 1.0, 6.0);
        std::printf ("        sine, 5 Hz, depth 100 %%: gain %.3f to %.3f, measured %.2f Hz\n", lo, hi, hz);
        check (lo < 0.02f && hi > 0.98f, "full depth goes all the way down and all the way up");
        check (std::abs (hz - 5.0f) < 0.25f, "RATE is the rate");

        auto half = gainOf ([] (double) { auto p = clean(); p.depth = 0.5f; return p; });
        const auto [hlo, hhi] = range (half.l, 1.0);
        check (std::abs (hlo - 0.5f) < 0.03f && hhi > 0.97f, "half depth dips to half");

        for (int sh = 0; sh < bother::kNumShapes; ++sh)
        {
            auto g = gainOf ([sh] (double) { auto p = clean(); p.shape = (Shape) sh; p.depth = 1.0f; p.rateHz = 7.0f; return p; });
            const float step = maxStep (g.l, 0.5);
            std::printf ("        %-9s biggest jump per sample %.4f\n", bother::kShapeNames[sh], step);
            check (finite (g.l) && step < 0.08f, "no clicks, even on the square and steps");
        }
    }

    {
        std::printf ("EDGE\n");
        auto sharpS = gainOf ([] (double) { auto p = clean(); p.depth = 1.0f; p.edge = 1.0f; return p; });
        auto pureS = gainOf ([] (double) { auto p = clean(); p.depth = 1.0f; p.edge = 0.0f; return p; });
        const float a = nearExtremes (pureS.l, 1.0, 0.0f, 1.0f), b = nearExtremes (sharpS.l, 1.0, 0.0f, 1.0f);
        std::printf ("        sine time spent fully on or off: centre %.0f %%, sharp %.0f %%\n", a * 100.0f, b * 100.0f);
        check (b > 0.85f && a < 0.35f, "sharpening turns a sine into a chop");

        auto sq = gainOf ([] (double) { auto p = clean(); p.shape = Shape::square; p.depth = 1.0f; return p; });
        auto rq = gainOf ([] (double) { auto p = clean(); p.shape = Shape::square; p.depth = 1.0f; p.edge = -1.0f; return p; });
        const auto [rlo, rhi] = range (rq.l, 1.0);
        const float c = nearExtremes (sq.l, 1.0, 0.0f, 1.0f), d = nearExtremes (rq.l, 1.0, rlo, rhi);
        std::printf ("        square time at its extremes: centre %.0f %%, round %.0f %%; rounded swing %.2f to %.2f\n",
                     c * 100.0f, d * 100.0f, rlo, rhi);
        check (c > 0.9f && d < 0.5f, "rounding melts the square's corners");
        check (rlo < 0.12f && rhi > 0.88f, "and keeps its depth (the smoother's loss is handed back)");

        auto st = gainOf ([] (double) { auto p = clean(); p.shape = Shape::steps; p.depth = 1.0f; p.rateHz = 4.0f; return p; });
        auto gl = gainOf ([] (double) { auto p = clean(); p.shape = Shape::steps; p.depth = 1.0f; p.rateHz = 4.0f; p.edge = -0.8f; return p; });
        std::printf ("        steps biggest jump: centre %.4f, round %.5f\n", maxStep (st.l, 0.5), maxStep (gl.l, 0.5));
        check (maxStep (gl.l, 0.5) < maxStep (st.l, 0.5) * 0.25f, "rounded steps glide");
    }

    {
        std::printf ("DRIFT\n");
        float rlo = 1e9f, rhi = 0.0f;
        run (dc, [] (double) { auto p = clean(); p.rateHz = 4.0f; p.driftDepth = 1.0f; p.driftRateHz = 0.5f; p.driftShape = Shape::sine; return p; },
             [&] (bother::Engine& e) { const float r = e.uiRate.load(); rlo = std::min (rlo, r); rhi = std::max (rhi, r); });
        std::printf ("        RATE 4 Hz, drift depth 100 %%: wobble ran %.2f to %.2f Hz\n", rlo, rhi);
        check (rlo < 0.6f && rhi > 28.0f, "full DRIFT DEPTH swings RATE three octaves each way");

        auto still = gainOf ([] (double) { auto p = clean(); p.depth = 1.0f; p.rateHz = 4.0f; p.driftDepth = 0.0f; return p; });
        auto wobbly = gainOf ([] (double) { auto p = clean(); p.depth = 1.0f; p.rateHz = 4.0f; p.driftDepth = 0.5f;
                                            p.driftRateHz = 0.3f; p.driftShape = Shape::sine; return p; });
        const float h1 = wobbleHz (wobbly.l, 0.2, 1.4), h2 = wobbleHz (wobbly.l, 1.8, 3.0);
        std::printf ("        drifting rate in two windows: %.1f Hz, %.1f Hz (steady: %.1f Hz)\n", h1, h2, wobbleHz (still.l, 0.2, 3.0));
        check (std::abs (h1 - h2) > 1.5f, "DRIFT moves the rate around");

        auto swayed = gainOf ([] (double) { auto p = clean(); p.depth = 1.0f; p.rateHz = 6.0f; p.sway = 1.0f;
                                            p.driftRateHz = 0.25f; p.driftShape = Shape::sine; return p; });
        const auto [a1, b1] = range (std::vector<float> (swayed.l.begin() + (long) (0.6 * sr), swayed.l.begin() + (long) (1.4 * sr)), 0.0);
        const auto [a2, b2] = range (std::vector<float> (swayed.l.begin() + (long) (2.6 * sr), swayed.l.begin() + (long) (3.4 * sr)), 0.0);
        std::printf ("        SWAY: dip %.2f near the drift's top, %.2f near its bottom\n", a1, a2);
        check (a2 - a1 > 0.6f, "SWAY lets the drift pull DEPTH too");
    }

    {
        std::printf ("STEREO AND HARMONIC\n");
        auto wide = gainOf ([] (double) { auto p = clean(); p.depth = 1.0f; p.spread = 1.0f; return p; });
        double corr = 0.0;
        for (size_t i = (size_t) sr; i < wide.l.size(); ++i) corr += (double) (wide.l[i] - 0.5f) * (wide.r[i] - 0.5f);
        check (corr < 0.0, "SPREAD 100 % puts the right channel half a cycle behind (an auto-pan)");

        std::vector<float> two ((size_t) (4.0 * sr));
        for (size_t i = 0; i < two.size(); ++i)
            two[i] = 0.25f * (float) (std::sin (2 * M_PI * 110.0 * (double) i / sr) + std::sin (2 * M_PI * 4000.0 * (double) i / sr));
        auto normal = run (two, [] (double) { auto p = clean(); p.depth = 0.8f; p.rateHz = 3.0f; return p; });
        auto harm = run (two, [] (double) { auto p = clean(); p.depth = 0.8f; p.rateHz = 3.0f; p.harmonic = true; return p; });
        // band energies in a trough and in a peak of the wobble (3 Hz: peaks at 1/12 s, troughs at 3/12 s past each cycle)
        const double lowPeak = tone (harm.l, 1.0 + 0.06, 1.0 + 0.11, 110.0), lowTrough = tone (harm.l, 1.0 + 0.226, 1.0 + 0.276, 110.0);
        const double hiPeak = tone (harm.l, 1.0 + 0.06, 1.0 + 0.11, 4000.0), hiTrough = tone (harm.l, 1.0 + 0.226, 1.0 + 0.276, 4000.0);
        std::printf ("        harmonic: 110 Hz %.3g -> %.3g, 4 kHz %.3g -> %.3g (peak -> trough)\n", lowPeak, lowTrough, hiPeak, hiTrough);
        check (lowPeak > lowTrough * 4.0 && hiTrough > hiPeak * 4.0, "HARMONIC: lows and highs breathe in opposite directions");
        check (rms (harm.l, 1.0, 4.0) > rms (normal.l, 1.0, 4.0) * 0.8f, "and it doesn't lose level doing it");
    }

    {
        std::printf ("DIRT\n");
        const auto in = sine (2.0, 220.0, 0.125f);   // about -18 dBFS rms
        const float rin = rms (in, 0.5, 2.0);
        for (float dr : { 0.25f, 0.5f, 0.75f, 1.0f })
        {
            auto o = run (in, [dr] (double) { auto p = clean(); p.depth = 0.0f; p.drive = dr; return p; });
            const float rr = rms (o.l, 0.5, 2.0);
            const double h3 = tone (o.l, 0.5, 2.0, 660.0) / tone (o.l, 0.5, 2.0, 220.0);
            std::printf ("        DRIVE %3.0f %%: level %+5.1f dB, 3rd harmonic %.1f dB under the fundamental\n",
                         dr * 100.0f, 20.0 * std::log10 (rr / rin), -10.0 * std::log10 (h3));
            check (finite (o.l) && std::abs (20.0f * std::log10 (rr / rin)) < 6.0f, "drive stays within 6 dB of the dry level");
        }
        auto d0 = run (in, [] (double) { auto p = clean(); p.depth = 0.0f; p.drive = 0.2f; return p; });
        auto d1 = run (in, [] (double) { auto p = clean(); p.depth = 0.0f; p.drive = 0.9f; return p; });
        check (tone (d1.l, 0.5, 2.0, 660.0) > tone (d0.l, 0.5, 2.0, 660.0) * 10.0, "more DRIVE, more harmonics");

        auto g0 = run (in, [] (double) { auto p = clean(); p.depth = 0.0f; return p; });
        auto g1 = run (in, [] (double) { auto p = clean(); p.depth = 0.0f; p.grit = 0.8f; return p; });
        const double fizz0 = tone (g0.l, 0.5, 2.0, 7000.0) + tone (g0.l, 0.5, 2.0, 11000.0);
        const double fizz1 = tone (g1.l, 0.5, 2.0, 7000.0) + tone (g1.l, 0.5, 2.0, 11000.0);
        std::printf ("        GRIT 80 %%: 7k + 11k energy %.3g (clean %.3g)\n", fizz1, fizz0);
        check (finite (g1.l) && fizz1 > fizz0 * 1000.0 && peak (g1.l) < 0.5f, "GRIT adds rasp without blowing up");

        const auto hiss = sine (2.0, 8000.0, 0.1f);
        auto bright = run (hiss, [] (double) { auto p = clean(); p.depth = 0.0f; p.tone = 1.0f; return p; });
        auto dark = run (hiss, [] (double) { auto p = clean(); p.depth = 0.0f; p.tone = -1.0f; return p; });
        const float bd = 20.0f * std::log10 (rms (bright.l, 0.5, 2.0) / rms (hiss, 0.5, 2.0));
        const float dd = 20.0f * std::log10 (rms (dark.l, 0.5, 2.0) / rms (hiss, 0.5, 2.0));
        std::printf ("        TONE at 8 kHz: bright %+.1f dB, dark %+.1f dB\n", bd, dd);
        check (bd > 6.0f && dd < -6.0f, "TONE tilts");
    }

    {
        std::printf ("TORTURE\n");
        const auto in = sine (8.0, 150.0, 0.7f);
        auto o = run (in, [] (double t)
        {
            bother::Params p;
            p.shape = (Shape) ((int) (t * 3.0) % bother::kNumShapes);
            p.driftShape = (Shape) ((int) (t * 1.7) % bother::kNumShapes);
            p.edge = (float) std::sin (t * 1.3);
            p.rateHz = bother::kMinRate + (bother::kMaxRate - bother::kMinRate) * (float) (0.5 + 0.5 * std::sin (t * 0.9));
            p.depth = 1.0f; p.spread = (float) (0.5 + 0.5 * std::cos (t));
            p.driftDepth = 1.0f; p.driftRateHz = 8.0f; p.sway = 1.0f; p.harmonic = std::sin (t * 2.0) > 0.0;
            p.drive = 1.0f; p.grit = (float) (0.5 + 0.5 * std::sin (t * 0.4)); p.tone = (float) std::sin (t * 0.7);
            return p;
        });
        std::printf ("        everything swept at once, audio-rate drift: peak %.3f\n", peak (o.l));
        check (finite (o.l) && finite (o.r) && peak (o.l) < 2.0f && peak (o.r) < 2.0f, "no NaN, no blow-up");

        auto quiet = run (constant (2.0, 0.0f), [] (double) { bother::Params p; p.drive = 1.0f; p.grit = 1.0f; p.harmonic = true; return p; });
        check (peak (quiet.l) < 1.0e-6f, "silence in, silence out with every dirt knob up");
    }

    {
        std::printf ("CPU\n");
        bother::Engine e;
        e.prepare (sr, 1);
        std::vector<float> l (block, 0.1f), r (block, 0.1f);
        bother::Params p;
        p.drive = 1.0f; p.grit = 0.6f; p.harmonic = true; p.driftDepth = 0.5f; p.edge = -0.5f;
        const int blocks = (int) (20.0 * sr / block);
        const auto t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < blocks; ++k)
            e.process (l.data(), r.data(), block, p);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("        20 s of stereo audio, everything on, in %.3f s (%.2f %% of one core)\n", secs, secs / 20.0 * 100.0);
        check (secs < 20.0 * 0.05, "under 5 % of one core");
    }

    std::printf (failures == 0 ? "ALL PASS\n" : "%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
