#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"
#include "Core/MidiExport.h"
#include <algorithm>
#include <memory>

namespace
{
const char* const paramIds[] = { "root", "scale", "inversion", "spread", "octave", "complexity", "mode", "rhythm",
                                 "arp", "rate", "gate", "swing", "arpOctaves", "humanize" };

constexpr int numScales = (int) cb::Scale::MelodicMinor + 1;
constexpr int numRhythms = (int) cb::RhythmPattern::Push + 1;
constexpr int numArps = (int) cb::ArpPattern::Strum + 1;
constexpr int numRates = (int) cb::Rate::SixteenthTriplet + 1;

template <typename E, typename NameFn> juce::StringArray names (int count, NameFn fn)
{
    juce::StringArray a;
    for (int i = 0; i < count; ++i) a.add (fn (static_cast<E> (i)));
    return a;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout ChordButtProcessor::createLayout()
{
    using juce::ParameterID;
    using juce::StringArray;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    StringArray roots;
    for (int i = 0; i < 12; ++i) roots.add (cb::noteName (i));

    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "root", 1 }, "Root", roots, 9));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale",
                    names<cb::Scale> (numScales, [] (cb::Scale s) { return cb::scaleName (s); }), (int) cb::Scale::Minor));
    layout.add (std::make_unique<juce::AudioParameterInt> (ParameterID { "inversion", 1 }, "Voicing", 0, 3, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { "spread", 1 }, "Spread",
                    juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterInt> (ParameterID { "octave", 1 }, "Octave", -2, 2, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (ParameterID { "complexity", 1 }, "Complexity", 0, 3, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
                    StringArray { "Chords", "Rhythm", "Arp" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "rhythm", 1 }, "Rhythm",
                    names<cb::RhythmPattern> (numRhythms, [] (cb::RhythmPattern r) { return cb::rhythmName (r); }), 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "arp", 1 }, "Arp",
                    names<cb::ArpPattern> (numArps, [] (cb::ArpPattern a) { return cb::arpName (a); }), 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "rate", 1 }, "Rate",
                    names<cb::Rate> (numRates, [] (cb::Rate r) { return cb::rateName (r); }), (int) cb::Rate::Sixteenth));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { "gate", 1 }, "Gate",
                    juce::NormalisableRange<float> (0.1f, 1.0f), 0.8f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { "swing", 1 }, "Swing",
                    juce::NormalisableRange<float> (0.0f, 0.6f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterInt> (ParameterID { "arpOctaves", 1 }, "Arp Octaves", 1, 3, 1));
    layout.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { "humanize", 1 }, "Humanize",
                    juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    return layout;
}

ChordButtProcessor::ChordButtProcessor()
    : AudioProcessor (BusesProperties()),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (auto* id : paramIds) apvts.addParameterListener (id, this);
    previewEngine.prepare (44100.0);
    rebuildPreview();
    startTimerHz (30);
}

ChordButtProcessor::~ChordButtProcessor()
{
    stopTimer();
    for (auto* id : paramIds) apvts.removeParameterListener (id, this);
}

void ChordButtProcessor::prepareToPlay (double sampleRate, int)
{
    previewEngine.prepare (sampleRate);
}

void ChordButtProcessor::releaseResources()
{
    previewEngine.allNotesOff();
}

std::optional<double> ChordButtProcessor::readHostPosition()
{
    heartbeat.fetch_add (1, std::memory_order_relaxed);

    std::optional<double> bpm;
    bool hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            hostPlaying = pos->getIsPlaying();
            if (auto b = pos->getBpm())
                if (*b > 0.0)
                {
                    bpm = *b;
                    lastHostBpm.store (*b);
                }
        }

    // The preview is independent of the song: when the host transport starts, stop it (lock-free).
    if (hostPlaying && ! hostWasPlaying)
        previewEngine.requestStop();
    hostWasPlaying = hostPlaying;
    return bpm;
}

void ChordButtProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const auto bpm = readHostPosition();
    previewEngine.process (midi, buffer.getNumSamples(), bpm);
}

void ChordButtProcessor::processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer& midi)
{
    readHostPosition();
    previewEngine.processBypassed (midi);
}

juce::AudioProcessorEditor* ChordButtProcessor::createEditor()
{
    return new ChordButtEditor (*this);
}

// ---- parameter access ----

cb::ParamValues ChordButtProcessor::paramValues() const
{
    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto idx = [&] (const char* id) { return (int) std::lround (raw (id)); };

    cb::ParamValues p;
    p.root = std::clamp (idx ("root"), 0, 11);
    p.scale = static_cast<cb::Scale> (std::clamp (idx ("scale"), 0, numScales - 1));
    p.voicing.inversion = std::clamp (idx ("inversion"), 0, 3);
    p.voicing.spread = raw ("spread");
    p.voicing.octave = std::clamp (idx ("octave"), -2, 2);
    p.complexity = std::clamp (idx ("complexity"), 0, 3);
    p.style.mode = static_cast<cb::PlayMode> (std::clamp (idx ("mode"), 0, 2));
    p.style.rhythm = static_cast<cb::RhythmPattern> (std::clamp (idx ("rhythm"), 0, numRhythms - 1));
    p.style.arp = static_cast<cb::ArpPattern> (std::clamp (idx ("arp"), 0, numArps - 1));
    p.style.rate = static_cast<cb::Rate> (std::clamp (idx ("rate"), 0, numRates - 1));
    p.style.gate = raw ("gate");
    p.style.swing = raw ("swing");
    p.style.arpOctaves = std::clamp (idx ("arpOctaves"), 1, 3);
    p.style.humanize = raw ("humanize");
    p.style.seed = seed;
    return p;
}

std::vector<cb::PadChord> ChordButtProcessor::currentPalette() const
{
    auto p = paramValues();
    return cb::buildPalette (p.scale, p.complexity);
}

std::vector<cb::VoicedSlot> ChordButtProcessor::voicedStrip() const
{
    auto p = paramValues();
    return cb::voiceStrip (stripData, p.root, p.scale, p.complexity, p.voicing);
}

std::vector<cb::NoteEvent> ChordButtProcessor::currentEvents() const
{
    return cb::eventsFor (paramValues(), stripData, seed);
}

cb::StyleParams ChordButtProcessor::currentStyle() const
{
    return paramValues().style;
}

int ChordButtProcessor::tonic() const { return paramValues().root; }
cb::Scale ChordButtProcessor::scale() const { return paramValues().scale; }

double ChordButtProcessor::hostBpm() const
{
    const double b = lastHostBpm.load();
    return cb::effectiveBpm (b > 0.0 ? std::optional<double> (b) : std::nullopt);
}

void ChordButtProcessor::newHumanizeSeed()
{
    juce::Random r;
    uint32_t s = 0;
    while (s == 0) s = (uint32_t) r.nextInt();
    seed = s;
    rebuildPreview();
    stateBroadcaster.sendChangeMessage();
}

// ---- library ----

const cb::Library& ChordButtProcessor::library()
{
    static const cb::Library lib = []
    {
        try
        {
            return cb::Library::fromJson (juce::String::fromUTF8 (BinaryData::library_json, BinaryData::library_jsonSize));
        }
        catch (const std::exception& e)
        {
            DBG ("ChordButt: bundled library failed to load: " << e.what());
            jassertfalse;   // the bundled JSON is covered by LibraryTests; never ship a broken one
            return cb::Library {};
        }
    }();
    return lib;
}

void ChordButtProcessor::loadLibraryEntry (const cb::LibraryEntry& entry)
{
    auto p = paramValues();
    cb::applyLibraryEntry (entry, p, stripData);   // root stays as it is
    if (auto* param = apvts.getParameter ("scale"))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) (int) p.scale));
        param->endChangeGesture();
    }
    stripChanged();
}

// ---- preview wiring ----

void ChordButtProcessor::rebuildPreview()
{
    dirty.store (false);
    auto events = std::make_shared<const std::vector<cb::NoteEvent>> (currentEvents());
    previewEngine.setSequence (events, std::max (stripData.totalBeats(), 1.0), true);
}

void ChordButtProcessor::stripChanged()
{
    rebuildPreview();
    stateBroadcaster.sendChangeMessage();
}

void ChordButtProcessor::parameterChanged (const juce::String&, float)
{
    dirty.store (true);   // may be the audio thread: only set a flag
}

void ChordButtProcessor::timerCallback()
{
    if (dirty.exchange (false))
    {
        rebuildPreview();
        stateBroadcaster.sendChangeMessage();
    }
}

// ---- state ----

void ChordButtProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto model = cb::makeState (paramValues(), stripData, seed);
    auto state = apvts.copyState();
    state.setProperty ("seed", model.getProperty ("seed"), nullptr);
    state.removeChild (state.getChildWithName ("STRIP"), nullptr);
    state.appendChild (model.getChildWithName ("STRIP").createCopy(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void ChordButtProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid() || ! tree.hasType (apvts.state.getType())) return;

    cb::ParamValues ignored;
    cb::readState (tree, ignored, stripData, seed);

    // Parameters missing from an older/partial state go back to their defaults.
    for (auto* id : paramIds)
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->getDefaultValue());

    auto paramsOnly = tree.createCopy();
    paramsOnly.removeChild (paramsOnly.getChildWithName ("STRIP"), nullptr);
    paramsOnly.removeProperty ("seed", nullptr);
    apvts.replaceState (paramsOnly);
    rebuildPreview();
    stateBroadcaster.sendChangeMessage();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChordButtProcessor();
}
