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
    
    lastValueSent[BPM] = 0;
    waitBeforeSending[BPM] = 0;
    lastValueSent[BEATS_PER_BAR] = 0;
    waitBeforeSending[BEATS_PER_BAR] = 0;
    
    tickPulseLength = 24; // 0.5ms at 48kHz, a bit more at 44.1kHz
    if (sampleRate > 50000.0) // 88.2 and 96 kHz
        tickPulseLength *= 2;
    if (sampleRate > 100000.0) // 176.4 and 192 kHz
        tickPulseLength *= 2;
    
    // set to tempo limits to 29.9bpm -> 400.2bpm - ticks will always be sent according to these
    minSamplesNumBetweenTicks = static_cast<int64_t>(sampleRate / ( (400.2*24.0)/60.0 ));
    maxSamplesNumBetweenTicks = static_cast<int64_t>(sampleRate / ( (29.9*24.0)/60.0 ));
    
#ifdef DEBUG
    LOGGER.reset();
#endif
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
    
    auto info = getPlayHead()->getPosition();
    auto timeSig = info->getTimeSignature();
    auto isPlaying = info->getIsPlaying();
    
    int beatsPerBar = 4; // 4/4 time sig per default
    auto bpm = info->getBpm().orFallback(0.0);
    
    bool timeSigIn8 = false;

    // clear buffers
    buffer.clear();
    for (auto i = 0; i < totalNumSamples; i++)
        outputData[i] = 0;
    
    
    
    
    /// ### SEND TIME SIGNATURE OVER USB ###
    if (timeSig.hasValue()) {
        beatsPerBar = (4 * timeSig->numerator) / (timeSig->denominator);
        auto beatPerBarToSend = beatsPerBar;
        if (timeSig->denominator == 8) {
            beatPerBarToSend = timeSig->numerator;
            timeSigIn8 = true;
        }
        
        sendMidiToHost(BEATS_PER_BAR, beatPerBarToSend, totalNumSamples, isPlaying, midiMessages);
    }
    
    
    
    /// ### PREPARATIONS BEFORE SAMPLE LOOP ###
        
    if (isPlaying && bpm >= 30.0 && bpm <= 400.0)
    {
        
#ifdef DEBUG
        if (!LOGGER.prevPlayingStatus) {
            LOGGER.reset();
            LOGGER.prevPlayingStatus = true;
        }
        LOGGER.logBlockInfo(info);
#endif
        
        auto dppqPerSample = bpm / (60.0*sampleRate);
        auto currentPpqPos = info->getPpqPosition().orFallback(0.0);
        
        lastValueSent[BPM] = 0;
        waitBeforeSending[BPM] = -1; // to indicate to sendMidiToHost() to delay sending
        
        // checking playing continuity (if playhead moved manually or we looped f.x.)
        auto curTimeInSamples = info->getTimeInSamples();
        if (!curTimeInSamples.hasValue() || std::abs(curTimeInSamples.orFallback(0) - expectedTimeInSamples) > 2)
            lastTickNo = -1; // if the playhead has been moved by more than 2 samples, we make lastTickNo invalid
        
        expectedTimeInSamples = curTimeInSamples.orFallback(0) + totalNumSamples; // for next block check
        
        
        
        
        /// ### MAIN SAMPLE LOOP ###
        
        for (auto i = 0; i < totalNumSamples; i++)
        {
            if (currentPpqPos >= 0.0) { // will be < 0 during pre-rolls, maybe a block before it starts, and sometimes when positionOfLastBarStart is after
                double errorRange = 20.0*dppqPerSample; // 20 samples error range because of rounding and samples not "landing" exactly on a tick
                
                // we start the sync when we are almost 0 modulo beatsPerBar quarternotes, i.e. start of a bar
                if (!hasSyncStarted) {
                    auto ppqPosFromLastBar = currentPpqPos - info->getPpqPositionOfLastBarStart().orFallback(0.0); // to check with beatsPerBar we need to start from the last bar
                    
                    if (fmod(ppqPosFromLastBar, static_cast<double>(beatsPerBar)) < errorRange) {
                        hasSyncStarted = true;
                        
                        // sync always starts by sending a tick, so this ensures samplesSinceLastTick => maxSamplesNumBetweenTicks to send the tick below
                        samplesSinceLastTick = static_cast<int64_t>(sampleRate);
                        lastTickNo = -1;
                    }
                }
                
                if (hasSyncStarted && !currentlySendingTickPulse) {
                    bool sendTick = false;
                    auto tickPos = currentPpqPos*24.0;
                    auto currentTickNo = static_cast<int64_t>(tickPos);
                    auto tickRest = tickPos - floor(tickPos); // decimals of the current tick position
                    bool extraTickInTimeSig8 = false;
                    
                    if (lastTickNo >= 0) { // if we have a valid last tick position
                        if (currentTickNo > lastTickNo) { // if there is 1 (or more) tick between current and last tick => we send a tick
                            sendTick = true;
                        }
                        else if (timeSigIn8 && (currentTickNo == lastTickNo)) { // in time signatures in x/8 we send twice as many ticks
                            tickRest -= 0.5;
                            if (tickRest >= 0 && tickRest < errorRange*24.0) {
                                sendTick = true;
                                extraTickInTimeSig8 = true;
                            }
                        }
                        // else {} -> we already sent current (or more) tick and there are no extra tick in x/8 time sig => we wait
                        
                    }
                    else { // if we do not have a valid last tick (sync just started, or playhead has moved)
                        if (tickRest < errorRange*24.0)
                            sendTick = true;
                    }
                    
                    
                    // we do not send a tick if it will give a tempo > 400bpm, and we make sure to send one to avoid tempo < 30bpm
                    if ( (sendTick && samplesSinceLastTick >= minSamplesNumBetweenTicks) || samplesSinceLastTick >= maxSamplesNumBetweenTicks) {
                        if (lastTickNo < 0)
                            lastTickNo = currentTickNo; // we "initialize" lastTickNo if it was not valid
                        else if (!extraTickInTimeSig8)
                            lastTickNo++; // we increment if it was valid, but not for the extra tick in x/8 time sig
                        samplesSinceLastTick = 0;
                        currentlySendingTickPulse = true;
#ifdef DEBUG
                        LOGGER.logTickPulseSent(currentPpqPos, lastTickNo, info);
#endif
                    }
                }
            }
            
            outputData[i] = getCurrentTickPulseSample(); // updates currentlySendingTickPulse accordingly
             
            currentPpqPos += dppqPerSample; // increment current ppq pos based on the current BPM
            samplesSinceLastTick++;
        }
    }
    
    
    
    
    /// ### WHEN NOT PLAYING OR WHEN BPM IS OUT OF RANGE ###
    
    else
    {
#ifdef DEBUG
        if (LOGGER.prevPlayingStatus)
            LOGGER.prevPlayingStatus = false;
#endif
            
        hasSyncStarted = false;
        
        // Finish sending pulse if needed
        if (currentlySendingTickPulse) {
            for (auto i = 0; i < totalNumSamples; i++)
                outputData[i] = getCurrentTickPulseSample(); // updates currentlySendingTickPulse accordingly
        }
        
        
        // Send BPM over USB if it is valid
        auto bpmToSend = bpm;
        if (timeSigIn8)
            bpmToSend *= 2;
        if (bpmToSend >= 30.0 && bpmToSend <= 400.0)
            sendMidiToHost(BPM, static_cast<int>(round(bpmToSend)), totalNumSamples, isPlaying, midiMessages);
    }
    
    
    
    
    /// ### FILL ACTUAL OUTPUT BUFFER ###
    
    int numChannels = getTotalNumOutputChannels();
    for (auto ch = 0 ; ch < numChannels ; ch++) {
        auto* data = buffer.getWritePointer(ch);
        
        for (auto i = 0 ; i < totalNumSamples ; i++)
            data[i] = outputData[i];
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
