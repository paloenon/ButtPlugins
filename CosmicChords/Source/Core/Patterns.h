#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace cb
{
enum class PlayMode { Held, Rhythm, Arp };
enum class RhythmPattern { Stabs, Offbeat, Pulse8, Charleston, Push };
enum class ArpPattern { Cascade, Pendulum, BrokenThirds, OctaveLeap, Gallop, Syncopated16, Strum };
enum class Rate { Quarter, Eighth, EighthTriplet, Sixteenth, SixteenthTriplet };  // 1, 1/2, 1/3, 1/4, 1/6 beats

struct StyleParams
{
    PlayMode mode = PlayMode::Held;
    RhythmPattern rhythm = RhythmPattern::Stabs;
    ArpPattern arp = ArpPattern::Cascade;
    Rate rate = Rate::Sixteenth;
    float gate = 0.8f;
    float swing = 0.0f;
    int arpOctaves = 1;
    float humanize = 0.0f;
    uint32_t seed = 1;
};

struct NoteEvent { double startBeat; double lengthBeats; int note; int velocity; };
struct VoicedSlot { std::vector<int> notes; double lengthBeats; };

// Deterministic. Output sorted by (startBeat, note).
std::vector<NoteEvent> generateEvents (const std::vector<VoicedSlot>&, const StyleParams&);

std::string rhythmName (RhythmPattern);
std::string arpName (ArpPattern);
std::string rateName (Rate);
} // namespace cb
