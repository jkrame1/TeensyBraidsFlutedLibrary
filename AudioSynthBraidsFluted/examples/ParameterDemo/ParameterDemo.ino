/*
 * ParameterDemo.ino
 * 
 * Demonstrates different sonic possibilities of the fluted oscillator
 * by cycling through various parameter presets
 * 
 * Each preset plays for a few seconds, demonstrating different sounds:
 * - Classic flute
 * - Breathy flute
 * - Clarinet-like
 * - Whistle/ethereal
 * - Percussive/plucked
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

// Preset structure
struct Preset {
    const char* name;
    float timbre;
    float color;
    float frequency;
    int duration;  // in milliseconds
};

// Define presets
const Preset presets[] = {
    {"Classic Flute", 0.70, 0.60, 523.25, 2000},        // C5
    {"Breathy Flute", 0.50, 0.40, 659.25, 2000},        // E5
    {"Clarinet-like", 0.88, 0.70, 261.63, 2000},        // C4
    {"Whistle/Ethereal", 0.92, 0.85, 880.00, 2000},     // A5
    {"Percussive/Plucked", 0.40, 0.80, 440.00, 800},    // A4 (short)
    {"Dark & Muted", 0.60, 0.20, 329.63, 2000},         // E4
    {"Bright & Clear", 0.75, 0.95, 698.46, 2000},       // F5
    {"Low & Resonant", 0.85, 0.50, 196.00, 2000},       // G3
};

const int numPresets = sizeof(presets) / sizeof(presets[0]);
int currentPreset = 0;
unsigned long lastChangeTime = 0;
bool noteIsPlaying = false;

void setup() {
    Serial.begin(115200);
    
    // Wait for serial connection (optional)
    delay(1000);
    
    // Audio setup
    AudioMemory(10);
    sgtl5000.enable();
    sgtl5000.volume(0.5);
    
    // Initialize fluted oscillator
    fluted.begin();
    fluted.setAmplitude(0.75);
    
    Serial.println("===========================================");
    Serial.println("Braids Fluted Oscillator - Parameter Demo");
    Serial.println("===========================================");
    Serial.println();
    Serial.println("This demo cycles through various presets");
    Serial.println("to showcase different sonic possibilities.");
    Serial.println();
    
    // Start with first preset
    loadPreset(0);
}

void loop() {
    unsigned long currentTime = millis();
    
    if (!noteIsPlaying) {
        // Start playing current preset
        const Preset& preset = presets[currentPreset];
        
        Serial.println("-------------------------------------------");
        Serial.print("Preset ");
        Serial.print(currentPreset + 1);
        Serial.print("/");
        Serial.print(numPresets);
        Serial.print(": ");
        Serial.println(preset.name);
        Serial.print("  Timbre: ");
        Serial.println(preset.timbre, 2);
        Serial.print("  Color:  ");
        Serial.println(preset.color, 2);
        Serial.print("  Freq:   ");
        Serial.print(preset.frequency, 2);
        Serial.println(" Hz");
        Serial.println();
        
        // Apply preset settings
        fluted.setTimbre(preset.timbre);
        fluted.setColor(preset.color);
        fluted.noteOn(preset.frequency);
        
        noteIsPlaying = true;
        lastChangeTime = currentTime;
        
    } else if (currentTime - lastChangeTime >= presets[currentPreset].duration) {
        // Note duration complete, turn off
        fluted.noteOff();
        noteIsPlaying = false;
        lastChangeTime = currentTime;
        
        // Move to next preset (with pause between)
        delay(500);
        currentPreset = (currentPreset + 1) % numPresets;
        
        // Add extra pause before looping
        if (currentPreset == 0) {
            Serial.println("===========================================");
            Serial.println("Cycling back to first preset...");
            Serial.println("===========================================");
            Serial.println();
            delay(1500);
        }
    }
}

void loadPreset(int index) {
    if (index >= 0 && index < numPresets) {
        currentPreset = index;
        const Preset& preset = presets[index];
        fluted.setTimbre(preset.timbre);
        fluted.setColor(preset.color);
    }
}
