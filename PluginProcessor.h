/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <rubberband/RubberBandStretcher.h>


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
    std::unique_ptr<RubberBand::RubberBandStretcher> stretchL;
    AudioBuffer<float>      stretchBufferL;
    std::unique_ptr<RubberBand::RubberBandStretcher> stretchR;
    AudioBuffer<float>      stretchBufferR;
    
    double seconds;
    bool isActive { false };
    bool mustUpdateProcessing { false };
    
    //float* rDelay, lDelay;
    
    
    Atomic<float>   mGain           {   0.0f };
    Atomic<float>   delayL          { 200.0f };
    Atomic<float>   delayR          { 200.0f };
    Atomic<float>   feedbackLevelL   {  -6.0f };
    Atomic<float>   feedbackLevelR   {  -6.0f };
    Atomic<float>   wetLevel        {   50.0f };
    
    AudioBuffer<float>     mDelayBuffer;
    
    float mLastInputGain    = 0.0f;
    float mLastFeedbackGainL = 0.0f;
    float mLastFeedbackGainR = 0.0f;
    
    int    mWritePos        = 0;
    int    mExpectedReadPosL = -1;
    int    mExpectedReadPosR = -1;
    double mSampleRateL      = 0;
    double mSampleRateR      = 0;
    double mSampleRate;
    
    void valueTreePropertyChanged(ValueTree& tree, const Identifier& property) override
    {
        mustUpdateProcessing = true;
    }
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VariDelayAudioProcessor)
};
