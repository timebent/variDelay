/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>



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
    
    void writeToDelayBuffer (AudioBuffer<float>& buffer,
                             const int channelIn, const int channelOut,
                             const int writePos, float startGain, float endGain,
                             bool replacing);
    
    void readFromDelayBuffer (AudioBuffer<float>& buffer,
                              const int channelIn, const int channelOut,
                              const int readPos, float startGain, float endGain,
                              bool replacing);

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

    static String paramGain;
    static String paramTime;
    static String paramFeedback;
    
private:
    double seconds = 0;
    bool isActive { false };
    bool mustUpdateProcessing { false };
    
    float* rDelay, lDelay;
    
    
    Atomic<float>   mGain     {   0.0f };
    Atomic<float>   mTime     { 200.0f };
    Atomic<float>   mFeedback {  -6.0f };
    
    AudioBuffer<float>&     mDelayBuffer;
    
    float mLastInputGain    = 0.0f;
    float mLastFeedbackGain = 0.0f;
    
    int    mWritePos        = 0;
    int    mExpectedReadPos = -1;
    double mSampleRate      = 0;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VariDelayAudioProcessor)
};
