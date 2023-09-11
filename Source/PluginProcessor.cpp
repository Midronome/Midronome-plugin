/*
    ==============================================================================

    This file is part of the Midronome plugin, a VST3/AU/AAX plugin for Digital
	Audio Workstations (DAW) whose purpose is to synchronize DAWs with the
	Midronome (more info on <https://www.midronome.com/>).
 
    Copyright © 2023 - Simon Lasnier (Midronome ApS)
  	Copyright © 2015-2018 - Maximilian Rest (E-RM)

	Note: the algorithm to send the audio pulses is partially inspired from the
	one in the Multiclock plugin from Maximilian Rest from E-RM, who nicely
	released his plugin under the GPL terms as well. Thank you so much Max :)
	See the multiclock's plugin code on
	https://github.com/m-rest/tick_tock/tree/main

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
    outputData = NULL;
}

MidronomeAudioProcessor::~MidronomeAudioProcessor()
{
    if(outputData != NULL)
        delete [] outputData;
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
    
    if (outputData != NULL)
        delete [] outputData;
    outputData = new float[samplesPerBlock];
    
    hasSyncStarted = false;
    currentlySendingTickPulse = false;
    
#if JucePlugin_ProducesMidiOutput
    lastValueSent[BPM] = 0;
    waitBeforeSending[BPM] = 0;
    lastValueSent[BEATS_PER_BAR] = 0;
    waitBeforeSending[BEATS_PER_BAR] = 0;
#endif
    
    tickPulseLength = 24; // 0.5ms at 48kHz, a bit more at 44.1kHz
    if (sampleRate > 50000.0) // 88.2 and 96 kHz
        tickPulseLength *= 2;
    if (sampleRate > 100000.0) // 176.4 and 192 kHz
        tickPulseLength *= 2;
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
    
    int beatsPerBar = 4; // 4/4 time sig per default
    auto bpm = info->getBpm().orFallback(120.0);

    // clear buffers
    buffer.clear();
    for (auto i = 0; i < totalNumSamples; i++)
        outputData[i] = 0;
    
    
    
    
    /// ### SEND TIME SIGNATURE OVER USB ###
    if (timeSig.hasValue()) {
        beatsPerBar = (4 * timeSig->numerator) / (timeSig->denominator);
#if JucePlugin_ProducesMidiOutput
        sendMidiToHost(BEATS_PER_BAR, beatsPerBar, totalNumSamples, isPlaying, midiMessages);
#endif
    }

    
    
    
    /// ### PREPARATIONS BEFORE SAMPLE LOOP ###
        
    if (isPlaying)
    {
        double dppqPerSample = bpm / (60.0f*sampleRate);
        double currentPpqPos = info->getPpqPosition().orFallback(0.0);
        
#if JucePlugin_ProducesMidiOutput
        lastValueSent[BPM] = 0;
        waitBeforeSending[BPM] = -1; // to indicate to sendMidiToHost() to delay sending
#endif
        
        if (!hasSyncStarted)
            currentPpqPos -= info->getPpqPositionOfLastBarStart().orFallback(0.0); // because to check with beatsPerBar we need to start from the last bar
        
        
#ifdef DEBUG
        if (hasSyncStarted)
            DEBUGDAT.add(info, sampleRate, totalNumSamples);
#endif
        
        
        /// ### MAIN SAMPLE LOOP ###
        
        for (auto i = 0; i < totalNumSamples; i++)
        {
            if (currentPpqPos >= 0.0) { // will be < 0 during pre-rolls and like
                double errorRange = 20.0*dppqPerSample; // 20 samples error range because of rounding and samples not "landing" exactly on a tick
                
                // we start the sync when we are almost 0 modulo beatsPerBar quarternotes, i.e. start of a bar
                if (!hasSyncStarted && fmod(currentPpqPos, static_cast<double>(beatsPerBar)) <= errorRange) {
                    hasSyncStarted = true;
#ifdef DEBUG
                    DEBUGDAT.reset();
                    DEBUGDAT.add(info, sampleRate, totalNumSamples);
#endif
                }
                
                // we send a pulse if we are almost 0 modulo 1/24th of a quarter note
                if (hasSyncStarted && !currentlySendingTickPulse && fmod(currentPpqPos, (1.0/24.0)) <= errorRange) {
                    currentlySendingTickPulse = true;
#ifdef DEBUG
                    DEBUGDAT.addTickSent(currentPpqPos);
#endif
                }
            }
            
            outputData[i] = getCurrentTickPulseSample(); // updates currentlySendingTickPulse accordingly
             
            currentPpqPos += dppqPerSample; // increment current ppq pos based on the current BPM
        }
    }
    
    else // not playing or recording
    {
        hasSyncStarted = false;
        
        
        /// ### FINISH SENDING PULSE ###
        if (currentlySendingTickPulse) {
            for (auto i = 0; i < totalNumSamples; i++)
                outputData[i] = getCurrentTickPulseSample(); // updates currentlySendingTickPulse accordingly
        }
        
        
        /// ### SEND BPM OVER USB ###
#if JucePlugin_ProducesMidiOutput
        if (info->getBpm().hasValue())
            sendMidiToHost(BPM, static_cast<int>(round(bpm)), totalNumSamples, isPlaying, midiMessages);
#endif
    }
    
    
    
    
    /// ### FILL ACTUAL OUTPUT BUFFER ###
    
    int numChannels = getTotalNumOutputChannels();
    for (auto ch = 0 ; ch < numChannels ; ch++) {
        auto* data = buffer.getWritePointer(ch);
        
        for (auto i = 0 ; i < totalNumSamples ; i++)
            data[i] = outputData[i];
    }
    
}


#if JucePlugin_ProducesMidiOutput

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
        
        if (v == BPM) {
            // Bitwig seems to be the only DAW not transmitting the PitchWheel below but it will transmit the CC messages...
            midiMessages.addEvent(juce::MidiMessage::controllerEvent(12, 85, newValue / 128), waitBeforeSending[v]);
            midiMessages.addEvent(juce::MidiMessage::controllerEvent(12, 86, newValue % 128), waitBeforeSending[v]);
        }
        else {
            midiMessages.addEvent(juce::MidiMessage::controllerEvent(12, 90, newValue), waitBeforeSending[v]);
        }
        
        if (v == BEATS_PER_BAR)
            newValue += (0x7F << 7); // because "MSB" for beats per bar is 0x7F
        
        if (newValue <= 0x3FFF) // max value is 14 bit long
            midiMessages.addEvent(juce::MidiMessage::pitchWheel(12, newValue), waitBeforeSending[v]);
        
        waitBeforeSending[v] = 0;
    }
    // else waitBeforeSending[v] == 0 -> we do nothing
}

#endif


#define TICK_HEIGHT         0.9f

//==============================================================================
// returns the current sample, 0 if not sending TickPulse, and updates currentlySendingTickPulse
float MidronomeAudioProcessor::getCurrentTickPulseSample()
{
    static int idx = 0;
    
    if (!currentlySendingTickPulse)
        return 0.0f;
    
    idx++;
    
    if (idx < 4) {
        return ((static_cast<float>(idx)*TICK_HEIGHT)/4.0f);
    }
    
    int samplesBeforeEnd = tickPulseLength - idx;
    
    if (samplesBeforeEnd <= 0) {
        currentlySendingTickPulse = false;
        idx = 0;
        return 0.0f;
    }
    
    if (samplesBeforeEnd < 15)
        return ((static_cast<float>(samplesBeforeEnd)*TICK_HEIGHT)/15.0f);
    
    return TICK_HEIGHT;
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
