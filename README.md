# Old Midronome VST3/AU/AAX Plugin

**IMPORTANT**:
* If you are syncing your Midronome on Mac you might want to use [U-SYNC](https://midronome.com/support) instead of this plugin. See the [Midronome User Guide](https://go.midronome.com/userguide) for more information.
* This plugin is not maintained anymore by Midronome, but feel free to report issues here anyways.


## Purpose

This plugin generates 24ppq Audio pulses in your DAW in order to sync with the [Midronome](https://www.midronome.com/). It can probably be used with any other device that can follow analog pulses as well.
You can download the v1.1.0 release under "*Releases*" in the right column.

See the [Midronome User Guide](https://go.midronome.com/userguide) section 5.4 on how to use the plugin to sync your Midronome. 

## MIDI

The plugin also generates MIDI messages which can be sent to the Midronome over USB in order to change the time signature and the tempo when the DAW playhead is not moving.
The VST3 and AAX version of the plugin sends both audio and MIDI, while the AU needs two plugins: "Midronome" sends Audio, while "MidronomeMIDI" sends MIDI.


## Compile the Code

The plugin is based around the [JUCE framework](https://juce.com/) version 7. The Projucer files are included.

To compile it, you will need:
* The JUCE framework and the Projucer - [more info](https://juce.com/download/)
* An IDE: Xcode on Mac, Visual Studio 2022 on Windows

IMPORTANT: to compile the AU "Midronome" plugin, first set "JucePlugin_WantsMidiInput" and "JucePlugin_ProducesMidiOutput" to 0 in the JucePluginDefines.h file.


## Copyright notice

This code is free software, it is released under the terms of [GNU GPLv3](https://www.gnu.org/licenses/gpl-3.0.en.html). You can redistribute it and/or modify it under the same terms. See the COPYING.txt file for more information.
