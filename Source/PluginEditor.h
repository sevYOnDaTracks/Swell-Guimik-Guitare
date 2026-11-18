#pragma once

#include "PluginProcessor.h"

//==============================================================================
// Apparence Swell : noir et blanc, controles plats aux coins arrondis.
class SwellLnf : public juce::LookAndFeel_V4
{
public:
    SwellLnf();
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
};

//==============================================================================
// Apercu des notes generees (piano roll), recalcule cote interface avec le meme moteur.
class RollView : public juce::Component
{
public:
    explicit RollView (GuimikProcessor& p) : proc (p) {}
    void refresh();
    void paint (juce::Graphics&) override;

private:
    struct Rect { float x, w; int pitch; int layer; bool soft; };
    GuimikProcessor& proc;
    gk::Engine ui;
    std::vector<Rect> rects;
    int totalTicks = 4 * gk::kTPB, bars = 4;
    float playTick = -1.0f;
    juce::String barNames[gk::kMaxBars];
};

//==============================================================================
class GuimikEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GuimikEditor (GuimikProcessor&);
    ~GuimikEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    void timerCallback() override;
    void setupCombo (juce::ComboBox&, const juce::String& id, const juce::String& caption);
    void setupSlider (juce::Slider&, const juce::String& id, const juce::String& caption);
    void randomise (const juce::String& id);

    GuimikProcessor& proc;
    SwellLnf lnf;
    juce::Image logo;
    juce::Typeface::Ptr bebas;
    juce::String chordText { "-" }, keyText { "-" };
    bool keyWarn = false;

    juce::ComboBox style, division, bars, key, scale, speed, source;
    juce::TextButton newIdea, play;
    juce::ToggleButton latch, preview;
    juce::TextEditor prog;
    juce::Slider bpm, hammer, slide, dbl, ghost, swing, human, previewVol;
    RollView roll;

    struct Row
    {
        juce::ToggleButton on;
        juce::ComboBox role, oct;
        juce::Slider dens, vel;
        juce::TextButton reroll;
    };
    Row rows[gk::kLayers];

    struct Caption { juce::Component* comp; juce::String text; };
    std::vector<Caption> captions;
    juce::Rectangle<int> panels[4];

    std::vector<std::unique_ptr<APVTS::ComboBoxAttachment>> comboAtt;
    std::vector<std::unique_ptr<APVTS::SliderAttachment>> sliderAtt;
    std::vector<std::unique_ptr<APVTS::ButtonAttachment>> buttonAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuimikEditor)
};
