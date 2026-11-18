#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// The whole faceplate is one custom-painted component laid out on a fixed
// 1672 x 941 design grid and scaled to the window size.
class NoviTapeEditor : public juce::AudioProcessorEditor,
                       public juce::DragAndDropContainer,
                       private juce::Timer
{
public:
    explicit NoviTapeEditor (NoviTapeProcessor&);
    ~NoviTapeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;

    static constexpr float designW = 1672.0f, designH = 941.0f;

private:
    enum class Target { None, Key, KnobVar, KnobAmt, SliderIn, SliderOut, Transport, Bay };

    void timerCallback() override;
    void rebuildBackground();
    void paintStatic (juce::Graphics&);
    void paintDynamic (juce::Graphics&);

    juce::Point<float> toDesign (juce::Point<float> p) const;
    Target findTarget (juce::Point<float> d, int& index) const;

    // effects
    bool fxOn (int fx) const;
    void setFxOn (int fx, bool on);
    void pressKey (int i);
    void releaseKey (int i);
    void setVariation (int v);
    void setAmountValue (float a);
    float paramValue (const juce::String& id) const;
    void setParamValue (const juce::String& id, float realValue);
    void beginGesture (const juce::String& id);
    void endGesture (const juce::String& id);

    // transport
    void transportDown (int j);
    void transportUp (int j);

    juce::Font font (const juce::Typeface::Ptr& tf, float height, float kerning = 0.0f) const;

    NoviTapeProcessor& proc;
    juce::Typeface::Ptr tfBold, tfSemi, tfHand, tfMono;

    juce::Image background, knobImage, noiseTile;
    int backgroundWidth = 0;

    // live display state
    float vPkL = 0, vPkR = 0, vHeat = 0, vuDb = -30;
    double angL = 0, angR = 0, lastFrameMs = 0;

    // interaction state
    Target dragTarget = Target::None;
    int dragIndex = -1;
    juce::Point<float> lastDrag;
    float varAccum = 0;
    std::array<bool, novitape::NumFx> keyHeld {}, willUnlatch {};
    std::array<double, novitape::NumFx> keyDownMs {};
    std::array<bool, 6> transportHeld {};
    bool takeDragStarted = false;
    juce::String lastError;
    double errorUntilMs = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoviTapeEditor)
};
