#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <cmath>
#include <math.h>

#include "CRC8.h"
#include "FSQVaricode.h"
#include "FSQ.h"
//#include "RE.h"

#define LED_PIN    13
#define OLED_RESET -1

#define NIT std::string::npos

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
  Serial.begin(9600);
  delay(2000);

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

  prev_nibble = 0;
  curr_nibble = 0;

  delay(1000);
  initNibbles();
  delay(1000);

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
  }
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

void parse_rx_text()
{
	if (rx_text.empty()) return;

	if (rx_text.length() > 65536) 
  {
		rx_text.clear();
		return;
	}

  Serial.print("rx_text: ");
  Serial.println(String(rx_text.c_str()));
  Serial.println();
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
    
    int binDifference =  confidentBin2.binNumber - confidentBin1.binNumber;
    // Serial.print("Difference: ");
    // Serial.println(binDifference);
    processDifference(binDifference);
    // Serial.print(" Bits: ");
    // Serial.print();
    // Serial.print(" String: ");
    // Serial.print();

    // Cleanup
    confidentBin1 = notabin;
    confidentBin2 = notabin;
  }
}

bool valid_char(int ch)
{
	if ( ch ==  10 || ch == 163 || ch == 176 || ch == 177 || ch == 215 || ch == 247 || (ch > 31 && ch < 128))
		return true;
	return false;
}

void processDifference(int difference)
{  
  int nibble = difference;
	int curr_ch = -1;

	if (nibble < -99 || nibble > 99) 
  {
		Serial.println();
    Serial.print("DEBUG: processDifference() - invalid bin difference provided: ");
    Serial.println(difference);
		return;
	}

  int nibbleIndex = difference + 99;
	nibble = nibbles[nibbleIndex];

  // Serial.print("Nibble index (difference + 99) is: ");
  // Serial.println(nibbleIndex);
  // Serial.print("Nibble found at index: ");
  // Serial.println(nibble);

  // -1 is our idle difference, indicating we already have our difference
	if (nibble >= 0) 
  { 
    // process nibble
		curr_nibble = nibble;

		if ((prev_nibble < 29) & (curr_nibble < 29)) 
    {
      // single-nibble characters
			curr_ch = wsq_varidecode[prev_nibble];
      // Serial.print("Single nibble - curr_ch: ");
      // Serial.println(curr_ch);
		} 
    else if ( (prev_nibble < 29) && (curr_nibble > 28) && (curr_nibble < 32)) 
    {
      // double-nibble characters
			curr_ch = wsq_varidecode[prev_nibble * 32 + curr_nibble];
      // Serial.print("Double nibble - curr_ch: ");
      // Serial.println(curr_ch);
		}

		if (curr_ch > 0) 
    {
			if ( valid_char(curr_ch)) 
      {
				if (rx_text.length() > 32768) rx_text.clear();
        rx_text += fsq_ascii[curr_ch];

				parse_rx_text();
			}
      else 
      {
        Serial.println("Not a valid char");
      }
		}

		prev_nibble = curr_nibble;
	}
}

void initNibbles()
{
	int nibble = 0;
	for (int i = 0; i < 199; i++) 
  {
		nibble = floor(0.5 + (i - 99)/3.0);

		// allow for wrap-around (33 tones for 32 tone differences)
		if (nibble < 0) nibble += 33;
		if (nibble > 32) nibble -= 33;

		// adjust for +1 difference at the transmitter
		nibble--;
		nibbles[i] = nibble;
	}

  Serial.println("Nibble Table Created");
  Serial.println();
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
