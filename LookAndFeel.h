/*
  ==============================================================================

    LookAndFeel.h
    Created: 12 Jul 2020 8:49:21am
    Author:  Billie (Govnah) Jean

  ==============================================================================
*/

#pragma once
#include "../JuceLibraryCode/JuceHeader.h"

class  DelayFeel : public juce::LookAndFeel_V4
{
public:
    DelayFeel();
    ~DelayFeel();
    
    void drawRotarySlider (Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle, Slider&) override;
    
    //Label* createSliderTextBox(Slider&) override;
    Label* createSliderTextBox(Slider&) override;
    Font getLabelFont(Label&) override;
    
    
private:
    Font getFont()
    {
        return Font ("Avenir Next Ultra Light", "Regular", 23.f);
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayFeel)
};

