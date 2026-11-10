#pragma once

#include "Look.h"
#include "Processor.h"
#include "RackUI.h"

namespace fenix
{

// Preset panel: the name in a recessed OLED window (click it for the menu), with previous / next /
// store caps underneath. The menu is built from scratch each
// time it opens, so it is always current and nothing rebuilds it while it is on screen.
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (Processor& p) : proc (p), nameButton (p.accent())
    {
        previous.onClick = [this] { step (-1); };
        next.onClick = [this] { step (1); };
        save.onClick = [this] { askForName(); };
        nameButton.onClick = [this] { showMenu(); };
        for (auto* c : std::initializer_list<juce::Component*> { &previous, &next, &save, &nameButton })
            addAndMakeVisible (c);
        refresh();
    }

    // Cheap enough to call every frame: only repaints when the name actually changes.
    void refresh()
    {
        const auto name = proc.presetName();
        if (name != nameButton.getButtonText())
            nameButton.setButtonText (name);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        nameButton.setBounds (b.removeFromTop (34));
        b.removeFromTop (10);
        auto row = b.removeFromTop (22);
        previous.setBounds (row.removeFromLeft (30));
        row.removeFromLeft (6);
        next.setBounds (row.removeFromLeft (30));
        row.removeFromLeft (6);
        save.setBounds (row.removeFromLeft (58));
    }

private:
    class NameButton : public juce::Button
    {
    public:
        explicit NameButton (juce::Colour a) : Button ({}), accent (a) {}

        void paintButton (juce::Graphics& g, bool over, bool) override
        {
            const auto area = getLocalBounds().toFloat();
            rackui::oled (g, area);
            rackui::oledText (g, getButtonText().toUpperCase(), area.reduced (10.0f, 0.0f).withTrimmedRight (14.0f), accent, 12.5f);
            juce::Path caret;
            const auto cx = area.getRight() - 12.0f, cy = area.getCentreY();
            caret.startNewSubPath (cx - 3.5f, cy - 1.5f);
            caret.lineTo (cx, cy + 2.0f);
            caret.lineTo (cx + 3.5f, cy - 1.5f);
            g.setColour (accent.withAlpha (over ? 0.9f : 0.45f));
            g.strokePath (caret, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

    private:
        juce::Colour accent;
    };

    struct Entry
    {
        juce::String name;
        int factoryIndex = -1;
        juce::File file;
    };

    std::vector<Entry> entries() const
    {
        std::vector<Entry> list;
        for (size_t i = 0; i < proc.factoryPresets.size(); ++i)
            list.push_back ({ proc.factoryPresets[i].name, (int) i, {} });
        for (const auto& file : proc.userPresets())
            list.push_back ({ file.getFileNameWithoutExtension(), -1, file });
        return list;
    }

    void load (const Entry& entry)
    {
        if (entry.factoryIndex >= 0)
            proc.loadFactoryPreset (entry.factoryIndex);
        else
            proc.loadUserPreset (entry.file);
        refresh();
    }

    void step (int delta)
    {
        const auto list = entries();
        if (list.empty())
            return;

        const auto current = proc.presetName();
        int index = -1;
        for (size_t i = 0; i < list.size(); ++i)
            if (list[i].name == current)
                index = (int) i;

        const auto count = (int) list.size();
        const auto target = index < 0 ? (delta > 0 ? 0 : count - 1) : (index + delta + count) % count;
        load (list[(size_t) target]);
    }

    void showMenu()
    {
        const auto current = proc.presetName();
        juce::PopupMenu menu;
        bool userSection = false;
        const juce::File* currentUserFile = nullptr;
        const auto list = entries();

        menu.addSectionHeader ("Factory");
        for (const auto& entry : list)
        {
            if (entry.factoryIndex < 0 && ! userSection)
            {
                menu.addSectionHeader ("User");
                userSection = true;
            }
            if (entry.factoryIndex < 0 && entry.name == current)
                currentUserFile = &entry.file;
            menu.addItem (entry.name, true, entry.name == current, [this, entry] { load (entry); });
        }

        menu.addSeparator();
        menu.addItem ("Save preset...", [this] { askForName(); });
        if (currentUserFile != nullptr)
            menu.addItem ("Delete \"" + current + "\"", [file = *currentUserFile] { file.deleteFile(); });
        menu.addItem ("Open preset folder", [dir = proc.userPresetDirectory()] {
            dir.createDirectory();
            dir.startAsProcess();
        });

        menu.showMenuAsync (juce::PopupMenu::Options()
                                .withTargetComponent (&nameButton)
                                .withMinimumWidth (nameButton.getWidth())
                                .withMaximumNumColumns (1));
    }

    void askForName()
    {
        dialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name for this preset:", juce::MessageBoxIconType::NoIcon, this);
        dialog->addTextEditor ("name", proc.presetName());
        dialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        dialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result) {
            const auto name = dialog->getTextEditorContents ("name").trim();
            if (result == 1 && name.isNotEmpty())
                proc.saveUserPreset (name);
            refresh();
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<PresetBar> (this)] {
                if (safe != nullptr)
                    safe->dialog.reset();
            });
        }));
    }

    Processor& proc;
    NameButton nameButton;
    rackui::CapButton previous { "<" }, next { ">" }, save { "STORE" };
    std::unique_ptr<juce::AlertWindow> dialog;
};

} // namespace fenix
