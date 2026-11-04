#pragma once

namespace ric
{

/** What the host told us about its transport for this block. */
struct HostInfo
{
    bool   available = false;  // the host provides a tempo (false in the standalone app)
    bool   playing   = false;
    double bpm       = 0.0;
    bool   hasPpq    = false;
    double ppq       = 0.0;
};

/** Per-instance memory between blocks. */
struct TransportState
{
    bool wasPlaying = false;
};

struct TransportDecision
{
    bool   run       = true;   // advance the simulation this block
    double bpm       = 120.0;  // tempo used for emitters, quantize and delay
    double hostBeat  = -1.0;   // >= 0: lock the musical clock to the host position
    bool   reset     = false;  // restart the simulation from its initial state
    bool   following = false;  // the DAW currently drives play/stop and tempo
};

/** DAW Sync rules.
    - Sync off (or no host transport): the plug-in runs on its own Play button and its own tempo.
    - Sync on: it runs only while the DAW plays, follows the DAW tempo and bar position,
      and restarts from the same initial state each time the DAW starts playing. */
inline TransportDecision decideTransport (bool syncEnabled, bool playParam, double internalBpm,
                                          const HostInfo& host, TransportState& state) noexcept
{
    TransportDecision d;
    d.run = playParam;
    d.bpm = internalBpm;

    const bool follow = syncEnabled && host.available;
    if (follow)
    {
        d.following = true;
        if (host.bpm > 0.0) d.bpm = host.bpm;
        d.run = host.playing;
        if (host.playing && host.hasPpq) d.hostBeat = host.ppq;
        if (host.playing && ! state.wasPlaying) d.reset = true;
    }

    state.wasPlaying = follow && host.playing;
    return d;
}

} // namespace ric
