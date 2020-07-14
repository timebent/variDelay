/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Delay.h"


//==============================================================================
/**
*/
class VariDelayAudioProcessor  : public AudioProcessor,
                                 public ValueTree::Listener
{
public:
    //==============================================================================
    VariDelayAudioProcessor();
    ~VariDelayAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;

    //==============================================================================
    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    void changeProgramName (int index, const String& newName) override;

    //==============================================================================
    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    // Called when user changes parameters
    void update();
    
    // Store Parameters
    AudioProcessorValueTreeState apvts;
    AudioProcessorValueTreeState::ParameterLayout createParameters();

private:
    double seconds = 0;
    bool isActive { false };
    bool mustUpdateProcessing { false };
    
    float* rDelay, lDelay;
    
    enum
    {
        delayIndex
    };
    dsp::ProcessorChain<Delay<float>> fxChain;

    // Called when user changes a parameter
    void valueTreePropertyChanged (ValueTree& tree, const Identifier& property) override
    {
        
        mustUpdateProcessing = true;
    }
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VariDelayAudioProcessor)
};
