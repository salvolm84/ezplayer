#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Core/DrumMap.h"

namespace theme
{
    const juce::Colour bg        { 0xff141619 };
    const juce::Colour panel     { 0xff1d2024 };
    const juce::Colour panelHi   { 0xff262a2f };
    const juce::Colour row       { 0xff1a1d21 };
    const juce::Colour rowAlt    { 0xff1f2226 };
    const juce::Colour outline   { 0xff33383e };
    const juce::Colour text      { 0xffe6e7e8 };
    const juce::Colour dim       { 0xff8c939b };
    const juce::Colour heading   { 0xffc9a36a };
    const juce::Colour accent    { 0xffff8a1d };
    const juce::Colour accentDim { 0xff8a4d14 };
    const juce::Colour selection { 0xff3a5f88 };
    const juce::Colour clip      { 0xff2f4a66 };
    const juce::Colour clipSel   { 0xff4c7cad };
    const juce::Colour playhead  { 0xffffd34d };

    juce::Colour pieceColour (tt::KitPiece);

    juce::Font font (float height, bool bold = false);
}

class GPLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GPLookAndFeel();

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawTableHeaderColumn (juce::Graphics&, juce::TableHeaderComponent&, const juce::String& name, int columnId,
                                int width, int height, bool isMouseOver, bool isMouseDown, int columnFlags) override;
    void drawTableHeaderBackground (juce::Graphics&, juce::TableHeaderComponent&) override;
    void drawTickBox (juce::Graphics&, juce::Component&, float x, float y, float w, float h,
                      bool ticked, bool enabled, bool highlighted, bool down) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawTreeviewPlusMinusBox (juce::Graphics&, const juce::Rectangle<float>& area, juce::Colour backgroundColour,
                                   bool isOpen, bool isMouseOver) override;
};
