#pragma once

// fenrikos rack UI kit, shared by every fenrikos plugin (keep the copies identical).
// A warm graphite faceplate with cream silkscreen, machined knobs over scales printed in real
// units, fixed-colour LEDs, LED selectors, push caps, recessed OLED windows and a calibrated VU.
// Needs the RackFonts binary-data target (Barlow, Barlow Condensed, JetBrains Mono, all SIL OFL).

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "RackFonts.h"

namespace rackui
{

//==============================================================================
namespace colour
{
    const juce::Colour plateTop { 0xff2a2b28 }, plateBottom { 0xff1f201e };
    const juce::Colour ink { 0xffece5d3 };          // silkscreen
    const juce::Colour inkSoft { 0xffcfc8b6 };      // scale numbers
    const juce::Colour inkDim { 0xff9a9383 };       // sub-legends
    const juce::Colour inkFaint { 0xff6f6a5f };     // small print
    const juce::Colour oled { 0xff060706 };
    const juce::Colour red { 0xffff4a36 };
}

static constexpr int earWidth = 34;

//==============================================================================
// Fonts
enum class Face { legend, legendBold, text, textMedium, mono, monoBold };

inline juce::Font font (Face face, float height, float tracking = 0.0f)
{
    static auto make = [] (const void* data, size_t size) { return juce::Typeface::createSystemTypefaceFor (data, size); };
    static auto legend = make (RackFonts::BarlowCondensedSemiBold_ttf, RackFonts::BarlowCondensedSemiBold_ttfSize);
    static auto legendBold = make (RackFonts::BarlowCondensedBold_ttf, RackFonts::BarlowCondensedBold_ttfSize);
    static auto text = make (RackFonts::BarlowRegular_ttf, RackFonts::BarlowRegular_ttfSize);
    static auto textMedium = make (RackFonts::BarlowMedium_ttf, RackFonts::BarlowMedium_ttfSize);
    static auto mono = make (RackFonts::JetBrainsMonoMedium_ttf, RackFonts::JetBrainsMonoMedium_ttfSize);
    static auto monoBold = make (RackFonts::JetBrainsMonoBold_ttf, RackFonts::JetBrainsMonoBold_ttfSize);
    const juce::Typeface::Ptr faces[] { legend, legendBold, text, textMedium, mono, monoBold };
    return juce::Font (juce::FontOptions (faces[(int) face]).withHeight (height).withKerningFactor (tracking));
}

inline void print (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, Face face, float height, float tracking,
                   juce::Colour c = colour::ink, juce::Justification j = juce::Justification::centred)
{
    g.setFont (font (face, height, tracking));
    g.setColour (c);
    g.drawText (s, r, j, false);
}

inline juce::Point<float> polar (juce::Point<float> c, float r, float a) { return c.translated (r * std::sin (a), -r * std::cos (a)); }

//==============================================================================
// Plate, ears, rules

// Warm graphite plate with a fine horizontal grain and lit top edge. Draw into cached images only.
inline void plate (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setGradientFill (juce::ColourGradient (colour::plateTop, 0.0f, r.getY(), colour::plateBottom, 0.0f, r.getBottom(), false));
    g.fillRect (r);
    g.setColour (juce::Colours::white.withAlpha (0.012f));
    for (float y = r.getY(); y < r.getBottom(); y += 3.0f)
        g.fillRect (r.getX(), y, r.getWidth(), 1.0f);
    juce::Random random (5);
    for (int i = 0, n = (int) (r.getWidth() * r.getHeight() / 9.0f); i < n; ++i)
    {
        g.setColour ((random.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.015f + random.nextFloat() * 0.02f));
        g.fillRect (r.getX() + random.nextFloat() * r.getWidth(), r.getY() + random.nextFloat() * r.getHeight(), 1.0f, 1.0f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.09f));
    g.fillRect (r.withHeight (1.0f));
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRect (r.withTop (r.getBottom() - 1.0f));
}

inline void screw (juce::Graphics& g, juce::Point<float> c, float radius, float angle)
{
    const auto head = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.fillEllipse (head.translated (0.0f, 1.0f).expanded (0.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe4e5e7), head.getX() + radius * 0.6f, head.getY() + radius * 0.5f,
                                             juce::Colour (0xff5d6066), head.getRight(), head.getBottom(), true));
    g.fillEllipse (head);
    g.setColour (juce::Colour (0xff2a2c2f));
    for (int k = 0; k < 2; ++k)
    {
        const auto a = angle + (float) k * juce::MathConstants<float>::halfPi;
        const auto d = juce::Point<float> (std::cos (a), std::sin (a)) * (radius * 0.6f);
        g.drawLine ({ c - d, c + d }, radius * 0.33f);
    }
}

// Rack ears either side of `bounds`: a fold line, two slotted screws each.
inline void ears (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto ear = (float) earWidth;
    for (const bool left : { true, false })
    {
        const auto e = left ? bounds.withWidth (ear) : bounds.withLeft (bounds.getRight() - ear);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff252623), 0.0f, e.getY(), juce::Colour (0xff1a1b19), 0.0f, e.getBottom(), false));
        g.fillRect (e);
        const auto fold = left ? e.getRight() - 1.0f : e.getX();
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.fillRect (fold, e.getY(), 1.0f, e.getHeight());
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRect (left ? fold - 1.0f : fold + 1.0f, e.getY(), 1.0f, e.getHeight());
        int i = 0;
        for (auto y : { e.getY() + 34.0f, e.getBottom() - 58.0f })
        {
            const auto slot = juce::Rectangle<float> (13.0f, 24.0f).withPosition (e.getCentreX() - 6.5f, y);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRoundedRectangle (slot.translated (0.0f, 1.0f), 6.5f);
            g.setColour (juce::Colour (0xff050505));
            g.fillRoundedRectangle (slot, 6.5f);
            screw (g, { slot.getCentreX(), slot.getY() + 15.0f }, 6.0f, i++ == 0 ? 0.44f : 1.2f);
        }
    }
}

// Engraved vertical divider and horizontal rule between sections.
inline void divider (juce::Graphics& g, float x, float y, float h)
{
    g.setColour (colour::ink.withAlpha (0.13f));
    g.fillRect (x, y, 1.0f, h);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRect (x + 1.0f, y, 1.0f, h);
}

inline void rule (juce::Graphics& g, float x1, float x2, float y)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (x1, y, x2 - x1, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRect (x1, y + 1.0f, x2 - x1, 1.0f);
}

// Raised dark bezel (meters, screens): returns the window inside it.
inline juce::Rectangle<float> bezel (juce::Graphics& g, juce::Rectangle<float> r, float inset = 7.0f)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0.0f, 4.0f).expanded (1.0f), 9.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b3d3f), 0.0f, r.getY(), juce::Colour (0xff121314), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawLine (r.getX() + 8.0f, r.getY() + 0.5f, r.getRight() - 8.0f, r.getY() + 0.5f, 1.0f);
    return r.reduced (inset);
}

// Inner shadow and a faint diagonal reflection over a window.
inline void glass (juce::Graphics& g, juce::Rectangle<float> w, float strength = 1.0f)
{
    const juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (w.toNearestInt());
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), 0.0f, w.getY(), juce::Colours::black.withAlpha (0.0f), 0.0f, w.getY() + 10.0f, false));
    g.fillRect (w.withHeight (10.0f));
    juce::Path sheen;
    sheen.startNewSubPath (w.getX(), w.getY());
    sheen.lineTo (w.getX() + w.getWidth() * 0.42f, w.getY());
    sheen.lineTo (w.getX() + w.getWidth() * 0.18f, w.getBottom());
    sheen.lineTo (w.getX(), w.getBottom());
    sheen.closeSubPath();
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f * strength), w.getX(), w.getY(),
                                             juce::Colours::white.withAlpha (0.0f), w.getX() + w.getWidth() * 0.3f, w.getCentreY(), false));
    g.fillPath (sheen);
}

// Recessed OLED window.
inline void oled (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.0f), 4.0f);
    g.setColour (colour::oled);
    g.fillRoundedRectangle (r, 4.0f);
    const juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (r.toNearestInt());
    g.setGradientFill (juce::ColourGradient (juce::Colours::black, 0.0f, r.getY(), juce::Colours::black.withAlpha (0.0f), 0.0f, r.getY() + 5.0f, false));
    g.fillRect (r.withHeight (5.0f));
}

// Lit OLED text: a soft halo under crisp glyphs.
inline void oledText (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, juce::Colour c, float height,
                      juce::Justification j = juce::Justification::centredLeft, bool bold = false)
{
    g.setFont (font (bold ? Face::monoBold : Face::mono, height, 0.06f));
    g.setColour (c.withAlpha (0.28f));
    for (auto d : { juce::Point<float> (-0.6f, 0.0f), { 0.6f, 0.0f }, { 0.0f, -0.6f }, { 0.0f, 0.6f } })
        g.drawText (s, r.translated (d.x, d.y), j, true);
    g.setColour (c.interpolatedWith (juce::Colours::white, 0.45f));
    g.drawText (s, r, j, true);
}

//==============================================================================
// LEDs and caps

inline void led (juce::Graphics& g, juce::Point<float> c, float radius, juce::Colour col, bool on)
{
    const auto lens = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c);
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (col.withAlpha (0.45f), c, col.withAlpha (0.0f), c.translated (radius * 2.6f, 0.0f), true));
        g.fillEllipse (lens.expanded (radius * 1.6f));
    }
    g.setColour (juce::Colours::black);
    g.fillEllipse (lens.expanded (1.5f));
    if (on)
        g.setGradientFill (juce::ColourGradient (col.interpolatedWith (juce::Colours::white, 0.75f), c.x - radius * 0.3f, c.y - radius * 0.3f,
                                                 col.darker (0.4f), c.x + radius, c.y + radius, true));
    else
        g.setGradientFill (juce::ColourGradient (col.withMultipliedSaturation (0.5f).withMultipliedBrightness (0.3f), c.x - radius * 0.3f, c.y - radius * 0.3f,
                                                 juce::Colour (0xff1c1b18), c.x + radius, c.y + radius, true));
    g.fillEllipse (lens);
}

inline void rectLed (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col, float level)
{
    if (level > 0.02f)
    {
        g.setColour (col.withAlpha (0.3f * level));
        g.fillRoundedRectangle (r.expanded (2.5f), 3.0f);
    }
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (r.expanded (1.0f), 1.5f);
    const auto off = col.withMultipliedSaturation (0.6f).withMultipliedBrightness (0.22f);
    const auto body = off.interpolatedWith (col, level);
    g.setGradientFill (juce::ColourGradient (body.interpolatedWith (juce::Colours::white, 0.45f * level + 0.05f), 0.0f, r.getY(),
                                             body, 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 1.0f);
}

// Machined push cap. `latched` draws it held down.
inline void cap (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool down, bool latched = false)
{
    const bool in = down || latched;
    if (in)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1c1a), 0.0f, r.getY(), juce::Colour (0xff262724), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawLine (r.getX() + 3.0f, r.getBottom() + 0.5f, r.getRight() - 3.0f, r.getBottom() + 0.5f, 1.0f);
        return;
    }
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 3.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (over ? 0xff464744 : 0xff3a3b38), 0.0f, r.getY(), juce::Colour (0xff1b1c1a), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.drawLine (r.getX() + 3.0f, r.getY() + 0.5f, r.getRight() - 3.0f, r.getY() + 0.5f, 1.0f);
}

// A push cap with its legend printed on it.
class CapButton : public juce::Button
{
public:
    explicit CapButton (const juce::String& legend) : Button (legend) {}

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f, 1.0f).withTrimmedBottom (1.5f);
        cap (g, r, over, down, latched);
        if (down || latched)
            r.translate (0.0f, 0.5f);
        const auto legend = getButtonText();
        if (legend == "<" || legend == ">")
        {
            juce::Path tri;
            const auto c = r.getCentre();
            const float s = legend == "<" ? -1.0f : 1.0f;
            tri.addTriangle (c.x - 3.0f * s, c.y - 4.0f, c.x - 3.0f * s, c.y + 4.0f, c.x + 3.5f * s, c.y);
            g.setColour (colour::ink);
            g.fillPath (tri);
            return;
        }
        print (g, legend, r, Face::legend, 11.0f, 0.16f, colour::ink);
    }

    bool latched = false;
};

//==============================================================================
// Knob: machined body over a printed scale. Metal knobs carry a spun insert with a collar in the
// product's colour; black knobs a long painted pointer. Name and unit are printed under it, and the
// unit line shows the value while the knob is touched.
struct Mark
{
    double value;
    juce::String text;
};

class Knob : public juce::Slider
{
public:
    enum class Variant { metal, black };
    static constexpr float sweep = juce::MathConstants<float>::pi * 0.75f;

    Knob (float bodySize, Variant v, juce::Colour accentColour, juce::String nameText, juce::String unitText, std::vector<Mark> scaleMarks)
        : size (bodySize), variant (v), accent (accentColour), name (std::move (nameText)), unit (std::move (unitText)), marks (std::move (scaleMarks))
    {
        setSliderStyle (RotaryHorizontalVerticalDrag);
        setTextBoxStyle (NoTextBox, false, 0, 0);
        setRotaryParameters (juce::MathConstants<float>::twoPi - sweep, juce::MathConstants<float>::twoPi + sweep, true);
        setMouseDragSensitivity (250);
        setPopupMenuEnabled (false);
    }

    Knob() : Knob (46.0f, Variant::black, colour::ink, {}, {}, {}) {}

    void configure (float bodySize, Variant v, juce::Colour accentColour, juce::String nameText, juce::String unitText, std::vector<Mark> scaleMarks)
    {
        size = bodySize;
        variant = v;
        accent = accentColour;
        name = std::move (nameText);
        unit = std::move (unitText);
        marks = std::move (scaleMarks);
        body = {};
        repaint();
    }

    // The printed name, and its colour (e.g. to show a MIDI mapping).
    void setLegend (const juce::String& text, juce::Colour c = colour::ink)
    {
        if (text != name || c != legendColour)
        {
            name = text;
            legendColour = c;
            repaint();
        }
    }

    // Component size this knob wants (scale labels need room around the body).
    juce::Point<int> preferredSize() const { return { juce::roundToInt (size + 70.0f), juce::roundToInt (size + (name.isEmpty() ? 52.0f : 74.0f)) }; }

    void setAccent (juce::Colour c) { accent = c; body = {}; repaint(); }
    void setMarks (std::vector<Mark> m) { marks = std::move (m); repaint(); }

    void paint (juce::Graphics& g) override
    {
        const float r = size * 0.5f;
        const juce::Point<float> c ((float) getWidth() * 0.5f, r + 26.0f);
        const auto angleAt = [] (double t) { return (float) (-sweep + 2.0 * sweep * t); };
        const bool live = isMouseOverOrDragging() && isEnabled();

        // Printed scale: a major tick and legend per mark, a minor tick between marks.
        const auto proportions = markProportions();
        for (size_t i = 0; i < proportions.size(); ++i)
        {
            const auto a = angleAt (proportions[i]);
            g.setColour (colour::ink.withAlpha (0.9f));
            g.drawLine ({ polar (c, r + 5.0f, a), polar (c, r + 11.0f, a) }, 1.4f);
            if (i + 1 < proportions.size())
            {
                const auto m = angleAt (0.5 * (proportions[i] + proportions[i + 1]));
                g.setColour (colour::ink.withAlpha (0.4f));
                g.drawLine ({ polar (c, r + 6.0f, m), polar (c, r + 10.0f, m) }, 1.0f);
            }
            print (g, marks[i].text, juce::Rectangle<float> (34.0f, 12.0f).withCentre (polar (c, r + 20.0f, a)),
                   Face::text, size > 60.0f ? 10.5f : 9.5f, 0.0f, colour::inkSoft);
        }

        const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        if (body.isNull() || ! juce::exactlyEqual (bodyScale, scale))
        {
            bodyScale = scale;
            body = renderBody (scale);
        }
        const auto side = (float) body.getWidth() / scale;
        g.setOpacity (isEnabled() ? 1.0f : 0.5f);
        g.drawImage (body, juce::Rectangle<float> (side, side).withCentre (c));
        g.setOpacity (1.0f);

        // The pointer turns; everything else in the body image is lit from a fixed direction.
        const auto a = angleAt (valueToProportionOfLength (getValue()));
        const float w = juce::jmax (2.0f, size * 0.036f);
        const float from = variant == Variant::metal ? 0.62f : 0.26f;
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawLine ({ polar (c, r * from, a).translated (0.0f, 0.7f), polar (c, r * 0.82f, a).translated (0.0f, 0.7f) }, w + 0.6f);
        g.setColour (juce::Colour (0xfff1ead8));
        g.drawLine ({ polar (c, r * from, a), polar (c, r * 0.82f, a) }, w);
        if (live)
        {
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (c));
        }

        if (name.isNotEmpty())
        {
            const float y = c.y + r + 18.0f;
            print (g, name, { 0.0f, y, (float) getWidth(), 14.0f }, Face::legend, 12.5f, 0.18f, legendColour);
            if (live)
                print (g, getTextFromValue (getValue()), { 0.0f, y + 15.0f, (float) getWidth(), 12.0f }, Face::mono, 9.5f, 0.0f,
                       accent.interpolatedWith (juce::Colours::white, 0.5f));
            else
                print (g, unit, { 0.0f, y + 15.0f, (float) getWidth(), 12.0f }, Face::text, 9.0f, 0.14f, colour::inkDim);
        }
    }

private:
    std::vector<double> markProportions()
    {
        std::vector<double> p;
        for (const auto& m : marks)
            p.push_back (juce::jlimit (0.0, 1.0, valueToProportionOfLength (m.value)));
        return p;
    }

    juce::Image renderBody (float scale) const
    {
        const float r = size * 0.5f, side = size * 1.6f;
        juce::Image img (juce::Image::ARGB, juce::roundToInt (side * scale), juce::roundToInt (side * scale), true);
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (scale));
        const juce::Point<float> c (side * 0.5f, side * 0.5f);
        const auto circle = [&c] (float rr) { return juce::Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (c); };

        for (int i = 6; i >= 0; --i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.075f));
            g.fillEllipse (circle (r + (float) i * r * 0.04f).translated (0.0f, r * 0.16f));
        }
        // Knurled skirt.
        g.setColour (juce::Colour (0xff181917));
        g.fillEllipse (circle (r));
        const int ridges = juce::jmax (40, juce::roundToInt (r * 2.4f));
        for (int i = 0; i < ridges; i += 2)
        {
            juce::Path seg;
            const float a0 = (float) i / (float) ridges * juce::MathConstants<float>::twoPi;
            seg.addPieSegment (circle (r), a0, a0 + juce::MathConstants<float>::twoPi / (float) ridges, 0.0f);
            g.setColour (juce::Colour (0xff2c2d2a));
            g.fillPath (seg);
        }
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        juce::Path rim;
        rim.addCentredArc (c.x, c.y, r - 0.5f, r - 0.5f, 0.0f, -1.3f, 1.3f, true);
        g.strokePath (rim, juce::PathStrokeType (1.0f));
        // Domed face.
        const float face = r * 0.82f;
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillEllipse (circle (face + 0.5f).translated (0.0f, 1.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3d3e3b), c.x - face * 0.3f, c.y - face * 0.5f,
                                                 juce::Colour (0xff151614), c.x + face * 0.4f, c.y + face, true));
        g.fillEllipse (circle (face));
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        juce::Path top;
        top.addCentredArc (c.x, c.y, face - 0.5f, face - 0.5f, 0.0f, -1.2f, 1.2f, true);
        g.strokePath (top, juce::PathStrokeType (0.8f));

        if (variant == Variant::metal)
        {
            // Spun insert: a fan of wedges gives the conic sheen; a coloured collar rings it.
            const float ins = r * 0.42f;
            g.setColour (juce::Colour (0xff0e0f0e));
            g.fillEllipse (circle (ins + 3.0f));
            g.setColour (accent);
            g.fillEllipse (circle (ins + 1.6f));
            constexpr int wedges = 120;
            for (int i = 0; i < wedges; ++i)
            {
                const float a0 = (float) i / wedges * juce::MathConstants<float>::twoPi;
                const float a1 = a0 + juce::MathConstants<float>::twoPi / wedges + 0.03f;
                const float m = 0.5f * (a0 + a1) - 0.35f;
                const float t = 0.55f + 0.3f * std::cos (2.0f * m) + 0.1f * std::cos (4.0f * m + 0.5f);
                juce::Path w;
                w.addPieSegment (circle (ins), a0, a1, 0.0f);
                g.setColour (juce::Colour (0xff8e9196).interpolatedWith (juce::Colour (0xffe6e7e9), juce::jlimit (0.0f, 1.0f, t)));
                g.fillPath (w);
            }
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.2f), c.x - ins * 0.3f, c.y - ins * 0.35f,
                                                     juce::Colours::white.withAlpha (0.0f), c.x + ins * 0.4f, c.y + ins * 0.4f, true));
            g.fillEllipse (circle (ins));
        }
        return img;
    }

    float size;
    Variant variant;
    juce::Colour accent;
    juce::String name, unit;
    std::vector<Mark> marks;
    juce::Colour legendColour = colour::ink;
    juce::Image body;
    float bodyScale = 0.0f;
};

//==============================================================================
// A row of options, each with an LED; clicking an option selects it. Bind it to a choice or bool
// parameter with attach(), or drive it yourself with setSelected / onChange.
class LedSelector : public juce::Component
{
public:
    LedSelector (juce::StringArray optionNames, juce::Colour ledColour, bool vertical = false, int columns = 1)
        : options (std::move (optionNames)), colourOn (ledColour), stacked (vertical), cols (juce::jmax (1, columns)) {}

    std::function<void (int)> onChange;

    void setSelected (int index)
    {
        if (index != selected)
        {
            selected = index;
            repaint();
        }
    }

    int getSelected() const { return selected; }

    void attach (juce::RangedAudioParameter& param, juce::UndoManager* undo = nullptr)
    {
        attachment = std::make_unique<juce::ParameterAttachment> (param, [this, &param] (float v) {
            setSelected (juce::roundToInt (param.convertTo0to1 (v) * (float) (options.size() - 1)));
        }, undo);
        onChange = [this, &param] (int i) {
            attachment->setValueAsCompleteGesture (param.convertFrom0to1 ((float) i / (float) juce::jmax (1, options.size() - 1)));
        };
        attachment->sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < options.size(); ++i)
        {
            const auto r = cell (i);
            const bool on = i == selected;
            led (g, { r.getX() + 4.5f, r.getCentreY() }, 3.5f, isEnabled() ? colourOn : colourOn.withSaturation (0.0f), on && isEnabled());
            print (g, options[i], r.withTrimmedLeft (13.0f), Face::legend, 10.5f, 0.1f,
                   (on ? colour::ink : colour::inkSoft).withMultipliedAlpha (isEnabled() ? 1.0f : 0.45f), juce::Justification::centredLeft);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! isEnabled())
            return;
        for (int i = 0; i < options.size(); ++i)
            if (cell (i).contains (e.position) && i != selected)
            {
                setSelected (i);
                if (onChange)
                    onChange (i);
            }
    }

    // Natural width of an option.
    float optionWidth (int i) const { return 18.0f + juce::GlyphArrangement::getStringWidth (font (Face::legend, 10.5f, 0.1f), options[i]); }

private:
    juce::Rectangle<float> cell (int i) const
    {
        if (stacked)
        {
            const int rows = (options.size() + cols - 1) / cols;
            const float w = (float) getWidth() / (float) cols, h = (float) getHeight() / (float) juce::jmax (1, rows);
            return { (float) (i / rows) * w, (float) (i % rows) * h, w, h };
        }
        float x = 0.0f;
        for (int k = 0; k < i; ++k)
            x += optionWidth (k) + 8.0f;
        return { x, 0.0f, optionWidth (i), (float) getHeight() };
    }

    juce::StringArray options;
    juce::Colour colourOn;
    bool stacked;
    int cols;
    int selected = 0;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// A latching switch drawn as an LED and its legend; click anywhere on it to toggle.
class LedToggle : public juce::ToggleButton
{
public:
    LedToggle (const juce::String& legend, juce::Colour ledColour, bool litWhenToggleOn = true)
        : ToggleButton (legend), colourOn (ledColour), litWhenOn (litWhenToggleOn) {}

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const bool on = getToggleState() == litWhenOn;
        const auto r = getLocalBounds().toFloat();
        led (g, { 4.5f, r.getCentreY() }, 3.5f, colourOn, on && isEnabled());
        print (g, getButtonText(), r.withTrimmedLeft (13.0f), Face::legend, 10.5f, 0.14f,
               (over || on ? colour::ink : colour::inkSoft).withMultipliedAlpha (isEnabled() ? 1.0f : 0.45f), juce::Justification::centredLeft);
    }

private:
    juce::Colour colourOn;
    bool litWhenOn;
};

// The engage switch: red LED over a push cap, "IN" printed underneath.
class InSwitch : public juce::ToggleButton
{
public:
    // Attached to a bypass parameter, so it lights while the toggle is off.
    explicit InSwitch (bool litWhenToggleOn = false) : litWhenOn (litWhenToggleOn) { setTooltip ("Engage / bypass"); }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto r = getLocalBounds().toFloat();
        const bool lit = getToggleState() == litWhenOn;
        led (g, { r.getCentreX(), 6.0f }, 4.5f, colour::red, lit);
        cap (g, juce::Rectangle<float> (38.0f, 26.0f).withCentre ({ r.getCentreX(), 32.0f }), over, down);
        print (g, "IN", { 0.0f, 50.0f, r.getWidth(), 14.0f }, Face::legend, 11.0f, 0.22f);
    }

private:
    bool litWhenOn;
};

//==============================================================================
// Backlit VU with a cream face. Reads RMS with 0 VU at `calibration` dBFS (CAL on its label row
// cycles -18 / -14 / -10 / -6). The peak lamp warns of real overs: sample peaks at -1 dBFS or above.
class VuMeter : public juce::Component
{
public:
    VuMeter (juce::String labelText, std::function<float()> rms, std::function<float()> peak, bool reduction = false)
        : label (std::move (labelText)), rmsSource (std::move (rms)), peakSource (std::move (peak)), grMode (reduction) {}

    std::function<void (float)> onCalibrationChanged;

    void setCalibration (float dbfs) { calibration = dbfs; repaint(); }

    bool tick (float dt)
    {
        const auto before = position;
        const bool wasPeak = peakHold > 0.0f;
        float target;
        if (grMode)
            target = 1.0f - juce::jlimit (0.0f, 1.0f, rmsSource() / 20.0f);
        else
            target = levelPosition (juce::Decibels::gainToDecibels (rmsSource(), -80.0f) - calibration);
        if (peakSource && peakSource() >= 0.891f)
            peakHold = 0.8f;
        // Second-order movement: ~300 ms to settle, slightly underdamped, like a real VU.
        velocity += ((target - position) * 120.0f - velocity * 19.0f) * dt;
        position = juce::jlimit (-0.03f, 1.05f, position + velocity * dt);
        peakHold = juce::jmax (0.0f, peakHold - dt);
        return std::abs (position - before) > 0.0005f || wasPeak != (peakHold > 0.0f);
    }

    void resized() override { under = {}; over = {}; }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (grMode || ! calArea().contains (e.position))
            return;
        const float steps[] { -18.0f, -14.0f, -10.0f, -6.0f };
        int i = 0;
        while (i < 4 && ! juce::exactlyEqual (steps[i], calibration))
            ++i;
        calibration = steps[(i + 1) % 4];
        if (onCalibrationChanged)
            onCalibrationChanged (calibration);
        under = {};
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        const auto frame = getLocalBounds().toFloat().withTrimmedBottom (24.0f);
        const auto face = frame.reduced (7.0f);
        if (under.isNull() || ! juce::exactlyEqual (scale, cacheScale))
        {
            cacheScale = scale;
            under = make (scale, [&] (juce::Graphics& ig) {
                bezel (ig, frame);
                drawFace (ig, face);
                print (ig, label, { 0.0f, frame.getBottom() + 6.0f, (float) getWidth(), 14.0f }, Face::legend, 11.0f, 0.22f);
                if (! grMode)
                    print (ig, "CAL " + juce::String ((int) calibration), calArea(), Face::mono, 8.5f, 0.0f, colour::inkDim,
                           juce::Justification::centredRight);
            });
            over = make (scale, [&] (juce::Graphics& ig) {
                const juce::Graphics::ScopedSaveState s (ig);
                ig.reduceClipRegion (face.toNearestInt());
                ig.setColour (juce::Colour (0xff1b1c1d));
                ig.fillEllipse (juce::Rectangle<float> (face.getWidth() * 0.3f, face.getHeight() * 0.3f).withCentre ({ face.getCentreX(), face.getBottom() + face.getHeight() * 0.04f }));
                glass (ig, face, 3.5f);
            });
        }
        g.drawImage (under, getLocalBounds().toFloat());
        {
            const juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (face.toNearestInt());
            const auto pivot = pivotFor (face);
            const auto R = radiusFor (face);
            const auto a = angleAt (position);
            g.setColour (juce::Colours::black.withAlpha (0.16f));
            g.drawLine ({ polar (pivot, R * 0.3f, a).translated (2.5f, 3.5f), polar (pivot, R * 0.97f, a).translated (2.5f, 3.5f) }, 1.4f);
            g.setColour (juce::Colour (0xff141210));
            g.drawLine ({ polar (pivot, R * 0.3f, a), polar (pivot, R * 0.97f, a) }, 1.3f);
            if (! grMode)
            {
                const juce::Point<float> lamp (face.getRight() - 10.0f, face.getY() + 10.0f);
                led (g, lamp, 3.2f, colour::red, peakHold > 0.0f);
            }
        }
        g.drawImage (over, getLocalBounds().toFloat());
    }

private:
    template <typename Fn>
    juce::Image make (float scale, Fn&& draw) const
    {
        juce::Image img (juce::Image::ARGB, juce::jmax (1, juce::roundToInt ((float) getWidth() * scale)), juce::jmax (1, juce::roundToInt ((float) getHeight() * scale)), true);
        juce::Graphics ig (img);
        ig.addTransform (juce::AffineTransform::scale (scale));
        draw (ig);
        return img;
    }

    juce::Rectangle<float> calArea() const { return { (float) getWidth() - 52.0f, (float) getHeight() - 18.0f, 50.0f, 14.0f }; }

    static constexpr float sweepDeg = 47.0f;
    static juce::Point<float> pivotFor (juce::Rectangle<float> f) { return { f.getCentreX(), f.getBottom() + f.getHeight() * 0.07f }; }
    static float radiusFor (juce::Rectangle<float> f)
    {
        return juce::jmin (f.getHeight() * 0.98f, (f.getWidth() * 0.5f - 8.0f) / (0.95f * std::sin (juce::degreesToRadians (sweepDeg))));
    }
    static float angleAt (float t) { const auto s = juce::degreesToRadians (sweepDeg); return -s + 2.0f * s * t; }
    static float levelPosition (float vu)
    {
        const auto lo = std::pow (10.0f, -20.0f / 20.0f), hi = std::pow (10.0f, 3.0f / 20.0f);
        return (std::pow (10.0f, vu / 20.0f) - lo) / (hi - lo);
    }

    void drawFace (juce::Graphics& g, juce::Rectangle<float> face) const
    {
        const juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (face.toNearestInt());
        const auto pivot = pivotFor (face);
        const auto R = radiusFor (face);
        const juce::Colour ink (0xff1f1b16), red (0xffc4291d);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffbe3a6), face.getCentreX(), face.getBottom(),
                                                 juce::Colour (0xffe2c993), face.getCentreX(), face.getY() - face.getHeight() * 0.3f, true));
        g.fillRect (face);
        const auto redFrom = grMode ? 0.0f : levelPosition (0.0f), redTo = grMode ? 0.5f : 1.0f;
        juce::Path arc, redArc;
        arc.addCentredArc (pivot.x, pivot.y, R * 0.78f, R * 0.78f, 0.0f, angleAt (grMode ? 0.5f : 0.0f), angleAt (grMode ? 1.0f : redFrom), true);
        redArc.addCentredArc (pivot.x, pivot.y, R * 0.79f, R * 0.79f, 0.0f, angleAt (redFrom), angleAt (redTo), true);
        g.setColour (ink);
        g.strokePath (arc, juce::PathStrokeType (1.2f));
        g.setColour (red);
        g.strokePath (redArc, juce::PathStrokeType (3.5f));
        const auto mark = [&] (float t, const juce::String& text, bool isRed) {
            const auto a = angleAt (t);
            g.setColour (isRed ? red : ink);
            g.drawLine ({ polar (pivot, R * 0.78f, a), polar (pivot, R * (text.isEmpty() ? 0.83f : 0.86f), a) }, text.isEmpty() ? 1.0f : 1.4f);
            if (text.isNotEmpty())
                print (g, text, juce::Rectangle<float> (26.0f, 12.0f).withCentre (polar (pivot, R * 0.95f, a)), Face::textMedium, 9.5f, 0.0f,
                       isRed ? red : ink);
        };
        if (grMode)
            for (int db = 0; db <= 20; db += 2)
                mark (1.0f - (float) db / 20.0f, db % 5 == 0 ? juce::String (db) : juce::String(), db >= 10);
        else
            for (auto [vu, text] : { std::pair { -20.0f, "20" }, { -10.0f, "10" }, { -7.0f, "7" }, { -5.0f, "5" }, { -3.0f, "3" },
                                     { -2.0f, "" }, { -1.0f, "" }, { 0.0f, "0" }, { 1.0f, "" }, { 2.0f, "" }, { 3.0f, "+3" } })
                mark (levelPosition (vu), text, vu >= 0.0f);
        print (g, grMode ? "GR" : "VU", juce::Rectangle<float> (60.0f, 20.0f).withCentre (pivot.translated (0.0f, -R * 0.5f)),
               Face::legendBold, 16.0f, 0.04f, ink);
    }

    juce::String label;
    std::function<float()> rmsSource, peakSource;
    bool grMode;
    float calibration = -10.0f;
    float position = 0.0f, velocity = 0.0f, peakHold = 0.0f;
    juce::Image under, over;
    float cacheScale = 0.0f;
};

//==============================================================================
// Ten rectangular LEDs in a column, fixed colours like real parts: green, yellow, red.
class LedLadder : public juce::Component
{
public:
    static constexpr int count = 10;

    LedLadder() { setInterceptsMouseClicks (false, false); }
    void setThresholds (const std::array<float, count>& t) { thresholds = t; }

    bool tick (float reading, float dt)
    {
        bool changed = false;
        for (int i = 0; i < count; ++i)
        {
            auto& l = levels[(size_t) i];
            const auto before = l;
            l = reading >= thresholds[(size_t) i] ? 1.0f : juce::jmax (0.0f, l - dt / 0.06f);
            changed |= std::abs (l - before) > 0.01f;
        }
        return changed;
    }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < count; ++i)
            rectLed (g, ledBounds (i), colourFor (i), levels[(size_t) i]);
    }

    juce::Rectangle<float> ledBounds (int i) const
    {
        const auto pitch = (float) getHeight() / (float) count;
        return { 2.0f, (float) getHeight() - pitch * (float) (i + 1) + (pitch - 6.0f) * 0.5f, (float) getWidth() - 4.0f, 6.0f };
    }

    static juce::Colour colourFor (int i) { return i < 6 ? juce::Colour (0xff3ee05a) : i < 8 ? juce::Colour (0xffffc928) : juce::Colour (0xffff3b2e); }

private:
    std::array<float, count> thresholds {};
    std::array<float, count> levels {};
};

//==============================================================================
// Nameplate column used at the left of every unit: maker line, model name, sub-legend.
inline void nameplate (juce::Graphics& g, juce::Point<float> at, const juce::String& maker, const juce::String& model,
                       const juce::String& subtitle, const juce::Path& mark, juce::Colour markColour)
{
    g.setColour (markColour);
    g.fillPath (mark, juce::AffineTransform::translation (at.x, at.y));
    print (g, maker, { at.x + 29.0f, at.y, 200.0f, 20.0f }, Face::legend, 14.5f, 0.34f, colour::ink, juce::Justification::centredLeft);
    print (g, model, { at.x - 1.0f, at.y + 26.0f, 260.0f, 34.0f }, Face::legendBold, 36.0f, 0.04f, colour::ink, juce::Justification::centredLeft);
    print (g, subtitle, { at.x, at.y + 60.0f, 260.0f, 12.0f }, Face::textMedium, 9.0f, 0.2f, colour::inkDim, juce::Justification::centredLeft);
}

// A rune drawn as round-capped strokes in a 20x20 box: each stroke is { x1, y1, x2, y2 }.
inline juce::Path rune (std::initializer_list<std::array<float, 4>> strokes, float thickness = 2.5f)
{
    juce::Path lines, out;
    for (const auto& s : strokes)
    {
        lines.startNewSubPath (s[0], s[1]);
        lines.lineTo (s[2], s[3]);
    }
    juce::PathStrokeType (thickness, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded).createStrokedPath (out, lines);
    return out;
}

} // namespace rackui
