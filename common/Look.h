#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "RackUI.h"

namespace fenix
{

namespace colours
{
    // Rack faceplate: black anodised aluminium with cream silkscreen legends.
    const juce::Colour plateTop { 0xff2b2d31 };
    const juce::Colour plateBottom { 0xff1d1f22 };
    const juce::Colour card { 0xff222428 };
    const juce::Colour cardEdge { 0x14ffffff };
    const juce::Colour glass { 0xff08090b };
    const juce::Colour engrave { 0xffeae4d3 };     // silkscreen
    const juce::Colour engraveDim { 0xff9c978a };  // dim silkscreen
    const juce::Colour tickOff { 0xff3a3c41 };
    const juce::Colour lcd { 0xff0b0c0a };
    const juce::Colour button { 0xff2e3035 };
    const juce::Colour metalLight { 0xffd9dadc };
    const juce::Colour metalDark { 0xff8d9095 };
}

namespace draw
{
    inline juce::Font font (float height, bool bold = false)
    {
        return juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain);
    }

    inline juce::Font mono (float height)
    {
        return juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain);
    }

    // Silkscreen legend: printed ink sits on the metal, so only a hairline of shadow.
    inline void engraved (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                          juce::Justification just, juce::Colour colour = colours::engrave)
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawText (text, area.translated (0.0f, 1.0f), just, false);
        g.setColour (colour);
        g.drawText (text, area, just, false);
    }

    // Stroke with a soft bloom, for lit traces on the displays.
    inline void glow (juce::Graphics& g, const juce::Path& path, juce::Colour colour, float width)
    {
        g.setColour (colour.withAlpha (0.16f));
        g.strokePath (path, juce::PathStrokeType (width * 3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Panel LED in a chrome bezel: a domed lens that glows when lit and reads as dark glass when off.
    inline void led (juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour colour, bool on)
    {
        const auto bezel = juce::Rectangle<float> (radius * 2.0f + 3.0f, radius * 2.0f + 3.0f).withCentre (centre);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff15161a), bezel.getCentreX(), bezel.getY(),
                                                 juce::Colour (0xff6a6d73), bezel.getCentreX(), bezel.getBottom(), false));
        g.fillEllipse (bezel);

        if (on)
        {
            g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.5f), centre, colour.withAlpha (0.0f),
                                                     centre.translated (radius * 3.8f, 0.0f), true));
            g.fillEllipse (juce::Rectangle<float> (radius * 7.6f, radius * 7.6f).withCentre (centre));
        }

        const auto lens = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
        const auto core = on ? colour.brighter (0.5f) : colour.darker (2.2f);
        g.setGradientFill (juce::ColourGradient (on ? juce::Colours::white.interpolatedWith (colour, 0.35f) : colour.darker (1.2f),
                                                 centre.x - radius * 0.3f, centre.y - radius * 0.35f,
                                                 on ? colour : core.darker (0.6f), centre.x + radius, centre.y + radius, true));
        g.fillEllipse (lens);
        g.setColour (juce::Colours::white.withAlpha (on ? 0.7f : 0.22f));
        g.fillEllipse (juce::Rectangle<float> (radius * 0.7f, radius * 0.45f).withCentre (centre.translated (-radius * 0.25f, -radius * 0.45f)));
    }

    // Brushed aluminium: a vertical light falloff plus fine horizontal grain. Deterministic, so the
    // texture never shimmers between redraws. Only ever drawn into cached background images.
    inline void brushedMetal (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour top, juce::Colour bottom, int seed = 7)
    {
        g.setGradientFill (juce::ColourGradient (top, area.getX(), area.getY(), bottom, area.getX(), area.getBottom(), false));
        g.fillRect (area);

        juce::Random random (seed);
        const auto lines = (int) (area.getHeight() * 2.2f);
        for (int i = 0; i < lines; ++i)
        {
            const auto y = area.getY() + random.nextFloat() * area.getHeight();
            const auto x = area.getX() + random.nextFloat() * area.getWidth() * 0.9f - area.getWidth() * 0.2f;
            const auto len = area.getWidth() * (0.2f + random.nextFloat() * 0.8f);
            const bool light = random.nextBool();
            g.setColour ((light ? juce::Colours::white : juce::Colours::black).withAlpha (0.012f + random.nextFloat() * (light ? 0.025f : 0.05f)));
            g.fillRect (juce::Rectangle<float> (x, y, len, random.nextFloat() < 0.15f ? 1.0f : 0.5f).getIntersection (area));
        }

        // Broad sheen across the plate, as if lit from above-left.
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.045f), area.getX(), area.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), area.getCentreX(), area.getBottom(), false));
        g.fillRect (area);
    }

    // Recessed cut-out: dark top edge in shadow, a lit lip along the bottom.
    inline void recess (juce::Graphics& g, juce::Rectangle<float> area, float corner, juce::Colour fill = colours::glass)
    {
        g.setColour (juce::Colours::white.withAlpha (0.09f));
        g.fillRoundedRectangle (area.translated (0.0f, 1.0f).expanded (1.0f), corner + 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.fillRoundedRectangle (area.expanded (1.0f), corner + 1.0f);
        g.setColour (fill);
        g.fillRoundedRectangle (area, corner);

        // Inner shadow falling from the top lip.
        const juce::Graphics::ScopedSaveState save (g);
        juce::Path shape;
        shape.addRoundedRectangle (area, corner);
        g.reduceClipRegion (shape);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), 0.0f, area.getY(),
                                                 juce::Colours::black.withAlpha (0.0f), 0.0f, area.getY() + 10.0f, false));
        g.fillRect (area.withHeight (10.0f));
    }

    // Raised black bezel around a meter or screen: bevelled ring with a lit top edge.
    inline void bezel (juce::Graphics& g, juce::Rectangle<float> area, float corner)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (area.translated (0.0f, 3.0f).expanded (1.5f), corner + 1.5f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3d4045), 0.0f, area.getY(),
                                                 juce::Colour (0xff111214), 0.0f, area.getBottom(), false));
        g.fillRoundedRectangle (area, corner);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1e2023), 0.0f, area.getY(),
                                                 juce::Colour (0xff2a2c30), 0.0f, area.getBottom(), false));
        g.fillRoundedRectangle (area.reduced (2.0f), juce::jmax (0.0f, corner - 2.0f));
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.drawLine (area.getX() + corner, area.getY() + 0.5f, area.getRight() - corner, area.getY() + 0.5f, 1.0f);
    }

    // Phillips-head screw, countersunk into the plate.
    inline void screw (juce::Graphics& g, juce::Point<float> centre, float radius, float angle = 0.6f)
    {
        const auto head = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (head.expanded (1.5f).translated (0.0f, 1.0f));
        g.setGradientFill (juce::ColourGradient (colours::metalLight, head.getX(), head.getY(),
                                                 juce::Colour (0xff55585e), head.getRight(), head.getBottom(), false));
        g.fillEllipse (head);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff7d8086), head.getX(), head.getY(),
                                                 juce::Colour (0xffc4c6ca), head.getRight(), head.getBottom(), false));
        g.fillEllipse (head.reduced (radius * 0.18f));

        juce::Path cross;
        const auto arm = radius * 0.55f;
        for (int k = 0; k < 2; ++k)
        {
            const auto a = angle + (float) k * juce::MathConstants<float>::halfPi;
            const auto d = juce::Point<float> (std::cos (a), std::sin (a)) * arm;
            cross.startNewSubPath (centre - d);
            cross.lineTo (centre + d);
        }
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.strokePath (cross, juce::PathStrokeType (radius * 0.28f), juce::AffineTransform::translation (0.4f, 0.6f));
        g.setColour (juce::Colour (0xff2a2c30));
        g.strokePath (cross, juce::PathStrokeType (radius * 0.28f));
    }

    // Rack ears either side of `bounds`, each folded back from the plate with two slotted screws.
    inline void rackEars (juce::Graphics& g, juce::Rectangle<float> bounds, float ear)
    {
        for (const bool left : { true, false })
        {
            const auto earArea = left ? bounds.withWidth (ear) : bounds.withLeft (bounds.getRight() - ear);
            g.setColour (juce::Colours::black.withAlpha (0.18f));
            g.fillRect (earArea);
            const auto foldX = left ? earArea.getRight() : earArea.getX();
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.fillRect (foldX - (left ? 1.0f : 0.0f), bounds.getY(), 1.0f, bounds.getHeight());
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (foldX + (left ? 0.0f : -1.0f), bounds.getY(), 1.0f, bounds.getHeight());

            for (auto y : { bounds.getY() + 46.0f, bounds.getBottom() - 46.0f })
            {
                const auto slot = juce::Rectangle<float> (13.0f, 24.0f).withCentre ({ earArea.getCentreX(), y });
                recess (g, slot, 6.5f, juce::Colour (0xff050506));
                screw (g, { slot.getCentreX(), slot.getCentreY() + 3.0f }, 6.0f, y < bounds.getCentreY() ? 0.4f : 1.1f);
            }
        }
    }

    // Machined seam across the plate: a dark groove with a lit lower lip.
    inline void seam (juce::Graphics& g, float x1, float x2, float y)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRect (x1, y, x2 - x1, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRect (x1, y + 1.0f, x2 - x1, 1.0f);
    }

    // Silkscreened section outline, with the title printed in a gap in the top rule.
    inline void silkBox (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title, juce::Colour titleColour, float corner = 6.0f)
    {
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold).withKerningFactor (0.18f));
        const auto textWidth = title.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), title) + 14.0f;
        {
            const juce::Graphics::ScopedSaveState save (g);
            if (textWidth > 0.0f)
                g.excludeClipRegion (juce::Rectangle<float> (area.getX() + 14.0f, area.getY() - 8.0f, textWidth, 16.0f).toNearestInt());
            g.setColour (colours::engrave.withAlpha (0.38f));
            g.drawRoundedRectangle (area, corner, 1.0f);
        }
        if (textWidth > 0.0f)
            engraved (g, title, { area.getX() + 21.0f, area.getY() - 8.0f, textWidth, 16.0f }, juce::Justification::centredLeft, titleColour);
    }

    // Raised panel the old Fenix code calls a card; on the rack plate it is a silkscreened outline.
    inline void card (juce::Graphics& g, juce::Rectangle<float> area, float corner = 12.0f)
    {
        juce::ignoreUnused (corner);
        silkBox (g, area, {}, colours::engrave);
    }

    // Fenix mark: Fehu, the F rune. An upright stave with two branches rising to the right,
    // drawn as round-capped strokes inside a square box.
    inline juce::Path fehu (juce::Rectangle<float> box)
    {
        const auto size = juce::jmin (box.getWidth(), box.getHeight());
        const auto at = [&] (float x, float y) { return juce::Point<float> (box.getX() + x * size, box.getY() + y * size); };

        juce::Path lines;
        for (auto [from, to] : { std::pair { at (0.36f, 0.10f), at (0.36f, 0.90f) },
                                 { at (0.36f, 0.40f), at (0.74f, 0.12f) },
                                 { at (0.36f, 0.64f), at (0.74f, 0.36f) } })
        {
            lines.startNewSubPath (from);
            lines.lineTo (to);
        }

        juce::Path rune;
        juce::PathStrokeType (0.13f * size, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded)
            .createStrokedPath (rune, lines);
        return rune;
    }

    // The mark's fixed brand gradient, amber to red, the same in every plugin.
    inline void fillFehu (juce::Graphics& g, juce::Rectangle<float> box)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffc45a), box.getCentreX(), box.getY(),
                                                 juce::Colour (0xffe84830), box.getCentreX(), box.getBottom(), false));
        g.fillPath (fehu (box));
    }
}

//==============================================================================
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Colour accent { juce::Colours::orange };

    // Plain arrow wherever a component asks for the default cursor (see rackui::arrowCursor).
    juce::MouseCursor getMouseCursorFor (juce::Component& c) override
    {
        const auto cursor = juce::LookAndFeel_V4::getMouseCursorFor (c);
        return cursor == juce::MouseCursor::NormalCursor || cursor == juce::MouseCursor::ParentCursor ? arrow : cursor;
    }

private:
    juce::MouseCursor arrow { rackui::arrowCursor() };

public:
    LookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1b1e23));
        setColour (juce::PopupMenu::textColourId, colours::engrave);
        setColour (juce::PopupMenu::headerTextColourId, colours::engraveDim);
        setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff1d2025));
        setColour (juce::AlertWindow::textColourId, colours::engrave);
        setColour (juce::TextEditor::backgroundColourId, colours::lcd);
        setColour (juce::TextEditor::textColourId, colours::engrave);
        setColour (juce::TextEditor::outlineColourId, colours::tickOff);
        setColour (juce::TextButton::buttonColourId, colours::button);
        setColour (juce::TextButton::textColourOffId, colours::engrave);
    }

    void setAccent (juce::Colour c)
    {
        accent = c;
        setColour (juce::PopupMenu::highlightedBackgroundColourId, c.withAlpha (0.3f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextEditor::focusedOutlineColourId, c);
        setColour (juce::TextEditor::highlightColourId, c.withAlpha (0.4f));
    }

    juce::Font getPopupMenuFont() override { return draw::font (14.0f); }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
        g.setColour (colours::cardEdge);
        g.drawRect (0, 0, width, height);
    }

    // Small machined push button: raised dark cap with a lit top edge; it sinks when pressed.
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto area = b.getLocalBounds().toFloat().reduced (1.0f, 1.5f);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillRoundedRectangle (area.translated (0.0f, 1.5f), 3.5f);
        if (down)
            area.translate (0.0f, 1.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (over ? 0xff4a4d53 : 0xff404349), 0.0f, area.getY(),
                                                 juce::Colour (0xff222428), 0.0f, area.getBottom(), false));
        g.fillRoundedRectangle (area, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (down ? 0.05f : 0.16f));
        g.drawLine (area.getX() + 3.0f, area.getY() + 0.5f, area.getRight() - 3.0f, area.getY() + 0.5f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override
    {
        return juce::FontOptions (11.5f, juce::Font::bold).withKerningFactor (0.08f);
    }
};

} // namespace fenix
