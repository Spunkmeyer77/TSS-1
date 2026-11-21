#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

namespace tss
{
    // phosphor colours
    const juce::Colour kGreen      { 0xff7dffa0 };
    const juce::Colour kGreenMid   { 0xff3fcf6c };
    const juce::Colour kGreenDim   { 0xff1f7a3e };
    const juce::Colour kInk        { 0xff4a4232 };   // panel lettering
    const juce::Colour kInkLight   { 0xfff4ecd8 };

    juce::Font mono (float size, bool bold = true);
    juce::Font sans (float size, bool bold = true);
    juce::Image image (const char* name);
}

// knob drawn from the knob artwork
class TSSLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TSSLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    juce::Image knob;
};

class Knob : public juce::Slider
{
public:
    Knob (TSS1Processor& p, const juce::String& paramId);
    juce::String paramId;
private:
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class Rocker : public juce::Button
{
public:
    Rocker (TSS1Processor& p, const juce::String& paramId);
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    juce::Image off, on;
    juce::AudioProcessorValueTreeState::ButtonAttachment attachment;
};

class SlotKey : public juce::Component
{
public:
    SlotKey (TSS1Processor& p, int index);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void update (bool selected, TSS1Processor::SlotStatus status, bool playing, bool blink);
    std::function<void (int)> onSelect;
private:
    TSS1Processor& proc;
    int index;
    bool selected = false, playing = false, blink = false;
    TSS1Processor::SlotStatus status = TSS1Processor::SlotStatus::empty;
    juce::Image off, on;
};

// the green CRT: voice, mode, the phrase editor, the waveform and the readouts
class CrtScreen : public juce::Component, private juce::TextEditor::Listener
{
public:
    explicit CrtScreen (TSS1Processor& p);
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void refresh (int frame);
    void showSlot (int slot);
    void setHighlight (const juce::String& paramId) { highlight = paramId; }

private:
    void textEditorTextChanged (juce::TextEditor&) override;
    void textEditorReturnKeyPressed (juce::TextEditor&) override;
    void textEditorEscapeKeyPressed (juce::TextEditor&) override;
    void textEditorFocusLost (juce::TextEditor&) override;
    void commitText();

    struct Hot { juce::Rectangle<float> r; juce::String action; };
    void addHot (juce::Rectangle<float> r, const juce::String& action) { hots.push_back ({ r, action }); }
    const Hot* hotAt (juce::Point<float> p) const;
    void doAction (const juce::String& action);
    void drawGlow (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Colour, juce::Justification, float size);

    TSS1Processor& proc;
    juce::TextEditor editor;
    std::vector<Hot> hots;
    juce::String hover, highlight, dragParam;
    float dragStartValue = 0;
    int shownSlot = -1, frame = 0;
};

class AboutOverlay : public juce::Component, private juce::Timer
{
public:
    AboutOverlay();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void open (bool instant = false);
private:
    void timerCallback() override;
    juce::Rectangle<float> monitorBounds() const;
    juce::Rectangle<float> screenBounds() const;
    juce::Image monitor;
    int typed = 0;
    std::vector<std::pair<juce::Rectangle<float>, juce::String>> links;
    juce::String hover;
};

class TSS1Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TSS1Editor (TSS1Processor&);
    ~TSS1Editor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    // the window shows the case only: the 890 x 540 part of the 1000 x 600 layout space starting at (55, 30)
    static constexpr int kWidth = 890, kHeight = 540, kOffsetX = 55, kOffsetY = 30;
    void showAbout (bool instant = false) { about.open (instant); }
    void refreshNow()                     { timerCallback(); }

private:
    void timerCallback() override;

    class Panel : public juce::Component
    {
    public:
        Panel();
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        std::function<void()> onLogo;
        juce::Rectangle<float> logoArea, storeArea;
    private:
        juce::Image panel;
        bool overLogo = false, overStore = false;
    };

    TSS1Processor& proc;
    TSSLookAndFeel lnf;
    juce::Component content;
    Panel panel;
    CrtScreen screen;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<Rocker> rockers;
    juce::OwnedArray<SlotKey> keys;
    AboutOverlay about;
    int frame = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TSS1Editor)
};
