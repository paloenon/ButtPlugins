#include "Core/MidiExport.h"
#include <algorithm>
#include <cmath>

namespace cb
{
namespace
{
double toTicks (double beats) { return std::round (beats * midiExportPPQ); }
}

juce::MidiFile buildMidiFile (const std::vector<NoteEvent>& events, double bpm, std::optional<double> lengthBeats)
{
    if (! (bpm > 0.0))
        bpm = 120.0;

    juce::MidiMessageSequence track;

    auto tempo = juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / bpm));
    tempo.setTimeStamp (0.0);
    track.addEvent (tempo);

    auto sig = juce::MidiMessage::timeSignatureMetaEvent (4, 4);
    sig.setTimeStamp (0.0);
    track.addEvent (sig);

    // With a length, the region ends exactly there (so it loops like the preview) and, as in the
    // preview, notes are clipped to it.
    const bool hasLength = lengthBeats.has_value() && *lengthBeats > 0.0;
    const double lengthTicks = hasLength ? toTicks (*lengthBeats) : 0.0;

    double lastTick = 0.0;
    for (const auto& e : events)
    {
        const auto on  = toTicks (e.startBeat);
        auto off = toTicks (e.startBeat + e.lengthBeats);
        if (hasLength)
        {
            if (on >= lengthTicks)
                continue;
            off = std::min (off, lengthTicks);
        }
        if (off <= on) off = on + 1.0;
        const auto vel = (juce::uint8) juce::jlimit (1, 127, e.velocity);
        const auto note = juce::jlimit (0, 127, e.note);

        auto mOn = juce::MidiMessage::noteOn (1, note, vel);
        mOn.setTimeStamp (on);
        auto mOff = juce::MidiMessage::noteOff (1, note);
        mOff.setTimeStamp (off);
        track.addEvent (mOn);
        track.addEvent (mOff);
        lastTick = std::max (lastTick, off);
    }

    // addEvent does not sort; do it manually so that at equal ticks note-offs come first.
    track.sort();
    // sort() is stable, but insertion put each note-on before later notes' offs; fix ordering explicitly.
    juce::Array<juce::MidiMessage> ordered;
    for (int i = 0; i < track.getNumEvents(); ++i)
        ordered.add (track.getEventPointer (i)->message);
    std::stable_sort (ordered.begin(), ordered.end(), [] (const juce::MidiMessage& a, const juce::MidiMessage& b)
    {
        if (a.getTimeStamp() < b.getTimeStamp()) return true;
        if (b.getTimeStamp() < a.getTimeStamp()) return false;
        auto rank = [] (const juce::MidiMessage& m) { return m.isMetaEvent() ? 0 : (m.isNoteOff() ? 1 : 2); };
        return rank (a) < rank (b);
    });

    juce::MidiMessageSequence sorted;
    for (auto& m : ordered)
        sorted.addEvent (m);
    sorted.updateMatchedPairs();

    auto eot = juce::MidiMessage::endOfTrack();
    eot.setTimeStamp (hasLength ? lengthTicks : lastTick);
    sorted.addEvent (eot);

    juce::MidiFile file;
    file.setTicksPerQuarterNote (midiExportPPQ);
    file.addTrack (sorted);
    return file;
}

juce::File writeTempMidi (const std::vector<NoteEvent>& events, double bpm, const juce::String& baseName,
                          std::optional<double> lengthBeats)
{
    auto name = juce::File::createLegalFileName (baseName.trim());
    if (name.isEmpty())
        name = "ChordButt";

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ChordButt");
    dir.createDirectory();

    auto file = dir.getChildFile (name + ".mid");
    file.deleteFile();

    if (auto out = file.createOutputStream())
    {
        buildMidiFile (events, bpm, lengthBeats).writeTo (*out, 1);
        out->flush();
    }
    return file;
}

double effectiveBpm (std::optional<double> hostBpm)
{
    return (hostBpm.has_value() && *hostBpm > 0.0) ? *hostBpm : 120.0;
}
}
