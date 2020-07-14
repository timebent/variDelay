

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
                       ), apvts (*this, nullptr, "Parameters", createParameters())
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
    const auto numChannels = getTotalNumOutputChannels();
    seconds = sampleRate;
    update();
    
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = uint32_t(samplesPerBlock);
    spec.numChannels = uint32_t(numChannels);
    fxChain.prepare(spec);
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
        
    ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    auto block = juce::dsp::AudioBlock<float> (buffer);
    auto context = juce::dsp::ProcessContextReplacing<float> (block);
    fxChain.process (context);
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
    mustUpdateProcessing = false;
    auto lTime = apvts.getRawParameterValue ("Time L");
    auto rTime = apvts.getRawParameterValue ("Time R");
    auto feedback = apvts.getRawParameterValue("FB");
    auto wetLevel = apvts.getRawParameterValue("WET");
    
    using mult = juce::ValueSmoothingTypes::Multiplicative;
    using lin = juce::ValueSmoothingTypes::Linear;
    
    juce::SmoothedValue<float, mult> lDelay;
    lDelay.setTargetValue(*lTime);
    
    juce::SmoothedValue<float, mult> rDelay;
    rDelay.setTargetValue(*rTime);
    
    juce::SmoothedValue<float, mult> mFeedback;
    mFeedback.setTargetValue(*feedback);
    
    juce::SmoothedValue<float, mult> mWet;
    mWet.setTargetValue(*wetLevel);
    
    auto& delay = fxChain.get<delayIndex>();
    
    delay.setDelayTime(0, lDelay.getNextValue());
    delay.setDelayTime(1, rDelay.getNextValue());
    delay.setFeedback(mFeedback.getNextValue());
    delay.setWetLevel(mWet.getNextValue());
}

juce::AudioProcessorValueTreeState::ParameterLayout VariDelayAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    
    parameters.push_back (std::make_unique<AudioParameterFloat>("Time L", "Delay Time L", NormalisableRange<float> (0.0f, 1.9f, 0.01f, 1.0f), 0.2f));
    parameters.push_back (std::make_unique<AudioParameterFloat>("Time R", "Delay Time R", NormalisableRange<float> (0.0f, 1.9f, 0.01f, 1.0f), 0.2f));
    parameters.push_back (std::make_unique<AudioParameterFloat>("FB", "Feedback", NormalisableRange<float> (0.0f, 0.9f, 0.01f, 1.0f), 0.2f));
    parameters.push_back (std::make_unique<AudioParameterFloat>("WET", "Wet Level", NormalisableRange<float> (0.0f, 1.0f, 0.01f, 1.0f), 0.2f));
    return { parameters.begin(), parameters.end() };
}
