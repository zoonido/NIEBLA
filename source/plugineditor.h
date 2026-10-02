// NIEBLA - Stage 6: the plugin window
#pragma once
#include <JuceHeader.h>
#include "pluginprocessor.h"

namespace nui
{
    // Colours from the approved mockup
    const juce::Colour bg     { 0xff2b3640 };
    const juce::Colour panel  { 0xff34414c };
    const juce::Colour raised { 0xff3f4d59 };
    const juce::Colour line   { 0xff4b5a67 };
    const juce::Colour fog    { 0xffdce3e7 };
    const juce::Colour muted  { 0xff93a2ad };
    const juce::Colour dim    { 0xff6e7e8a };
    const juce::Colour layerColours[4] { juce::Colour (0xff8cc5b8), juce::Colour (0xffa9a4d8), juce::Colour (0xffddb57a), juce::Colour (0xffd59ca9) };

    juce::Font font (float size, bool bold = false);

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
        void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
        void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.0f); }
        juce::Font getPopupMenuFont() override { return font (13.0f); }
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    };

    // Base for controls bound to one parameter; tagged with a hover-help key
    struct ParamControl : public juce::Component
    {
        ParamControl (juce::RangedAudioParameter* p, juce::Colour c, const juce::String& hoverKey);
        int  getIndex() const;
        void setIndex (int i);
        juce::RangedAudioParameter* param;
        juce::Colour colour;
    };

    // Row (or grid) of buttons for a choice parameter: Sync/Free, LP/BP/HP, voice types...
    struct ChoiceButtons : public ParamControl
    {
        ChoiceButtons (juce::RangedAudioParameter* p, juce::Colour c, const juce::String& hoverKey, int columns = 0, bool filled = false);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        int columns;
        bool filled;
    };

    // "<  value  >" stepper. If a mode parameter is set and switched to its second option,
    // the stepper edits the alternative (free-time) parameter instead.
    struct Stepper : public ParamControl
    {
        Stepper (juce::RangedAudioParameter* p, juce::Colour c, const juce::String& hoverKey,
                 juce::RangedAudioParameter* modeParam = nullptr, juce::RangedAudioParameter* altParam = nullptr, float altStep = 0.1f);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        void step (int direction);
        bool usingAlt() const;
        juce::RangedAudioParameter* mode;
        juce::RangedAudioParameter* alt;
        float altStep;
        bool highlight = false;
    };

    // Rotary knob with value readout and label underneath
    struct Knob : public juce::Component
    {
        Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& label, juce::Colour c, const juce::String& hoverKey);
        void resized() override;
        void paint (juce::Graphics&) override;
        juce::Slider slider;
        juce::String label;
        juce::RangedAudioParameter* param;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    // Thin horizontal slider (Fine, Pan, Level in the MIX strips)
    struct MiniSlider : public juce::Component
    {
        MiniSlider (juce::AudioProcessorValueTreeState& state, const juce::String& id, juce::Colour c, const juce::String& hoverKey, bool centred);
        void resized() override;
        void paint (juce::Graphics&) override;
        juce::Slider slider;
        juce::RangedAudioParameter* param;
        bool showValue = true;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    // On/off button for a bool parameter (Freeze, Tune to scale)
    struct ParamToggle : public ParamControl
    {
        ParamToggle (juce::RangedAudioParameter* p, juce::Colour c, const juce::String& hoverKey, const juce::String& text);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        juce::String text;
    };

    // Simple clickable text button painted in NIEBLA style
    struct FlatButton : public juce::Component
    {
        FlatButton (const juce::String& text, const juce::String& hoverKey);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
        juce::String text;
        bool active = false;
        juce::Colour activeColour = fog;
        std::function<void()> onClick;
    };
}

//==============================================================================
// The Bloom view: how one note unfolds across the four layers, with draggable handles
class BloomView : public juce::Component
{
public:
    explicit BloomView (NieblaAudioProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    struct Handles { juce::Point<float> start, target; bool hasTarget; };
    float xForBeats (double beats) const;
    double beatsForX (float x) const;
    float yForSemis (double semis) const;
    double retardBeats (int layer) const;
    int snappedInterval (int layer) const;

    NieblaAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& state;
    Handles handles[4];
    int dragLayer = -1;
    bool dragTarget = false;
    juce::Rectangle<float> plot;
    static constexpr double beatsShown = 6.0;
};

//==============================================================================
class LayerStrip : public juce::Component
{
public:
    LayerStrip (NieblaAudioProcessor&, int layer, std::function<void (int)> openLayer);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    int layer;
    juce::Colour colour;
    juce::ComboBox voice;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> voiceAttachment;
    nui::Stepper retard, interval, glide;
    nui::MiniSlider fine, pan, level;
    nui::ChoiceButtons pitchMode;
    nui::FlatButton mute, edit;
    juce::RangedAudioParameter* onParam;
    juce::RangedAudioParameter* pitchModeParam;
};

//==============================================================================
class LayerPage : public juce::Component
{
public:
    LayerPage (NieblaAudioProcessor&, int layer);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();   // swaps knobs that depend on a mode, relabels Character knobs

private:
    juce::String id (const juce::String& suffix) const;
    NieblaAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& state;
    int layer;
    juce::Colour colour;

    nui::ChoiceButtons voice, filterType, retardMode, pitchMode, glideMode, delayMode, revType, revShift;
    nui::ParamToggle revFreeze, revTune;
    nui::Knob char1, char2, octave, level, pan;
    nui::Knob attack, decay, sustain, release;
    nui::Knob cutoff, resonance, filterEnv;
    nui::Knob retardSync, retardTime, humanize;
    nui::Knob interval, fine, glideSync, glideTime, glideCurve;
    nui::Knob delaySync, delayMs, delayFeedback, delayMix;
    nui::Knob revSize, revDecay, revDamp, revMix, revShimmer, revLowCut, revHighCut;

    juce::Rectangle<int> voiceBox, envBox, filterBox, retardBox, pitchBox, fxBox;
    juce::Rectangle<float> envGraph, filterGraph, pitchGraph;
};

//==============================================================================
class NieblaAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NieblaAudioProcessorEditor (NieblaAudioProcessor&);
    ~NieblaAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;

    void showPage (int page);   // 0 = MIX, 1-4 = layers A-D

    static constexpr int width = 1100, height = 740;

private:
    void timerCallback() override;
    void showHelp (const juce::String& key);
    void savePreset();

    NieblaAudioProcessor& processor;
    nui::LookAndFeel lookAndFeel;

    juce::ComboBox rootBox, scaleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootAttachment, scaleAttachment;
    nui::ChoiceButtons forceScale;
    nui::Knob master, warmthKnob, driftKnob;
    nui::FlatButton saveButton;

    BloomView bloom;
    std::unique_ptr<LayerStrip> strips[4];
    std::unique_ptr<LayerPage> pages[4];

    int page = 0;
    juce::String helpTitle, helpText;
    juce::Rectangle<int> tabRects[5], presetRect, prevRect, nextRect, tempoRect, keysRect;
    double lastSnapshot = -1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NieblaAudioProcessorEditor)
};
