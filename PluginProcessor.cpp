

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
    /* init buffers, delay buffer in constructor */
    stretchBufferL.setSize(1, samplesPerBlock);
    stretchBufferR.setSize(1, samplesPerBlock);
    
    /* init instances of RubberBand */
    stretchL = std::make_unique<RubberBand::RubberBandStretcher>(sampleRate, 1, RubberBand::RubberBandStretcher::Option::OptionProcessRealTime, 1.0f, 0.8f);
    stretchR = std::make_unique<RubberBand::RubberBandStretcher>(sampleRate, 1,RubberBand::RubberBandStretcher::Option::OptionProcessRealTime, 1.0f, 0.8f);
    
    /* vestigual structure I'm afraid to remove */
    mSampleRate = sampleRate;
    mSampleRateL = sampleRate;
    mSampleRateR = sampleRate;
    
    update();
    
    // sample buffer for 2 seconds + 2 buffers safety
    mDelayBuffer.setSize (getTotalNumOutputChannels(), 2.0 * (samplesPerBlock + sampleRate), false, false);
    
    
    /* used to trigger a certain if statement in readFromDelayBuffer() that cross fades between the two delay settings*/
    mExpectedReadPosL = -1;
    mExpectedReadPosR = -1;
    
    /* clear garbage */
    stretchL->reset();
    stretchR->reset();
    mDelayBuffer.clear();
    DBG("prepare");
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
    /* for GUI */
    if (mustUpdateProcessing)
        update();
    
    juce::ScopedNoDenormals noDenormals;
   
    
    const float gain = Decibels::decibelsToGain (mGain.get());
    const float feedbackL = Decibels::decibelsToGain (feedbackLevelL.get());
    const float feedbackR = Decibels::decibelsToGain (feedbackLevelR.get());
    
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
        /* seems like channel 0 means left and channel 1 means right? Not the case. */
        if (Bus* outputBusL = getBus (false, 0))
        {
            /*-------------------------------------------------------*/
            /*
                skipped the first time, sends you to the next if() statement
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
    /* basically same as above, just checkout the output part */
        
        const float timeR = delayR.get();
            
        const int rightChan = input->getChannelIndexInProcessBlockBuffer (1);
            
        /* true means we are replacing rather than adding, so whole buffer is copied directly to delayBuffer */
        writeToDelayBuffer(buffer, rightChan, 1, mWritePos, 1.0f, 1.0f, true);
            
            
        auto readPosR = roundToInt (mWritePos - (mSampleRateR * timeR / 1000.0));
        if (readPosR < 0)
            readPosR += mDelayBuffer.getNumSamples(); // wraps readPos around mDelayBuffer size
        
        /*-----------------------------------------------------------*/
        /*
            this runs when set to 0 and still produces a different delay line
            something horrific happens if you put 1 here.  Something like
         
        */
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
/*========================================================================================*/
/*================================= WRITE TO DELAY =======================================*/
/*
    Writes to the circular buffer.  The bool is used to determine whether or not to
    copyFrom or addFrom.  'midPos' is used to wrap around the circular buffer if the
    audioBuffer is too big.
*/
void VariDelayAudioProcessor::writeToDelayBuffer (AudioBuffer<float>& buffer, const int channelIn, const int channelOut,
                                                  const int writePos, float startGain, float endGain, bool replacing)
{
    
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


/*=========================================================================================*/
/*================================= READ FROM DELAY =======================================*/
void VariDelayAudioProcessor::readFromDelayBuffer (AudioSampleBuffer& buffer, const int channelIn, const int channelOut, const int readPos, float startGain, float endGain, bool replacing)
{
   
    
    auto readPointersL = stretchBufferL.getArrayOfReadPointers();
    auto readPointersR = stretchBufferR.getArrayOfReadPointers();
    auto samplesAvailableL = stretchL->available();
    auto samplesAvailableR = stretchR->available();
    
    auto outputSamples = buffer.getNumSamples();
    auto writePointers = buffer.getArrayOfWritePointers();
    

    if (readPos + buffer.getNumSamples() <= mDelayBuffer.getNumSamples())
    {
        
        if (replacing) // (channel, startSample, source*, length, gain, gain)
        {
            
            if (channelOut == 0)
            {
                stretchBufferL.copyFromWithRamp (channelOut, 0, mDelayBuffer.getReadPointer (channelIn, readPos), stretchBufferL.getNumSamples(), startGain, endGain);
                
                while (samplesAvailableL < outputSamples)
                {
                    stretchL->process(readPointersL, stretchBufferL.getNumSamples(), false); //fills mTempBuffer
                    samplesAvailableL = stretchL->available();
                }
            } else
            {
                stretchBufferR.copyFromWithRamp (0, 0, mDelayBuffer.getReadPointer (channelIn, readPos), stretchBufferR.getNumSamples(), startGain, endGain);
                
                while (samplesAvailableR < outputSamples)
                {
                    stretchR->process(readPointersR, stretchBufferR.getNumSamples(), false); //fills mTempBuffer
                    samplesAvailableR = stretchR->available();
                }
            }
        }
        else
        {
            if (channelOut == 0)
            {
                stretchBufferL.addFromWithRamp (channelOut, 0, mDelayBuffer.getReadPointer (channelIn, readPos), stretchBufferL.getNumSamples(), startGain, endGain);
                
                while (samplesAvailableL < outputSamples)
                {
                    stretchL->process(readPointersL, stretchBufferL.getNumSamples(), false); //fills mTempBuffer
                    samplesAvailableL = stretchL->available();
                }
            } else
            { // have to add to channel 0 because I made two buffers and a single channel buffer only has one channel (0)
                stretchBufferR.addFromWithRamp (0, 0, mDelayBuffer.getReadPointer (channelIn, readPos), stretchBufferR.getNumSamples(), startGain, endGain);
                
                while (samplesAvailableR < outputSamples)
                {
                    stretchR->process(readPointersR, stretchBufferR.getNumSamples(), false); //fills mTempBuffer
                    samplesAvailableR = stretchR->available();
                }
            }
        }
    }
    else
    {
        const auto midPos  = mDelayBuffer.getNumSamples() - readPos;
        const auto midGain = jmap (float (midPos) / buffer.getNumSamples(), startGain, endGain);
        if (replacing)
        {
            if (channelOut == 0)
            {
                stretchBufferL.copyFromWithRamp (channelOut, 0, mDelayBuffer.getReadPointer (channelIn, readPos), midPos, startGain, midGain);
                stretchBufferL.copyFromWithRamp (channelOut, midPos, mDelayBuffer.getReadPointer (channelIn), stretchBufferL.getNumSamples() - midPos, midGain, endGain);
                
                while (samplesAvailableL < outputSamples)
                {
                    stretchL->process(readPointersL, stretchBufferL.getNumSamples(), false); //fills mTempBuffer
                    samplesAvailableL = stretchL->available();
                }
            } else
            {
                stretchBufferR.copyFromWithRamp (0, 0, mDelayBuffer.getReadPointer (channelIn, readPos), midPos, startGain, midGain);
                stretchBufferR.copyFromWithRamp (0, midPos, mDelayBuffer.getReadPointer (channelIn), stretchBufferR.getNumSamples() - midPos, midGain, endGain);
 
                while (samplesAvailableR < outputSamples)
                {
                    stretchR->process(readPointersR, stretchBufferR.getNumSamples(), false); //fills mTempBuffer
                    samplesAvailableR = stretchR->available();
                }
            }
            
        }
        else
        {
            if (channelOut == 0)
            {
                stretchBufferL.addFromWithRamp (channelOut, 0, mDelayBuffer.getReadPointer (channelIn, readPos), midPos, startGain, midGain);
                stretchBufferL.addFromWithRamp (channelOut, midPos, mDelayBuffer.getReadPointer (channelIn), stretchBufferL.getNumSamples() - midPos, midGain, endGain);
                
                while (samplesAvailableL < outputSamples)
                {
                    stretchL->process(readPointersL, stretchBufferL.getNumSamples(), false); //fills mTempBuffer
                    samplesAvailableL = stretchL->available();
                }
            } else
            {
                stretchBufferR.addFromWithRamp (0, 0, mDelayBuffer.getReadPointer (channelIn, readPos), midPos, startGain, midGain);
                stretchBufferR.addFromWithRamp (0, midPos, mDelayBuffer.getReadPointer (channelIn), stretchBufferR.getNumSamples() - midPos, midGain, endGain);
                
                while (samplesAvailableR < outputSamples)
                {
                    stretchR->process(readPointersR, stretchBufferR.getNumSamples(), false); //fills mTempBuffer
                    samplesAvailableR = stretchR->available();
                }
            }
        }
        
    }
    stretchL->retrieve(writePointers, buffer.getNumSamples());
    stretchR->retrieve(writePointers, buffer.getNumSamples());
    
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
