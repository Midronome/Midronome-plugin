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
    typedef enum values_type {
        BPM,
        BEATS_PER_BAR
    } values_type_t;
    
    void sendMidiToHost(values_type_t v, int newValue, int totalNumSamples, bool isPlaying, juce::MidiBuffer& midiMessages);
    
    int lastValueSent[2];
    int waitBeforeSending[2];
    
    
#ifdef DEBUG
    class DebugLogger {
      public:
        DebugLogger() {}
        
        void logBlockInfo(juce::Optional<juce::AudioPlayHead::PositionInfo>& info) {
            if (nextBlockInfoLog < SIZE) {
                blockInfoLogs[nextBlockInfoLog].set(info);
                nextBlockInfoLog++;
            }
        }
        
        void logPulseSent(double ppqPos) {
            if (nextTickLog < SIZE) {
                tickSentPpqPos[nextTickLog] = ppqPos;
                nextTickLog++;
            }
        }
        
        void reset() {
            nextBlockInfoLog = 0;
            nextTickLog = 0;
            prevPlayingStatus = false;
        }
        
        bool prevPlayingStatus;
        
        
      private:
        
        class BlockInfo {
          public:
            BlockInfo() {}
            
            void set(juce::Optional<juce::AudioPlayHead::PositionInfo>& info) {
                ppqPosition = info->getPpqPosition().orFallback(-999999.0);
                timeInSamples = info->getTimeInSamples().orFallback(-999999);
                timeInSeconds = info->getTimeInSeconds().orFallback(-999999.0);
                bpm = info->getBpm().orFallback(-999999.0);
                isRecordingOrPlaying = 0;
                if (info->getIsPlaying())
                    isRecordingOrPlaying += 1;
                if (info->getIsRecording())
                    isRecordingOrPlaying += 10;
            }
            
          private:
            double ppqPosition;
            int64_t timeInSamples;
            double timeInSeconds;
            double bpm;
            int isRecordingOrPlaying; // 1 for playing, 10 for recording (11 for both)
        };
        
        static const int SIZE = 512;
        unsigned int nextBlockInfoLog;
        unsigned int nextTickLog;
        BlockInfo blockInfoLogs[SIZE];
        double tickSentPpqPos[SIZE];
    };
    
    DebugLogger LOGGER;
#endif
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidronomeAudioProcessor)
};
