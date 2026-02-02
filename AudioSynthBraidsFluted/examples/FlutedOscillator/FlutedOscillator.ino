/*
 * FlutedOscillator.ino
 * 
 * Example sketch for AudioSynthBraidsFluted
 * Demonstrates the fluted physical modeling oscillator
 * 
 * Hardware:
 * - Teensy 3.2, 3.5, 3.6, 4.0, or 4.1
 * - Audio Shield or I2S DAC
 * 
 * Connect potentiometers (optional):
 * - A0: Timbre (feedback/resonance)
 * - A1: Color (brightness/dampening)
 * - A2: Frequency
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

// Variables for control
float currentFreq = 440.0;
bool noteIsOn = false;
unsigned long lastNoteTime = 0;
int notePattern = 0;

// Musical notes (in Hz)
const float notes[] = {
    261.63,  // C4
    293.66,  // D4
    329.63,  // E4
    349.23,  // F4
    392.00,  // G4
    440.00,  // A4
    493.88,  // B4
    523.25   // C5
};
const int numNotes = 8;

void setup() {
    Serial.begin(115200);
    
    // Audio connections require memory to work
    AudioMemory(10);
    
    // Enable the audio shield
    sgtl5000.enable();
    sgtl5000.volume(0.5);
    
    // Initialize the fluted oscillator
    fluted.begin();
    fluted.setAmplitude(0.8);
    
    Serial.println("Braids Fluted Oscillator - Example");
    Serial.println("Playing automatic melody...");
    Serial.println();
    Serial.println("Controls (if potentiometers connected):");
    Serial.println("  A0 - Timbre (feedback/resonance)");
    Serial.println("  A1 - Color (brightness/dampening)");
    Serial.println("  A2 - Frequency offset");
}

void loop() {
    // Read potentiometers if connected
    float timbre = analogRead(A0) / 1023.0;
    float color = analogRead(A1) / 1023.0;
    float freqMod = analogRead(A2) / 1023.0;
    
    // Update parameters
    fluted.setTimbre(timbre);
    fluted.setColor(color);
    
    // Automatic melody player
    unsigned long currentTime = millis();
    
    if (!noteIsOn && (currentTime - lastNoteTime > 600)) {
        // Play next note
        float baseFreq = notes[notePattern % numNotes];
        currentFreq = baseFreq * (0.8 + freqMod * 0.4); // Freq mod range: 0.8x to 1.2x
        
        fluted.noteOn(currentFreq);
        noteIsOn = true;
        lastNoteTime = currentTime;
        notePattern++;
        
        // Print current settings
        Serial.print("Note: ");
        Serial.print(currentFreq, 2);
        Serial.print(" Hz | Timbre: ");
        Serial.print(timbre, 2);
        Serial.print(" | Color: ");
        Serial.println(color, 2);
        
    } else if (noteIsOn && (currentTime - lastNoteTime > 400)) {
        // Note off
        fluted.noteOff();
        noteIsOn = false;
        lastNoteTime = currentTime;
    }
    
    delay(10);
}
