#pragma once

#include "Scene.h"

namespace ric
{

struct RandomOptions
{
    float density       = 0.5f;  // how many obstacles
    float chaos         = 0.5f;  // gravity, spin, ball collisions, speed
    bool  lockObstacles = false;
    bool  lockEmitters  = false;
    bool  lockSounds    = false;
    bool  lockWorld     = false;
    bool  keepScale     = false;
};

/** Builds a new playable scene from a seed. Locked parts are kept from `current`. */
Scene randomScene (juce::uint32 seed, const RandomOptions& options, const Scene& current);

/** If the scene stays silent for 10 s, re-aims emitters at obstacles. */
void ensurePlayable (Scene& scene);

/** Small variations around an existing scene (amount 0..1). */
Scene mutateScene (const Scene& scene, juce::uint32 seed, float amount);

} // namespace ric
