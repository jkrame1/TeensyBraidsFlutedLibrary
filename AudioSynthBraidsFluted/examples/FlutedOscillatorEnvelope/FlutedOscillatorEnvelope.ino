/*
 * FlutedOscillatorEnvelope.ino
 * 
 * Demonstrates the ASR envelope system of AudioSynthBraidsFluted
 * 
 * This example cycles through different envelope configurations:
 * 1. Amplitude envelope only (default - most natural)
 * 2. Amplitude + Color envelope (brightness opens with note)
 * 3. Amplitude + Timbre envelope (tone changes during note)
 * 4. All three modulated (complex evolving sound)
 * 
 * Hardware:
 * - Teensy 3.2, 3.5, 3.6, 4.0, or 4.1
 * - Audio Shield or I2S DAC
 */

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include "AudioSynthBraidsFluted.h"

// Create audio objects
AudioSynthBraidsFluted   fluted;
AudioOutputI2S           i2s;
AudioConnection          patchCord1(fluted, 0, i2s, 0);
AudioConnection          patchCord2(fluted, 0, i2s, 1);
AudioControlSGTL5000     sgtl5000;

// Timing variables
unsigned long noteStartTime = 0;
unsigned long configChangeTime = 0;
bool noteIsOn = false;
int currentNote = 0;
int currentConfig = 0;

// Musical notes
const float notes[] = {
    261.63,  // C4
    329.63,  // E4
    392.00,  // G4
    523.25   // C5
};
const int numNotes = 4;

// Configuration names
const char* configNames[] = {
    "Amplitude Only",
    "Amplitude + Color",
    "Amplitude + Timbre",
    "All Parameters"
};

void setup() {
    Serial.begin(115200);
    delay(500);
    
    AudioMemory(10);
    sgtl5000.enable();
    sgtl5000.volume(0.5);
    
    // Initialize fluted oscillator
    fluted.begin();
    
    // Set base parameters
    fluted.setTimbre(0.6);    // Nice flute tone
    fluted.setColor(0.5);     // Moderate breath noise
    fluted.setAmplitude(0.8);
    
    // Set envelope times
    fluted.setAttack(50.0);   // 50ms attack
    fluted.setRelease(200.0); // 200ms release
    
    // Start with configuration 0
    setConfiguration(0);
    
    Serial.println("========================================");
    Serial.println("Fluted Oscillator - Envelope Demo");
    Serial.println("========================================");
    Serial.println();
    Serial.println("Cycling through envelope configurations");
    Serial.println("Each plays 4 notes, then switches...");
    Serial.println();
    
    configChangeTime = millis();
}

void loop() {
    unsigned long currentTime = millis();
    
    // Change configuration every 10 seconds (4 notes × 2.5s)
    if (currentTime - configChangeTime >= 10000) {
        currentConfig = (currentConfig + 1) % 4;
        setConfiguration(currentConfig);
        configChangeTime = currentTime;
        currentNote = 0;  // Reset note pattern
    }
    
    // Play notes
    if (!noteIsOn && (currentTime - noteStartTime >= 500)) {
        // Note on
        fluted.noteOn(notes[currentNote % numNotes]);
        noteIsOn = true;
        noteStartTime = currentTime;
        
        Serial.print("♪ ");
        Serial.print(notes[currentNote % numNotes], 1);
        Serial.println(" Hz");
        
        currentNote++;
        
    } else if (noteIsOn && (currentTime - noteStartTime >= 400)) {
        // Note off (triggers release)
        fluted.noteOff();
        noteIsOn = false;
        noteStartTime = currentTime;
    }
    
    delay(10);
}

void setConfiguration(int config) {
    Serial.println("========================================");
    Serial.print("Configuration ");
    Serial.print(config + 1);
    Serial.print("/4: ");
    Serial.println(configNames[config]);
    Serial.println("========================================");
    
    switch (config) {
        case 0:
            // Amplitude only (most natural, like a real flute)
            Serial.println("Envelope controls: Volume");
            Serial.println("Timbre & Color: Static");
            Serial.println();
            fluted.setEnvelopeRouting(false, false, true);
            fluted.setAmplitudeEnvDepth(1.0);
            break;
            
        case 1:
            // Amplitude + Color (brightness opens with volume)
            Serial.println("Envelope controls: Volume + Brightness");
            Serial.println("Timbre: Static");
            Serial.println("(Dark attack, bright sustain)");
            Serial.println();
            fluted.setEnvelopeRouting(false, true, true);
            fluted.setColorEnvDepth(0.8);      // 80% modulation
            fluted.setAmplitudeEnvDepth(1.0);
            break;
            
        case 2:
            // Amplitude + Timbre (tone evolves during note)
            Serial.println("Envelope controls: Volume + Tone");
            Serial.println("Color: Static");
            Serial.println("(Tone shifts during attack)");
            Serial.println();
            fluted.setEnvelopeRouting(true, false, true);
            fluted.setTimbreEnvDepth(0.6);     // 60% modulation
            fluted.setAmplitudeEnvDepth(1.0);
            break;
            
        case 3:
            // All three (complex evolving sound)
            Serial.println("Envelope controls: Volume + Tone + Brightness");
            Serial.println("(Full spectral evolution)");
            Serial.println();
            fluted.setEnvelopeRouting(true, true, true);
            fluted.setTimbreEnvDepth(0.5);
            fluted.setColorEnvDepth(0.7);
            fluted.setAmplitudeEnvDepth(1.0);
            break;
    }
}
