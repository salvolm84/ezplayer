#include "Theme.h"

namespace theme
{
    juce::Colour pieceColour (tt::KitPiece p)
    {
        switch (p)
        {
            case tt::KitPiece::kick:       return juce::Colour (0xffff6b4a);
            case tt::KitPiece::snare:      return juce::Colour (0xffffb84a);
            case tt::KitPiece::sidestick:  return juce::Colour (0xffe0c27a);
            case tt::KitPiece::hihat:      return juce::Colour (0xff59d1c4);
            case tt::KitPiece::hihatPedal: return juce::Colour (0xff3e9b91);
            case tt::KitPiece::toms:       return juce::Colour (0xffb37bff);
            case tt::KitPiece::ride:       return juce::Colour (0xff5aa9ff);
            case tt::KitPiece::crash:      return juce::Colour (0xff9fd36b);
            case tt::KitPiece::other:
            case tt::KitPiece::count:      break;
        }
        return juce::Colour (0xffa0a4a8);
    }

    juce::Font font (float height, bool bold)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }
}

GPLookAndFeel::GPLookAndFeel()
{
    using namespace juce;
    setColour (ResizableWindow::backgroundColourId, theme::bg);
    setColour (TextButton::buttonColourId, theme::panelHi);
    setColour (TextButton::buttonOnColourId, theme::accentDim);
    setColour (TextButton::textColourOffId, theme::text);
    setColour (TextButton::textColourOnId, Colours::white);
    setColour (ComboBox::backgroundColourId, theme::panelHi);
    setColour (ComboBox::outlineColourId, theme::outline);
    setColour (ComboBox::textColourId, theme::text);
    setColour (ComboBox::arrowColourId, theme::dim);
    setColour (PopupMenu::backgroundColourId, theme::panel);
    setColour (PopupMenu::highlightedBackgroundColourId, theme::selection);
    setColour (PopupMenu::textColourId, theme::text);
    setColour (TextEditor::backgroundColourId, theme::row);
    setColour (TextEditor::outlineColourId, theme::outline);
    setColour (TextEditor::focusedOutlineColourId, theme::accent);
    setColour (TextEditor::textColourId, theme::text);
    setColour (TextEditor::highlightColourId, theme::selection);
    setColour (CaretComponent::caretColourId, theme::accent);
    setColour (Label::textColourId, theme::text);
    setColour (ListBox::backgroundColourId, theme::row);
    setColour (ListBox::outlineColourId, theme::outline);
    setColour (TreeView::backgroundColourId, theme::panel);
    setColour (TreeView::selectedItemBackgroundColourId, theme::selection);
    setColour (TreeView::linesColourId, theme::outline);
    setColour (ScrollBar::thumbColourId, theme::outline.brighter (0.2f));
    setColour (TableHeaderComponent::backgroundColourId, theme::panelHi);
    setColour (TableHeaderComponent::textColourId, theme::dim);
    setColour (TableHeaderComponent::outlineColourId, theme::outline);
    setColour (ToggleButton::textColourId, theme::text);
    setColour (ToggleButton::tickColourId, theme::accent);
    setColour (ToggleButton::tickDisabledColourId, theme::dim);
    setColour (Slider::thumbColourId, theme::accent);
    setColour (Slider::trackColourId, theme::accentDim);
    setColour (Slider::backgroundColourId, theme::row);
    setColour (Slider::textBoxTextColourId, theme::text);
    setColour (Slider::textBoxBackgroundColourId, theme::row);
    setColour (Slider::textBoxOutlineColourId, theme::outline);
    setColour (AlertWindow::backgroundColourId, theme::panel);
    setColour (AlertWindow::textColourId, theme::text);
    setColour (AlertWindow::outlineColourId, theme::outline);
    setColour (TooltipWindow::backgroundColourId, theme::panelHi);
    setColour (TooltipWindow::textColourId, theme::text);
    setColour (TooltipWindow::outlineColourId, theme::outline);
}

void GPLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto on = b.getToggleState();
    auto c = on ? theme::accentDim : theme::panelHi;

    if (! b.isEnabled())       c = c.withMultipliedAlpha (0.5f);
    else if (down)             c = c.brighter (0.25f);
    else if (highlighted)      c = c.brighter (0.1f);

    g.setColour (c);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (on ? theme::accent : theme::outline);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

void GPLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
                     .withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

juce::Font GPLookAndFeel::getTextButtonFont (juce::TextButton&, int h)
{
    return theme::font (juce::jmin (14.0f, h * 0.55f));
}

void GPLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (theme::panelHi.withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.5f));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (box.hasKeyboardFocus (true) ? theme::accent : theme::outline);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);

    juce::Path p;
    auto ax = (float) w - 12.0f, ay = h * 0.5f;
    p.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (theme::dim);
    g.fillPath (p);
}

juce::Font GPLookAndFeel::getComboBoxFont (juce::ComboBox& b) { return theme::font (juce::jmin (14.0f, b.getHeight() * 0.55f)); }
juce::Font GPLookAndFeel::getPopupMenuFont() { return theme::font (14.0f); }

void GPLookAndFeel::drawTableHeaderBackground (juce::Graphics& g, juce::TableHeaderComponent& h)
{
    g.fillAll (theme::panelHi);
    g.setColour (theme::outline);
    g.fillRect (0, h.getHeight() - 1, h.getWidth(), 1);
}

void GPLookAndFeel::drawTableHeaderColumn (juce::Graphics& g, juce::TableHeaderComponent&, const juce::String& name, int,
                                           int width, int height, bool over, bool, int flags)
{
    if (over)
        g.fillAll (theme::panelHi.brighter (0.06f));

    auto area = juce::Rectangle<int> (width, height).reduced (6, 0);
    if ((flags & (juce::TableHeaderComponent::sortedForwards | juce::TableHeaderComponent::sortedBackwards)) != 0)
    {
        juce::Path p;
        auto up = (flags & juce::TableHeaderComponent::sortedForwards) != 0;
        auto ax = (float) area.getRight() - 4.0f, ay = height * 0.5f;
        if (up) p.addTriangle (ax - 4, ay + 2, ax + 4, ay + 2, ax, ay - 3);
        else    p.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour (theme::accent);
        g.fillPath (p);
        area.removeFromRight (12);
    }

    g.setColour (theme::dim);
    g.setFont (theme::font (12.5f, true));
    g.drawFittedText (name, area, juce::Justification::centredLeft, 1);
    g.setColour (theme::outline);
    g.fillRect (width - 1, 4, 1, height - 8);
}

void GPLookAndFeel::drawTickBox (juce::Graphics& g, juce::Component&, float x, float y, float w, float h,
                                 bool ticked, bool enabled, bool, bool)
{
    auto r = juce::Rectangle<float> (x, y, w, h).reduced (1.0f);
    g.setColour (theme::row);
    g.fillRoundedRectangle (r, 2.5f);
    g.setColour (ticked ? theme::accent : theme::outline);
    g.drawRoundedRectangle (r, 2.5f, 1.0f);

    if (ticked)
    {
        g.setColour (theme::accent.withMultipliedAlpha (enabled ? 1.0f : 0.5f));
        g.fillRoundedRectangle (r.reduced (3.0f), 1.5f);
    }
}

void GPLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                      juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }

    auto track = juce::Rectangle<float> ((float) x, y + h * 0.5f - 2.0f, (float) w, 4.0f);
    g.setColour (theme::row);
    g.fillRoundedRectangle (track, 2.0f);
    g.setColour (theme::accentDim);
    g.fillRoundedRectangle (track.withRight (pos), 2.0f);
    g.setColour (theme::accent);
    g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ pos, y + h * 0.5f }));
}

void GPLookAndFeel::drawTreeviewPlusMinusBox (juce::Graphics& g, const juce::Rectangle<float>& area, juce::Colour,
                                              bool isOpen, bool isMouseOver)
{
    juce::Path p;
    auto c = area.getCentre();
    if (isOpen) p.addTriangle (c.x - 4, c.y - 2, c.x + 4, c.y - 2, c.x, c.y + 3);
    else        p.addTriangle (c.x - 2, c.y - 4, c.x - 2, c.y + 4, c.x + 3, c.y);
    g.setColour (isMouseOver ? theme::text : theme::dim);
    g.fillPath (p);
}
