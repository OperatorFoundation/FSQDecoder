#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <cmath>
#include <math.h>

#define LED_PIN    13
#define OLED_RESET -1

const int myInput = AUDIO_INPUT_LINEIN;
//const int myInput = AUDIO_INPUT_MIC;

// Create the Audio components.  These should be created in the
// order data flows, inputs/sources -> processing -> outputs
//
// ******* inputs/sources *******
AudioInputUSB           usb;
AudioInputI2S           audioInput; // audio shield: mic or line-in
AudioSynthWaveformSine  sinewave;
AudioSynthWaveform      wave;
AudioSynthToneSweep     sweep;
AudioSynthKarplusStrong string;
AudioSynthNoiseWhite    noise;
//
// ******* processing *******

AudioAnalyzeFFT1024     myFFT; // Fast Fourier Transform
//
// ******* outputs *******
AudioOutputI2S          audioOutput; // audio shield: headphones & line-out

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

void setup() 
{
  Serial.begin(9600);
    
  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(12);

  // enableAudioShield();

  // Configure the window algorithm to use
  myFFT.windowFunction(AudioWindowHanning1024);

  // playSyntheticWave();

  Serial.println("FSQDecoder Ready!");
  Serial.println();
}

// void loop()
// {
//   float n;
//   int i;

//   if (myFFT.available()) {
//     // each time new FFT data is available
//     // print it all to the Arduino Serial Monitor
//     Serial.print("FFT: ");
//     for (i=0; i<40; i++) {
//       n = myFFT.read(i);
//       if (n >= 0.01) {
//         Serial.print(n);
//         Serial.print(" ");
//       } else {
//         Serial.print("  -  "); // don't print "0.00"
//       }
//     }
//     Serial.println();
//   }
// }
void loop()
{
  float binValue;
  int binNumber;

  int notABin = -1;
  int loudestBin = -1;
  float loudestBinValue = 0.0;
  int loudestBinRepeats = 0;

  int confidenceMinRepeats = 5;
  int confidentToneA1 = -1;
  int confidentToneA2 = -1;
  int confidentToneB1 = -1;
  int confidentToneB1 = -1;

  if (myFFT.available())
  {
    // each time new FFT data is available
    // print it all to the Arduino Serial Monitor
    Serial.println("~~~~~~~~~~~~~~~~~~~~~~");
    Serial.println("myFFT is available");
    // Serial.write(0x1A);

    // TODO: How many times was a particular bin the loudest in a row?
    // TODO: When the tone changes check the counter to see if it reached a level of "confidence" (we will try 10 for a condfidence test for now**) before resetting to 0 
    // **Check FLDigi's FSQ decoder code for the correct value
    // TODO: Collect a pair of confident tones and discover the difference

    for (binNumber=0; binNumber<256; binNumber++) // 256 is the total number of bins
    {
      // blink LED
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);

      binValue = myFFT.read(binNumber);
      
      // Serial.write(0xDD);
      
      if (binValue >= 0.01)
      {
        Serial.println();
        Serial.println("Bin: " + String(binNumber));
        Serial.println("Value: " + String(binValue));
        Serial.println();

        // drawBin(binNumber, binValue);
        
        if (binValue > loudestBinValue) // This is the loudest we've seen so far
        {

          Serial.println("We found a new loudest bin: " + String(binNumber) + " value: " + String(binValue));

          // Did we have a previous loud bin?
          if (loudestBinRepeats > 0 && loudestBinValue > 0.01 && loudestBin != notABin) // We did
          {
            Serial.println("Previous loudest bin: " + String(loudestBin) + " value: " + String(loudestBinValue) + " repeats: " + String(loudestBinRepeats));

            // Is it the same bin?
            if (binNumber == loudestBin) // This is the same bin add a repeat
            {
              loudestBinRepeats += 1;
              Serial.println("It's the same bin. New repeat value: " + String(loudestBinRepeats));
            }
            else // This is a new loudest bin but we have one already, lets save the previous one if it is good enough
            {
              Serial.println("This is a new loudest bin!");

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
                Serial.println("The loudest bin has changed, but the previous bin did not repeat enough times: " + String(loudestBinRepeats));
                Serial.println("Discarding the previous bin tracking and monitoring the new bin.");
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
      // else
      // {
      //   drawBin(binNumber, 0);        
      // }
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

void enableAudioShield()
{
  // Enable the audio shield and set the output volume.
  audioShield.enable();
  audioShield.inputSelect(myInput);
  audioShield.volume(0.5);
}

void playSyntheticWave()
{
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
