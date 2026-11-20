#include "PluginEditor.h"
#include "Resources.h"

using namespace juce;

namespace tss
{
    Font mono (float size, bool bold)
    {
        return Font (FontOptions (Font::getDefaultMonospacedFontName(), size, bold ? Font::bold : Font::plain));
    }
    Font sans (float size, bool bold)
    {
        return Font (FontOptions (Font::getDefaultSansSerifFontName(), size, bold ? Font::bold : Font::plain)).withExtraKerningFactor (0.08f);
    }
    Image image (const char* name)
    {
        int size = 0;
        if (auto* d = getResource (name, size))
            return ImageCache::getFromMemory (d, size);
        return {};
    }

    // the Transistor Labs mark: a hexagon with TR inside
    void drawLogo (Graphics& g, Rectangle<float> r, Colour c, float stroke)
    {
        Path hex;
        const auto centre = r.getCentre();
        const float rad = jmin (r.getWidth(), r.getHeight()) * 0.5f;
        for (int i = 0; i < 6; ++i)
        {
            const float a = MathConstants<float>::pi / 3.0f * (float) i - MathConstants<float>::halfPi;
            const Point<float> p (centre.x + rad * std::cos (a), centre.y + rad * std::sin (a));
            if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
        }
        hex.closeSubPath();
        g.setColour (c);
        g.strokePath (hex, PathStrokeType (stroke));
        g.setFont (sans (rad * 0.95f, true).withExtraKerningFactor (0.0f));
        g.drawText ("TR", r.translated (0, rad * 0.04f), Justification::centred, false);
    }

    void drawEmbossed (Graphics& g, const String& s, Rectangle<float> r, const Font& f, Justification j)
    {
        g.setFont (f);
        g.setColour (kInkLight.withAlpha (0.7f));
        g.drawText (s, r.translated (0, 0.8f), j, false);
        g.setColour (kInk);
        g.drawText (s, r, j, false);
    }

    void drawNineSlice (Graphics& g, const Image& img, Rectangle<float> dest, int srcInset, float destInset)
    {
        if (! img.isValid()) return;
        const int w = img.getWidth(), h = img.getHeight(), s = srcInset;
        const float d = destInset;
        const int sx[4] = { 0, s, w - s, w };
        const int sy[4] = { 0, s, h - s, h };
        const float dx[4] = { dest.getX(), dest.getX() + d, dest.getRight() - d, dest.getRight() };
        const float dy[4] = { dest.getY(), dest.getY() + d, dest.getBottom() - d, dest.getBottom() };
        Graphics::ScopedSaveState ss (g);
        g.setOpacity (1.0f);
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        for (int yi = 0; yi < 3; ++yi)
            for (int xi = 0; xi < 3; ++xi)
            {
                const auto src = img.getClippedImage ({ sx[xi], sy[yi], sx[xi + 1] - sx[xi], sy[yi + 1] - sy[yi] });
                const Rectangle<float> dr (dx[xi], dy[yi], dx[xi + 1] - dx[xi], dy[yi + 1] - dy[yi]);
                if (dr.getWidth() > 0 && dr.getHeight() > 0)
                    g.drawImage (src, dr, RectanglePlacement::stretchToFit);
            }
    }

    // panel geometry (the artwork is 2000 x 1200, the editor 1000 x 600)
    const Rectangle<float> kScreenArea  { 122.0f, 130.5f, 377.0f, 279.5f };
    const Point<float> kKnobCentres[6]  { { 634, 187.5f }, { 733, 187.5f }, { 832.5f, 187.5f },
                                          { 634, 287.5f }, { 733, 287.5f }, { 832.5f, 287.5f } };
    const char* const kKnobIds[6]       { "speed", "pitch", "range", "intensity", "harmonic", "crush" };
    const char* const kKnobLabels[6]    { "SPEED", "PITCH", "RANGE", "AMOUNT", "HARMONIC", "CRUSH" };
    const Rectangle<float> kRockerSlots[2] { { 639.0f, 355.0f, 77.0f, 39.0f }, { 752.0f, 355.0f, 77.0f, 39.0f } };
    const char* const kRockerIds[2]     { "fx", "filter" };
    const char* const kRockerLabels[2]  { "FX", "FILTER" };
    const float kKeyX[11][2] = { { 110, 178.5f }, { 189, 257 }, { 267.5f, 342.5f }, { 351.5f, 420.5f }, { 429.5f, 499.5f },
                                 { 508, 577 }, { 586, 655 }, { 664, 734 }, { 743.5f, 813 }, { 821, 890.5f }, { 0, 0 } };
    const float kKeyY[2][2] = { { 467.5f, 492.5f }, { 502.5f, 529.0f } };
}

using namespace tss;

//==============================================================================
TSSLookAndFeel::TSSLookAndFeel()
{
    knob = image ("knob.png");
    setColour (PopupMenu::backgroundColourId, Colour (0xff06160b));
    setColour (PopupMenu::textColourId, kGreen);
    setColour (PopupMenu::highlightedBackgroundColourId, kGreenDim);
    setColour (PopupMenu::highlightedTextColourId, Colour (0xffeafff0));
}

void TSSLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, Slider&)
{
    const auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float size = jmin (r.getWidth(), r.getHeight());
    const auto box = Rectangle<float> (size, size).withCentre (r.getCentre());
    // soft shadow in the panel hole
    g.setColour (Colours::black.withAlpha (0.28f));
    g.fillEllipse (box.translated (1.5f, 3.0f).expanded (0.5f));
    if (! knob.isValid()) { g.setColour (Colours::black); g.fillEllipse (box); return; }
    const float angle = startAngle + pos * (endAngle - startAngle);
    Graphics::ScopedSaveState ss (g);
    g.setOpacity (1.0f);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    const float scale = size / (float) knob.getWidth();
    g.drawImageTransformed (knob, AffineTransform::translation (-knob.getWidth() * 0.5f, -knob.getHeight() * 0.5f)
                                        .rotated (angle)
                                        .scaled (scale)
                                        .translated (box.getCentreX(), box.getCentreY()));
}

Knob::Knob (TSS1Processor& p, const String& id)
    : Slider (Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox), paramId (id),
      attachment (p.apvts, id, *this)
{
    setRotaryParameters (MathConstants<float>::pi * 1.22f, MathConstants<float>::pi * 2.78f, true);
    if (auto* prm = p.apvts.getParameter (id))
        setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    setMouseDragSensitivity (220);
    setVelocityBasedMode (false);
    setPopupDisplayEnabled (false, false, nullptr);
}

Rocker::Rocker (TSS1Processor& p, const String& id)
    : Button (id), off (image ("rocker_off.png")), on (image ("rocker_on.png")),
      attachment (p.apvts, id, *this)
{
    setClickingTogglesState (true);
}

void Rocker::paintButton (Graphics& g, bool over, bool)
{
    const auto& img = getToggleState() ? on : off;
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    if (img.isValid())
    {
        Graphics::ScopedSaveState ss (g);
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (img, r, RectanglePlacement::centred);
    }
    if (over)
    {
        g.setColour (Colours::white.withAlpha (0.06f));
        g.fillRoundedRectangle (r, 4.0f);
    }
}

//==============================================================================
SlotKey::SlotKey (TSS1Processor& p, int i) : proc (p), index (i),
    off (image ("key_off.png")), on (image ("key_on.png"))
{
    setRepaintsOnMouseActivity (false);
}

void SlotKey::update (bool sel, TSS1Processor::SlotStatus st, bool play, bool bl)
{
    if (sel == selected && st == status && play == playing && bl == blink) return;
    selected = sel; status = st; playing = play; blink = bl;
    repaint();
}

void SlotKey::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.5f);
    const float inset = jmin (r.getHeight() * 0.36f, 9.0f);
    if (playing) drawNineSlice (g, on, r, 120, inset);
    else         drawNineSlice (g, off, r.translated (0, -0.5f), 110, inset);

    // slot number
    g.setFont (sans (9.5f, true));
    g.setColour (Colour (0xff6d6553).withAlpha (playing ? 0.75f : 0.95f));
    g.drawText (String (index + 1).paddedLeft ('0', 2), r.withTrimmedRight (8).translated (0, playing ? 0.8f : 0.0f), Justification::centred, false);

    // LED: on when the phrase is rendered and ready, blinking while it renders, bright while playing
    const Point<float> led (r.getRight() - inset * 0.9f, r.getY() + inset * 0.62f);
    Colour c = Colour (0xff3c3b33);
    float glow = 0.0f;
    if (playing)                                                  { c = Colour (0xffb8ffc8); glow = 1.0f; }
    else if (status == TSS1Processor::SlotStatus::ready)          { c = kGreenMid; glow = 0.35f; }
    else if (status == TSS1Processor::SlotStatus::rendering)      { c = blink ? kGreenMid : Colour (0xff2f5a3a); glow = blink ? 0.3f : 0.0f; }
    if (glow > 0)
    {
        g.setGradientFill (ColourGradient (kGreen.withAlpha (0.55f * glow), led, kGreen.withAlpha (0.0f), led.translated (7.0f, 0), true));
        g.fillEllipse (Rectangle<float> (14.0f, 14.0f).withCentre (led));
    }
    g.setColour (c);
    g.fillEllipse (Rectangle<float> (4.2f, 4.2f).withCentre (led));
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawEllipse (Rectangle<float> (4.2f, 4.2f).withCentre (led), 0.5f);

    // the slot shown on the screen gets a phosphor frame
    if (selected)
    {
        g.setColour (kGreenMid.withAlpha (0.25f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.5f, 2.5f);
        g.setColour (kGreen.withAlpha (0.9f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 3.0f, 1.1f);
    }
}

void SlotKey::mouseDown (const MouseEvent& e)
{
    if (onSelect) onSelect (index);
    if (e.mods.isShiftDown()) proc.previewSlot (index);
}

//==============================================================================
CrtScreen::CrtScreen (TSS1Processor& p) : proc (p)
{
    addAndMakeVisible (editor);
    editor.setMultiLine (true, true);
    editor.setReturnKeyStartsNewLine (false);
    editor.setScrollbarsShown (false);
    editor.setInputRestrictions (240);
    editor.setFont (mono (13.0f, true));
    editor.setIndents (4, 3);
    editor.setColour (TextEditor::backgroundColourId, Colours::transparentBlack);
    editor.setColour (TextEditor::textColourId, kGreen);
    editor.setColour (TextEditor::highlightColourId, kGreenDim.withAlpha (0.7f));
    editor.setColour (TextEditor::highlightedTextColourId, Colour (0xffeafff0));
    editor.setColour (TextEditor::outlineColourId, Colours::transparentBlack);
    editor.setColour (TextEditor::focusedOutlineColourId, Colours::transparentBlack);
    editor.setColour (CaretComponent::caretColourId, kGreen);
    editor.setTextToShowWhenEmpty ("TYPE A PHRASE...", kGreenDim);
    editor.addListener (this);
    showSlot (proc.getSelectedSlot());
}

void CrtScreen::resized()
{
    editor.setBounds (26, 97, getWidth() - 40, 54);
}

void CrtScreen::showSlot (int slot)
{
    if (shownSlot != -1 && editor.hasKeyboardFocus (true)) commitText();
    shownSlot = slot;
    editor.setText (proc.getPhrase (slot), dontSendNotification);
}

void CrtScreen::commitText()
{
    if (shownSlot >= 0) proc.setPhrase (shownSlot, editor.getText().replaceCharacters ("\r\n", "  "));
}

void CrtScreen::textEditorTextChanged (TextEditor&)          { commitText(); }
void CrtScreen::textEditorReturnKeyPressed (TextEditor&)     { commitText(); proc.previewSlot (shownSlot); }
void CrtScreen::textEditorEscapeKeyPressed (TextEditor&)     { editor.setText (proc.getPhrase (shownSlot), dontSendNotification); unfocusAllComponents(); }
void CrtScreen::textEditorFocusLost (TextEditor&)            { commitText(); }

void CrtScreen::drawGlow (Graphics& g, const String& s, Rectangle<float> r, Colour c, Justification j, float size)
{
    g.setFont (mono (size, true));
    g.setColour (c.withAlpha (0.18f));
    g.drawText (s, r.translated (-0.7f, 0), j, false);
    g.drawText (s, r.translated (0.7f, 0), j, false);
    g.drawText (s, r.translated (0, 0.8f), j, false);
    g.setColour (c);
    g.drawText (s, r, j, false);
}

void CrtScreen::refresh (int f)
{
    frame = f;
    if (shownSlot != proc.getSelectedSlot()) showSlot (proc.getSelectedSlot());
    repaint();
}

const CrtScreen::Hot* CrtScreen::hotAt (Point<float> p) const
{
    for (auto& h : hots) if (h.r.contains (p)) return &h;
    return nullptr;
}

void CrtScreen::paint (Graphics& g)
{
    hots.clear();
    auto b = getLocalBounds().toFloat();

    // deepen the glass of the panel artwork to a dark phosphor screen
    g.setColour (Colour (0xff031409).withAlpha (0.86f));
    g.fillRoundedRectangle (b.reduced (1.0f), 20.0f);
    g.setGradientFill (ColourGradient (Colour (0xff0d3d1d).withAlpha (0.55f), b.getCentre(),
                                       Colours::transparentBlack, b.getTopLeft(), true));
    g.fillRoundedRectangle (b.reduced (1.0f), 20.0f);

    const float L = 14.0f, R = b.getWidth() - 14.0f, W = R - L;
    const float fs = 11.5f, lh = 15.0f;
    auto text = [&] (const String& s, float x, float y, Colour c, const String& action = {}, bool inverse = false)
    {
        g.setFont (mono (fs, true));
        const float w = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), s) + 1.0f;
        Rectangle<float> r (x, y, w, lh);
        const bool hot = action.isNotEmpty() && (hover == action || highlight == action || dragParam == action);
        if (inverse || hot)
        {
            g.setColour ((inverse ? kGreen : kGreenDim).withAlpha (inverse ? 0.9f : 0.55f));
            g.fillRect (r.expanded (2.0f, 0.0f));
            drawGlow (g, s, r, inverse ? Colour (0xff031409) : Colour (0xffeafff0), Justification::centredLeft, fs);
        }
        else drawGlow (g, s, r, c, Justification::centredLeft, fs);
        if (action.isNotEmpty()) addHot (r.expanded (3.0f, 0.0f), action);
        return x + w;
    };
    auto charW = [&] { g.setFont (mono (fs, true)); return (float) GlyphArrangement::getStringWidth (g.getCurrentFont(), "M"); };
    const float cw = charW();

    const int sel = proc.getSelectedSlot();
    const bool melodic = proc.isMelodic();
    auto& ap = proc.apvts;
    auto val = [&] (const char* id) { auto* p = ap.getParameter (id); return p->convertFrom0to1 (p->getValue()); };

    // title
    text ("TRANSISTOR SPEECH SYSTEM", L, 10, kGreen);
    {
        const String v = "TSS-1 V" JucePlugin_VersionString;
        g.setFont (mono (fs, true));
        text (v, R - (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), v) - 1.0f, 10, kGreenMid);
    }
    g.setColour (kGreenDim.withAlpha (0.8f));
    for (float x = L; x < R; x += 4.0f) g.fillRect (x, 28.0f, 2.0f, 1.0f);

    // voice
    float x = text ("> VOICE ", L, 33, kGreenMid);
    const int vi = (int) val ("voice");
    text (kVoicePresets[jlimit (0, kNumVoices - 1, vi)].name, x, 33, kGreen, "voiceMenu");
    {
        float rx = R - cw * 7.5f;
        rx = text ("[<]", rx, 33, kGreen, "voicePrev") + cw;
        text ("[>]", rx, 33, kGreen, "voiceNext");
    }

    // mode
    x = text ("> MODE  ", L, 48, kGreenMid);
    text (melodic ? "MELODIC" : "TRIGGER", x, 48, kGreen, "mode");
    if (! melodic)
    {
        const int rn = (int) val ("root");
        const String s = "KEYS " + TSS1Processor::noteName (rn) + "-" + TSS1Processor::noteName (rn + kNumSlots - 1);
        g.setFont (mono (fs, true));
        text (s, R - (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), s) - 1.0f, 48, kGreen, "root");
    }
    else
    {
        const String s = "KEYS PLAY SLOT " + String (sel + 1).paddedLeft ('0', 2);
        g.setFont (mono (fs, true));
        text (s, R - (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), s) - 1.0f, 48, kGreenMid);
    }

    // play options
    x = text ("> PLAY  ", L, 63, kGreenMid);
    x = text (val ("poly") > 0.5f ? "POLY" : "MONO", x, 63, kGreen, "poly") + cw;
    text (val ("gate") > 0.5f ? "GATE" : "ONE-SHOT", x, 63, kGreen, "gate");
    {
        const float lv = val ("level");
        const String s = "LEVEL " + String (lv >= 0 ? "+" : "") + String (lv, 1) + "DB";
        g.setFont (mono (fs, true));
        text (s, R - (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), s) - 1.0f, 63, kGreen, "level");
    }

    // slot line
    x = text ("> SLOT " + String (sel + 1).paddedLeft ('0', 2), L, 78, kGreenMid);
    if (! melodic) text ("  KEY " + TSS1Processor::noteName ((int) val ("root") + sel), x, 78, kGreenMid);
    {
        const auto st = proc.getSlotStatus (sel);
        const String s = st == TSS1Processor::SlotStatus::ready ? "READY" : st == TSS1Processor::SlotStatus::empty ? "EMPTY" : "RENDERING";
        g.setFont (mono (fs, true));
        text (s, R - (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), s) - 1.0f, 78,
              st == TSS1Processor::SlotStatus::ready ? kGreen : kGreenMid);
    }

    // phrase box
    auto box = Rectangle<float> (L, 95.0f, W, 58.0f);
    g.setColour (kGreenDim.withAlpha (editor.hasKeyboardFocus (true) ? 0.95f : 0.6f));
    for (float xx = box.getX(); xx < box.getRight(); xx += 4.0f) { g.fillRect (xx, box.getY(), 2.0f, 1.0f); g.fillRect (xx, box.getBottom() - 1, 2.0f, 1.0f); }
    for (float yy = box.getY(); yy < box.getBottom(); yy += 4.0f) { g.fillRect (box.getX(), yy, 1.0f, 2.0f); g.fillRect (box.getRight() - 1, yy, 1.0f, 2.0f); }
    drawGlow (g, ">", Rectangle<float> (L + 3, 99.0f, 10.0f, 14.0f), kGreen, Justification::centredLeft, 13.0f);

    // playback: the clip on show, the word being spoken and the waveform
    const auto ps = proc.getPlayState();
    SpeechClip::Ptr clip = proc.getPlayingClip();
    const bool showPlaying = ps.playing && clip != nullptr && (melodic ? ps.slot == sel : ps.slot == sel);
    if (! showPlaying) clip = proc.getClip (sel);
    const auto word = Rectangle<float> (L, 157.0f, W, 18.0f);
    if (showPlaying)
    {
        const int wi = clip->wordAt ((int) ps.position);
        if (wi >= 0)
        {
            const auto& w = clip->words[(size_t) wi];
            drawGlow (g, clip->text.substring (w.textStart, w.textStart + jmax (1, w.textLength)).toUpperCase(), word, kGreen, Justification::centred, 16.0f);
        }
        if (melodic && ps.note >= 0)
            drawGlow (g, TSS1Processor::noteName (ps.note), word, kGreenMid, Justification::centredRight, fs);
    }
    else
    {
        drawGlow (g, "SHIFT-CLICK A KEY OR PRESS RETURN TO PREVIEW", word, kGreenDim.brighter (0.2f), Justification::centred, 10.0f);
    }

    const auto wave = Rectangle<float> (L, 178.0f, W, 60.0f);
    g.setColour (kGreenDim.withAlpha (0.35f));
    g.fillRect (wave.getX(), wave.getCentreY(), wave.getWidth(), 1.0f);
    if (clip != nullptr && clip->numSamples() > 1)
    {
        const float* d = clip->audio.getReadPointer (0);
        const int n = clip->numSamples();
        const int cols = (int) wave.getWidth();
        const double playFrac = showPlaying ? ps.position / (double) n : -1.0;
        float peak = 0.05f;
        for (int i = 0; i < n; i += 4) peak = jmax (peak, std::abs (d[i]));
        for (int c = 0; c < cols; ++c)
        {
            const int a = (int) ((juce::int64) c * n / cols), e = jmax (a + 1, (int) ((juce::int64) (c + 1) * n / cols));
            float lo = 0, hi = 0;
            for (int i = a; i < e && i < n; ++i) { lo = jmin (lo, d[i]); hi = jmax (hi, d[i]); }
            const float h = wave.getHeight() * 0.5f / peak;
            const bool played = playFrac >= 0 && (double) c / cols <= playFrac;
            g.setColour (played ? kGreen : (showPlaying ? kGreenDim : kGreenMid.withAlpha (0.8f)));
            g.fillRect (wave.getX() + (float) c, wave.getCentreY() - hi * h, 1.0f, jmax (1.0f, (hi - lo) * h));
        }
        if (playFrac >= 0)
        {
            const float px = wave.getX() + (float) (playFrac * cols);
            g.setColour (Colour (0xffeafff0));
            g.fillRect (px, wave.getY(), 1.0f, wave.getHeight());
        }
    }

    // readouts (drag up/down to change)
    const char* ids[5]  { "speed", "pitch", "range", "gap", "amp" };
    const char* names[5] { "WPM ", "PITCH ", "RANGE ", "GAP ", "VOL " };
    x = L;
    for (int i = 0; i < 5; ++i)
        x = text (String (names[i]) + String ((int) val (ids[i])), x, 243, kGreen, ids[i]) + cw * 1.2f;

    // status
    String status;
    auto& es = EspeakService::get();
    if (es.getError().isNotEmpty()) status = "ERROR: " + es.getError().toUpperCase();
    else if (! es.isReady())        status = "STARTING SPEECH ENGINE";
    else if (proc.isRendering())    status = "RENDERING";
    else                            status = melodic ? "READY - PLAY THE KEYBOARD" : "READY";
    x = text (status, L, 259, es.getError().isNotEmpty() ? Colour (0xffffd27d) : kGreenMid);
    if ((frame / 15) % 2 == 0) { g.setColour (kGreen); g.fillRect (x + 3, 261.0f, 7.0f, 11.0f); }
}

void CrtScreen::paintOverChildren (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    // scanlines
    g.setColour (Colours::black.withAlpha (0.22f));
    for (float y = b.getY(); y < b.getBottom(); y += 2.0f) g.fillRect (b.getX(), y, b.getWidth(), 0.8f);
    // vignette and glass
    Path clip; clip.addRoundedRectangle (b, 20.0f);
    g.reduceClipRegion (clip);
    g.setGradientFill (ColourGradient (Colours::transparentBlack, b.getCentre(), Colours::black.withAlpha (0.55f),
                                       b.getTopLeft().translated (-10, -10), true));
    g.fillRect (b);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.09f), b.getX() + 30, b.getY() + 10,
                                       Colours::white.withAlpha (0.0f), b.getX() + 170, b.getY() + 110, true));
    g.fillEllipse (b.getX() - 40, b.getY() - 50, 260, 150);
}

void CrtScreen::doAction (const String& a)
{
    auto& ap = proc.apvts;
    auto toggle = [&ap] (const char* id)
    {
        auto* p = ap.getParameter (id);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
        p->endChangeGesture();
    };
    const int vi = ((AudioParameterChoice*) ap.getParameter ("voice"))->getIndex();
    if (a == "voicePrev") proc.loadVoicePreset ((vi + kNumVoices - 1) % kNumVoices);
    else if (a == "voiceNext") proc.loadVoicePreset ((vi + 1) % kNumVoices);
    else if (a == "mode") toggle ("mode");
    else if (a == "poly") toggle ("poly");
    else if (a == "gate") toggle ("gate");
    else if (a == "voiceMenu")
    {
        PopupMenu m;
        const char* groups[4] = { "RETRO COMPUTING", "CINEMATIC AI", "MODERN AND TECHNICAL", "CHARACTER" };
        const int starts[5] = { 0, 6, 13, 20, kNumVoices };
        for (int gi = 0; gi < 4; ++gi)
        {
            m.addSectionHeader (groups[gi]);
            for (int i = starts[gi]; i < starts[gi + 1]; ++i)
                m.addItem (i + 1, kVoicePresets[i].name, true, i == vi);
        }
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                         [this] (int r) { if (r > 0) proc.loadVoicePreset (r - 1); });
    }
}

void CrtScreen::mouseDown (const MouseEvent& e)
{
    unfocusAllComponents();
    commitText();
    const auto* h = hotAt (e.position);
    if (h == nullptr) return;
    static const StringArray dragIds { "speed", "pitch", "range", "gap", "amp", "root", "level" };
    if (dragIds.contains (h->action))
    {
        auto* p = proc.apvts.getParameter (h->action);
        if (e.getNumberOfClicks() > 1)
        {
            p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture();
            return;
        }
        dragParam = h->action;
        dragStartValue = p->convertFrom0to1 (p->getValue());
        p->beginChangeGesture();
    }
    else doAction (h->action);
}

void CrtScreen::mouseDrag (const MouseEvent& e)
{
    if (dragParam.isEmpty()) return;
    auto* p = proc.apvts.getParameter (dragParam);
    const float perPixel = dragParam == "root" ? 0.2f : dragParam == "level" ? 0.1f
                         : (dragParam == "pitch" || dragParam == "range") ? 0.5f : dragParam == "gap" ? 0.2f : 1.0f;
    const float v = dragStartValue - (float) e.getDistanceFromDragStartY() * perPixel;
    p->setValueNotifyingHost (p->convertTo0to1 (jlimit (p->getNormalisableRange().start, p->getNormalisableRange().end, v)));
}

void CrtScreen::mouseUp (const MouseEvent&)
{
    if (dragParam.isNotEmpty()) proc.apvts.getParameter (dragParam)->endChangeGesture();
    dragParam.clear();
}

void CrtScreen::mouseMove (const MouseEvent& e)
{
    const auto* h = hotAt (e.position);
    const String now = h != nullptr ? h->action : String();
    setMouseCursor (h != nullptr ? (StringArray { "speed", "pitch", "range", "gap", "amp", "root", "level" }.contains (now)
                                        ? MouseCursor::UpDownResizeCursor : MouseCursor::PointingHandCursor)
                                 : MouseCursor::NormalCursor);
    hover = now;
}

//==============================================================================
AboutOverlay::AboutOverlay() : monitor (image ("monitor.png"))
{
    setVisible (false);
}

Rectangle<float> AboutOverlay::monitorBounds() const
{
    const float w = 540.0f, h = w * 822.0f / 1072.0f;
    return Rectangle<float> (w, h).withCentre (getLocalBounds().toFloat().getCentre());
}

Rectangle<float> AboutOverlay::screenBounds() const
{
    const auto m = monitorBounds();
    const float s = m.getWidth() / 1072.0f;
    return Rectangle<float> (m.getX() + 128 * s, m.getY() + 116 * s, (927 - 128) * s, (735 - 116) * s);
}

void AboutOverlay::open (bool instant)
{
    typed = instant ? 2000 : 0;
    setVisible (true);
    toFront (false);
    if (! instant) startTimerHz (60);
}

void AboutOverlay::timerCallback()
{
    typed += 4;
    if (typed > 2000) stopTimer();
    repaint();
}

void AboutOverlay::paint (Graphics& g)
{
    g.fillAll (Colours::black.withAlpha (0.62f));
    const auto m = monitorBounds();
    if (monitor.isValid())
    {
        Graphics::ScopedSaveState ss (g);
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (monitor, m, RectanglePlacement::stretchToFit);
    }
    const auto s = screenBounds();
    g.setColour (Colour (0xff031409).withAlpha (0.9f));
    g.fillRoundedRectangle (s, 22.0f);

    links.clear();
    struct Line { String text; String link; Colour c; float size; };
    const Line lines[] =
    {
        { "TRANSISTOR LABS", {}, kGreenMid, 12.0f },
        { "TSS-1  TRANSISTOR SPEECH SYSTEM", {}, kGreen, 15.0f },
        { "VERSION " JucePlugin_VersionString, {}, kGreenMid, 12.0f },
        { "", {}, kGreen, 8.0f },
        { "ROBOTIC TEXT-TO-SPEECH INSTRUMENT", {}, kGreen, 12.0f },
        { "SPEECH: ESPEAK-NG FORMANT SYNTHESIS", {}, kGreenMid, 11.0f },
        { "VOICES AFTER DJZ-SPEAK BY MUSHROOMFLEET", {}, kGreenMid, 11.0f },
        { "", {}, kGreen, 8.0f },
        { "CONTACT  TICCOROSS@GMAIL.COM", "mailto:ticcoross@gmail.com", kGreen, 12.0f },
        { "STORE    TICCOROSS.GUMROAD.COM", "https://ticcoross.gumroad.com/", kGreen, 12.0f },
        { "", {}, kGreen, 6.0f },
        { "[ OPEN THE MANUAL ]", "manual", kGreen, 12.0f },
        { "[ SOURCE CODE - GPL-3.0 ]", "https://github.com/Spunkmeyer77/TSS-1", kGreen, 12.0f },
        { "", {}, kGreen, 8.0f },
        { "CLICK OUTSIDE TO CLOSE", {}, kGreenDim.brighter (0.3f), 10.0f },
    };

    drawLogo (g, Rectangle<float> (34.0f, 34.0f).withCentre ({ s.getCentreX(), s.getY() + 34.0f }), kGreen, 1.8f);
    float y = s.getY() + 60.0f;
    int budget = typed;
    for (auto& l : lines)
    {
        const auto shown = l.text.substring (0, jmax (0, budget));
        budget -= l.text.length();
        const float lh = l.size + 7.0f;
        Rectangle<float> r (s.getX() + 10, y, s.getWidth() - 20, lh);
        if (shown.isNotEmpty())
        {
            const bool over = l.link.isNotEmpty() && hover == l.link;
            g.setFont (mono (l.size, true));
            if (over)
            {
                const float w = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), l.text) + 8.0f;
                g.setColour (kGreen.withAlpha (0.9f));
                g.fillRect (Rectangle<float> (w, lh - 2).withCentre (r.getCentre()));
            }
            const auto c = over ? Colour (0xff031409) : l.c;
            g.setColour (c.withAlpha (0.2f));
            g.drawText (shown, r.translated (0.8f, 0), Justification::centred, false);
            g.setColour (c);
            g.drawText (shown, r, Justification::centred, false);
            if (l.link.isNotEmpty())
            {
                const float w = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), l.text) + 8.0f;
                links.push_back ({ Rectangle<float> (w, lh).withCentre (r.getCentre()), l.link });
            }
        }
        y += lh;
    }
    // scanlines over the monitor screen
    g.setColour (Colours::black.withAlpha (0.2f));
    for (float yy = s.getY(); yy < s.getBottom(); yy += 2.0f) g.fillRect (s.getX() + 6, yy, s.getWidth() - 12, 0.8f);
}

void AboutOverlay::mouseMove (const MouseEvent& e)
{
    String now;
    for (auto& l : links) if (l.first.contains (e.position)) now = l.second;
    if (now != hover) { hover = now; repaint(); }
    setMouseCursor (now.isNotEmpty() ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
}

void AboutOverlay::mouseDown (const MouseEvent& e)
{
    for (auto& l : links)
        if (l.first.contains (e.position))
        {
            if (l.second == "manual")
            {
                int size = 0;
                if (auto* d = getResource ("TSS-1 Manual.pdf", size))
                {
                    auto f = File::getSpecialLocation (File::tempDirectory).getChildFile ("TSS-1 Manual.pdf");
                    f.replaceWithData (d, (size_t) size);
                    f.startAsProcess();
                }
            }
            else URL (l.second).launchInDefaultBrowser();
            return;
        }
    if (! monitorBounds().contains (e.position)) { stopTimer(); setVisible (false); }
}

//==============================================================================
TSS1Editor::Panel::Panel() : panel (image ("panel.jpg"))
{
    logoArea  = { 84.0f, 42.0f, 150.0f, 26.0f };
    storeArea = { 742.0f, 42.0f, 166.0f, 26.0f };
}

void TSS1Editor::Panel::paint (Graphics& g)
{
    if (panel.isValid())
    {
        Graphics::ScopedSaveState ss (g);
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (panel, getLocalBounds().toFloat(), RectanglePlacement::stretchToFit);
    }
    else g.fillAll (Colour (0xffd8ceb4));

    // header strip: brand, model, store
    drawLogo (g, Rectangle<float> (22.0f, 22.0f).withCentre ({ 97.0f, 55.0f }).translated (0, 0.8f), kInkLight.withAlpha (0.7f), 1.6f);
    drawLogo (g, Rectangle<float> (22.0f, 22.0f).withCentre ({ 97.0f, 55.0f }), overLogo ? Colour (0xff2d6b3e) : kInk, 1.6f);
    drawEmbossed (g, "TRANSISTOR LABS", { 114.0f, 45.0f, 160.0f, 20.0f }, sans (12.0f, true).withExtraKerningFactor (0.18f), Justification::centredLeft);
    drawEmbossed (g, "TSS-1", { 380.0f, 41.0f, 90.0f, 28.0f }, sans (20.0f, true).withExtraKerningFactor (0.1f), Justification::centredRight);
    drawEmbossed (g, "TRANSISTOR SPEECH SYSTEM", { 478.0f, 45.0f, 230.0f, 20.0f }, sans (11.0f, true).withExtraKerningFactor (0.2f), Justification::centredLeft);
    g.setColour (overStore ? Colour (0xff2d6b3e) : kInk);
    drawEmbossed (g, "TICCOROSS.GUMROAD.COM", storeArea, sans (10.0f, true).withExtraKerningFactor (0.12f), Justification::centredRight);
    if (overStore) { g.setColour (kInk.withAlpha (0.6f)); g.fillRect (storeArea.getRight() - 150.0f, storeArea.getBottom() - 4.0f, 150.0f, 1.0f); }

    // control legends
    for (int i = 0; i < 6; ++i)
        drawEmbossed (g, kKnobLabels[i], Rectangle<float> (90.0f, 14.0f).withCentre (kKnobCentres[i].translated (0, 41.0f)),
                      sans (9.5f, true).withExtraKerningFactor (0.15f), Justification::centred);
    drawEmbossed (g, "EFFECTS", { 586.0f, 239.0f, 80.0f, 12.0f }, sans (8.5f, true).withExtraKerningFactor (0.25f), Justification::centredLeft);
    for (int i = 0; i < 2; ++i)
        drawEmbossed (g, kRockerLabels[i], Rectangle<float> (90.0f, 14.0f).withCentre ({ kRockerSlots[i].getCentreX(), kRockerSlots[i].getBottom() + 10.0f }),
                      sans (9.5f, true).withExtraKerningFactor (0.15f), Justification::centred);
    drawEmbossed (g, "PHRASE SLOTS", { 96.0f, 443.0f, 200.0f, 12.0f }, sans (8.5f, true).withExtraKerningFactor (0.25f), Justification::centredLeft);
    drawEmbossed (g, "SHIFT-CLICK TO PREVIEW", { 700.0f, 443.0f, 205.0f, 12.0f }, sans (8.5f, true).withExtraKerningFactor (0.2f), Justification::centredRight);
}

void TSS1Editor::Panel::mouseMove (const MouseEvent& e)
{
    const bool l = logoArea.contains (e.position), s = storeArea.contains (e.position);
    if (l != overLogo || s != overStore) { overLogo = l; overStore = s; repaint(); }
    setMouseCursor ((l || s) ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
}

void TSS1Editor::Panel::mouseDown (const MouseEvent& e)
{
    unfocusAllComponents();
    if (logoArea.contains (e.position) && onLogo) onLogo();
    else if (storeArea.contains (e.position)) URL ("https://ticcoross.gumroad.com/").launchInDefaultBrowser();
}

//==============================================================================
TSS1Editor::TSS1Editor (TSS1Processor& p)
    : AudioProcessorEditor (&p), proc (p), screen (p), plastic (image ("plastic.jpg"))
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.setBounds (0, 0, kWidth, kHeight);
    content.addAndMakeVisible (panel);
    panel.setBounds (0, 0, kWidth, kHeight);
    panel.onLogo = [this] { about.open(); };

    content.addAndMakeVisible (screen);
    screen.setBounds (kScreenArea.toNearestInt());

    for (int i = 0; i < 6; ++i)
    {
        auto* k = knobs.add (new Knob (p, kKnobIds[i]));
        content.addAndMakeVisible (k);
        k->setBounds (Rectangle<float> (66.0f, 66.0f).withCentre (kKnobCentres[i]).toNearestInt());
        k->onDragStart = [this, k] { screen.setHighlight (k->paramId); };
        k->onDragEnd   = [this]    { screen.setHighlight ({}); };
    }
    for (int i = 0; i < 2; ++i)
    {
        auto* r = rockers.add (new Rocker (p, kRockerIds[i]));
        content.addAndMakeVisible (r);
        r->setBounds (kRockerSlots[i].toNearestInt());
    }
    for (int i = 0; i < kNumSlots; ++i)
    {
        auto* k = keys.add (new SlotKey (p, i));
        content.addAndMakeVisible (k);
        const int col = i % 10, row = i / 10;
        k->setBounds (Rectangle<float>::leftTopRightBottom (kKeyX[col][0], kKeyY[row][0], kKeyX[col][1], kKeyY[row][1]).toNearestInt());
        k->onSelect = [this] (int s) { proc.setSelectedSlot (s); screen.showSlot (s); };
    }
    content.addChildComponent (about);
    about.setBounds (0, 0, kWidth, kHeight);

    setResizable (true, true);
    setResizeLimits (kWidth * 6 / 10, kHeight * 6 / 10, kWidth * 2, kHeight * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kWidth / kHeight);
    setSize (kWidth, kHeight);
    startTimerHz (30);
}

TSS1Editor::~TSS1Editor()
{
    setLookAndFeel (nullptr);
}

void TSS1Editor::paint (Graphics& g)
{
    if (plastic.isValid()) g.setTiledImageFill (plastic, 0, 0, 1.0f);
    else g.setColour (Colour (0xffd8ceb4));
    g.fillAll();
}

void TSS1Editor::resized()
{
    const float s = (float) getWidth() / (float) kWidth;
    content.setTransform (AffineTransform::scale (s));
}

void TSS1Editor::timerCallback()
{
    ++frame;
    const auto ps = proc.getPlayState();
    const int sel = proc.getSelectedSlot();
    const bool blink = (frame / 8) % 2 == 0;
    const bool melodic = proc.isMelodic();
    for (int i = 0; i < kNumSlots; ++i)
        keys[i]->update (i == sel, proc.getSlotStatus (i), ps.playing && ps.slot == i && (! melodic || i == sel), blink);
    screen.refresh (frame);
}
