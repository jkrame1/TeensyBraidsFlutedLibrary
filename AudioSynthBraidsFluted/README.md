# Braids Fluted Algorithm for Teensy Audio Library

This is an adaptation of the "fluted" physical modeling algorithm from Mutable Instruments' Braids module for the Teensy Audio Library.

## Overview

The fluted algorithm simulates a flute-like instrument using waveguide synthesis with:
- Delay line (waveguide) for the resonant body
- Noise excitation source
- Configurable parameters for timbre and color

## Files

- `AudioSynthBraidsFluted.h` - Header file with class definition
- `AudioSynthBraidsFluted.cpp` - Implementation file
- `examples/FlutedOscillator/FlutedOscillator.ino` - Example sketch

## Usage

```cpp
#include <Audio.h>
#include "AudioSynthBraidsFluted.h"

AudioSynthBraidsFluted fluted;
AudioOutputI2S i2s;
AudioConnection patchCord1(fluted, 0, i2s, 0);
AudioConnection patchCord2(fluted, 0, i2s, 1);
AudioControlSGTL5000 sgtl5000;

void setup() {
  AudioMemory(10);
  sgtl5000.enable();
  sgtl5000.volume(0.5);
  
  fluted.begin();
  fluted.noteOn(440.0); // A4
  fluted.setTimbre(0.5); // 0.0 to 1.0
  fluted.setColor(0.5);  // 0.0 to 1.0
}
```

## License

Based on Mutable Instruments Braids (MIT License)
Adapted for Teensy Audio Library
