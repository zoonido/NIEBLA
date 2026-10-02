// NIEBLA - Stage 6: the plugin window
#include "plugineditor.h"
#include <set>

using namespace nui;

//==============================================================================
// HOVER HELP
//==============================================================================
namespace
{
    const char* layerLetters = "ABCD";

    // Character knob names per voice type (same order as the Voice list)
    const char* characterNames[10][2] {
        { "Breath", "Vibrato" }, { "Hardness", "Inharmonic" }, { "Ratio", "Index" }, { "Drift", "Harmonics" },
        { "Detune", "Width" }, { "Vowel", "Ensemble" }, { "Drawbars", "Rotary" }, { "Tine", "Tremolo" },
        { "Ensemble", "Bow" }, { "Colour", "Tracking" } };

    const char* characterHelp[10][2] {
        { "Adds breath noise and a soft chiff at the start of each note.", "Vibrato depth. It fades in gently after the note starts." },
        { "How bright the strike is: higher brings out the upper partials.", "Moves the partials from harmonic (tuned) toward a struck-metal bell." },
        { "Frequency ratio between modulator and carrier. Changes the basic tone colour.", "FM amount. Higher is brighter and more metallic; it follows the envelope." },
        { "Slow random pitch wander, up to 15 cents, like an old oscillator.", "Adds 2nd and 3rd harmonics to the pure sine." },
        { "Spreads three saws apart in pitch for a thicker, beating sound.", "Pans the detuned saws left and right for a wider stereo image." },
        { "Morphs the vowel from A through E, I and O to U.", "Spreads the choir voices apart in pitch for a bigger group sound." },
        { "Blends from soft flute drawbars (left) to full, bright drawbars (right).", "Rotary speaker speed. Zero is off; higher spins faster." },
        { "Tine hardness: more bell-like attack and a brighter bark.", "Stereo tremolo that moves the piano left and right." },
        { "Ensemble chorus: the slow, shimmering motion of string machines.", "Bow softness: darkens and softens the strings." },
        { "Brightness of the wind noise, from dark to airy.", "Tunes the noise to the note you play. High values sound like a whistle." } };

    struct Help { const char* key; const char* title; const char* text; };
    const Help helpTable[] {
        { "on",         "Mute",           "Switches this layer off. Notes already sounding fade out with their release." },
        { "voice",      "Voice",          "The sound source for this layer." },
        { "octave",     "Octave",         "Moves this layer up or down by whole octaves." },
        { "level",      "Level",          "Volume of this layer in the mix." },
        { "pan",        "Pan",            "Places this layer left or right." },
        { "attack",     "Attack",         "How long the note takes to fade in. Long attacks make slow, swelling pads." },
        { "decay",      "Decay",          "How long the note takes to fall from its peak to the sustain level." },
        { "sustain",    "Sustain",        "The level the note holds at while you keep the key down." },
        { "release",    "Release",        "How long the note fades after you let go." },
        { "filtertype", "Filter type",    "LP keeps the lows, BP keeps a band in the middle, HP keeps the highs." },
        { "cutoff",     "Cutoff",         "Where the filter starts cutting. Lower is darker." },
        { "resonance",  "Resonance",      "Boosts the sound right at the cutoff for a more vocal, whistling filter." },
        { "filterenv",  "Env amount",     "Lets the envelope open (or close) the filter as each note plays." },
        { "retardmode", "Retard mode",    "Sync follows Ableton's tempo in note values. Free uses seconds." },
        { "retard",     "Time retard",    "How long this layer waits before it plays the note. The note-off waits the same, so the note keeps its length." },
        { "humanize",   "Humanize",       "Adds up to 60 ms of random extra delay to each note, so repeats don't feel mechanical." },
        { "interval",   "Interval",       "Shifts this layer by semitones, snapped to the global scale. Editing here or on the MIX tab changes the same control." },
        { "fine",       "Fine",           "Nudges the shifted note up to 50 cents sharp or flat, on top of the snapped interval. Use it to make a layer drift slightly out of tune." },
        { "pitchmode",  "Pitch mode",     "Jump plays the shifted note right away. Glide starts on the note you played and slides to it." },
        { "glidemode",  "Glide mode",     "Sync sets the glide in note values. Free sets it in seconds." },
        { "glide",      "Glide time",     "How long the slide to the shifted note takes." },
        { "glidecurve", "Glide curve",    "Zero is a steady slide. Higher starts fast and settles slowly, which sounds natural on pads." },
        { "delaymode",  "Delay mode",     "Sync follows Ableton's tempo. ms sets the time directly." },
        { "delaytime",  "Delay time",     "Time between repeats." },
        { "delayfb",    "Feedback",       "How many repeats. Each repeat gets a little darker." },
        { "delaymix",   "Delay mix",      "Balance between the dry sound and the echoes." },
        { "revsize",    "Reverb size",    "Size of the space." },
        { "revdecay",   "Reverb decay",   "How long the reverb takes to fade by 60 dB." },
        { "revdamp",    "Damping",        "How quickly the high frequencies fade in the tail. Higher is darker." },
        { "revmix",     "Reverb mix",     "Balance between the dry sound and the reverb." },
        { "revtype",    "Reverb type",    "Hall is a smooth, open space. Pipe is the ringing, fluttering inside of a metal tube. Wood is a warm wooden room with a resonant body." },
        { "revshimmer", "Shimmer",        "Pitches the reverb up and feeds it back in, so the tail climbs into a bright halo." },
        { "revshift",   "Shimmer interval","How far each pass of the shimmer climbs: an octave or a fifth." },
        { "revfreeze",  "Freeze",         "Holds the current reverb tail forever. New notes play on top without adding to it." },
        { "revlowcut",  "Low cut",        "Removes lows from the reverb only, so long tails don't get muddy." },
        { "revhighcut", "High cut",       "Removes highs from the reverb only, for a darker, softer space." },
        { "revtune",    "Tune to scale",  "Pipe only: tunes the pipe's ring to the global scale root, so the metal always resonates in key." },
        { "warmth",     "Warmth",         "Tape-style saturation on the whole mix: rounder peaks, soft harmonics and a gentle high-end roll-off." },
        { "drift",      "Drift",          "Gives every note its own tiny pitch and filter differences, like a vintage analog polysynth." },
        { "root",       "Root",           "The key of the global scale." },
        { "scale",      "Scale",          "Every layer's Interval snaps to this scale." },
        { "forcescale", "Force to scale", "When on, notes you play outside the scale move to the nearest scale note." },
        { "master",     "Master",         "Overall output volume." },
        { "bloom",      "Bloom",          "Drag a round handle sideways to set time retard. Drag a square handle up or down to set the interval." },
        { "save",       "Save preset",    "Saves the current sound as your own preset." },
        { "edit",       "Edit",           "Opens this layer's full page." } };

    juce::String layerPrefix (int layer) { return juce::String::charToString ((juce::juce_wchar) ("abcd"[layer])) + "_"; }
}

juce::Font nui::font (float size, bool bold)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

//==============================================================================
// LOOK AND FEEL
//==============================================================================
nui::LookAndFeel::LookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::textColourId, fog);
    setColour (juce::ComboBox::outlineColourId, line);
    setColour (juce::ComboBox::arrowColourId, muted);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, fog);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, raised);
    setColour (juce::PopupMenu::highlightedTextColourId, fog);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, fog);
    setColour (juce::TextEditor::backgroundColourId, bg);
    setColour (juce::TextEditor::textColourId, fog);
    setColour (juce::TextEditor::outlineColourId, line);
    setColour (juce::TextButton::buttonColourId, raised);
    setColour (juce::TextButton::textColourOffId, fog);
}

void nui::LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = start + pos * (end - start);
    const auto colour = s.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 2, radius - 2, 0.0f, start, end, true);
    g.setColour (line);
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Bipolar knobs (Pan, Fine, Env amount...) fill from the top
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (start + end) * 0.5f : start;
    if (std::abs (angle - from) > 0.01f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, radius - 2, radius - 2, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (colour);
        g.strokePath (value, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float knobR = radius * 0.62f;
    g.setColour (raised);
    g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2, knobR * 2);
    g.setColour (juce::Colour (0xff56666f));
    g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2, knobR * 2, 1.0f);

    const auto tip = centre.getPointOnCircumference (knobR * 0.85f, angle);
    g.setColour (fog);
    g.drawLine ({ centre, tip }, 2.0f);
}

void nui::LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    const float cy = y + h * 0.5f;
    const auto colour = s.findColour (juce::Slider::thumbColourId);
    const bool centred = (bool) s.getProperties().getWithDefault ("centred", false);
    const bool thick = (bool) s.getProperties().getWithDefault ("thick", false);
    const float t = thick ? 6.0f : 4.0f;

    g.setColour (line);
    g.fillRoundedRectangle ((float) x, cy - t * 0.5f, (float) w, t, t * 0.5f);

    if (centred)
    {
        const float mid = x + w * 0.5f;
        g.setColour (muted);
        g.fillRect (mid - 0.5f, cy - 5.0f, 1.0f, 10.0f);
        g.setColour (colour.withAlpha (0.6f));
        g.fillRect (juce::jmin (mid, pos), cy - 1.5f, std::abs (pos - mid), 3.0f);
    }
    else
    {
        g.setColour (colour);
        g.fillRoundedRectangle ((float) x, cy - t * 0.5f, pos - x, t, t * 0.5f);
    }

    if (thick)
    {
        g.setColour (fog);
        g.fillRoundedRectangle (pos - 3.0f, cy - 8.0f, 6.0f, 16.0f, 3.0f);
    }
    else
    {
        g.setColour (bg);
        g.fillEllipse (pos - 7.0f, cy - 7.0f, 14.0f, 14.0f);
        g.setColour (colour);
        g.fillEllipse (pos - 5.0f, cy - 5.0f, 10.0f, 10.0f);
    }
}

void nui::LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (line);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    juce::Path arrow;
    const float ax = w - 16.0f, ay = h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (muted);
    g.fillPath (arrow);
}

void nui::LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 0, box.getWidth() - 26, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

//==============================================================================
// CONTROLS
//==============================================================================
nui::ParamControl::ParamControl (juce::RangedAudioParameter* p, juce::Colour c, const juce::String& hoverKey)
    : param (p), colour (c)
{
    jassert (param != nullptr);
    getProperties().set ("hover", hoverKey);
}

int nui::ParamControl::getIndex() const
{
    return juce::roundToInt (param->convertFrom0to1 (param->getValue()));
}

void nui::ParamControl::setIndex (int i)
{
    const auto& range = param->getNormalisableRange();
    const float v = juce::jlimit (range.start, range.end, (float) i);
    param->beginChangeGesture();
    param->setValueNotifyingHost (param->convertTo0to1 (v));
    param->endChangeGesture();
}

//------------------------------------------------------------------------------
nui::ChoiceButtons::ChoiceButtons (juce::RangedAudioParameter* p, juce::Colour c, const juce::String& hoverKey, int cols, bool fill)
    : ParamControl (p, c, hoverKey), columns (cols), filled (fill) {}

void nui::ChoiceButtons::paint (juce::Graphics& g)
{
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param);
    const auto labels = choice != nullptr ? choice->choices : juce::StringArray { "Off", "On" };
    const int n = labels.size();
    const int cols = columns > 0 ? columns : n;
    const int rows = (n + cols - 1) / cols;
    const int current = getIndex();
    const float gap = filled ? 6.0f : 0.0f;
    const float cw = (getWidth() - gap * (cols - 1)) / (float) cols;
    const float ch = (getHeight() - gap * (rows - 1)) / (float) rows;

    if (! filled)
    {
        g.setColour (line);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
    }

    for (int i = 0; i < n; ++i)
    {
        const juce::Rectangle<float> cell ((i % cols) * (cw + gap), (i / cols) * (ch + gap), cw, ch);
        const bool on = i == current;
        if (filled)
        {
            g.setColour (on ? colour : juce::Colours::transparentBlack);
            g.fillRoundedRectangle (cell.reduced (0.5f), 6.0f);
            g.setColour (on ? colour : line);
            g.drawRoundedRectangle (cell.reduced (0.5f), 6.0f, 1.0f);
            g.setColour (on ? bg : muted);
        }
        else
        {
            if (on)
            {
                g.setColour (raised);
                g.fillRoundedRectangle (cell.reduced (1.0f), 5.0f);
            }
            g.setColour (on ? fog : muted);
        }
        g.setFont (font (filled ? 13.0f : 12.0f));
        g.drawText (labels[i], cell, juce::Justification::centred);
    }
}

void nui::ChoiceButtons::mouseDown (const juce::MouseEvent& e)
{
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param);
    const int n = choice != nullptr ? choice->choices.size() : 2;
    const int cols = columns > 0 ? columns : n;
    const int rows = (n + cols - 1) / cols;
    const int col = juce::jlimit (0, cols - 1, e.x * cols / juce::jmax (1, getWidth()));
    const int row = juce::jlimit (0, rows - 1, e.y * rows / juce::jmax (1, getHeight()));
    const int index = row * cols + col;
    if (index < n)
        setIndex (index);
    repaint();
}

//------------------------------------------------------------------------------
nui::Stepper::Stepper (juce::RangedAudioParameter* p, juce::Colour c, const juce::String& hoverKey,
                       juce::RangedAudioParameter* modeParam, juce::RangedAudioParameter* altParam, float step)
    : ParamControl (p, c, hoverKey), mode (modeParam), alt (altParam), altStep (step) {}

bool nui::Stepper::usingAlt() const
{
    return mode != nullptr && alt != nullptr && mode->getValue() > 0.5f;
}

void nui::Stepper::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (highlight ? raised : bg);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (highlight ? colour : line);
    g.drawRoundedRectangle (r, 5.0f, highlight ? 1.5f : 1.0f);

    auto* p = usingAlt() ? alt : param;
    juce::String text = p->getCurrentValueAsText();
    if (p->getParameterID().endsWith ("interval") && getIndex() > 0)
        text = "+" + text;
    if (p->getLabel().isNotEmpty())
        text << " " << p->getLabel();

    g.setFont (font (12.0f));
    g.setColour (muted);
    g.drawText (juce::String::fromUTF8 ("\xe2\x97\x82"), r.removeFromLeft (18.0f), juce::Justification::centred);
    g.drawText (juce::String::fromUTF8 ("\xe2\x96\xb8"), r.removeFromRight (18.0f), juce::Justification::centred);
    g.setColour (fog);
    g.drawText (text, r, juce::Justification::centred);
}

void nui::Stepper::step (int direction)
{
    if (usingAlt())
    {
        const auto& range = alt->getNormalisableRange();
        const float v = juce::jlimit (range.start, range.end, alt->convertFrom0to1 (alt->getValue()) + direction * altStep);
        alt->beginChangeGesture();
        alt->setValueNotifyingHost (alt->convertTo0to1 (v));
        alt->endChangeGesture();
    }
    else
    {
        setIndex (getIndex() + direction);
    }
    repaint();
}

void nui::Stepper::mouseDown (const juce::MouseEvent& e)
{
    step (e.x < getWidth() / 2 ? -1 : 1);
}

void nui::Stepper::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (w.deltaY != 0.0f)
        step (w.deltaY > 0 ? 1 : -1);
}

//------------------------------------------------------------------------------
nui::Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& text, juce::Colour c, const juce::String& hoverKey)
    : label (text), param (state.getParameter (id))
{
    jassert (param != nullptr);
    getProperties().set ("hover", hoverKey);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setColour (juce::Slider::rotarySliderFillColourId, c);
    slider.setMouseDragSensitivity (180);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void nui::Knob::resized()
{
    slider.setBounds (getLocalBounds().withHeight (getHeight() - 30).withSizeKeepingCentre (50, getHeight() - 30));
}

void nui::Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().removeFromBottom (30);
    juce::String text = param->getCurrentValueAsText();
    if (param->getLabel().isNotEmpty()) text << " " << param->getLabel();
    if (param->getParameterID().endsWith ("interval") && param->convertFrom0to1 (param->getValue()) > 0.5f) text = "+" + text;
    g.setColour (fog);
    g.setFont (font (12.0f));
    g.drawText (text, r.removeFromTop (15), juce::Justification::centred);
    g.setColour (muted);
    g.setFont (font (11.0f));
    g.drawText (label, r, juce::Justification::centred);
}

//------------------------------------------------------------------------------
nui::MiniSlider::MiniSlider (juce::AudioProcessorValueTreeState& state, const juce::String& id, juce::Colour c, const juce::String& hoverKey, bool centred)
    : param (state.getParameter (id))
{
    getProperties().set ("hover", hoverKey);
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::thumbColourId, c);
    slider.getProperties().set ("centred", centred);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void nui::MiniSlider::resized()
{
    auto r = getLocalBounds();
    if (showValue) r.removeFromRight (52);
    slider.setBounds (r);
}

void nui::MiniSlider::paint (juce::Graphics& g)
{
    if (! showValue) return;
    juce::String text = param->getCurrentValueAsText();
    if (param->getLabel().isNotEmpty()) text << " " << param->getLabel();
    g.setColour (fog);
    g.setFont (font (12.0f));
    g.drawText (text, getLocalBounds().removeFromRight (50), juce::Justification::centredRight);
}

//------------------------------------------------------------------------------
nui::ParamToggle::ParamToggle (juce::RangedAudioParameter* p, juce::Colour c, const juce::String& hoverKey, const juce::String& t)
    : ParamControl (p, c, hoverKey), text (t)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void nui::ParamToggle::paint (juce::Graphics& g)
{
    const bool on = param->getValue() > 0.5f;
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (on ? colour.withAlpha (0.25f) : raised);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? colour : line);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    g.setColour (on ? colour : fog);
    g.setFont (font (12.0f));
    g.drawText (text, r, juce::Justification::centred);
}

void nui::ParamToggle::mouseDown (const juce::MouseEvent&)
{
    param->beginChangeGesture();
    param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f);
    param->endChangeGesture();
    repaint();
}

//------------------------------------------------------------------------------
nui::FlatButton::FlatButton (const juce::String& t, const juce::String& hoverKey) : text (t)
{
    getProperties().set ("hover", hoverKey);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void nui::FlatButton::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (active ? activeColour.withAlpha (0.25f) : raised);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (active ? activeColour : line);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    g.setColour (active ? activeColour : fog);
    g.setFont (font (12.0f));
    g.drawText (text, r, juce::Justification::centred);
}

//==============================================================================
// BLOOM VIEW
//==============================================================================
BloomView::BloomView (NieblaAudioProcessor& p) : processor (p), state (p.apvts)
{
    getProperties().set ("hover", "bloom");
}

float BloomView::xForBeats (double beats) const { return plot.getX() + (float) (beats / beatsShown) * plot.getWidth(); }
double BloomView::beatsForX (float x) const     { return (x - plot.getX()) / plot.getWidth() * beatsShown; }
float BloomView::yForSemis (double semis) const { return plot.getBottom() - (float) ((semis + 12.0) / 26.0) * plot.getHeight(); }

double BloomView::retardBeats (int layer) const
{
    const auto p = layerPrefix (layer);
    if (state.getRawParameterValue (p + "retardmode")->load() < 0.5f)
        return niebla::retardSyncBeats ((int) state.getRawParameterValue (p + "retardsync")->load());
    return state.getRawParameterValue (p + "retardtime")->load() * processor.getHostBpm() / 60.0;
}

int BloomView::snappedInterval (int layer) const
{
    const int shift = (int) state.getRawParameterValue (layerPrefix (layer) + "interval")->load();
    if (shift == 0) return 0;
    const int root = (int) state.getRawParameterValue ("root")->load();
    const int played = 60 + root;
    return niebla::snapToScale (played + shift, root, (int) state.getRawParameterValue ("scale")->load(), shift) - played;
}

void BloomView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (line);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

    plot = bounds.reduced (16.0f).withTrimmedLeft (40.0f).withTrimmedBottom (18.0f).withTrimmedTop (4.0f);

    const int root = (int) state.getRawParameterValue ("root")->load();
    const int scale = (int) state.getRawParameterValue ("scale")->load();
    const auto notes = niebla::noteNameList();

    // Beat grid
    for (int b = 0; b <= (int) beatsShown; ++b)
    {
        const float x = xForBeats (b);
        g.setColour (line.withAlpha (b % 4 == 0 ? 0.9f : 0.5f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (muted);
        g.setFont (font (11.0f));
        const juce::String label = b % 4 == 0 ? "bar " + juce::String (b / 4 + 1) : "beat " + juce::String (b % 4 + 1);
        g.drawText (label, juce::Rectangle<float> (x - 30, plot.getBottom() + 4, 60, 14), juce::Justification::centred);
    }

    // Scale-note rows (only the ones the layers land on get a name)
    std::set<int> used { 0 };
    for (int l = 0; l < 4; ++l) used.insert (snappedInterval (l));
    for (int s = -12; s <= 13; ++s)
    {
        if (! niebla::isInScale (60 + root + s, root, scale)) continue;
        const float y = yForSemis (s);
        g.setColour (line.withAlpha (used.count (s) ? 0.8f : 0.3f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        if (used.count (s))
        {
            juce::String name = notes[(root + s + 120) % 12];
            if (s >= 12) name << "'";
            if (s < 0) name << ",";
            g.setColour (muted);
            g.setFont (font (11.0f));
            g.drawText (name, juce::Rectangle<float> (plot.getX() - 40, y - 7, 32, 14), juce::Justification::centredRight);
        }
    }

    // One curve per layer
    const double bpm = processor.getHostBpm();
    for (int l = 0; l < 4; ++l)
    {
        const auto p = layerPrefix (l);
        const bool on = state.getRawParameterValue (p + "on")->load() > 0.5f;
        const auto colour = on ? layerColours[l] : dim;
        const double start = juce::jmin (beatsShown, retardBeats (l));
        const double fine = state.getRawParameterValue (p + "fine")->load() / 100.0;
        const double target = snappedInterval (l) + fine;
        const bool glide = state.getRawParameterValue (p + "pitchmode")->load() > 0.5f;
        const double glideBeats = state.getRawParameterValue (p + "glidemode")->load() < 0.5f
                                    ? niebla::glideSyncBeats ((int) state.getRawParameterValue (p + "glidesync")->load())
                                    : state.getRawParameterValue (p + "glidetime")->load() * bpm / 60.0;
        const double k = state.getRawParameterValue (p + "glidecurve")->load() * 8.0;

        juce::Path path;
        const double startSemis = glide ? 0.0 : target;
        path.startNewSubPath (xForBeats (start), yForSemis (startSemis));
        if (glide && glideBeats > 0.0)
        {
            for (int i = 1; i <= 48; ++i)
            {
                const double t = i / 48.0;
                const double shaped = k < 0.01 ? t : (1.0 - std::exp (-k * t)) / (1.0 - std::exp (-k));
                const double b = start + t * glideBeats;
                if (b > beatsShown) break;
                path.lineTo (xForBeats (b), yForSemis (target * shaped));
            }
        }
        path.lineTo (plot.getRight(), yForSemis (target));

        g.setColour (colour.withAlpha (0.18f));
        g.strokePath (path, juce::PathStrokeType (10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Handles
        const juce::Point<float> sp (xForBeats (start), yForSemis (startSemis));
        const float tx = glide ? juce::jmin (plot.getRight() - 14.0f, xForBeats (start + glideBeats) + 28.0f)
                               : juce::jmin (plot.getRight() - 14.0f, sp.x + 56.0f);
        handles[l] = { sp, { tx, yForSemis (target) }, l != 0 || target != 0.0 || true };

        g.setColour (bg);
        g.fillEllipse (sp.x - 8, sp.y - 8, 16, 16);
        g.setColour (colour);
        g.drawEllipse (sp.x - 8, sp.y - 8, 16, 16, 2.0f);
        g.fillEllipse (sp.x - 3.5f, sp.y - 3.5f, 7, 7);
        g.setFont (font (12.0f));
        g.drawText (juce::String::charToString ((juce::juce_wchar) layerLetters[l]), juce::Rectangle<float> (sp.x + 6, sp.y - 24, 16, 14), juce::Justification::centred);

        const auto tp = handles[l].target;
        g.setColour (bg);
        g.fillRoundedRectangle (tp.x - 7, tp.y - 7, 14, 14, 3);
        g.setColour (colour);
        g.drawRoundedRectangle (tp.x - 7, tp.y - 7, 14, 14, 3, 2.0f);
        juce::Path arrows;
        arrows.addTriangle (tp.x - 2.5f, tp.y - 1, tp.x + 2.5f, tp.y - 1, tp.x, tp.y - 4);
        arrows.addTriangle (tp.x - 2.5f, tp.y + 1, tp.x + 2.5f, tp.y + 1, tp.x, tp.y + 4);
        g.fillPath (arrows);
    }
}

void BloomView::mouseMove (const juce::MouseEvent& e)
{
    for (int l = 3; l >= 0; --l)
        if (e.position.getDistanceFrom (handles[l].start) < 11 || e.position.getDistanceFrom (handles[l].target) < 11)
        {
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            return;
        }
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void BloomView::mouseDown (const juce::MouseEvent& e)
{
    dragLayer = -1;
    for (int l = 3; l >= 0 && dragLayer < 0; --l)
    {
        if (e.position.getDistanceFrom (handles[l].target) < 11) { dragLayer = l; dragTarget = true; }
        else if (e.position.getDistanceFrom (handles[l].start) < 11) { dragLayer = l; dragTarget = false; }
    }
    if (dragLayer < 0) return;

    const auto p = layerPrefix (dragLayer);
    const juce::String id = dragTarget ? p + "interval"
                                       : (state.getRawParameterValue (p + "retardmode")->load() < 0.5f ? p + "retardsync" : p + "retardtime");
    state.getParameter (id)->beginChangeGesture();
}

void BloomView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragLayer < 0) return;
    const auto p = layerPrefix (dragLayer);

    if (dragTarget)
    {
        const double semis = (plot.getBottom() - e.position.y) / plot.getHeight() * 26.0 - 12.0;
        auto* param = state.getParameter (p + "interval");
        param->setValueNotifyingHost (param->convertTo0to1 ((float) juce::jlimit (-12, 12, juce::roundToInt (semis))));
    }
    else
    {
        const double beats = juce::jlimit (0.0, beatsShown, beatsForX (e.position.x));
        if (state.getRawParameterValue (p + "retardmode")->load() < 0.5f)
        {
            int best = 0;   // nearest note value
            for (int i = 1; i < 17; ++i)
                if (std::abs (niebla::retardSyncBeats (i) - beats) < std::abs (niebla::retardSyncBeats (best) - beats))
                    best = i;
            auto* param = state.getParameter (p + "retardsync");
            param->setValueNotifyingHost (param->convertTo0to1 ((float) best));
        }
        else
        {
            auto* param = state.getParameter (p + "retardtime");
            param->setValueNotifyingHost (param->convertTo0to1 ((float) (beats * 60.0 / processor.getHostBpm())));
        }
    }
    repaint();
}

void BloomView::mouseUp (const juce::MouseEvent&)
{
    if (dragLayer < 0) return;
    const auto p = layerPrefix (dragLayer);
    for (auto id : { p + "interval", p + "retardsync", p + "retardtime" })
        state.getParameter (id)->endChangeGesture();
    dragLayer = -1;
}

//==============================================================================
// MIX STRIP
//==============================================================================
LayerStrip::LayerStrip (NieblaAudioProcessor& proc, int l, std::function<void (int)> openLayer)
    : layer (l), colour (layerColours[l]),
      retard   (proc.apvts.getParameter (layerPrefix (l) + "retardsync"), layerColours[l], "retard",
                proc.apvts.getParameter (layerPrefix (l) + "retardmode"), proc.apvts.getParameter (layerPrefix (l) + "retardtime"), 0.05f),
      interval (proc.apvts.getParameter (layerPrefix (l) + "interval"), layerColours[l], "interval"),
      glide    (proc.apvts.getParameter (layerPrefix (l) + "glidesync"), layerColours[l], "glide",
                proc.apvts.getParameter (layerPrefix (l) + "glidemode"), proc.apvts.getParameter (layerPrefix (l) + "glidetime"), 0.1f),
      fine  (proc.apvts, layerPrefix (l) + "fine", layerColours[l], "fine", true),
      pan   (proc.apvts, layerPrefix (l) + "pan", layerColours[l], "pan", true),
      level (proc.apvts, layerPrefix (l) + "level", layerColours[l], "level", false),
      pitchMode (proc.apvts.getParameter (layerPrefix (l) + "pitchmode"), layerColours[l], "pitchmode"),
      mute ("Mute", "on"), edit (juce::String::fromUTF8 ("Edit \xe2\x96\xb8"), "edit")
{
    const auto p = layerPrefix (l);
    onParam = proc.apvts.getParameter (p + "on");
    pitchModeParam = proc.apvts.getParameter (p + "pitchmode");

    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (p + "voice"));
    voice.addItemList (choice->choices, 1);
    voice.getProperties().set ("hover", "voice");
    voiceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, p + "voice", voice);

    level.slider.getProperties().set ("thick", true);
    level.showValue = false;
    interval.highlight = false;

    mute.activeColour = colour;
    mute.onClick = [this]
    {
        onParam->beginChangeGesture();
        onParam->setValueNotifyingHost (onParam->getValue() > 0.5f ? 0.0f : 1.0f);
        onParam->endChangeGesture();
        repaint();
    };
    edit.onClick = [openLayer, l] { openLayer (l + 1); };

    for (auto* c : std::initializer_list<juce::Component*> { &voice, &retard, &interval, &glide, &fine, &pan, &level, &pitchMode, &mute, &edit })
        addAndMakeVisible (c);
}

void LayerStrip::resized()
{
    auto r = getLocalBounds().reduced (14, 0);
    r.removeFromTop (12);
    auto head = r.removeFromTop (30);
    voice.setBounds (head.removeFromRight (120).reduced (0, 2));
    r.removeFromTop (8);

    auto row = [&r] { auto x = r.removeFromTop (28); return x.removeFromRight (132).reduced (0, 3); };
    retard.setBounds (row());
    interval.setBounds (row());
    fine.setBounds (row());
    pitchMode.setBounds (row());
    glide.setBounds (row());
    pan.setBounds (row());

    r.removeFromTop (22);
    level.setBounds (r.removeFromTop (20));
    r.removeFromTop (10);
    auto buttons = r.removeFromTop (26);
    mute.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 4));
    buttons.removeFromLeft (8);
    edit.setBounds (buttons);
}

void LayerStrip::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (line);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);
    g.setColour (colour);
    g.fillRoundedRectangle (bounds.getX() + 1, bounds.getY(), bounds.getWidth() - 2, 3.0f, 1.5f);

    const bool on = onParam->getValue() > 0.5f;
    mute.active = ! on;
    glide.setAlpha (pitchModeParam->getValue() > 0.5f ? 1.0f : 0.35f);
    setAlpha (1.0f);

    g.setColour (on ? colour : dim);
    g.setFont (font (26.0f));
    g.drawText (juce::String::charToString ((juce::juce_wchar) layerLetters[layer]), 14, 12, 40, 30, juce::Justification::centredLeft);

    const char* rows[] { "Time retard", "Interval", "Fine", "Pitch mode", "Glide time", "Pan" };
    g.setFont (font (12.0f));
    for (int i = 0; i < 6; ++i)
    {
        const int y = 50 + i * 28;
        g.setColour (muted);
        g.drawText (rows[i], 14, y, 110, 28, juce::Justification::centredLeft);
        g.setColour (line);
        g.drawHorizontalLine (y + 28, 14.0f, (float) getWidth() - 14.0f);
    }
    g.setColour (muted);
    g.drawText ("Level", 14, 50 + 6 * 28 + 2, 60, 20, juce::Justification::centredLeft);
    g.setColour (fog);
    g.drawText (level.param->getCurrentValueAsText(), getWidth() - 110, 50 + 6 * 28 + 2, 96, 20, juce::Justification::centredRight);
}

//==============================================================================
// LAYER PAGE
//==============================================================================
LayerPage::LayerPage (NieblaAudioProcessor& p, int l)
    : processor (p), state (p.apvts), layer (l), colour (layerColours[l]),
      voice      (p.apvts.getParameter (layerPrefix (l) + "voice"), layerColours[l], "voice", 5, true),
      filterType (p.apvts.getParameter (layerPrefix (l) + "filtertype"), layerColours[l], "filtertype"),
      retardMode (p.apvts.getParameter (layerPrefix (l) + "retardmode"), layerColours[l], "retardmode"),
      pitchMode  (p.apvts.getParameter (layerPrefix (l) + "pitchmode"), layerColours[l], "pitchmode"),
      glideMode  (p.apvts.getParameter (layerPrefix (l) + "glidemode"), layerColours[l], "glidemode"),
      delayMode  (p.apvts.getParameter (layerPrefix (l) + "delaymode"), layerColours[l], "delaymode"),
      revType    (p.apvts.getParameter (layerPrefix (l) + "revtype"), layerColours[l], "revtype"),
      revShift   (p.apvts.getParameter (layerPrefix (l) + "revshift"), layerColours[l], "revshift"),
      revFreeze  (p.apvts.getParameter (layerPrefix (l) + "revfreeze"), layerColours[l], "revfreeze", "Freeze"),
      revTune    (p.apvts.getParameter (layerPrefix (l) + "revtune"), layerColours[l], "revtune", "Tune to scale"),
      char1  (p.apvts, layerPrefix (l) + "char1", "Character 1", layerColours[l], "char1@" + juce::String (l)),
      char2  (p.apvts, layerPrefix (l) + "char2", "Character 2", layerColours[l], "char2@" + juce::String (l)),
      octave (p.apvts, layerPrefix (l) + "octave", "Octave", layerColours[l], "octave"),
      level  (p.apvts, layerPrefix (l) + "level", "Level", layerColours[l], "level"),
      pan    (p.apvts, layerPrefix (l) + "pan", "Pan", layerColours[l], "pan"),
      attack  (p.apvts, layerPrefix (l) + "attack", "Attack", layerColours[l], "attack"),
      decay   (p.apvts, layerPrefix (l) + "decay", "Decay", layerColours[l], "decay"),
      sustain (p.apvts, layerPrefix (l) + "sustain", "Sustain", layerColours[l], "sustain"),
      release (p.apvts, layerPrefix (l) + "release", "Release", layerColours[l], "release"),
      cutoff    (p.apvts, layerPrefix (l) + "cutoff", "Cutoff", layerColours[l], "cutoff"),
      resonance (p.apvts, layerPrefix (l) + "resonance", "Resonance", layerColours[l], "resonance"),
      filterEnv (p.apvts, layerPrefix (l) + "filterenv", "Env amount", layerColours[l], "filterenv"),
      retardSync (p.apvts, layerPrefix (l) + "retardsync", "Retard", layerColours[l], "retard"),
      retardTime (p.apvts, layerPrefix (l) + "retardtime", "Retard", layerColours[l], "retard"),
      humanize   (p.apvts, layerPrefix (l) + "humanize", "Humanize", layerColours[l], "humanize"),
      interval   (p.apvts, layerPrefix (l) + "interval", "Interval", layerColours[l], "interval"),
      fine       (p.apvts, layerPrefix (l) + "fine", "Fine", layerColours[l], "fine"),
      glideSync  (p.apvts, layerPrefix (l) + "glidesync", "Glide time", layerColours[l], "glide"),
      glideTime  (p.apvts, layerPrefix (l) + "glidetime", "Glide time", layerColours[l], "glide"),
      glideCurve (p.apvts, layerPrefix (l) + "glidecurve", "Curve", layerColours[l], "glidecurve"),
      delaySync     (p.apvts, layerPrefix (l) + "delaysync", "Time", layerColours[l], "delaytime"),
      delayMs       (p.apvts, layerPrefix (l) + "delayms", "Time", layerColours[l], "delaytime"),
      delayFeedback (p.apvts, layerPrefix (l) + "delayfb", "Feedback", layerColours[l], "delayfb"),
      delayMix      (p.apvts, layerPrefix (l) + "delaymix", "Mix", layerColours[l], "delaymix"),
      revSize  (p.apvts, layerPrefix (l) + "revsize", "Size", layerColours[l], "revsize"),
      revDecay (p.apvts, layerPrefix (l) + "revdecay", "Decay", layerColours[l], "revdecay"),
      revDamp  (p.apvts, layerPrefix (l) + "revdamp", "Damping", layerColours[l], "revdamp"),
      revMix   (p.apvts, layerPrefix (l) + "revmix", "Mix", layerColours[l], "revmix"),
      revShimmer (p.apvts, layerPrefix (l) + "revshimmer", "Shimmer", layerColours[l], "revshimmer"),
      revLowCut  (p.apvts, layerPrefix (l) + "revlowcut", "Low cut", layerColours[l], "revlowcut"),
      revHighCut (p.apvts, layerPrefix (l) + "revhighcut", "High cut", layerColours[l], "revhighcut")
{
    for (auto* c : std::initializer_list<juce::Component*> {
             &voice, &filterType, &retardMode, &pitchMode, &glideMode, &delayMode,
             &char1, &char2, &octave, &level, &pan, &attack, &decay, &sustain, &release,
             &cutoff, &resonance, &filterEnv, &retardSync, &retardTime, &humanize,
             &interval, &fine, &glideSync, &glideTime, &glideCurve,
             &delaySync, &delayMs, &delayFeedback, &delayMix, &revSize, &revDecay, &revDamp, &revMix,
             &revType, &revShift, &revFreeze, &revTune, &revShimmer, &revLowCut, &revHighCut })
        addAndMakeVisible (c);
    refresh();
}

juce::String LayerPage::id (const juce::String& suffix) const { return layerPrefix (layer) + suffix; }

void LayerPage::refresh()
{
    const bool retardFree = state.getRawParameterValue (id ("retardmode"))->load() > 0.5f;
    retardSync.setVisible (! retardFree);
    retardTime.setVisible (retardFree);

    const bool glideFree = state.getRawParameterValue (id ("glidemode"))->load() > 0.5f;
    glideSync.setVisible (! glideFree);
    glideTime.setVisible (glideFree);

    const bool delayMsMode = state.getRawParameterValue (id ("delaymode"))->load() > 0.5f;
    delaySync.setVisible (! delayMsMode);
    delayMs.setVisible (delayMsMode);

    const int v = juce::jlimit (0, 9, (int) state.getRawParameterValue (id ("voice"))->load());
    char1.label = characterNames[v][0];
    char2.label = characterNames[v][1];

    const int reverbType = (int) state.getRawParameterValue (id ("revtype"))->load();
    revSize.label = reverbType == 1 ? "Length" : "Size";
    revDamp.label = reverbType == 0 ? "Damping" : "Material";
    revTune.setVisible (reverbType == 1);
    revShift.setAlpha (state.getRawParameterValue (id ("revshimmer"))->load() > 0.001f ? 1.0f : 0.35f);

    const bool glide = state.getRawParameterValue (id ("pitchmode"))->load() > 0.5f;
    for (auto* c : std::initializer_list<juce::Component*> { &glideMode, &glideSync, &glideTime, &glideCurve })
        c->setAlpha (glide ? 1.0f : 0.35f);
}

void LayerPage::resized()
{
    auto r = getLocalBounds().reduced (16, 8);
    const int colW = (r.getWidth() - 28) / 3;
    auto top = r.removeFromTop (262);
    r.removeFromTop (12);
    auto bottom = r;

    voiceBox  = top.removeFromLeft (colW);     top.removeFromLeft (14);
    envBox    = top.removeFromLeft (colW);     top.removeFromLeft (14);
    filterBox = top;
    retardBox = bottom.removeFromLeft (colW);  bottom.removeFromLeft (14);
    pitchBox  = bottom.removeFromLeft (colW);  bottom.removeFromLeft (14);
    fxBox     = bottom;

    auto knobRow = [] (juce::Rectangle<int> area, std::initializer_list<juce::Component*> knobs)
    {
        const int w = 60;
        int x = area.getX();
        for (auto* k : knobs) { k->setBounds (x, area.getY(), w, 86); x += w + 4; }
    };

    // Voice
    {
        auto a = voiceBox.reduced (14);
        a.removeFromTop (30);
        voice.setBounds (a.removeFromTop (66));
        a.removeFromTop (12);
        knobRow (a.removeFromTop (86), { &char1, &char2, &octave, &level, &pan });
    }
    // Envelope
    {
        auto a = envBox.reduced (14);
        a.removeFromTop (30);
        envGraph = a.removeFromTop (100).toFloat();
        a.removeFromTop (10);
        knobRow (a.removeFromTop (86), { &attack, &decay, &sustain, &release });
    }
    // Filter
    {
        auto a = filterBox.reduced (14);
        filterType.setBounds (a.getX() + 70, a.getY() - 2, 132, 26);
        a.removeFromTop (30);
        filterGraph = a.removeFromTop (100).toFloat();
        a.removeFromTop (10);
        knobRow (a.removeFromTop (86), { &cutoff, &resonance, &filterEnv });
    }
    // Time retard + delay
    {
        auto a = retardBox.reduced (14);
        retardMode.setBounds (a.getX() + 110, a.getY() - 2, 110, 26);
        a.removeFromTop (32);
        knobRow (a.removeFromTop (86), { &retardSync, &humanize });
        retardTime.setBounds (retardSync.getBounds());
        a.removeFromTop (10);
        delayMode.setBounds (a.getX() + 70, a.getY() - 2, 96, 26);
        a.removeFromTop (30);
        knobRow (a.removeFromTop (86), { &delaySync, &delayFeedback, &delayMix });
        delayMs.setBounds (delaySync.getBounds());
    }
    // Pitch shift
    {
        auto a = pitchBox.reduced (14);
        pitchMode.setBounds (a.getX() + 100, a.getY() - 2, 110, 26);
        glideMode.setBounds (a.getRight() - 96, a.getY() - 2, 96, 26);
        a.removeFromTop (30);
        pitchGraph = a.removeFromTop (92).toFloat();
        a.removeFromTop (8);
        knobRow (a.removeFromTop (86), { &interval, &fine, &glideSync, &glideCurve });
        glideTime.setBounds (glideSync.getBounds());
    }
    // Reverb
    {
        auto a = fxBox.reduced (14);
        revType.setBounds (a.getRight() - 150, a.getY() - 2, 150, 26);
        a.removeFromTop (32);
        knobRow (a.removeFromTop (86), { &revSize, &revDecay, &revDamp, &revMix });
        a.removeFromTop (8);
        auto row = a.removeFromTop (86);
        knobRow (row, { &revShimmer, &revLowCut, &revHighCut });
        auto buttons = row.withTrimmedLeft (3 * 64).reduced (0, 2);
        revFreeze.setBounds (buttons.removeFromTop (24));
        buttons.removeFromTop (5);
        revShift.setBounds (buttons.removeFromTop (24));
        buttons.removeFromTop (5);
        revTune.setBounds (buttons.removeFromTop (24));
    }
}

void LayerPage::paint (juce::Graphics& g)
{
    auto box = [&] (juce::Rectangle<int> b, const juce::String& title)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (b.toFloat(), 10.0f);
        g.setColour (line);
        g.drawRoundedRectangle (b.toFloat().reduced (0.5f), 10.0f, 1.0f);
        g.setColour (fog);
        g.setFont (font (16.0f));
        g.drawText (title, b.getX() + 14, b.getY() + 10, 200, 24, juce::Justification::centredLeft);
    };
    box (voiceBox, "Voice");
    box (envBox, "Envelope");
    box (filterBox, "Filter");
    box (retardBox, "Time retard");
    box (pitchBox, "Pitch shift");
    box (fxBox, "Reverb");
    g.setColour (line);
    g.drawHorizontalLine (retardBox.getY() + 14 + 32 + 86 + 4, (float) retardBox.getX() + 14, (float) retardBox.getRight() - 14);
    g.setColour (fog);
    g.setFont (font (16.0f));
    g.drawText ("Delay", retardBox.getX() + 14, retardBox.getY() + 14 + 32 + 86 + 8, 200, 26, juce::Justification::centredLeft);

    // Pipe: show the note the pipe rings at
    auto rv = [this] (const char* suffix) { return state.getRawParameterValue (id (suffix))->load(); };
    if ((int) rv ("revtype") == 1)
    {
        const int root = (int) state.getRawParameterValue ("root")->load();
        const double f = NieblaReverb::pipeFrequency (rv ("revsize"), rv ("revtune") > 0.5f ? root : -1);
        const int midi = juce::roundToInt (69.0 + 12.0 * std::log2 (f / 440.0));
        g.setColour (muted);
        g.setFont (font (11.0f));
        g.drawText ("rings at " + juce::String (juce::roundToInt (f)) + " Hz (" + niebla::noteNameList()[(midi % 12 + 12) % 12] + ")",
                    fxBox.getX() + 80, fxBox.getY() + 10, 140, 24, juce::Justification::centredLeft);
    }

    // Layer tag
    g.setColour (colour);
    g.drawRoundedRectangle ((float) voiceBox.getX() + 74, (float) voiceBox.getY() + 13, 62, 18, 4, 1.0f);
    g.setFont (font (11.0f));
    g.drawText ("Layer " + juce::String::charToString ((juce::juce_wchar) layerLetters[layer]), voiceBox.getX() + 74, voiceBox.getY() + 13, 62, 18, juce::Justification::centred);

    auto graphBack = [&] (juce::Rectangle<float> r) { g.setColour (juce::Colours::black.withAlpha (0.12f)); g.fillRoundedRectangle (r, 6.0f); };
    auto v = [this] (const char* s) { return state.getRawParameterValue (id (s))->load(); };

    // Envelope shape (square-root time scale so long and short settings both read well)
    {
        graphBack (envGraph);
        const auto r = envGraph.reduced (8.0f, 10.0f);
        const float a = std::sqrt (v ("attack")), d = std::sqrt (v ("decay")), rel = std::sqrt (v ("release")), hold = 1.2f;
        const float total = a + d + hold + rel;
        auto x = [&] (float t) { return r.getX() + t / total * r.getWidth(); };
        const float sus = v ("sustain");
        juce::Path path;
        path.startNewSubPath (r.getX(), r.getBottom());
        path.quadraticTo (x (a * 0.6f), r.getBottom(), x (a), r.getY());
        path.quadraticTo (x (a + d * 0.3f), r.getBottom() - sus * r.getHeight(), x (a + d), r.getBottom() - sus * r.getHeight());
        path.lineTo (x (a + d + hold), r.getBottom() - sus * r.getHeight());
        path.quadraticTo (x (a + d + hold + rel * 0.3f), r.getBottom(), r.getRight(), r.getBottom());
        juce::Path fill (path);
        fill.closeSubPath();
        g.setColour (colour.withAlpha (0.12f));
        g.fillPath (fill);
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (2.0f));
    }

    // Filter response
    {
        graphBack (filterGraph);
        const auto r = filterGraph.reduced (8.0f, 10.0f);
        const int type = (int) v ("filtertype");
        const double fc = v ("cutoff");
        const double q = juce::jmap ((double) v ("resonance"), 0.0, 1.0, 0.707, 10.0);
        juce::Path path;
        for (int i = 0; i <= 120; ++i)
        {
            const double f = 20.0 * std::pow (1000.0, i / 120.0);
            const double xr = f / fc;
            const double den = std::sqrt ((1 - xr * xr) * (1 - xr * xr) + (xr / q) * (xr / q));
            const double mag = type == 0 ? 1.0 / den : (type == 1 ? (xr / q) / den : xr * xr / den);
            const double db = juce::jlimit (-30.0, 24.0, 20.0 * std::log10 (mag + 1e-9));
            const float px = r.getX() + i / 120.0f * r.getWidth();
            const float py = r.getY() + (float) ((24.0 - db) / 54.0) * r.getHeight();
            if (i == 0) path.startNewSubPath (px, py); else path.lineTo (px, py);
        }
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (2.0f));
    }

    // Pitch shift preview
    {
        graphBack (pitchGraph);
        const auto r = pitchGraph.reduced (10.0f, 14.0f);
        const int root = (int) state.getRawParameterValue ("root")->load();
        const int shift = (int) v ("interval");
        const int target = shift == 0 ? 0 : niebla::snapToScale (60 + root + shift, root, (int) state.getRawParameterValue ("scale")->load(), shift) - (60 + root);
        const auto notes = niebla::noteNameList();
        const bool up = target >= 0;
        const float yPlayed = up ? r.getBottom() : r.getY();
        const float yTarget = target == 0 ? yPlayed : (up ? r.getY() : r.getBottom());
        g.setColour (line);
        const float dashes[] { 3.0f, 4.0f };
        g.drawDashedLine ({ r.getX(), yPlayed, r.getRight(), yPlayed }, dashes, 2);
        g.drawDashedLine ({ r.getX(), yTarget, r.getRight(), yTarget }, dashes, 2);
        g.setColour (muted);
        g.setFont (font (10.0f));
        g.drawText (notes[root] + " played", juce::Rectangle<float> (r.getX() + 2, yPlayed - (up ? 14 : -2), 90, 12), juce::Justification::centredLeft);
        const float fineCents = v ("fine");
        g.drawText (notes[(root + target + 120) % 12] + (std::abs (fineCents) >= 0.5f ? (fineCents > 0 ? " +" : " ") + juce::String (juce::roundToInt (fineCents)) + " ct" : juce::String()),
                    juce::Rectangle<float> (r.getX() + 2, yTarget + (up ? -14 : 2), 90, 12), juce::Justification::centredLeft);

        const bool glide = v ("pitchmode") > 0.5f;
        const double k = v ("glidecurve") * 8.0;
        const float x0 = r.getX() + r.getWidth() * 0.22f;
        juce::Path path;
        path.startNewSubPath (x0, glide ? yPlayed : yTarget);
        if (glide)
            for (int i = 1; i <= 40; ++i)
            {
                const double t = i / 40.0;
                const double s = k < 0.01 ? t : (1.0 - std::exp (-k * t)) / (1.0 - std::exp (-k));
                path.lineTo (x0 + (float) t * r.getWidth() * 0.55f, yPlayed + (float) s * (yTarget - yPlayed));
            }
        path.lineTo (r.getRight(), yTarget);
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (2.5f));
        g.fillEllipse (x0 - 4, (glide ? yPlayed : yTarget) - 4, 8, 8);
    }

}

//==============================================================================
// EDITOR
//==============================================================================
NieblaAudioProcessorEditor::NieblaAudioProcessorEditor (NieblaAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      forceScale (p.apvts.getParameter ("forcescale"), juce::Colour (0xff6f8e88), "forcescale"),
      master (p.apvts, "master", "Master", fog, "master"),
      warmthKnob (p.apvts, "warmth", "Warmth", juce::Colour (0xffddb57a), "warmth"),
      driftKnob (p.apvts, "drift", "Drift", juce::Colour (0xff8cc5b8), "drift"),
      saveButton ("Save preset", "save"),
      bloom (p)
{
    setLookAndFeel (&lookAndFeel);

    rootBox.addItemList (niebla::noteNameList(), 1);
    rootBox.getProperties().set ("hover", "root");
    rootAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "root", rootBox);
    auto* scaleChoice = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter ("scale"));
    scaleBox.addItemList (scaleChoice->choices, 1);
    scaleBox.getProperties().set ("hover", "scale");
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "scale", scaleBox);

    saveButton.onClick = [this] { savePreset(); };

    for (auto* c : std::initializer_list<juce::Component*> { &rootBox, &scaleBox, &forceScale, &master, &warmthKnob, &driftKnob, &saveButton, &bloom })
        addAndMakeVisible (c);

    for (int l = 0; l < 4; ++l)
    {
        strips[l] = std::make_unique<LayerStrip> (p, l, [this] (int page) { showPage (page); });
        pages[l]  = std::make_unique<LayerPage> (p, l);
        addAndMakeVisible (*strips[l]);
        addChildComponent (*pages[l]);
    }

    addMouseListener (this, true);   // hover help for every child control
    setSize (width, height);
    showPage (0);
    showHelp ("bloom");
    startTimerHz (20);
}

NieblaAudioProcessorEditor::~NieblaAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void NieblaAudioProcessorEditor::showPage (int newPage)
{
    page = juce::jlimit (0, 4, newPage);
    bloom.setVisible (page == 0);
    for (int l = 0; l < 4; ++l)
    {
        strips[l]->setVisible (page == 0);
        pages[l]->setVisible (page == l + 1);
    }
    if (page > 0) pages[page - 1]->refresh();
    repaint();
}

void NieblaAudioProcessorEditor::resized()
{
    // Header
    for (int i = 0; i < 5; ++i)
        tabRects[i] = { 196 + i * 70 - (i > 0 ? 0 : 0), 16, i == 0 ? 64 : 62, 34 };
    presetRect = { 640, 16, 230, 34 };
    prevRect = presetRect.withWidth (30);
    nextRect = presetRect.withTrimmedLeft (presetRect.getWidth() - 30);
    saveButton.setBounds (882, 16, 100, 34);
    tempoRect = { 994, 16, 90, 34 };

    // Global bar
    rootBox.setBounds (116, 76, 64, 30);
    scaleBox.setBounds (190, 76, 160, 30);
    keysRect = { 366, 80, 12 * 21, 24 };
    forceScale.setBounds (744, 78, 90, 26);
    warmthKnob.setBounds (866, 66, 70, 60);
    driftKnob.setBounds (936, 66, 70, 60);
    master.setBounds (1006, 66, 76, 60);

    // MIX page
    bloom.setBounds (16, 164, width - 32, 218);
    const int stripW = (width - 32 - 36) / 4;
    for (int l = 0; l < 4; ++l)
        strips[l]->setBounds (16 + l * (stripW + 12), 392, stripW, 306);

    for (auto& pg : pages)
        pg->setBounds (0, 128, width, height - 128 - 38);
}

void NieblaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    // Soft fog glow
    g.setGradientFill (juce::ColourGradient (fog.withAlpha (0.07f), 200, 0, fog.withAlpha (0.0f), 700, 260, true));
    g.fillRect (getLocalBounds());

    // Logo
    g.setColour (fog);
    g.setFont (font (30.0f).withExtraKerningFactor (0.34f));
    g.drawText ("NIEBLA", 22, 12, 170, 42, juce::Justification::centredLeft);

    // Tabs
    for (int i = 0; i < 5; ++i)
    {
        const auto r = tabRects[i].toFloat();
        const bool on = i == page;
        g.setColour (on ? raised : juce::Colours::transparentBlack);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (on ? muted : line);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        g.setFont (font (14.0f));
        g.setColour (on ? fog : muted);
        if (i == 0)
            g.drawText ("MIX", r, juce::Justification::centred);
        else
        {
            g.setColour (layerColours[i - 1]);
            g.fillEllipse (r.getX() + 16, r.getCentreY() - 4, 8, 8);
            g.setColour (on ? fog : muted);
            g.drawText (juce::String::charToString ((juce::juce_wchar) layerLetters[i - 1]), r.withTrimmedLeft (28), juce::Justification::centredLeft);
        }
    }

    // Preset browser
    g.setColour (muted);
    g.setFont (font (10.0f));
    g.drawText ("Preset", presetRect.getX(), 2, 100, 14, juce::Justification::centredLeft);
    g.drawText ("Host tempo", tempoRect.getX(), 2, 100, 14, juce::Justification::centredLeft);
    g.setColour (panel);
    g.fillRoundedRectangle (presetRect.toFloat(), 6.0f);
    g.setColour (line);
    g.drawRoundedRectangle (presetRect.toFloat().reduced (0.5f), 6.0f, 1.0f);
    g.setColour (muted);
    g.setFont (font (13.0f));
    g.drawText (juce::String::fromUTF8 ("\xe2\x97\x82"), prevRect, juce::Justification::centred);
    g.drawText (juce::String::fromUTF8 ("\xe2\x96\xb8"), nextRect, juce::Justification::centred);
    g.setColour (fog);
    g.setFont (font (14.0f));
    g.drawText (processor.getCurrentPresetName(), presetRect.reduced (32, 0), juce::Justification::centred);

    g.setColour (panel);
    g.fillRoundedRectangle (tempoRect.toFloat(), 6.0f);
    g.setColour (line);
    g.drawRoundedRectangle (tempoRect.toFloat().reduced (0.5f), 6.0f, 1.0f);
    g.setColour (fog);
    g.drawText (juce::String (juce::roundToInt (processor.getHostBpm())) + " BPM", tempoRect, juce::Justification::centred);

    g.setColour (line);
    g.drawHorizontalLine (64, 0.0f, (float) width);

    // Global scale bar
    g.setColour (juce::Colours::black.withAlpha (0.08f));
    g.fillRect (0, 65, width, 62);
    g.setColour (muted);
    g.setFont (font (12.0f));
    g.drawText ("Global scale", 22, 76, 90, 30, juce::Justification::centredLeft);
    g.setFont (font (10.0f));
    g.drawText ("Root", 116, 62, 60, 14, juce::Justification::centredLeft);
    g.drawText ("Scale", 190, 62, 60, 14, juce::Justification::centredLeft);

    const int root = (int) processor.apvts.getRawParameterValue ("root")->load();
    const int scale = (int) processor.apvts.getRawParameterValue ("scale")->load();
    const auto notes = niebla::noteNameList();
    for (int i = 0; i < 12; ++i)
    {
        const auto r = juce::Rectangle<float> ((float) keysRect.getX() + i * 21, (float) keysRect.getY(), 19, 24);
        const bool in = niebla::isInScale (i, root, scale);
        g.setColour (in ? juce::Colour (0xff5e7470) : panel);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (in ? juce::Colour (0xff7fa39c) : line);
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        g.setColour (in ? fog : dim);
        g.setFont (font (9.0f, i == root));
        g.drawText (notes[i], r.withTrimmedTop (8), juce::Justification::centred);
    }
    g.setColour (muted);
    g.setFont (font (12.0f));
    g.drawText ("Force to scale", 640, 78, 100, 26, juce::Justification::centredLeft);

    g.setColour (line);
    g.drawHorizontalLine (127, 0.0f, (float) width);

    if (page == 0)
    {
        g.setColour (fog);
        g.setFont (font (16.0f));
        g.drawText ("Bloom", 18, 136, 70, 24, juce::Justification::centredLeft);
        g.setColour (muted);
        g.setFont (font (12.0f));
        g.drawText ("How one played note unfolds across the four layers. Drag the handles to shape it.", 84, 136, 700, 24, juce::Justification::centredLeft);
    }

    // Hover help bar
    const auto foot = juce::Rectangle<int> (0, height - 38, width, 38);
    g.setColour (juce::Colours::black.withAlpha (0.1f));
    g.fillRect (foot);
    g.setColour (line);
    g.drawHorizontalLine (foot.getY(), 0.0f, (float) width);
    g.setColour (fog);
    g.setFont (font (14.0f));
    const int titleW = juce::GlyphArrangement::getStringWidthInt (font (14.0f), helpTitle) + 16;
    g.drawText (helpTitle, 22, foot.getY(), titleW, foot.getHeight(), juce::Justification::centredLeft);
    g.setColour (muted);
    g.setFont (font (12.0f));
    g.drawText (helpText, 22 + titleW, foot.getY(), width - 44 - titleW, foot.getHeight(), juce::Justification::centredLeft);
}

void NieblaAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.eventComponent != this) return;
    const auto pos = e.getPosition();

    for (int i = 0; i < 5; ++i)
        if (tabRects[i].contains (pos)) { showPage (i); return; }

    if (prevRect.contains (pos)) { processor.loadPreset (processor.getCurrentPresetIndex() - 1); repaint(); return; }
    if (nextRect.contains (pos)) { processor.loadPreset (processor.getCurrentPresetIndex() + 1); repaint(); return; }

    if (presetRect.contains (pos))
    {
        juce::PopupMenu menu;
        const auto names = processor.getPresetNames();
        for (int i = 0; i < names.size(); ++i)
        {
            if (i == NieblaAudioProcessor::getNumFactoryPresets() && i > 0)
                menu.addSeparator();
            menu.addItem (i + 1, names[i], true, i == processor.getCurrentPresetIndex());
        }
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (presetRect)),
                            [this] (int result) { if (result > 0) { processor.loadPreset (result - 1); repaint(); } });
    }
}

void NieblaAudioProcessorEditor::mouseEnter (const juce::MouseEvent& e)
{
    for (auto* c = e.eventComponent; c != nullptr && c != this; c = c->getParentComponent())
    {
        const auto key = c->getProperties()["hover"].toString();
        if (key.isNotEmpty()) { showHelp (key); return; }
    }
}

void NieblaAudioProcessorEditor::showHelp (const juce::String& key)
{
    if (key.startsWith ("char"))
    {
        const int knob = key.substring (4, 5).getIntValue() - 1;
        const int layer = key.fromFirstOccurrenceOf ("@", false, false).getIntValue();
        const int v = juce::jlimit (0, 9, (int) processor.apvts.getRawParameterValue (layerPrefix (layer) + "voice")->load());
        helpTitle = characterNames[v][knob];
        helpText = characterHelp[v][knob];
    }
    else
    {
        for (auto& h : helpTable)
            if (key == h.key) { helpTitle = h.title; helpText = h.text; break; }
    }
    repaint (0, height - 38, width, 38);
}

void NieblaAudioProcessorEditor::savePreset()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name your sound:", juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel (&lookAndFeel);
    w->addTextEditor ("name", processor.getCurrentPresetName());
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<juce::AlertWindow> safe (w);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, safe] (int result)
    {
        if (result == 1 && safe != nullptr)
            processor.saveUserPreset (safe->getTextEditorContents ("name"));
        repaint();
    }), true);
}

void NieblaAudioProcessorEditor::timerCallback()
{
    // Repaint only when something changed (a knob, automation, a preset or the tempo)
    double snapshot = processor.getHostBpm() * 7.0 + processor.getCurrentPresetIndex() * 13.0;
    int i = 1;
    for (auto* p : processor.getParameters())
        snapshot += p->getValue() * (i++ % 97 + 1);

    if (snapshot != lastSnapshot)
    {
        lastSnapshot = snapshot;
        if (page > 0) pages[page - 1]->refresh();
        repaint();
    }
}
