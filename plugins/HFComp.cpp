// Fenix HF Comp: high-band-only compression, as a standalone plugin.
// The DSP, parameters, presets and display live in common/modules/HFComp.h,
// shared with Fenix Bus.

#include "Editor.h"
#include "Hardware.h"
#include "modules/HFComp.h"

using namespace fenix;

class HFCompProcessor final : public Processor
{
public:
    HFCompProcessor() : Processor (makeLayout())
    {
        module.attach (apvts);
        factoryPresets = hfcomp::presets();
    }

    static Layout makeLayout()
    {
        Layout layout;
        hfcomp::addParameters (layout);
        return layout;
    }

    juce::String tagline() const override { return "High-frequency compressor"; }
    juce::Colour accent() const override { return hfcomp::accent(); }
    bool reductionMeter() const override { return true; }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        module.prepare (sampleRate, maxBlockSize);
        setLatencySamples (module.latency());
    }

    void process (juce::AudioBuffer<float>& buffer) override
    {
        module.process (buffer, getTotalNumInputChannels());
        meter.store (module.reduction.load());
    }

    std::unique_ptr<Display> createDisplay() override { return std::make_unique<hfcomp::View> (*this, module); }
    juce::AudioProcessorEditor* createEditor() override;

private:
    hfcomp::Module module;
};

//==============================================================================
// HF Comp drawn as one piece of rack hardware: a painted 2U-style faceplate with four knobs,
// an LED ladder meter with a source selector and an IN switch. Everything that only exists in
// software (presets, the value readout, the version) lives in a toolbar strip under the unit.
class HFCompRackEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int ear = 34, plateWidth = 920, plateHeight = 250, toolbarHeight = 36;

    explicit HFCompRackEditor (Processor& p) : AudioProcessorEditor (p), proc (p), presetBar (p)
    {
        lnf.setAccent (p.accent());
        setLookAndFeel (&lnf);

        threshold = &addKnob ("threshold", 40.0f, { { -40, "-40" }, { -35, "" }, { -30, "-30" }, { -25, "" }, { -20, "-20" },
                                                    { -15, "" }, { -10, "-10" }, { -5, "" }, { 0, "0" } });
        crossover = &addKnob ("crossover", 28.0f, { { 2000, "2" }, { 3000, "3" }, { 4000, "4" }, { 5000, "" }, { 6000, "6" },
                                                    { 8000, "8" }, { 10000, "" }, { 12000, "12" }, { 16000, "16" } });
        mix = &addKnob ("mix", 28.0f, { { 0, "0" }, { 12.5f, "" }, { 25, "25" }, { 37.5f, "" }, { 50, "50" }, { 62.5f, "" },
                                        { 75, "75" }, { 87.5f, "" }, { 100, "100" } });
        output = &addKnob ("output", 40.0f, { { -12, "-12" }, { -9, "" }, { -6, "-6" }, { -3, "" }, { 0, "0" }, { 3, "" },
                                              { 6, "+6" }, { 9, "" }, { 12, "+12" } });

        power.setTooltip ("Engage / bypass");
        powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "bypass", power);
        addAndMakeVisible (power);

        meterSelect.onClick = [this] { setMeterSource ((meterSource + 1) % 3); };
        addAndMakeVisible (meterSelect);
        addAndMakeVisible (ladder);
        setMeterSource ((int) p.apvts.state.getProperty ("meter", 0));

        addAndMakeVisible (presetBar);
        addAndMakeVisible (updateBadge);

        setSize (plateWidth + 2 * ear, plateHeight + toolbarHeight);
        startTimerHz (60);
    }

    ~HFCompRackEditor() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        g.drawImageAt (background, 0, 0);

        if (readout.isNotEmpty())
        {
            const auto bar = getLocalBounds().removeFromBottom (toolbarHeight).toFloat();
            hw::print (g, readout, bar.withSizeKeepingCentre (300.0f, bar.getHeight()).translated (60.0f, 0.0f), 12.0f,
                       juce::Justification::centred, false, hw::ink::legend, 0.06f);
        }
    }

    void resized() override
    {
        const int x = ear;
        const auto at = [x] (hw::Knob& k, int cx, int cy) {
            const int side = juce::roundToInt (k.knobRadius() * 2.0f + 64.0f);
            k.setBounds (juce::Rectangle<int> (side, side).withCentre ({ x + cx, cy }));
        };
        at (*threshold, 262, 128);
        at (*crossover, 404, 128);
        at (*mix, 736, 128);
        at (*output, 854, 128);

        power.setBounds (x + 70, 104, 36, 42);
        ladder.setBounds (x + 541, 60, 22, 140);
        meterSelect.setBounds (x + 598, 82, 64, 104);

        const int barY = plateHeight;
        presetBar.setBounds (14, barY + 5, 380, 26);
        updateBadge.setBounds (getWidth() - 274, barY, 260, toolbarHeight);

        background = juce::Image (juce::Image::RGB, getWidth(), getHeight(), false);
        juce::Graphics g (background);
        paintPanel (g);
    }

private:
    // Meter source: gain reduction, input level or output level, cycled by the METER button.
    class MeterSelect : public juce::Button
    {
    public:
        MeterSelect() : Button ("METER") { setTooltip ("Meter source"); }

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            const auto b = getLocalBounds().toFloat();
            const char* names[] { "GR", "IN", "OUT" };
            for (int i = 0; i < 3; ++i)
            {
                const auto y = b.getY() + 9.0f + (float) i * 18.0f;
                hw::roundLed (g, { b.getX() + 12.0f, y }, 3.5f, juce::Colour (0xffffc928), i == source ? 1.0f : 0.0f);
                hw::print (g, names[i], { b.getX() + 22.0f, y - 6.0f, 40.0f, 12.0f }, 9.5f, juce::Justification::centredLeft);
            }
            hw::pushCap (g, { b.getX() + 6.0f, b.getBottom() - 26.0f, 34.0f, 22.0f }, over, down);
        }

        int source = 0;
    };

    hw::Knob& addKnob (const char* id, float radius, std::vector<hw::Mark> marks)
    {
        auto& param = *proc.apvts.getParameter (id);
        auto& knob = *knobs.emplace_back (std::make_unique<hw::Knob> (param, std::move (marks), radius));
        attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, knob));
        knob.setDoubleClickReturnValue (true, param.convertFrom0to1 (param.getDefaultValue()));
        addAndMakeVisible (knob);
        return knob;
    }

    void setMeterSource (int source)
    {
        meterSource = juce::jlimit (0, 2, source);
        meterSelect.source = meterSource;
        meterSelect.repaint();
        proc.apvts.state.setProperty ("meter", meterSource, nullptr);
        // Gain reduction in dB, or level in VU (0 VU = -18 dBFS), bottom LED to top.
        ladder.setThresholds (meterSource == 0 ? std::array<float, 10> { 1, 2, 3, 4, 6, 8, 10, 12, 15, 20 }
                                               : std::array<float, 10> { -20, -10, -7, -5, -3, -2, -1, 0, 1, 3 });
    }

    float meterReading() const
    {
        if (meterSource == 0)
            return proc.meter.load();
        const auto rms = meterSource == 1 ? proc.inputRms.load() : proc.outputRms.load();
        return juce::Decibels::gainToDecibels (rms, -100.0f) + 18.0f;
    }

    void timerCallback() override
    {
        if (ladder.tick (meterReading(), 1.0f / 60.0f))
            ladder.repaint();
        presetBar.refresh();

        // The toolbar names the control under the mouse and its value.
        juce::String text;
        for (auto& k : knobs)
            if (k->isMouseOverOrDragging())
                text = k->getName() + "    " + k->getTextFromValue (k->getValue());
        if (text != readout)
        {
            readout = text;
            repaint (getLocalBounds().removeFromBottom (toolbarHeight));
        }
    }

    void paintPanel (juce::Graphics& g)
    {
        const auto plateArea = juce::Rectangle<float> (0.0f, 0.0f, (float) getWidth(), (float) plateHeight);
        hw::powderCoat (g, plateArea);
        draw::rackEars (g, plateArea, (float) ear);
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.fillRect (plateArea.withHeight (1.0f));
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillRect (plateArea.withTop (plateArea.getBottom() - 1.0f));

        const float x = (float) ear;
        const auto legend = hw::ink::legend, dim = hw::ink::legendDim;
        const auto modelInk = juce::Colour (0xff86c4d1);   // the one printed colour on the panel

        // Maker and model, top left.
        draw::fillFehu (g, { x + 24.0f, 24.0f, 28.0f, 28.0f });
        hw::print (g, "FENIX", { x + 56.0f, 24.0f, 120.0f, 18.0f }, 18.0f, juce::Justification::centredLeft, true, legend, 0.32f);
        hw::print (g, "HF COMP", { x + 57.0f, 42.0f, 120.0f, 12.0f }, 10.5f, juce::Justification::centredLeft, true, modelInk, 0.3f);
        hw::print (g, "HIGH FREQUENCY COMPRESSOR", { x + 26.0f, 66.0f, 200.0f, 10.0f }, 7.5f, juce::Justification::centredLeft, false, dim, 0.18f);

        // IN switch, bottom left.
        hw::print (g, "IN", { x + 70.0f, 148.0f, 36.0f, 12.0f }, 9.5f);

        // Knob legends: name, then the unit the scale is printed in.
        const auto name = [&] (hw::Knob& k, const juce::String& title, const juce::String& unit) {
            const auto cx = (float) k.getBounds().getCentreX();
            hw::print (g, title, { cx - 60.0f, 196.0f, 120.0f, 14.0f }, 11.0f, juce::Justification::centred, true, legend, 0.16f);
            hw::print (g, unit, { cx - 60.0f, 211.0f, 120.0f, 10.0f }, 8.0f, juce::Justification::centred, false, dim, 0.12f);
        };
        name (*threshold, "THRESHOLD", "dB");
        name (*crossover, "CROSSOVER", "kHz");
        name (*mix, "MIX", "% WET");
        name (*output, "OUTPUT", "dB");

        // Meter section: a printed box, the ladder's scales either side, and the selector.
        const auto meterBox = juce::Rectangle<float> (x + 494.0f, 42.0f, 174.0f, 180.0f);
        {
            // The rule breaks around the title, as printed.
            const juce::Graphics::ScopedSaveState save (g);
            g.excludeClipRegion (juce::Rectangle<float> (60.0f, 16.0f).withCentre ({ meterBox.getCentreX(), meterBox.getY() }).toNearestInt());
            g.setColour (legend.withAlpha (0.5f));
            g.drawRoundedRectangle (meterBox, 4.0f, 1.0f);
        }
        hw::print (g, "METER", juce::Rectangle<float> (60.0f, 14.0f).withCentre ({ meterBox.getCentreX(), meterBox.getY() }), 9.5f,
                   juce::Justification::centred, true, legend, 0.2f);
        const auto ladderBox = ladder.getBounds().toFloat();
        const char* gr[] { "1", "2", "3", "4", "6", "8", "10", "12", "15", "20" };
        const char* vu[] { "-20", "-10", "-7", "-5", "-3", "-2", "-1", "0", "+1", "+3" };
        for (int i = 0; i < hw::LedLadder::count; ++i)
        {
            const auto led = ladder.ledBounds (i).translated (ladderBox.getX(), ladderBox.getY());
            hw::print (g, gr[i], { ladderBox.getX() - 32.0f, led.getCentreY() - 6.0f, 28.0f, 12.0f }, 8.5f,
                       juce::Justification::centredRight, false, legend, 0.0f);
            hw::print (g, vu[i], { ladderBox.getRight() + 4.0f, led.getCentreY() - 6.0f, 28.0f, 12.0f }, 8.5f,
                       juce::Justification::centredLeft, false, i >= 7 ? juce::Colour (0xffe0675c) : legend, 0.0f);
        }
        hw::print (g, "GR", { ladderBox.getX() - 32.0f, ladderBox.getBottom() + 4.0f, 28.0f, 10.0f }, 7.5f,
                   juce::Justification::centredRight, true, dim, 0.12f);
        hw::print (g, "VU", { ladderBox.getRight() + 4.0f, ladderBox.getBottom() + 4.0f, 28.0f, 10.0f }, 7.5f,
                   juce::Justification::centredLeft, true, dim, 0.12f);
        hw::print (g, "SOURCE", { (float) meterSelect.getX(), (float) meterSelect.getBottom() + 2.0f, 46.0f, 10.0f }, 7.5f,
                   juce::Justification::centred, true, dim, 0.12f);

        // Small print along the bottom edge.
        hw::print (g, "FENRIKOS", { x + 24.0f, (float) plateHeight - 22.0f, 200.0f, 10.0f }, 7.5f,
                   juce::Justification::centredLeft, true, dim, 0.3f);
        hw::print (g, "FENIX COLOUR SERIES", { (float) (ear + plateWidth) - 224.0f, (float) plateHeight - 22.0f, 200.0f, 10.0f }, 7.5f,
                   juce::Justification::centredRight, true, dim, 0.3f);

        // Toolbar strip: plain software chrome under the unit.
        const auto bar = juce::Rectangle<float> (0.0f, (float) plateHeight, (float) getWidth(), (float) toolbarHeight);
        g.setColour (juce::Colour (0xff111214));
        g.fillRect (bar);
        g.setColour (juce::Colours::black);
        g.fillRect (bar.withHeight (1.0f));
    }

    Processor& proc;
    LookAndFeel lnf;
    std::vector<std::unique_ptr<hw::Knob>> knobs;
    hw::Knob* threshold = nullptr;
    hw::Knob* crossover = nullptr;
    hw::Knob* mix = nullptr;
    hw::Knob* output = nullptr;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    hw::LedButton power { juce::Colour (0xffff3b2e), false };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    hw::LedLadder ladder;
    MeterSelect meterSelect;
    int meterSource = 0;
    PresetBar presetBar;
    fenrikos::UpdateBadge updateBadge { juce::String (juce::CharPointer_UTF8 ("FENIX HF COMP  \xc2\xb7  V")) + JucePlugin_VersionString,
                                        juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.2f),
                                        colours::engraveDim, proc.accent(), juce::Justification::centredRight };
    juce::String readout;
    juce::Image background;
};

juce::AudioProcessorEditor* HFCompProcessor::createEditor() { return new HFCompRackEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HFCompProcessor(); }
