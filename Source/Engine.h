// BOTHER's engine: DRIVE, then the tremolo (shape, edge, drift), then GRIT and TONE.
// Header only and JUCE free, so the offline test can drive it directly.
#pragma once

#include "Lfo.h"
#include <array>
#include <atomic>

namespace bother
{
    constexpr float kMinRate = 0.05f, kMaxRate = 80.0f;      // RATE knob, Hz
    constexpr float kMinDrift = 0.02f, kMaxDrift = 8.0f;     // DRIFT RATE knob, Hz
    constexpr float kDriftOctaves = 3.0f;                    // DRIFT DEPTH at full: +-3 octaves of RATE
    constexpr float kCeilingRate = 2000.0f;                  // nothing wobbles faster than this
    constexpr float kHarmonicSplit = 700.0f;                 // the harmonic tremolo's crossover
    constexpr int kScope = 1024;                             // telemetry ring, at kScopeHz
    constexpr float kScopeHz = 400.0f;

    struct Params
    {
        Shape shape = Shape::sine;
        float edge = 0.0f;          // -1 round .. +1 sharp
        float rateHz = 5.0f;        // already worked out from SYNC by the caller
        float depth = 0.6f;         // 0..1
        float spread = 0.0f;        // 0..1, right channel's phase offset, up to half a cycle
        bool harmonic = false;

        Shape driftShape = Shape::wander;
        float driftRateHz = 0.15f;
        float driftDepth = 0.0f;    // 0..1
        float sway = 0.0f;          // 0..1, how much the drift also pulls DEPTH

        float drive = 0.15f;        // 0..1
        float grit = 0.0f;          // 0..1
        float tone = 0.0f;          // -1 dark .. +1 bright
    };

    class Engine
    {
    public:
        void prepare (double sampleRate, uint32_t seed_)
        {
            sr = (float) sampleRate;
            seed = seed_ | 1u;
            for (auto& c : ch)
            {
                c = Channel {};
                c.dc.prepare (sr);
                c.gritDc.prepare (sr);
                c.split.setCutoff (kHarmonicSplit, sr);
                c.tilt.setCutoff (900.0f, sr);
                c.tilt2.setCutoff (900.0f, sr);
            }
            depthG.prepare (sr, 15.0f, 0.6f);
            driveG.prepare (sr, 30.0f, 0.0f);
            gritG.prepare (sr, 30.0f, 0.0f);
            toneG.prepare (sr, 30.0f, 0.0f);
            spreadG.prepare (sr, 60.0f, 0.0f);
            cycles = 0.0;
            driftCycles = 0.0;
            scopeEvery = std::max (1, (int) (sr / kScopeHz));
            scopeCount = 0;
            primed = false;
        }

        // Where the wobble is, in cycles. The processor sets this from the host's beat position when
        // SYNC is on and nothing is drifting the rate, so the chop lands on the grid.
        void setCycles (double c) { cycles = c; }
        double getCycles() const { return cycles; }

        void process (float* L, float* R, int n, const Params& p)
        {
            if (! primed)
            {
                depthG.prepare (sr, 15.0f, p.depth);
                driveG.prepare (sr, 30.0f, p.drive);
                gritG.prepare (sr, 30.0f, p.grit);
                toneG.prepare (sr, 30.0f, p.tone);
                spreadG.prepare (sr, 60.0f, p.spread);
                primed = true;
            }
            depthG.set (p.depth);
            driveG.set (p.drive);
            gritG.set (p.grit);
            toneG.set (p.tone);
            spreadG.set (p.spread);

            float* io[2] = { L, R };
            for (int start = 0; start < n; start += kChunk)
            {
                const int m = std::min (kChunk, n - start);

                // The drift LFO and EDGE's filters move at chunk rate; everything audible is per sample.
                const float drift = rawShape (p.driftShape, driftCycles, seed ^ 0x5bd1e995u);
                const float rate = std::clamp (p.rateHz * std::exp2 (p.driftDepth * kDriftOctaves * drift),
                                               0.005f, kCeilingRate);
                driftCycles += (double) p.driftRateHz * (double) m / sr;
                for (auto& c : ch)
                    c.edge.setup (rate, p.edge, sr);
                const float swayDepth = 1.0f - p.sway + p.sway * 0.5f * (drift + 1.0f);

                for (int i = 0; i < m; ++i)
                {
                    const int s = start + i;
                    const float depth = std::clamp (depthG.next() * swayDepth, 0.0f, 1.0f);
                    const float drive = driveG.next(), grit = gritG.next(), tone = toneG.next();
                    const double offset = 0.5 * (double) spreadG.next();
                    const Drive dv = driveFor (drive);
                    const float toneLo = dbToGain (-8.0f * tone), toneHi = dbToGain (8.0f * tone);

                    for (int c = 0; c < 2; ++c)
                    {
                        auto& st = ch[(size_t) c];
                        float x = io[c][s];

                        // ---- DRIVE: pre-tremolo, so the chop bites the dirt
                        x = driveStage (st, x, dv);

                        // ---- the wobble
                        const double pos = cycles + (c == 1 ? offset : 0.0);
                        const float b = st.edge.process (rawShape (p.shape, pos, seed));
                        const float u = 0.5f * (b + 1.0f);              // 0..1
                        const float gain = 1.0f - depth * (1.0f - u);
                        if (p.harmonic)
                        {
                            // lows and highs breathe in opposite directions (the brownface trick)
                            const float lo = st.split.process (x);
                            const float hi = x - lo;
                            const float gainHi = 1.0f - depth * u;
                            x = 1.12f * (lo * gain + hi * gainHi);
                        }
                        else
                        {
                            st.split.process (x);   // keep the crossover warm for a clean switch
                            x *= gain;
                        }
                        if (c == 0)
                            lastGain = gain;

                        // ---- GRIT: after the wobble, so the quiet troughs sputter
                        x = gritStage (st, x, grit);

                        // ---- TONE: tilt around 900 Hz, +-8 dB at the ends
                        const float lo = st.tilt2.process (st.tilt.process (x));
                        x = lo * toneLo + (x - lo) * toneHi;

                        io[c][s] = x;
                    }

                    cycles += (double) rate / sr;
                    if (++scopeCount >= scopeEvery)
                    {
                        scopeCount = 0;
                        const int w = scopeWrite.load (std::memory_order_relaxed);
                        scope[(size_t) (w % kScope)].store (lastGain, std::memory_order_relaxed);
                        scopeWrite.store (w + 1, std::memory_order_release);
                    }
                }
                // keep the counters small; the fractional part is all that matters to the sound,
                // and whole cycles only pick the random values for STEPS and WANDER
                if (cycles > 1.0e7) cycles -= 1.0e7;
                if (driftCycles > 1.0e7) driftCycles -= 1.0e7;

                uiDrift.store (drift, std::memory_order_relaxed);
                uiRate.store (rate, std::memory_order_relaxed);
            }
        }

        // telemetry for the face
        std::array<std::atomic<float>, kScope> scope {};
        std::atomic<int> scopeWrite { 0 };
        std::atomic<float> uiDrift { 0.0f }, uiRate { 0.0f };

    private:
        struct Channel
        {
            Edge edge;
            AdaaTanh sat;
            DcBlock dc, gritDc;
            OnePole split, tilt, tilt2;
            float held = 0.0f, holdCount = 1.0e9f;
        };

        static constexpr int kChunk = 32;
        static constexpr float kDriveComp = 0.72f;

        struct Drive { float g, bias, offset, comp, amt; };

        // up to +27 dB into a slightly lopsided tanh, then most of that level taken back out.
        // Below about 3 % it fades to a clean bypass so DRIVE at zero really is clean.
        static Drive driveFor (float drive)
        {
            Drive d;
            d.g = dbToGain (27.0f * drive);
            d.bias = 0.25f * drive;
            d.offset = std::tanh (d.bias);
            d.comp = std::pow (d.g, -kDriveComp);
            d.amt = std::min (1.0f, drive * 33.0f);
            return d;
        }

        static float driveStage (Channel& st, float x, const Drive& d)
        {
            const float y = st.sat.process (d.g * x + d.bias) - d.offset;
            const float wet = st.dc.process (y) * d.comp;
            return x + (wet - x) * d.amt;
        }

        float gritStage (Channel& st, float x, float grit)
        {
            if (grit < 1.0e-4f)
            {
                st.holdCount = 1.0e9f;
                return x;
            }
            // rasp: a little half-wave asymmetry for the fuzzy edge
            float y = st.gritDc.process (x + 0.6f * grit * std::abs (x));
            // fewer bits: 16 down to 4
            const float bits = 16.0f - 12.0f * std::pow (grit, 0.7f);
            const float q = 2.0f / std::exp2 (bits);
            y = std::round (y / q) * q;
            // lower sample rate: hold each value for up to 25 samples
            const float hold = 1.0f + 24.0f * grit * grit;
            st.holdCount += 1.0f;
            if (st.holdCount >= hold)
            {
                st.holdCount = std::fmod (st.holdCount, hold);
                st.held = y;
            }
            return st.held;
        }

        float sr = 48000.0f;
        uint32_t seed = 1u;
        std::array<Channel, 2> ch {};
        Glide depthG, driveG, gritG, toneG, spreadG;
        double cycles = 0.0, driftCycles = 0.0;
        float lastGain = 1.0f;
        int scopeEvery = 120, scopeCount = 0;
        bool primed = false;
    };
}
