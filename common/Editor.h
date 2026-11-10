#pragma once

#include "PresetBar.h"
#include "UpdateCheck.h"
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
        addAndMakeVisible (updateBadge);
    }

    ~EditorBase() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override { g.drawImageAt (background, 0, 0); }

    // Width of each rack ear; the plate between them holds the controls.
    static constexpr int ear = 34;

    void resized() override
    {
        const auto right = getWidth() - ear;
        power.setBounds (right - 70, 20, 50, 28);
        updateBadge.setBounds (ear + 24, getHeight() - 24, getWidth() / 2, 24);
        presetBar.setBounds (right - 486, 20, 396, 28);
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
    // `width` is the plate's width; the rack ears are added either side.
    void start (int width, int height)
    {
        setSize (width + 2 * ear, height);
        startTimerHz (60);
    }

    Knob& addKnob (juce::RangedAudioParameter& param, juce::Colour accent, const juce::String& label = {})
    {
        auto& knob = *knobs.emplace_back (std::make_unique<Knob> (param, accent, label));
        attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, param.getParameterID(), knob));
        knob.setDoubleClickReturnValue (true, param.convertFrom0to1 (param.getDefaultValue()));
        knob.setPopupMenuEnabled (false);
        addAndMakeVisible (knob);
        return knob;
    }

    // The faceplate between the ears; layouts work inside this.
    juce::Rectangle<int> plate() const { return getLocalBounds().reduced (ear, 0); }

    Processor& proc;
    LookAndFeel lnf;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::unique_ptr<VuMeter> leftMeter, rightMeter;

private:
    void paintFaceplate (juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto face = plate().toFloat();
        const auto accent = proc.accent();

        draw::brushedMetal (g, bounds, colours::plateTop, colours::plateBottom);

        // Rack ears: a fold in the metal either side, each with two slotted mounting screws.
        for (auto earArea : { bounds.withWidth ((float) ear), bounds.withLeft (bounds.getRight() - (float) ear) })
        {
            g.setColour (juce::Colours::black.withAlpha (0.18f));
            g.fillRect (earArea);
            const bool left = earArea.getX() == 0.0f;
            const auto foldX = left ? earArea.getRight() : earArea.getX();
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.fillRect (foldX - (left ? 1.0f : 0.0f), 0.0f, 1.0f, bounds.getHeight());
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (foldX + (left ? 0.0f : -1.0f), 0.0f, 1.0f, bounds.getHeight());

            for (auto y : { 46.0f, bounds.getHeight() - 46.0f })
            {
                const auto slot = juce::Rectangle<float> (13.0f, 24.0f).withCentre ({ earArea.getCentreX(), y });
                draw::recess (g, slot, 6.5f, juce::Colour (0xff050506));
                draw::screw (g, { slot.getCentreX(), slot.getCentreY() + 3.0f }, 6.0f, y < 100.0f ? 0.4f : 1.1f);
            }
        }

        // Top and bottom edges of the plate catch the light.
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRect (bounds.withHeight (1.0f));
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRect (bounds.withTop (bounds.getBottom() - 1.0f));

        // Nameplate: the rune and model name silkscreened at the top left.
        const auto x = face.getX();
        draw::fillFehu (g, { x + 16.0f, 16.0f, 34.0f, 34.0f });
        g.setFont (juce::FontOptions (24.0f, juce::Font::bold).withKerningFactor (0.28f));
        draw::engraved (g, "FENIX", { x + 56.0f, 13.0f, 120.0f, 26.0f }, juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold).withKerningFactor (0.2f));
        draw::engraved (g, proc.getName().fromFirstOccurrenceOf ("Fenix ", false, false).toUpperCase(),
                        { x + 168.0f, 18.0f, 220.0f, 20.0f }, juce::Justification::centredLeft, accent.brighter (0.2f));
        g.setFont (juce::FontOptions (10.5f, juce::Font::plain).withKerningFactor (0.08f));
        g.setColour (colours::engraveDim);
        g.drawText (proc.tagline().toUpperCase(), juce::Rectangle<float> (x + 57.0f, 40.0f, 320.0f, 14.0f), juce::Justification::centredLeft);

        draw::seam (g, face.getX() + 16.0f, face.getRight() - 16.0f, 66.0f);

        g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.3f));
        g.setColour (colours::engraveDim);
        g.drawText ("FENRIKOS", face.withTop (bounds.getBottom() - 24.0f).withTrimmedRight (24.0f), juce::Justification::centredRight);
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

    // "FENIX COLOUR SERIES · V1.0.0" in the footer; says so when an update is out.
    fenrikos::UpdateBadge updateBadge { juce::String (juce::CharPointer_UTF8 ("FENIX COLOUR SERIES  \xc2\xb7  V")) + JucePlugin_VersionString,
                                        juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.25f),
                                        colours::engraveDim.withAlpha (0.45f), proc.accent(), juce::Justification::centredLeft };
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
        const auto area = plate().reduced (20, 0);
        leftMeter->setBounds (area.getX(), top, meterWidth, mainHeight);
        rightMeter->setBounds (area.getRight() - meterWidth, top, meterWidth, mainHeight);
        display->setBounds (area.getX() + meterWidth + 14, top, area.getWidth() - 2 * (meterWidth + 14), mainHeight);

        knobPanel = { area.getX(), top + mainHeight + 22, area.getWidth(), getHeight() - (top + mainHeight + 22) - 30 };

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
