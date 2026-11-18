#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace novitape;

namespace
{
    const char* const kIds[NumFx] = { "speed", "warble", "saturate", "brake", "reverse", "lofi", "mute", "stutter" };
    const char* const kNames[NumFx] = { "Speed", "Warble", "Saturate", "Brake", "Reverse", "Lo-Fi", "Mute", "Stutter" };
    constexpr int kFirstMidiNote = 60;   // C5 in FL Studio's naming (middle C)
}

juce::String ParamID::on (int fx)        { return juce::String (kIds[fx]) + "_on"; }
juce::String ParamID::variation (int fx) { return juce::String (kIds[fx]) + "_var"; }
juce::String ParamID::amount (int fx)    { return juce::String (kIds[fx]) + "_amt"; }

juce::AudioProcessorValueTreeState::ParameterLayout NoviTapeProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int fx = 0; fx < NumFx; ++fx)
    {
        juce::StringArray vars;
        for (int v = 0; v < 4; ++v) vars.add (juce::String (TapeEngine::variationNames[fx][v]));
        const juce::String name (kNames[fx]);
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ParamID::on (fx), 1 }, name, false));
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamID::variation (fx), 1 }, name + " Variation", vars, 0));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamID::amount (fx), 1 },
                                                                 name + " " + juce::String (TapeEngine::amountNames[fx]).toLowerCase(),
                                                                 juce::NormalisableRange<float> (0.0f, 1.0f), TapeEngine::defaultAmounts[fx]));
    }
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamID::input, 1 }, "Input",
                                                             juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                                                             juce::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamID::output, 1 }, "Output",
                                                             juce::NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f,
                                                             juce::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ParamID::engage, 1 }, "Engage", true));
    return layout;
}

NoviTapeProcessor::NoviTapeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NoviTape", createLayout())
{
    for (int fx = 0; fx < NumFx; ++fx)
    {
        pOn[(size_t) fx]  = apvts.getRawParameterValue (ParamID::on (fx));
        pVar[(size_t) fx] = apvts.getRawParameterValue (ParamID::variation (fx));
        pAmt[(size_t) fx] = apvts.getRawParameterValue (ParamID::amount (fx));
        midiHeld[(size_t) fx].store (false);
    }
    pInput = apvts.getRawParameterValue (ParamID::input);
    pOutput = apvts.getRawParameterValue (ParamID::output);
    pEngage = apvts.getRawParameterValue (ParamID::engage);
    engine.prepare (44100.0);
    writerThread.startThread();
}

NoviTapeProcessor::~NoviTapeProcessor()
{
    stopRecording();
    writerThread.stopThread (2000);
}

void NoviTapeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    if (isRecording()) stopRecording();
    currentRate = sampleRate;
    engine.prepare (sampleRate);
    monoScratch.setSize (1, juce::jmax (1, samplesPerBlock), false, true, true);
    setLatencySamples (0);
}

void NoviTapeProcessor::releaseResources() {}

bool NoviTapeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void NoviTapeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch) buffer.clear (ch, 0, n);

    // MIDI notes 60..67 hold effects 1..8 (momentary)
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int idx = m.getNoteNumber() - kFirstMidiNote;
        if (idx < 0 || idx >= NumFx) continue;
        if (m.isNoteOn()) midiHeld[(size_t) idx].store (true);
        else if (m.isNoteOff()) midiHeld[(size_t) idx].store (false);
    }
    midi.clear();

    EngineInput in;
    for (int fx = 0; fx < NumFx; ++fx)
    {
        auto& s = in.fx[(size_t) fx];
        s.on = pOn[(size_t) fx]->load() > 0.5f || midiHeld[(size_t) fx].load();
        s.variation = juce::jlimit (0, 3, (int) std::lround (pVar[(size_t) fx]->load()));
        s.amount = pAmt[(size_t) fx]->load();
    }
    in.inputGain = juce::Decibels::decibelsToGain (pInput->load());
    in.outputGain = juce::Decibels::decibelsToGain (pOutput->load(), -60.0f);
    in.engaged = pEngage->load() > 0.5f;
    in.stopHold = stopHold.load();
    in.pauseHold = pauseHold.load();
    in.shuttle = shuttle.load();

    in.bpm = 120.0;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) in.bpm = *b;
            in.hostPlaying = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) in.ppqAtBlockStart = *q;
            if (auto s = pos->getTimeInSeconds()) hostSeconds.store (*s);
        }
    }
    hostBpm.store (in.bpm);
    hostPpq.store (in.ppqAtBlockStart);
    hostPlaying.store (in.hostPlaying);

    float* L = buffer.getWritePointer (0);
    float* R = nullptr;
    if (buffer.getNumChannels() > 1) R = buffer.getWritePointer (1);
    else
    {
        if (monoScratch.getNumSamples() < n) monoScratch.setSize (1, n, false, true, true);
        monoScratch.copyFrom (0, 0, buffer, 0, 0, n);
        R = monoScratch.getWritePointer (0);
    }
    engine.process (L, R, n, in);

    if (auto* w = activeWriter.load())
    {
        const float* chans[2] = { L, R };
        w->write (chans, n);
        recordedSamples += n;
    }
}

bool NoviTapeProcessor::startRecording()
{
    const juce::ScopedLock sl (writerLock);
    if (activeWriter.load() != nullptr) return true;

    auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("NoviTape");
    if (! dir.createDirectory()) return false;
    auto file = dir.getNonexistentChildFile ("NoviTape take " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M%S"), ".wav", false);

    auto stream = std::make_unique<juce::FileOutputStream> (file);
    if (! stream->openedOk()) return false;

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), currentRate, 2, 24, {}, 0));
    if (writer == nullptr) return false;
    stream.release();   // the writer owns the stream now

    threadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer.release(), writerThread, 32768);
    currentTake = file;
    recordedSamples = 0;
    activeWriter.store (threadedWriter.get());
    return true;
}

void NoviTapeProcessor::stopRecording()
{
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> finished;
    {
        const juce::ScopedLock sl (writerLock);
        if (activeWriter.load() == nullptr) return;
        activeWriter.store (nullptr);
        finished = std::move (threadedWriter);
    }
    // Let any block that is mid-write finish before the writer is flushed and closed.
    juce::Thread::sleep (60);
    finished.reset();
    const juce::ScopedLock sl (takeLock);
    lastTake = currentTake;
}

double NoviTapeProcessor::recordingSeconds() const
{
    return currentRate > 0 ? (double) recordedSamples.load() / currentRate : 0.0;
}

void NoviTapeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("selectedFx", selectedFx.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void NoviTapeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            selectedFx.store (juce::jlimit (0, NumFx - 1, (int) tree.getProperty ("selectedFx", 0)));
            apvts.replaceState (tree);
        }
    }
}

juce::AudioProcessorEditor* NoviTapeProcessor::createEditor() { return new NoviTapeEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NoviTapeProcessor(); }
