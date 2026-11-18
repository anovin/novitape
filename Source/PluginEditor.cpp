#include "PluginEditor.h"
#include "BinaryData.h"

#include <cmath>

using namespace novitape;
using juce::Colour;
using juce::Rectangle;

namespace
{
    // ---------- palette ----------
    const Colour cRoom    (0xff181614);
    const Colour cTeal    (0xff5db6a6);
    const Colour cTealHi  (0xff72c8b8);
    const Colour cTealLo  (0xff55ab9c);
    const Colour cTealEdge(0xff397a6e);
    const Colour cCream   (0xffede3cc);
    const Colour cCreamHi (0xfff5ecd8);
    const Colour cCreamLo (0xffe1d5ba);
    const Colour cCreamEdge(0xffa89a7e);
    const Colour cCoral   (0xffee6b57);
    const Colour cInk     (0xff2a2724);
    const Colour cEngrave (0xff34312d);
    const Colour cLabel   (0xffefe5cf);

    // ---------- layout (design units, 1672 x 941) ----------
    const Rectangle<float> rKeysHousing (330, 20, 1015, 118);
    const Rectangle<float> rLedBox (40, 95, 255, 210);
    const Rectangle<float> rVuBox (1375, 110, 272, 188);
    const Rectangle<float> rBezel (330, 152, 1015, 522);
    const Rectangle<float> rBay (355, 177, 965, 472);
    const Rectangle<float> rShell (408, 208, 860, 440);
    const Rectangle<float> rCard (418, 218, 840, 318);
    const Rectangle<float> rWindow (490, 338, 695, 192);
    const Rectangle<float> rMidWin (710, 375, 255, 115);
    const Rectangle<float> rNotch (750, 588, 175, 60);
    const Rectangle<float> rTransport (340, 716, 995, 192);
    const Rectangle<float> rSliderIn (40, 788, 275, 34);
    const Rectangle<float> rSliderOut (1358, 788, 282, 34);
    const juce::Point<float> cKnobVar (168, 520), cKnobAmt (1505, 520);
    const float knobR = 108.0f;
    const juce::Point<float> cReelL (618, 434), cReelR (1058, 434);
    const juce::Point<float> vuPivot (1511, 280);
    const float vuRadius = 118.0f, vuSpan = 46.0f;
    const float reelMin = 86.0f, reelMax = 112.0f;
    const float varAngles[4] = { -105.0f, -35.0f, 35.0f, 105.0f };
    const float ledThresholds[7] = { -36, -24, -15, -9, -5, -2, -0.5f };

    Rectangle<float> keyRect (int i)
    {
        const float w = (1015.0f - 36.0f - 7.0f * 12.0f) / 8.0f;
        return { 348.0f + (float) i * (w + 12.0f), 31.0f, w, 84.0f };
    }
    Rectangle<float> transportRect (int j)
    {
        const float w = (995.0f - 34.0f - 5.0f * 15.0f) / 6.0f;
        return { 357.0f + (float) j * (w + 15.0f), 732.0f, w, 144.0f };
    }
    float sliderThumbX (const Rectangle<float>& track, float v) { return track.getX() + (0.04f + v * 0.92f) * track.getWidth(); }

    float vuAngleDeg (float db)
    {
        auto amp = [] (float d) { return std::pow (10.0f, d / 20.0f); };
        db = juce::jlimit (-20.0f, 3.0f, db);
        const float a = (amp (db) - amp (-20.0f)) / (amp (3.0f) - amp (-20.0f));
        return -vuSpan + 2.0f * vuSpan * a;
    }
    juce::Point<float> polar (juce::Point<float> c, float deg, float r)
    {
        const float a = juce::degreesToRadians (deg);
        return { c.x + std::sin (a) * r, c.y - std::cos (a) * r };
    }

    void fillVGrad (juce::Graphics& g, Rectangle<float> r, Colour top, Colour bottom, float radius)
    {
        g.setGradientFill (juce::ColourGradient (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);
    }
    void innerShadow (juce::Graphics& g, Rectangle<float> r, float radius, float depth, float alpha)
    {
        const int steps = 8;
        for (int k = 0; k < steps; ++k)
        {
            const float t = (float) k / (float) steps;
            g.setColour (juce::Colours::black.withAlpha (alpha * (1.0f - t) * 0.35f));
            auto rr = r.reduced (t * depth * 0.5f).withTrimmedBottom (-t * depth * 0.2f);
            g.drawRoundedRectangle (rr.reduced (0.5f), juce::jmax (1.0f, radius - t * depth * 0.5f), depth / (float) steps * 1.6f);
        }
    }
    void drawScrew (juce::Graphics& g, juce::Point<float> c, float r)
    {
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillEllipse (c.x - r, c.y - r + r * 0.12f, r * 2, r * 2);
        juce::ColourGradient grad (Colour (0xffd9d4cc), c.x - r * 0.3f, c.y - r * 0.4f, Colour (0xff5c5853), c.x + r * 0.7f, c.y + r * 0.8f, true);
        grad.addColour (0.45, Colour (0xff9a958d));
        g.setGradientFill (grad);
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        g.setColour (Colour (0xff2d2b28));
        g.drawEllipse (c.x - r + 1.5f, c.y - r + 1.5f, r * 2 - 3, r * 2 - 3, r * 0.1f);
        g.setColour (Colour (0xff1e1c1a));
        const float len = r * 1.24f, th = r * 0.25f;
        for (float a : { 45.0f, -45.0f })
        {
            juce::Path p;
            p.addRoundedRectangle (-len / 2, -th / 2, len, th, th * 0.3f);
            g.fillPath (p, juce::AffineTransform::rotation (juce::degreesToRadians (a)).translated (c));
        }
    }
    void housing (juce::Graphics& g, Rectangle<float> r, float radius)
    {
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.fillRoundedRectangle (r.translated (0, 2), radius);
        fillVGrad (g, r, Colour (0xff2b2927), Colour (0xff3a3734), radius);
        innerShadow (g, r, radius, 12, 1.0f);
    }
    void meterBox (juce::Graphics& g, Rectangle<float> r)
    {
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.fillRoundedRectangle (r.translated (0, 2), 10);
        g.setColour (Colour (0xff1a1918));
        g.fillRoundedRectangle (r, 10);
        g.setColour (Colour (0xff232120));
        g.fillRoundedRectangle (r.reduced (4), 8);
        innerShadow (g, r.reduced (4), 8, 14, 1.0f);
    }
}

// =====================================================================

NoviTapeEditor::NoviTapeEditor (NoviTapeProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    tfBold = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize);
    tfSemi = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
    tfHand = juce::Typeface::createSystemTypefaceFor (BinaryData::ReenieBeanie_ttf, BinaryData::ReenieBeanie_ttfSize);
    tfMono = juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize);

    // grime texture
    noiseTile = juce::Image (juce::Image::ARGB, 256, 256, true);
    {
        juce::Random rnd (7);
        juce::Image::BitmapData bd (noiseTile, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x)
            {
                const float v = rnd.nextFloat();
                const float a = v > 0.55f ? (v - 0.55f) * 0.55f : 0.0f;
                bd.setPixelColour (x, y, Colour (0xff1a1714).withAlpha (a));
            }
    }

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio (designW / designH);
        c->setSizeLimits (836, 470, 1672, 941);
    }
    setSize (1003, 564);
    startTimerHz (30);
}

NoviTapeEditor::~NoviTapeEditor()
{
    stopTimer();
    proc.stopHold = false; proc.pauseHold = false; proc.shuttle = 0;
}

juce::Font NoviTapeEditor::font (const juce::Typeface::Ptr& tf, float height, float kerning) const
{
    return juce::Font (juce::FontOptions (tf)).withHeight (height).withExtraKerningFactor (kerning);
}

void NoviTapeEditor::resized()
{
    if (getWidth() * 2 != backgroundWidth) rebuildBackground();
}

void NoviTapeEditor::timerCallback() { repaint(); }

// ---------------- parameters ----------------

float NoviTapeEditor::paramValue (const juce::String& id) const
{
    if (auto* v = proc.apvts.getRawParameterValue (id)) return v->load();
    return 0.0f;
}
void NoviTapeEditor::setParamValue (const juce::String& id, float realValue)
{
    if (auto* prm = proc.apvts.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (realValue));
}
void NoviTapeEditor::beginGesture (const juce::String& id) { if (auto* prm = proc.apvts.getParameter (id)) prm->beginChangeGesture(); }
void NoviTapeEditor::endGesture (const juce::String& id)   { if (auto* prm = proc.apvts.getParameter (id)) prm->endChangeGesture(); }

bool NoviTapeEditor::fxOn (int fx) const
{
    return paramValue (ParamID::on (fx)) > 0.5f || proc.midiHeld[(size_t) fx].load();
}
void NoviTapeEditor::setFxOn (int fx, bool on)
{
    const auto id = ParamID::on (fx);
    beginGesture (id);
    setParamValue (id, on ? 1.0f : 0.0f);
    endGesture (id);
}

// Hold a key for a momentary hit; tap it to latch, tap again to release.
void NoviTapeEditor::pressKey (int i)
{
    proc.selectedFx = i;
    keyHeld[(size_t) i] = true;
    keyDownMs[(size_t) i] = juce::Time::getMillisecondCounterHiRes();
    if (paramValue (ParamID::on (i)) > 0.5f) willUnlatch[(size_t) i] = true;
    else { willUnlatch[(size_t) i] = false; setFxOn (i, true); }
}
void NoviTapeEditor::releaseKey (int i)
{
    keyHeld[(size_t) i] = false;
    const double held = juce::Time::getMillisecondCounterHiRes() - keyDownMs[(size_t) i];
    if (willUnlatch[(size_t) i]) { willUnlatch[(size_t) i] = false; setFxOn (i, false); }
    else if (held >= 260.0) setFxOn (i, false);
}
void NoviTapeEditor::setVariation (int v)
{
    const int fx = proc.selectedFx.load();
    const auto id = ParamID::variation (fx);
    beginGesture (id);
    setParamValue (id, (float) juce::jlimit (0, 3, v));
    endGesture (id);
}
void NoviTapeEditor::setAmountValue (float a)
{
    setParamValue (ParamID::amount (proc.selectedFx.load()), juce::jlimit (0.0f, 1.0f, a));
}

// ---------------- transport ----------------

void NoviTapeEditor::transportDown (int j)
{
    transportHeld[(size_t) j] = true;
    switch (j)
    {
        case 0: proc.stopHold = true; break;
        case 1: proc.shuttle = -1; break;
        case 2:
        {
            const bool engaged = paramValue (ParamID::engage) > 0.5f;
            beginGesture (ParamID::engage); setParamValue (ParamID::engage, engaged ? 0.0f : 1.0f); endGesture (ParamID::engage);
            break;
        }
        case 3: proc.shuttle = 1; break;
        case 4: proc.pauseHold = true; break;
        case 5:
            if (proc.isRecording()) proc.stopRecording();
            else if (! proc.startRecording())
            {
                lastError = "COULDN'T START RECORDING IN MUSIC/NOVITAPE";
                errorUntilMs = juce::Time::getMillisecondCounterHiRes() + 4000;
            }
            break;
        default: break;
    }
}
void NoviTapeEditor::transportUp (int j)
{
    transportHeld[(size_t) j] = false;
    switch (j)
    {
        case 0: proc.stopHold = false; break;
        case 1: case 3: proc.shuttle = 0; break;
        case 4: proc.pauseHold = false; break;
        default: break;
    }
}

// ---------------- mouse ----------------

juce::Point<float> NoviTapeEditor::toDesign (juce::Point<float> p) const
{
    const float s = (float) getWidth() / designW;
    return { p.x / s, p.y / s };
}

NoviTapeEditor::Target NoviTapeEditor::findTarget (juce::Point<float> d, int& index) const
{
    for (int i = 0; i < NumFx; ++i) if (keyRect (i).expanded (5).contains (d)) { index = i; return Target::Key; }
    for (int j = 0; j < 6; ++j) if (transportRect (j).expanded (6).contains (d)) { index = j; return Target::Transport; }
    if (d.getDistanceFrom (cKnobVar) < knobR + 14) return Target::KnobVar;
    if (d.getDistanceFrom (cKnobAmt) < knobR + 14) return Target::KnobAmt;
    if (rSliderIn.expanded (12, 26).contains (d)) return Target::SliderIn;
    if (rSliderOut.expanded (12, 26).contains (d)) return Target::SliderOut;
    if (rBay.contains (d)) return Target::Bay;
    return Target::None;
}

void NoviTapeEditor::mouseMove (const juce::MouseEvent& e)
{
    int idx = -1;
    const auto t = findTarget (toDesign (e.position), idx);
    const bool canDragTake = t == Target::Bay && proc.getLastTake().existsAsFile() && ! proc.isRecording();
    setMouseCursor (t == Target::None ? juce::MouseCursor::NormalCursor
                    : (t == Target::Bay ? (canDragTake ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor)
                                        : juce::MouseCursor::PointingHandCursor));
}

void NoviTapeEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto d = toDesign (e.position);
    dragIndex = -1;
    dragTarget = findTarget (d, dragIndex);
    lastDrag = d;
    varAccum = 0;
    takeDragStarted = false;

    switch (dragTarget)
    {
        case Target::Key: pressKey (dragIndex); break;
        case Target::Transport: transportDown (dragIndex); break;
        case Target::KnobVar: proc.selectedFx.load(); break;
        case Target::KnobAmt: beginGesture (ParamID::amount (proc.selectedFx.load())); break;
        case Target::SliderIn:
        case Target::SliderOut:
        {
            const bool isIn = dragTarget == Target::SliderIn;
            const auto& track = isIn ? rSliderIn : rSliderOut;
            const float v = juce::jlimit (0.0f, 1.0f, ((d.x - track.getX()) / track.getWidth() - 0.04f) / 0.92f);
            beginGesture (isIn ? ParamID::input : ParamID::output);
            setParamValue (isIn ? ParamID::input : ParamID::output, isIn ? -12.0f + v * 24.0f : -24.0f + v * 30.0f);
            break;
        }
        default: break;
    }
}

void NoviTapeEditor::mouseDrag (const juce::MouseEvent& e)
{
    const auto d = toDesign (e.position);
    const float delta = (d.x - lastDrag.x) - (d.y - lastDrag.y);
    lastDrag = d;

    switch (dragTarget)
    {
        case Target::KnobVar:
        {
            varAccum += delta;
            const int cur = (int) std::lround (paramValue (ParamID::variation (proc.selectedFx.load())));
            if (varAccum > 40) { varAccum = 0; setVariation (cur + 1); }
            else if (varAccum < -40) { varAccum = 0; setVariation (cur - 1); }
            break;
        }
        case Target::KnobAmt:
        {
            const int fx = proc.selectedFx.load();
            const float fine = e.mods.isShiftDown() ? 900.0f : 220.0f;
            setAmountValue (paramValue (ParamID::amount (fx)) + delta / fine);
            break;
        }
        case Target::SliderIn:
        case Target::SliderOut:
        {
            const bool isIn = dragTarget == Target::SliderIn;
            const auto& track = isIn ? rSliderIn : rSliderOut;
            const float v = juce::jlimit (0.0f, 1.0f, ((d.x - track.getX()) / track.getWidth() - 0.04f) / 0.92f);
            setParamValue (isIn ? ParamID::input : ParamID::output, isIn ? -12.0f + v * 24.0f : -24.0f + v * 30.0f);
            break;
        }
        case Target::Bay:
        {
            const auto take = proc.getLastTake();
            if (! takeDragStarted && take.existsAsFile() && ! proc.isRecording() && e.getDistanceFromDragStart() > 6)
            {
                takeDragStarted = true;
                juce::DragAndDropContainer::performExternalDragDropOfFiles ({ take.getFullPathName() }, false, this);
            }
            break;
        }
        default: break;
    }
}

void NoviTapeEditor::mouseUp (const juce::MouseEvent&)
{
    switch (dragTarget)
    {
        case Target::Key: releaseKey (dragIndex); break;
        case Target::Transport: transportUp (dragIndex); break;
        case Target::KnobAmt: endGesture (ParamID::amount (proc.selectedFx.load())); break;
        case Target::SliderIn: endGesture (ParamID::input); break;
        case Target::SliderOut: endGesture (ParamID::output); break;
        default: break;
    }
    dragTarget = Target::None;
    dragIndex = -1;
}

void NoviTapeEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    int idx = -1;
    const auto t = findTarget (toDesign (e.position), idx);
    const int fx = proc.selectedFx.load();
    if (t == Target::KnobAmt) { const auto id = ParamID::amount (fx); beginGesture (id); setParamValue (id, TapeEngine::defaultAmounts[fx]); endGesture (id); }
    if (t == Target::SliderIn)  { beginGesture (ParamID::input);  setParamValue (ParamID::input, 0.0f);  endGesture (ParamID::input); }
    if (t == Target::SliderOut) { beginGesture (ParamID::output); setParamValue (ParamID::output, 0.0f); endGesture (ParamID::output); }
    if (t == Target::Bay) { const auto take = proc.getLastTake(); if (take.existsAsFile()) take.revealToUser(); }
}

void NoviTapeEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int idx = -1;
    const auto t = findTarget (toDesign (e.position), idx);
    const int fx = proc.selectedFx.load();
    const float dir = (w.deltaY != 0 ? w.deltaY : w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
    if (dir == 0) return;
    if (t == Target::KnobVar)
    {
        varAccum += dir;
        if (std::abs (varAccum) > 0.15f) { setVariation ((int) std::lround (paramValue (ParamID::variation (fx))) + (varAccum > 0 ? 1 : -1)); varAccum = 0; }
    }
    else if (t == Target::KnobAmt)
    {
        const auto id = ParamID::amount (fx);
        beginGesture (id); setAmountValue (paramValue (id) + dir * 0.1f); endGesture (id);
    }
}

// ---------------- painting ----------------

void NoviTapeEditor::rebuildBackground()
{
    const int w = juce::jmax (1, getWidth() * 2), h = juce::roundToInt ((float) w * designH / designW);
    backgroundWidth = w;
    background = juce::Image (juce::Image::ARGB, w, h, true);
    {
        juce::Graphics g (background);
        g.addTransform (juce::AffineTransform::scale ((float) w / designW));
        paintStatic (g);
    }

    // knob image (rotated at paint time)
    const int ks = juce::roundToInt (knobR * 2 * (float) w / designW) + 4;
    knobImage = juce::Image (juce::Image::ARGB, ks, ks, true);
    {
        juce::Graphics g (knobImage);
        const float s = (float) ks / (knobR * 2 + 4);
        g.addTransform (juce::AffineTransform::scale (s));
        const juce::Point<float> c (knobR + 2, knobR + 2);
        // knurled skirt
        for (int k = 0; k < 120; ++k)
        {
            const float a0 = juce::MathConstants<float>::twoPi * (float) k / 120.0f;
            const float a1 = juce::MathConstants<float>::twoPi * (float) (k + 1) / 120.0f;
            juce::Path wedge;
            wedge.addPieSegment (c.x - knobR, c.y - knobR, knobR * 2, knobR * 2, a0, a1, 0.0f);
            g.setColour ((k & 1) ? Colour (0xff3a3835) : Colour (0xff181716));
            g.fillPath (wedge);
        }
        g.setColour (Colour (0xff141312));
        g.drawEllipse (c.x - knobR + 1.5f, c.y - knobR + 1.5f, knobR * 2 - 3, knobR * 2 - 3, 3.0f);
        // cap
        const float cr = knobR * 0.74f;
        g.setColour (Colour (0xff121110));
        g.fillEllipse (c.x - cr - 3, c.y - cr - 3, (cr + 3) * 2, (cr + 3) * 2);
        juce::ColourGradient cap (Colour (0xff43403d), c.x - cr * 0.2f, c.y - cr * 0.36f, Colour (0xff1b1a19), c.x + cr, c.y + cr, true);
        cap.addColour (0.62, Colour (0xff232120));
        g.setGradientFill (cap);
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);
        // pointer
        g.setColour (cLabel);
        g.fillRoundedRectangle (c.x - 4, c.y - knobR * 0.9f, 8, knobR * 0.84f, 4);
    }
}

void NoviTapeEditor::paintStatic (juce::Graphics& g)
{
    const Rectangle<float> all (0, 0, designW, designH);
    g.setColour (cRoom);
    g.fillRect (all);

    // faceplate
    const auto plate = all.reduced (2);
    fillVGrad (g, plate, Colour (0xffa09b93), Colour (0xff7d7871), 30);
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addRoundedRectangle (plate, 30);
        g.reduceClipRegion (clip);
        juce::Random rnd (42);
        for (int k = 0; k < 46; ++k)
        {
            const float x = rnd.nextFloat() * designW, y = rnd.nextFloat() * designH, r = 40 + rnd.nextFloat() * 200;
            juce::ColourGradient blot (Colour (0xff332e29).withAlpha (0.05f + rnd.nextFloat() * 0.07f), x, y, Colour (0x00332e29), x + r, y, true);
            g.setGradientFill (blot);
            g.fillEllipse (x - r, y - r * 0.7f, r * 2, r * 1.4f);
        }
        for (int k = 0; k < 70; ++k)
        {
            const float x = rnd.nextFloat() * designW, y = rnd.nextFloat() * designH, len = 10 + rnd.nextFloat() * 60;
            const float a = rnd.nextFloat() * 3.1416f;
            g.setColour (juce::Colours::white.withAlpha (0.05f + rnd.nextFloat() * 0.06f));
            g.drawLine (x, y, x + std::cos (a) * len, y + std::sin (a) * len, 0.8f);
        }
        g.setTiledImageFill (noiseTile, 0, 0, 1.0f);
        g.fillRect (all);
    }
    g.setColour (Colour (0xff2b2926));
    g.drawRoundedRectangle (plate.reduced (3.5f), 28, 7);
    g.setColour (Colour (0xff6d6862));
    g.drawRoundedRectangle (plate.reduced (8), 24, 2);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawLine (40, 12, designW - 40, 12, 2);

    // groove between upper and lower plate
    g.setColour (Colour (0xff2f2d2a)); g.fillRect (14.0f, 699.0f, 1644.0f, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.25f)); g.fillRect (14.0f, 703.0f, 1644.0f, 2.0f);

    for (auto c : { juce::Point<float> (40, 42), { 1632, 42 }, { 40, 894 }, { 1632, 894 } }) drawScrew (g, c, 28);

    // housings
    housing (g, rKeysHousing, 16);
    housing (g, rTransport, 16);

    // LED meter box and labels
    meterBox (g, rLedBox);
    g.setFont (font (tfBold, 17, 0.16f));
    const char* ledLabels[3] = { "L", "R", "HEAT" };
    for (int c = 0; c < 3; ++c)
    {
        const Rectangle<float> lr (60.0f + (float) c * 77.0f, 312.0f, 61.0f, 24.0f);
        g.setColour (juce::Colours::white.withAlpha (0.28f)); g.drawText (ledLabels[c], lr.translated (0, 1), juce::Justification::centred);
        g.setColour (cEngrave); g.drawText (ledLabels[c], lr, juce::Justification::centred);
    }

    // VU meter scale
    meterBox (g, rVuBox);
    {
        juce::Path arc1, arc2;
        arc1.addCentredArc (vuPivot.x, vuPivot.y, vuRadius, vuRadius, 0, juce::degreesToRadians (-vuSpan), juce::degreesToRadians (vuAngleDeg (0)), true);
        arc2.addCentredArc (vuPivot.x, vuPivot.y, vuRadius, vuRadius, 0, juce::degreesToRadians (vuAngleDeg (0)), juce::degreesToRadians (vuSpan), true);
        g.setColour (cLabel); g.strokePath (arc1, juce::PathStrokeType (3.0f));
        g.setColour (Colour (0xffef5d48)); g.strokePath (arc2, juce::PathStrokeType (9.0f));
        g.setFont (font (tfMono, 13));
        for (int db : { -20, -10, -7, -5, -3, -1, 0, 1, 2, 3 })
        {
            const float a = vuAngleDeg ((float) db);
            const bool major = db == -20 || db == -10 || db == -5 || db == 0 || db == 3;
            g.setColour (db > 0 ? Colour (0xffef5d48) : cLabel);
            g.drawLine (juce::Line<float> (polar (vuPivot, a, vuRadius + 2), polar (vuPivot, a, vuRadius + (major ? 20.0f : 12.0f))), major ? 4.0f : 2.5f);
            if (major)
            {
                const auto p = polar (vuPivot, a, vuRadius - 18);
                g.setColour (db > 0 ? Colour (0xffef5d48) : Colour (0xffcfc5ae));
                g.drawText (db > 0 ? "+" + juce::String (db) : juce::String (db), Rectangle<float> (p.x - 20, p.y - 8, 40, 16), juce::Justification::centred);
            }
        }
        g.setColour (Colour (0xff8f877a));
        g.setFont (font (tfSemi, 15, 0.2f));
        g.drawText ("VU", Rectangle<float> (vuPivot.x - 40, vuPivot.y - 62, 80, 18), juce::Justification::centred);
    }
    g.setFont (font (tfBold, 17, 0.16f));
    g.setColour (juce::Colours::white.withAlpha (0.28f)); g.drawText ("OUTPUT VU", Rectangle<float> (1375, 308, 272, 24), juce::Justification::centred);
    g.setColour (cEngrave); g.drawText ("OUTPUT VU", Rectangle<float> (1375, 307, 272, 24), juce::Justification::centred);

    // cassette bay
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (rBezel.translated (0, 3), 24);
    fillVGrad (g, rBezel, Colour (0xff5c5955), Colour (0xff46433f), 24);
    g.setColour (Colour (0xff252321)); g.fillRoundedRectangle (rBay, 16);
    innerShadow (g, rBay, 16, 22, 1.2f);
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (rShell.translated (0, 5), 14);
    g.setColour (Colour (0xff3a3835)); g.fillRoundedRectangle (rShell, 14);

    // label card with stripes
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addRoundedRectangle (rCard, 10);
        g.reduceClipRegion (clip);
        g.setColour (cCream); g.fillRect (rCard);
        g.setColour (Colour (0xffee6a56)); g.fillRect (rCard.getX(), rCard.getY() + 40, rCard.getWidth(), 39.0f);
        g.setColour (Colour (0xff5ab5a4)); g.fillRect (rCard.getX(), rCard.getY() + 79, rCard.getWidth(), 38.0f);
        g.setTiledImageFill (noiseTile, 0, 0, 0.35f);
        g.fillRect (rCard);
    }
    g.setColour (Colour (0xff24211f));
    g.fillRoundedRectangle (rWindow, 96);

    // foot, holes and counter notch
    {
        juce::Path foot;
        foot.startNewSubPath (520, 536); foot.lineTo (1155, 536); foot.lineTo (1190, 648); foot.lineTo (485, 648); foot.closeSubPath();
        g.setColour (Colour (0xff353330)); g.fillPath (foot);
        for (auto c : { juce::Point<float> (582, 616), { 1100, 616 } })
        {
            g.setColour (Colour (0xff141312)); g.fillEllipse (c.x - 18, c.y - 18, 36, 36);
        }
        g.setColour (Colour (0xff1c1a19)); g.fillRoundedRectangle (rNotch, 6);
    }

    // knob scales
    g.setColour (cLabel);
    for (int k = 0; k < 4; ++k)
    {
        g.drawLine (juce::Line<float> (polar (cKnobVar, varAngles[k], 120), polar (cKnobVar, varAngles[k], 138)), 6.0f);
        const auto p = polar (cKnobVar, varAngles[k], 158);
        g.setFont (font (tfBold, 20));
        g.setColour (Colour (0xff2e2b28));
        g.drawText (juce::StringArray { "I", "II", "III", "IV" }[k], Rectangle<float> (p.x - 20, p.y - 11, 40, 22), juce::Justification::centred);
        g.setColour (cLabel);
    }
    for (float a = -135.0f; a <= 135.0f; a += 22.5f)
    {
        bool nearDetent = false;
        for (float v : varAngles) if (std::abs (v - a) < 12) nearDetent = true;
        if (nearDetent) continue;
        g.setColour (cLabel.withAlpha (0.6f));
        g.drawLine (juce::Line<float> (polar (cKnobVar, a, 122), polar (cKnobVar, a, 131)), 3.0f);
    }
    for (int k = 0; k <= 10; ++k)
    {
        const float a = -135.0f + (float) k * 27.0f;
        const bool major = k % 5 == 0;
        g.setColour (k >= 9 ? Colour (0xffef5d48) : cLabel);
        g.drawLine (juce::Line<float> (polar (cKnobAmt, a, 120), polar (cKnobAmt, a, major ? 142.0f : 134.0f)), major ? 6.0f : 4.0f);
    }
    g.setFont (font (tfBold, 17, 0.16f));
    g.setColour (juce::Colours::white.withAlpha (0.28f)); g.drawText ("VARIATION", Rectangle<float> (28, 639, 280, 22), juce::Justification::centred);
    g.setColour (cEngrave); g.drawText ("VARIATION", Rectangle<float> (28, 638, 280, 22), juce::Justification::centred);

    // slider ticks, tracks and captions
    for (auto x0 : { 55.0f, 1373.0f })
    {
        const float span = x0 < 100 ? 245.0f : 252.0f;
        for (int k = 0; k < 11; ++k)
        {
            const float x = x0 + span * (float) k / 10.0f, h = (k % 5 == 0) ? 24.0f : 13.0f;
            g.setColour (cLabel.withAlpha (0.85f));
            g.fillRoundedRectangle (x - 1.5f, 766.0f - h, 3.0f, h, 1.0f);
        }
    }
    for (auto* tr : { &rSliderIn, &rSliderOut })
    {
        g.setColour (juce::Colours::white.withAlpha (0.22f)); g.fillRoundedRectangle (tr->translated (0, 2), 17);
        g.setColour (Colour (0xff1d1c1a)); g.fillRoundedRectangle (*tr, 17);
        innerShadow (g, *tr, 17, 8, 1.4f);
    }
    g.setFont (font (tfBold, 17, 0.16f));
    g.setColour (juce::Colours::white.withAlpha (0.28f));
    g.drawText ("INPUT", Rectangle<float> (82, 853, 120, 24), juce::Justification::centredLeft);
    g.drawText ("OUTPUT", Rectangle<float> (1358, 853, 120, 24), juce::Justification::centredLeft);
    g.setColour (cEngrave);
    g.drawText ("INPUT", Rectangle<float> (82, 852, 120, 24), juce::Justification::centredLeft);
    g.drawText ("OUTPUT", Rectangle<float> (1358, 852, 120, 24), juce::Justification::centredLeft);
}

void NoviTapeEditor::paint (juce::Graphics& g)
{
    if (background.isNull() || backgroundWidth != getWidth() * 2) rebuildBackground();
    g.drawImage (background, getLocalBounds().toFloat());
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / designW));
    paintDynamic (g);
}

void NoviTapeEditor::paintDynamic (juce::Graphics& g)
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt = lastFrameMs > 0 ? juce::jlimit (0.0, 0.1, (now - lastFrameMs) / 1000.0) : 0.033;
    lastFrameMs = now;
    const int sel = proc.selectedFx.load();
    const bool engaged = paramValue (ParamID::engage) > 0.5f;

    // ---------- effect keys ----------
    for (int i = 0; i < NumFx; ++i)
    {
        const bool teal = (i == 0 || i == 3 || i == 4 || i == 7);
        const bool on = fxOn (i);
        const bool pressed = on || keyHeld[(size_t) i];
        auto r = keyRect (i);
        const float edge = pressed ? 2.0f : 7.0f;
        auto face = r.translated (0, pressed ? 5.0f : 0.0f);
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (face.translated (0, edge + 3), 7);
        g.setColour (teal ? cTealEdge : cCreamEdge); g.fillRoundedRectangle (face.translated (0, edge), 7);
        fillVGrad (g, face, teal ? cTealHi : cCreamHi, teal ? cTealLo : cCreamLo, 7);
        g.setColour (juce::Colours::white.withAlpha (pressed ? 0.15f : 0.4f));
        g.drawLine (face.getX() + 6, face.getY() + 1.5f, face.getRight() - 6, face.getY() + 1.5f, 2.0f);

        g.setColour (cInk.withAlpha (0.55f));
        g.setFont (font (tfMono, 17));
        g.drawText (juce::String (i + 1), Rectangle<float> (face.getX() + 10, face.getY() + 6, 30, 18), juce::Justification::centredLeft);
        g.setColour (cInk);
        g.setFont (font (tfBold, 21, 0.04f));
        g.drawText (TapeEngine::effectNames[i], Rectangle<float> (face.getX() + 9, face.getBottom() - 32, face.getWidth() - 12, 26), juce::Justification::centredLeft);

        // LED: coral while held, amber when latched on
        const auto led = Rectangle<float> (face.getRight() - 24, face.getY() + 10, 13, 13);
        const bool latched = on && ! keyHeld[(size_t) i] && ! proc.midiHeld[(size_t) i].load();
        if (on)
        {
            const Colour lc = latched ? Colour (0xffffd27a) : Colour (0xffff7a62);
            g.setColour (lc.withAlpha (0.35f)); g.fillEllipse (led.expanded (6));
            g.setColour (lc); g.fillEllipse (led);
        }
        else
        {
            g.setColour (Colour (0xff4a3f3a)); g.fillEllipse (led);
        }
        if (i == sel)
        {
            g.setColour (cCoral.withAlpha (0.45f));
            g.fillRoundedRectangle (r.getCentreX() - r.getWidth() * 0.27f, 128.0f, r.getWidth() * 0.54f, 6.0f, 3.0f);
            g.setColour (cCoral);
            g.fillRoundedRectangle (r.getCentreX() - r.getWidth() * 0.25f, 129.0f, r.getWidth() * 0.5f, 4.0f, 2.0f);
        }
    }

    // ---------- meters ----------
    const float pL = proc.engine.meters.peakL.exchange (0.0f), pR = proc.engine.meters.peakR.exchange (0.0f);
    vPkL = juce::jmax (pL, vPkL * (float) std::exp (-dt * 4.0));
    vPkR = juce::jmax (pR, vPkR * (float) std::exp (-dt * 4.0));
    vHeat += (proc.engine.meters.heat.load() - vHeat) * (float) juce::jmin (1.0, dt * 8.0);
    int levels[3];
    for (int c = 0; c < 2; ++c)
    {
        const float db = juce::Decibels::gainToDecibels (c == 0 ? vPkL : vPkR, -100.0f);
        int lit = 0; for (float t : ledThresholds) if (db >= t) ++lit;
        levels[c] = lit;
    }
    levels[2] = juce::jlimit (0, 7, juce::roundToInt (vHeat / 2.2f * 7.0f));
    for (int c = 0; c < 3; ++c)
    {
        const float x = 60.0f + (float) c * 77.0f, segH = (172.0f - 30.0f) / 7.0f;
        for (int s = 0; s < 7; ++s)
        {
            const Rectangle<float> seg (x, 115.0f + 172.0f - (float) (s + 1) * segH - (float) s * 5.0f + 5.0f, 61.0f, segH);
            const bool red = c < 2 ? s >= 4 : s >= 3;
            if (s < levels[c])
            {
                const Colour lc = red ? Colour (0xfff2604a) : Colour (0xff5fc1ae);
                g.setColour (lc.withAlpha (0.25f)); g.fillRoundedRectangle (seg.expanded (3), 5);
                g.setColour (lc); g.fillRoundedRectangle (seg, 3);
            }
            else { g.setColour (Colour (0xff3a3937)); g.fillRoundedRectangle (seg, 3); }
        }
    }
    const float rms = proc.engine.meters.rms.load();
    const float vuTarget = juce::Decibels::gainToDecibels (rms, -100.0f) + 14.0f;
    vuDb += (vuTarget - vuDb) * (float) (1.0 - std::exp (-dt / 0.3));
    {
        const float a = vuAngleDeg (vuDb);
        g.setColour (cLabel);
        g.drawLine (juce::Line<float> (vuPivot, polar (vuPivot, a, 138)), 3.5f);
        juce::Path cap;
        cap.addCentredArc (vuPivot.x, vuPivot.y + 8, 30, 22, 0, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        cap.closeSubPath();
        g.setColour (Colour (0xff141312)); g.fillPath (cap);
    }

    // ---------- cassette label text ----------
    const double bpm = proc.hostBpm.load();
    const bool hostPlaying = proc.hostPlaying.load();
    const bool recording = proc.isRecording();
    const auto take = proc.getLastTake();
    {
        juce::String label = recording ? "Recording to Music/NoviTape..."
                           : take.existsAsFile() ? "Take saved: drag the tape into FL"
                           : "Side A: live tape";
        g.setColour (Colour (0xff2f2a26));
        g.setFont (font (tfHand, 40));
        g.drawText (label, Rectangle<float> (rCard.getX() + 22, rCard.getY() + 2, 560, 40), juce::Justification::centredLeft, true);
        const double secs = proc.hostSeconds.load();
        const juce::String tm = juce::String ((int) secs / 60) + ":" + juce::String ((int) secs % 60).paddedLeft ('0', 2);
        g.setColour (Colour (0xff5a5048));
        g.setFont (font (tfMono, 21));
        g.drawText (tm, Rectangle<float> (rCard.getRight() - 200, rCard.getY() + 4, 178, 36), juce::Justification::centredRight);

        const int v = (int) std::lround (paramValue (ParamID::variation (sel)));
        const float amt = paramValue (ParamID::amount (sel));
        const bool selOn = fxOn (sel);
        juce::String mode = juce::String (TapeEngine::effectNames[sel]) + "  -  " + TapeEngine::variationNames[sel][juce::jlimit (0, 3, v)]
                          + "  -  " + TapeEngine::amountNames[sel] + " " + juce::String (juce::roundToInt (amt * 100)) + "%";
        if (selOn) mode += keyHeld[(size_t) sel] || proc.midiHeld[(size_t) sel].load() ? "  -  ON" : "  -  LATCHED";
        g.setColour (Colour (0xfffff6e6));
        g.setFont (font (tfBold, 25, 0.12f));
        g.drawText (mode, Rectangle<float> (rCard.getX() + 22, rCard.getY() + 40, rCard.getWidth() - 44, 39), juce::Justification::centredLeft, true);

        juce::String state;
        if (now < errorUntilMs) state = lastError;
        else
        {
            juce::StringArray active;
            for (int i = 0; i < NumFx; ++i) if (fxOn (i)) active.add (TapeEngine::effectNames[i]);
            const float lag = proc.engine.meters.lagSeconds.load();
            state = recording ? "REC " + juce::String (proc.recordingSeconds(), 1) + "S"
                  : ! engaged ? "BYPASSED"
                  : proc.shuttle.load() < 0 ? "REWIND"
                  : proc.shuttle.load() > 0 ? "CUE"
                  : proc.stopHold.load() ? "TAPE STOP"
                  : proc.pauseHold.load() ? "PAUSE"
                  : lag > 0.002f ? "TAPE LAG " + juce::String (lag, 2) + "S"
                  : "LIVE";
            state += "  -  " + juce::String (juce::roundToInt (bpm)) + " BPM  -  FX " + (active.isEmpty() ? juce::String ("OFF") : active.joinIntoString (" + "));
        }
        g.setColour (Colour (0xff173a34));
        g.setFont (font (tfSemi, 22, 0.12f));
        g.drawText (state, Rectangle<float> (rCard.getX() + 22, rCard.getY() + 79, rCard.getWidth() - 44, 38), juce::Justification::centredLeft, true);
    }

    // ---------- reels ----------
    const float signal = rms;
    const bool rolling = engaged && (hostPlaying || signal > 0.0005f || recording);
    const double rate = rolling ? (double) proc.engine.meters.rate.load() : 0.0;
    const double ppq = proc.hostPpq.load();
    const float progress = (float) std::fmod (juce::jmax (0.0, ppq), 128.0) / 128.0f;   // a "side" lasts 32 bars
    const float rL = std::sqrt (reelMin * reelMin + (reelMax * reelMax - reelMin * reelMin) * (1.0f - progress));
    const float rR = std::sqrt (reelMin * reelMin + (reelMax * reelMax - reelMin * reelMin) * progress);
    angL += rate * dt * 260.0 * (reelMin / rL);
    angR += rate * dt * 260.0 * (reelMin / rR);
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path win; win.addRoundedRectangle (rWindow, 96);
        g.reduceClipRegion (win);
        for (auto [c, r] : { std::pair<juce::Point<float>, float> { cReelL, rL }, { cReelR, rR } })
        {
            g.setColour (Colour (0xff5e3e2e));
            g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
            g.setColour (Colour (0xff4f3224));
            for (float rr = 88; rr < r; rr += 4.0f) g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 1.2f);
        }
        g.setColour (Colour (0xffd8cdb3));
        g.fillRect (cReelL.x, 516.0f, cReelR.x - cReelL.x, 5.0f);
        for (auto [c, ang] : { std::pair<juce::Point<float>, double> { cReelL, angL }, { cReelR, angR } })
        {
            g.setColour (Colour (0xffe9dfc8)); g.fillEllipse (c.x - 84, c.y - 84, 168, 168);
            g.setColour (Colour (0xffbdb196)); g.drawEllipse (c.x - 83, c.y - 83, 166, 166, 3);
            g.setColour (Colour (0xffc9bea4)); g.fillEllipse (c.x - 66, c.y - 66, 132, 132);
            g.setColour (Colour (0xff1d1b1a)); g.fillEllipse (c.x - 60, c.y - 60, 120, 120);
            g.setColour (Colour (0xffe9dfc8));
            for (int k = 0; k < 6; ++k)
            {
                juce::Path tooth; tooth.addRoundedRectangle (-9.0f, -58.0f, 18.0f, 22.0f, 2.0f);
                g.fillPath (tooth, juce::AffineTransform::rotation ((float) juce::degreesToRadians (ang + k * 60.0)).translated (c));
            }
            g.setColour (Colour (0xff1d1b1a)); g.fillEllipse (c.x - 30, c.y - 30, 60, 60);
        }
        g.setColour (Colour (0xff1c1a19));
        g.drawRoundedRectangle (rMidWin, 8, 6);
    }

    // glass glare over the bay
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path bayClip; bayClip.addRoundedRectangle (rBay, 16);
        g.reduceClipRegion (bayClip);
        auto glare = [&] (float x0, float w, float alpha)
        {
            juce::Path p;
            p.startNewSubPath (x0, rBay.getBottom()); p.lineTo (x0 + w, rBay.getBottom());
            p.lineTo (x0 + w + 260, rBay.getY()); p.lineTo (x0 + 260, rBay.getY()); p.closeSubPath();
            g.setColour (juce::Colours::white.withAlpha (alpha)); g.fillPath (p);
        };
        glare (330, 62, 0.07f);
        glare (930, 45, 0.05f);
    }
    for (auto c : { juce::Point<float> (405, 213), { 1270, 213 }, { 405, 612 }, { 1270, 612 } }) drawScrew (g, c, 19);

    // tape counter (counts "turns" of the take-up reel)
    {
        const int count = juce::jmax (0, (int) std::floor (ppq * 1.7)) % 1000;
        const juce::String digits = juce::String (count).paddedLeft ('0', 3);
        g.setFont (font (tfMono, 26));
        for (int k = 0; k < 3; ++k)
        {
            const Rectangle<float> d (rNotch.getCentreX() - 54.0f + (float) k * 37.0f, rNotch.getY() + 13, 32, 34);
            g.setColour (Colour (0xff2e2b29)); g.fillRoundedRectangle (d, 3);
            g.setColour (Colour (0xffefe6d2)); g.drawText (digits.substring (k, k + 1), d, juce::Justification::centred);
        }
    }

    // ---------- knobs ----------
    {
        const int v = juce::jlimit (0, 3, (int) std::lround (paramValue (ParamID::variation (sel))));
        const float amt = paramValue (ParamID::amount (sel));
        auto drawKnob = [&] (juce::Point<float> c, float angleDeg)
        {
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillEllipse (c.x - knobR, c.y - knobR + 12, knobR * 2, knobR * 2);
            const float s = (knobR * 2 + 4) / (float) knobImage.getWidth();
            g.drawImageTransformed (knobImage,
                juce::AffineTransform::scale (s)
                    .translated (-(knobR + 2), -(knobR + 2))
                    .rotated (juce::degreesToRadians (angleDeg))
                    .translated (c), false);
        };
        drawKnob (cKnobVar, varAngles[v]);
        drawKnob (cKnobAmt, -135.0f + amt * 270.0f);

        g.setColour (Colour (0xff22201d));
        g.setFont (font (tfBold, 23, 0.1f));
        g.drawText (TapeEngine::variationNames[sel][v], Rectangle<float> (28, 662, 280, 28), juce::Justification::centred);
        g.drawText (juce::String (juce::roundToInt (amt * 100)) + "%", Rectangle<float> (1365, 662, 280, 28), juce::Justification::centred);
        g.setFont (font (tfBold, 17, 0.16f));
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.drawText (TapeEngine::amountNames[sel], Rectangle<float> (1365, 639, 280, 22), juce::Justification::centred);
        g.setColour (cEngrave);
        g.drawText (TapeEngine::amountNames[sel], Rectangle<float> (1365, 638, 280, 22), juce::Justification::centred);
    }

    // ---------- sliders ----------
    {
        const float inDb = paramValue (ParamID::input), outDb = paramValue (ParamID::output);
        const float vIn = (inDb + 12.0f) / 24.0f, vOut = (outDb + 24.0f) / 30.0f;
        for (auto [track, v] : { std::pair<Rectangle<float>, float> { rSliderIn, vIn }, { rSliderOut, vOut } })
        {
            const float cx = sliderThumbX (track, v), cy = track.getCentreY();
            const Rectangle<float> t (cx - 38, cy - 35, 76, 70);
            g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (t.translated (0, 10), 7);
            g.setColour (Colour (0xff8f846f)); g.fillRoundedRectangle (t.translated (0, 6), 7);
            g.setColour (Colour (0xffe7dcc4)); g.fillRoundedRectangle (t, 7);
            g.setColour (Colour (0xffcfc3a9)); g.drawRoundedRectangle (t.reduced (1.5f), 6, 3);
            for (float x = t.getX() + 12; x < t.getRight() - 14; x += 9)
            {
                g.setColour (Colour (0xffb9ad95)); g.fillRect (x + 6, t.getY() + 14, 3.0f, t.getHeight() - 28);
            }
        }
        g.setFont (font (tfBold, 17, 0.1f));
        g.setColour (cEngrave);
        g.drawText ((inDb >= 0 ? "+" : "") + juce::String (inDb, 1) + " DB", Rectangle<float> (180, 852, 135, 24), juce::Justification::centredRight);
        g.drawText ((outDb >= 0 ? "+" : "") + juce::String (outDb, 1) + " DB", Rectangle<float> (1460, 852, 134, 24), juce::Justification::centredRight);
    }

    // ---------- transport ----------
    for (int j = 0; j < 6; ++j)
    {
        auto r = transportRect (j);
        const bool held = transportHeld[(size_t) j];
        const bool litTeal = j == 2 && engaged;
        const bool rec = j == 5 && recording;
        const float edge = held ? 4.0f : 10.0f;
        auto face = r.translated (0, held ? 6.0f : 0.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (face.translated (0, edge + 4), 9);
        g.setColour (litTeal ? Colour (0xff33705f) : rec ? Colour (0xff3d1a15) : Colour (0xff262421));
        g.fillRoundedRectangle (face.translated (0, edge), 9);
        if (litTeal) fillVGrad (g, face, Colour (0xff6fc6b6), Colour (0xff53a999), 9);
        else if (rec)
        {
            const float pulse = 0.5f + 0.5f * (float) std::sin (now / 1000.0 * juce::MathConstants<double>::twoPi);
            fillVGrad (g, face, Colour (0xff7a3a31).brighter (0.35f * pulse), Colour (0xff5c2a23), 9);
        }
        else fillVGrad (g, face, Colour (0xff5d5955), Colour (0xff46423e), 9);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawLine (face.getX() + 8, face.getY() + 1.5f, face.getRight() - 8, face.getY() + 1.5f, 2);

        const auto c = face.getCentre();
        const float s = face.getWidth() * 0.34f;
        juce::Path icon;
        switch (j)
        {
            case 0: icon.addRoundedRectangle (c.x - s * 0.33f, c.y - s * 0.33f, s * 0.66f, s * 0.66f, 2.0f); break;
            case 1: icon.addTriangle (c.x, c.y - s * 0.35f, c.x, c.y + s * 0.35f, c.x - s * 0.42f, c.y);
                    icon.addTriangle (c.x + s * 0.42f, c.y - s * 0.35f, c.x + s * 0.42f, c.y + s * 0.35f, c.x, c.y); break;
            case 2: icon.addTriangle (c.x - s * 0.28f, c.y - s * 0.4f, c.x - s * 0.28f, c.y + s * 0.4f, c.x + s * 0.38f, c.y); break;
            case 3: icon.addTriangle (c.x - s * 0.42f, c.y - s * 0.35f, c.x - s * 0.42f, c.y + s * 0.35f, c.x, c.y);
                    icon.addTriangle (c.x, c.y - s * 0.35f, c.x, c.y + s * 0.35f, c.x + s * 0.42f, c.y); break;
            case 4: icon.addRoundedRectangle (c.x - s * 0.3f, c.y - s * 0.38f, s * 0.22f, s * 0.76f, 2.0f);
                    icon.addRoundedRectangle (c.x + s * 0.08f, c.y - s * 0.38f, s * 0.22f, s * 0.76f, 2.0f); break;
            default: icon.addEllipse (c.x - s * 0.37f, c.y - s * 0.37f, s * 0.74f, s * 0.74f); break;
        }
        g.setColour (j == 5 ? Colour (0xfff05a45) : cLabel);
        g.fillPath (icon);
    }
}
