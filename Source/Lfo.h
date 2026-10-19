// The wobble: seven shapes, and EDGE, which rounds them off or sharpens them up.
#pragma once

#include "Dsp.h"

namespace bother
{
    enum class Shape { sine, triangle, square, rampUp, rampDown, steps, wander };
    constexpr int kNumShapes = 7;
    inline const char* const kShapeNames[kNumShapes] = { "SINE", "TRIANGLE", "SQUARE", "RAMP UP", "RAMP DOWN", "STEPS", "WANDER" };

    // The bare shape, -1..1, at a position measured in cycles (whole part = which cycle, fraction = where in it).
    inline float rawShape (Shape s, double cycles, uint32_t seed)
    {
        const double fl = std::floor (cycles);
        const float p = (float) (cycles - fl);
        const auto n = (int64_t) fl;
        switch (s)
        {
            case Shape::sine:     return std::sin (kTwoPi * p);
            case Shape::triangle: return p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f);
            case Shape::square:   return p < 0.5f ? 1.0f : -1.0f;
            case Shape::rampUp:   return 2.0f * p - 1.0f;
            case Shape::rampDown: return 1.0f - 2.0f * p;
            case Shape::steps:    return cycleRandom (n, seed);
            case Shape::wander:
            {
                const float a = cycleRandom (n, seed), b = cycleRandom (n + 1, seed);
                const float t = 0.5f - 0.5f * std::cos (kPi * p);
                return a + (b - a) * t;
            }
        }
        return 0.0f;
    }

    // EDGE, one per channel.
    //   centre: the pure shape (with a sub-millisecond softening so a square never clicks)
    //   right:  sharpen. A tanh curve pushes the shape toward hard on/off: a sine becomes a chop.
    //   left:   round. A two-pole smoother closes in on the rate itself, so corners melt and steps glide.
    //           The level the smoother takes away is given back, so rounding never just means "shallower".
    struct Edge
    {
        float z1 = 0.0f, z2 = 0.0f;
        float a = 1.0f, k = 0.0f, tanhK = 1.0f, comp = 1.0f;

        void setup (float rateHz, float edge, float sr)
        {
            edge = std::clamp (edge, -1.0f, 1.0f);
            const float e = std::max (0.0f, edge);
            k = 60.0f * e * e * e;
            tanhK = k > 1.0e-3f ? std::tanh (k) : 1.0f;

            const float base = std::min (250.0f + 40.0f * rateHz, sr * 0.4f);
            float fc = base;
            if (edge < 0.0f)
                fc = base * std::pow (std::max (0.6f * rateHz, 0.01f) / base, -edge);
            a = OnePole::coef (fc, sr);

            // two cascaded one-poles take the fundamental down by 1 / (1 + (f/fc)^2); hand it back
            const float r = rateHz / fc;
            comp = std::min (6.0f, 1.0f + r * r);
        }

        float process (float x)
        {
            if (k > 1.0e-3f)
                x = std::tanh (k * x) / tanhK;
            z1 += a * (x - z1);
            z2 += a * (z1 - z2);
            return limitUnit (z2 * comp);
        }

        void reset (float v = 0.0f) { z1 = z2 = v; }
    };

    // Two cycles of a shape at a given EDGE, for the picture on the face. Runs the real Edge at a
    // nominal 5 Hz so the drawing rounds and sharpens exactly as the sound does.
    inline void picture (Shape s, float edge, int pointsPerCycle, float* out2Cycles)
    {
        constexpr float rate = 5.0f;
        const float sr = rate * (float) pointsPerCycle;
        Edge e;
        e.setup (rate, edge, sr);
        e.reset (rawShape (s, 0.0, 7u));
        const int warm = 6 * pointsPerCycle;
        for (int i = 0; i < warm + 2 * pointsPerCycle; ++i)
        {
            const float y = e.process (rawShape (s, (double) i / pointsPerCycle, 7u));
            if (i >= warm)
                out2Cycles[i - warm] = y;
        }
    }
}
