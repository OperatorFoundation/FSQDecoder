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

static const char *FSQBOL = " \n";
static const char *FSQEOL = "\n ";
static const char *FSQEOT = "  \b  ";
static std::string triggers = " !#$%&'()*+,-.;<=>?@[\\]^_{|}~";
static std::string allcall = "allcall";
static std::string cqcqcq = "cqcqcq";
// static fre_t call("([[:alnum:]]?[[:alpha:]/]+[[:digit:]]+[[:alnum:]/]+)", REG_EXTENDED);

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
  ch_sqlch_open = false;
  b_bot = false;
  b_eol = false;
  b_eot = false;

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

  }// if FFT.available
//  else
//  {
//    Serial.println("No FFT data available.");
//    delay(5000);
//    return;
//  }
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

// test for valid callsign
// returns:
// 0 - not a callsign
// 1 - mycall
// 2 - allcall
// 4 - cqcqcq
// 8 - any other valid call
int valid_callsign(std::string s)
{
	if (s.length() < 3) return 0;
	if (s.length() > 20) return 0;

	if (s == allcall) return 2;
	if (s == cqcqcq) return 4;
	// if (s == mycall) return 1;
	if (s.find("Heard") != std::string::npos) return 0;

  // FIXME:
	// static char sz[21];
	// memset(sz, 0, 21);
	// strcpy(sz, s.c_str());
	// bool matches = call.match(sz);
	// return (matches ? 8 : 0);
  return 8;
}

void parse_rx_text()
{
	// char ztbuf[20];
	// struct timeval tv;
	// gettimeofday(&tv, NULL);
	// struct tm tm;
	// time_t t_temp;
	// t_temp=(time_t)tv.tv_sec;
	// gmtime_r(&t_temp, &tm);
	// strftime(ztbuf, sizeof(ztbuf), "%Y%m%d,%H%M%S", &tm);

	toprint.clear();

	if (rx_text.empty()) return;
	if (rx_text.length() > 65536) 
  {
		rx_text.clear();
		return;
	}

	// state = TEXT;

	size_t p = rx_text.find(':');
	if (p == 0) 
  {
		rx_text.erase(0,1);
		return;
	}

	if (p == std::string::npos || rx_text.length() < p + 2) 
  {
		return;
	}

	std::string rxcrc = rx_text.substr(p+1,2);

	int max = p+1;
	if (max > 20) max = 20;
	std::string substr;

	for (int i = 1; i < max; i++) 
  {
		if (rx_text[p-i] <= ' ' || rx_text[p-i] > 'z') 
    {
			rx_text.erase(0, p+1);
			return;
		}

		substr = rx_text.substr(p-i, i);

		if ((crc.sval(substr) == rxcrc) && valid_callsign(substr)) 
    {
			station_calling = substr;
			break;
		}
	}

	// if (station_calling == mycall) 
  // { 
  //   // do not display any of own rx stream
	// 	Serial.println("Station calling is mycall: %s", station_calling.c_str());
	// 	rx_text.erase(0, p+3);
	// 	return;
	// }

	// if (!station_calling.empty()) 
  // {
	// 	REQ(add_to_heard_list, station_calling, szestimate);
	// 	if (enable_heard_log) 
  //   {
	// 		std::string sheard = ztbuf;
	// 		sheard.append(",").append(station_calling);
	// 		sheard.append(",").append(szestimate).append("\n");
	// 		heard_log << sheard;
	// 		heard_log.flush();
	// 	}
	// }

  // remove station_calling, colon and checksum
	rx_text.erase(0, p+3);

  // extract all directed callsigns
  // look for 'allcall', 'cqcqcq' or mycall

	bool all = false;
	bool directed = false;

  // test next word in std::string
	size_t tr_pos = 0;
	char tr = rx_text[tr_pos];
	size_t trigger = triggers.find(tr);

  // strip any leading spaces before either text or first directed callsign

	while (rx_text.length() > 1 &&
		triggers.find(rx_text[0]) != std::string::npos)
		rx_text.erase(0,1);

  // find first word
	while ( tr_pos < rx_text.length() && ((trigger = triggers.find(rx_text[tr_pos])) == std::string::npos) ) 
  {
		tr_pos++;
	}

	while (trigger != std::string::npos && tr_pos < rx_text.length()) 
  {
		int word_is = valid_callsign(rx_text.substr(0, tr_pos));

		if (word_is == 0) 
    {
			rx_text.insert(0," ");
			break; // not a callsign
		}

		if (word_is == 1) 
    {
			directed = true; // mycall
		}
		// test for cqcqcq and allcall
		else if (word_is != 8)
    {
      all = true;
    }

		rx_text.erase(0, tr_pos);

		while (rx_text.length() > 1 && (rx_text[0] == ' ' && rx_text[1] == ' '))
    {
      rx_text.erase(0,1);
    }

		if (rx_text[0] != ' ') break;

		rx_text.erase(0, 1);

		tr_pos = 0;
		tr = rx_text[tr_pos];
		trigger = triggers.find(tr);

		while ( tr_pos < rx_text.length() && (trigger == std::string::npos) ) 
    {
			tr_pos++;
			tr = rx_text[tr_pos];
			trigger = triggers.find(tr);
		}
	}

	if ( (all == false) && (directed == false)) {
		rx_text.clear();
		return;
	}

  // remove eot if present
	if (rx_text.length() > 3) rx_text.erase(rx_text.length() - 3);

	toprint.assign(station_calling).append(":");

  // test for trigger
	tr = rx_text[0];
	trigger = triggers.find(tr);

	if (trigger == NIT) 
  {
		tr = ' '; // force to be text line
		rx_text.insert(0, " ");
	}

  // if asleep suppress all but the * trigger

	// if (btn_SELCAL->value() == 0) 
  // {
	// 	if (tr == '*') parse_star();
	// 	rx_text.clear();
	// 	return;
	// }

  // now process own call triggers
	// if (directed) 
  // {
	// 	switch (tr) 
  //   {
	// 		case ' ': parse_space(false);   break;
	// 		case '?': parse_qmark();   break;
	// 		case '*': parse_star();    break;
	// 		case '+': parse_plus();    break;
	// 		case '-': break;//parse_minus();   break;
	// 		case ';': parse_relay();    break;
	// 		case '!': parse_repeat();    break;
	// 		case '~': parse_delayed_repeat();   break;
	// 		case '#': parse_pound();   break;
	// 		case '$': parse_dollar();  break;
	// 		case '@': parse_at();      break;
	// 		case '&': parse_amp();     break;
	// 		case '^': parse_carat();   break;
	// 		case '%': parse_pcnt();    break;
	// 		case '|': parse_vline();   break;
	// 		case '>': parse_greater(); break;
	// 		case '<': parse_less();    break;
	// 		case '[': parse_relayed(); break;
	// 	}
	// }

  // // if allcall; only respond to the ' ', '*', '#', '%', and '[' triggers
	// else {
	// 	switch (tr) 
  //   {
	// 		case ' ': parse_space(true);   break;
	// 		case '*': parse_star();    break;
	// 		case '#': parse_pound();   break;
	// 		case '%': parse_pcnt();    break;
	// 		case '[': parse_relayed(); break;
	// 	}
	// }

  Serial.print("rx_text: ");
  Serial.println(String(rx_text.c_str()));
	rx_text.clear();
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

void lfCheck(int ch)
{
	static char lfpair[3] = "01";
	static char bstrng[4] = "012";

	lfpair[0] = lfpair[1];
	lfpair[1] = 0xFF & ch;

	bstrng[0] = bstrng[1];
	bstrng[1] = bstrng[2];
	bstrng[2] = 0xFF & ch;

	if (bstrng[0] == FSQEOT[0]    // find SP SP BS SP
		&& bstrng[1] == FSQEOT[1]
		&& bstrng[2] == FSQEOT[2]
		) {
		b_eot = true;
	} else if (lfpair[0] == FSQBOL[0] && lfpair[1] == FSQBOL[1]) {
		b_bot = true;
	} else if (lfpair[0] == FSQEOL[0] && lfpair[1] == FSQEOL[1]) {
		b_eol = true;
	}
}

bool fsq_squelch_open()
{
  // FIXME
	// return ch_sqlch_open || metric >= progStatus.sldrSquelchValue;
  return ch_sqlch_open;
}

// void write_rx_mon_char(int ch)
// {
// 	int ach = ch & 0xFF;
// 	if (!progdefaults.fsq_directed) 
//   {
// 		display_fsq_rx_text(fsq_ascii[ach], FTextBase::FSQ_UND);

// 		if (ach == '\n')
// 			display_fsq_rx_text(fsq_lf, FTextBase::FSQ_UND);
// 	}

// 	display_fsq_mon_text(fsq_ascii[ach], FTextBase::RECV);

// 	if (ach == '\n')
// 		display_fsq_mon_text(fsq_lf, FTextBase::RECV);
// }

bool valid_char(int ch)
{
	if ( ch ==  10 || ch == 163 || ch == 176 || ch == 177 || ch == 215 || ch == 247 || (ch > 31 && ch < 128))
		return true;
	return false;
}

// https://github.com/w1hkj/fldigi/blob/master/src/fsq/fsq.cxx
void processDifference(int difference)
{  
  int nibble = difference;
	int curr_ch = -1;

	if (nibble < -99 || nibble > 99) 
  {
		Serial.println();
    Serial.print("processDifference() - invalid bin difference provided: ");
    Serial.println(difference);
		return;
	}
  else
  {
    Serial.println();
    Serial.print("DEBUG: processDifference() - difference is: ");
    Serial.println(difference);
  }

	nibble = nibbles[nibble + 99];

  // -1 is our idle differencebol, indicating we already have our differencebol
	if (nibble >= 0) 
  { 
    // process nibble
		curr_nibble = nibble;

    
		if ((prev_nibble < 29) & (curr_nibble < 29)) 
    {
      // single-nibble characters
			curr_ch = wsq_varidecode[prev_nibble];
		} 
    else if ( (prev_nibble < 29) && (curr_nibble > 28) && (curr_nibble < 32)) 
    {
      // double-nibble characters
			curr_ch = wsq_varidecode[prev_nibble * 32 + curr_nibble];
		}
		if (curr_ch > 0) 
    {
			// if (enable_audit_log) 
      // {
			// 	audit_log << fsq_ascii[curr_ch];// & 0xFF];
			// 	if (curr_ch == '\n') audit_log << '\n';
			// 	audit_log.flush();
			// }

			lfCheck(curr_ch);

			if (b_bot) 
      {
				ch_sqlch_open = true;
				// rx_text.clear();
			}

			// if (fsq_squelch_open()) 
      // {
			// 	// write_rx_mon_char(curr_ch);
      //   Serial.print(curr_ch);
			// 	if (b_bot)
      //   {
      //     char ztbuf[20];
      //     struct timeval tv;
      //     gettimeofday(&tv,NULL);
      //     struct tm tm;
      //     time_t t_temp;
      //     t_temp=(time_t)tv.tv_sec;
      //     gmtime_r(&t_temp, &tm);
      //     strftime(ztbuf,sizeof(ztbuf),"%m/%d %H:%M:%S ",&tm);
      //     display_fsq_mon_text( ztbuf, FTextBase::CTRL);
      //     display_fsq_mon_text( fsq_bot, FTextBase::CTRL);
      //   }
			// 	if (b_eol) 
      //   {
			// 		display_fsq_mon_text( fsq_eol, FTextBase::CTRL);
			// 		noisefilt->reset();
			// 		noisefilt->run(1);
			// 		sigfilt->reset();
			// 		sigfilt->run(1);
			// 		snprintf(szestimate, sizeof(szestimate), "%.0f db", s2n );
			// 	}
			// 	if (b_eot) 
      //   {
			// 		snprintf(szestimate, sizeof(szestimate), "%.0f db", s2n );
			// 		noisefilt->reset();
			// 		noisefilt->run(1);
			// 		sigfilt->reset();
			// 		sigfilt->run(1);
			// 		display_fsq_mon_text( fsq_eot, FTextBase::CTRL);
			// 	}
			// }

			if ( valid_char(curr_ch) || b_eol || b_eot ) 
      {
				// if (rx_text.length() > 32768) rx_text.clear();

				// if ( fsq_squelch_open() || !progStatus.sqlonoff ) 
        if ( fsq_squelch_open())
        {
					rx_text += curr_ch;
					if (b_eot) 
          {
						parse_rx_text();
						// if (state == TEXT) ch_sqlch_open = false;
					}
				}
			}
      else 
      {
        Serial.println("Not a valid char");
      }


			if (fsq_squelch_open() && (b_eot || b_eol)) 
      {
				ch_sqlch_open = false;
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

		// adjust for +1 differencebol at the transmitter
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
