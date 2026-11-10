#pragma once

#include "Look.h"
#include "Processor.h"

namespace fenix
{

namespace detail
{
    // Unit vector for a rotary angle measured clockwise from 12 o'clock.
    inline juce::Point<float> polar (juce::Point<float> centre, float radius, float angle)
    {
        return centre.translated (radius * std::sin (angle), -radius * std::cos (angle));
    }

    // Short scale label: "30 Hz" -> "30", "6 kHz" -> "6k", 2000 -> "2k".
    inline juce::String shortLabel (const juce::String& choice)
    {
        return choice.replace (" kHz", "k").replace (" Hz", "");
    }

    // The parts of a rack knob that do not turn with it (the light is fixed, so the shading on
    // the knurl and the spun-aluminium cap stays put): shadow, skirt, knurled grip and cap.
    // Drawn once per size into an image; only the pointer is drawn live.
    inline juce::Image renderKnobBody (float radius, float scale)
    {
        const auto pad = radius * 0.35f;
        const auto side = (radius + pad) * 2.0f;
        juce::Image image (juce::Image::ARGB, juce::jmax (1, juce::roundToInt (side * scale)), juce::jmax (1, juce::roundToInt (side * scale)), true);
        juce::Graphics g (image);
        g.addTransform (juce::AffineTransform::scale (scale));
        const juce::Point<float> c (side * 0.5f, side * 0.5f);
        const auto circle = [&c] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c); };

        // Soft contact shadow, falling down and slightly right.
        for (int i = 6; i >= 0; --i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.07f));
            g.fillEllipse (circle (radius + (float) i * radius * 0.035f).translated (radius * 0.05f, radius * 0.12f));
        }

        // Skirt.
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3c41), c.x, c.y - radius, juce::Colour (0xff0c0d0f), c.x, c.y + radius, false));
        g.fillEllipse (circle (radius));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1b1e), c.x, c.y - radius, juce::Colour (0xff26282c), c.x, c.y + radius, false));
        g.fillEllipse (circle (radius * 0.93f));

        // Knurled grip: fine ridges, lit from the upper left.
        const auto gripR = radius * 0.8f;
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (circle (gripR + 1.5f).translated (0.0f, 1.5f));
        g.setColour (juce::Colour (0xff141518));
        g.fillEllipse (circle (gripR));
        constexpr int ridges = 48;
        for (int i = 0; i < ridges; ++i)
        {
            const auto a = (float) i / (float) ridges * juce::MathConstants<float>::twoPi;
            const auto light = 0.5f + 0.5f * std::cos (a - juce::MathConstants<float>::pi * 1.75f);
            g.setColour (juce::Colour::fromFloatRGBA (0.16f + 0.3f * light, 0.165f + 0.3f * light, 0.18f + 0.3f * light, 1.0f));
            juce::Path ridge;
            ridge.startNewSubPath (polar (c, gripR * 0.86f, a));
            ridge.lineTo (polar (c, gripR - 0.6f, a));
            g.strokePath (ridge, juce::PathStrokeType (juce::jmax (0.8f, gripR * 0.045f)));
        }

        // Spun aluminium cap: a fan of wedges approximates the conic sheen of machined metal.
        const auto capR = gripR * 0.82f;
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (circle (capR + 1.0f).translated (0.0f, 1.0f));
        g.setColour (colours::metalDark.interpolatedWith (colours::metalLight, 0.5f).darker (0.15f));
        g.fillEllipse (circle (capR));
        constexpr int wedges = 180;
        for (int i = 0; i < wedges; ++i)
        {
            const auto a0 = (float) i / (float) wedges * juce::MathConstants<float>::twoPi;
            const auto a1 = (float) (i + 1) / (float) wedges * juce::MathConstants<float>::twoPi + 0.04f;
            const auto mid = 0.5f * (a0 + a1);
            const auto sheen = 0.5f + 0.26f * std::cos (2.0f * (mid - 0.75f)) + 0.12f * std::cos (mid - 5.5f);
            juce::Path wedge;
            wedge.addPieSegment (circle (capR), a0, a1, 0.0f);
            g.setColour (colours::metalDark.interpolatedWith (colours::metalLight, juce::jlimit (0.0f, 1.0f, sheen)).darker (0.15f));
            g.fillPath (wedge);
        }
        // Lathe rings.
        for (float rr = capR * 0.15f; rr < capR; rr += 1.3f)
        {
            g.setColour ((((int) (rr * 7.0f)) % 3 == 0 ? juce::Colours::white : juce::Colours::black).withAlpha (0.035f));
            g.drawEllipse (circle (rr), 0.6f);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, c, juce::Colours::black.withAlpha (0.25f), c.translated (capR, 0.0f), true));
        g.fillEllipse (circle (capR));
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawEllipse (circle (capR - 0.5f), 0.8f);

        // Rim highlight on the skirt's upper edge.
        juce::Path rim;
        rim.addCentredArc (c.x, c.y, radius - 0.6f, radius - 0.6f, 0.0f, -1.2f, 1.2f, true);
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.strokePath (rim, juce::PathStrokeType (1.0f));
        return image;
    }

    inline juce::String shortLabel (float value, bool signedScale)
    {
        if (std::abs (value) >= 1000.0f)
            return juce::String (juce::roundToInt (value / 1000.0f)) + "k";
        const auto rounded = juce::roundToInt (value);
        return (signedScale && rounded > 0 ? "+" : "") + juce::String (rounded);
    }
}

//==============================================================================
// Rotary: dark dished knob with a glowing value arc and an accent indicator.
// Choice parameters become stepped switches with a lit dot and a label at every position.
class Knob : public juce::Slider
{
public:
    Knob (juce::RangedAudioParameter& p, juce::Colour accentColour, const juce::String& label = {})
        : name ((label.isEmpty() ? p.getName (24) : label).toUpperCase()), accent (accentColour)
    {
        setSliderStyle (RotaryHorizontalVerticalDrag);
        setTextBoxStyle (NoTextBox, false, 0, 0);
        setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);

        const auto& range = p.getNormalisableRange();
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&p))
        {
            for (const auto& c : choice->choices)
                stepLabels.add (detail::shortLabel (c));
        }
        else
        {
            bipolar = range.start < 0.0f && range.end > 0.0f;
            endLabels[0] = detail::shortLabel (range.start, bipolar);
            endLabels[1] = detail::shortLabel (range.end, bipolar);
        }
    }

    bool isStepped() const { return ! stepLabels.isEmpty(); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        const auto valueArea = bounds.removeFromBottom (16.0f);
        const auto nameArea = bounds.removeFromBottom (18.0f);

        const bool stepped = ! stepLabels.isEmpty();
        const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto centre = bounds.getCentre();
        const auto ringR = size * 0.5f - (stepped ? 18.0f : 8.0f);
        const auto r = ringR - 8.0f;
        const auto params = getRotaryParameters();
        const auto pos = (float) valueToProportionOfLength (getValue());
        const auto angleAt = [&] (float t) { return params.startAngleRadians + t * (params.endAngleRadians - params.startAngleRadians); };
        const bool active = isMouseOverOrDragging();
        const auto ink = colours::engrave.withAlpha (0.85f);

        // Silkscreened scale: a tick at every position of a switch, eleven ticks on a pot.
        const int steps = stepped ? stepLabels.size() - 1 : 10;
        for (int i = 0; i <= steps; ++i)
        {
            const auto t = (float) i / (float) steps;
            const bool major = stepped || i == 0 || i == steps || i == steps / 2;
            g.setColour (major ? ink : ink.withAlpha (0.5f));
            g.drawLine ({ detail::polar (centre, ringR - (major ? 5.0f : 3.5f), angleAt (t)), detail::polar (centre, ringR, angleAt (t)) }, major ? 1.4f : 1.0f);
        }

        g.setFont (draw::font (9.5f, true));
        if (stepped)
        {
            for (int i = 0; i <= steps; ++i)
            {
                const auto t = (float) i / (float) steps;
                const bool selected = std::abs (t - pos) < 0.5f / (float) steps;
                g.setColour (selected ? accent.brighter (0.5f) : colours::engraveDim);
                g.drawText (stepLabels[i], juce::Rectangle<float> (34.0f, 12.0f).withCentre (detail::polar (centre, ringR + 11.0f, angleAt (t))),
                            juce::Justification::centred);
            }
        }
        else
        {
            g.setColour (colours::engraveDim);
            for (int end = 0; end < 2; ++end)
            {
                const auto p = detail::polar (centre, ringR, angleAt ((float) end)).translated (end == 0 ? -4.0f : 4.0f, 12.0f);
                g.drawText (endLabels[end], juce::Rectangle<float> (34.0f, 12.0f).withCentre (p), juce::Justification::centred);
            }
        }

        // Body from the cache, then the parts that turn: the skirt's printed line and the cap's painted pointer.
        const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        if (body.isNull() || bodyRadius != r || bodyScale != scale)
        {
            bodyRadius = r;
            bodyScale = scale;
            body = detail::renderKnobBody (r, scale);
        }
        const auto side = (float) body.getWidth() / scale;
        g.drawImage (body, juce::Rectangle<float> (side, side).withCentre (centre));

        const auto a = angleAt (pos);
        g.setColour (colours::engrave);
        g.drawLine ({ detail::polar (centre, r * 0.84f, a), detail::polar (centre, r * 0.98f, a) }, 2.0f);
        juce::Path pointer;
        pointer.startNewSubPath (detail::polar (centre, r * 0.12f, a));
        pointer.lineTo (detail::polar (centre, r * 0.6f, a));
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.strokePath (pointer, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation (0.0f, 0.8f));
        g.setColour (accent.darker (0.1f));
        g.strokePath (pointer, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (active)
        {
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre));
        }

        g.setFont (juce::FontOptions (10.5f, juce::Font::bold).withKerningFactor (0.14f));
        draw::engraved (g, name, nameArea, juce::Justification::centred, colours::engrave);
        g.setFont (draw::font (11.5f));
        g.setColour (active ? accent.brighter (0.5f) : colours::engraveDim);
        g.drawText (getTextFromValue (getValue()), valueArea, juce::Justification::centred);
    }

private:
    juce::String name;
    juce::Colour accent;
    juce::StringArray stepLabels;
    juce::String endLabels[2];
    bool bipolar = false;
    juce::Image body;
    float bodyRadius = 0.0f, bodyScale = 0.0f;
};

//==============================================================================
// Pill switch with a glowing track when engaged. By default it is attached to "bypass",
// so it lights while the toggle is off; pass litWhenOn for ordinary on/off parameters.
class PowerButton : public juce::ToggleButton
{
public:
    explicit PowerButton (juce::Colour accentColour, bool litWhenToggleOn = false, juce::String legendText = "IN")
        : accent (accentColour), litWhenOn (litWhenToggleOn), legend (std::move (legendText))
    {
        setTooltip ("Engage / bypass");
    }

    // Illuminated push button: a square lens in a black collar that glows when engaged.
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto collar = getLocalBounds().toFloat().reduced (2.0f);
        const bool lit = litWhenOn == getToggleState();

        if (lit)
        {
            g.setColour (accent.withAlpha (0.18f));
            g.fillRoundedRectangle (collar.expanded (2.0f), 6.0f);
        }
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.fillRoundedRectangle (collar, 4.0f);

        auto lens = collar.reduced (2.5f);
        if (down)
            lens.translate (0.0f, 0.8f);
        if (lit)
            g.setGradientFill (juce::ColourGradient (accent.brighter (0.6f), lens.getCentreX(), lens.getCentreY(),
                                                     accent.darker (0.25f), lens.getX(), lens.getY(), true));
        else
            g.setGradientFill (juce::ColourGradient (accent.darker (1.6f).withSaturation (0.4f), 0.0f, lens.getY(),
                                                     accent.darker (2.6f).withSaturation (0.3f), 0.0f, lens.getBottom(), false));
        g.fillRoundedRectangle (lens, 2.5f);
        g.setColour (juce::Colours::white.withAlpha (lit ? 0.45f : (over ? 0.2f : 0.1f)));
        g.drawLine (lens.getX() + 2.0f, lens.getY() + 0.7f, lens.getRight() - 2.0f, lens.getY() + 0.7f, 1.0f);

        g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.15f));
        g.setColour (lit ? juce::Colour (0xff1b1208).withAlpha (0.8f) : accent.withAlpha (0.45f));
        g.drawText (legend, lens, juce::Justification::centred);
    }

private:
    juce::Colour accent;
    bool litWhenOn;
    juce::String legend;
};

//==============================================================================
// Analogue-style VU on a dark face, with spring-damped needle ballistics.
// Level mode reads RMS with 0 VU = -18 dBFS; Reduction mode reads gain reduction in dB.
class VuMeter : public juce::Component
{
public:
    enum class Mode { level, reduction };

    VuMeter (juce::String labelText, Mode m, std::function<float()> readSource, juce::Colour accentColour = colours::engrave)
        : label (std::move (labelText)), mode (m), source (std::move (readSource)), accent (accentColour)
    {
        setInterceptsMouseClicks (false, false);
    }

    // Returns true when the needle or peak light moved enough to need a repaint.
    bool tick (float dt)
    {
        const auto previous = position;
        const auto wasPeaking = peakHold > 0.0f;
        float target;
        if (mode == Mode::level)
        {
            const auto vu = juce::Decibels::gainToDecibels (source(), -60.0f) + 18.0f;
            target = levelPosition (vu);
            if (vu > 1.0f)
                peakHold = 0.6f;
        }
        else
        {
            target = 1.0f - juce::jlimit (0.0f, 1.0f, source() / 20.0f);
        }

        // Second-order needle: natural frequency ~13 rad/s, slightly underdamped like a real movement.
        velocity += ((target - position) * 170.0f - velocity * 22.0f) * dt;
        position = juce::jlimit (-0.02f, 1.06f, position + velocity * dt);
        peakHold = juce::jmax (0.0f, peakHold - dt);
        return std::abs (position - previous) > 0.0005f || wasPeaking != (peakHold > 0.0f);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f, 0.0f).withTrimmedBottom (3.0f);
        const auto labelArea = bounds.removeFromBottom (22.0f);
        const auto frame = bounds.reduced (2.0f, 2.0f);
        const auto face = frame.reduced (8.0f);

        // Everything but the needle and peak lamp is static: bezel, face and printed scale go in one
        // image, the glass and the pivot cover that sit over the needle in another.
        const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        if (under.isNull() || scale != cacheScale)
        {
            cacheScale = scale;
            const auto make = [&] (auto&& draw) {
                juce::Image image (juce::Image::ARGB, juce::jmax (1, juce::roundToInt ((float) getWidth() * scale)),
                                   juce::jmax (1, juce::roundToInt ((float) getHeight() * scale)), true);
                juce::Graphics ig (image);
                ig.addTransform (juce::AffineTransform::scale (scale));
                draw (ig);
                return image;
            };
            under = make ([&] (juce::Graphics& ig) {
                draw::bezel (ig, frame, 9.0f);
                draw::screw (ig, { frame.getX() + 6.0f, frame.getY() + 6.0f }, 2.6f, 0.3f);
                draw::screw (ig, { frame.getRight() - 6.0f, frame.getY() + 6.0f }, 2.6f, 1.1f);
                drawFace (ig, face);
                ig.setFont (juce::FontOptions (10.5f, juce::Font::bold).withKerningFactor (0.16f));
                draw::engraved (ig, label, labelArea.translated (0.0f, -1.0f), juce::Justification::centred, colours::engrave);
            });
            over = make ([&] (juce::Graphics& ig) { drawGlass (ig, face); });
        }

        g.drawImage (under, getLocalBounds().toFloat());

        {
            const juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (face.toNearestInt());
            const auto pivot = pivotFor (face);
            const auto radius = radiusFor (face);
            const auto a = angleAt (position);
            const juce::Line<float> needle (detail::polar (pivot, radius * 0.3f, a), detail::polar (pivot, radius * 0.97f, a));
            g.setColour (juce::Colours::black.withAlpha (0.18f));
            g.drawLine ({ needle.getStart().translated (3.0f, 4.0f), needle.getEnd().translated (3.0f, 4.0f) }, 1.6f);
            g.setColour (juce::Colour (0xff141210));
            g.drawLine (needle, 1.3f);
            if (mode == Mode::level)
                draw::led (g, { face.getRight() - 11.0f, face.getY() + 11.0f }, 3.2f, juce::Colour (0xffff3b2a), peakHold > 0.0f);
        }

        g.drawImage (over, getLocalBounds().toFloat());
    }

    void resized() override { under = {}; over = {}; }

private:
    static constexpr float sweepDegrees = 47.0f;
    static juce::Point<float> pivotFor (juce::Rectangle<float> face) { return { face.getCentreX(), face.getBottom() - face.getHeight() * 0.04f }; }
    static float radiusFor (juce::Rectangle<float> face)
    {
        return juce::jmin (face.getHeight() * 0.9f, (face.getWidth() * 0.5f - 12.0f) / (0.93f * std::sin (juce::degreesToRadians (sweepDegrees))));
    }
    static float angleAt (float t) { const auto sweep = juce::degreesToRadians (sweepDegrees); return -sweep + 2.0f * sweep * t; }

    // Backlit cream face with black ink, as on a classic studio VU.
    void drawFace (juce::Graphics& g, juce::Rectangle<float> face) const
    {
        const juce::Graphics::ScopedSaveState save (g);
        juce::Path faceShape;
        faceShape.addRoundedRectangle (face, 4.0f);
        g.reduceClipRegion (faceShape);

        const auto pivot = pivotFor (face);
        const auto radius = radiusFor (face);
        const juce::Colour ink (0xff1f1b16), red (0xffc4291d);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff6eccb), face.getCentreX(), face.getY(),
                                                 juce::Colour (0xffe5cf98), face.getCentreX(), face.getBottom(), false));
        g.fillRect (face);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0x40ffb347), pivot.x, face.getBottom(),
                                                 juce::Colour (0x00ffb347), pivot.x, face.getY(), true));
        g.fillRect (face);

        const auto redStart = mode == Mode::level ? levelPosition (0.0f) : 0.0f;
        const auto redEnd = mode == Mode::level ? 1.0f : 0.5f;
        juce::Path arc, redArc;
        arc.addCentredArc (pivot.x, pivot.y, radius * 0.78f, radius * 0.78f, 0.0f, angleAt (0.0f), angleAt (redStart), true);
        redArc.addCentredArc (pivot.x, pivot.y, radius * 0.79f, radius * 0.79f, 0.0f, angleAt (redStart), angleAt (redEnd), true);
        g.setColour (ink);
        g.strokePath (arc, juce::PathStrokeType (1.2f));
        g.setColour (red);
        g.strokePath (redArc, juce::PathStrokeType (3.5f));

        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        const auto drawMark = [&] (float t, const juce::String& text, bool isRed) {
            const auto a = angleAt (t);
            g.setColour (isRed ? red : ink);
            g.drawLine ({ detail::polar (pivot, radius * 0.78f, a), detail::polar (pivot, radius * (text.isEmpty() ? 0.83f : 0.86f), a) },
                        text.isEmpty() ? 1.0f : 1.4f);
            if (text.isNotEmpty())
                g.drawText (text, juce::Rectangle<float> (26.0f, 12.0f).withCentre (detail::polar (pivot, radius * 0.93f, a)),
                            juce::Justification::centred);
        };

        if (mode == Mode::level)
        {
            for (auto [vu, text] : { std::pair { -20.0f, "20" }, { -10.0f, "10" }, { -7.0f, "7" }, { -5.0f, "5" },
                                     { -3.0f, "3" }, { -2.0f, "" }, { -1.0f, "" }, { 0.0f, "0" }, { 1.0f, "" },
                                     { 2.0f, "" }, { 3.0f, "+3" } })
                drawMark (levelPosition (vu), text, vu >= 0.0f);
        }
        else
        {
            for (int db = 0; db <= 20; db += 2)
                drawMark (1.0f - (float) db / 20.0f, db % 5 == 0 ? juce::String (db) : juce::String(), db >= 10);
        }

        g.setColour (ink.withAlpha (0.85f));
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold).withKerningFactor (0.05f));
        g.drawText (mode == Mode::level ? "VU" : "GR", juce::Rectangle<float> (60.0f, 20.0f).withCentre (pivot.translated (0.0f, -radius * 0.5f)),
                    juce::Justification::centred);
        g.setFont (juce::FontOptions (7.5f, juce::Font::bold).withKerningFactor (0.3f));
        g.setColour (ink.withAlpha (0.5f));
        g.drawText ("FENRIKOS", juce::Rectangle<float> (80.0f, 10.0f).withCentre ({ face.getCentreX(), face.getY() + 14.0f }), juce::Justification::centred);
    }

    // Pivot cover, the window's inner shadow and a glass reflection: these sit over the needle.
    static void drawGlass (juce::Graphics& g, juce::Rectangle<float> face)
    {
        juce::Path faceShape;
        faceShape.addRoundedRectangle (face, 4.0f);
        const juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (faceShape);

        const auto cover = juce::Rectangle<float> (face.getWidth() * 0.26f, face.getHeight() * 0.2f)
                               .withCentre ({ face.getCentreX(), face.getBottom() + face.getHeight() * 0.03f });
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3c41), cover.getCentreX(), cover.getY(),
                                                 juce::Colour (0xff101113), cover.getCentreX(), cover.getCentreY(), false));
        g.fillEllipse (cover);

        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.45f), 0.0f, face.getY(),
                                                 juce::Colours::black.withAlpha (0.0f), 0.0f, face.getY() + 14.0f, false));
        g.fillRect (face.withHeight (14.0f));
        for (auto side : { face.withWidth (8.0f), face.withLeft (face.getRight() - 8.0f) })
        {
            const bool left = side.getX() == face.getX();
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.25f), left ? face.getX() : face.getRight(), 0.0f,
                                                     juce::Colours::black.withAlpha (0.0f), left ? face.getX() + 8.0f : face.getRight() - 8.0f, 0.0f, false));
            g.fillRect (side);
        }

        juce::Path sheen;
        sheen.startNewSubPath (face.getX(), face.getY());
        sheen.lineTo (face.getX() + face.getWidth() * 0.62f, face.getY());
        sheen.lineTo (face.getX() + face.getWidth() * 0.32f, face.getBottom());
        sheen.lineTo (face.getX(), face.getBottom());
        sheen.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), face.getX(), face.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), face.getX() + face.getWidth() * 0.4f, face.getCentreY(), false));
        g.fillPath (sheen);
    }

    // Real VU scales are linear in voltage, so 0 VU sits about two thirds of the way across.
    static float levelPosition (float vu)
    {
        const auto lo = std::pow (10.0f, -20.0f / 20.0f), hi = std::pow (10.0f, 3.0f / 20.0f);
        return (std::pow (10.0f, vu / 20.0f) - lo) / (hi - lo);
    }

    juce::String label;
    Mode mode;
    std::function<float()> source;
    juce::Colour accent;
    float position = 0.0f, velocity = 0.0f, peakHold = 0.0f;
    juce::Image under, over;
    float cacheScale = 0.0f;
};

//==============================================================================
// Dark screen inset in a card. Subclasses draw into the screen and get fresh output
// samples from the processor once per frame.
class Display : public juce::Component
{
public:
    // `paramPrefix` is prepended to parameter ids, so a module's view can sit inside a larger plugin.
    Display (Processor& p, juce::Colour accentColour, juce::String paramPrefix = {})
        : proc (p), accent (accentColour), prefix (std::move (paramPrefix))
    {
        setInterceptsMouseClicks (false, false);
    }

    explicit Display (Processor& p) : Display (p, p.accent()) {}

    void tick()
    {
        const int n = proc.scope.pull (left.data(), right.data(), ScopeFifo::capacity);
        onSamples (left.data(), right.data(), n);
        update();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (2.0f, 1.0f).withTrimmedBottom (3.0f);
        const auto screen = bounds.reduced (8.0f);
        juce::Path screenShape;
        screenShape.addRoundedRectangle (screen, 4.0f);

        // Bezel, glass and glow never change, and filling the glow live every frame is most
        // of the cost of a frame, so they are rendered once per size and display scale, as is
        // the reflection that sits over the trace.
        const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        if (background.isNull() || scale != backgroundScale)
        {
            backgroundScale = scale;
            const auto w = juce::jmax (1, juce::roundToInt ((float) getWidth() * scale));
            const auto h = juce::jmax (1, juce::roundToInt ((float) getHeight() * scale));
            background = juce::Image (juce::Image::ARGB, w, h, true);
            {
                juce::Graphics bg (background);
                bg.addTransform (juce::AffineTransform::scale (scale));
                draw::bezel (bg, bounds, 9.0f);
                bg.setColour (colours::glass);
                bg.fillPath (screenShape);
                bg.reduceClipRegion (screenShape);
                bg.setGradientFill (juce::ColourGradient (accent.withAlpha (0.12f), screen.getCentreX(), screen.getBottom(),
                                                          accent.withAlpha (0.0f), screen.getCentreX(), screen.getY(), true));
                bg.fillRect (screen);
            }
            glass = juce::Image (juce::Image::ARGB, w, h, true);
            {
                juce::Graphics gl (glass);
                gl.addTransform (juce::AffineTransform::scale (scale));
                gl.reduceClipRegion (screenShape);
                gl.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.7f), 0.0f, screen.getY(),
                                                          juce::Colours::black.withAlpha (0.0f), 0.0f, screen.getY() + 12.0f, false));
                gl.fillRect (screen.withHeight (12.0f));
                juce::Path sheen;
                sheen.startNewSubPath (screen.getX(), screen.getY());
                sheen.lineTo (screen.getX() + screen.getWidth() * 0.45f, screen.getY());
                sheen.lineTo (screen.getX() + screen.getWidth() * 0.25f, screen.getBottom());
                sheen.lineTo (screen.getX(), screen.getBottom());
                sheen.closeSubPath();
                gl.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), screen.getX(), screen.getY(),
                                                          juce::Colours::white.withAlpha (0.0f), screen.getX() + screen.getWidth() * 0.3f, screen.getCentreY(), false));
                gl.fillPath (sheen);
            }
        }
        g.drawImage (background, getLocalBounds().toFloat());

        {
            const juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (screenShape);
            drawContent (g, screen.reduced (10.0f, 8.0f));
        }

        g.drawImage (glass, getLocalBounds().toFloat());
    }

    void resized() override { background = {}; glass = {}; }

protected:
    virtual void onSamples (const float*, const float*, int) {}
    virtual void update() {}
    virtual void drawContent (juce::Graphics&, juce::Rectangle<float> area) = 0;

    float paramValue (const char* id) const { return proc.apvts.getRawParameterValue (prefix + id)->load(); }

    void drawGridLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                        juce::Justification just = juce::Justification::centredLeft) const
    {
        g.setColour (accent.withAlpha (0.45f));
        g.setFont (draw::mono (10.0f));
        g.drawText (text, area, just, false);
    }

    juce::Colour gridColour() const { return accent.withAlpha (0.1f); }

    Processor& proc;
    juce::Colour accent;

private:
    juce::String prefix;
    std::array<float, ScopeFifo::capacity> left {}, right {};
    juce::Image background, glass;
    float backgroundScale = 0.0f;
};

//==============================================================================
// Default screen: a triggered oscilloscope of the output.
class ScopeDisplay : public Display
{
public:
    using Display::Display;

protected:
    void onSamples (const float* l, const float* r, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            history[(size_t) writePos] = 0.5f * (l[i] + r[i]);
            writePos = (writePos + 1) % (int) history.size();
        }
    }

    void drawContent (juce::Graphics& g, juce::Rectangle<float> area) override
    {
        g.setColour (gridColour());
        g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());
        drawTrace (g, area, 1.0f);
    }

    // Draws `window` samples starting at the most recent rising zero crossing, so the trace stands still.
    void drawTrace (juce::Graphics& g, juce::Rectangle<float> area, float gain, int window = 1024) const
    {
        const auto size = (int) history.size();
        int start = (writePos - window - 1 + size) % size;
        for (int back = 0; back < 2048; ++back)
        {
            const int i = (writePos - window - back - 1 + 2 * size) % size;
            if (history[(size_t) i] <= 0.0f && history[(size_t) ((i + 1) % size)] > 0.0f)
            {
                start = i;
                break;
            }
        }

        // One vertex per pixel column is all the screen can show.
        juce::Path trace;
        const auto columns = juce::jmax (2, (int) area.getWidth());
        for (int c = 0; c < columns; ++c)
        {
            const auto k = c * (window - 1) / (columns - 1);
            const auto v = juce::jlimit (-1.0f, 1.0f, history[(size_t) ((start + k) % size)] * gain);
            const auto x = area.getX() + (float) c;
            const auto y = area.getCentreY() - v * area.getHeight() * 0.46f;
            c == 0 ? trace.startNewSubPath (x, y) : trace.lineTo (x, y);
        }
        draw::glow (g, trace, accent.brighter (0.3f), 1.6f);
    }

    std::array<float, 4096> history {};
    int writePos = 0;
};

} // namespace fenix
