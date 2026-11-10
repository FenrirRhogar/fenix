#pragma once

#include "PresetBar.h"
#include "RackUI.h"
#include "UpdateCheck.h"
#include "Widgets.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace fenix
{

// The Fehu rune, Fenix's mark, in a 20x20 box.
inline juce::Path fehuMark() { return rackui::rune ({ { 7, 2, 7, 18 }, { 7, 8, 15, 2.5f }, { 7, 12.5f, 15, 7 } }); }

//==============================================================================
// What goes on a single-plugin faceplate: knobs either side of a centre piece.
struct KnobSpec
{
    juce::String id;
    bool big;
    std::vector<rackui::Mark> marks;
    juce::String unit;
    juce::String name = {};
};

enum class Centre { vu, display, ladder };

struct RackSpec
{
    std::vector<KnobSpec> left, right;
    Centre centre = Centre::vu;
    int centreWidth = 172;
};

//==============================================================================
// Everything every Fenix unit shares: the plate, rack ears, nameplate with preset panel, IN switch,
// small print, a cached background and a 60 Hz frame timer.
class RackBase : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int ear = rackui::earWidth;

    explicit RackBase (Processor& p) : AudioProcessorEditor (p), proc (p), presetBar (p)
    {
        lnf.setAccent (p.accent());
        setLookAndFeel (&lnf);
        inAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "bypass", inSwitch);
        addAndMakeVisible (inSwitch);
        addAndMakeVisible (presetBar);
        addAndMakeVisible (updateBadge);
        sizeSelect.onChange = [this] (int i) { setPanelSize (i); };
        addAndMakeVisible (sizeSelect);
    }

    ~RackBase() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override { g.drawImageAt (background, 0, 0); }

    // The panel is laid out at its base size and scaled as a whole to the size picked on SIZE.
    void resized() override
    {
        if (baseWidth == 0)
            return;
        scale = (float) getWidth() / (float) baseWidth;

        presetBar.setBounds (ear + 30, 112, 206, 66);
        updateBadge.setBounds (baseWidth - ear - 330, baseHeight - 20, 300, 14);
        sizeSelect.setBounds (baseWidth / 2 - 20, baseHeight - 20, 92, 14);
        layout();
        rackui::scaleChildren (*this, scale);

        background = juce::Image (juce::Image::RGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), false);
        juce::Graphics g (background);
        g.addTransform (juce::AffineTransform::scale (scale));
        const auto all = juce::Rectangle<float> ((float) baseWidth, (float) baseHeight);
        rackui::plate (g, all.reduced ((float) ear, 0.0f));
        rackui::ears (g, all);
        rackui::nameplate (g, { (float) ear + 30.0f, 26.0f }, "FENIX", proc.getName().fromFirstOccurrenceOf ("Fenix ", false, false).toUpperCase(),
                           proc.tagline().toUpperCase(), fehuMark(), rackui::colour::ink);
        rackui::print (g, "fenrikos", { (float) ear + 30.0f, (float) baseHeight - 20.0f, 200.0f, 14.0f }, rackui::Face::textMedium, 8.0f, 0.3f,
                       rackui::colour::inkFaint, juce::Justification::centredLeft);
        rackui::print (g, "SIZE", { (float) sizeSelect.getX() - 40.0f, (float) sizeSelect.getY(), 34.0f, 14.0f }, rackui::Face::textMedium, 8.0f, 0.3f,
                       rackui::colour::inkFaint, juce::Justification::centredRight);
        paintPanel (g);
    }

protected:
    virtual void layout() = 0;
    virtual void paintPanel (juce::Graphics&) {}
    virtual void tick() {}

    void start (int plateWidth, int plateHeight)
    {
        baseWidth = plateWidth + 2 * ear;
        baseHeight = plateHeight;
        setPanelSize ((int) proc.apvts.state.getProperty ("uiSize", 1));
        startTimerHz (60);
    }

    // S / M / L: the whole panel at 80, 100 or 125 %. Saved with the session.
    void setPanelSize (int index)
    {
        index = juce::jlimit (0, 2, index);
        sizeSelect.setSelected (index);
        proc.apvts.state.setProperty ("uiSize", index, nullptr);
        const auto s = rackui::sizeScale (index);
        setSize (juce::roundToInt ((float) baseWidth * s), juce::roundToInt ((float) baseHeight * s));
    }

    int baseWidth = 0, baseHeight = 0;
    float scale = 1.0f;
    rackui::LedSelector sizeSelect { rackui::sizeNames(), juce::Colour (0xffffc928) };

    rackui::Knob& addKnob (const juce::String& id, bool big, std::vector<rackui::Mark> marks, const juce::String& unit,
                           juce::String name, juce::Colour accent, float bigSize = 76.0f, float smallSize = 52.0f)
    {
        auto& param = *proc.apvts.getParameter (id);
        if (name.isEmpty())
            name = param.getName (32).toUpperCase();
        auto& knob = *knobs.emplace_back (std::make_unique<rackui::Knob> (big ? bigSize : smallSize, big ? rackui::Knob::Variant::metal : rackui::Knob::Variant::black,
                                                                          accent, name, unit, std::move (marks)));
        attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, knob));
        knob.setDoubleClickReturnValue (true, param.convertFrom0to1 (param.getDefaultValue()));
        addAndMakeVisible (knob);
        return knob;
    }

    std::unique_ptr<rackui::VuMeter> makeVu (const juce::String& label, std::atomic<float>& rms, std::atomic<float>& peak)
    {
        auto vu = std::make_unique<rackui::VuMeter> (label, [&rms] { return rms.load(); }, [&peak] { return peak.load(); });
        vu->setCalibration ((float) proc.apvts.state.getProperty ("vuCal", -10.0f));
        vu->onCalibrationChanged = [this] (float cal) { proc.apvts.state.setProperty ("vuCal", cal, nullptr); };
        return vu;
    }

    Processor& proc;
    LookAndFeel lnf;
    PresetBar presetBar;
    rackui::InSwitch inSwitch;
    std::vector<std::unique_ptr<rackui::Knob>> knobs;

private:
    void timerCallback() override
    {
        tick();
        presetBar.refresh();
    }

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> inAttachment;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    fenrikos::UpdateBadge updateBadge { juce::String (juce::CharPointer_UTF8 ("FENIX COLOUR SERIES  \xc2\xb7  V")) + JucePlugin_VersionString,
                                        rackui::font (rackui::Face::textMedium, 8.0f, 0.3f), rackui::colour::inkFaint, proc.accent(),
                                        juce::Justification::centredRight };
    juce::Image background;
};

//==============================================================================
// HF Comp's meter: a ten-LED ladder reading gain reduction, input or output, chosen by a source
// selector. Level readings follow the same calibration as the VU meters.
class LadderMeter : public juce::Component
{
public:
    explicit LadderMeter (Processor& p) : proc (p)
    {
        addAndMakeVisible (ladder);
        addAndMakeVisible (source);
        source.onChange = [this] (int s) { setSource (s); };
        setSource ((int) proc.apvts.state.getProperty ("meter", 0));
    }

    void tick (float dt)
    {
        float reading = proc.meter.load();
        if (meterSource > 0)
        {
            const auto rms = meterSource == 1 ? proc.inputRms.load() : proc.outputRms.load();
            reading = juce::Decibels::gainToDecibels (rms, -100.0f) - (float) proc.apvts.state.getProperty ("vuCal", -10.0f);
        }
        if (ladder.tick (reading, dt))
            ladder.repaint();
    }

    void resized() override
    {
        ladder.setBounds (34, 22, 22, 150);
        source.setBounds (100, 52, 60, 66);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace rackui;
        print (g, meterSource == 0 ? "REDUCTION" : meterSource == 1 ? "INPUT" : "OUTPUT", { 0.0f, 0.0f, 90.0f, 14.0f }, Face::legend, 11.0f, 0.2f);
        const char* gr[] { "1", "2", "3", "4", "6", "8", "10", "12", "15", "20" };
        const char* vu[] { "-20", "-10", "-7", "-5", "-3", "-2", "-1", "0", "+1", "+3" };
        const auto box = ladder.getBounds().toFloat();
        for (int i = 0; i < LedLadder::count; ++i)
        {
            const auto led = ladder.ledBounds (i).translated (box.getX(), box.getY());
            print (g, gr[i], { 0.0f, led.getCentreY() - 6.0f, box.getX() - 6.0f, 12.0f }, Face::text, 9.5f, 0.0f,
                   meterSource == 0 ? colour::inkSoft : colour::inkFaint, juce::Justification::centredRight);
            print (g, vu[i], { box.getRight() + 6.0f, led.getCentreY() - 6.0f, 30.0f, 12.0f }, Face::text, 9.5f, 0.0f,
                   meterSource == 0 ? colour::inkFaint : (i >= 7 ? juce::Colour (0xffe0675c) : colour::inkSoft), juce::Justification::centredLeft);
        }
        print (g, "dB GR", { 0.0f, box.getBottom() + 6.0f, box.getX() + 6.0f, 10.0f }, Face::textMedium, 8.0f, 0.12f, colour::inkDim, juce::Justification::centredRight);
        print (g, "VU", { box.getRight() + 6.0f, box.getBottom() + 6.0f, 30.0f, 10.0f }, Face::textMedium, 8.0f, 0.12f, colour::inkDim, juce::Justification::centredLeft);
        print (g, "SOURCE", { 100.0f, 124.0f, 60.0f, 10.0f }, Face::textMedium, 8.0f, 0.18f, colour::inkDim, juce::Justification::centredLeft);
    }

private:
    void setSource (int s)
    {
        meterSource = juce::jlimit (0, 2, s);
        source.setSelected (meterSource);
        proc.apvts.state.setProperty ("meter", meterSource, nullptr);
        ladder.setThresholds (meterSource == 0 ? std::array<float, 10> { 1, 2, 3, 4, 6, 8, 10, 12, 15, 20 }
                                               : std::array<float, 10> { -20, -10, -7, -5, -3, -2, -1, 0, 1, 3 });
        repaint();
    }

    Processor& proc;
    rackui::LedLadder ladder;
    rackui::LedSelector source { { "GR", "IN", "OUT" }, juce::Colour (0xffffc928), true };
    int meterSource = 0;
};

//==============================================================================
// A single Fenix plugin as one rack unit: nameplate | knobs | centre piece | knobs | IN.
class RackEditor : public RackBase
{
public:
    static constexpr int plateHeight = 250;

    RackEditor (Processor& p, RackSpec rackSpec) : RackBase (p), spec (std::move (rackSpec))
    {
        for (auto* side : { &spec.left, &spec.right })
            for (const auto& k : *side)
                (side == &spec.left ? leftKnobs : rightKnobs).push_back (&addKnob (k.id, k.big, k.marks, k.unit, k.name, p.accent()));

        if (spec.centre == Centre::vu)
            vu = makeVu ("OUTPUT", p.outputRms, p.outputPeak);
        else if (spec.centre == Centre::display)
            display = p.createDisplay();
        else
            ladderMeter = std::make_unique<LadderMeter> (p);
        addAndMakeVisible (centre());

        // Plate width follows the contents.
        int width = 30 + 206 + gap + 2 + gap + gap + 2 + gap + 50 + 30;
        for (const auto& k : knobs)
            width += k->preferredSize().x;
        width += spec.centreWidth + 2 * gap;
        start (width, plateHeight);
    }

protected:
    static constexpr int gap = 18;

    juce::Component& centre()
    {
        if (vu != nullptr)
            return *vu;
        if (display != nullptr)
            return *display;
        return *ladderMeter;
    }

    void layout() override
    {
        int x = ear + 30 + 206 + gap;
        dividers.clear();
        dividers.push_back (x);
        x += 2 + gap;
        const auto placeKnob = [&] (rackui::Knob* k) {
            const auto s = k->preferredSize();
            k->setBounds (x, (plateHeight - s.y) / 2 - 2, s.x, s.y);
            x += s.x;
        };
        for (auto* k : leftKnobs)
            placeKnob (k);
        x += gap;
        const int centreHeight = spec.centre == Centre::vu ? 172 : spec.centre == Centre::display ? 176 : 190;
        centre().setBounds (x, (plateHeight - centreHeight) / 2 - 4, spec.centreWidth, centreHeight);
        x += spec.centreWidth + gap;
        for (auto* k : rightKnobs)
            placeKnob (k);
        x += gap;
        dividers.push_back (x);
        x += 2 + gap;
        inSwitch.setBounds (x, (plateHeight - 64) / 2 - 6, 50, 64);
    }

    void paintPanel (juce::Graphics& g) override
    {
        for (auto x : dividers)
            rackui::divider (g, (float) x, 37.0f, 176.0f);
    }

    void tick() override
    {
        constexpr float dt = 1.0f / 60.0f;
        if (vu != nullptr && vu->tick (dt))
            vu->repaint();
        if (display != nullptr)
        {
            display->tick();
            display->repaint();
        }
        if (ladderMeter != nullptr)
            ladderMeter->tick (dt);
    }

    RackSpec spec;
    std::vector<rackui::Knob*> leftKnobs, rightKnobs;
    std::unique_ptr<rackui::VuMeter> vu;
    std::unique_ptr<Display> display;
    std::unique_ptr<LadderMeter> ladderMeter;
    std::vector<int> dividers;
};

// Fallback for a plugin that does not describe its panel: every parameter as a small knob around a scope.
inline juce::AudioProcessorEditor* Processor::createEditor()
{
    RackSpec spec;
    spec.centre = Centre::display;
    spec.centreWidth = 260;
    for (auto* raw : getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (raw); p != nullptr && p->getParameterID() != "bypass")
            spec.right.push_back ({ p->getParameterID(), false, {}, p->getLabel() });
    return new RackEditor (*this, spec);
}

inline std::unique_ptr<Display> Processor::createDisplay() { return std::make_unique<ScopeDisplay> (*this); }

// Scale marks: evenly spaced plain numbers, or explicit (value, legend) pairs.
inline std::vector<rackui::Mark> marks (std::initializer_list<std::pair<double, const char*>> m)
{
    std::vector<rackui::Mark> out;
    for (auto [v, t] : m)
        out.push_back ({ v, t });
    return out;
}

inline std::vector<rackui::Mark> dbMarks() { return marks ({ { -12, "-12" }, { -6, "-6" }, { 0, "0" }, { 6, "+6" }, { 12, "+12" } }); }
inline std::vector<rackui::Mark> pctMarks() { return marks ({ { 0, "0" }, { 25, "25" }, { 50, "50" }, { 75, "75" }, { 100, "100" } }); }

} // namespace fenix
