#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include "TapeEngine.h"

namespace ParamID
{
    juce::String on (int fx);
    juce::String variation (int fx);
    juce::String amount (int fx);
    inline constexpr const char* input  = "input";
    inline constexpr const char* output = "output";
    inline constexpr const char* engage = "engage";
}

class NoviTapeProcessor : public juce::AudioProcessor
{
public:
    NoviTapeProcessor();
    ~NoviTapeProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "NoviTape"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    novitape::TapeEngine engine;

    // Performance holds from the transport keys (message thread -> audio thread)
    std::atomic<bool> stopHold { false }, pauseHold { false };
    std::atomic<int> shuttle { 0 };
    std::atomic<int> selectedFx { 0 };

    // MIDI-held effects, read by the editor to light the keys
    std::array<std::atomic<bool>, novitape::NumFx> midiHeld {};

    // Host info for the faceplate
    std::atomic<double> hostBpm { 120.0 }, hostPpq { 0.0 }, hostSeconds { 0.0 };
    std::atomic<bool> hostPlaying { false };

    // Recorder (writes the plugin's output to a WAV in Music/NoviTape)
    bool startRecording();
    void stopRecording();
    bool isRecording() const noexcept { return activeWriter.load() != nullptr; }
    double recordingSeconds() const;
    juce::File getLastTake() const { const juce::ScopedLock sl (takeLock); return lastTake; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::array<std::atomic<float>*, novitape::NumFx> pOn {}, pVar {}, pAmt {};
    std::atomic<float>* pInput = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pEngage = nullptr;

    juce::AudioBuffer<float> monoScratch;
    double currentRate = 44100.0;

    juce::TimeSliceThread writerThread { "NoviTape recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };
    juce::CriticalSection writerLock, takeLock;
    juce::File currentTake, lastTake;
    std::atomic<juce::int64> recordedSamples { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoviTapeProcessor)
};
