/*
  ==============================================================================

    LookAndFeel.cpp
    Created: 12 Jul 2020 8:49:21am
    Author:  Billie (Govnah) Jean

  ==============================================================================
*/

#include "LookAndFeel.h"

DelayFeel::DelayFeel()
{
    
    setColour(juce::Slider::rotarySliderFillColourId, Colours::aquamarine);
    
    
    
    //setColour(juce::Label::backgroundColourId, Colours::white);
}

DelayFeel::~DelayFeel()
{
    
}
//Label* DelayFeel::createSliderTextBox(Slider& slider)
//{
//    //Range<int> range(0, 100);
//    
//   // setColour(range, slider.findColour(Slider::textBoxOutlineColourId));
//}


/*=================================================================================*/
/*----------------------------- drawRotarySlider ----------------------------------*/
/*=================================================================================*/
void DelayFeel::drawRotarySlider (Graphics& g, int x, int y, int width, int height,
                                     float sliderPos, float rotaryStartAngle, float rotaryEndAngle, Slider& slider)
{
    auto radius = jmin (width / 2, height / 2) - 2.0f;
    auto centreX = x + width  * 0.5f;
    auto centreY = y + height * 0.5f;
    auto rx = centreX - radius;
    auto ry = centreY - radius;
    auto rw = radius * 2.0f;
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    auto isMouseOver = slider.isMouseOverOrDragging() && slider.isEnabled();
    
    
    //g.setColour(slider.findColour(Slider::textBoxOutlineColourId));
    
    if (slider.isEnabled())
        g.setColour (slider.findColour (Slider::rotarySliderFillColourId).withAlpha (isMouseOver ? 1.0f : 0.7f));
    else
        g.setColour (Colour (0x80808080));
    
    {
        Path filledArc;
        filledArc.addPieSegment (rx, ry, rw, rw, rotaryStartAngle, angle, 0.0);
        g.fillPath (filledArc);
    }
    
    {
        auto lineThickness = jmin (15.0f, jmin (width, height) * 0.45f) * 0.1f;
        Path outlineArc;
        outlineArc.addPieSegment (rx, ry, rw, rw, rotaryStartAngle, rotaryEndAngle, 0.0);
        g.strokePath (outlineArc, PathStrokeType (lineThickness));
    }
    
   
}

Label* DelayFeel::createSliderTextBox(Slider& slider)
{
    Label* l = LookAndFeel_V4::createSliderTextBox(slider);
    
    // make sure editor text is black (so it shows on white background)
    //l->setColour(juce::Label::outlineColourId, Colours::white.withAlpha(0.0f));
    l->setColour(juce::Slider::textBoxBackgroundColourId, Colours::red);
    
    return l;
}

/*=================================== FONT =========================================*/

Font DelayFeel::getLabelFont(Label& label)
{
    return getFont();
}





