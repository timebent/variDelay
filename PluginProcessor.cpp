

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
VariDelayAudioProcessor::VariDelayAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), apvts (*this, nullptr, "Parameters", createParameters()), mDelayBuffer(2, 96000)
#endif
{
    apvts.state.addListener (this);
}

VariDelayAudioProcessor::~VariDelayAudioProcessor()
{
}

//==============================================================================
const juce::String VariDelayAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool VariDelayAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool VariDelayAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool VariDelayAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double VariDelayAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int VariDelayAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int VariDelayAudioProcessor::getCurrentProgram()
{
    return 0;
}

void VariDelayAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String VariDelayAudioProcessor::getProgramName (int index)
{
    return {};
}

void VariDelayAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void VariDelayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    mSampleRate = sampleRate;
    mSampleRateL = sampleRate;
    mSampleRateR = sampleRate;
    update();
    // sample buffer for 2 seconds + 2 buffers safety
    mDelayBuffer.setSize (getTotalNumOutputChannels(), 2.0 * (samplesPerBlock + sampleRate), false, false);
    mDelayBuffer.clear();
    
    mExpectedReadPosL = -1;
    mExpectedReadPosR = -1;
}



void VariDelayAudioProcessor::releaseResources()
{

}

#ifndef JucePlugin_PreferredChannelConfigurations
bool VariDelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else

    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void VariDelayAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    if (mustUpdateProcessing)
        update();
    
    juce::ScopedNoDenormals noDenormals;
    auto inputChannels  = getTotalNumInputChannels();
    auto outputChannels = getTotalNumOutputChannels();
    
    const float gain = Decibels::decibelsToGain (mGain.get());
    const float feedbackL = Decibels::decibelsToGain (feedbackLevelL.get());
    const float feedbackR = Decibels::decibelsToGain (feedbackLevelR.get());
    /* mono input currently, create if statement later to accomplish stereo input */
    
    // adapt dry gain
    buffer.applyGainRamp (0, buffer.getNumSamples(), mLastInputGain, gain);
    mLastInputGain = gain;
    
    
    /*===============================================================*/
    /*--------------------- LEFT CHANNEL ----------------------------*/
    /*===============================================================*/
    if (Bus* input = getBus (true, 0))
    {
        
        const float timeL = delayL.get();
        /*
            write dry sig from left channel of dryBuffer to left channel of delayBuffer
        */
        const int leftChan = input->getChannelIndexInProcessBlockBuffer (0);

        /* true means we are replacing rather than adding, so whole buffer is copied directly to delayBuffer */
        writeToDelayBuffer(buffer, leftChan, 0, mWritePos, 1.0f, 1.0f, true);
        /*-------------------------------------------------------*/
        /*
         readPos is an index of the delayLine, set back in position by the delay time (ms)
            mWritePos starts at 0, incremented by buffer size in samples every process loop,
            wrapped around the size of the delay buffer
        */
        /*-------------------------------------------------------*/
        auto readPosL = roundToInt (mWritePos - (mSampleRateL * timeL / 1000.0));
        if (readPosL < 0)
            readPosL += mDelayBuffer.getNumSamples(); // wraps readPos around mDelayBuffer size

        /*-------------------------------------------------------*/
        if (Bus* outputBusL = getBus (false, 0))
        {
            /*-------------------------------------------------------*/
            /*
                skipped the first time, sends you to teh next if() statement
            */
                if (mExpectedReadPosL >= 0 )
                {
                /*
                    fade out if readPos is off,  (does this mean if we changed the delayTime?)
                    always false the first time through
                */
                    auto endGainL = (readPosL == mExpectedReadPosL) ? 1.0f : 0.0f;
                    /* ensures correct blockBuffer */
                    const int outputChannelNum = outputBusL->getChannelIndexInProcessBlockBuffer (0);
                    // (buffer, channel in, channel out, readPos, start, end, replacing)
                    readFromDelayBuffer (buffer, 0, outputChannelNum, mExpectedReadPosL, 1.0, endGainL, false);
                    
                }
                
                /*-------------------------------------------------------*/
                /*
                    runs the first time, because default expected is -1 ( less than zero )
                     ades in from 0.0 to 1.0 using addFromWithRamp
                */
                if (readPosL != mExpectedReadPosL)
                {
                    const int outputChannelNum = outputBusL->getChannelIndexInProcessBlockBuffer (0);
                    readFromDelayBuffer (buffer, 0, outputChannelNum, readPosL, 0.0, 1.0, false);
                }
            }
        // add feedback to delay
        const int fbChanL = input->getChannelIndexInProcessBlockBuffer (0);
        writeToDelayBuffer (buffer, fbChanL, 0, mWritePos, mLastFeedbackGainL, feedbackL, false);
        
        mExpectedReadPosL = readPosL + buffer.getNumSamples();
        if (mExpectedReadPosL >= mDelayBuffer.getNumSamples())
            mExpectedReadPosL -= mDelayBuffer.getNumSamples();
        
    
    
    /*===============================================================*/
    /*--------------------- RIGHT CHANNEL ---------------------------*/
    /*===============================================================*/
        const float timeR = delayR.get();
            
        const int rightChan = input->getChannelIndexInProcessBlockBuffer (1);
            
        /* true means we are replacing rather than adding, so whole buffer is copied directly to delayBuffer */
        writeToDelayBuffer(buffer, rightChan, 1, mWritePos, 1.0f, 1.0f, true);
            
            
        auto readPosR = roundToInt (mWritePos - (mSampleRateR * timeR / 1000.0));
        if (readPosR < 0)
            readPosR += mDelayBuffer.getNumSamples(); // wraps readPos around mDelayBuffer size
            
        if (Bus* outputR = getBus (false, 0))
        {
            /*-------------------------------------------------------*/
            /*
                skipped the first time, sends you to teh next if() statement
            */
            if (mExpectedReadPosR >= 0 )
            {
                /*
                    fade out if readPos is off,  (does this mean if we changed the delayTime?)
                    always false the first time through
                */
                auto endGainR = (readPosR == mExpectedReadPosR) ? 1.0f : 0.0f;
                /* ensures correct blockBuffer */
                const int outputChannelNum = outputR->getChannelIndexInProcessBlockBuffer (1);
               
                // (buffer, channel in, channel out, readPos, start, end, replacing)
                readFromDelayBuffer (buffer, 1, outputChannelNum, mExpectedReadPosR, 1.0, endGainR, false);
                    
            }
                
            /*-------------------------------------------------------*/
            /*
                runs the first time, because default expected is -1 ( less than zero )
                ades in from 0.0 to 1.0 using addFromWithRamp
            */
            if (readPosR != mExpectedReadPosR)
            {
                const int outputChannelNum = outputR->getChannelIndexInProcessBlockBuffer (1);
                
                readFromDelayBuffer (buffer, 1, outputChannelNum, readPosR, 0.0, 1.0, false);
            }
        }
        // add feedback to delay
        const int fbChanR = input->getChannelIndexInProcessBlockBuffer (1);
        writeToDelayBuffer (buffer, fbChanR, 1, mWritePos, mLastFeedbackGainR, feedbackR, false);
        
            
        mExpectedReadPosR = readPosR + buffer.getNumSamples();
        if (mExpectedReadPosR >= mDelayBuffer.getNumSamples())
        mExpectedReadPosR -= mDelayBuffer.getNumSamples();
    
    
    }
    
    mLastFeedbackGainL = feedbackL;
    mLastFeedbackGainR = feedbackR;
    
    // advance positions
    mWritePos += buffer.getNumSamples();
    if (mWritePos >= mDelayBuffer.getNumSamples())
        mWritePos -= mDelayBuffer.getNumSamples();
    
    
    
}
/** Copies samples from an array of floats into one of the channels, applying a gain ramp.
 
 @param destChannel          the channel within this buffer to copy the samples to
 @param destStartSample      the start sample within this buffer's channel
 @param source               the source buffer to read from
 @param numSamples           the number of samples to process
 @param startGain            the gain to apply to the first sample (this is multiplied with
 the source samples before they are copied to this buffer)
 @param endGain              the gain to apply to the final sample. The gain is linearly
 interpolated between the first and last samples.
 
 @see addFrom
 */
void VariDelayAudioProcessor::writeToDelayBuffer (AudioBuffer<float>& buffer,
                                                  const int channelIn, const int channelOut,
                                                  const int writePos, float startGain, float endGain, bool replacing)
{
    /*-------------------------------------------------------*/
    if (writePos + buffer.getNumSamples() <= mDelayBuffer.getNumSamples())
    {
        if (replacing)
            mDelayBuffer.copyFromWithRamp (channelOut, writePos, buffer.getReadPointer (channelIn), buffer.getNumSamples(), startGain, endGain);
        else
            mDelayBuffer.addFromWithRamp (channelOut, writePos, buffer.getReadPointer (channelIn), buffer.getNumSamples(), startGain, endGain);
    }
    /*-------------------------------------------------------*/
    
    
    else
    {
        const auto midPos  = mDelayBuffer.getNumSamples() - writePos;
        const auto midGain = jmap (float (midPos) / buffer.getNumSamples(), startGain, endGain);
        if (replacing)
        {
            mDelayBuffer.copyFromWithRamp (channelOut, writePos, buffer.getReadPointer (channelIn), midPos, startGain, midGain);
            mDelayBuffer.copyFromWithRamp (channelOut, 0, buffer.getReadPointer (channelIn, midPos), buffer.getNumSamples() - midPos, midGain, endGain);
        }
        else
        {
            mDelayBuffer.addFromWithRamp (channelOut, writePos, buffer.getReadPointer (channelIn), midPos, mLastInputGain, midGain);
            mDelayBuffer.addFromWithRamp (channelOut, 0, buffer.getReadPointer (channelIn, midPos), buffer.getNumSamples() - midPos, midGain, endGain);
        }
    }
}


/*
 This is used by the output of the process block to get the samples out of the delayBuffer
 the if/else statement is a function of the wrapper
 We pass the AudioSampleBuffer& (weird type?) because we are calling buffer.copyFromWithRamp() and buffer.addFromWithRamp()
 which actually outputs this
 endGain might be 0.0 if we adjust the delayTime (i believe)
 
 'replacing' triggers the 'else' statements, for when the delay time has changed
 
 iteration through each delay buffer is done around the function call in processBlock()
 */
void VariDelayAudioProcessor::readFromDelayBuffer (AudioSampleBuffer& buffer,
                                                   const int channelIn, const int channelOut,
                                                   const int readPos,
                                                   float startGain, float endGain,
                                                   bool replacing)
{
    /*-------------------------------------------------------*/
    /*
     this is run when the readPos is equal to mExpectedReadPos, so when the delay hasn't changed
     */
    if (readPos + buffer.getNumSamples() <= mDelayBuffer.getNumSamples())
    {
        
        if (replacing) // (channel, startSample, source*, length, gain, gain)
            buffer.copyFromWithRamp (channelOut, 0, mDelayBuffer.getReadPointer (channelIn, readPos), buffer.getNumSamples(), startGain, endGain);
        else
            buffer.addFromWithRamp (channelOut, 0, mDelayBuffer.getReadPointer (channelIn, readPos), buffer.getNumSamples(), startGain, endGain);
    }
    /*-------------------------------------------------------*/
    /*
     triggered when the buffer(output) length would read past the length of the current delayBuffer
     */
    else
    {
        // the length of the audio buffer would overlap the end of the delay buffer, so we have to create the 'midPos'
        const auto midPos  = mDelayBuffer.getNumSamples() - readPos;
        /*
         nothing if endGain = 1.0, but if 0.0 this will schedule the fade out to take as long as we have until the next buffer.
         targetRangeMin + value0To1 * (targetRangeMax - targetRangeMin); startGain is min, endGain is max.
         1.0 + 0.6 * (0.0 - 1.0) = 0.4.
         midGain -> what gain is at midPos as we approach 0
         */
        const auto midGain = jmap (float (midPos) / buffer.getNumSamples(), startGain, endGain);
        if (replacing)
        {
            /*
             go from startGain (1.0) to midGain over the length of midPos,
             starting at sample 0 of buffer and readPos of mDelayBuffer
             then we go from midGain to 0.0 over the remaining samples after midGain
             */
            buffer.copyFromWithRamp (channelOut, 0, mDelayBuffer.getReadPointer (channelIn, readPos), midPos, startGain, midGain);
            buffer.copyFromWithRamp (channelOut, midPos, mDelayBuffer.getReadPointer (channelIn), buffer.getNumSamples() - midPos, midGain, endGain);
        }
        else
        {
            buffer.addFromWithRamp (channelOut, 0, mDelayBuffer.getReadPointer (channelIn, readPos), midPos, startGain, midGain);
            buffer.addFromWithRamp (channelOut, midPos, mDelayBuffer.getReadPointer (channelIn), buffer.getNumSamples() - midPos, midGain, endGain);
        }
    }
}

//==============================================================================
bool VariDelayAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* VariDelayAudioProcessor::createEditor()
{
    return new VariDelayAudioProcessorEditor (*this);
}

//==============================================================================
void VariDelayAudioProcessor::getStateInformation (MemoryBlock& destData)
{

}

void VariDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
   
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VariDelayAudioProcessor();
}

void VariDelayAudioProcessor::update()
{
    auto currentL = delayL;
    auto currentR = delayR;
    
    mustUpdateProcessing = false;
    auto lTime = apvts.getRawParameterValue ("Time L");
    auto rTime = apvts.getRawParameterValue ("Time R");
    auto feedbackL = apvts.getRawParameterValue("FB L");
    auto feedbackR = apvts.getRawParameterValue("FB R");
    auto wetLevel = apvts.getRawParameterValue("WET");
    
    using mult = juce::ValueSmoothingTypes::Multiplicative;
    using lin = juce::ValueSmoothingTypes::Linear;
    
    juce::SmoothedValue<float, lin> lDelay;
    lDelay.setTargetValue(*lTime);
    
    juce::SmoothedValue<float, lin> rDelay;
    rDelay.setTargetValue(*rTime);
    
    juce::SmoothedValue<float, lin> mFeedbackL;
    mFeedbackL.setTargetValue(*feedbackL);
    
    juce::SmoothedValue<float, lin> mFeedbackR;
    mFeedbackR.setTargetValue(*feedbackR);
    
    juce::SmoothedValue<float, lin> mWet;
    mWet.setTargetValue(*wetLevel);
    
    auto newDelayL = lDelay.getNextValue();
    auto newDelayR = rDelay.getNextValue();
    
    auto changePercentL = *lTime / currentL;
    auto changePercentR = *rTime / currentR;
    
    delayL = lDelay.getNextValue();
    delayR = rDelay.getNextValue();
    feedbackLevelL = mFeedbackL.getNextValue();
    feedbackLevelR = mFeedbackR.getNextValue();
   // wetLevel = mWet.getNextValue();
    
    
    
 

}

juce::AudioProcessorValueTreeState::ParameterLayout VariDelayAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    
    parameters.push_back (std::make_unique<AudioParameterFloat>("Time L", "Delay Time L", NormalisableRange<float> (0.0f, 2000.f, 1.f, 1.0f), delayL.get()));
    parameters.push_back (std::make_unique<AudioParameterFloat>("Time R", "Delay Time R", NormalisableRange<float> (0.0f, 2000.f, 1.f, 1.0f), delayR.get()));
    parameters.push_back (std::make_unique<AudioParameterFloat>("FB L", "Feedback L",
              NormalisableRange<float> (-100.0f, 6.0f, 0.1f, std::log (0.5f) / std::log (100.0f / 106.0f)), feedbackLevelL.get()));
    parameters.push_back (std::make_unique<AudioParameterFloat>("FB R", "Feedback R",
              NormalisableRange<float> (-100.0f, 6.0f, 0.1f, std::log (0.5f) / std::log (100.0f / 106.0f)), feedbackLevelR.get()));
                          
    parameters.push_back (std::make_unique<AudioParameterFloat>("WET", "Wet Level", NormalisableRange<float> (0.0f, 1.0f, 0.01f, 1.0f), 0.2f));
                          
    return { parameters.begin(), parameters.end() };
}
