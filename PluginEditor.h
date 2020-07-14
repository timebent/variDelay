/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "LookAndFeel.h"

//==============================================================================
/**
*/
class VariDelayAudioProcessorEditor  : public AudioProcessorEditor
{
public:
    VariDelayAudioProcessorEditor (VariDelayAudioProcessor&);
    ~VariDelayAudioProcessorEditor() override;

    //==============================================================================
    void paint (Graphics&) override;
    void resized() override;

private:
    DelayFeel delayFeel;
    
    std::unique_ptr<juce::Slider> delaySliderL;
    std::unique_ptr<juce::Slider> delaySliderR;
    std::unique_ptr<juce::Slider> feedbackSlider;
    std::unique_ptr<juce::Slider> wetSlider;
    
    // Labels
    std::unique_ptr<Label> leftLabel;
    std::unique_ptr<Label> rightLabel;
    std::unique_ptr<Label> feedbackLabel;
    std::unique_ptr<Label> wetLabel;
    
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> delayAttachmentL;
    std::unique_ptr<Attachment> delayAttachmentR;
    std::unique_ptr<Attachment> feedbackAttachment;
    std::unique_ptr<Attachment> wetAttachment;
    
    VariDelayAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VariDelayAudioProcessorEditor)
};
