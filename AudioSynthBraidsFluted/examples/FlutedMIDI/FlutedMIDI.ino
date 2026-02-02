/*
 * FlutedMIDI.ino
 * 
 * MIDI-controlled fluted oscillator example
 * 
 * Hardware:
 * - Teensy 3.2, 3.5, 3.6, 4.0, or 4.1
 * - Audio Shield or I2S DAC
 * - MIDI interface connected to Serial1 (RX=0, TX=1)
 *   or USB MIDI
 * 
 * MIDI CC Mappings:
 * - CC 1 (Mod Wheel): Timbre
 * - CC 74 (Brightness): Color
 * - CC 7 (Volume): Amplitude
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

// MIDI variables
byte midiChannel = 1;  // Listen to MIDI channel 1

void setup() {
    Serial.begin(115200);
    
    // Start MIDI on Serial1 at standard MIDI baud rate
    Serial1.begin(31250);
    
    // Audio connections require memory
    AudioMemory(10);
    
    // Enable the audio shield
    sgtl5000.enable();
    sgtl5000.volume(0.5);
    
    // Initialize the fluted oscillator
    fluted.begin();
    fluted.setAmplitude(0.8);
    fluted.setTimbre(0.5);
    fluted.setColor(0.7);
    
    Serial.println("MIDI Fluted Oscillator Ready");
    Serial.println("Listening on MIDI Channel 1");
}

void loop() {
    // Check for MIDI messages
    if (Serial1.available() >= 3) {
        byte status = Serial1.read();
        byte data1 = Serial1.read();
        byte data2 = Serial1.read();
        
        byte messageType = status & 0xF0;
        byte channel = (status & 0x0F) + 1;
        
        // Only respond to our MIDI channel
        if (channel == midiChannel) {
            handleMIDI(messageType, data1, data2);
        }
    }
    
    // Also check for USB MIDI
    if (usbMIDI.read()) {
        byte messageType = usbMIDI.getType();
        byte channel = usbMIDI.getChannel();
        
        if (channel == midiChannel) {
            byte data1 = usbMIDI.getData1();
            byte data2 = usbMIDI.getData2();
            handleUSBMIDI(messageType, data1, data2);
        }
    }
}

void handleMIDI(byte messageType, byte data1, byte data2) {
    switch (messageType) {
        case 0x90:  // Note On
            if (data2 > 0) {  // Velocity > 0
                float freq = midiNoteToFrequency(data1);
                fluted.noteOn(freq);
                Serial.print("Note On: ");
                Serial.print(data1);
                Serial.print(" (");
                Serial.print(freq, 2);
                Serial.println(" Hz)");
            } else {  // Velocity = 0 is Note Off
                fluted.noteOff();
                Serial.println("Note Off");
            }
            break;
            
        case 0x80:  // Note Off
            fluted.noteOff();
            Serial.println("Note Off");
            break;
            
        case 0xB0:  // Control Change
            handleControlChange(data1, data2);
            break;
    }
}

void handleUSBMIDI(byte messageType, byte data1, byte data2) {
    switch (messageType) {
        case usbMIDI.NoteOn:
            if (data2 > 0) {
                float freq = midiNoteToFrequency(data1);
                fluted.noteOn(freq);
                Serial.print("USB Note On: ");
                Serial.print(data1);
                Serial.print(" (");
                Serial.print(freq, 2);
                Serial.println(" Hz)");
            } else {
                fluted.noteOff();
                Serial.println("USB Note Off");
            }
            break;
            
        case usbMIDI.NoteOff:
            fluted.noteOff();
            Serial.println("USB Note Off");
            break;
            
        case usbMIDI.ControlChange:
            handleControlChange(data1, data2);
            break;
    }
}

void handleControlChange(byte controller, byte value) {
    float normalized = value / 127.0;
    
    switch (controller) {
        case 1:  // Mod Wheel - Timbre
            fluted.setTimbre(normalized);
            Serial.print("Timbre: ");
            Serial.println(normalized, 3);
            break;
            
        case 74:  // Brightness - Color
            fluted.setColor(normalized);
            Serial.print("Color: ");
            Serial.println(normalized, 3);
            break;
            
        case 7:  // Volume
            fluted.setAmplitude(normalized);
            Serial.print("Amplitude: ");
            Serial.println(normalized, 3);
            break;
    }
}

float midiNoteToFrequency(byte midiNote) {
    // MIDI note 69 = A4 = 440 Hz
    // Frequency = 440 * 2^((midiNote - 69) / 12)
    return 440.0 * pow(2.0, (midiNote - 69) / 12.0);
}
