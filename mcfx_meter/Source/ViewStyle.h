/*
 ==============================================================================

 This file is part of the mcfx (Multichannel Effects) plug-in suite.
 Copyright (c) 2013/2014 - Matthias Kronlachner
 www.matthiaskronlachner.com

 Permission is granted to use this software under the terms of:
 the GPL v2 (or any later version)

 Details of these licenses can be found at: www.gnu.org/licenses

 mcfx is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

 ==============================================================================
 */

#ifndef VIEWSTYLE_H_INCLUDED
#define VIEWSTYLE_H_INCLUDED

#include "JuceHeader.h"
#include <functional>
#include <vector>

/** What the meter's views share, so they look like one plug-in: the palette,
    the readout box and status tag, the gear and its settings panel, the
    toolbar look, and which frequencies get a grid line. */
namespace ViewStyle
{
    // Plot surfaces, as alphas of white over the plot background.
    const Colour plotBg (0xe0101010);
    constexpr float gridMinor  = 0.08f;
    constexpr float gridMajor  = 0.18f;
    constexpr float frame      = 0.3f;
    constexpr float axisText   = 0.75f;
    constexpr float statusText = 0.6f;
    constexpr float crosshair  = 0.4f;
    constexpr float plateAlpha = 0.78f;        // black behind readouts and tags

    const Colour accent (0xff7fffd4);          // aquamarine, the plug-in's colour
    const Colour accentOrange (0xffff8a3d);
    const Colour pausedText (0xffff6060);
    const Colour pauseButtonOn (0xffb03030);

    //==========================================================================
    /** The dark look of every control: toolbars, the editor's strip, the
        settings callouts and the menus they open. */
    class MeterLookAndFeel : public LookAndFeel_V3
    {
    public:
        MeterLookAndFeel()
        {
            const Colour field (0xff262626), edge (0xff4a4a4a), text (0xffe8e8e8);

            setColour (ComboBox::backgroundColourId, field);
            setColour (ComboBox::textColourId, text);
            setColour (ComboBox::outlineColourId, edge);
            setColour (ComboBox::arrowColourId, text.withAlpha (0.7f));
            setColour (ComboBox::focusedOutlineColourId, accent.withAlpha (0.6f));

            setColour (PopupMenu::backgroundColourId, Colour (0xff1e1e1e));
            setColour (PopupMenu::textColourId, text);
            setColour (PopupMenu::headerTextColourId, text.withAlpha (0.6f));
            setColour (PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
            setColour (PopupMenu::highlightedTextColourId, Colours::white);

            setColour (TextButton::buttonColourId, Colour (0xff3a3a3a));
            setColour (TextButton::buttonOnColourId, pauseButtonOn);
            setColour (TextButton::textColourOffId, text);
            setColour (TextButton::textColourOnId, Colours::white);

            setColour (ToggleButton::textColourId, text);
            setColour (ToggleButton::tickColourId, text);
            setColour (ToggleButton::tickDisabledColourId, text.withAlpha (0.4f));

            setColour (Label::textColourId, text);
            setColour (Slider::textBoxBackgroundColourId, field);
            setColour (Slider::textBoxTextColourId, text);
            setColour (Slider::textBoxOutlineColourId, edge);
            setColour (TextEditor::backgroundColourId, field);
            setColour (TextEditor::textColourId, text);
            setColour (TextEditor::highlightColourId, accent.withAlpha (0.35f));
            setColour (CaretComponent::caretColourId, text);

            setColour (BubbleComponent::backgroundColourId, Colour (0xf0202020));
            setColour (BubbleComponent::outlineColourId, edge);
            setColour (TooltipWindow::backgroundColourId, Colour (0xf0202020));
            setColour (TooltipWindow::textColourId, text);
            setColour (TooltipWindow::outlineColourId, edge);
        }

        Font getComboBoxFont (ComboBox& box) override
        {
            return withDefaultMetrics (FontOptions (jmin (13.f, (float) box.getHeight() * 0.62f)));
        }

        Font getPopupMenuFont() override { return withDefaultMetrics (FontOptions (14.f)); }

        Font getTextButtonFont (TextButton&, int buttonHeight) override
        {
            return withDefaultMetrics (FontOptions (jmin (13.f, (float) buttonHeight * 0.6f)));
        }
    };

    //==========================================================================
    /** Lines of text on a dark rounded plate, next to `anchor` and kept inside
        `area`: the hover readouts. The first colour that is missing is white. */
    inline void drawReadoutBox (Graphics& g, const StringArray& lines, const Array<Colour>& colours,
                                Point<float> anchor, Rectangle<float> area, float s)
    {
        if (lines.isEmpty())
            return;

        auto S = [s] (float v) { return roundToInt (v * s); };
        g.setFont (Font (FontOptions (11.f * s)));
        int bw = 0;
        for (auto& l : lines)
            bw = jmax (bw, GlyphArrangement::getStringWidthInt (g.getCurrentFont(), l));
        bw += S (12);
        const int lh = S (14);
        const int bh = lh * lines.size() + S (6);
        const int off = S (12);

        int bx = (int) anchor.x + off, by = (int) anchor.y + off;
        if (bx + bw > (int) area.getRight() - 2)  bx = (int) anchor.x - off - bw;
        if (by + bh > (int) area.getBottom() - 2) by = (int) anchor.y - off - bh;
        bx = jmax ((int) area.getX() + 2, bx);
        by = jmax ((int) area.getY() + 2, by);

        g.setColour (Colours::black.withAlpha (plateAlpha));
        g.fillRoundedRectangle ((float) bx, (float) by, (float) bw, (float) bh, 3.f * s);
        for (int i = 0; i < lines.size(); ++i)
        {
            g.setColour (i < colours.size() ? colours[i] : Colours::white);
            g.drawText (lines[i], bx + S (6), by + S (3) + lh * i, bw - S (8), lh, Justification::centredLeft);
        }
    }

    /** A one-line tag on a plate in the top right corner of `plot`, `row`
        lines down: the zoom with how to undo it, and PAUSED. */
    inline void drawStatusTag (Graphics& g, const String& text, Colour colour,
                               Rectangle<float> plot, int row, float s)
    {
        if (text.isEmpty())
            return;

        auto S = [s] (float v) { return roundToInt (v * s); };
        g.setFont (Font (FontOptions (11.f * s)));
        const int tw = GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + S (10);
        const int th = S (14);
        const int x = (int) plot.getRight() - tw - 3;
        const int y = (int) plot.getY() + 3 + row * (th + 2);
        g.setColour (Colours::black.withAlpha (plateAlpha));
        g.fillRoundedRectangle ((float) x, (float) y, (float) tw, (float) th, 3.f * s);
        g.setColour (colour);
        g.drawText (text, x, y, tw, th, Justification::centred);
    }

    /** The hint every zoomable view shows while zoomed. */
    inline String resetHint() { return "(double click: reset)"; }

    //==========================================================================
    /** A gear, eight teeth around a hole, for the settings button. */
    inline void makeGearButton (ShapeButton& b, const String& tooltip)
    {
        Path gear;
        gear.addStar ({ 8.f, 8.f }, 8, 5.2f, 8.f);
        gear.addEllipse (5.2f, 5.2f, 5.6f, 5.6f);
        gear.setUsingNonZeroWinding (false);
        b.setShape (gear, false, true, false);
        b.setBorderSize (BorderSize<int> (3));
        b.setTooltip (tooltip);
    }

    /** Shows `content` in a callout from `anchor`, with the meter's look: the
        callout has no parent to inherit it from. */
    inline CallOutBox& launchCallout (std::unique_ptr<Component> content, Component& anchor)
    {
        auto& box = CallOutBox::launchAsynchronously (std::move (content), anchor.getScreenBounds(), nullptr);
        box.setLookAndFeel (&anchor.getLookAndFeel());
        return box;
    }

    /** The settings behind a gear button: labelled drop-downs, then toggles.
        Edits `Settings` in place and reports each change. A view's panel adds
        its rows in its constructor and then calls fitToContent(). */
    template <typename Settings>
    class SettingsPanel : public Component
    {
    public:
        SettingsPanel (Settings& s, std::function<void()> onChange)
            : settings (s), changed (std::move (onChange)) {}

        void resized() override
        {
            int y = 8;
            for (int i = 0; i < combos.size(); ++i, y += 28)
            {
                labels[i]->setBounds (8, y, kLabelW, 22);
                combos[i]->setBounds (kFieldX, y, getWidth() - kFieldX - 8, 22);
            }
            for (auto* t : toggles)
            {
                t->setBounds (kFieldX, y, getWidth() - kFieldX - 8, 22);
                y += 24;
            }
        }

    protected:
        void addRow (const String& name, const StringArray& items, int selected, const String& tip,
                     std::function<void (Settings&, int)> set)
        {
            auto* l = labels.add (new Label ({}, name));
            l->setJustificationType (Justification::centredRight);
            addAndMakeVisible (l);

            auto* cb = combos.add (new ComboBox());
            cb->addItemList (items, 1);
            cb->setSelectedItemIndex (selected, dontSendNotification);
            cb->setTooltip (tip);
            cb->onChange = [this, cb, set] { set (settings, cb->getSelectedItemIndex()); changed(); };
            addAndMakeVisible (cb);
        }

        void addToggle (const String& name, bool on, const String& tip,
                        std::function<void (Settings&, bool)> set)
        {
            auto* t = toggles.add (new ToggleButton (name));
            t->setToggleState (on, dontSendNotification);
            t->setTooltip (tip);
            t->onClick = [this, t, set] { set (settings, t->getToggleState()); changed(); };
            addAndMakeVisible (t);
        }

        void fitToContent()
        {
            setSize (330, 8 + combos.size() * 28 + toggles.size() * 24 + 6);
        }

    private:
        static constexpr int kLabelW = 104, kFieldX = 114;

        Settings& settings;
        std::function<void()> changed;
        OwnedArray<Label> labels;
        OwnedArray<ComboBox> combos;
        OwnedArray<ToggleButton> toggles;
    };

    //==========================================================================
    /** The frequencies between lo and hi that get a grid line, none closer
        than `minGap` px to another: decades first, then 2s and 5s, then the
        rest, so thinning never drops 1k for 1.5k. Ascending. */
    inline std::vector<float> freqTicks (float lo, float hi, const std::function<float (float)>& toPos,
                                         float minGap)
    {
        static const std::vector<std::vector<float>> tiers {
            { 10.f, 100.f, 1000.f, 10000.f },
            { 20.f, 50.f, 200.f, 500.f, 2000.f, 5000.f, 20000.f },
            { 30.f, 40.f, 60.f, 80.f, 150.f, 300.f, 400.f, 600.f, 800.f, 1500.f, 3000.f,
              4000.f, 6000.f, 8000.f, 12000.f, 15000.f } };

        std::vector<float> picked;
        std::vector<float> pos;
        for (auto& tier : tiers)
            for (float f : tier)
            {
                if (f < lo * 0.999f || f > hi * 1.001f)
                    continue;
                const float p = toPos (f);
                bool clear = true;
                for (float q : pos)
                    if (std::abs (p - q) < minGap) { clear = false; break; }
                if (clear)
                {
                    picked.push_back (f);
                    pos.push_back (p);
                }
            }
        std::sort (picked.begin(), picked.end());
        return picked;
    }

    inline bool isDecade (float hz) { return hz == 10.f || hz == 100.f || hz == 1000.f || hz == 10000.f; }

    /** "80", "1.5k", "20k". */
    inline String freqLabel (float hz)
    {
        return hz >= 1000.f ? String (hz / 1000.f) + "k" : String (roundToInt (hz));
    }

    /** A round step for an axis: 1, 2 or 5 times a power of ten, at least `raw`. */
    inline float niceStep (float raw)
    {
        const float p10 = std::pow (10.f, std::floor (std::log10 (jmax (1.0e-9f, raw))));
        const float m = raw / p10;
        return p10 * (m <= 1.f ? 1.f : m <= 2.f ? 2.f : m <= 5.f ? 5.f : 10.f);
    }
}

#endif // VIEWSTYLE_H_INCLUDED
