# Midronome VST3/AU/AAX Plugin

This plugin is designed to send 24ppq Audio pulses in order to sync with the [Midronome](https://www.midronome.com/). See more info on [the Midronome manual](https://www.midronome.com/support) and on the [Midronome Youtube Channel](https://www.youtube.com/@midronome).

You can download the compiled plugin on the [the Midronome support page](https://www.midronome.com/support).

Notice that the VST3 and AAX version of the plugin sends both audio and MIDI, while the AU needs two plugins: "Midronome" sends Audio, while "MidronomeMIDI" sends MIDI. The "MidronomeMIDI" AU is a "MIDI FX" plugin and can only be loaded in Logic Pro.


## Compile the Code

The plugin is based around the [JUCE framework](https://juce.com/), the Projucer files are included.

To compile it, you will need:
* The JUCE framework and the Projucer - [more info](https://juce.com/download/)
* An IDE: Xcode on Mac, Visual Studio 2022 on Windows

IMPORTANT: to compile the AU "Midronome" plugin, first set "JucePlugin_WantsMidiInput" and "JucePlugin_ProducesMidiOutput" to 0 in the JucePluginDefines.h file.



## Copyright notice

This code is free software, it is released under the terms of [GNU GPLv3](https://www.gnu.org/licenses/gpl-3.0.en.html). You can redistribute it and/or modify it under the same terms. See the COPYING.txt file for more information.