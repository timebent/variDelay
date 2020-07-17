
#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
VariDelayAudioProcessorEditor::VariDelayAudioProcessorEditor (VariDelayAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    
    setLookAndFeel(&delayFeel);
    
    delaySliderL = std::make_unique<Slider>(Slider::SliderStyle::RotaryVerticalDrag, Slider::TextBoxBelow);
    delaySliderL->setBounds(100, 100, 100, 100);
    addAndMakeVisible (delaySliderL.get());
    
    leftLabel = std::make_unique<Label>("", "Delay Left");
    addAndMakeVisible (leftLabel.get());
    leftLabel->attachToComponent (delaySliderL.get(), false);
    leftLabel->setJustificationType (Justification::centred);
    
    delaySliderR = std::make_unique<Slider>(Slider::SliderStyle::RotaryVerticalDrag, Slider::TextBoxBelow);
    delaySliderR->setBounds(300, 100, 100, 100);
    addAndMakeVisible (delaySliderR.get());
    
    rightLabel = std::make_unique<Label>("", "Delay Right");
    addAndMakeVisible (rightLabel.get());
    rightLabel->attachToComponent (delaySliderR.get(), false);
    rightLabel->setJustificationType (Justification::centred);
    
    feedbackSliderL = std::make_unique<Slider>(Slider::SliderStyle::RotaryVerticalDrag, Slider::TextBoxBelow);
    feedbackSliderL->setBounds(100, 250, 100, 100);
    addAndMakeVisible (feedbackSliderL.get());
    
    fbLabelL = std::make_unique<Label>("", "Feedback Left");
    addAndMakeVisible (fbLabelL.get());
    fbLabelL->attachToComponent (feedbackSliderL.get(), false);
    fbLabelL->setJustificationType (Justification::centred);
    
    feedbackSliderR = std::make_unique<Slider>(Slider::SliderStyle::RotaryVerticalDrag, Slider::TextBoxBelow);
    feedbackSliderR->setBounds(300, 250, 100, 100);
    addAndMakeVisible (feedbackSliderR.get());
    
    fbLabelR = std::make_unique<Label>("", "Feedback Right");
    addAndMakeVisible (fbLabelR.get());
    fbLabelR->attachToComponent (feedbackSliderR.get(), false);
    fbLabelR->setJustificationType (Justification::centred);
    
    wetSlider = std::make_unique<Slider>(Slider::SliderStyle::RotaryVerticalDrag, Slider::TextBoxBelow);
    wetSlider->setBounds(500, 250, 100, 100);
    addAndMakeVisible (wetSlider.get());
    
    wetLabel = std::make_unique<Label>("", "Wet Mix");
    addAndMakeVisible (wetLabel.get());
    wetLabel->attachToComponent (wetSlider.get(), false);
    wetLabel->setJustificationType (Justification::centred);
    
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    delayAttachmentL = std::make_unique<Attachment>(audioProcessor.apvts, "Time L", *delaySliderL);
    delayAttachmentR = std::make_unique<Attachment>(audioProcessor.apvts, "Time R", *delaySliderR);
    feedbackAttachmentL = std::make_unique<Attachment>(audioProcessor.apvts, "FB L", *feedbackSliderL);
    feedbackAttachmentR = std::make_unique<Attachment>(audioProcessor.apvts, "FB R", *feedbackSliderR);
    wetAttachment = std::make_unique<Attachment>(audioProcessor.apvts, "WET", *wetSlider);
   
    
    auto& lnf = getLookAndFeel();
    lnf.setColour (Slider::textBoxTextColourId, Colours::whitesmoke.darker());
    lnf.setColour (Slider::textBoxHighlightColourId, Colours::whitesmoke);
    lnf.setColour (Slider::textBoxOutlineColourId, Colours::transparentWhite);

    sendLookAndFeelChange();
    
    setSize (500, 500);
}

VariDelayAudioProcessorEditor::~VariDelayAudioProcessorEditor()
{
}

//==============================================================================
void VariDelayAudioProcessorEditor::paint (juce::Graphics& g)
{
    lookAndFeelChanged();
    auto black = juce::Colours::black;
    auto bounds = getLocalBounds().toFloat();
    Point<float> centre(bounds.getCentre().toFloat());
    /* dummy value because gradient is radial */
    Point<float> right(bounds.getTopRight().toFloat());
    
    g.setColour(black);
    juce::ColourGradient fillGradient(black.brighter(), centre, black, right, true);
    g.setGradientFill(fillGradient);
    g.fillAll();
}

void VariDelayAudioProcessorEditor::resized()
{


}
