// Small DSP helpers for BOTHER. Header only, no JUCE needed.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace bother
{
    constexpr float kPi = 3.14159265358979f;
    constexpr float kTwoPi = 6.28318530717959f;

    inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

    // A stateless hash: the same cycle number always gives the same random value, so both
    // channels (and the picture on the face) agree on what STEPS and WANDER do, with no shared state.
    inline float cycleRandom (int64_t n, uint32_t seed)
    {
        uint64_t x = (uint64_t) n * 0x9E3779B97F4A7C15ull ^ ((uint64_t) seed << 17 | seed);
        x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ull;
        x ^= x >> 27; x *= 0x94d049bb133111ebull;
        x ^= x >> 31;
        return (float) ((x >> 40) & 0xFFFFFFull) * (2.0f / 16777215.0f) - 1.0f;
    }

    // One-pole low pass with a settable cutoff.
    struct OnePole
    {
        float z = 0.0f, a = 1.0f;
        static float coef (float hz, float sr) { return 1.0f - std::exp (-kTwoPi * std::min (hz, sr * 0.45f) / sr); }
        void setCutoff (float hz, float sr) { a = coef (hz, sr); }
        float process (float x) { z += a * (x - z); return z; }
        void reset (float v = 0.0f) { z = v; }
    };

    // DC blocker.
    struct DcBlock
    {
        float x1 = 0.0f, y1 = 0.0f, r = 0.9995f;
        void prepare (float sr) { r = 1.0f - kTwoPi * 12.0f / sr; }
        float process (float x) { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
        void reset() { x1 = y1 = 0.0f; }
    };

    // Per-sample one-pole glide towards a target (about `ms` to get most of the way there).
    struct Glide
    {
        float v = 0.0f, a = 1.0f, target = 0.0f;
        void prepare (float sr, float ms, float start) { a = 1.0f - std::exp (-1.0f / (0.001f * ms * sr)); v = target = start; }
        void set (float t) { target = t; }
        float next() { v += a * (target - v); return v; }
    };

    // tanh with first-order antiderivative anti-aliasing, so DRIVE can be pushed without fizz.
    struct AdaaTanh
    {
        double x1 = 0.0;
        static double F (double x)   // log (cosh x), written so it never overflows
        {
            const double a = std::abs (x);
            return a + std::log1p (std::exp (-2.0 * a)) - 0.69314718055994531;
        }
        float process (float xf)
        {
            const double x = xf;
            const double d = x - x1;
            double y;
            if (std::abs (d) < 1.0e-5)
                y = std::tanh (0.5 * (x + x1));
            else
                y = (F (x) - F (x1)) / d;
            x1 = x;
            return (float) y;
        }
        void reset() { x1 = 0.0; }
    };

    // Soft ceiling at +-1 for the modulation signal: untouched below 0.85, eased into 1 above it.
    inline float limitUnit (float x)
    {
        constexpr float knee = 0.85f;
        const float a = std::abs (x);
        if (a <= knee)
            return x;
        const float y = knee + (1.0f - knee) * std::tanh ((a - knee) / (1.0f - knee));
        return x < 0.0f ? -y : y;
    }
}
