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
  float binValue;
  int binNumber;

  int notABin = 300;
  int loudestBin = 300;
  float loudestBinValue = 0.0;
  int loudestBinRepeats = 0;

  int confidenceMinRepeats = 10;
  int confidentToneA = 300;
  int confidentToneB = 300;

  // TODO: How many times was a particular bin the loudest in a row
  // TODO: When the tone changes check the counter to see if it reached a level of "confidence" (we will try 10 for a condfidence test for now**) before resetting to 0 
  // **Check FLDigi's FSQ decoder code for the correct value
  // TODO: Collect a pair of confident tones and discover the difference

  if (myFFT.available())
  {
    // each time new FFT data is available
    // print it all to the Arduino Serial Monitor

    Serial.write(0x1A);
    
    for (binNumber=0; binNumber<256; binNumber++) // 256 is the total number of bins
    {
      binValue = myFFT.read(binNumber);

      Serial.write(0xDD);
      
      // TODO: Which is loudest (biggest value of n of all of the 256 bins) bin number 

      if (binValue >= 0.01)
      {
        drawBin(binNumber, binValue);

        
        if (binValue > loudestBinValue) // This is the loudest we've seen so far
        {
          // Did we have a previous loud bin?
          if (loudestBinRepeats > 0 && loudestBinValue > 0.0 && loudestBin != notABin) // We did
          {
            // Is it the same bin?
            if (binNumber == loudestBin) // This is the same bin add a repeat
            {
              loudestBinRepeats += 1;
            }
            else // This is a new loudest bin but we have one already, lets save the previous one if it is good enough
            {
              // Can we be confident that we have seen it enough times to save it to a pair?
              if (loudestBinRepeats >= confidenceMinRepeats)
              {
                if (confidentToneA == notABin) // A is empty (probably a first run)
                {
                  confidentToneA = loudestBin;
                }
                else if (confidentToneB == notABin) // B is empty
                {
                  confidentToneB = loudestBin; // This means we now have a complete pair
                  Serial.println("We found a new pair:");
                  Serial.print("Tone A: "); Serial.println(confidentToneA);
                  Serial.print("Tone B: "); Serial.println(confidentToneB);
                }
                else // We have a previous AB pair, starting over
                {
                  confidentToneA = loudestBin;
                  confidentToneB = notABin;
                }
              }
              else 
              {
                Serial.println("The loudest bin has changed, but the previous bin did not repeat enough times. Discarding the previous bin tracking and monitoring the new bin.");
                loudestBin = binNumber;
                loudestBinValue = binValue;
                loudestBinRepeats = 1;
              }
            }
            
          }
          else // This is our first loudest bin
          {
            loudestBin = binNumber;
            loudestBinValue = binValue;
            loudestBinRepeats += 1;
          }          
        }
        else if (binValue == loudestBinValue)
        {
          loudestBinRepeats += 1;
        }
      }
      else
      {
        drawBin(binNumber, 0);        
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
