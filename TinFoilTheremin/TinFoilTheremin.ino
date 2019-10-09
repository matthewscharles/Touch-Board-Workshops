/*******************************************************************************

Tin Foil Theremin
by Charles Matthews

COMPILE FOR BARE CONDUCTIVE TOUCH BOARD USB MIDI, iPAD COMPATIBLE
yes, you heard me right..yes, the on board MIDI will still work :P
haven't blown anything up yet
not tonight anyway

This is from my goto workshop kit, and therefore a little bit idiosyncratic..
For now, it creates twelve channels of continuous sound, with volume changed continuously by the inputs.
Best used with some tinfoil ;)

Experimental version with added instrument changing function --
temporarily connect ground to pin 7 to cycle through instruments.

Some of this code refers to my old SD-based preset system, which I never quite got working.
Create some blank text files called NOTES.TXT and INSTS.TXT

 ---
 Adapted from Bare Conductive code written by Stefan Dzisiewski-Smith and Peter Krige,
 based on code code from Nathan Seidle.

 (^^I owe all of you cold beers. Let's go drinking some time) -CM

 This work is licensed under a Creative Commons Attribution-ShareAlike 3.0
 Unported License (CC BY-SA 3.0) http://creativecommons.org/licenses/by-sa/3.0/

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.

*******************************************************************************/
// from proximity code
// mapping and filter definitions
#define LOW_DIFF 0
#define HIGH_DIFF 60
#define filterWeight 0.3f // 0.0f to 1.0f - higher value = more smoothing
//float lastProx = 0;

// the electrode to monitor
#define ELECTRODE 0

// compiler error handling
#include "Compiler_Errors.h"

// serial rate
#define baudRate 57600

// include the relevant libraries
#include <MPR121.h>
#include <Wire.h>
#include <SoftwareSerial.h>

//dropping in some USB MIDI bits, because why not -CM
// #include "Midi_object.h"
//because this library isn't in the original package
 MIDIEvent e;

//and now let's make an array of MIDI objects
#define numElectrodes 12 //spoiler alert..
// midi_object_t MIDIobjects[numElectrodes]; // create an array of MIDI objects to use (one for each electrode)
// nah - let's just make it an int array. can't be bothered to look up why this library wasn't included
// but it's all cool
int lastCC[12];
int lastSensor[12];

float lastProx[12];
bool optionFlag = false;
int instrumentIndex = 0;

//set bend?
int bendFlag = false;

SoftwareSerial mySerial(12, 10); // Soft TX on 10, we don't use RX in this code

// Touch Board Setup variables
#define firstPin 0
#define lastPin 11

#define optionPin 7

// VS1053 setup
byte note = 0; // The MIDI note value to be played
byte resetMIDI = 8; // Tied to VS1053 Reset line
byte ledPin = 13; // MIDI traffic inidicator
int  instrument = 0;

// key definitions
const byte whiteNotes[] = {60, 62, 64, 65, 67, 69, 71, 72, 74, 76, 77, 79, 81};
const byte penta[] = {60, 62, 64, 67, 69, 72, 74, 76, 79, 81, 84, 86};
const byte allNotes[] = {60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71};

const byte channels[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12};

//let's chuck a few more scale options in -CM
const int MAJOR[7] = {0, 2, 4, 5, 7, 9, 11};
const int MINOR[7] = {0, 2, 3, 5, 7, 9, 11};
//modes
const int IONIAN[7] = {0, 2, 4, 5, 7, 9, 11};
const int DORIAN[7] = {0, 2, 3, 5, 7, 8, 11};
const int PHRYGIAN[7] = {0, 1, 3, 5, 6, 8, 11};
const int LYDIAN[7] = {0, 2, 4, 6, 7, 9, 11};
const int MIXOLYDIAN[7] = {0, 2, 4, 5, 7, 8, 11};
const int AEOLIAN[7] = {0, 2, 3, 5, 6, 8, 11};
const int LOCRIAN[7] = {0, 1, 2, 5, 6, 7, 11};


void setup(){


  Serial.begin(baudRate);

  // Setup soft serial for MIDI control
  mySerial.begin(31250);
  Wire.begin();

  // 0x5C is the MPR121 I2C address on the Bare Touch Board
  if(!MPR121.begin(0x5C)){
    Serial.println("error setting up MPR121");
    switch(MPR121.getError()){
      case NO_ERROR:
        Serial.println("no error");
        break;
      case ADDRESS_UNKNOWN:
        Serial.println("incorrect address");
        break;
      case READBACK_FAIL:
        Serial.println("readback failure");
        break;
      case OVERCURRENT_FLAG:
        Serial.println("overcurrent on REXT pin");
        break;
      case OUT_OF_RANGE:
        Serial.println("electrode out of range");
        break;
      case NOT_INITED:
        Serial.println("not initialised");
        break;
      default:
        Serial.println("unknown error");
        break;
    }
    while(1);
  }

  // pin 4 is the MPR121 interrupt on the Bare Touch Board
  MPR121.setInterruptPin(4);
  // initial data update
  MPR121.updateTouchData();

  // add some options
  pinMode(optionPin, INPUT_PULLUP);

  // Reset the VS1053
  pinMode(resetMIDI, OUTPUT);
  digitalWrite(resetMIDI, LOW);
  delay(100);
  digitalWrite(resetMIDI, HIGH);
  delay(100);

  // initialise MIDI
  setupMidi();
  setNotes();
  //huh, what's this then? -CM
  setUSBMIDI();

}

void setNotes(){
  //CM2019: modifying this bit in a hurry so this will be sloppy
  for (int i = 0; i < 12; i++) {
     talkMIDI(0xB0, 0, 0x00); // Default bank GM1
     talkMIDI(192 + channels[i] + (i % 3), instrumentIndex + 73, 0);
     talkMIDI(176 + channels[i], 0x07, 0); //0xB0 is channel message, set channel volume to max (127)
     noteOn(channels[i], allNotes[i] - (i % 3) * 12, 127);
  }
}

void options(){

  if (!optionFlag && digitalRead(optionPin) == 0) {
    //CM2019: modifying this bit in a hurry so this will be sloppy
    instrumentIndex += 1;
    instrumentIndex %=3;
    setNotes();
    optionFlag = false;
  } else if (optionPin == 1){
    optionFlag = true;
  }

}

//from 57 to 103

void loop(){

  options();
  MPR121.updateAll();

  for (int i = 0; i<12; i++){
    //I ditched the note ons, kinda boring for my purposes. this is modified from MIDI_interface_generic -CM
    int reading = MPR121.getBaselineData(i)-MPR121.getFilteredData(i);

    // constrain the reading between our low and high mapping values
    unsigned int prox = constrain(reading, LOW_DIFF, HIGH_DIFF);

    // implement a simple (IIR lowpass) smoothing filter
    lastProx[i] = (filterWeight*lastProx[i]) + ((1-filterWeight)*(float)prox);

    // map the LOW_DIFF..HIGH_DIFF range to 0..255 (8-bit resolution for analogWrite)
    uint8_t thisOutput = (uint8_t)map(lastProx[i],LOW_DIFF,HIGH_DIFF,0,127);

    Serial.println(thisOutput); //do we need this? leaving it in for now, might be useful -CM
    //set the volume on channel whatever -CM
      talkMIDI(176 + channels[i], 0x07, thisOutput); //0xB0 is channel message, set channel volume to max (127)

      //OK, now let's send this over the USB MIDI path as well -CM
      //just for a bit of continuity, I'll define these here from the "generic" code (and maybe better to move them up top)
      int inputMin = 520;
      int inputMax = 480;
      int outputMin = 0;
      int outputMax = 127; //turn it up to 11
      e.m3 = (unsigned char)constrain(map(MPR121.getFilteredData(i), inputMin, inputMax, outputMin, outputMax), 0, 127);


    if(e.m3!=lastCC[i]){ // only output a new controller value if it has changed since last time

      lastCC[i]=e.m3;

      e.type = 0x08;
      e.m1 = 0xB0; // control change message
      e.m2 = i + 102;     // select the correct controller number - you should use numbers
                                                  // between 102 and 119 unless you know what you are doing
                                                  // I mean, I kind of do I guess, but I like these numbers -CM
      MIDIUSB.write(e);

      //another quick test for pitch bend - shouldn't be in the main flow of things



        if(bendFlag) {

          talkMIDI(224+i, 0x65, constrain(e.m3 + random(0), 0, 127));


              }

    }
  }
//quick test on analog reads -- if I've committed this, I'm a bad person.
    for (int i = 0; i<6; i++){
//      e.type = 0x08;
//      e.m1 = analogRead(A0 + i) / 8; // control change message
//      e.m2 = i + 102;     // select the correct controller number - you should use numbers
//                                                  // between 102 and 119 unless you know what you are doing
//                                                  // I mean, I kind of do I guess, but I like these numbers -CM
//      MIDIUSB.write(e);
  int scaled = constrain(127 - ((analogRead(A0 + i) + 300) / 8), 0, 127);
//   talkMIDI(224+i, 0x65, constrain(scaled + random(0), 0, 127));
  talkMIDI(176 + channels[i], 0x07, scaled); //0xB0 is channel message, set channel volume to max (127)
  //need to work out what these objects do -- set them to pitch bend for now?

  // e.m3 = (unsigned char)constrain(map(MPR121.getFilteredData(i), inputMin, inputMax, outputMin, outputMax), 0, 127);
  if(scaled!=lastSensor[i]){ // only output a new controller value if it has changed since last time
  e.m3 = scaled;
  e.type = 0x08;
  e.m1 = 0xB0; // control change message
  e.m2 = i;     // select the correct controller number - you should use numbers
                                              // between 102 and 119 unless you know what you are doing
                                              // I mean, I kind of do I guess, but I like these numbers -CM
  MIDIUSB.write(e);
  lastSensor[i] = scaled;
    }
}
}


// functions below are little helpers based on using the SoftwareSerial
// as a MIDI stream input to the VS1053 - all based on stuff from Nathan Seidle
// (woohoo)

// Send a MIDI note-on message.  Like pressing a piano key.
// channel ranges from 0-15
void noteOn(byte channel, byte note, byte attack_velocity) {
  talkMIDI( (0x90 | channel), note, attack_velocity);
}

// Send a MIDI note-off message.  Like releasing a piano key.
void noteOff(byte channel, byte note, byte release_velocity) {
  talkMIDI( (0x80 | channel), note, release_velocity);
}

// Sends a generic MIDI message. Doesn't check to see that cmd is greater than 127,
// or that data values are less than 127.
void talkMIDI(byte cmd, byte data1, byte data2) {
  digitalWrite(ledPin, HIGH);
  mySerial.write(cmd);
  mySerial.write(data1);

  // Some commands only have one data byte. All cmds less than 0xBn have 2 data bytes
  // (sort of: http://253.ccarh.org/handout/midiprotocol/)
  if( (cmd & 0xF0) <= 0xB0)
    mySerial.write(data2);

  digitalWrite(ledPin, LOW);
}

//Let's set up some continuous MIDI controls over the USB connection -CM
void setUSBMIDI(){
//  //this is modified from MIDI_interface_generic in the original TB package
//  //but actually, might not use it after all, seems to belong to a different library.
//  //these scaling values are fixed..I'll map them above.
//  for (int i = 0; i < 12; i++) {
//    MIDIobjects[i].type = MIDI_CONTROL;
//    MIDIobjects[i].controllerNumber = 102; // 102..119 are undefined in the MIDI specification
//    //Ultimately I might like to keep these on different channels, but keeping the original mapping for compatibility -CM
//    MIDIobjects[i].inputMin = 520;  // note than inputMin is greater than inputMax here
//                                    // this means that the closer your hand is to the sensor
//                                    // the higher the output value will be
//                                    // to reverse the mapping, make inputMax greater than inputMin
//    MIDIobjects[i].inputMax = 480;  // the further apart the inputMin and inputMax are from each other
//                                    // the larger of a range the sensor will work over
//    MIDIobjects[i].outputMin = 0;   // minimum output to controller - smallest valid value is 0
//    MIDIobjects[i].outputMax = 127; // maximum output to controller - largest valid value is 127
//  }

}

// SETTING UP THE INSTRUMENT:
// The below function "setupMidi()" is where the instrument bank is defined. Use the VS1053 instrument library
// below to aid you in selecting your desire instrument from within the respective instrument bank


void setupMidi(){

  // Volume - don't comment out this code!
  talkMIDI(0xB0, 0x07, 127); //0xB0 is channel message, set channel volume to max (127)
  for (int i = 0; i < 16; i++) {
    talkMIDI(176 + channels[i], 0x07, 0);
    //RPN controller
    talkMIDI(176 + channels[i], 0x65, 00);
  talkMIDI(176 + channels[i], 0x64, 00);

  // set the semitone limits
  talkMIDI(176 + channels[i], 0x06, 12); // 24 should give me a full octave? too much?

  // set the cents limits (fine-tuning)
  //talkMIDI(0xB0|channel, 0x26, 00);

  // reset the RPN controller
  talkMIDI(176 + channels[i], 0x65, 127);
  talkMIDI(176 + channels[i], 0x64, 127);
  }

  // ---------------------------------------------------------------------------------------------------------
  // Melodic Instruments GM1
  // ---------------------------------------------------------------------------------------------------------
  // To Play "Electric Piano" (5):
  talkMIDI(0xB0, 0, 0x00); // Default bank GM1
  // We change the instrument by changing the middle number in the brackets
  // talkMIDI(0xC0, number, 0); "number" can be any number from the melodic table below
  talkMIDI(0xC0, 17, 0); // Set instrument number. 0xC0 is a 1 data byte command(55,0)
  // ---------------------------------------------------------------------------------------------------------
  // Percussion Instruments (Drums, GM1 + GM2)
  // ---------------------------------------------------------------------------------------------------------
  // uncomment the two lines of code below to use - you will also need to comment out the two "talkMIDI" lines
  // of code in the Melodic Instruments section above
  // talkMIDI(0xB0, 0, 0x78); // Bank select: drums
  // talkMIDI(0xC0, 0, 0); // Set a dummy instrument number
  // ---------------------------------------------------------------------------------------------------------

}

/* MIDI INSTRUMENT LIBRARY:

MELODIC INSTRUMENTS (GM1)
When using the Melodic bank (0x79 - same as default), open chooses an instrument and the octave to map
To use these instruments below change "number" in talkMIDI(0xC0, number, 0) in setupMidi()
NB - these are offset by 1. CM

1   Acoustic Grand Piano       33  Acoustic Bass             65  Soprano Sax           97   Rain (FX 1)
2   Bright Acoustic Piano      34  Electric Bass (finger)    66  Alto Sax              98   Sound Track (FX 2)
3   Electric Grand Piano       35  Electric Bass (pick)      67  Tenor Sax             99   Crystal (FX 3)
4   Honky-tonk Piano           36  Fretless Bass             68  Baritone Sax          100  Atmosphere (FX 4)
5   Electric Piano 1           37  Slap Bass 1               69  Oboe                  101  Brigthness (FX 5)
6   Electric Piano 2           38  Slap Bass 2               70  English Horn          102  Goblins (FX 6)
7   Harpsichord                39  Synth Bass 1              71  Bassoon               103  Echoes (FX 7)
8   Clavi                      40  Synth Bass 2              72  Clarinet              104  Sci-fi (FX 8)
9   Celesta                    41  Violin                    73  Piccolo               105  Sitar
10  Glockenspiel               42  Viola                     74  Flute                 106  Banjo
11  Music Box                  43  Cello                     75  Recorder              107  Shamisen
12  Vibraphone                 44  Contrabass                76  Pan Flute             108  Koto
13  Marimba                    45  Tremolo Strings           77  Blown Bottle          109  Kalimba
14  Xylophone                  46  Pizzicato Strings         78  Shakuhachi            110  Bag Pipe
15  Tubular Bells              47  Orchestral Harp           79  Whistle               111  Fiddle
16  Dulcimer                   48  Trimpani                  80  Ocarina               112  Shanai
17  Drawbar Organ              49  String Ensembles 1        81  Square Lead (Lead 1)  113  Tinkle Bell
18  Percussive Organ           50  String Ensembles 2        82  Saw Lead (Lead)       114  Agogo
19  Rock Organ                 51  Synth Strings 1           83  Calliope (Lead 3)     115  Pitched Percussion
20  Church Organ               52  Synth Strings 2           84  Chiff Lead (Lead 4)   116  Woodblock
21  Reed Organ                 53  Choir Aahs                85  Charang Lead (Lead 5) 117  Taiko
22  Accordion                  54  Voice oohs                86  Voice Lead (Lead)     118  Melodic Tom
23  Harmonica                  55  Synth Voice               87  Fifths Lead (Lead 7)  119  Synth Drum
24  Tango Accordion            56  Orchestra Hit             88  Bass + Lead (Lead 8)  120  Reverse Cymbal
25  Acoustic Guitar (nylon)    57  Trumpet                   89  New Age (Pad 1)       121  Guitar Fret Noise
26  Acoutstic Guitar (steel)   58  Trombone                  90  Warm Pad (Pad 2)      122  Breath Noise
27  Electric Guitar (jazz)     59  Tuba                      91  Polysynth (Pad 3)     123  Seashore
28  Electric Guitar (clean)    60  Muted Trumpet             92  Choir (Pad 4)         124  Bird Tweet
29  Electric Guitar (muted)    61  French Horn               93  Bowed (Pad 5)         125  Telephone Ring
30  Overdriven Guitar          62  Brass Section             94  Metallic (Pad 6)      126  Helicopter
31  Distortion Guitar          63  Synth Brass 1             95  Halo (Pad 7)          127  Applause
32  Guitar Harmonics           64  Synth Brass 2             96  Sweep (Pad 8)         128  Gunshot

PERCUSSION INSTRUMENTS (GM1 + GM2)

When in the drum bank (0x78), there are not different instruments, only different notes.
To play the different sounds, select an instrument # like 5, then play notes 27 to 87.

27  High Q                     43  High Floor Tom            59  Ride Cymbal 2         75  Claves
28  Slap                       44  Pedal Hi-hat [EXC 1]      60  High Bongo            76  Hi Wood Block
29  Scratch Push [EXC 7]       45  Low Tom                   61  Low Bongo             77  Low Wood Block
30  Srcatch Pull [EXC 7]       46  Open Hi-hat [EXC 1]       62  Mute Hi Conga         78  Mute Cuica [EXC 4]
31  Sticks                     47  Low-Mid Tom               63  Open Hi Conga         79  Open Cuica [EXC 4]
32  Square Click               48  High Mid Tom              64  Low Conga             80  Mute Triangle [EXC 5]
33  Metronome Click            49  Crash Cymbal 1            65  High Timbale          81  Open Triangle [EXC 5]
34  Metronome Bell             50  High Tom                  66  Low Timbale           82  Shaker
35  Acoustic Bass Drum         51  Ride Cymbal 1             67  High Agogo            83 Jingle bell
36  Bass Drum 1                52  Chinese Cymbal            68  Low Agogo             84  Bell tree
37  Side Stick                 53  Ride Bell                 69  Casbasa               85  Castanets
38  Acoustic Snare             54  Tambourine                70  Maracas               86  Mute Surdo [EXC 6]
39  Hand Clap                  55  Splash Cymbal             71  Short Whistle [EXC 2] 87  Open Surdo [EXC 6]
40  Electric Snare             56  Cow bell                  72  Long Whistle [EXC 2]
41  Low Floor Tom              57  Crash Cymbal 2            73  Short Guiro [EXC 3]
42  Closed Hi-hat [EXC 1]      58  Vibra-slap                74  Long Guiro [EXC 3]

*/
