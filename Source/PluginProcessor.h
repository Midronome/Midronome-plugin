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

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class MidronomeAudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    //==============================================================================
    MidronomeAudioProcessor();
    ~MidronomeAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sr, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    //==============================================================================
    float getCurrentTickPulseSample();
    
    
    //==============================================================================
    double sampleRate;
    float* outputData;
    
    bool hasSyncStarted;
    bool currentlySendingTickPulse;
    
    int tickPulseLength;
    
    //==============================================================================
#if JucePlugin_ProducesMidiOutput
    typedef enum values_type {
        BPM,
        BEATS_PER_BAR
    } values_type_t;
    
    void sendMidiToHost(values_type_t v, int newValue, int totalNumSamples, bool isPlaying, juce::MidiBuffer& midiMessages);
    
    int lastValueSent[2];
    int waitBeforeSending[2];
#endif
    
    
#ifdef DEBUG
    class DebugInfo {
      public:
        DebugInfo() {
            //data = new datastruct_t[SIZE];
            reset();
        }
        
        ~DebugInfo() {
            //delete[] data;
        }
        
        
        void reset() {
            currentPos = -1;
            amntTickSent = 0;
            for (auto i = 0 ; i < SIZE ; i++) {
                data[i].hostPpqPos = -1.0f;
                data[i].hostLastBarPos = -1.0f;
                data[i].hostBpm = -1.0f;
                data[i].dPpqPerBlock = -1.0f;
                data[i].firstTickSentAtSample = -1.0f;
                data[i].amntTicksSent = 0;
            }
        }
        
        void add(juce::Optional<juce::AudioPlayHead::PositionInfo> info, double sampleRate, int blockSize) {
            currentPos++;
            
            if (currentPos < 0 || currentPos >= SIZE) {
                currentPos = SIZE;
                return;
            }
            
            data[currentPos].hostPpqPos = static_cast<float>(info->getPpqPosition().orFallback(-10.0));
            data[currentPos].hostLastBarPos = static_cast<float>(info->getPpqPositionOfLastBarStart().orFallback(-10.0));
            double bpm = info->getBpm().orFallback(-10.0);
            data[currentPos].hostBpm = static_cast<float>(bpm);
            double dPpqPerBlock = (static_cast<double>(blockSize)*bpm) / (60.0*sampleRate);
            data[currentPos].dPpqPerBlock = static_cast<float>(dPpqPerBlock);
        }
        
        void addTickSent(double pos) {
            if (currentPos < 0 || currentPos >= SIZE)
                return;
            
            amntTickSent++;
            
            data[currentPos].amntTicksSent = amntTickSent;
            if (data[currentPos].firstTickSentAtSample == -1.0f)
                data[currentPos].firstTickSentAtSample = static_cast<float>(pos);
        }
        
      private:
        static const int SIZE = 1024;
        int currentPos;
        int amntTickSent;
    
        typedef struct datastruct {
            float hostPpqPos;
            float hostLastBarPos;
            float hostBpm;
            float dPpqPerBlock;
            float firstTickSentAtSample;
            int amntTicksSent;
        } datastruct_t ;
        
        datastruct_t data[SIZE];
    };
    
    DebugInfo DEBUGDAT;
#endif
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidronomeAudioProcessor)
};
