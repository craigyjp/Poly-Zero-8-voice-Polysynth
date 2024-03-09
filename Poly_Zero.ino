/*
  poly Zero - Firmware Rev 1.0

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
#define TRIG_PWM -1   // pin 13 on 74HC595

byte ccType = 0;  //(EEPROM)

#include "Settings.h"

int count = 0;  //For MIDI Clk Sync
int DelayForSH3 = 12;
int patchNo = 1;               //Current patch no
int voiceToReturn = -1;        //Initialise
long earliestTime = millis();  //For voice allocation - initialise to now


void setup() {
  SPI.begin();
  octoswitch.begin(PIN_DATA, PIN_LOAD, PIN_CLK);
  octoswitch.setCallback(onButtonPress);
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

  //Read Encoder Direction from EEPROM
  encCW = getEncoderDir();
  //Read MIDI Out Channel from EEPROM
  midiOutCh = getMIDIOutCh();

  pixel.begin();
  pixel.setPixelColor(0, pixel.Color(colour[0][0], colour[0][1], colour[0][2]));
  pixel.setPixelColor(1, pixel.Color(colour[0][0], colour[0][1], colour[0][2]));
  pixel.setPixelColor(2, pixel.Color(colour[0][0], colour[0][1], colour[0][2]));
  pixel.setPixelColor(3, pixel.Color(colour[0][0], colour[0][1], colour[0][2]));
  pixel.show();

  recallPatch(patchNo);  //Load first patch
}

void myNoteOn(byte channel, byte note, byte velocity) {
  MIDI.sendNoteOn(note, velocity, channel);
  if (sendNotes) {
    usbMIDI.sendNoteOn(note, velocity, channel);
  }
}

void myNoteOff(byte channel, byte note, byte velocity) {
  MIDI.sendNoteOff(note, velocity, channel);
  if (sendNotes) {
    usbMIDI.sendNoteOff(note, velocity, channel);
  }
}

void myConvertControlChange(byte channel, byte number, byte value) {
  int newvalue = value;
  myControlChange(channel, number, newvalue);
}

void myPitchBend(byte channel, int bend) {
  MIDI.sendPitchBend(bend, channel);
  if (sendNotes) {
    usbMIDI.sendPitchBend(bend, channel);
  }
}

void myAfterTouch(byte channel, byte pressure) {
  MIDI.sendAfterTouch(pressure, channel);
  if (sendNotes) {
    usbMIDI.sendAfterTouch(pressure, channel);
  }
}

void allNotesOff() {
}

void updateosc1fmWaveMod() {
  showCurrentParameterPage("Wave Mod", int(osc1fmWaveModstr));
}

void updateosc2fmWaveMod() {
  showCurrentParameterPage("Wave Mod", int(osc2fmWaveModstr));
}

void updateosc1fmDepth() {
  showCurrentParameterPage("FM Depth", int(osc1fmDepthstr));
}

void updateosc2fmDepth() {
  showCurrentParameterPage("FM Depth", int(osc2fmDepthstr));
}

void updateosc1Tune() {
  showCurrentParameterPage("OSC1 Tune", String(osc1Tunestr));
}

void updateosc2Tune() {
  showCurrentParameterPage("OSC2 Tune", String(osc2Tunestr));
}

void updateosc1WaveMod() {
  showCurrentParameterPage("OSC1 Mod", String(osc1WaveModstr));
}

void updateosc2WaveMod() {
  showCurrentParameterPage("OSC2 Mod", String(osc2WaveModstr));
}

void updateosc1Range() {
  switch (oct1) {
    case 1:
      showCurrentParameterPage("Osc1 Range", String("-1"));

      srp.writePin(DCO1_OCT_RED_LED, HIGH);
      srp.writePin(DCO1_OCT_GREEN_LED, LOW);
      break;

    case 2:
      showCurrentParameterPage("Osc1 Range", String("0"));

      srp.writePin(DCO1_OCT_RED_LED, HIGH);
      srp.writePin(DCO1_OCT_GREEN_LED, HIGH);
      break;

    case 3:
      showCurrentParameterPage("Osc1 Range", String("+1"));

      srp.writePin(DCO1_OCT_RED_LED, LOW);
      srp.writePin(DCO1_OCT_GREEN_LED, HIGH);
      break;
  }
}

void updateosc2Range() {
  switch (oct2) {
    case 1:
      showCurrentParameterPage("Osc2 Range", String("-1"));

      srp.writePin(DCO2_OCT_RED_LED, HIGH);
      srp.writePin(DCO2_OCT_GREEN_LED, LOW);
      break;

    case 2:
      showCurrentParameterPage("Osc2 Range", String("0"));

      srp.writePin(DCO2_OCT_RED_LED, HIGH);
      srp.writePin(DCO2_OCT_GREEN_LED, HIGH);
      break;

    case 3:
      showCurrentParameterPage("Osc2 Range", String("+1"));

      srp.writePin(DCO2_OCT_RED_LED, LOW);
      srp.writePin(DCO2_OCT_GREEN_LED, HIGH);
      break;
  }
}

void updateosc1Bank() {
  switch (osc1Bank) {
    case 2:
      showCurrentParameterPage("Osc1 Bank", String("FM"));

      srp.writePin(DCO1_MODE_RED_LED, HIGH);
      srp.writePin(DCO1_MODE_GREEN_LED, HIGH);
      break;

    case 1:
      showCurrentParameterPage("Osc1 Bank", String("Fold"));

      srp.writePin(DCO1_MODE_RED_LED, HIGH);
      srp.writePin(DCO1_MODE_GREEN_LED, LOW);
      break;

    case 3:
      showCurrentParameterPage("Osc1 Bank", String("AM"));

      srp.writePin(DCO1_MODE_RED_LED, LOW);
      srp.writePin(DCO1_MODE_GREEN_LED, HIGH);
      break;
  }
  updateosc1WaveSelect();
}

void updateosc2Bank() {
  switch (osc2Bank) {
    case 2:
      showCurrentParameterPage("Osc2 Bank", String("FM"));

      srp.writePin(DCO2_MODE_RED_LED, HIGH);
      srp.writePin(DCO2_MODE_GREEN_LED, HIGH);
      break;

    case 1:
      showCurrentParameterPage("Osc2 Bank", String("Fold"));

      srp.writePin(DCO2_MODE_RED_LED, HIGH);
      srp.writePin(DCO2_MODE_GREEN_LED, LOW);
      break;

    case 3:
      showCurrentParameterPage("Osc2 Bank", String("AM"));

      srp.writePin(DCO2_MODE_RED_LED, LOW);
      srp.writePin(DCO2_MODE_GREEN_LED, HIGH);
      break;
  }
  updateosc2WaveSelect();
}

void updateglideTime() {
  showCurrentParameterPage("Glide Time", String(glideTimedisplay * 10) + " Seconds");
}

void updateglideSW() {
  if (glideSW == 1) {
    if (!recallPatchFlag) {
      showCurrentParameterPage("Glide", "On");
    }
    srp.writePin(GLIDE_LED, HIGH);  // LED on
    midiCCOut(CCglideSW, 127);
  } else {
    srp.writePin(GLIDE_LED, LOW);  // LED off
    if (!recallPatchFlag) {
      showCurrentParameterPage("Glide", "Off");
      midiCCOut(CCglideSW, 127);
    }
  }
}

void updateosc1WaveSelect() {
  switch (osc1Bank) {
    case 1:
      switch (osc1WaveSelect) {
        case 1:
          Oscillator1Waveform = "Sawtooth";
          break;

        case 2:
          Oscillator1Waveform = "Sinewave";
          break;

        case 3:
          Oscillator1Waveform = "Squarewave";
          break;

        case 4:
          Oscillator1Waveform = "Triangle";
          break;

        case 5:
          Oscillator1Waveform = "Octave Saw";
          break;

        case 6:
          Oscillator1Waveform = "FM 1";
          break;

        case 7:
          Oscillator1Waveform = "FM 2";
          break;

        case 8:
          Oscillator1Waveform = "FM 3";
          break;
      }
      break;

    case 2:
      switch (osc1WaveSelect) {
        case 1:
          Oscillator1Waveform = "FM 1";
          break;

        case 2:
          Oscillator1Waveform = "FM 2";
          break;

        case 3:
          Oscillator1Waveform = "FM 3";
          break;

        case 4:
          Oscillator1Waveform = "FM 4";
          break;

        case 5:
          Oscillator1Waveform = "FM 5";
          break;

        case 6:
          Oscillator1Waveform = "FM 6";
          break;

        case 7:
          Oscillator1Waveform = "FM 7";
          break;

        case 8:
          Oscillator1Waveform = "FM 8";
          break;
      }
      break;

    case 3:
      switch (osc1WaveSelect) {
        case 1:
          Oscillator1Waveform = "Sawtooth";
          break;

        case 2:
          Oscillator1Waveform = "FM 1";
          break;

        case 3:
          Oscillator1Waveform = "FM 2";
          break;

        case 4:
          Oscillator1Waveform = "FM 3";
          break;

        case 5:
          Oscillator1Waveform = "FM 1 Multi";
          break;

        case 6:
          Oscillator1Waveform = "FM 2 Multi";
          break;

        case 7:
          Oscillator1Waveform = "FM 3 Multi";
          break;

        case 8:
          Oscillator1Waveform = "FM 3 Multi";
          break;
      }
      break;
  }
  //pixel.clear();
  pixel.setPixelColor(1, pixel.Color(colour[osc1WaveSelect -1][0], colour[osc1WaveSelect -1][1], colour[osc1WaveSelect -1][2]));
  pixel.show();
  showCurrentParameterPage("OSC1 Wave", Oscillator1Waveform);
}

void updateosc2WaveSelect() {
  switch (osc2Bank) {
    case 1:
      switch (osc2WaveSelect) {
        case 1:
          Oscillator2Waveform = "Sawtooth";
          break;

        case 2:
          Oscillator2Waveform = "Sinewave";
          break;

        case 3:
          Oscillator2Waveform = "Squarewave";
          break;

        case 4:
          Oscillator2Waveform = "Triangle";
          break;

        case 5:
          Oscillator2Waveform = "Octave Saw";
          break;

        case 6:
          Oscillator2Waveform = "FM 1";
          break;

        case 7:
          Oscillator2Waveform = "FM 2";
          break;

        case 8:
          Oscillator2Waveform = "FM 3";
          break;
      }
      break;

    case 2:
      switch (osc2WaveSelect) {
        case 1:
          Oscillator2Waveform = "FM 1";
          break;

        case 2:
          Oscillator2Waveform = "FM 2";
          break;

        case 3:
          Oscillator2Waveform = "FM 3";
          break;

        case 4:
          Oscillator2Waveform = "FM 4";
          break;

        case 5:
          Oscillator2Waveform = "FM 5";
          break;

        case 6:
          Oscillator2Waveform = "FM 6";
          break;

        case 7:
          Oscillator2Waveform = "FM 7";
          break;

        case 8:
          Oscillator2Waveform = "FM 8";
          break;
      }
      break;

    case 3:
      switch (osc2WaveSelect) {
        case 1:
          Oscillator2Waveform = "Sawtooth";
          break;

        case 2:
          Oscillator2Waveform = "FM 1";
          break;

        case 3:
          Oscillator2Waveform = "FM 2";
          break;

        case 4:
          Oscillator2Waveform = "FM 3";
          break;

        case 5:
          Oscillator2Waveform = "FM 1 Multi";
          break;

        case 6:
          Oscillator2Waveform = "FM 2 Multi";
          break;

        case 7:
          Oscillator2Waveform = "FM 3 Multi";
          break;

        case 8:
          Oscillator2Waveform = "FM 3 Multi";
          break;
      }
      break;
  }
  //pixel.clear();
  pixel.setPixelColor(2, pixel.Color(colour[osc2WaveSelect -1][0], colour[osc2WaveSelect -1][1], colour[osc2WaveSelect -1][2]));
  pixel.show();
  showCurrentParameterPage("OSC2 Wave", Oscillator2Waveform);
}

void updatenoiseLevel() {
  showCurrentParameterPage("Noise Level", String(noiseLeveldisplay));
}

void updateOsc2Level() {
  showCurrentParameterPage("OSC2 Level", int(osc2Levelstr));
}

void updateOsc1Level() {
  showCurrentParameterPage("OSC1 Level", int(osc1Levelstr));
}

void updateKeyTrack() {
  showCurrentParameterPage("KeyTrack Lev", int(keyTrackstr));
}

void updateFilterCutoff() {
  showCurrentParameterPage("Cutoff", String(filterCutoffstr) + " Hz");
}

void updatefilterLFO() {
  showCurrentParameterPage("TM depth", int(filterLFOstr));
}

void updatefilterRes() {
  showCurrentParameterPage("Resonance", int(filterResstr));
}

void updateeffect1() {
  showCurrentParameterPage("Effect 1", int(effect1str));
}

void updateeffect2() {
  showCurrentParameterPage("Effect 2", int(effect2str));
}

void updateeffect3() {
  showCurrentParameterPage("Effect 3", int(effect3str));
}

void updateeffectMix() {
  showCurrentParameterPage("Effect Mix", "Mix " + String(mixbstr) + " / " + String(mixastr));
}

void updateFilterType() {
  switch (filterType) {
    case 1:
      if (!filterPoleSW) {
        showCurrentParameterPage("Filter Type", String("3P LowPass"));
      } else {
        showCurrentParameterPage("Filter Type", String("4P LowPass"));
      }
      sr.writePin(FILTER_A, LOW);
      sr.writePin(FILTER_B, LOW);
      sr.writePin(FILTER_C, LOW);
      break;

    case 2:
      if (!filterPoleSW) {
        showCurrentParameterPage("Filter Type", String("1P LowPass"));
      } else {
        showCurrentParameterPage("Filter Type", String("2P LowPass"));
      }
      sr.writePin(FILTER_A, HIGH);
      sr.writePin(FILTER_B, LOW);
      sr.writePin(FILTER_C, LOW);
      break;

    case 3:
      if (!filterPoleSW) {
        showCurrentParameterPage("Filter Type", String("3P HP + 1P LP"));
      } else {
        showCurrentParameterPage("Filter Type", String("4P HighPass"));
      }
      sr.writePin(FILTER_A, LOW);
      sr.writePin(FILTER_B, HIGH);
      sr.writePin(FILTER_C, LOW);
      break;

    case 4:
      if (!filterPoleSW) {
        showCurrentParameterPage("Filter Type", String("1P HP + 1P LP"));
      } else {
        showCurrentParameterPage("Filter Type", String("2P HighPass"));
      }
      sr.writePin(FILTER_A, HIGH);
      sr.writePin(FILTER_B, HIGH);
      sr.writePin(FILTER_C, LOW);
      break;

    case 5:
      if (!filterPoleSW) {
        showCurrentParameterPage("Filter Type", String("2P HP + 1P LP"));
      } else {
        showCurrentParameterPage("Filter Type", String("4P BandPass"));
      }
      sr.writePin(FILTER_A, LOW);
      sr.writePin(FILTER_B, LOW);
      sr.writePin(FILTER_C, HIGH);
      break;

    case 6:
      if (!filterPoleSW) {
        showCurrentParameterPage("Filter Type", String("2P BP + 1P LP"));
      } else {
        showCurrentParameterPage("Filter Type", String("2P BandPass"));
      }
      sr.writePin(FILTER_A, HIGH);
      sr.writePin(FILTER_B, LOW);
      sr.writePin(FILTER_C, HIGH);
      break;

    case 7:
      if (!filterPoleSW) {
        showCurrentParameterPage("Filter Type", String("3P AP + 1P LP"));
      } else {
        showCurrentParameterPage("Filter Type", String("3P AllPass"));
      }
      sr.writePin(FILTER_A, LOW);
      sr.writePin(FILTER_B, HIGH);
      sr.writePin(FILTER_C, HIGH);
      break;

    case 8:
      if (!filterPoleSW) {
        showCurrentParameterPage("Filter Type", String("2P Notch + LP"));
      } else {
        showCurrentParameterPage("Filter Type", String("Notch"));
      }
      sr.writePin(FILTER_A, HIGH);
      sr.writePin(FILTER_B, HIGH);
      sr.writePin(FILTER_C, HIGH);
      break;
  }
      //pixel.clear();
      pixel.setPixelColor(0, pixel.Color(colour[filterType -1][0], colour[filterType -1][1], colour[filterType -1][2]));
      pixel.show();
}

void updatefilterEGlevel() {
  showCurrentParameterPage("EG Depth", int(filterEGlevelstr));
}

void updateLFORate() {
  showCurrentParameterPage("LFO Rate", String(LFORatedisplay) + " Hz");
}

void updatelfoDelay() {
  showCurrentParameterPage("LFO Delay", String(lfoDelaystr) + " mS");
}

void updateamDepth() {
  showCurrentParameterPage("AM Depth", String(amDepthstr));
}

void updateStratusLFOWaveform() {
  if (!lfoAlt) {
    switch (LFOWaveform) {
      case 1:
        StratusLFOWaveform = "Saw +Oct";
        break;

      case 2:
        StratusLFOWaveform = "Quad Saw";
        break;

      case 3:
        StratusLFOWaveform = "Quad Pulse";
        break;

      case 4:
        StratusLFOWaveform = "Tri Step";
        break;

      case 5:
        StratusLFOWaveform = "Sine +Oct";
        break;

      case 6:
        StratusLFOWaveform = "Sine +3rd";
        break;

      case 7:
        StratusLFOWaveform = "Sine +4th";
        break;

      case 8:
        StratusLFOWaveform = "Rand Slopes";
        break;
    }
  } else {
    switch (LFOWaveform) {
      case 1:
        StratusLFOWaveform = "Sawtooth Up";
        break;

      case 2:
        StratusLFOWaveform = "Sawtooth Down";
        break;

      case 3:
        StratusLFOWaveform = "Squarewave";
        break;

      case 4:
        StratusLFOWaveform = "Triangle";
        break;

      case 5:
        StratusLFOWaveform = "Sinewave";
        break;

      case 6:
        StratusLFOWaveform = "Sweeps";
        break;

      case 7:
        StratusLFOWaveform = "Lumps";
        break;

      case 8:
        StratusLFOWaveform = "Sample & Hold";
        break;
    }
  }
  //pixel.clear();
  pixel.setPixelColor(3, pixel.Color(colour[LFOWaveform -1][0], colour[LFOWaveform -1][1], colour[LFOWaveform -1][2]));
  pixel.show();
  showCurrentParameterPage("LFO Wave", StratusLFOWaveform);
}

void updatefilterAttack() {
  if (filterAttackstr < 1000) {
    showCurrentParameterPage("VCF Attack", String(int(filterAttackstr)) + " ms");
  } else {
    showCurrentParameterPage("VCF Attack", String(filterAttackstr * 0.001) + " s");
  }
}

void updatefilterDecay() {
  if (filterDecaystr < 1000) {
    showCurrentParameterPage("VCF Decay", String(int(filterDecaystr)) + " ms");
  } else {
    showCurrentParameterPage("VCF Decay", String(filterDecaystr * 0.001) + " s");
  }
}

void updatefilterSustain() {
  showCurrentParameterPage("VCF Sustain", String(filterSustainstr));
}

void updatefilterRelease() {
  if (filterReleasestr < 1000) {
    showCurrentParameterPage("VCF Release", String(int(filterReleasestr)) + " ms");
  } else {
    showCurrentParameterPage("VCF Release", String(filterReleasestr * 0.001) + " s");
  }
}

void updateampAttack() {
  if (ampAttackstr < 1000) {
    showCurrentParameterPage("VCA Attack", String(int(ampAttackstr)) + " ms");
  } else {
    showCurrentParameterPage("VCA Attack", String(ampAttackstr * 0.001) + " s");
  }
}

void updateampDecay() {
  if (ampDecaystr < 1000) {
    showCurrentParameterPage("VCA Decay", String(int(ampDecaystr)) + " ms");
  } else {
    showCurrentParameterPage("VCA Decay", String(ampDecaystr * 0.001) + " s");
  }
}

void updateampSustain() {
  showCurrentParameterPage("VCA Sustain", String(ampSustainstr));
}

void updateampRelease() {
  if (ampReleasestr < 1000) {
    showCurrentParameterPage("VCA Release", String(int(ampReleasestr)) + " ms");
  } else {
    showCurrentParameterPage("VCA Release", String(ampReleasestr * 0.001) + " s");
  }
}

void updatevolumeControl() {
  showCurrentParameterPage("Volume", int(volumeControlstr));
}

////////////////////////////////////////////////////////////////


void updatefilterPoleSwitch() {
  if (filterPoleSW) {
    //showCurrentParameterPage("VCF Pole SW", "On");
    sr.writePin(FILTER_POLE, HIGH);
    srp.writePin(FILTER_POLE_RED_LED, HIGH);
    srp.writePin(FILTER_POLE_GREEN_LED, LOW);
    midiCCOut(CCfilterPoleSW, 127);
    updateFilterType();
  } else {
    //showCurrentParameterPage("VCF Pole SW", "Off");
    sr.writePin(FILTER_POLE, LOW);
    srp.writePin(FILTER_POLE_RED_LED, LOW);
    srp.writePin(FILTER_POLE_GREEN_LED, HIGH);
    midiCCOut(CCfilterPoleSW, 0);
    updateFilterType();
  }
}

void updateFilterLoop() {
  switch (FilterLoop) {
    case 1:
      showCurrentParameterPage("Filter Loop", "Off");
      srp.writePin(FILTER_LOOP_RED_LED, LOW);
      srp.writePin(FILTER_LOOP_GREEN_LED, LOW);
      sr.writePin(FLOOPBIT0, LOW);
      sr.writePin(FLOOPBIT1, LOW);
      break;

    case 2:
      showCurrentParameterPage("Filter Loop", "Gated Mode");
      srp.writePin(FILTER_LOOP_RED_LED, HIGH);
      srp.writePin(FILTER_LOOP_GREEN_LED, LOW);
      sr.writePin(FLOOPBIT0, HIGH);
      sr.writePin(FLOOPBIT1, LOW);
      break;

    case 3:
      showCurrentParameterPage("Filter Loop", "LFO Mode");
      srp.writePin(FILTER_LOOP_RED_LED, LOW);
      srp.writePin(FILTER_LOOP_GREEN_LED, HIGH);
      sr.writePin(FLOOPBIT0, HIGH);
      sr.writePin(FLOOPBIT1, HIGH);
      break;
  }
}

void updatefilterEGinv() {
  if (filterEGinv) {
    showCurrentParameterPage("Filter Env", "Positive");
    sr.writePin(FILTER_EG_INV, LOW);
    srp.writePin(EG_INV_LED, LOW);
    midiCCOut(CCfilterEGinv, 0);
  } else {
    showCurrentParameterPage("Filter Env", "Negative");
    sr.writePin(FILTER_EG_INV, HIGH);
    srp.writePin(EG_INV_LED, HIGH);
    midiCCOut(CCfilterEGinv, 127);
  }
}

void updatefilterVelSW() {
  if (filterVelSW) {
    showCurrentParameterPage("Filter Velocity", "On");
    //sr.writePin(FILTER_EG_INV, LOW);
    srp.writePin(FILTER_VELOCITY_LED, HIGH);
    midiCCOut(CCfilterVelSW, 127);
  } else {
    showCurrentParameterPage("Filter Velocity", "Off");
    //sr.writePin(FILTER_EG_INV, HIGH);
    srp.writePin(FILTER_VELOCITY_LED, LOW);
    midiCCOut(CCfilterVelSW, 0);
  }
}

void updateampVelSW() {
  if (ampVelSW) {
    showCurrentParameterPage("Amp Velocity", "On");
    //sr.writePin(FILTER_EG_INV, LOW);
    srp.writePin(VCA_VELOCITY_LED, HIGH);
    midiCCOut(CCampVelSW, 127);
  } else {
    showCurrentParameterPage("Amp Velocity", "Off");
    //sr.writePin(FILTER_EG_INV, HIGH);
    srp.writePin(VCA_VELOCITY_LED, LOW);
    midiCCOut(CCampVelSW, 0);
  }
}

void updateAmpLoop() {
  switch (AmpLoop) {
    case 1:
      showCurrentParameterPage("VCA Loop", "Off");
      srp.writePin(VCA_LOOP_RED_LED, LOW);
      srp.writePin(VCA_LOOP_GREEN_LED, LOW);
      sr.writePin(ALOOPBIT0, LOW);
      sr.writePin(ALOOPBIT1, LOW);
      break;

    case 2:
      showCurrentParameterPage("VCA Loop", "Gated Mode");
      srp.writePin(VCA_LOOP_RED_LED, HIGH);
      srp.writePin(VCA_LOOP_GREEN_LED, LOW);
      sr.writePin(ALOOPBIT0, HIGH);
      sr.writePin(ALOOPBIT1, LOW);
      break;

    case 3:
      showCurrentParameterPage("VCA Loop", "LFO Mode");
      srp.writePin(VCA_LOOP_RED_LED, LOW);
      srp.writePin(VCA_LOOP_GREEN_LED, HIGH);
      sr.writePin(ALOOPBIT0, HIGH);
      sr.writePin(ALOOPBIT1, HIGH);
      break;
  }
  oldAmpLoop = AmpLoop;
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
  switch (lfoMult) {
    case 2:
      showCurrentParameterPage("Lfo Multi", String("X 1.0"));

      srp.writePin(LFO_MULT_RED_LED, HIGH);
      srp.writePin(LFO_MULT_GREEN_LED, HIGH);
      break;

    case 1:
      showCurrentParameterPage("Lfo Multi", String("X 0.5"));

      srp.writePin(LFO_MULT_RED_LED, HIGH);
      srp.writePin(LFO_MULT_GREEN_LED, LOW);
      break;

    case 3:
      showCurrentParameterPage("Lfo Multi", String("X 1.5"));

      srp.writePin(LFO_MULT_RED_LED, LOW);
      srp.writePin(LFO_MULT_GREEN_LED, HIGH);
      break;
  }
}

void updateeffectBankSW() {
  switch (effectBankSW) {
    case 1:
      showCurrentParameterPage("Effects Bank", String("Factory"));

      srp.writePin(EFFECT_BANK_RED_LED, LOW);
      srp.writePin(EFFECT_BANK_GREEN_LED, LOW);
      break;

    case 2:
      showCurrentParameterPage("Effects Bank", String("User 1"));

      srp.writePin(EFFECT_BANK_RED_LED, HIGH);
      srp.writePin(EFFECT_BANK_GREEN_LED, LOW);
      break;

    case 3:
      showCurrentParameterPage("Effects Bank", String("User 2"));

      srp.writePin(EFFECT_BANK_RED_LED, LOW);
      srp.writePin(EFFECT_BANK_GREEN_LED, HIGH);
      break;

    case 4:
      showCurrentParameterPage("Effects Bank", String("User 3"));

      srp.writePin(EFFECT_BANK_RED_LED, HIGH);
      srp.writePin(EFFECT_BANK_GREEN_LED, HIGH);
      break;
  }
}

void updatefilterLinLogSW() {
  if (filterLinLogSW) {
    showCurrentParameterPage("Filter Envelope", "Linear");
    sr.writePin(FILTER_LIN_LOG, HIGH);
    srp.writePin(FILTER_LINLOG_RED_LED, HIGH);
    srp.writePin(FILTER_LINLOG_GREEN_LED, LOW);
    midiCCOut(CCfilterLinLogSW, 127);
  } else {
    showCurrentParameterPage("Filter Envelope", "Log");
    sr.writePin(FILTER_LIN_LOG, LOW);
    srp.writePin(FILTER_LINLOG_RED_LED, LOW);
    srp.writePin(FILTER_LINLOG_GREEN_LED, HIGH);
    midiCCOut(CCfilterLinLogSW, 0);
  }
}

void updatevcaLinLogSW() {
  if (vcaLinLogSW) {
    showCurrentParameterPage("Amp Envelope", "Linear");
    sr.writePin(AMP_LIN_LOG, HIGH);
    srp.writePin(VCA_LINLOG_RED_LED, HIGH);
    srp.writePin(VCA_LINLOG_GREEN_LED, LOW);
    midiCCOut(CCvcaLinLogSW, 127);
  } else {
    showCurrentParameterPage("Amp Envelope", "Log");
    sr.writePin(AMP_LIN_LOG, LOW);
    srp.writePin(VCA_LINLOG_RED_LED, LOW);
    srp.writePin(VCA_LINLOG_GREEN_LED, HIGH);
    midiCCOut(CCvcaLinLogSW, 0);
  }
}

void updateFilterEnv() {
  switch (filterLogLin) {
    case 0:
      sr.writePin(FILTER_LIN_LOG, LOW);
      break;
    case 1:
      sr.writePin(FILTER_LIN_LOG, HIGH);
      break;
  }
  oldfilterLogLin = filterLogLin;
}

void updateAmpEnv() {
  switch (ampLogLin) {
    case 0:
      sr.writePin(AMP_LIN_LOG, LOW);
      break;
    case 1:
      sr.writePin(AMP_LIN_LOG, HIGH);
      break;
  }
  oldampLogLin = ampLogLin;
}

void updateAmpGatedSW() {
  if (!AmpGatedSW) {
    showCurrentParameterPage("VCA Gate", "Off");
    srp.writePin(VCA_GATE_LED, LOW);  // LED off
    ampAttack = oldampAttack;
    ampDecay = oldampDecay;
    ampSustain = oldampSustain;
    ampRelease = oldampRelease;
    midiCCOut(CCAmpGatedSW, 0);

  } else {
    showCurrentParameterPage("VCA Gate", "On");
    srp.writePin(VCA_GATE_LED, HIGH);  // LED on
    ampAttack = 0;
    ampDecay = 0;
    ampSustain = 1023;
    ampRelease = 0;
    midiCCOut(CCAmpGatedSW, 127);
  }
}

void updatePitchBend() {
  showCurrentParameterPage("Bender Range", int(PitchBendLevelstr));
}

void updatemodWheel() {
  showCurrentParameterPage("Mod Range", int(modWheelLevelstr));
}

void updateAfterTouchDest() {
  switch (AfterTouchDest) {
    case 0:
      sr.writePin(AFTERTOUCH_A, LOW);
      sr.writePin(AFTERTOUCH_B, LOW);
      sr.writePin(AFTERTOUCH_C, LOW);
      break;

    case 1:
      sr.writePin(AFTERTOUCH_A, HIGH);
      sr.writePin(AFTERTOUCH_B, LOW);
      sr.writePin(AFTERTOUCH_C, LOW);
      break;

    case 2:
      sr.writePin(AFTERTOUCH_A, LOW);
      sr.writePin(AFTERTOUCH_B, HIGH);
      sr.writePin(AFTERTOUCH_C, LOW);
      break;

    case 3:
      sr.writePin(AFTERTOUCH_A, HIGH);
      sr.writePin(AFTERTOUCH_B, HIGH);
      sr.writePin(AFTERTOUCH_C, LOW);
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
      osc1Tunestr = map(value, 0, 4095, 0, 127);
      osc1Tunestr = PITCH[osc1Tunestr];  // for display
      updateosc1Tune();
      break;

    case CCosc1fmDepth:
      osc1fmDepth = value;
      osc1fmDepthstr = map(value, 0, 4095, 0, 127);
      updateosc1fmDepth();
      break;

    case CCosc2fmDepth:
      osc2fmDepth = value;
      osc2fmDepthstr = map(value, 0, 4095, 0, 127);
      updateosc2fmDepth();
      break;

    case CCosc2Tune:
      osc2Tune = value;
      osc2Tunestr = map(value, 0, 4095, 0, 127);
      osc2Tunestr = PITCH[osc2Tunestr];  // for display
      updateosc2Tune();
      break;

    case CCosc1WaveMod:
      osc1WaveMod = value;
      osc1WaveModstr = map(value, 0, 4095, 0, 127);
      updateosc1WaveMod();
      break;

    case CCosc2WaveMod:
      osc2WaveMod = value;
      osc2WaveModstr = map(value, 0, 4095, 0, 127);
      updateosc2WaveMod();
      break;

    case CCeffect1:
      effect1 = value;
      effect1str = map(value, 0, 4095, 0, 127);
      updateeffect1();
      break;

    case CCeffect2:
      effect2 = value;
      effect2str = map(value, 0, 4095, 0, 127);
      updateeffect2();
      break;

    case CCeffect3:
      effect3 = value;
      effect3str = map(value, 0, 4095, 0, 127);
      updateeffect3();
      break;

    case CCeffectMix:
      effectMix = value;
      mixa = map(value, 0, 4095, 0, 4095);
      mixb = map(value, 0, 4095, 4095, 0);
      mixastr = map(value, 0, 4095, 0, 127);
      mixbstr = map(value, 0, 4095, 127, 0);
      updateeffectMix();
      break;

    case CCglideTime:
      glideTimestr = map(value, 0, 4095, 0, 127);
      glideTimedisplay = LINEAR[glideTimestr];
      glideTime = value;
      updateglideTime();
      break;

    case CCosc1fmWaveMod:
      osc1fmWaveModstr = map(value, 0, 4095, 0, 127);
      osc1fmWaveMod = value;
      updateosc1fmWaveMod();
      break;

    case CCosc2fmWaveMod:
      osc2fmWaveModstr = map(value, 0, 4095, 0, 127);
      osc2fmWaveMod = value;
      updateosc2fmWaveMod();
      break;

    case CCnoiseLevel:
      noiseLevel = value;
      noiseLevelstr = map(value, 0, 4095, 0, 127);
      noiseLeveldisplay = LINEARCENTREZERO[noiseLevelstr];
      updatenoiseLevel();
      break;

    case CCosc2Level:
      osc2Level = value;
      osc2Levelstr = map(value, 0, 4095, 0, 127);
      updateOsc2Level();
      break;

    case CCosc1Level:
      osc1Level = value;
      osc1Levelstr = map(value, 0, 4095, 0, 127);
      updateOsc1Level();
      break;

    case CCKeyTrack:
      keyTrack = value;
      keyTrackstr = map(value, 0, 4095, 0, 127);
      updateKeyTrack();
      break;

    case CCfilterCutoff:
      filterCutoff = value;
      filterCutoffstr = map(value, 0, 4095, 0, 127);
      filterCutoffstr = FILTERCUTOFF[filterCutoffstr];
      updateFilterCutoff();
      break;

    case CCfilterLFO:
      filterLFO = value;
      filterLFOstr = map(value, 0, 4095, 0, 127);
      updatefilterLFO();
      break;

    case CCfilterRes:
      filterRes = value;
      filterResstr = map(value, 0, 4095, 0, 127);
      updatefilterRes();
      break;

    case CCfilterType:
      updateFilterType();
      break;

    case CCfilterEGlevel:
      filterEGlevel = value;
      filterEGlevelstr = map(value, 0, 4095, 0, 127);
      updatefilterEGlevel();
      break;

    case CCLFORate:
      LFORatestr = map(value, 0, 4095, 0, 127);
      LFORatedisplay = LFOTEMPO[LFORatestr];  // for display
      LFORate = value;
      updateLFORate();
      break;

    case CCLFOWaveform:
      updateStratusLFOWaveform();
      break;

    case CCfilterAttack:
      filterAttack = value;
      filterAttackstr = map(value, 0, 4095, 0, 127);
      filterAttackstr = ENVTIMES[filterAttackstr];
      updatefilterAttack();
      break;

    case CCfilterDecay:
      filterDecay = value;
      filterDecaystr = map(value, 0, 4095, 0, 127);
      filterDecaystr = ENVTIMES[filterDecaystr];
      updatefilterDecay();
      break;

    case CCfilterSustain:
      filterSustain = value;
      filterSustainstr = map(value, 0, 4095, 0, 127);
      filterSustainstr = LINEAR_FILTERMIXERSTR[filterSustainstr];
      updatefilterSustain();
      break;

    case CCfilterRelease:
      filterRelease = value;
      filterReleasestr = map(value, 0, 4095, 0, 127);
      filterReleasestr = ENVTIMES[filterReleasestr];
      updatefilterRelease();
      break;

    case CCampAttack:
      ampAttack = value;
      oldampAttack = value;
      ampAttackstr = map(value, 0, 4095, 0, 127);
      ampAttackstr = ENVTIMES[ampAttackstr];
      updateampAttack();
      break;

    case CCampDecay:
      ampDecay = value;
      oldampDecay = value;
      ampDecaystr = map(value, 0, 4095, 0, 127);
      ampDecaystr = ENVTIMES[ampDecaystr];
      updateampDecay();
      break;

    case CCampSustain:
      ampSustain = value;
      oldampSustain = value;
      ampSustainstr = map(value, 0, 4095, 0, 127);
      ampSustainstr = LINEAR_FILTERMIXERSTR[ampSustainstr];
      updateampSustain();
      break;

    case CCampRelease:
      ampRelease = value;
      oldampRelease = value;
      ampReleasestr = map(value, 0, 4095, 0, 127);
      ampReleasestr = ENVTIMES[ampReleasestr];
      updateampRelease();
      break;

    case CCvolumeControl:
      volumeControl = value;
      volumeControlstr = map(value, 0, 4095, 0, 127);
      updatevolumeControl();
      break;

    case CClfoDelay:
      lfoDelay = value;
      lfoDelaystr = map(value, 0, 4095, 0, 127);
      updatelfoDelay();
      break;

    case CCamDepth:
      amDepth = value;
      amDepthstr = map(value, 0, 4095, 0, 127);
      updateamDepth();
      break;

    case CCPitchBend:
      PitchBendLevel = value;
      PitchBendLevelstr = map(value, 0, 4095, 0, 127);
      PitchBendLevelstr = PITCHBEND[PitchBendLevelstr];  // for display
      updatePitchBend();
      break;

    case CCmodwheel:
      switch (modWheelDepth) {
        case 1:
          modWheelLevel = ((value * 8) / 5);
          osc1fmDepth = (int(modWheelLevel));
          //          Serial.print("ModWheel Depth ");
          //          Serial.println(modWheelLevel);
          break;

        case 2:
          modWheelLevel = ((value * 8) / 4);
          osc1fmDepth = (int(modWheelLevel));
          //          Serial.print("ModWheel Depth ");
          //          Serial.println(modWheelLevel);
          break;

        case 3:
          modWheelLevel = ((value * 8) / 3.5);
          osc1fmDepth = (int(modWheelLevel));
          //          Serial.print("ModWheel Depth ");
          //          Serial.println(modWheelLevel);
          break;

        case 4:
          modWheelLevel = ((value * 8) / 3);
          osc1fmDepth = (int(modWheelLevel));
          //          Serial.print("ModWheel Depth ");
          //          Serial.println(modWheelLevel);
          break;

        case 5:
          modWheelLevel = ((value * 8) / 2.5);
          osc2fmDepth = (int(modWheelLevel));
          //          Serial.print("ModWheel Depth ");
          //          Serial.println(modWheelLevel);
          break;

        case 6:
          modWheelLevel = ((value * 8) / 2);
          osc1fmDepth = (int(modWheelLevel));
          //          Serial.print("ModWheel Depth ");
          //          Serial.println(modWheelLevel);
          break;

        case 7:
          modWheelLevel = ((value * 8) / 1.75);
          osc1fmDepth = (int(modWheelLevel));
          //          Serial.print("ModWheel Depth ");
          //          Serial.println(modWheelLevel);
          break;

        case 8:
          modWheelLevel = ((value * 8) / 1.5);
          osc1fmDepth = (int(modWheelLevel));
          //          Serial.print("ModWheel Depth ");
          //          Serial.println(modWheelLevel);
          break;

        case 9:
          modWheelLevel = ((value * 8) / 1.25);
          osc1fmDepth = (int(modWheelLevel));
          //          Serial.print("ModWheel Depth ");
          //          Serial.println(modWheelLevel);
          break;

        case 10:
          modWheelLevel = (value * 8);
          osc1fmDepth = (int(modWheelLevel));
          //          Serial.print("ModWheel Depth ");
          //          Serial.println(modWheelLevel);
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

    case CCfilterLinLogSW:
      value > 0 ? filterLinLogSW = 1 : filterLinLogSW = 0;
      updatefilterLinLogSW();
      break;

    case CCvcaLinLogSW:
      value > 0 ? vcaLinLogSW = 1 : vcaLinLogSW = 0;
      updatevcaLinLogSW();
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
  effectMix = data[62].toFloat();
  glideSW = data[63].toInt();
  lfoDelay = data[64].toFloat();
  lfoMult = data[65].toInt();
  oldampAttack = data[66].toInt();
  oldampDecay = data[67].toInt();
  oldampSustain = data[68].toInt();
  oldampRelease = data[69].toInt();
  effectBankSW = data[70].toInt();
  filterLinLogSW = data[71].toInt();
  vcaLinLogSW = data[72].toInt();

  oldfilterCutoff = filterCutoff;

  //Switches

  updatefilterPoleSwitch();
  updatefilterEGinv();
  updatelfoAlt();
  updatelfoMult();
  updateosc1Range();
  updateosc2Range();
  updateosc1Bank();
  updateosc2Bank();
  updateosc1WaveSelect();
  updateosc2WaveSelect();
  updateFilterType();
  updateFilterEnv();
  updateAmpEnv();
  updateFilterLoop();
  updateAmpLoop();
  updateAfterTouchDest();
  updateglideSW();
  updatefilterVelSW();
  updateampVelSW();
  updateeffectBankSW();
  updatefilterLinLogSW();
  updatevcaLinLogSW();

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
         + "," + String(effect3) + "," + String(effectMix) + "," + String(glideSW) + "," + String(lfoDelay) + "," + String(lfoMult) + "," + String(oldampAttack) + "," + String(oldampDecay)
         + "," + String(oldampSustain) + "," + String(oldampRelease) + "," + String(effectBankSW) + "," + String(filterLinLogSW) + "," + String(vcaLinLogSW);
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
  delayMicroseconds(75);
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
    if (oct1 > 3) {
      oct1 = 1;
    }
    myControlChange(midiChannel, CCosc1Oct, oct1);
  }

  if (btnIndex == DCO2_OCT_SW && btnType == ROX_PRESSED) {
    oct2 = oct2 + 1;
    if (oct2 > 3) {
      oct2 = 1;
    }
    myControlChange(midiChannel, CCosc2Oct, oct2);
  }

  if (btnIndex == DCO1_MODE_SW && btnType == ROX_PRESSED) {
    osc1Bank = osc1Bank + 1;
    if (osc1Bank > 3) {
      osc1Bank = 1;
    }
    myControlChange(midiChannel, CCosc1Bank, osc1Bank);
  }

  if (btnIndex == DCO2_MODE_SW && btnType == ROX_PRESSED) {
    osc2Bank = osc2Bank + 1;
    if (osc2Bank > 3) {
      osc2Bank = 1;
    }
    myControlChange(midiChannel, CCosc2Bank, osc2Bank);
  }

  if (btnIndex == FILTER_TYPE_SW && btnType == ROX_PRESSED) {
    filterType = filterType + 1;
    if (filterType > 8) {
      filterType = 1;
    }
    myControlChange(midiChannel, CCfilterType, filterType);
  }

  if (btnIndex == LFO_ALT_SW && btnType == ROX_PRESSED) {
    lfoAlt = !lfoAlt;
    myControlChange(midiChannel, CClfoAlt, lfoAlt);
  }

  if (btnIndex == LFO_MULT_SW && btnType == ROX_PRESSED) {
    lfoMult = lfoMult + 1;
    if (lfoMult > 3) {
      lfoMult = 1;
    }
    myControlChange(midiChannel, CClfoMult, lfoMult);
  }

  if (btnIndex == LFO_WAVE_SW && btnType == ROX_PRESSED) {
    LFOWaveform = LFOWaveform + 1;
    if (LFOWaveform > 8) {
      LFOWaveform = 1;
    }
    myControlChange(midiChannel, CCLFOWaveform, LFOWaveform);
  }

  if (btnIndex == DCO1_WAVE_SW && btnType == ROX_PRESSED) {
    osc1WaveSelect = osc1WaveSelect + 1;
    if (osc1WaveSelect > 8) {
      osc1WaveSelect = 1;
    }
    myControlChange(midiChannel, CCosc1WaveSelect, osc1WaveSelect);
  }

  if (btnIndex == DCO2_WAVE_SW && btnType == ROX_PRESSED) {
    osc2WaveSelect = osc2WaveSelect + 1;
    if (osc2WaveSelect > 8) {
      osc2WaveSelect = 1;
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
    if (FilterLoop > 3) {
      FilterLoop = 1;
    }
    myControlChange(midiChannel, CCFilterLoop, FilterLoop);
  }

  if (btnIndex == VCA_LOOP_SW && btnType == ROX_PRESSED) {
   AmpLoop = AmpLoop + 1;
    if (AmpLoop > 3) {
      AmpLoop = 1;
    }
    myControlChange(midiChannel, CCAmpLoop, AmpLoop);
  }

  if (btnIndex == VCA_GATE_SW && btnType == ROX_PRESSED) {
    AmpGatedSW = !AmpGatedSW;
    myControlChange(midiChannel, CCAmpGatedSW, AmpGatedSW);
  }

  if (btnIndex == EFFECT_BANK_SW && btnType == ROX_PRESSED) {
   effectBankSW = effectBankSW + 1;
    if (effectBankSW > 4) {
      effectBankSW = 1;
    }
    myControlChange(midiChannel, CCeffectBankSW, effectBankSW);
  }

    if (btnIndex == VCA_LINLOG_SW && btnType == ROX_PRESSED) {
    vcaLinLogSW = !vcaLinLogSW;
    myControlChange(midiChannel, CCvcaLinLogSW, vcaLinLogSW);
  }

    if (btnIndex == FILTER_LINLOG_SW && btnType == ROX_PRESSED) {
    filterLinLogSW = !filterLinLogSW;
    myControlChange(midiChannel, CCfilterLinLogSW, filterLinLogSW);
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


void loop() {

  checkMux();           // Read the sliders and switches
  checkSwitches();      // Read the buttons for the program menus etc
  checkEncoder();       // check the encoder status
  octoswitch.update();  // read all the buttons for the Quadra
  trig.update();        // update all the triggers
  srp.update();         // update all the LEDs in the buttons
  sr.update();          // update all the outputs


  // Read all the MIDI ports
  myusb.Task();
  midi1.read();  //USB HOST MIDI Class Compliant
  MIDI.read(midiChannel);
  usbMIDI.read(midiChannel);
}
