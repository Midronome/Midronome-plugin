/*
    ==============================================================================

    This file is part of the Midronome plugin, a VST3/AU/AAX plugin for Digital
	Audio Workstations (DAW) whose purpose is to synchronize DAWs with the
	Midronome (more info on <https://www.midronome.com/>).
 
    Copyright © 2023 - Simon Lasnier (Midronome ApS)

    The Midronome plugin is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by the Free
	Software Foundation, either version 3 of the License, or (at your option) any
	later version.

    The Midronome plugin is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
	details.

    You should have received a copy of the GNU General Public License along with
    the Midronome plugin. If not, see <https://www.gnu.org/licenses/>.
 
    ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MidronomeAudioProcessor::MidronomeAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

MidronomeAudioProcessor::~MidronomeAudioProcessor()
{
}

//==============================================================================
const juce::String MidronomeAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MidronomeAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool MidronomeAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool MidronomeAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double MidronomeAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MidronomeAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int MidronomeAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MidronomeAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String MidronomeAudioProcessor::getProgramName (int index)
{
    return {};
}

void MidronomeAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void MidronomeAudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    
    lastValueSent[BPM] = 0;
    waitBeforeSending[BPM] = 0;
    lastValueSent[BEATS_PER_BAR] = 0;
    waitBeforeSending[BEATS_PER_BAR] = 0;
}

void MidronomeAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MidronomeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
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

void MidronomeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    
    
    
    /// ### INITIALISATION ###
    
    auto totalNumSamples = buffer.getNumSamples();
    
    juce::Optional<juce::AudioPlayHead::PositionInfo> info = getPlayHead()->getPosition();
    juce::Optional<juce::AudioPlayHead::TimeSignature> timeSig = info->getTimeSignature();
    
    auto isPlaying = (info->getIsPlaying() || info->getIsRecording());
    auto bpm = info->getBpm().orFallback(120.0);

    
    /// ### SEND TIME SIGNATURE OVER USB ###
    if (timeSig.hasValue()) {
        sendMidiToHost(BEATS_PER_BAR, (4 * timeSig->numerator) / (timeSig->denominator), totalNumSamples, isPlaying, midiMessages);
    }
    
    /// ### SEND BPM OVER USB ###
    if (!isPlaying && info->getBpm().hasValue()) {
        sendMidiToHost(BPM, static_cast<int>(round(bpm)), totalNumSamples, isPlaying, midiMessages);
    }
}



void MidronomeAudioProcessor::sendMidiToHost(values_type_t v, int newValue, int totalNumSamples, bool isPlaying, juce::MidiBuffer& midiMessages) {
    if (lastValueSent[v] != newValue && waitBeforeSending[v] <= 0) {
        if (!isPlaying && (v != BPM || waitBeforeSending[v] == 0)) {  // no waiting time when not playing except for BPM the first time (with waitBeforeSending[v] = -1)
            waitBeforeSending[v] = 1;
        }
        else {
            waitBeforeSending[v] = static_cast<int>(sampleRate) / 4; // send new value in 250ms - so it's sent after the bar / after stopping sync
            if (v == BEATS_PER_BAR && (lastValueSent[BEATS_PER_BAR] == 1 || newValue == 1))
                waitBeforeSending[v] /= 2; // reduce to 125ms if we are sending new time sig new/old time sig is 1/4
        }
    }
    
    if (waitBeforeSending[v] >= totalNumSamples) {
        waitBeforeSending[v] -= totalNumSamples;
    }
    else if (waitBeforeSending[v] > 0) {
        lastValueSent[v] = newValue;
        
        /*if (v == BPM) {
            // Bitwig seems to be the only DAW not transmitting the PitchWheel below but it will transmit the CC messages...
            midiMessages.addEvent(juce::MidiMessage::controllerEvent(12, 85, newValue / 128), waitBeforeSending[v]);
            midiMessages.addEvent(juce::MidiMessage::controllerEvent(12, 86, newValue % 128), waitBeforeSending[v]);
        }
        else {
            midiMessages.addEvent(juce::MidiMessage::controllerEvent(12, 90, newValue), waitBeforeSending[v]);
        }*/
        
        if (v == BEATS_PER_BAR)
            newValue += (0x7F << 7); // because "MSB" for beats per bar is 0x7F
        
        if (newValue <= 0x3FFF) // max value is 14 bit long
            midiMessages.addEvent(juce::MidiMessage::pitchWheel(12, newValue), waitBeforeSending[v]);
        
        waitBeforeSending[v] = 0;
    }
    // else waitBeforeSending[v] == 0 -> we do nothing
}




//==============================================================================
bool MidronomeAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* MidronomeAudioProcessor::createEditor()
{
    return new MidronomeAudioProcessorEditor (*this);
}

//==============================================================================
void MidronomeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void MidronomeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MidronomeAudioProcessor();
}
