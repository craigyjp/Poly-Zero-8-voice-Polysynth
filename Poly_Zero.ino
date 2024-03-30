/*
  poly Zero - Firmware Rev 1.1

  Includes code by:
    Dave Benn - Handling MUXs, a few other bits and original inspiration  https://www.notesandvolts.com/2019/01/teensy-synth-part-10-hardware.html
    ElectroTechnique for general method of menus and updates.

  Arduino IDE
  Tools Settings:
  Board: "Teensy4,1"
  USB Type: "Serial + MIDI"
  CPU Speed: "600"
  Optimize: "Fastest"

  Performance Tests   CPU  Mem
  180Mhz Faster       81.6 44
  180Mhz Fastest      77.8 44
  180Mhz Fastest+PC   79.0 44
  180Mhz Fastest+LTO  76.7 44
  240MHz Fastest+LTO  55.9 44

  Additional libraries:
    Agileware CircularBuffer available in Arduino libraries manager
    Replacement files are in the Modified Libraries folder and need to be placed in the teensy Audio folder.
*/

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <MIDI.h>
#include <USBHost_t36.h>
#include "MidiCC.h"
#include "Constants.h"
#include "Parameters.h"
#include "PatchMgr.h"
#include "HWControls.h"
#include "EepromMgr.h"
#include <RoxMux.h>
#include <Adafruit_NeoPixel.h>

#define PARAMETER 0      //The main page for displaying the current patch and control (parameter) changes
#define RECALL 1         //Patches list
#define SAVE 2           //Save patch page
#define REINITIALISE 3   // Reinitialise message
#define PATCH 4          // Show current patch bypassing PARAMETER
#define PATCHNAMING 5    // Patch naming page
#define DELETE 6         //Delete patch page
#define DELETEMSG 7      //Delete patch message page
#define SETTINGS 8       //Settings page
#define SETTINGSVALUE 9  //Settings page

unsigned int state = PARAMETER;

char gateTrig[] = "TTTTTTTT";
float sfAdj[8];
#define NO_OF_VOICES 8

struct VoiceAndNote {
  int note;
  int velocity;
  long timeOn;
  bool sustained;  // Sustain flag
  bool keyDown;
  double noteFreq;  // Note frequency
  int position;
  bool noteOn;
};

struct VoiceAndNote voices[NO_OF_VOICES] = {
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false }
};

boolean voiceOn[NO_OF_VOICES] = { false, false, false, false, false, false, false, false };
int voiceToReturn = -1;        //Initialise to 'null'
long earliestTime = millis();  //For voice allocation - initialise to now
int prevNote = 0;              //Initialised to middle value
bool notes[128] = { 0 }, initial_loop = 1;
int8_t noteOrder[40] = { 0 }, orderIndx = { 0 };
int count = 0;  //For MIDI Clk Sync
int DelayForSH3 = 12;
int patchNo = 1;  //Current patch no

#include "ST7735Display.h"

boolean cardStatus = false;

//USB HOST MIDI Class Compliant
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
MIDIDevice midi1(myusb);

//MIDI 5 Pin DIN
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial8, MIDI2);

Adafruit_NeoPixel pixel(4, NEOPIXEL_OUTPUT, NEO_GRB + NEO_KHZ800);
int colour[][3] = {
  { 100, 0, 0 },  // red
  { 88, 12, 0 },  // orange
  { 65, 35, 0 },  // yellow
  { 0, 100, 0 },  // green
  { 0, 70, 20 },  // cyan
  { 0, 0, 100 },  // blue
  { 40, 0, 40 },  // magenta
  { 45, 40, 35 }  // white
};

#define OCTO_TOTAL 3
#define BTN_DEBOUNCE 50
RoxOctoswitch<OCTO_TOTAL, BTN_DEBOUNCE> octoswitch;

// pins for 74HC165
#define PIN_DATA 36  // pin 9 on 74HC165 (DATA)
#define PIN_LOAD 37  // pin 1 on 74HC165 (LOAD)
#define PIN_CLK 33   // pin 2 on 74HC165 (CLK))

#define SRP_TOTAL 4
Rox74HC595<SRP_TOTAL> srp;

// pins for 74HC595
#define LED_DATA 21   // pin 14 on 74HC595 (DATA)
#define LED_CLK 22    // pin 11 on 74HC595 (CLK)
#define LED_LATCH 23  // pin 12 on 74HC595 (LATCH)
#define LED_PWM -1    // pin 13 on 74HC595

#define SR_TOTAL 4
Rox74HC595<SR_TOTAL> sr;

// pins for 74HC595
#define CONTROL_DATA 6   // pin 14 on 74HC595 (DATA)
#define CONTROL_CLK 7    // pin 11 on 74HC595 (CLK)
#define CONTROL_LATCH 8  // pin 12 on 74HC595 (LATCH)
#define CONTROL_PWM -1   // pin 13 on 74HC595

#define TRIG_TOTAL 1
Rox74HC595<TRIG_TOTAL> trig;

// pins for 74HC595
#define TRIG_DATA 38   // pin 14 on 74HC595 (DATA)
#define TRIG_CLK 39    // pin 11 on 74HC595 (CLK)
#define TRIG_LATCH 40  // pin 12 on 74HC595 (LATCH)
#define TRIG_PWM -1    // pin 13 on 74HC595

byte ccType = 0;  //(EEPROM)

#include "Settings.h"


void setup() {
  SPI.begin();

  octoswitch.begin(PIN_DATA, PIN_LOAD, PIN_CLK);
  octoswitch.setCallback(onButtonPress);
  octoswitch.setIgnoreAfterHold(EFFECT_BANK_SW, true);
  octoswitch.setIgnoreAfterHold(LFO_WAVE_SW, true);
  srp.begin(LED_DATA, LED_LATCH, LED_CLK, LED_PWM);
  sr.begin(CONTROL_DATA, CONTROL_LATCH, CONTROL_CLK, CONTROL_PWM);
  trig.begin(TRIG_DATA, TRIG_LATCH, TRIG_CLK, TRIG_PWM);
  setupDisplay();
  setUpSettings();
  setupHardware();
  //setUpLEDS();

  cardStatus = SD.begin(BUILTIN_SDCARD);
  if (cardStatus) {
    Serial.println("SD card is connected");
    //Get patch numbers and names from SD card
    loadPatches();
    if (patches.size() == 0) {
      //save an initialised patch to SD card
      savePatch("1", INITPATCH);
      loadPatches();
    }
  } else {
    Serial.println("SD card is not connected or unusable");
    reinitialiseToPanel();
    showPatchPage("No SD", "conn'd / usable");
  }

  //Read MIDI Channel from EEPROM
  midiChannel = getMIDIChannel();
  Serial.println("MIDI Ch:" + String(midiChannel) + " (0 is Omni On)");

  //Read CC type from EEPROM
  ccType = getCCType();

  //Read UpdateParams type from EEPROM
  updateParams = getUpdateParams();

  //Read SendNotes type from EEPROM
  sendNotes = getSendNotes();

  for (int i = 0; i < 8; i++) {
    if ((sfAdj[i] < 0.9f) || (sfAdj[i] > 1.1f) || isnan(sfAdj[i])) sfAdj[i] = 1.0f;
  }

  //USB HOST MIDI Class Compliant
  delay(400);  //Wait to turn on USB Host
  myusb.begin();
  midi1.setHandleControlChange(myConvertControlChange);
  midi1.setHandleProgramChange(myProgramChange);
  midi1.setHandleNoteOff(myNoteOff);
  midi1.setHandleNoteOn(myNoteOn);
  midi1.setHandlePitchChange(myPitchBend);
  midi1.setHandleAfterTouch(myAfterTouch);
  Serial.println("USB HOST MIDI Class Compliant Listening");

  //USB Client MIDI
  usbMIDI.setHandleControlChange(myConvertControlChange);
  usbMIDI.setHandleProgramChange(myProgramChange);
  usbMIDI.setHandleNoteOff(myNoteOff);
  usbMIDI.setHandleNoteOn(myNoteOn);
  usbMIDI.setHandlePitchChange(myPitchBend);
  usbMIDI.setHandleAfterTouch(myAfterTouch);
  Serial.println("USB Client MIDI Listening");

  //MIDI 5 Pin DIN
  MIDI.begin();
  MIDI.setHandleControlChange(myConvertControlChange);
  MIDI.setHandleProgramChange(myProgramChange);
  MIDI.setHandleNoteOn(myNoteOn);
  MIDI.setHandleNoteOff(myNoteOff);
  MIDI.setHandlePitchBend(myPitchBend);
  MIDI.setHandleAfterTouchChannel(myAfterTouch);
  Serial.println("MIDI In DIN Listening");

  MIDI2.begin();
  for (int i = 1; i < 9; i++) {
    MIDI2.sendNoteOn(60, 127, i);
    delay(1);
    MIDI2.sendNoteOff(60, 0, i);
  }

  //Read Encoder Direction from EEPROM
  encCW = getEncoderDir();

  //Read MIDI Out Channel from EEPROM
  midiOutCh = getMIDIOutCh();

  //Read Modwheel Depth from EEPROM
  modWheelDepth = getModWheelDepth();
  if (modWheelDepth < 0 || modWheelDepth > 10) {
    storeModWheelDepth(0);
  }

  //Read aftertouch Dest from EEPROM
  AfterTouchDest = getAfterTouch();
  if (AfterTouchDest < 0 || AfterTouchDest > 4) {
    storeAfterTouch(0);
  }

  //Read aftertouch Depth from EEPROM
  afterTouchDepth = getafterTouchDepth();
  if (afterTouchDepth < 0 || afterTouchDepth > 10) {
    storeafterTouchDepth(0);
  }

  pixel.begin();
  pixel.setPixelColor(0, pixel.Color(colour[0][0], colour[0][1], colour[0][2]));
  pixel.setPixelColor(1, pixel.Color(colour[0][0], colour[0][1], colour[0][2]));
  pixel.setPixelColor(2, pixel.Color(colour[0][0], colour[0][1], colour[0][2]));
  pixel.setPixelColor(3, pixel.Color(colour[0][0], colour[0][1], colour[0][2]));
  pixel.show();

  sr.writePin(FILTER_KEYTRACK, HIGH);

  recallPatch(patchNo);  //Load first patch
}

void commandTopNote() {
  int topNote = 0;
  bool noteActive = false;

  for (int i = 0; i < 128; i++) {
    if (notes[i]) {
      topNote = i;
      noteActive = true;
    }
  }

  if (noteActive)
    commandNote(topNote);
  else  // All notes are off, turn off gate
    trig.writePin(GATE_NOTE1, LOW);
}

void commandBottomNote() {
  int bottomNote = 0;
  bool noteActive = false;

  for (int i = 127; i >= 0; i--) {
    if (notes[i]) {
      bottomNote = i;
      noteActive = true;
    }
  }

  if (noteActive)
    commandNote(bottomNote);
  else  // All notes are off, turn off gate
    trig.writePin(GATE_NOTE1, LOW);
}

void commandLastNote() {

  int8_t noteIndx;

  for (int i = 0; i < 40; i++) {
    noteIndx = noteOrder[mod(orderIndx - i, 40)];
    if (notes[noteIndx]) {
      commandNote(noteIndx);
      return;
    }
  }
  trig.writePin(GATE_NOTE1, LOW);  // All notes are off
}

void commandNote(int noteMsg) {
  unsigned int mV = (unsigned int)((float)(noteMsg + realoctave) * NOTE_SF * sfAdj[0] + 0.5);
  sample_data = (channel_a & 0xFFF0000F) | (((int(mV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data);
  MIDI2.sendNoteOn(noteMsg, 127, 1);
  trig.writePin(GATE_NOTE1, HIGH);
}

void commandTopNoteUni() {
  int topNote = 0;
  bool noteActive = false;

  for (int i = 0; i < 128; i++) {
    if (notes[i]) {
      topNote = i;
      noteActive = true;
    }
  }

  if (noteActive) {
    commandNoteUni(topNote);
  } else {  // All notes are off, turn off gate
    trig.writePin(GATE_NOTE1, LOW);
    trig.writePin(GATE_NOTE2, LOW);
    trig.writePin(GATE_NOTE3, LOW);
    trig.writePin(GATE_NOTE4, LOW);
    trig.writePin(GATE_NOTE5, LOW);
    trig.writePin(GATE_NOTE6, LOW);
    trig.writePin(GATE_NOTE7, LOW);
    trig.writePin(GATE_NOTE8, LOW);
  }
}

void commandBottomNoteUni() {
  int bottomNote = 0;
  bool noteActive = false;

  for (int i = 127; i >= 0; i--) {
    if (notes[i]) {
      bottomNote = i;
      noteActive = true;
    }
  }

  if (noteActive) {
    commandNoteUni(bottomNote);
  } else {  // All notes are off, turn off gate
    trig.writePin(GATE_NOTE1, LOW);
    trig.writePin(GATE_NOTE2, LOW);
    trig.writePin(GATE_NOTE3, LOW);
    trig.writePin(GATE_NOTE4, LOW);
    trig.writePin(GATE_NOTE5, LOW);
    trig.writePin(GATE_NOTE6, LOW);
    trig.writePin(GATE_NOTE7, LOW);
    trig.writePin(GATE_NOTE8, LOW);
  }
}

void commandLastNoteUni() {

  int8_t noteIndx;

  for (int i = 0; i < 40; i++) {
    noteIndx = noteOrder[mod(orderIndx - i, 40)];
    if (notes[noteIndx]) {
      commandNoteUni(noteIndx);
      return;
    }
  }
  trig.writePin(GATE_NOTE1, LOW);
  trig.writePin(GATE_NOTE2, LOW);
  trig.writePin(GATE_NOTE3, LOW);
  trig.writePin(GATE_NOTE4, LOW);
  trig.writePin(GATE_NOTE5, LOW);
  trig.writePin(GATE_NOTE6, LOW);
  trig.writePin(GATE_NOTE7, LOW);
  trig.writePin(GATE_NOTE8, LOW);
}

void commandNoteUni(int noteMsg) {

  unsigned int mV1 = (unsigned int)((float)(noteMsg + realoctave) * NOTE_SF * sfAdj[0] + 0.5);
  sample_data = (channel_a & 0xFFF0000F) | (((int(mV1)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data);
  unsigned int mV2 = (unsigned int)((float)(noteMsg + realoctave) * NOTE_SF * sfAdj[1] + 0.5);
  sample_data = (channel_b & 0xFFF0000F) | (((int(mV2)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data);
  unsigned int mV3 = (unsigned int)((float)(noteMsg + realoctave) * NOTE_SF * sfAdj[2] + 0.5);
  sample_data = (channel_c & 0xFFF0000F) | (((int(mV3)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data);
  unsigned int mV4 = (unsigned int)((float)(noteMsg + realoctave) * NOTE_SF * sfAdj[3] + 0.5);
  sample_data = (channel_d & 0xFFF0000F) | (((int(mV4)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data);
  unsigned int mV5 = (unsigned int)((float)(noteMsg + realoctave) * NOTE_SF * sfAdj[4] + 0.5);
  sample_data = (channel_e & 0xFFF0000F) | (((int(mV5)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data);
  unsigned int mV6 = (unsigned int)((float)(noteMsg + realoctave) * NOTE_SF * sfAdj[5] + 0.5);
  sample_data = (channel_f & 0xFFF0000F) | (((int(mV6)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data);
  unsigned int mV7 = (unsigned int)((float)(noteMsg + realoctave) * NOTE_SF * sfAdj[4] + 0.5);
  sample_data = (channel_g & 0xFFF0000F) | (((int(mV7)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data);
  unsigned int mV8 = (unsigned int)((float)(noteMsg + realoctave) * NOTE_SF * sfAdj[5] + 0.5);
  sample_data = (channel_h & 0xFFF0000F) | (((int(mV8)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data);

  MIDI2.sendNoteOn(noteMsg, 127, 1);
  trig.writePin(GATE_NOTE1, HIGH);
  MIDI2.sendNoteOn(noteMsg, 127, 2);
  trig.writePin(GATE_NOTE2, HIGH);
  MIDI2.sendNoteOn(noteMsg, 127, 3);
  trig.writePin(GATE_NOTE3, HIGH);
  MIDI2.sendNoteOn(noteMsg, 127, 4);
  trig.writePin(GATE_NOTE4, HIGH);
  MIDI2.sendNoteOn(noteMsg, 127, 5);
  trig.writePin(GATE_NOTE5, HIGH);
  MIDI2.sendNoteOn(noteMsg, 127, 6);
  trig.writePin(GATE_NOTE6, HIGH);
  MIDI2.sendNoteOn(noteMsg, 127, 7);
  trig.writePin(GATE_NOTE7, HIGH);
  MIDI2.sendNoteOn(noteMsg, 127, 8);
  trig.writePin(GATE_NOTE8, HIGH);
}

void myNoteOn(byte channel, byte note, byte velocity) {

  // for lfo multi trigger
  numberOfNotes = numberOfNotes + 1;
  keyTrackMult = keyTrack / 1023;

  //Check for out of range notes
  if (note < 0 || note > 127) return;

  prevNote = note;
  switch (keyboardMode) {
    case 0:
      switch (getVoiceNo(-1)) {
        case 1:
          voices[0].note = note;
          voices[0].velocity = velocity;
          voices[0].timeOn = millis();
          updateVoice1();
          MIDI2.sendNoteOn(voices[0].note, 127, 1);
          trig.writePin(GATE_NOTE1, HIGH);
          voiceOn[0] = true;
          break;
        case 2:
          voices[1].note = note;
          voices[1].velocity = velocity;
          voices[1].timeOn = millis();
          updateVoice2();
          MIDI2.sendNoteOn(voices[1].note, 127, 2);
          trig.writePin(GATE_NOTE2, HIGH);
          voiceOn[1] = true;
          break;
        case 3:
          voices[2].note = note;
          voices[2].velocity = velocity;
          voices[2].timeOn = millis();
          updateVoice3();
          MIDI2.sendNoteOn(voices[2].note, 127, 3);
          trig.writePin(GATE_NOTE3, HIGH);
          voiceOn[2] = true;
          break;
        case 4:
          voices[3].note = note;
          voices[3].velocity = velocity;
          voices[3].timeOn = millis();
          updateVoice4();
          MIDI2.sendNoteOn(voices[3].note, 127, 4);
          trig.writePin(GATE_NOTE4, HIGH);
          voiceOn[3] = true;
          break;
        case 5:
          voices[4].note = note;
          voices[4].velocity = velocity;
          voices[4].timeOn = millis();
          updateVoice5();
          MIDI2.sendNoteOn(voices[4].note, 127, 5);
          trig.writePin(GATE_NOTE5, HIGH);
          voiceOn[4] = true;
          break;
        case 6:
          voices[5].note = note;
          voices[5].velocity = velocity;
          voices[5].timeOn = millis();
          updateVoice6();
          MIDI2.sendNoteOn(voices[5].note, 127, 6);
          trig.writePin(GATE_NOTE6, HIGH);
          voiceOn[5] = true;
          break;
        case 7:
          voices[6].note = note;
          voices[6].velocity = velocity;
          voices[6].timeOn = millis();
          updateVoice7();
          MIDI2.sendNoteOn(voices[6].note, 127, 7);
          trig.writePin(GATE_NOTE7, HIGH);
          voiceOn[6] = true;
          break;
        case 8:
          voices[7].note = note;
          voices[7].velocity = velocity;
          voices[7].timeOn = millis();
          updateVoice8();
          MIDI2.sendNoteOn(voices[7].note, 127, 8);
          trig.writePin(GATE_NOTE8, HIGH);
          voiceOn[7] = true;
          break;
      }
      break;

    case 1:
      noteMsg = note;

      if (velocity == 0) {
        notes[noteMsg] = false;
      } else {
        notes[noteMsg] = true;
      }

      velmV = ((unsigned int)((float)velocity) * VEL_SF);
      sample_data = (channel_a & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
      outputDAC(DAC_NOTE2, sample_data);
      if (NotePriority == 0) {  // Highest note priority
        commandTopNote();
      } else if (NotePriority == 1) {  // Lowest note priority
        commandBottomNote();
      } else {                 // Last note priority
        if (notes[noteMsg]) {  // If note is on and using last note priority, add to ordered list
          orderIndx = (orderIndx + 1) % 40;
          noteOrder[orderIndx] = noteMsg;
        }
        commandLastNote();
      }
      break;

    case 2:
      noteMsg = note;

      if (velocity == 0) {
        notes[noteMsg] = false;
      } else {
        notes[noteMsg] = true;
      }

      velmV = ((unsigned int)((float)velocity) * VEL_SF);
      sample_data = (channel_a & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
      outputDAC(DAC_NOTE2, sample_data);
      sample_data = (channel_b & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
      outputDAC(DAC_NOTE2, sample_data);
      sample_data = (channel_c & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
      outputDAC(DAC_NOTE2, sample_data);
      sample_data = (channel_d & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
      outputDAC(DAC_NOTE2, sample_data);
      sample_data = (channel_e & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
      outputDAC(DAC_NOTE2, sample_data);
      sample_data = (channel_f & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
      outputDAC(DAC_NOTE2, sample_data);
      sample_data = (channel_g & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
      outputDAC(DAC_NOTE2, sample_data);
      sample_data = (channel_h & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
      outputDAC(DAC_NOTE2, sample_data);
      if (NotePriority == 0) {  // Highest note priority
        commandTopNoteUni();
      } else if (NotePriority == 1) {  // Lowest note priority
        commandBottomNoteUni();
      } else {                 // Last note priority
        if (notes[noteMsg]) {  // If note is on and using last note priority, add to ordered list
          orderIndx = (orderIndx + 1) % 40;
          noteOrder[orderIndx] = noteMsg;
        }
        commandLastNoteUni();
      }
      break;
  }
}

void myNoteOff(byte channel, byte note, byte velocity) {

  numberOfNotes = numberOfNotes - 1;
  oldnumberOfNotes = oldnumberOfNotes - 1;

  switch (keyboardMode) {
    case 0:
      switch (getVoiceNo(note)) {
        case 1:
          trig.writePin(GATE_NOTE1, LOW);
          voices[0].note = -1;
          voiceOn[0] = false;
          break;
        case 2:
          trig.writePin(GATE_NOTE2, LOW);
          voices[1].note = -1;
          voiceOn[1] = false;
          break;
        case 3:
          trig.writePin(GATE_NOTE3, LOW);
          voices[2].note = -1;
          voiceOn[2] = false;
          break;
        case 4:
          trig.writePin(GATE_NOTE4, LOW);
          voices[3].note = -1;
          voiceOn[3] = false;
          break;
        case 5:
          trig.writePin(GATE_NOTE5, LOW);
          voices[4].note = -1;
          voiceOn[4] = false;
          break;
        case 6:
          trig.writePin(GATE_NOTE6, LOW);
          voices[5].note = -1;
          voiceOn[5] = false;
          break;
        case 7:
          trig.writePin(GATE_NOTE7, LOW);
          voices[6].note = -1;
          voiceOn[6] = false;
          break;
        case 8:
          trig.writePin(GATE_NOTE8, LOW);
          voices[7].note = -1;
          voiceOn[7] = false;
          break;
      }
      break;

    case 1:
      noteMsg = note;

      if (velocity == 0 || velocity == 64) {
        notes[noteMsg] = false;
      } else {
        notes[noteMsg] = true;
      }

      // Pins NP_SEL1 and NP_SEL2 indictate note priority
      velmV = ((unsigned int)((float)velocity) * VEL_SF);
      sample_data = (channel_a & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
      outputDAC(DAC_NOTE2, sample_data);
      if (NotePriority == 0) {  // Highest note priority
        commandTopNote();
      } else if (NotePriority == 1) {  // Lowest note priority
        commandBottomNote();
      } else {                 // Last note priority
        if (notes[noteMsg]) {  // If note is on and using last note priority, add to ordered list
          orderIndx = (orderIndx + 1) % 40;
          noteOrder[orderIndx] = noteMsg;
        }
        commandLastNote();
      }
      break;

    case 2:
      noteMsg = note;

      if (velocity == 0 || velocity == 64) {
        notes[noteMsg] = false;
      } else {
        notes[noteMsg] = true;
      }

      velmV = ((unsigned int)((float)velocity) * VEL_SF);
      sample_data = (channel_a & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
      outputDAC(DAC_NOTE2, sample_data);
      sample_data = (channel_b & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
      outputDAC(DAC_NOTE2, sample_data);
      sample_data = (channel_c & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
      outputDAC(DAC_NOTE2, sample_data);
      sample_data = (channel_d & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
      outputDAC(DAC_NOTE2, sample_data);
      sample_data = (channel_e & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
      outputDAC(DAC_NOTE2, sample_data);
      sample_data = (channel_f & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
      outputDAC(DAC_NOTE2, sample_data);
      sample_data = (channel_g & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
      outputDAC(DAC_NOTE2, sample_data);
      sample_data = (channel_h & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
      outputDAC(DAC_NOTE2, sample_data);
      if (NotePriority == 0) {  // Highest note priority
        commandTopNoteUni();
      } else if (NotePriority == 1) {  // Lowest note priority
        commandBottomNoteUni();
      } else {                 // Last note priority
        if (notes[noteMsg]) {  // If note is on and using last note priority, add to ordered list
          orderIndx = (orderIndx + 1) % 40;
          noteOrder[orderIndx] = noteMsg;
        }
        commandLastNoteUni();
      }
      break;
  }
}

int getVoiceNo(int note) {
  voiceToReturn = -1;       //Initialise to 'null'
  earliestTime = millis();  //Initialise to now
  if (note == -1) {
    //NoteOn() - Get the oldest free voice (recent voices may be still on release stage)
    for (int i = 0; i < NO_OF_VOICES; i++) {
      if (voices[i].note == -1) {
        if (voices[i].timeOn < earliestTime) {
          earliestTime = voices[i].timeOn;
          voiceToReturn = i;
        }
      }
    }
    if (voiceToReturn == -1) {
      //No free voices, need to steal oldest sounding voice
      earliestTime = millis();  //Reinitialise
      for (int i = 0; i < NO_OF_VOICES; i++) {
        if (voices[i].timeOn < earliestTime) {
          earliestTime = voices[i].timeOn;
          voiceToReturn = i;
        }
      }
    }
    return voiceToReturn + 1;
  } else {
    //NoteOff() - Get voice number from note
    for (int i = 0; i < NO_OF_VOICES; i++) {
      if (voices[i].note == note) {
        return i + 1;
      }
    }
  }
  //Shouldn't get here, return voice 1
  return 1;
}

void updateVoice1() {
  unsigned int mV = (unsigned int)((float)(voices[0].note + realoctave) * NOTE_SF * sfAdj[0] + 0.5);
  mV = mV * keyTrackMult;
  sample_data = (channel_a & 0xFFF0000F) | (((int(mV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data);
  velmV = ((unsigned int)((float)voices[0].velocity) * VEL_SF);
  sample_data = (channel_a & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE2, sample_data);
}

void updateVoice2() {
  unsigned int mV = (unsigned int)((float)(voices[1].note + realoctave) * NOTE_SF * sfAdj[1] + 0.5);
  mV = mV * keyTrackMult;
  sample_data = (channel_b & 0xFFF0000F) | (((int(mV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data);
  velmV = ((unsigned int)((float)voices[1].velocity) * VEL_SF);
  sample_data = (channel_b & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE2, sample_data);
}

void updateVoice3() {
  unsigned int mV = (unsigned int)((float)(voices[2].note + realoctave) * NOTE_SF * sfAdj[2] + 0.5);
  mV = mV * keyTrackMult;
  sample_data = (channel_c & 0xFFF0000F) | (((int(mV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data);
  velmV = ((unsigned int)((float)voices[2].velocity) * VEL_SF);
  sample_data = (channel_c & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE2, sample_data);
}

void updateVoice4() {
  unsigned int mV = (unsigned int)((float)(voices[3].note + realoctave) * NOTE_SF * sfAdj[3] + 0.5);
  mV = mV * keyTrackMult;
  sample_data = (channel_d & 0xFFF0000F) | (((int(mV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data);
  velmV = ((unsigned int)((float)voices[3].velocity) * VEL_SF);
  sample_data = (channel_d & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE2, sample_data);
}

void updateVoice5() {
  unsigned int mV = (unsigned int)((float)(voices[4].note + realoctave) * NOTE_SF * sfAdj[4] + 0.5);
  mV = mV * keyTrackMult;
  sample_data = (channel_e & 0xFFF0000F) | (((int(mV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data);
  velmV = ((unsigned int)((float)voices[4].velocity) * VEL_SF);
  sample_data = (channel_e & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE2, sample_data);
}

void updateVoice6() {
  unsigned int mV = (unsigned int)((float)(voices[5].note + realoctave) * NOTE_SF * sfAdj[5] + 0.5);
  mV = mV * keyTrackMult;
  sample_data = (channel_f & 0xFFF0000F) | (((int(mV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data);
  velmV = ((unsigned int)((float)voices[5].velocity) * VEL_SF);
  sample_data = (channel_f & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE2, sample_data);
}

void updateVoice7() {
  unsigned int mV = (unsigned int)((float)(voices[6].note + realoctave) * NOTE_SF * sfAdj[6] + 0.5);
  mV = mV * keyTrackMult;
  sample_data = (channel_g & 0xFFF0000F) | (((int(mV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data);
  velmV = ((unsigned int)((float)voices[6].velocity) * VEL_SF);
  sample_data = (channel_g & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE2, sample_data);
}

void updateVoice8() {
  unsigned int mV = (unsigned int)((float)(voices[7].note + realoctave) * NOTE_SF * sfAdj[7] + 0.5);
  mV = mV * keyTrackMult;
  sample_data = (channel_h & 0xFFF0000F) | (((int(mV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE1, sample_data);
  velmV = ((unsigned int)((float)voices[7].velocity) * VEL_SF);
  sample_data = (channel_h & 0xFFF0000F) | (((int(velmV)) & 0xFFFF) << 4);
  outputDAC(DAC_NOTE2, sample_data);
}

void myConvertControlChange(byte channel, byte number, byte value) {
  int newvalue = (value << 3);
  myControlChange(channel, number, newvalue);
}

void myPitchBend(byte channel, int bend) {
  MIDI.sendPitchBend(bend, channel);
  if (sendNotes) {
    usbMIDI.sendPitchBend(bend, channel);
  }
}

void myAfterTouch(byte channel, byte value) {

  float newAfterTouchDepth = 0.0;

  if (afterTouchDepth > 0) {
    newAfterTouchDepth = afterTouchDepth / 10.0;
  }
  if (afterTouchDepth == 0) {
    afterTouch = 0;
  } else {
    afterTouch = int(value * newAfterTouchDepth) << 3;
  }

  switch (AfterTouchDest) {
    case 1:
      osc1fmDepth = afterTouch;
      osc2fmDepth = afterTouch;
      break;

    case 2:
      filterCutoff = (filterCutoff + afterTouch);
      if (afterTouch <= 8) {
        filterCutoff = oldfilterCutoff;
      }
      break;

    case 3:
      filterLFO = afterTouch;
      break;

    case 4:
      amDepth = afterTouch;
      break;
  }
}

void allNotesOff() {
  for (int i = 0; i < NO_OF_VOICES; i++) {
    voices[i].note = -1;
    voiceOn[i] = false;
  }
  trig.writePin(GATE_NOTE1, LOW);
  trig.writePin(GATE_NOTE2, LOW);
  trig.writePin(GATE_NOTE3, LOW);
  trig.writePin(GATE_NOTE4, LOW);
  trig.writePin(GATE_NOTE5, LOW);
  trig.writePin(GATE_NOTE6, LOW);
  trig.writePin(GATE_NOTE7, LOW);
  trig.writePin(GATE_NOTE8, LOW);
}

void updateosc1fmWaveMod() {
  parameterGroup = 7;
}

void updateosc2fmWaveMod() {
  parameterGroup = 8;
}

void updateosc1fmDepth() {
  parameterGroup = 7;
}

void updateosc2fmDepth() {
  parameterGroup = 8;
}

void updateosc1Tune() {
  parameterGroup = 5;
}

void updateosc2Tune() {
  parameterGroup = 5;
}

void updateosc1WaveMod() {
  parameterGroup = 7;
}

void updateosc2WaveMod() {
  parameterGroup = 8;
}

void updateosc1Range() {
  parameterGroup = 7;
  switch (oct1) {
    case 0:
      sendCCtoAllDevices(17, 0);
      srp.writePin(DCO1_OCT_RED_LED, HIGH);
      srp.writePin(DCO1_OCT_GREEN_LED, LOW);
      break;

    case 1:
      sendCCtoAllDevices(17, 64);
      srp.writePin(DCO1_OCT_RED_LED, HIGH);
      srp.writePin(DCO1_OCT_GREEN_LED, HIGH);
      break;

    case 2:
      sendCCtoAllDevices(17, 127);
      srp.writePin(DCO1_OCT_RED_LED, LOW);
      srp.writePin(DCO1_OCT_GREEN_LED, HIGH);
      break;
  }
}

void updateosc2Range() {
  parameterGroup = 8;
  switch (oct2) {
    case 0:
      sendCCtoAllDevices(19, 0);
      srp.writePin(DCO2_OCT_RED_LED, HIGH);
      srp.writePin(DCO2_OCT_GREEN_LED, LOW);
      break;

    case 1:
      sendCCtoAllDevices(19, 64);
      srp.writePin(DCO2_OCT_RED_LED, HIGH);
      srp.writePin(DCO2_OCT_GREEN_LED, HIGH);
      break;

    case 2:
      sendCCtoAllDevices(19, 127);
      srp.writePin(DCO2_OCT_RED_LED, LOW);
      srp.writePin(DCO2_OCT_GREEN_LED, HIGH);
      break;
  }
}

void updateosc1Bank() {
  switch (osc1Bank) {
    case 0:
      sendCCtoAllDevices(16, 0);
      srp.writePin(DCO1_MODE_RED_LED, HIGH);
      srp.writePin(DCO1_MODE_GREEN_LED, LOW);
      break;

    case 1:
      sendCCtoAllDevices(16, 1);
      srp.writePin(DCO1_MODE_RED_LED, HIGH);
      srp.writePin(DCO1_MODE_GREEN_LED, HIGH);
      break;

    case 2:
      sendCCtoAllDevices(16, 2);
      srp.writePin(DCO1_MODE_RED_LED, LOW);
      srp.writePin(DCO1_MODE_GREEN_LED, HIGH);
      break;
  }
  updateosc1WaveSelect();
}

void updateosc2Bank() {
  switch (osc2Bank) {

    case 0:
      sendCCtoAllDevices(18, 0);
      srp.writePin(DCO2_MODE_RED_LED, HIGH);
      srp.writePin(DCO2_MODE_GREEN_LED, LOW);
      break;

    case 1:
      sendCCtoAllDevices(18, 1);
      srp.writePin(DCO2_MODE_RED_LED, HIGH);
      srp.writePin(DCO2_MODE_GREEN_LED, HIGH);
      break;

    case 2:
      sendCCtoAllDevices(18, 2);
      srp.writePin(DCO2_MODE_RED_LED, LOW);
      srp.writePin(DCO2_MODE_GREEN_LED, HIGH);
      break;
  }
  updateosc2WaveSelect();
}

void updateglideTime() {
  parameterGroup = 5;
  sendCCtoAllDevices(5, glideTime);
}

void updateglideSW() {
  parameterGroup = 5;
  if (glideSW == 1) {
    sendCCtoAllDevices(65, 1);
    srp.writePin(GLIDE_LED, HIGH);  // LED on
    midiCCOut(CCglideSW, 127);
    updateglideTime();
  } else {
    sendCCtoAllDevices(65, 0);
    srp.writePin(GLIDE_LED, LOW);  // LED off
    midiCCOut(CCglideSW, 127);
    updateglideTime();
  }
}

void sendCCtoAllDevices(int CCnumberTosend, int value) {
  //
  MIDI2.sendControlChange(CCnumberTosend, value, 1);
  //}
}

void updateosc1WaveSelect() {
  parameterGroup = 7;
  switch (osc1Bank) {
    case 0:
      switch (osc1WaveSelect) {
        case 0:
          sendCCtoAllDevices(70, 0);
          break;

        case 1:
          sendCCtoAllDevices(70, 1);
          break;

        case 2:
          sendCCtoAllDevices(70, 2);
          break;

        case 3:
          sendCCtoAllDevices(70, 3);
          break;

        case 4:
          sendCCtoAllDevices(70, 4);
          break;

        case 5:
          sendCCtoAllDevices(70, 5);
          break;

        case 6:
          sendCCtoAllDevices(70, 6);
          break;

        case 7:
          sendCCtoAllDevices(70, 7);
          break;
      }
      break;

    case 1:
      switch (osc1WaveSelect) {
        case 0:
          sendCCtoAllDevices(70, 0);
          break;

        case 1:
          sendCCtoAllDevices(70, 1);
          break;

        case 2:
          sendCCtoAllDevices(70, 2);
          break;

        case 3:
          sendCCtoAllDevices(70, 3);
          break;

        case 4:
          sendCCtoAllDevices(70, 4);
          break;

        case 5:
          sendCCtoAllDevices(70, 5);
          break;

        case 6:
          sendCCtoAllDevices(70, 6);
          break;

        case 7:
          sendCCtoAllDevices(70, 7);
          break;
      }
      break;

    case 2:
      switch (osc1WaveSelect) {
        case 0:
          sendCCtoAllDevices(70, 0);
          break;

        case 1:
          sendCCtoAllDevices(70, 1);
          break;

        case 2:
          sendCCtoAllDevices(70, 2);
          break;

        case 3:
          sendCCtoAllDevices(70, 3);
          break;

        case 4:
          sendCCtoAllDevices(70, 4);
          break;

        case 5:
          sendCCtoAllDevices(70, 5);
          break;

        case 6:
          sendCCtoAllDevices(70, 6);
          break;

        case 7:
          sendCCtoAllDevices(70, 7);
          break;
      }
      break;
  }
  //pixel.clear();
  pixel.setPixelColor(1, pixel.Color(colour[osc1WaveSelect][0], colour[osc1WaveSelect][1], colour[osc1WaveSelect][2]));
  pixel.show();
}

void updateosc2WaveSelect() {
  parameterGroup = 8;
  switch (osc2Bank) {
    case 0:
      switch (osc2WaveSelect) {
        case 0:
          sendCCtoAllDevices(71, 0);
          break;

        case 1:
          sendCCtoAllDevices(71, 1);
          break;

        case 2:
          sendCCtoAllDevices(71, 2);
          break;

        case 3:
          sendCCtoAllDevices(71, 3);
          break;

        case 4:
          sendCCtoAllDevices(71, 4);
          break;

        case 5:
          sendCCtoAllDevices(71, 5);
          break;

        case 6:
          sendCCtoAllDevices(71, 6);
          break;

        case 7:
          sendCCtoAllDevices(71, 7);
          break;
      }
      break;

    case 1:
      switch (osc2WaveSelect) {
        case 0:
          sendCCtoAllDevices(71, 0);
          break;

        case 1:
          sendCCtoAllDevices(71, 1);
          break;

        case 2:
          sendCCtoAllDevices(71, 2);
          break;

        case 3:
          sendCCtoAllDevices(71, 3);
          break;

        case 4:
          sendCCtoAllDevices(71, 4);
          break;

        case 5:
          sendCCtoAllDevices(71, 5);
          break;

        case 6:
          sendCCtoAllDevices(71, 6);
          break;

        case 7:
          sendCCtoAllDevices(71, 7);
          break;
      }
      break;

    case 2:
      switch (osc2WaveSelect) {
        case 0:
          sendCCtoAllDevices(71, 0);
          break;

        case 1:
          sendCCtoAllDevices(71, 1);
          break;

        case 2:
          sendCCtoAllDevices(71, 2);
          break;

        case 3:
          sendCCtoAllDevices(71, 3);
          break;

        case 4:
          sendCCtoAllDevices(71, 4);
          break;

        case 5:
          sendCCtoAllDevices(71, 5);
          break;

        case 6:
          sendCCtoAllDevices(71, 6);
          break;

        case 7:
          sendCCtoAllDevices(71, 7);
          break;
      }
      break;
  }
  //pixel.clear();
  pixel.setPixelColor(2, pixel.Color(colour[osc2WaveSelect][0], colour[osc2WaveSelect][1], colour[osc2WaveSelect][2]));
  pixel.show();
}

void updatenoiseLevel() {
  parameterGroup = 6;
}

void updateOsc2Level() {
  parameterGroup = 8;
}

void updateOsc1Level() {
  parameterGroup = 7;
}

void updateKeyTrack() {
  parameterGroup = 1;
}

void updateFilterCutoff() {
  parameterGroup = 1;
}

void updatefilterLFO() {
  parameterGroup = 1;
}

void updatefilterRes() {
  parameterGroup = 1;
}

void updateeffect1() {
  parameterGroup = 0;
}

void updateeffect2() {
  parameterGroup = 0;
}

void updateeffect3() {
  parameterGroup = 0;
}

void updateeffectMix() {
  parameterGroup = 0;
}

void updateFilterType() {
  parameterGroup = 1;
  switch (filterType) {
    case 0:
      sr.writePin(FILTER_A, LOW);
      sr.writePin(FILTER_B, LOW);
      sr.writePin(FILTER_C, LOW);
      break;

    case 1:
      sr.writePin(FILTER_A, HIGH);
      sr.writePin(FILTER_B, LOW);
      sr.writePin(FILTER_C, LOW);
      break;

    case 2:
      sr.writePin(FILTER_A, LOW);
      sr.writePin(FILTER_B, HIGH);
      sr.writePin(FILTER_C, LOW);
      break;

    case 3:
      sr.writePin(FILTER_A, HIGH);
      sr.writePin(FILTER_B, HIGH);
      sr.writePin(FILTER_C, LOW);
      break;

    case 4:
      sr.writePin(FILTER_A, LOW);
      sr.writePin(FILTER_B, LOW);
      sr.writePin(FILTER_C, HIGH);
      break;

    case 5:
      sr.writePin(FILTER_A, HIGH);
      sr.writePin(FILTER_B, LOW);
      sr.writePin(FILTER_C, HIGH);
      break;

    case 6:
      sr.writePin(FILTER_A, LOW);
      sr.writePin(FILTER_B, HIGH);
      sr.writePin(FILTER_C, HIGH);
      break;

    case 7:
      sr.writePin(FILTER_A, HIGH);
      sr.writePin(FILTER_B, HIGH);
      sr.writePin(FILTER_C, HIGH);
      break;
  }
  pixel.setPixelColor(0, pixel.Color(colour[filterType][0], colour[filterType][1], colour[filterType][2]));
  pixel.show();
}

void updatefilterEGlevel() {
  parameterGroup = 1;
}

void updateLFORate() {
  parameterGroup = 4;
}

void updatelfoDelay() {
  parameterGroup = 4;
}

void updateamDepth() {
  parameterGroup = 6;
}

void updateosc1osc2fmDepth() {
  parameterGroup = 4;
}

void updateStratusLFOWaveform() {
  parameterGroup = 4;
  if (!lfoAlt) {
    switch (LFOWaveform) {
      case 0:
        LFOWaveCV = 40;
        break;

      case 1:
        LFOWaveCV = 160;
        break;

      case 2:
        LFOWaveCV = 280;
        break;

      case 3:
        LFOWaveCV = 376;
        break;

      case 4:
        LFOWaveCV = 592;
        break;

      case 5:
        LFOWaveCV = 720;
        break;

      case 6:
        LFOWaveCV = 840;
        break;

      case 7:
        LFOWaveCV = 968;
        break;
    }
  } else {
    switch (LFOWaveform) {
      case 0:
        LFOWaveCV = 40;
        break;

      case 1:
        LFOWaveCV = 160;
        break;

      case 2:
        LFOWaveCV = 280;
        break;

      case 3:
        LFOWaveCV = 376;
        break;

      case 4:
        LFOWaveCV = 592;
        break;

      case 5:
        LFOWaveCV = 720;
        break;

      case 6:
        LFOWaveCV = 840;
        break;

      case 7:
        LFOWaveCV = 968;
        break;
    }
  }

  pixel.setPixelColor(3, pixel.Color(colour[LFOWaveform][0], colour[LFOWaveform][1], colour[LFOWaveform][2]));
  pixel.show();
}

void updatefilterAttack() {
  parameterGroup = 2;
}

void updatefilterDecay() {
  parameterGroup = 2;
}

void updatefilterSustain() {
  parameterGroup = 2;
}

void updatefilterRelease() {
  parameterGroup = 2;
}

void updateampAttack() {
  parameterGroup = 3;
}

void updateampDecay() {
  parameterGroup = 3;
}

void updateampSustain() {
  parameterGroup = 3;
}

void updateampRelease() {
  parameterGroup = 3;
}

void updatevolumeControl() {
  parameterGroup = 6;
}

////////////////////////////////////////////////////////////////


void updatefilterPoleSwitch() {
  if (filterPoleSW) {
    sr.writePin(FILTER_POLE, HIGH);
    srp.writePin(FILTER_POLE_RED_LED, HIGH);
    srp.writePin(FILTER_POLE_GREEN_LED, LOW);
    midiCCOut(CCfilterPoleSW, 127);
    updateFilterType();
  } else {
    sr.writePin(FILTER_POLE, LOW);
    srp.writePin(FILTER_POLE_RED_LED, LOW);
    srp.writePin(FILTER_POLE_GREEN_LED, HIGH);
    midiCCOut(CCfilterPoleSW, 0);
    updateFilterType();
  }
}

void updateFilterLoop() {
  parameterGroup = 2;
  switch (FilterLoop) {
    case 0:
      srp.writePin(FILTER_LOOP_RED_LED, LOW);
      srp.writePin(FILTER_LOOP_GREEN_LED, LOW);
      sr.writePin(FLOOPBIT0, LOW);
      sr.writePin(FLOOPBIT1, LOW);
      break;

    case 1:
      srp.writePin(FILTER_LOOP_RED_LED, HIGH);
      srp.writePin(FILTER_LOOP_GREEN_LED, LOW);
      sr.writePin(FLOOPBIT0, HIGH);
      sr.writePin(FLOOPBIT1, LOW);
      break;

    case 2:
      srp.writePin(FILTER_LOOP_RED_LED, LOW);
      srp.writePin(FILTER_LOOP_GREEN_LED, HIGH);
      sr.writePin(FLOOPBIT0, HIGH);
      sr.writePin(FLOOPBIT1, HIGH);
      break;
  }
}

void updatefilterEGinv() {
  parameterGroup = 2;
  if (filterEGinv) {
    sr.writePin(FILTER_EG_INV, LOW);
    srp.writePin(EG_INV_LED, LOW);
    midiCCOut(CCfilterEGinv, 0);
  } else {
    sr.writePin(FILTER_EG_INV, HIGH);
    srp.writePin(EG_INV_LED, HIGH);
    midiCCOut(CCfilterEGinv, 127);
  }
}

void updatefilterVelSW() {
  parameterGroup = 2;
  if (filterVelSW) {
    sr.writePin(FILTER_VELOCITY, HIGH);
    srp.writePin(FILTER_VELOCITY_LED, HIGH);
    midiCCOut(CCfilterVelSW, 127);
  } else {
    sr.writePin(FILTER_VELOCITY, LOW);
    srp.writePin(FILTER_VELOCITY_LED, LOW);
    midiCCOut(CCfilterVelSW, 0);
  }
}

void updateampVelSW() {
  parameterGroup = 3;
  if (ampVelSW) {
    sr.writePin(AMP_VELOCITY, HIGH);
    srp.writePin(VCA_VELOCITY_LED, HIGH);
    midiCCOut(CCampVelSW, 127);
  } else {
    sr.writePin(AMP_VELOCITY, LOW);
    srp.writePin(VCA_VELOCITY_LED, LOW);
    midiCCOut(CCampVelSW, 0);
  }
}

void updateAmpLoop() {
  parameterGroup = 3;
  switch (AmpLoop) {
    case 0:
      srp.writePin(VCA_LOOP_RED_LED, LOW);
      srp.writePin(VCA_LOOP_GREEN_LED, LOW);
      sr.writePin(ALOOPBIT0, LOW);
      sr.writePin(ALOOPBIT1, LOW);
      break;

    case 1:
      srp.writePin(VCA_LOOP_RED_LED, HIGH);
      srp.writePin(VCA_LOOP_GREEN_LED, LOW);
      sr.writePin(ALOOPBIT0, HIGH);
      sr.writePin(ALOOPBIT1, LOW);
      break;

    case 2:
      srp.writePin(VCA_LOOP_RED_LED, LOW);
      srp.writePin(VCA_LOOP_GREEN_LED, HIGH);
      sr.writePin(ALOOPBIT0, HIGH);
      sr.writePin(ALOOPBIT1, HIGH);
      break;
  }
}

void updatekeyboardMode() {
  parameterGroup = 5;
  switch (keyboardMode) {
    case 0:
      srp.writePin(POLY_MODE_RED_LED, HIGH);
      srp.writePin(POLY_MODE_GREEN_LED, LOW);
      allNotesOff();
      break;

    case 1:
      srp.writePin(POLY_MODE_RED_LED, LOW);
      srp.writePin(POLY_MODE_GREEN_LED, HIGH);
      allNotesOff();
      break;

    case 2:
      srp.writePin(POLY_MODE_RED_LED, HIGH);
      srp.writePin(POLY_MODE_GREEN_LED, HIGH);
      allNotesOff();
      break;
  }
}

void updateNotePriority() {
  parameterGroup = 5;
  switch (NotePriority) {
    case 0:
      srp.writePin(PRIORITY_RED_LED, HIGH);
      srp.writePin(PRIORITY_GREEN_LED, LOW);
      break;

    case 1:
      srp.writePin(PRIORITY_RED_LED, LOW);
      srp.writePin(PRIORITY_GREEN_LED, HIGH);
      break;

    case 2:
      srp.writePin(PRIORITY_RED_LED, HIGH);
      srp.writePin(PRIORITY_GREEN_LED, HIGH);
      break;
  }
}

void updatelfoAlt() {
  if (lfoAlt) {
    updateStratusLFOWaveform();
    sr.writePin(LFO_ALT, HIGH);
    srp.writePin(LFO_ALT_RED_LED, HIGH);
    srp.writePin(LFO_ALT_GREEN_LED, LOW);
    midiCCOut(CClfoAlt, 0);
  } else {
    updateStratusLFOWaveform();
    sr.writePin(LFO_ALT, LOW);
    srp.writePin(LFO_ALT_RED_LED, LOW);
    srp.writePin(LFO_ALT_GREEN_LED, HIGH);
    midiCCOut(CClfoAlt, 127);
  }
}

void updatelfoMult() {
  parameterGroup = 4;
  switch (lfoMult) {

    case 0:
      srp.writePin(LFO_MULT_RED_LED, HIGH);
      srp.writePin(LFO_MULT_GREEN_LED, LOW);
      LFOMultCV = 40;
      break;

    case 1:
      srp.writePin(LFO_MULT_RED_LED, HIGH);
      srp.writePin(LFO_MULT_GREEN_LED, HIGH);
      LFOMultCV = 400;
      break;

    case 2:
      srp.writePin(LFO_MULT_RED_LED, LOW);
      srp.writePin(LFO_MULT_GREEN_LED, HIGH);
      LFOMultCV = 1023;
      break;
  }
}

void updateeffectBankSW() {
  parameterGroup = 0;
  switch (effectBankSW) {
    case 0:
      srp.writePin(EFFECT_BANK_RED_LED, LOW);
      srp.writePin(EFFECT_BANK_GREEN_LED, LOW);
      sr.writePin(EFFECT_BANK, LOW);

      sr.writePin(EFFECT_ROM_0, HIGH);
      sr.writePin(EFFECT_ROM_1, HIGH);

      break;

    case 1:
      srp.writePin(EFFECT_BANK_RED_LED, HIGH);
      srp.writePin(EFFECT_BANK_GREEN_LED, LOW);
      sr.writePin(EFFECT_BANK, HIGH);

      sr.writePin(EFFECT_ROM_0, LOW);
      sr.writePin(EFFECT_ROM_1, LOW);
      delay(5);
      sr.writePin(EFFECT_BANK, LOW);
      delay(5);
      sr.writePin(EFFECT_BANK, HIGH);
      delay(5);
      break;

    case 2:
      srp.writePin(EFFECT_BANK_RED_LED, LOW);
      srp.writePin(EFFECT_BANK_GREEN_LED, HIGH);
      sr.writePin(EFFECT_BANK, HIGH);

      sr.writePin(EFFECT_ROM_0, HIGH);
      sr.writePin(EFFECT_ROM_1, LOW);
      delay(5);
      sr.writePin(EFFECT_BANK, LOW);
      delay(5);
      sr.writePin(EFFECT_BANK, HIGH);
      delay(5);
      break;

    case 3:
      srp.writePin(EFFECT_BANK_RED_LED, HIGH);
      srp.writePin(EFFECT_BANK_GREEN_LED, HIGH);
      sr.writePin(EFFECT_BANK, HIGH);

      sr.writePin(EFFECT_ROM_0, LOW);
      sr.writePin(EFFECT_ROM_1, HIGH);
      delay(5);
      sr.writePin(EFFECT_BANK, LOW);
      delay(5);
      sr.writePin(EFFECT_BANK, HIGH);
      delay(5);
      break;
  }
}

void updateeffectNumSW() {
  parameterGroup = 0;
  switch (effectNumSW) {
    case 0:
      sr.writePin(EFFECT_0, LOW);
      sr.writePin(EFFECT_1, LOW);
      sr.writePin(EFFECT_2, LOW);
      break;

    case 1:
      sr.writePin(EFFECT_0, HIGH);
      sr.writePin(EFFECT_1, LOW);
      sr.writePin(EFFECT_2, LOW);
      break;

    case 2:
      sr.writePin(EFFECT_0, LOW);
      sr.writePin(EFFECT_1, HIGH);
      sr.writePin(EFFECT_2, LOW);
      break;

    case 3:
      sr.writePin(EFFECT_0, HIGH);
      sr.writePin(EFFECT_1, HIGH);
      sr.writePin(EFFECT_2, LOW);
      break;

    case 4:
      sr.writePin(EFFECT_0, LOW);
      sr.writePin(EFFECT_1, LOW);
      sr.writePin(EFFECT_2, HIGH);
      break;

    case 5:
      sr.writePin(EFFECT_0, HIGH);
      sr.writePin(EFFECT_1, LOW);
      sr.writePin(EFFECT_2, HIGH);
      break;

    case 6:
      sr.writePin(EFFECT_0, LOW);
      sr.writePin(EFFECT_1, HIGH);
      sr.writePin(EFFECT_2, HIGH);
      break;

    case 7:
      sr.writePin(EFFECT_0, HIGH);
      sr.writePin(EFFECT_1, HIGH);
      sr.writePin(EFFECT_2, HIGH);
      break;
  }
}

void updateenvLinLogSW() {
  parameterGroup = 2;
  if (envLinLogSW) {
    sr.writePin(FILTER_LIN_LOG, HIGH);
    sr.writePin(AMP_LIN_LOG, HIGH);
    srp.writePin(LIN_LOG_RED_LED, HIGH);
    srp.writePin(LIN_LOG_GREEN_LED, LOW);
    midiCCOut(CCenvLinLogSW, 127);
  } else {
    sr.writePin(FILTER_LIN_LOG, LOW);
    sr.writePin(AMP_LIN_LOG, LOW);
    srp.writePin(LIN_LOG_RED_LED, LOW);
    srp.writePin(LIN_LOG_GREEN_LED, HIGH);
    midiCCOut(CCenvLinLogSW, 0);
  }
}

void updateAmpGatedSW() {
  parameterGroup = 3;
  if (!AmpGatedSW) {
    srp.writePin(VCA_GATE_LED, LOW);  // LED off
    ampAttack = oldampAttack;
    ampDecay = oldampDecay;
    ampSustain = oldampSustain;
    ampRelease = oldampRelease;
    midiCCOut(CCAmpGatedSW, 0);

  } else {
    srp.writePin(VCA_GATE_LED, HIGH);  // LED on
    ampAttack = 0;
    ampDecay = 0;
    ampSustain = 1023;
    ampRelease = 0;
    midiCCOut(CCAmpGatedSW, 127);
  }
}

void updatemonoMultiSW() {
  parameterGroup = 4;
  if (monoMultiSW) {
    srp.writePin(MONO_MULTI_LED, HIGH);
    midiCCOut(CCmonoMultiSW, 127);
  } else {
    srp.writePin(MONO_MULTI_LED, LOW);
    midiCCOut(CCmonoMultiSW, 0);
  }
}

void updateAfterTouchDest() {
  switch (AfterTouchDest) {
    case 0:
      // sr.writePin(AFTERTOUCH_A, LOW);
      // sr.writePin(AFTERTOUCH_B, LOW);
      // sr.writePin(AFTERTOUCH_C, LOW);
      break;

    case 1:
      // sr.writePin(AFTERTOUCH_A, HIGH);
      // sr.writePin(AFTERTOUCH_B, LOW);
      // sr.writePin(AFTERTOUCH_C, LOW);
      break;

    case 2:
      // sr.writePin(AFTERTOUCH_A, LOW);
      // sr.writePin(AFTERTOUCH_B, HIGH);
      // sr.writePin(AFTERTOUCH_C, LOW);
      break;

    case 3:
      // sr.writePin(AFTERTOUCH_A, HIGH);
      // sr.writePin(AFTERTOUCH_B, HIGH);
      // sr.writePin(AFTERTOUCH_C, LOW);
      break;
  }
  oldAfterTouchDest = AfterTouchDest;
}

void updatePatchname() {
  showPatchPage(String(patchNo), patchName);
}

void myControlChange(byte channel, byte control, int value) {
  switch (control) {
    case CCosc1Tune:
      osc1Tune = value;
      osc1Tunestr = map(value, 0, readRes, 0, 127);
      osc1Tunestr = PITCH[osc1Tunestr];  // for display
      updateosc1Tune();
      break;

    case CCosc1fmDepth:
      osc1fmDepth = value;
      if (osc1osc2fmDepth) {
        osc2fmDepth = osc1fmDepth;
      }
      osc1fmDepthstr = map(value, 0, readRes, 0, 127);
      updateosc1fmDepth();
      break;

    case CCosc2fmDepth:
      osc2fmDepth = value;
      osc2fmDepthstr = map(value, 0, readRes, 0, 127);
      updateosc2fmDepth();
      break;

    case CCosc2Tune:
      osc2Tune = value;
      osc2Tunestr = map(value, 0, readRes, 0, 127);
      osc2Tunestr = PITCH[osc2Tunestr];  // for display
      updateosc2Tune();
      break;

    case CCosc1WaveMod:
      osc1WaveMod = value;
      osc1WaveModstr = map(value, 0, readRes, 0, 127);
      updateosc1WaveMod();
      break;

    case CCosc2WaveMod:
      osc2WaveMod = value;
      osc2WaveModstr = map(value, 0, readRes, 0, 127);
      updateosc2WaveMod();
      break;

    case CCeffect1:
      effect1 = value;
      effect1str = map(value, 0, readRes, 0, 127);
      updateeffect1();
      break;

    case CCeffect2:
      effect2 = value;
      effect2str = map(value, 0, readRes, 0, 127);
      updateeffect2();
      break;

    case CCeffect3:
      effect3 = value;
      effect3str = map(value, 0, readRes, 0, 127);
      updateeffect3();
      break;

    case CCeffectMix:
      mixa = map(value, 0, readRes, 0, readRes);
      mixastr = map(value, 0, readRes, 0, 127);
      mixbstr = map(value, 0, readRes, 127, 0);
      updateeffectMix();
      break;

    case CCglideTime:
      glideTimestr = map(value, 0, readRes, 0, 127);
      glideTimedisplay = LINEAR[glideTimestr];
      glideTime = map(value, 0, readRes, 1, 127);
      updateglideTime();
      break;

    case CCosc1fmWaveMod:
      osc1fmWaveModstr = map(value, 0, readRes, 0, 127);
      osc1fmWaveMod = value;
      updateosc1fmWaveMod();
      break;

    case CCosc2fmWaveMod:
      osc2fmWaveModstr = map(value, 0, readRes, 0, 127);
      osc2fmWaveMod = value;
      updateosc2fmWaveMod();
      break;

    case CCnoiseLevel:
      noiseLevel = value;
      noiseLevelstr = map(value, 0, readRes, 0, 127);
      noiseLeveldisplay = LINEARCENTREZERO[noiseLevelstr];
      updatenoiseLevel();
      break;

    case CCosc2Level:
      osc2Level = value;
      osc2Levelstr = map(value, 0, readRes, 0, 127);
      updateOsc2Level();
      break;

    case CCosc1Level:
      osc1Level = value;
      osc1Levelstr = map(value, 0, readRes, 0, 127);
      updateOsc1Level();
      break;

    case CCKeyTrack:
      keyTrack = value;
      keyTrackstr = map(value, 0, readRes, 0, 127);
      updateKeyTrack();
      break;

    case CCfilterCutoff:
      filterCutoff = value;
      filterCutoffstr = map(value, 0, readRes, 0, 127);
      filterCutoffstr = FILTERCUTOFF[filterCutoffstr];
      updateFilterCutoff();
      break;

    case CCfilterLFO:
      filterLFO = value;
      filterLFOstr = map(value, 0, readRes, 0, 127);
      updatefilterLFO();
      break;

    case CCfilterRes:
      filterRes = value;
      filterResstr = map(value, 0, readRes, 0, 127);
      updatefilterRes();
      break;

    case CCfilterType:
      updateFilterType();
      break;

    case CCfilterEGlevel:
      filterEGlevel = value;
      filterEGlevelstr = map(value, 0, readRes, 0, 127);
      updatefilterEGlevel();
      break;

    case CCLFORate:
      LFORatestr = map(value, 0, readRes, 0, 127);
      LFORatedisplay = LFOTEMPO[LFORatestr];  // for display
      LFORate = value;
      updateLFORate();
      break;

    case CCLFOWaveformButton:
      updateStratusLFOWaveform();
      break;

    case CCosc1osc2fmDepth:
      value > 0 ? osc1osc2fmDepth = 1 : osc1osc2fmDepth = 0;
      updateosc1osc2fmDepth();
      break;

    case CCfilterAttack:
      filterAttack = value;
      filterAttackstr = map(value, 0, readRes, 0, 127);
      filterAttackstr = ENVTIMES[filterAttackstr];
      updatefilterAttack();
      break;

    case CCfilterDecay:
      filterDecay = value;
      filterDecaystr = map(value, 0, readRes, 0, 127);
      filterDecaystr = ENVTIMES[filterDecaystr];
      updatefilterDecay();
      break;

    case CCfilterSustain:
      filterSustain = value;
      filterSustainstr = map(value, 0, readRes, 0, 127);
      filterSustainstr = LINEAR_FILTERMIXERSTR[filterSustainstr];
      updatefilterSustain();
      break;

    case CCfilterRelease:
      filterRelease = value;
      filterReleasestr = map(value, 0, readRes, 0, 127);
      filterReleasestr = ENVTIMES[filterReleasestr];
      updatefilterRelease();
      break;

    case CCampAttack:
      ampAttack = value;
      oldampAttack = value;
      ampAttackstr = map(value, 0, readRes, 0, 127);
      ampAttackstr = ENVTIMES[ampAttackstr];
      updateampAttack();
      break;

    case CCampDecay:
      ampDecay = value;
      oldampDecay = value;
      ampDecaystr = map(value, 0, readRes, 0, 127);
      ampDecaystr = ENVTIMES[ampDecaystr];
      updateampDecay();
      break;

    case CCampSustain:
      ampSustain = value;
      oldampSustain = value;
      ampSustainstr = map(value, 0, readRes, 0, 127);
      ampSustainstr = LINEAR_FILTERMIXERSTR[ampSustainstr];
      updateampSustain();
      break;

    case CCampRelease:
      ampRelease = value;
      oldampRelease = value;
      ampReleasestr = map(value, 0, readRes, 0, 127);
      ampReleasestr = ENVTIMES[ampReleasestr];
      updateampRelease();
      break;

    case CCvolumeControl:
      volumeControl = value;
      volumeControlstr = map(value, 0, readRes, 0, 127);
      updatevolumeControl();
      break;

    case CClfoDelay:
      lfoDelay = value;
      lfoDelaystr = map(value, 0, readRes, 0, 127);
      updatelfoDelay();
      break;

    case CCamDepth:
      amDepth = value;
      amDepthstr = map(value, 0, readRes, 0, 127);
      updateamDepth();
      break;

    case CCmodwheel:
      switch (modWheelDepth) {

        case 0:
          osc1fmDepth = 0;
          osc2fmDepth = 0;
          break;

        case 1:
          modWheelLevel = (value / 5);
          osc1fmDepth = (int(modWheelLevel));
          osc2fmDepth = (int(modWheelLevel));
          break;

        case 2:
          modWheelLevel = (value / 4);
          osc1fmDepth = (int(modWheelLevel));
          osc2fmDepth = (int(modWheelLevel));
          break;

        case 3:
          modWheelLevel = (value / 3.5);
          osc1fmDepth = (int(modWheelLevel));
          osc2fmDepth = (int(modWheelLevel));
          break;

        case 4:
          modWheelLevel = (value / 3);
          osc1fmDepth = (int(modWheelLevel));
          osc2fmDepth = (int(modWheelLevel));
          break;

        case 5:
          modWheelLevel = (value / 2.5);
          osc1fmDepth = (int(modWheelLevel));
          osc2fmDepth = (int(modWheelLevel));
          break;

        case 6:
          modWheelLevel = (value / 2);
          osc1fmDepth = (int(modWheelLevel));
          osc2fmDepth = (int(modWheelLevel));
          break;

        case 7:
          modWheelLevel = (value / 1.75);
          osc1fmDepth = (int(modWheelLevel));
          osc2fmDepth = (int(modWheelLevel));
          break;

        case 8:
          modWheelLevel = (value / 1.5);
          osc1fmDepth = (int(modWheelLevel));
          osc2fmDepth = (int(modWheelLevel));
          break;

        case 9:
          modWheelLevel = (value / 1.25);
          osc1fmDepth = (int(modWheelLevel));
          osc2fmDepth = (int(modWheelLevel));
          break;

        case 10:
          modWheelLevel = value;
          osc1fmDepth = (int(modWheelLevel));
          osc2fmDepth = (int(modWheelLevel));
          break;
      }
      break;

      ////////////////////////////////////////////////

    case CCosc1WaveSelect:
      updateosc1WaveSelect();
      break;

    case CCosc2WaveSelect:
      updateosc2WaveSelect();
      break;

    case CCglideSW:
      value > 0 ? glideSW = 1 : glideSW = 0;
      updateglideSW();
      break;

    case CCfilterPoleSW:
      value > 0 ? filterPoleSW = 1 : filterPoleSW = 0;
      updatefilterPoleSwitch();
      break;

    case CCfilterEGinv:
      value > 0 ? filterEGinv = 1 : filterEGinv = 0;
      updatefilterEGinv();
      break;

    case CCosc1Bank:
      updateosc1Bank();
      break;

    case CCosc2Bank:
      updateosc2Bank();
      break;

    case CCosc1Oct:
      updateosc1Range();
      break;

    case CCosc2Oct:
      updateosc2Range();
      break;

    case CClfoAlt:
      value > 0 ? lfoAlt = 1 : lfoAlt = 0;
      updatelfoAlt();
      break;

    case CClfoMult:
      updatelfoMult();
      break;

    case CCfilterVelSW:
      value > 0 ? filterVelSW = 1 : filterVelSW = 0;
      updatefilterVelSW();
      break;

    case CCampVelSW:
      value > 0 ? ampVelSW = 1 : ampVelSW = 0;
      updateampVelSW();
      break;

    case CCFilterLoop:
      updateFilterLoop();
      break;

    case CCAmpLoop:
      updateAmpLoop();
      break;

    case CCAmpGatedSW:
      value > 0 ? AmpGatedSW = 1 : AmpGatedSW = 0;
      updateAmpGatedSW();
      break;

    case CCeffectBankSW:
      updateeffectBankSW();
      break;

    case CCeffectNumSW:
      updateeffectNumSW();
      break;

    case CCenvLinLogSW:
      value > 0 ? envLinLogSW = 1 : envLinLogSW = 0;
      updateenvLinLogSW();
      break;

    case CCkeyboardMode:
      updatekeyboardMode();
      break;

    case CCNotePriority:
      updateNotePriority();
      break;

    case CCmonoMultiSW:
      value > 0 ? monoMultiSW = 1 : monoMultiSW = 0;
      updatemonoMultiSW();
      break;

    case CCallnotesoff:
      allNotesOff();
      break;
  }
}

void myProgramChange(byte channel, byte program) {
  state = PATCH;
  patchNo = program + 1;
  recallPatch(patchNo);
  Serial.print("MIDI Pgm Change:");
  Serial.println(patchNo);
  state = PARAMETER;
}

void recallPatch(int patchNo) {
  allNotesOff();
  MIDI.sendProgramChange(0, midiOutCh);
  delay(50);
  recallPatchFlag = true;
  File patchFile = SD.open(String(patchNo).c_str());
  if (!patchFile) {
    Serial.println("File not found");
  } else {
    String data[NO_OF_PARAMS];  //Array of data read in
    recallPatchData(patchFile, data);
    setCurrentPatchData(data);
    patchFile.close();
  }
  recallPatchFlag = false;
}

void setCurrentPatchData(String data[]) {
  patchName = data[0];
  osc1Tune = data[1].toFloat();
  osc1fmDepth = data[2].toFloat();
  osc2Tune = data[3].toFloat();
  osc1WaveMod = data[4].toFloat();
  osc2WaveMod = data[5].toFloat();
  osc1WaveSelect = data[6].toFloat();
  osc2WaveSelect = data[7].toFloat();
  glideTime = data[8].toFloat();
  filterLogLin = data[9].toInt();
  noiseLevel = data[10].toFloat();
  osc2Level = data[11].toFloat();
  osc1Level = data[12].toFloat();
  osc1fmWaveMod = data[13].toFloat();
  ampLogLin = data[14].toInt();
  filterCutoff = data[15].toFloat();
  filterLFO = data[16].toFloat();
  filterRes = data[17].toFloat();
  filterType = data[18].toInt();
  filterA = data[19].toFloat();
  filterB = data[20].toFloat();
  filterC = data[21].toFloat();
  filterEGlevel = data[22].toFloat();
  LFORate = data[23].toFloat();
  LFOWaveform = data[24].toInt();
  filterAttack = data[25].toFloat();
  filterDecay = data[26].toFloat();
  filterSustain = data[27].toFloat();
  filterRelease = data[28].toFloat();
  ampAttack = data[29].toInt();
  ampDecay = data[30].toInt();
  ampSustain = data[31].toInt();
  ampRelease = data[32].toInt();
  volumeControl = data[33].toFloat();
  osc1Bank = data[34].toInt();
  keyTrack = data[35].toFloat();
  filterPoleSW = data[36].toInt();
  FilterLoop = data[37].toInt();
  filterEGinv = data[38].toInt();
  osc1BankB = data[39].toInt();
  AmpLoop = data[40].toInt();
  osc2Bank = data[41].toInt();
  osc2BankB = data[42].toInt();
  lfoAlt = data[43].toInt();
  osc1WaveA = data[44].toInt();
  osc1WaveB = data[45].toInt();
  osc1WaveC = data[46].toInt();
  modWheelLevel = data[47].toFloat();
  PitchBendLevel = data[48].toFloat();
  oct1 = data[49].toInt();
  filterVelSW = data[50].toInt();
  oct2 = data[51].toInt();
  ampVelSW = data[52].toInt();
  osc2WaveA = data[53].toInt();
  osc2WaveB = data[54].toInt();
  osc2WaveC = data[55].toInt();
  AfterTouchDest = data[56].toInt();
  osc2fmDepth = data[57].toFloat();
  osc2fmWaveMod = data[58].toFloat();
  effect1 = data[59].toFloat();
  effect2 = data[60].toFloat();
  effect3 = data[61].toFloat();
  mixa = data[62].toFloat();
  glideSW = data[63].toInt();
  lfoDelay = data[64].toFloat();
  lfoMult = data[65].toInt();
  oldampAttack = data[66].toInt();
  oldampDecay = data[67].toInt();
  oldampSustain = data[68].toInt();
  oldampRelease = data[69].toInt();
  effectBankSW = data[70].toInt();
  envLinLogSW = data[71].toInt();
  keyboardMode = data[72].toInt();
  NotePriority = data[73].toInt();
  monoMultiSW = data[74].toInt();
  effectNumSW = data[75].toInt();
  osc1osc2fmDepth = data[76].toInt();
  afterTouchDepth = data[77].toInt();


  oldfilterCutoff = filterCutoff;

  //Switches

  updateosc1Range();
  updateosc2Range();
  updateosc1Bank();
  updateosc2Bank();
  updatefilterPoleSwitch();
  updatefilterEGinv();
  updatelfoAlt();
  updatelfoMult();
  updateFilterType();
  updateFilterLoop();
  updateAmpLoop();
  updateglideSW();
  updatefilterVelSW();
  updateampVelSW();
  updateeffectBankSW();
  updateenvLinLogSW();
  updatekeyboardMode();
  updateNotePriority();
  updatemonoMultiSW();
  updateeffectNumSW();

  if (osc1osc2fmDepth) {
    osc2fmDepth = osc1fmDepth;
  }

  //Patchname
  updatePatchname();

  Serial.print("Set Patch: ");
  Serial.println(patchName);
}

String getCurrentPatchData() {
  return patchName + "," + String(osc1Tune) + "," + String(osc1fmDepth) + "," + String(osc2Tune) + "," + String(osc1WaveMod) + "," + String(osc2WaveMod) + "," + String(osc1WaveSelect) + "," + String(osc2WaveSelect)
         + "," + String(glideTime) + "," + String(filterLogLin) + "," + String(noiseLevel) + "," + String(osc2Level) + "," + String(osc1Level) + "," + String(osc1fmWaveMod) + "," + String(ampLogLin) + "," + String(filterCutoff)
         + "," + String(filterLFO) + "," + String(filterRes) + "," + String(filterType) + "," + String(filterA) + "," + String(filterB) + "," + String(filterC) + "," + String(filterEGlevel) + "," + String(LFORate)
         + "," + String(LFOWaveform) + "," + String(filterAttack) + "," + String(filterDecay) + "," + String(filterSustain) + "," + String(filterRelease) + "," + String(ampAttack) + "," + String(ampDecay)
         + "," + String(ampSustain) + "," + String(ampRelease) + "," + String(volumeControl) + "," + String(osc1Bank) + "," + String(keyTrack) + "," + String(filterPoleSW) + "," + String(FilterLoop)
         + "," + String(filterEGinv) + "," + String(osc1BankB) + "," + String(AmpLoop) + "," + String(osc2Bank) + "," + String(osc2BankB) + "," + String(lfoAlt) + "," + String(osc1WaveA) + "," + String(osc1WaveB)
         + "," + String(osc1WaveC) + "," + String(modWheelLevel) + "," + String(PitchBendLevel) + "," + String(oct1) + "," + String(filterVelSW) + "," + String(oct2) + "," + String(ampVelSW) + "," + String(osc2WaveA)
         + "," + String(osc2WaveB) + "," + String(osc2WaveC) + "," + String(AfterTouchDest) + "," + String(osc2fmDepth) + "," + String(osc2fmWaveMod) + "," + String(effect1) + "," + String(effect2)
         + "," + String(effect3) + "," + String(mixa) + "," + String(glideSW) + "," + String(lfoDelay) + "," + String(lfoMult) + "," + String(oldampAttack) + "," + String(oldampDecay)
         + "," + String(oldampSustain) + "," + String(oldampRelease) + "," + String(effectBankSW) + "," + String(envLinLogSW) + "," + String(keyboardMode) + "," + String(NotePriority) + "," + String(monoMultiSW)
         + "," + String(effectNumSW) + "," + String(osc1osc2fmDepth) + "," + String(AfterTouchDepth);
}

void checkMux() {

  mux1Read = adc->adc0->analogRead(MUX1_S);
  mux2Read = adc->adc0->analogRead(MUX2_S);
  mux3Read = adc->adc1->analogRead(MUX3_S);
  mux4Read = adc->adc1->analogRead(MUX4_S);
  mux5Read = adc->adc1->analogRead(MUX5_S);

  if (mux1Read > (mux1ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux1Read < (mux1ValuesPrev[muxInput] - QUANTISE_FACTOR)) {
    mux1ValuesPrev[muxInput] = mux1Read;
    mux1ReadMIDI = (mux1Read >> resolutionFrig);  // Change range to 0-127

    switch (muxInput) {
      case MUX1_volumeControl:
        midiCCOut(CCvolumeControl, mux1ReadMIDI);
        myControlChange(midiChannel, CCvolumeControl, mux1Read);
        break;
      case MUX1_ampAttack:
        midiCCOut(CCampAttack, mux1ReadMIDI);
        myControlChange(midiChannel, CCampAttack, mux1Read);
        break;
      case MUX1_ampDecay:
        midiCCOut(CCampDecay, mux1ReadMIDI);
        myControlChange(midiChannel, CCampDecay, mux1Read);
        break;
      case MUX1_spare3:
        break;
      case MUX1_ampSustain:
        midiCCOut(CCampSustain, mux1ReadMIDI);
        myControlChange(midiChannel, CCampSustain, mux1Read);
        break;
      case MUX1_effectMix:
        midiCCOut(CCeffectMix, mux1ReadMIDI);
        myControlChange(midiChannel, CCeffectMix, mux1Read);
        break;
      case MUX1_ampRelease:
        midiCCOut(CCampRelease, mux1ReadMIDI);
        myControlChange(midiChannel, CCampRelease, mux1Read);
        break;
      case MUX1_amDepth:
        midiCCOut(CCamDepth, mux1ReadMIDI);
        myControlChange(midiChannel, CCamDepth, mux1Read);
        break;
    }
  }

  if (mux2Read > (mux2ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux2Read < (mux2ValuesPrev[muxInput] - QUANTISE_FACTOR)) {
    mux2ValuesPrev[muxInput] = mux2Read;
    mux2ReadMIDI = (mux2Read >> resolutionFrig);  // Change range to 0-127

    switch (muxInput) {
      case MUX2_filterSustain:
        midiCCOut(CCfilterSustain, mux2ReadMIDI);
        myControlChange(midiChannel, CCfilterSustain, mux2Read);
        break;
      case MUX2_filterDecay:
        midiCCOut(CCfilterDecay, mux2ReadMIDI);
        myControlChange(midiChannel, CCfilterDecay, mux2Read);
        break;
      case MUX2_filterAttack:
        midiCCOut(CCfilterAttack, mux2ReadMIDI);
        myControlChange(midiChannel, CCfilterAttack, mux2Read);
        break;
      case MUX2_filterRelease:
        midiCCOut(CCfilterRelease, mux2ReadMIDI);
        myControlChange(midiChannel, CCfilterRelease, mux2Read);
        break;
      case MUX2_Resonance:
        midiCCOut(CCfilterRes, mux2ReadMIDI);
        myControlChange(midiChannel, CCfilterRes, mux2Read);
        break;
      case MUX2_keyTrack:
        midiCCOut(CCKeyTrack, mux2ReadMIDI);
        myControlChange(midiChannel, CCKeyTrack, mux2Read);
        break;
      case MUX2_filterEGlevel:
        midiCCOut(CCfilterEGlevel, mux2ReadMIDI);
        myControlChange(midiChannel, CCfilterEGlevel, mux2Read);
        break;
      case MUX2_spare7:
        break;
    }
  }

  if (mux3Read > (mux3ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux3Read < (mux3ValuesPrev[muxInput] - QUANTISE_FACTOR)) {
    mux3ValuesPrev[muxInput] = mux3Read;
    mux3ReadMIDI = (mux3Read >> resolutionFrig);  // Change range to 0-127

    switch (muxInput) {
      case MUX3_spare0:
        break;
      case MUX3_tmDepth:
        midiCCOut(CCfilterLFO, mux3ReadMIDI);
        myControlChange(midiChannel, CCfilterLFO, mux3Read);
        break;
      case MUX3_filterCutoff:
        midiCCOut(CCfilterCutoff, mux3ReadMIDI);
        myControlChange(midiChannel, CCfilterCutoff, mux3Read);
        break;
      case MUX3_spare3:
        break;
      case MUX3_osc1Level:
        midiCCOut(CCosc1Level, mux3ReadMIDI);
        myControlChange(midiChannel, CCosc1Level, mux3Read);
        break;
      case MUX3_spare5:
        break;
      case MUX3_osc2Level:
        midiCCOut(CCosc2Level, mux3ReadMIDI);
        myControlChange(midiChannel, CCosc2Level, mux3Read);
        break;
      case MUX3_effect3:
        midiCCOut(CCeffect3, mux3ReadMIDI);
        myControlChange(midiChannel, CCeffect3, mux3Read);
        break;
    }
  }

  if (mux4Read > (mux4ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux4Read < (mux4ValuesPrev[muxInput] - QUANTISE_FACTOR)) {
    mux4ValuesPrev[muxInput] = mux4Read;
    mux4ReadMIDI = (mux4Read >> resolutionFrig);  // Change range to 0-127

    switch (muxInput) {
      case MUX4_spare0:
        break;
      case MUX4_effect2:
        midiCCOut(CCeffect2, mux4ReadMIDI);
        myControlChange(midiChannel, CCeffect2, mux4Read);
        break;
      case MUX4_osc2fmDepth:
        midiCCOut(CCosc2fmDepth, mux4ReadMIDI);
        myControlChange(midiChannel, CCosc2fmDepth, mux4Read);
        break;
      case MUX4_spare3:
        break;
      case MUX4_osc1fmDepth:
        midiCCOut(CCosc1fmDepth, mux4ReadMIDI);
        myControlChange(midiChannel, CCosc1fmDepth, mux4Read);
        break;
      case MUX4_effect1:
        midiCCOut(CCeffect1, mux4ReadMIDI);
        myControlChange(midiChannel, CCeffect1, mux4Read);
        break;
      case MUX4_osc1ModDepth:
        midiCCOut(CCosc1fmWaveMod, mux4ReadMIDI);
        myControlChange(midiChannel, CCosc1fmWaveMod, mux4Read);
        break;
      case MUX4_osc2ModDepth:
        midiCCOut(CCosc2fmWaveMod, mux4ReadMIDI);
        myControlChange(midiChannel, CCosc2fmWaveMod, mux4Read);
        break;
    }
  }

  if (mux5Read > (mux5ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux5Read < (mux5ValuesPrev[muxInput] - QUANTISE_FACTOR)) {
    mux5ValuesPrev[muxInput] = mux5Read;
    mux5ReadMIDI = (mux5Read >> resolutionFrig);  // Change range to 0-127

    switch (muxInput) {
      case MUX5_lfoDelay:
        midiCCOut(CClfoDelay, mux5ReadMIDI);
        myControlChange(midiChannel, CClfoDelay, mux5Read);
        break;
      case MUX5_noiseLevel:
        midiCCOut(CCnoiseLevel, mux5ReadMIDI);
        myControlChange(midiChannel, CCnoiseLevel, mux5Read);
        break;
      case MUX5_glideTime:
        midiCCOut(CCglideTime, mux5ReadMIDI);
        myControlChange(midiChannel, CCglideTime, mux5Read);
        break;
      case MUX5_lfoRate:
        midiCCOut(CCLFORate, mux5ReadMIDI);
        myControlChange(midiChannel, CCLFORate, mux5Read);
        break;
      case MUX5_osc1Mod:
        midiCCOut(CCosc1WaveMod, mux5ReadMIDI);
        myControlChange(midiChannel, CCosc1WaveMod, mux5Read);
        break;
      case MUX5_osc2Mod:
        midiCCOut(CCosc2WaveMod, mux5ReadMIDI);
        myControlChange(midiChannel, CCosc2WaveMod, mux5Read);
        break;
      case MUX5_osc1Freq:
        midiCCOut(CCosc1Tune, mux5ReadMIDI);
        myControlChange(midiChannel, CCosc1Tune, mux5Read);
        break;
      case MUX5_osc2Freq:
        midiCCOut(CCosc2Tune, mux5ReadMIDI);
        myControlChange(midiChannel, CCosc2Tune, mux5Read);
        break;
    }
  }

  muxInput++;
  if (muxInput >= MUXCHANNELS)
    muxInput = 0;

  digitalWriteFast(MUX_0, muxInput & B0001);
  digitalWriteFast(MUX_1, muxInput & B0010);
  digitalWriteFast(MUX_2, muxInput & B0100);
  //delayMicroseconds(75);
}

void writeDemux() {

  switch (muxOutput) {
    case 0:
      switch (LFODelayGo) {
        case 1:
          sample_data1 = (channel_a & 0xFFF0000F) | (((int(osc1fmDepth * DACMULT)) & 0xFFFF) << 4);
          break;

        case 0:
          sample_data1 = (channel_a & 0xFFF0000F) | ((0 & 0xFFFF) << 4);
          break;
      }
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_1, LOW);
      sample_data1 = (channel_c & 0xFFF0000F) | (((int(filterAttack * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;

    case 1:
      sample_data1 = (channel_a & 0xFFF0000F) | (((int(osc2fmWaveMod * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_1, LOW);

      sample_data1 = (channel_c & 0xFFF0000F) | (((int(filterDecay * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;

    case 2:
      sample_data1 = (channel_a & 0xFFF0000F) | (((int(osc1fmWaveMod * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_1, LOW);

      sample_data1 = (channel_c & 0xFFF0000F) | (((int(filterSustain * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;

    case 3:
      sample_data1 = (channel_a & 0xFFF0000F) | (((int(osc1Tune * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_1, LOW);

      sample_data1 = (channel_c & 0xFFF0000F) | (((int(filterRelease * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;

    case 4:
      sample_data1 = (channel_a & 0xFFF0000F) | (((int(osc2Tune * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_1, LOW);

      sample_data1 = (channel_c & 0xFFF0000F) | (((int(ampAttack * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;

    case 5:
      sample_data1 = (channel_a & 0xFFF0000F) | (((int(noiseLevel * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_1, LOW);

      sample_data1 = (channel_c & 0xFFF0000F) | (((int(ampDecay * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;

    case 6:
      switch (LFODelayGo) {
        case 1:
          sample_data1 = (channel_a & 0xFFF0000F) | (((int(filterLFO * DACMULT)) & 0xFFFF) << 4);
          break;

        case 0:
          sample_data1 = (channel_a & 0xFFF0000F) | ((0 & 0xFFFF) << 4);
          break;
      }
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_1, LOW);

      sample_data1 = (channel_c & 0xFFF0000F) | (((int(ampSustain * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;

    case 7:
      sample_data1 = (channel_a & 0xFFF0000F) | (((int(volumeControl * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_1, LOW);

      sample_data1 = (channel_c & 0xFFF0000F) | (((int(ampRelease * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;

    case 8:
      sample_data1 = (channel_a & 0xFFF0000F) | (((int(osc1Level * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_1, LOW);

      // reuse PWMLFO rate for FM2depth
      switch (LFODelayGo) {
        case 1:
          sample_data1 = (channel_c & 0xFFF0000F) | (((int(osc2fmDepth * DACMULT)) & 0xFFFF) << 4);
          break;

        case 0:
          sample_data1 = (channel_c & 0xFFF0000F) | ((0 & 0xFFFF) << 4);
          break;
      }
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;

    case 9:
      sample_data1 = (channel_a & 0xFFF0000F) | (((int(mixa * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_1, LOW);

      sample_data1 = (channel_c & 0xFFF0000F) | (((int(LFORate * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;

    case 10:
      sample_data1 = (channel_a & 0xFFF0000F) | (((int(osc2Level * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_1, LOW);

      sample_data1 = (channel_c & 0xFFF0000F) | (((int((LFOWaveCV)*DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;

    case 11:
      sample_data1 = (channel_a & 0xFFF0000F) | (((int(effect1 * DACMULT3)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_1, LOW);

      sample_data1 = (channel_c & 0xFFF0000F) | (((int(filterEGlevel * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;

    case 12:
      sample_data1 = (channel_a & 0xFFF0000F) | (((int(LFOMultCV * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_1, LOW);

      sample_data1 = (channel_c & 0xFFF0000F) | (((int(filterCutoff * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;

    case 13:
      sample_data1 = (channel_a & 0xFFF0000F) | (((int(osc1WaveMod * DACMULT2)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_1, LOW);

      sample_data1 = (channel_c & 0xFFF0000F) | (((int(filterRes * DACMULT)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;

    case 14:
      sample_data1 = (channel_a & 0xFFF0000F) | (((int(osc2WaveMod * DACMULT2)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_1, LOW);

      sample_data1 = (channel_c & 0xFFF0000F) | (((int(effect2 * DACMULT3)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;

    case 15:
      switch (LFODelayGo) {
        case 1:
          sample_data1 = (channel_a & 0xFFF0000F) | (((int(amDepth * DACMULT)) & 0xFFFF) << 4);
          break;

        case 0:
          sample_data1 = (channel_a & 0xFFF0000F) | ((0 & 0xFFFF) << 4);
          break;
      }
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_1, LOW);

      sample_data1 = (channel_c & 0xFFF0000F) | (((int(effect3 * DACMULT3)) & 0xFFFF) << 4);
      outputDAC(DAC_CS1, sample_data1);
      digitalWriteFast(DEMUX_EN_2, LOW);
      break;
  }
  delayMicroseconds(800);
  digitalWriteFast(DEMUX_EN_1, HIGH);
  digitalWriteFast(DEMUX_EN_2, HIGH);

  muxOutput++;
  if (muxOutput >= DEMUXCHANNELS)

    muxOutput = 0;

  digitalWriteFast(DEMUX_0, muxOutput & B0001);
  digitalWriteFast(DEMUX_1, muxOutput & B0010);
  digitalWriteFast(DEMUX_2, muxOutput & B0100);
  digitalWriteFast(DEMUX_3, muxOutput & B1000);
}

void onButtonPress(uint16_t btnIndex, uint8_t btnType) {

  if (btnIndex == GLIDE_SW && btnType == ROX_PRESSED) {
    glideSW = !glideSW;
    myControlChange(midiChannel, CCglideSW, glideSW);
  }

  if (btnIndex == FILTER_POLE_SW && btnType == ROX_PRESSED) {
    filterPoleSW = !filterPoleSW;
    myControlChange(midiChannel, CCfilterPoleSW, filterPoleSW);
  }

  if (btnIndex == EG_INVERT_SW && btnType == ROX_PRESSED) {
    filterEGinv = !filterEGinv;
    myControlChange(midiChannel, CCfilterEGinv, filterEGinv);
  }

  if (btnIndex == DCO1_OCT_SW && btnType == ROX_PRESSED) {
    oct1 = oct1 + 1;
    if (oct1 > 2) {
      oct1 = 0;
    }
    myControlChange(midiChannel, CCosc1Oct, oct1);
  }

  if (btnIndex == DCO2_OCT_SW && btnType == ROX_PRESSED) {
    oct2 = oct2 + 1;
    if (oct2 > 2) {
      oct2 = 0;
    }
    myControlChange(midiChannel, CCosc2Oct, oct2);
  }

  if (btnIndex == DCO1_MODE_SW && btnType == ROX_PRESSED) {
    osc1Bank = osc1Bank + 1;
    if (osc1Bank > 2) {
      osc1Bank = 0;
    }
    myControlChange(midiChannel, CCosc1Bank, osc1Bank);
  }

  if (btnIndex == DCO2_MODE_SW && btnType == ROX_PRESSED) {
    osc2Bank = osc2Bank + 1;
    if (osc2Bank > 2) {
      osc2Bank = 0;
    }
    myControlChange(midiChannel, CCosc2Bank, osc2Bank);
  }

  if (btnIndex == FILTER_TYPE_SW && btnType == ROX_PRESSED) {
    filterType = filterType + 1;
    if (filterType > 7) {
      filterType = 0;
    }
    myControlChange(midiChannel, CCfilterType, filterType);
  }

  if (btnIndex == LFO_ALT_SW && btnType == ROX_PRESSED) {
    lfoAlt = !lfoAlt;
    myControlChange(midiChannel, CClfoAlt, lfoAlt);
  }

  if (btnIndex == LFO_MULT_SW && btnType == ROX_PRESSED) {
    lfoMult = lfoMult + 1;
    if (lfoMult > 2) {
      lfoMult = 0;
    }
    myControlChange(midiChannel, CClfoMult, lfoMult);
  }

  if (btnIndex == LFO_WAVE_SW && btnType == ROX_RELEASED) {
    LFOWaveform = LFOWaveform + 1;
    if (LFOWaveform > 7) {
      LFOWaveform = 0;
    }
    myControlChange(midiChannel, CCLFOWaveformButton, LFOWaveform);
  } else if (btnIndex == LFO_WAVE_SW && btnType == ROX_HELD) {
    osc1osc2fmDepth = !osc1osc2fmDepth;
    myControlChange(midiChannel, CCosc1osc2fmDepth, osc1osc2fmDepth);
  }

  if (btnIndex == DCO1_WAVE_SW && btnType == ROX_PRESSED) {
    osc1WaveSelect = osc1WaveSelect + 1;
    if (osc1WaveSelect > 7) {
      osc1WaveSelect = 0;
    }
    myControlChange(midiChannel, CCosc1WaveSelect, osc1WaveSelect);
  }

  if (btnIndex == DCO2_WAVE_SW && btnType == ROX_PRESSED) {
    osc2WaveSelect = osc2WaveSelect + 1;
    if (osc2WaveSelect > 7) {
      osc2WaveSelect = 0;
    }
    myControlChange(midiChannel, CCosc2WaveSelect, osc2WaveSelect);
  }

  if (btnIndex == FILTER_VELOCITY_SW && btnType == ROX_PRESSED) {
    filterVelSW = !filterVelSW;
    myControlChange(midiChannel, CCfilterVelSW, filterVelSW);
  }

  if (btnIndex == VCA_VELOCITY_SW && btnType == ROX_PRESSED) {
    ampVelSW = !ampVelSW;
    myControlChange(midiChannel, CCampVelSW, ampVelSW);
  }

  if (btnIndex == FILTER_LOOP_SW && btnType == ROX_PRESSED) {
    FilterLoop = FilterLoop + 1;
    if (FilterLoop > 2) {
      FilterLoop = 0;
    }
    myControlChange(midiChannel, CCFilterLoop, FilterLoop);
  }

  if (btnIndex == VCA_LOOP_SW && btnType == ROX_PRESSED) {
    AmpLoop = AmpLoop + 1;
    if (AmpLoop > 2) {
      AmpLoop = 0;
    }
    myControlChange(midiChannel, CCAmpLoop, AmpLoop);
  }

  if (btnIndex == VCA_GATE_SW && btnType == ROX_PRESSED) {
    AmpGatedSW = !AmpGatedSW;
    myControlChange(midiChannel, CCAmpGatedSW, AmpGatedSW);
  }

  if (btnIndex == EFFECT_BANK_SW && btnType == ROX_RELEASED) {
    effectNumSW = effectNumSW + 1;
    if (effectNumSW > 7) {
      effectNumSW = 0;
    }
    myControlChange(midiChannel, CCeffectNumSW, effectNumSW);
  } else if (btnIndex == EFFECT_BANK_SW && btnType == ROX_HELD) {
    effectBankSW = effectBankSW + 1;
    if (effectBankSW > 3) {
      effectBankSW = 0;
    }
    myControlChange(midiChannel, CCeffectBankSW, effectBankSW);
  }

  if (btnIndex == LIN_LOG_SW && btnType == ROX_PRESSED) {
    envLinLogSW = !envLinLogSW;
    myControlChange(midiChannel, CCenvLinLogSW, envLinLogSW);
  }

  if (btnIndex == POLYMODE_SW && btnType == ROX_PRESSED) {
    keyboardMode = keyboardMode + 1;
    if (keyboardMode > 2) {
      keyboardMode = 0;
    }
    myControlChange(midiChannel, CCkeyboardMode, keyboardMode);
  }

  if (btnIndex == PRIORITY_SW && btnType == ROX_PRESSED) {
    NotePriority = NotePriority + 1;
    if (NotePriority > 2) {
      NotePriority = 0;
    }
    myControlChange(midiChannel, CCNotePriority, NotePriority);
  }

  if (btnIndex == MONO_MULTI_SW && btnType == ROX_PRESSED) {
    monoMultiSW = !monoMultiSW;
    myControlChange(midiChannel, CCmonoMultiSW, monoMultiSW);
  }
}

void showSettingsPage() {
  showSettingsPage(settings::current_setting(), settings::current_setting_value(), state);
}

void midiCCOut(byte cc, byte value) {
  if (midiOutCh > 0) {
    if (updateParams) {
      usbMIDI.sendControlChange(cc, value, midiOutCh);  //MIDI DIN is set to Out
    }
    MIDI.sendControlChange(cc, value, midiOutCh);  //MIDI DIN is set to Out
  }
}


void checkSwitches() {

  saveButton.update();
  if (saveButton.held()) {
    switch (state) {
      case PARAMETER:
      case PATCH:
        state = DELETE;
        break;
    }
  } else if (saveButton.numClicks() == 1) {
    switch (state) {
      case PARAMETER:
        if (patches.size() < PATCHES_LIMIT) {
          resetPatchesOrdering();  //Reset order of patches from first patch
          patches.push({ patches.size() + 1, INITPATCHNAME });
          state = SAVE;
        }
        break;
      case SAVE:
        //Save as new patch with INITIALPATCH name or overwrite existing keeping name - bypassing patch renaming
        patchName = patches.last().patchName;
        state = PATCH;
        savePatch(String(patches.last().patchNo).c_str(), getCurrentPatchData());
        showPatchPage(patches.last().patchNo, patches.last().patchName);
        patchNo = patches.last().patchNo;
        loadPatches();  //Get rid of pushed patch if it wasn't saved
        setPatchesOrdering(patchNo);
        renamedPatch = "";
        state = PARAMETER;
        break;
      case PATCHNAMING:
        if (renamedPatch.length() > 0) patchName = renamedPatch;  //Prevent empty strings
        state = PATCH;
        savePatch(String(patches.last().patchNo).c_str(), getCurrentPatchData());
        showPatchPage(patches.last().patchNo, patchName);
        patchNo = patches.last().patchNo;
        loadPatches();  //Get rid of pushed patch if it wasn't saved
        setPatchesOrdering(patchNo);
        renamedPatch = "";
        state = PARAMETER;
        break;
    }
  }

  settingsButton.update();
  if (settingsButton.held()) {
    //If recall held, set current patch to match current hardware state
    //Reinitialise all hardware values to force them to be re-read if different
    state = REINITIALISE;
    reinitialiseToPanel();
  } else if (settingsButton.numClicks() == 1) {
    switch (state) {
      case PARAMETER:
        state = SETTINGS;
        showSettingsPage();
        break;
      case SETTINGS:
        showSettingsPage();
      case SETTINGSVALUE:
        settings::save_current_value();
        state = SETTINGS;
        showSettingsPage();
        break;
    }
  }

  backButton.update();
  if (backButton.held()) {
    //If Back button held, Panic - all notes off
  } else if (backButton.numClicks() == 1) {
    switch (state) {
      case RECALL:
        setPatchesOrdering(patchNo);
        state = PARAMETER;
        break;
      case SAVE:
        renamedPatch = "";
        state = PARAMETER;
        loadPatches();  //Remove patch that was to be saved
        setPatchesOrdering(patchNo);
        break;
      case PATCHNAMING:
        charIndex = 0;
        renamedPatch = "";
        state = SAVE;
        break;
      case DELETE:
        setPatchesOrdering(patchNo);
        state = PARAMETER;
        break;
      case SETTINGS:
        state = PARAMETER;
        break;
      case SETTINGSVALUE:
        state = SETTINGS;
        showSettingsPage();
        break;
    }
  }

  //Encoder switch
  recallButton.update();
  if (recallButton.held()) {
    //If Recall button held, return to current patch setting
    //which clears any changes made
    state = PATCH;
    //Recall the current patch
    patchNo = patches.first().patchNo;
    recallPatch(patchNo);
    state = PARAMETER;
  } else if (recallButton.numClicks() == 1) {
    switch (state) {
      case PARAMETER:
        state = RECALL;  //show patch list
        break;
      case RECALL:
        state = PATCH;
        //Recall the current patch
        patchNo = patches.first().patchNo;
        recallPatch(patchNo);
        state = PARAMETER;
        break;
      case SAVE:
        showRenamingPage(patches.last().patchName);
        patchName = patches.last().patchName;
        state = PATCHNAMING;
        break;
      case PATCHNAMING:
        if (renamedPatch.length() < 12)  //actually 12 chars
        {
          renamedPatch.concat(String(currentCharacter));
          charIndex = 0;
          currentCharacter = CHARACTERS[charIndex];
          showRenamingPage(renamedPatch);
        }
        break;
      case DELETE:
        //Don't delete final patch
        if (patches.size() > 1) {
          state = DELETEMSG;
          patchNo = patches.first().patchNo;     //PatchNo to delete from SD card
          patches.shift();                       //Remove patch from circular buffer
          deletePatch(String(patchNo).c_str());  //Delete from SD card
          loadPatches();                         //Repopulate circular buffer to start from lowest Patch No
          renumberPatchesOnSD();
          loadPatches();                      //Repopulate circular buffer again after delete
          patchNo = patches.first().patchNo;  //Go back to 1
          recallPatch(patchNo);               //Load first patch
        }
        state = PARAMETER;
        break;
      case SETTINGS:
        state = SETTINGSVALUE;
        showSettingsPage();
        break;
      case SETTINGSVALUE:
        settings::save_current_value();
        state = SETTINGS;
        showSettingsPage();
        break;
    }
  }
}

void reinitialiseToPanel() {
  //This sets the current patch to be the same as the current hardware panel state - all the pots
  //The four button controls stay the same state
  //This reinialises the previous hardware values to force a re-read
  muxInput = 0;
  for (int i = 0; i < MUXCHANNELS; i++) {
    mux1ValuesPrev[i] = RE_READ;
    mux2ValuesPrev[i] = RE_READ;
    mux3ValuesPrev[i] = RE_READ;
    mux4ValuesPrev[i] = RE_READ;
    mux5ValuesPrev[i] = RE_READ;
  }
  patchName = INITPATCHNAME;
  showPatchPage("Initial", "Panel Settings");
}

void checkEncoder() {
  //Encoder works with relative inc and dec values
  //Detent encoder goes up in 4 steps, hence +/-3

  long encRead = encoder.read();
  if ((encCW && encRead > encPrevious + 3) || (!encCW && encRead < encPrevious - 3)) {
    switch (state) {
      case PARAMETER:
        state = PATCH;
        patches.push(patches.shift());
        patchNo = patches.first().patchNo;
        recallPatch(patchNo);
        state = PARAMETER;
        break;
      case RECALL:
        patches.push(patches.shift());
        break;
      case SAVE:
        patches.push(patches.shift());
        break;
      case PATCHNAMING:
        if (charIndex == TOTALCHARS) charIndex = 0;  //Wrap around
        currentCharacter = CHARACTERS[charIndex++];
        showRenamingPage(renamedPatch + currentCharacter);
        break;
      case DELETE:
        patches.push(patches.shift());
        break;
      case SETTINGS:
        settings::increment_setting();
        showSettingsPage();
        break;
      case SETTINGSVALUE:
        settings::increment_setting_value();
        showSettingsPage();
        break;
    }
    encPrevious = encRead;
  } else if ((encCW && encRead < encPrevious - 3) || (!encCW && encRead > encPrevious + 3)) {
    switch (state) {
      case PARAMETER:
        state = PATCH;
        patches.unshift(patches.pop());
        patchNo = patches.first().patchNo;
        recallPatch(patchNo);
        state = PARAMETER;
        break;
      case RECALL:
        patches.unshift(patches.pop());
        break;
      case SAVE:
        patches.unshift(patches.pop());
        break;
      case PATCHNAMING:
        if (charIndex == -1)
          charIndex = TOTALCHARS - 1;
        currentCharacter = CHARACTERS[charIndex--];
        showRenamingPage(renamedPatch + currentCharacter);
        break;
      case DELETE:
        patches.unshift(patches.pop());
        break;
      case SETTINGS:
        settings::decrement_setting();
        showSettingsPage();
        break;
      case SETTINGSVALUE:
        settings::decrement_setting_value();
        showSettingsPage();
        break;
    }
    encPrevious = encRead;
  }
}

void getDelayTime() {
  delaytime = lfoDelay;
  if (delaytime <= 0) {
    delaytime = 0.1;
  }
  interval = (delaytime * 10);
}

void LFODelayHandle() {
  // LFO Delay code
  getDelayTime();

  unsigned long currentMillis = millis();
  if (monoMultiSW && !LFODelayGo) {
    if (oldnumberOfNotes < numberOfNotes) {
      previousMillis = currentMillis;
      oldnumberOfNotes = numberOfNotes;
    }
  }
  if (numberOfNotes > 0) {
    if (currentMillis - previousMillis >= interval) {
      LFODelayGo = 1;
    } else {
      LFODelayGo = 0;
    }
  } else {
    LFODelayGo = 1;
    previousMillis = currentMillis;  //reset timer so its ready for the next time
  }
}

int mod(int a, int b) {
  int r = a % b;
  return r < 0 ? r + b : r;
}

void outputDAC(int CHIP_SELECT, uint32_t sample_data) {
  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE1));
  digitalWrite(CHIP_SELECT, LOW);
  SPI.transfer32(sample_data);
  delayMicroseconds(8);  // Settling time delay
  digitalWrite(CHIP_SELECT, HIGH);
  SPI.endTransaction();
}

void loop() {

  checkMux();  // Read the sliders and switches
  writeDemux();
  checkSwitches();      // Read the buttons for the program menus etc
  checkEncoder();       // check the encoder status
  octoswitch.update();  // read all the buttons for the Synth
  trig.update();        // update all the triggers
  srp.update();         // update all the LEDs in the buttons
  sr.update();          // update all the outputs


  // Read all the MIDI ports
  myusb.Task();
  midi1.read();  //USB HOST MIDI Class Compliant
  MIDI.read(midiChannel);
  usbMIDI.read(midiChannel);
  MIDI2.read();
  LFODelayHandle();
}
