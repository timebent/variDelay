/*
  ==============================================================================

    Delay.h
    Created: 11 Jul 2020 2:43:53pm
    Author:  Billie (Govnah) Jean

  ==============================================================================
*/

#pragma once

template <typename Type>
class DelayLine
{
public:
    /* fills delay line with 0's */
    void clear() noexcept
    {
        std::fill (rawData.begin(), rawData.end(), Type (0));
    }
    
    size_t size() const noexcept
    {
        return rawData.size();
    }
    
    void resize (size_t newValue)
    {
        rawData.resize (newValue);
        readIndex = 0;
    }
    
    
    Type back() const noexcept
    {
        return rawData[readIndex];
    }
    
    Type get (size_t delayInSamples) const noexcept
    {
        jassert (delayInSamples >= 0 && delayInSamples < size());
        return rawData[(readIndex + 1 + delayInSamples) % size()];   // [3]
    }
    
    /** Set the specified sample in the delay line */
    void set (size_t delayInSamples, Type newValue) noexcept
    {
        jassert (delayInSamples >= 0 && delayInSamples < size());
        
        rawData[(readIndex + 1 + delayInSamples) % size()] = newValue; // [4]
    }
    
    /** Adds a new value to the delay line, changing the head index to the least recently added sample */
    void push (Type valueToAdd) noexcept
    {
        rawData[readIndex] = valueToAdd;
      
        if (readIndex == 0)
        {
            /* end of buffer */
            readIndex = size() - 1;
        }
        else
        {
            /* get index that is before the current index? */
            readIndex = readIndex - 1;
        }
        readIndex = readIndex == 0 ? size() - 1 : readIndex - 1;
    }
    
private:
   
    std::vector<Type> rawData;
    size_t readIndex = 0;
};

//==============================================================================
template <typename Type, size_t maxNumChannels = 2>
class Delay
{
public:
    //==============================================================================
    Delay()
    {
        setMaxDelayTime (2.0f);
        setDelayTime (0, 0.7f);
        setDelayTime (1, 0.5f);
        setWetLevel (0.8f);
        setFeedback (0.5f);
    }
    
    //==============================================================================
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        jassert (spec.numChannels <= maxNumChannels);
        sampleRate = (Type) spec.sampleRate;
        updateDelayLineSize();
        updateDelayTime();
   
    }
    
    //==============================================================================
    void reset() noexcept
    {
       for (auto& dline : delayLines)
            dline.clear();
    }
    
    //==============================================================================
    size_t getNumChannels() const noexcept
    {
        return delayLines.size();
    }
    
    //==============================================================================
    void setMaxDelayTime (Type newValue)
    {
        jassert (newValue > Type (0));
        maxDelayTime = newValue;
        updateDelayLineSize(); // [1]
    }
    
    //==============================================================================
    void setFeedback (Type newValue) noexcept
    {
        jassert (newValue >= Type (0) && newValue <= Type (1));
        feedback = newValue;
    }
    
    //==============================================================================
    void setWetLevel (Type newValue) noexcept
    {
        jassert (newValue >= Type (0) && newValue <= Type (1));
        wetLevel = newValue;
    }
    
    //==============================================================================
    void setDelayTime (size_t channel, Type newValue)
    {
        
        if (channel >= getNumChannels())
        {
            jassertfalse;
            return;
        }
        
        jassert (newValue >= Type (0));
        delayTime[channel] = newValue;
        
        updateDelayTime();  // [3]
    }
    
    //==============================================================================
    template <typename ProcessContext>
    void process (const ProcessContext& context) noexcept
    {
        auto& inputBlock  = context.getInputBlock();
        auto& outputBlock = context.getOutputBlock();
        auto numSamples  = outputBlock.getNumSamples();
        auto numChannels = outputBlock.getNumChannels();
        
        /* I guess we want them to be the same size */
        jassert (inputBlock.getNumSamples() == numSamples);
        jassert (inputBlock.getNumChannels() == numChannels);
        
        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            auto* input  = inputBlock .getChannelPointer (ch);
            auto* output = outputBlock.getChannelPointer (ch);
            auto& dline = delayLines[ch];
            auto& dTime = delayTimeInSamples[ch];
            
          
            
            for (size_t i = 0; i < numSamples; ++i)
            {
                /* get sample from dline that is dTime samples back */
                auto delayedSample = dline.get (dTime);
                auto nextDelayedSample = dline.get(dTime + 1);
                
                /* reference to current dry input sample */
                auto inputSample = input[i];
                
                /* readIndex is now equal to the index that is dTime (in samples) back */
                readIndex = validateReadPosition (writeIndex - dTime);
                
                /* gives me a percent into the next index */
                readIndexFraction = readIndex - (long)readIndex;
                
                /* calculate how far into the change between samples we are and adds that to the delayedSample  */
                const float interpSample = delayedSample + (readIndexFraction * (nextDelayedSample - delayedSample));
                
                
                auto dlineInputSample = std::tanh (inputSample + feedback * interpSample);
                
                dline.push (dlineInputSample);
                
                /*
                    output inputSample + delayedSample, where
                    delayedSample is scaled by wetLevel.
                    Should I scale inputSample too?
                    Don't need to use interpSample because we already
                    interpolated as it went to the delayBuffer
                */
                auto outputSample = inputSample + wetLevel * delayedSample;
                
                output[i] = outputSample;
                
                /* increment writeIndex until it hits maxDelay size, then set to 0 */
                writeIndex = writeIndex != maxDelayInSamples - 1 ? writeIndex + 1 : 0;
            }
        }
    }
    
private:
    //==============================================================================
    std::array<DelayLine<Type>, maxNumChannels> delayLines; // array of delay lines
    std::array<size_t, maxNumChannels> delayTimeInSamples; // array of delay times in samples
    std::array<Type, maxNumChannels> delayTime; // array of delay times in sec
    Type feedback { Type (0) };
    Type wetLevel { Type (0) };
    float readIndex {0.f}, readIndexFraction {0.f};
    long maxDelayInSamples {0}, writeIndex {0};

    
    Type sampleRate   { Type (44.1e3) };
    Type maxDelayTime { Type (2) };
    
    //==============================================================================
    void updateDelayLineSize()
    {
        /*
            std::ceil -> smallest integer not below argument
            makes sure we have enough sample indices
        */
        auto delayLineSizeSamples = (size_t) std::ceil (maxDelayTime * sampleRate);
        
        /*
            make a bunch of delayLines at the specified size
            delayLines = numChannels
         */
        for (auto& dline : delayLines)
            dline.resize (delayLineSizeSamples);
    }
    
    //==============================================================================
    void updateDelayTime() noexcept
    {
        /* set delayTime for each Channel */
        for (size_t ch = 0; ch < maxNumChannels; ++ch)
            delayTimeInSamples[ch] = (size_t) juce::roundToInt (delayTime[ch] * sampleRate);
    }
    
   
    
    // Ensure the read position is above 0 and less than the maximum delay amount
    inline float validateReadPosition (float readPosition)
    {
        /* first time readPosition is negative */
        if (readPosition >= 0.f)
        {
            if (readPosition < maxDelayInSamples)
                return readPosition;
            
            /*
                maxDelayInSamples is buffer size so subtracting it
                is essentially a modulo
            */
            return readPosition - maxDelayInSamples;
        }
        /*
            Read position has undershot, wrap forward.
            maxDelayInSamples should be the size of the
            delayBuffer
        */
        return readPosition + maxDelayInSamples;
    }
};

