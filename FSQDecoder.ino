#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <cmath>
#include <math.h>

#define OLED_RESET -1

const int myInput = AUDIO_INPUT_LINEIN;
//const int myInput = AUDIO_INPUT_MIC;

// Create the Audio components.  These should be created in the
// order data flows, inputs/sources -> processing -> outputs
//
AudioInputUSB          usb;
AudioInputI2S          audioInput;         // audio shield: mic or line-in
AudioSynthWaveformSine sinewave;
AudioSynthWaveform wave;
AudioSynthToneSweep sweep;
AudioSynthKarplusStrong  string;
AudioSynthNoiseWhite   noise;
AudioAnalyzeFFT1024    myFFT;
AudioOutputI2S         audioOutput;        // audio shield: headphones & line-out

// Connect either the live input or synthesized sine wave
AudioConnection patchCord1(usb, 0, myFFT, 0);
//AudioConnection patchCord2(usb, 0, audioOutput, 0);
//AudioConnection patchCord1(audioInput, 0, myFFT, 0);
//AudioConnection patchCord1(sinewave, 0, myFFT, 0);
//AudioConnection patchCord1(sweep, 0, myFFT, 0);
//AudioConnection patchCord1(wave, 0, myFFT, 0);
//AudioConnection patchCord1(string, 0, myFFT, 0);
//AudioConnection patchCord1(noise, 0, myFFT, 0);

AudioControlSGTL5000 audioShield;

void setup() {
  Serial.begin(9600);
    
  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(12);

  // Enable the audio shield and set the output volume.
  audioShield.enable();
  audioShield.inputSelect(myInput);
  audioShield.volume(0.5);

  // Configure the window algorithm to use
  myFFT.windowFunction(AudioWindowHanning1024);
  //myFFT.windowFunction(NULL);

  // Create a synthetic sine wave, for testing
  // To use this, edit the connections above
  sinewave.amplitude(0.8);
  sinewave.frequency(1034.007);

  wave.amplitude(0.8);
  wave.frequency(1034.007);

//  noise.amplitude(1.0);

  sweep.play(1.0, 0, 24000, 60);
  wave.begin(WAVEFORM_SAWTOOTH);

  string.noteOn(130.81, 1.0);
}

void loop()
{
  float n;
  int i;

  if (myFFT.available())
  {
    // each time new FFT data is available
    // print it all to the Arduino Serial Monitor

    Serial.write(0x1A);
    
    for (i=0; i<256; i++)
    {
      n = myFFT.read(i);

      Serial.write(0xDD);
      
      if (n >= 0.01)
      {
        drawBin(i, n);
      }
      else
      {
        drawBin(i, 0);        
      }
    }
  }
}

void drawBin(int i, float n)
{    
  union
  {
    float n;
    uint8_t bytes[4];
  } floatBytes;

  floatBytes.n = n;
  Serial.write(floatBytes.bytes[0]);
  Serial.write(floatBytes.bytes[1]);
  Serial.write(floatBytes.bytes[2]);
  Serial.write(floatBytes.bytes[3]);
}
