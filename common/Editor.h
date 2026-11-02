#pragma once

#include "PresetBar.h"
#include "Widgets.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace fenix
{

// Chrome shared by every Fenix editor: header with logo, preset bar and power switch,
// input/output meters, a background cached as an image, and a 60 Hz frame timer.
// Subclasses add their controls, lay them out in layout() and draw static panels in paintPanels().
class EditorBase : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EditorBase (Processor& p) : AudioProcessorEditor (p), proc (p), power (p.accent()), presetBar (p)
    {
        lnf.setAccent (p.accent());
        setLookAndFeel (&lnf);

        powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "bypass", power);
        addAndMakeVisible (power);
        addAndMakeVisible (presetBar);

        leftMeter = p.reductionMeter()
                        ? std::make_unique<VuMeter> ("REDUCTION", VuMeter::Mode::reduction, [&p] { return p.meter.load(); }, p.accent())
                        : std::make_unique<VuMeter> ("INPUT", VuMeter::Mode::level, [&p] { return p.inputRms.load(); }, p.accent());
        rightMeter = std::make_unique<VuMeter> ("OUTPUT", VuMeter::Mode::level, [&p] { return p.outputRms.load(); }, p.accent());
        addAndMakeVisible (*leftMeter);
        addAndMakeVisible (*rightMeter);
    }

    ~EditorBase() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override { g.drawImageAt (background, 0, 0); }

    void resized() override
    {
        power.setBounds (getWidth() - 68, 21, 46, 24);
        presetBar.setBounds (getWidth() - 480, 19, 390, 28);
        layout();

        background = juce::Image (juce::Image::RGB, getWidth(), getHeight(), false);
        juce::Graphics g (background);
        paintFaceplate (g);
        paintPanels (g);
    }

protected:
    virtual void layout() = 0;
    virtual void paintPanels (juce::Graphics&) {}
    virtual void tick() {}

    // Call at the end of the subclass constructor, once every child exists.
    void start (int width, int height)
    {
        setSize (width, height);
        startTimerHz (60);
    }

    Knob& addKnob (juce::RangedAudioParameter& param, juce::Colour accent)
    {
        auto& knob = *knobs.emplace_back (std::make_unique<Knob> (param, accent));
        attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, param.getParameterID(), knob));
        knob.setDoubleClickReturnValue (true, param.convertFrom0to1 (param.getDefaultValue()));
        knob.setPopupMenuEnabled (false);
        addAndMakeVisible (knob);
        return knob;
    }

    Processor& proc;
    LookAndFeel lnf;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::unique_ptr<VuMeter> leftMeter, rightMeter;

private:
    void paintFaceplate (juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto accent = proc.accent();
        g.setGradientFill (juce::ColourGradient (colours::plateTop, 0.0f, 0.0f, colours::plateBottom, 0.0f, bounds.getBottom(), false));
        g.fillAll();

        // Soft accent glow from the top edge: the plugin's colour identity.
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.16f), bounds.getCentreX(), 0.0f,
                                                 accent.withAlpha (0.0f), bounds.getCentreX(), 90.0f, false));
        g.fillRect (bounds.withHeight (90.0f));
        g.setColour (accent);
        g.fillRect (bounds.withHeight (2.0f));

        const auto mark = draw::flame ({ 26.0f, 17.0f, 20.0f, 30.0f });
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffd56b), 36.0f, 18.0f, accent.darker (0.3f), 36.0f, 47.0f, false));
        g.fillPath (mark);

        g.setFont (juce::FontOptions (23.0f, juce::Font::bold).withKerningFactor (0.2f));
        draw::engraved (g, "FENIX", { 56.0f, 14.0f, 110.0f, 26.0f }, juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold).withKerningFactor (0.14f));
        g.setColour (accent.brighter (0.25f));
        g.drawText (proc.getName().fromFirstOccurrenceOf ("Fenix ", false, false).toUpperCase(),
                    juce::Rectangle<float> (160.0f, 18.0f, 220.0f, 20.0f), juce::Justification::centredLeft);
        g.setFont (draw::font (11.5f));
        g.setColour (colours::engraveDim);
        g.drawText (proc.tagline(), juce::Rectangle<float> (57.0f, 40.0f, 300.0f, 14.0f), juce::Justification::centredLeft);

        g.setColour (colours::cardEdge);
        g.fillRect (20.0f, 66.0f, bounds.getWidth() - 40.0f, 1.0f);

        g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.25f));
        g.setColour (colours::engraveDim.withAlpha (0.8f));
        g.drawText ("FENRIKOS", bounds.withTop (bounds.getBottom() - 24.0f).withTrimmedRight (24.0f), juce::Justification::centredRight);
        g.setColour (colours::engraveDim.withAlpha (0.45f));
        g.drawText ("FENIX COLOUR SERIES", bounds.withTop (bounds.getBottom() - 24.0f).withTrimmedLeft (24.0f), juce::Justification::centredLeft);
    }

    void timerCallback() override
    {
        constexpr float dt = 1.0f / 60.0f;
        if (leftMeter->tick (dt))
            leftMeter->repaint();
        if (rightMeter->tick (dt))
            rightMeter->repaint();
        tick();
        presetBar.refresh();
    }

    PowerButton power;
    PresetBar presetBar;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    juce::Image background;
};

//==============================================================================
// Single-plugin editor: meters either side of the plugin's display, and a row of knobs,
// one per parameter, with mix/output trims drawn smaller.
class Editor : public EditorBase
{
public:
    explicit Editor (Processor& p) : EditorBase (p)
    {
        for (auto* raw : p.getParameters())
        {
            auto* param = dynamic_cast<juce::RangedAudioParameter*> (raw);
            if (param == nullptr || param->getParameterID() == "bypass")
                continue;

            addKnob (*param, p.accent());
            compact.push_back (param->getParameterID() == "output" || param->getParameterID() == "mix");
        }

        display = p.createDisplay();
        addAndMakeVisible (*display);
        start (860, 500);
    }

protected:
    void layout() override
    {
        const int meterWidth = 176, top = 82, mainHeight = 200;
        leftMeter->setBounds (20, top, meterWidth, mainHeight);
        rightMeter->setBounds (getWidth() - 20 - meterWidth, top, meterWidth, mainHeight);
        display->setBounds (20 + meterWidth + 14, top, getWidth() - 2 * (20 + meterWidth + 14), mainHeight);

        knobPanel = { 20, top + mainHeight + 16, getWidth() - 40, getHeight() - (top + mainHeight + 16) - 30 };

        // Main knobs get wide cells, mix/output trims narrower ones; the group is centred.
        int total = 0;
        for (bool small : compact)
            total += small ? 112 : 150;
        auto cells = knobPanel.reduced (0, 10).withSizeKeepingCentre (juce::jmin (total, knobPanel.getWidth() - 20), knobPanel.getHeight() - 20);
        for (size_t i = 0; i < knobs.size(); ++i)
        {
            auto cell = cells.removeFromLeft (compact[i] ? 112 : 150);
            knobs[i]->setBounds (compact[i] ? cell.withTrimmedTop (22) : cell);
        }
    }

    void paintPanels (juce::Graphics& g) override { draw::card (g, knobPanel.toFloat(), 14.0f); }

    void tick() override
    {
        display->tick();
        display->repaint();
    }

private:
    std::vector<bool> compact;
    std::unique_ptr<Display> display;
    juce::Rectangle<int> knobPanel;
};

inline juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }
inline std::unique_ptr<Display> Processor::createDisplay() { return std::make_unique<ScopeDisplay> (*this); }

} // namespace fenix
