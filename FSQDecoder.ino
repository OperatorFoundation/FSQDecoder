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

struct Bin {
  int binNumber;
  float value;
  int repeats;
};

struct Bin soundingBins[255];
int totalSoundingBins = 0;
struct Bin notabin = {-1, -1, 0};
struct Bin loudestBin = notabin;

struct Bin confidentBin1 = notabin;
struct Bin confidentBin2 = notabin;
struct Bin loudestBins[4];
int totalLoudestBins = 4;


void setup() 
{
  Serial.begin(2000000);
  delay(1000);

  loudestBins[0] = notabin;
  loudestBins[1] = notabin;
  loudestBins[2] = notabin;
  loudestBins[3] = notabin;

  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(12);

  // enableAudioShield();

  // Configure the window algorithm to use
  myFFT.windowFunction(AudioWindowHanning1024);

  // playSyntheticWave();
  Serial.println();
  Serial.println("♫⋆｡♪ ₊˚♬ ﾟ.");
  Serial.println("FSQDecoder Ready!");
  Serial.println("♫⋆｡♪ ₊˚♬ ﾟ.");
  Serial.println();
}

void loop()
{
  if (myFFT.available()) 
  {
    // each time new FFT data is available
    // print it all to the Arduino Serial Monitor
    // Serial.print("FFT: ");
    for (int binNumber=0; binNumber<256; binNumber++) 
    {
      float binValue = myFFT.read(binNumber);
      if (binValue >= 0.01) 
      {
        // Serial.print(binValue);
        // Serial.print(" ");

        struct Bin soundingBin = {binNumber, binValue, 1};

        // Save this bin in soundingBins
        soundingBins[totalSoundingBins] = soundingBin;
        totalSoundingBins += 1;
        // Serial.println();
        // Serial.println("Stored a new bin: ");
        // Serial.print("Bin number: "); Serial.println(soundingBin.binNumber);
        // Serial.print("Bin value: "); Serial.println(soundingBin.value);
        // Serial.print("Bin repeats: "); Serial.println(soundingBin.repeats);
        // Serial.print("Total sounding bins: "); Serial.println(totalSoundingBins);
        // Serial.println();
      } 
      else 
      {
        // Serial.print("  -  "); // don't print "0.00"
      }
    } // End bin loop (256 rounds)

    if (totalSoundingBins > 0)
    {
      // Loop through our saved bins for this round and find the loudest.
      for (int thisBin = 0; thisBin < totalSoundingBins; thisBin++)
      {
        if (soundingBins[thisBin].value > loudestBin.value)
        {
          loudestBin = soundingBins[thisBin];
        }
      }

      if (loudestBin.binNumber > 0)
      {
        handleLoudestBin(loudestBin);
      }
    }

    // Cleanup
    loudestBin = {-1, -1, 0};
    totalSoundingBins = 0;

  } // if FFT.available
}

void handleLoudestBin(struct Bin newLoudestBin)
{
  bool alreadyALoudestBin = false;

  for (int thisBin = 0; thisBin < totalLoudestBins; thisBin++)
  {
    if (newLoudestBin.binNumber == loudestBins[thisBin].binNumber)
    {
      alreadyALoudestBin = true;
      loudestBins[thisBin].repeats += 1;

      if (loudestBins[thisBin].repeats > 7)
      {
        handleConfidentBin(loudestBins[thisBin]);
        loudestBins[thisBin] = notabin;
      }

      return;
    }
  }

  if (!alreadyALoudestBin)
  {
    int emptyBinIndex = -1;

    for (int thisBin = 0; thisBin < totalLoudestBins; thisBin++)
    {
      if (loudestBins[thisBin].binNumber < 0)
      {
        emptyBinIndex = thisBin;
        break;
      }
    }
    
    if (emptyBinIndex < 0)
    {
      loudestBins[0] = notabin;
      loudestBins[1] = notabin;
      loudestBins[2] = notabin;
      loudestBins[3] = notabin;
    }
    else
    {
      // Check the previous bin to see if it has been repeated enough to be a confident bin
      if (emptyBinIndex > 0)
      {
        if (loudestBins[emptyBinIndex - 1].repeats > 7) // We have a bin that has repeated enough times.
        {
          handleConfidentBin(loudestBins[emptyBinIndex - 1]);
          loudestBins[emptyBinIndex - 1] = newLoudestBin;
        }
      }

      loudestBins[emptyBinIndex] = newLoudestBin;
    }
  }
}

void handleConfidentBin(struct Bin newConfidentBin)
{
  if (confidentBin1.binNumber == notabin.binNumber || confidentBin1.binNumber == newConfidentBin.binNumber)
  {
    confidentBin1 = newConfidentBin;
  }
  else if (confidentBin2.binNumber == notabin.binNumber || confidentBin2.binNumber == newConfidentBin.binNumber)
  {
    confidentBin2 = newConfidentBin;
    
    // Serial.println("We have two confident bin values.");
    // Serial.print("bin1: "); Serial.println(confidentBin1.binNumber); 
    // Serial.print("repeats: "); Serial.println(confidentBin1.repeats);
    // Serial.print("bin2: "); Serial.println(confidentBin2.binNumber); 
    // Serial.print("repeats: "); Serial.println(confidentBin2.repeats);
    int binDifference = confidentBin1.binNumber - confidentBin2.binNumber;
    // Serial.print("Difference: ");
    Serial.print(binDifference);
    // Serial.print(" Bits: ");
    // Serial.print();
    // Serial.print(" String: ");
    // Serial.print();

    // Cleanup
    confidentBin1 = notabin;
    confidentBin2 = notabin;
  }
}


// void loop()
// {
//   float binValue;
//   int binNumber;

//   int notABin = -1;
//   int loudestBin = -1;
//   float loudestBinValue = 0.0;
//   int loudestBinRepeats = 0;

//   int confidenceMinRepeats = 5;
//   int confidentToneA1 = -1;
//   int confidentToneA2 = -1;
//   int confidentToneB1 = -1;
//   int confidentToneB2 = -1;

//   if (myFFT.available())
//   {
//     // each time new FFT data is available
//     // print it all to the Arduino Serial Monitor
//     Serial.println("~~~~~~~~~~~~~~~~~~~~~~");
//     Serial.println("myFFT is available");
//     // Serial.write(0x1A);

//     // TODO: How many times was a particular bin the loudest in a row?
//     // TODO: When the tone changes check the counter to see if it reached a level of "confidence" (we will try 10 for a condfidence test for now**) before resetting to 0 
//     // **Check FLDigi's FSQ decoder code for the correct value
//     // TODO: Collect a pair of confident tones and discover the difference

//     for (binNumber=0; binNumber<256; binNumber++) // 256 is the total number of bins
//     {
//       // blink LED
//       digitalWrite(LED_PIN, HIGH);
//       delay(100);
//       digitalWrite(LED_PIN, LOW);
//       delay(100);

//       binValue = myFFT.read(binNumber);
      
//       // Serial.write(0xDD);
      
//       if (binValue >= 0.01)
//       {
//         Serial.println();
//         Serial.println("Bin: " + String(binNumber));
//         Serial.println("Value: " + String(binValue));
//         Serial.println();

//         // drawBin(binNumber, binValue);
        
//         if (binValue > loudestBinValue) // This is the loudest we've seen so far
//         {

//           Serial.println("We found a new loudest bin: " + String(binNumber) + " value: " + String(binValue));

//           // Did we have a previous loud bin?
//           if (loudestBinRepeats > 0 && loudestBinValue > 0.01 && loudestBin != notABin) // We did
//           {
//             Serial.println("Previous loudest bin: " + String(loudestBin) + " value: " + String(loudestBinValue) + " repeats: " + String(loudestBinRepeats));

//             // Is it the same bin?
//             if (binNumber == loudestBin) // This is the same bin add a repeat
//             {
//               loudestBinRepeats += 1;
//               Serial.println("It's the same bin. New repeat value: " + String(loudestBinRepeats));
//             }
//             else // This is a new loudest bin but we have one already, lets save the previous one if it is good enough
//             {
//               Serial.println("This is a new loudest bin!");

//               // Can we be confident that we have seen it enough times to save it to a pair?
//               if (loudestBinRepeats >= confidenceMinRepeats)
//               {
//                 if (confidentToneA1 == notABin) // A is empty (probably a first run)
//                 {
//                   confidentToneA1 = loudestBin;
//                 }
//                 else if (confidentToneA2 == notABin) 
//                 {
//                   confidentToneA2 = loudestBin;
//                 }
//                 else if (confidentToneB1 == notABin)
//                 {
//                   confidentToneB1 = loudestBin;
//                 }
//                 else if (confidentToneB2 == notABin) // B is empty
//                 {
//                   confidentToneB2 = loudestBin; // This means we now have a complete pair
//                   Serial.println("We found a new pair:");
//                   Serial.print("Tone A1: "); Serial.println(confidentToneA1);
//                   Serial.print("Tone A2: "); Serial.println(confidentToneA2);
//                   Serial.print("Tone B1: "); Serial.println(confidentToneB1);
//                   Serial.print("Tone B2: "); Serial.println(confidentToneB2);
//                 }
//                 else // We have a previous AB pair, starting over
//                 {
//                   confidentToneA1 = loudestBin;
//                   confidentToneA2 = notABin;
//                   confidentToneB1 = notABin;
//                   confidentToneB2 = notABin;
//                 }
//               }
//               else 
//               {
//                 Serial.println("The loudest bin has changed, but the previous bin did not repeat enough times: " + String(loudestBinRepeats));
//                 Serial.println("Discarding the previous bin tracking and monitoring the new bin.");
//                 loudestBin = binNumber;
//                 loudestBinValue = binValue;
//                 loudestBinRepeats = 1;
//               }
//             }
            
//           }
//           else // This is our first loudest bin
//           {
//             loudestBin = binNumber;
//             loudestBinValue = binValue;
//             loudestBinRepeats += 1;
//           }          
//         }
//         else if (binValue == loudestBinValue)
//         {
//           loudestBinRepeats += 1;
//         }
//       }
//       // else
//       // {
//       //   drawBin(binNumber, 0);        
//       // }
//     }
//   }
// }

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
