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
MidronomeAudioProcessorEditor::MidronomeAudioProcessorEditor (MidronomeAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (300, 250);
}

MidronomeAudioProcessorEditor::~MidronomeAudioProcessorEditor()
{
}

//==============================================================================
void MidronomeAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto myImg = juce::ImageCache::getFromMemory(BinaryData::midrologo_png, BinaryData::midrologo_pngSize);
    g.fillAll(juce::Colours::black);
    g.drawImageAt(myImg, 86, 87);
}

void MidronomeAudioProcessorEditor::resized()
{
}
