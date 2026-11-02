#pragma once

#include "Widgets.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace fenix
{

// The faceplate: logo and preset bar across the top, meters either side of the
// plugin's display, and a recessed row of knobs, one per parameter.
class Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int width = 860, height = 500;

    explicit Editor (Processor& p) : AudioProcessorEditor (p), proc (p), power (p.accent())
    {
        lnf.setAccent (p.accent());
        setLookAndFeel (&lnf);

        for (auto* raw : p.getParameters())
        {
            auto* param = dynamic_cast<juce::RangedAudioParameter*> (raw);
            if (param == nullptr || param->getParameterID() == "bypass")
                continue;

            auto& knob = *knobs.emplace_back (std::make_unique<Knob> (*param, p.accent()));
            attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, param->getParameterID(), knob));
            knob.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
            knob.setPopupMenuEnabled (false);
            compact.push_back (param->getParameterID() == "output" || param->getParameterID() == "mix");
            addAndMakeVisible (knob);
        }

        powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "bypass", power);
        addAndMakeVisible (power);

        display = p.createDisplay();
        addAndMakeVisible (*display);

        leftMeter = p.reductionMeter()
                        ? std::make_unique<VuMeter> ("REDUCTION", VuMeter::Mode::reduction, [&p] { return p.meter.load(); })
                        : std::make_unique<VuMeter> ("INPUT", VuMeter::Mode::level, [&p] { return p.inputRms.load(); });
        rightMeter = std::make_unique<VuMeter> ("OUTPUT", VuMeter::Mode::level, [&p] { return p.outputRms.load(); });
        addAndMakeVisible (*leftMeter);
        addAndMakeVisible (*rightMeter);

        presets.onChange = [this] { presetChosen (presets.getSelectedId()); };
        previous.onClick = [this] { stepPreset (-1); };
        next.onClick = [this] { stepPreset (1); };
        save.onClick = [this] { askForPresetName(); };
        for (auto* c : std::initializer_list<juce::Component*> { &presets, &previous, &next, &save })
            addAndMakeVisible (c);
        refreshPresetList();

        texture = draw::brushedTexture (width, height);
        setSize (width, height);
        startTimerHz (60);
    }

    ~Editor() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (colours::plateTop, 0.0f, 0.0f, colours::plateBottom, 0.0f, bounds.getBottom(), false));
        g.fillAll();
        g.drawImageAt (texture, 0, 0);

        // Accent strip along the top edge with a soft glow, the plugin's colour identity.
        g.setGradientFill (juce::ColourGradient (proc.accent().withAlpha (0.35f), 0.0f, 0.0f,
                                                 proc.accent().withAlpha (0.0f), 0.0f, 14.0f, false));
        g.fillRect (bounds.withHeight (14.0f));
        g.setColour (proc.accent());
        g.fillRect (bounds.withHeight (2.0f));

        // Header: flame mark, suite name, plugin name and tagline.
        const auto mark = draw::flame ({ 32.0f, 16.0f, 20.0f, 30.0f });
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffd56b), 42.0f, 18.0f, proc.accent().darker (0.4f), 42.0f, 46.0f, false));
        g.fillPath (mark);

        g.setFont (juce::FontOptions (24.0f, juce::Font::bold).withKerningFactor (0.18f));
        draw::engraved (g, "FENIX", { 62.0f, 13.0f, 110.0f, 26.0f }, juce::Justification::centredLeft);
        const auto pluginName = proc.getName().fromFirstOccurrenceOf ("Fenix ", false, false).toUpperCase();
        g.setFont (juce::FontOptions (16.0f, juce::Font::bold).withKerningFactor (0.12f));
        g.setColour (proc.accent().brighter (0.2f));
        g.drawText (pluginName, juce::Rectangle<float> (166.0f, 17.0f, 200.0f, 22.0f), juce::Justification::centredLeft);
        g.setFont (draw::font (11.0f));
        draw::engraved (g, proc.tagline(), { 63.0f, 38.0f, 300.0f, 14.0f }, juce::Justification::centredLeft, colours::engraveDim);

        g.setFont (draw::font (10.0f, true));
        draw::engraved (g, "IN", juce::Rectangle<float> (power.getBounds().toFloat()).translated (0.0f, 22.0f), juce::Justification::centred, colours::engraveDim);

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRect (16.0f, 64.0f, bounds.getWidth() - 32.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRect (16.0f, 65.0f, bounds.getWidth() - 32.0f, 1.0f);

        draw::recess (g, knobPanel.toFloat(), 10.0f);

        for (auto [x, y] : { std::pair { 12.0f, 12.0f }, { bounds.getRight() - 12.0f, 12.0f },
                             { 12.0f, bounds.getBottom() - 12.0f }, { bounds.getRight() - 12.0f, bounds.getBottom() - 12.0f } })
            draw::screw (g, { x, y }, x * 0.37f + y * 0.11f);

        g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.25f));
        draw::engraved (g, "FENRIKOS", bounds.withTop (bounds.getBottom() - 24.0f).withTrimmedRight (30.0f),
                        juce::Justification::centredRight, colours::engraveDim);
        draw::engraved (g, "FENIX COLOUR SERIES", bounds.withTop (bounds.getBottom() - 24.0f).withTrimmedLeft (30.0f),
                        juce::Justification::centredLeft, colours::engraveDim.withAlpha (0.6f));
    }

    void resized() override
    {
        power.setBounds (getWidth() - 62, 14, 32, 32);
        auto bar = juce::Rectangle<int> (getWidth() - 470, 18, 390, 26);
        save.setBounds (bar.removeFromRight (58));
        bar.removeFromRight (8);
        next.setBounds (bar.removeFromRight (28));
        previous.setBounds (bar.removeFromLeft (28));
        presets.setBounds (bar.reduced (4, 0));

        const int meterWidth = 176, top = 80, mainHeight = 200;
        leftMeter->setBounds (20, top, meterWidth, mainHeight);
        rightMeter->setBounds (getWidth() - 20 - meterWidth, top, meterWidth, mainHeight);
        display->setBounds (20 + meterWidth + 14, top, getWidth() - 2 * (20 + meterWidth + 14), mainHeight);

        knobPanel = { 20, top + mainHeight + 16, getWidth() - 40, getHeight() - (top + mainHeight + 16) - 30 };

        // Main knobs get wide cells, mix/output trims narrower ones; the group is centred.
        int total = 0;
        for (bool small : compact)
            total += small ? 112 : 150;
        auto cells = knobPanel.reduced (0, 8).withSizeKeepingCentre (juce::jmin (total, knobPanel.getWidth() - 20), knobPanel.getHeight() - 16);
        for (size_t i = 0; i < knobs.size(); ++i)
        {
            auto cell = cells.removeFromLeft (compact[i] ? 112 : 150);
            knobs[i]->setBounds (compact[i] ? cell.withTrimmedTop (22) : cell);
        }
    }

private:
    void timerCallback() override
    {
        constexpr float dt = 1.0f / 60.0f;
        leftMeter->tick (dt);
        rightMeter->tick (dt);
        display->tick();
        leftMeter->repaint();
        rightMeter->repaint();
        display->repaint();

        if (proc.presetName() != presets.getText())
            refreshPresetList();
    }

    // Factory presets use ids 1..n, user presets 1000 and up.
    void refreshPresetList()
    {
        presets.clear (juce::dontSendNotification);
        for (size_t i = 0; i < proc.factoryPresets.size(); ++i)
            presets.addItem (proc.factoryPresets[i].name, (int) i + 1);

        userFiles = proc.userPresets();
        if (! userFiles.isEmpty())
        {
            presets.addSectionHeading ("User");
            for (int i = 0; i < userFiles.size(); ++i)
                presets.addItem (userFiles[i].getFileNameWithoutExtension(), 1000 + i);
        }

        presets.setText (proc.presetName(), juce::dontSendNotification);
    }

    void presetChosen (int id)
    {
        if (id >= 1000)
            proc.loadUserPreset (userFiles[id - 1000]);
        else if (id >= 1)
            proc.loadFactoryPreset (id - 1);
        presets.setText (proc.presetName(), juce::dontSendNotification);
    }

    void stepPreset (int delta)
    {
        juce::Array<int> ids;
        for (int i = 0; i < presets.getNumItems(); ++i)
            ids.add (presets.getItemId (i));
        if (ids.isEmpty())
            return;

        const auto current = ids.indexOf (presets.getSelectedId());
        const auto target = current < 0 ? (delta > 0 ? 0 : ids.size() - 1) : (current + delta + ids.size()) % ids.size();
        presetChosen (ids[target]);
    }

    void askForPresetName()
    {
        dialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name for this preset:", juce::MessageBoxIconType::NoIcon, this);
        dialog->setLookAndFeel (&lnf);
        dialog->addTextEditor ("name", proc.presetName());
        dialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        dialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result) {
            const auto name = dialog->getTextEditorContents ("name").trim();
            if (result == 1 && name.isNotEmpty())
                proc.saveUserPreset (name);
            refreshPresetList();
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Editor> (this)] {
                if (safe != nullptr)
                    safe->dialog.reset();
            });
        }));
    }

    Processor& proc;
    LookAndFeel lnf;
    juce::Image texture;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<bool> compact;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    PowerButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;

    std::unique_ptr<Display> display;
    std::unique_ptr<VuMeter> leftMeter, rightMeter;
    juce::Rectangle<int> knobPanel;

    juce::ComboBox presets;
    juce::TextButton previous { "<" }, next { ">" }, save { "SAVE" };
    juce::Array<juce::File> userFiles;
    std::unique_ptr<juce::AlertWindow> dialog;
};

inline juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }
inline std::unique_ptr<Display> Processor::createDisplay() { return std::make_unique<ScopeDisplay> (*this); }

} // namespace fenix
