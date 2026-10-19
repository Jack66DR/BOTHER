#pragma once

// BOTHER's factory presets and where RANDOMISE may wander. Values are real units
// (shapes and beats by their index: SINE 0, TRIANGLE 1, SQUARE 2, RAMP UP 3, RAMP DOWN 4, STEPS 5, WANDER 6;
// beats 6 = 1/4, 9 = 1/8, 10 = 1/8 T, 12 = 1/16).

#include "manners/MannersPresets.h"
#include "PluginProcessor.h"

namespace bother
{
    inline manners::Presets::Setup presetSetup()
    {
        namespace P = ParamIDs;
        manners::Presets::Setup s;
        s.plugin = "BOTHER";
        s.levels = juce::StringArray { P::in, P::out };

        s.factory = {
            { "MILD BOTHER", {} },   // the defaults
            { "SURF SHACK", { { P::shape, 1.0f }, { P::rate, 6.5f }, { P::depth, 0.7f }, { P::edge, 0.15f },
                              { P::drive, 0.35f }, { P::tone, 0.25f } } },
            { "BROWNFACE", { { P::shape, 0.0f }, { P::rate, 4.2f }, { P::depth, 0.75f }, { P::harmonic, 1.0f },
                             { P::drive, 0.25f }, { P::tone, -0.15f }, { P::driftDepth, 0.06f }, { P::driftRate, 0.1f } } },
            { "CHOPPER", { { P::shape, 2.0f }, { P::edge, 0.6f }, { P::sync, 1.0f }, { P::beat, 12.0f }, { P::depth, 1.0f },
                           { P::drive, 0.2f } } },
            { "SEASICK", { { P::shape, 0.0f }, { P::rate, 3.0f }, { P::depth, 0.65f }, { P::edge, -0.3f }, { P::spread, 0.5f },
                           { P::driftDepth, 0.45f }, { P::driftRate, 0.12f }, { P::driftShape, 6.0f }, { P::sway, 0.4f } } },
            { "PANIC ROOM", { { P::shape, 5.0f }, { P::edge, -0.2f }, { P::rate, 9.0f }, { P::depth, 0.9f },
                              { P::driftDepth, 0.7f }, { P::driftRate, 0.45f }, { P::driftShape, 2.0f },
                              { P::grit, 0.4f }, { P::drive, 0.4f } } },
            { "RING THE BELL", { { P::shape, 0.0f }, { P::rate, 55.0f }, { P::depth, 1.0f }, { P::driftDepth, 0.3f },
                                 { P::driftRate, 0.2f }, { P::driftShape, 0.0f }, { P::drive, 0.0f }, { P::mix, 0.7f } } },
            { "WASP", { { P::shape, 4.0f }, { P::edge, 0.5f }, { P::rate, 14.0f }, { P::depth, 0.85f }, { P::grit, 0.55f },
                        { P::drive, 0.6f }, { P::tone, 0.4f }, { P::spread, 0.25f } } },
            { "SLOW BREATHING", { { P::shape, 0.0f }, { P::edge, -0.6f }, { P::rate, 0.35f }, { P::depth, 0.8f },
                                  { P::spread, 1.0f }, { P::drive, 0.1f }, { P::tone, -0.2f } } },
            { "BROKEN AMP", { { P::shape, 2.0f }, { P::edge, -0.45f }, { P::rate, 7.0f }, { P::depth, 0.8f },
                              { P::driftDepth, 0.3f }, { P::driftRate, 0.3f }, { P::driftShape, 5.0f }, { P::sway, 0.7f },
                              { P::drive, 0.55f }, { P::grit, 0.25f }, { P::tone, -0.35f } } },
        };

        s.randomise = {
            { P::shape, -0.49f, 6.49f }, { P::edge, -1.0f, 1.0f }, { P::rate, 0.3f, 16.0f }, { P::depth, 0.3f, 1.0f },
            { P::spread, 0.0f, 1.0f }, { P::harmonic, 0.0f, 1.0f }, { P::driftRate, 0.05f, 2.0f },
            { P::driftDepth, 0.0f, 0.7f }, { P::driftShape, -0.49f, 6.49f }, { P::sway, 0.0f, 1.0f },
            { P::drive, 0.0f, 0.8f }, { P::grit, 0.0f, 0.6f }, { P::tone, -0.6f, 0.6f },
        };
        return s;
    }
}
