#include "TeensyThreads.h"
#include "ScreenParams.h"

// This Teensy3 native optimized version requires specific pins
//#define sclk 27 // SCLK can also use pin 14
//#define mosi 26 // MOSI can also use pin 7
#define cs 2   // CS & DC can use pins 2, 6, 9, 10, 15, 20, 21, 22, 23
#define dc 3   //but certain pairs must NOT be used: 2+10, 6+9, 20+23, 21+22
#define rst 9  // RST can use any pin
#define DISPLAYTIMEOUT 1500

#include <Adafruit_GFX.h>
//#include "ST7735_t3.h" // Local copy from TD1.48 that works for 0.96" IPS 160x80 display
#include <ST7735_t3.h>  // Hardware-specific library
#include <ST7789_t3.h>  // Hardware-specific library

#include <Fonts/Org_01.h>
#include "Yeysk16pt7b.h"
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansOblique24pt7b.h>
#include <Fonts/FreeSansBoldOblique24pt7b.h>

#define PULSE 1
#define VAR_TRI 2
#define FILTER_ENV 3
#define AMP_ENV1 4
#define AMP_ENV2 5

//ST7735_t3 tft = ST7735_t3(cs, dc, 26, 27, rst);
ST7789_t3 tft = ST7789_t3(cs, dc, 26, 27, rst);

String currentParameter = "";
String currentValue = "";
float currentFloatValue = 0.0;
String currentPgmNum = "";
String currentPatchName = "";
String newPatchName = "";
const char *currentSettingsOption = "";
const char *currentSettingsValue = "";
int currentSettingsPart = SETTINGS;
int paramType = PARAMETER;

boolean MIDIClkSignal = false;

unsigned long timer = 0;

void startTimer() {
  // if (state == PARAMETER) {
  //   timer = millis();
  // }
}

void renderBootUpPage() {
  tft.fillScreen(ST7735_BLACK);
  tft.drawRect(42, 30, 46, 11, ST7735_WHITE);
  tft.fillRect(88, 30, 61, 11, ST7735_WHITE);
  tft.setCursor(45, 31);
  tft.setFont(&Org_01);
  tft.setTextSize(1);
  tft.setTextColor(ST7735_WHITE);
  tft.println("CHERRY");
  tft.setTextColor(ST7735_BLACK);
  tft.setCursor(91, 37);
  tft.println("EDITOR");
  tft.setTextColor(ST7735_YELLOW);
  tft.setFont(&Yeysk16pt7b);
  tft.setCursor(0, 70);
  tft.setTextSize(1);
  tft.println("ZERO");
  tft.setTextColor(ST7735_RED);
  tft.setFont(&FreeSans9pt7b);
  tft.setCursor(110, 95);
  tft.println(VERSION);
}

void renderCurrentPatchPage() {
  tft.setFont(&FreeSans12pt7b);
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(0, 5);
  tft.setTextSize(3);
  tft.setTextColor(ST7735_YELLOW);
  tft.println(currentPgmNum);
  tft.setCursor(80, 15);
  tft.setFont(&FreeSans9pt7b);
  tft.setTextSize(2);
  tft.setTextColor(ST7735_WHITE);
  tft.println(currentPatchName);
  tft.setFont(&FreeSans12pt7b);
  tft.setTextSize(1);
  tft.drawFastHLine(0, 58, tft.width(), ST7735_RED);
  tft.setTextColor(ST7735_WHITE);

  switch (parameterGroup) {
    case 0:
      tft.setTextColor(ST7735_WHITE);
      tft.setTextSize(1);
      tft.setCursor(0, 70);  //effect mix
      tft.print("Effect:");
      tft.setCursor(80, 70);
      tft.setTextColor(ST7735_YELLOW);
      char buf1[30];  // first word of effect name
      switch (effectBankSW) {
        case 0:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(name01[effectNumSW])));
          break;
        case 1:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(name11[effectNumSW])));
          break;
        case 2:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(name21[effectNumSW])));
          break;
        case 3:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(name31[effectNumSW])));
          break;
      }

      // Check if the pointer is valid
      if (str_ptr != nullptr) {
        // Copy the string from program memory to RAM
        strcpy_P(buf1, str_ptr);
      } else {
        // Handle the case where the pointer is NULL (if needed)
      }
      tft.setTextSize(1);
      tft.print(buf1);
      tft.print(" ");

      char buf2[30];  // second word of effect name
      switch (effectBankSW) {
        case 0:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(name02[effectNumSW])));
          break;
        case 1:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(name12[effectNumSW])));
          break;
        case 2:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(name22[effectNumSW])));
          break;
        case 3:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(name32[effectNumSW])));
          break;
      }

      // Check if the pointer is valid
      if (str_ptr != nullptr) {
        // Copy the string from program memory to RAM
        strcpy_P(buf2, str_ptr);
      } else {
        // Handle the case where the pointer is NULL (if needed)
      }
      tft.print(buf2);
      tft.setTextSize(1);
      tft.setTextColor(ST7735_WHITE);
      tft.setCursor(0, 100);
      tft.print("Bank:");
      tft.setTextColor(ST7735_YELLOW);
      tft.setCursor(80, 100);
      tft.print(effectBankSW + 1);
      tft.setTextColor(ST7735_WHITE);
      tft.setCursor(160, 100);
      tft.print("Number:");
      tft.setTextColor(ST7735_YELLOW);
      tft.setCursor(270, 100);
      tft.print(effectNumSW + 1);
      tft.setTextSize(1);
      tft.setTextColor(ST7735_WHITE);

      tft.setCursor(0, 130);  //effect param1
      char buf3[30];
      switch (effectBankSW) {
        case 0:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(name03[effectNumSW])));
          break;
        case 1:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(name13[effectNumSW])));
          break;
        case 2:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(name23[effectNumSW])));
          break;
        case 3:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(name33[effectNumSW])));
          break;
      }

      // Check if the pointer is valid
      if (str_ptr != nullptr) {
        // Copy the string from program memory to RAM
        strcpy_P(buf3, str_ptr);
      } else {
        // Handle the case where the pointer is NULL (if needed)
      }
      tft.setTextSize(1);
      tft.print(buf3);

      tft.setCursor(0, 160);  //effect param2
      char buf4[30];
      switch (effectBankSW) {
        case 0:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(name04[effectNumSW])));
          break;
        case 1:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(name14[effectNumSW])));
          break;
        case 2:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(name24[effectNumSW])));
          break;
        case 3:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(name34[effectNumSW])));
          break;
      }
      // Check if the pointer is valid
      if (str_ptr != nullptr) {
        // Copy the string from program memory to RAM
        strcpy_P(buf4, str_ptr);
      } else {
        // Handle the case where the pointer is NULL (if needed)
      }
      tft.setTextSize(1);
      tft.print(buf4);

      tft.setCursor(0, 190);  //effect param3
      char buf5[30];
      switch (effectBankSW) {
        case 0:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(name05[effectNumSW])));
          break;
        case 1:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(name15[effectNumSW])));
          break;
        case 2:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(name25[effectNumSW])));
          break;
        case 3:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(name35[effectNumSW])));
          break;
      }
      // Check if the pointer is valid
      if (str_ptr != nullptr) {
        // Copy the string from program memory to RAM
        strcpy_P(buf5, str_ptr);
      } else {
        // Handle the case where the pointer is NULL (if needed)
      }
      tft.setTextSize(1);
      tft.print(buf5);


      tft.setCursor(0, 220);  //effect mix
      tft.print("Effects Mix");
      tft.drawFastHLine(160, 226, 160, ST7735_RED);
      tft.drawFastVLine(160, 220, 12, ST7735_RED);
      tft.drawFastVLine(240, 220, 12, ST7735_RED);
      tft.drawFastVLine(319, 220, 12, ST7735_RED);
      tft.fillRoundRect(160, 128, int(effect1 / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect(160, 158, int(effect2 / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect(160, 188, int(effect3 / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect((int(mixa / 6.7) + 160), 218, 8, 16, 2, ST7735_YELLOW);
      break;

    case 1:

      char filterDisplay[30];
      switch (filterPoleSW) {
        case 0:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(filter01[filterType])));
          break;
        case 1:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(filter02[filterType])));
          break;
      }
      // Check if the pointer is valid
      if (str_ptr != nullptr) {
        // Copy the string from program memory to RAM
        strcpy_P(filterDisplay, str_ptr);
      } else {
        // Handle the case where the pointer is NULL (if needed)
      }
      tft.setTextSize(1);
      tft.setCursor(0, 70);
      tft.print("Filter Type");
      tft.setCursor(160, 70);
      tft.setTextColor(ST7735_YELLOW);
      tft.print(filterDisplay);
      tft.setTextSize(1);
      tft.setTextColor(ST7735_WHITE);
      tft.setCursor(0, 100);
      tft.print("Filter Cutoff");
      tft.setCursor(0, 130);
      tft.print("Filter Res");
      tft.setCursor(0, 160);
      tft.print("Filter Env");
      tft.setCursor(0, 190);
      tft.print("KeyTrack");
      tft.setCursor(0, 220);
      tft.print("LFO Depth");

      tft.fillRoundRect(160, 98, int(filterCutoff / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect(160, 128, int(filterRes / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect(160, 158, int(filterEGlevel / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect(160, 188, int(keyTrack / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect(160, 218, int(filterLFO / 6.5), 16, 2, ST7735_YELLOW);
      break;

    case 2:  // filter ADSR

      tft.setTextColor(ST7735_WHITE);
      tft.setTextSize(1);
      tft.setCursor(0, 70);
      tft.print("Filter Attack");
      tft.setCursor(0, 100);
      tft.print("Filter Decay");
      tft.setCursor(0, 130);
      tft.print("Filter Sustain");
      tft.setCursor(0, 160);
      tft.print("Filter Release");
      tft.setCursor(0, 190);
      tft.print("Looping");
      tft.setCursor(160, 190);
      tft.print("Velocity");
      tft.setCursor(0, 220);
      tft.print("EG Inv");
      tft.setCursor(160, 220);
      tft.print("EnvType");
      tft.setTextColor(ST7735_YELLOW);
      tft.setCursor(90, 190);
      switch (FilterLoop) {
        case 0:
          tft.print("Off");
          break;
        case 1:
          tft.print("Gated");
          break;
        case 2:
          tft.print("LFO");
          break;
      }
      tft.setCursor(257, 190);
      switch (filterVelSW) {
        case 0:
          tft.print("Off");
          break;
        case 1:
          tft.print("On");
          break;
      }
      tft.setCursor(90, 220);
      switch (filterEGinv) {
        case 0:
          tft.print("Neg");
          break;
        case 1:
          tft.print("Pos");
          break;
      }
      tft.setCursor(257, 220);
      switch (envLinLogSW) {
        case 0:
          tft.print("Log");
          break;
        case 1:
          tft.print("Lin");
          break;
      }

      tft.fillRoundRect(160, 68, int(filterAttack / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect(160, 98, int(filterDecay / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect(160, 128, int(filterSustain / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect(160, 158, int(filterRelease / 6.5), 16, 2, ST7735_YELLOW);
      break;

    case 3:  // VCA ADSR
      tft.setTextColor(ST7735_WHITE);
      tft.setTextSize(1);
      tft.setCursor(0, 70);
      tft.print("Amp Attack");
      tft.setCursor(0, 100);
      tft.print("Amp Decay");
      tft.setCursor(0, 130);
      tft.print("Amp Sustain");
      tft.setCursor(0, 160);
      tft.print("Amp Release");
      tft.setCursor(0, 190);
      tft.print("Looping");
      tft.setCursor(160, 190);
      tft.print("Velocity");
      tft.setCursor(0, 220);
      tft.print("Gated");
      tft.setCursor(160, 220);
      tft.print("EnvType");
      tft.setTextColor(ST7735_YELLOW);
      tft.setCursor(90, 190);
      switch (AmpLoop) {
        case 0:
          tft.print("Off");
          break;
        case 1:
          tft.print("Gated");
          break;
        case 2:
          tft.print("LFO");
          break;
      }
      tft.setCursor(257, 190);
      switch (ampVelSW) {
        case 0:
          tft.print("Off");
          break;
        case 1:
          tft.print("On");
          break;
      }
      tft.setCursor(90, 220);
      switch (AmpGatedSW) {
        case 0:
          tft.print("Off");
          break;
        case 1:
          tft.print("On");
          break;
      }
      tft.setCursor(257, 220);
      switch (envLinLogSW) {
        case 0:
          tft.print("Log");
          break;
        case 1:
          tft.print("Lin");
          break;
      }

      tft.fillRoundRect(160, 68, int(ampAttack / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect(160, 98, int(ampDecay / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect(160, 128, int(ampSustain / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect(160, 158, int(ampRelease / 6.5), 16, 2, ST7735_YELLOW);
      break;

    case 4:  // LFO
      char lfoDisplay[30];
      switch (lfoAlt) {
        case 0:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(lfo02[LFOWaveform])));
          break;
        case 1:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(lfo01[LFOWaveform])));
          break;
      }
      // Check if the pointer is valid
      if (str_ptr != nullptr) {
        // Copy the string from program memory to RAM
        strcpy_P(lfoDisplay, str_ptr);
      } else {
        // Handle the case where the pointer is NULL (if needed)
      }
      tft.setTextSize(1);
      tft.setCursor(0, 70);
      tft.print("LFO Wave");
      tft.setCursor(160, 70);
      tft.setTextColor(ST7735_YELLOW);
      tft.print(lfoDisplay);
      tft.setTextColor(ST7735_WHITE);
      tft.setCursor(0, 100);
      tft.print("LFO Rate");
      tft.setCursor(0, 130);
      tft.print("LFO Delay");
      tft.setCursor(0, 160);
      tft.print("LFO Multiplier");
      tft.setCursor(0, 190);
      tft.print("Multi Trigger");
      tft.setCursor(0, 220);
      tft.print("FM Mod Sync");
      tft.setTextColor(ST7735_YELLOW);
      tft.setCursor(160, 160);
      switch (lfoMult) {
        case 0:
          tft.print("x 0.5");
          break;
        case 1:
          tft.print("x 1.0");
          break;
        case 2:
          tft.print("x 2.0");
          break;
      }
      tft.setCursor(160, 190);
      switch (monoMultiSW) {
        case 0:
          tft.print("Off");
          break;
        case 1:
          tft.print("On");
          break;
      }
      tft.setCursor(160, 220);
      switch (osc1osc2fmDepth) {
        case 0:
          tft.print("Off");
          break;
        case 1:
          tft.print("On");
          break;
      }

      tft.fillRoundRect(160, 98, int(LFORate / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect(160, 128, int(lfoDelay / 6.5), 16, 2, ST7735_YELLOW);
      break;

    case 5:
      tft.setTextColor(ST7735_WHITE);
      tft.setTextSize(1);
      tft.setCursor(0, 70);
      tft.print("Glide Time");
      tft.setCursor(0, 100);
      tft.print("Glide");
      tft.setCursor(0, 130);
      tft.print("Key Mode");
      tft.setCursor(0, 160);
      tft.print("Note Priority");
      tft.setCursor(0, 190);
      tft.print("OSC1 Coarse");
      tft.setCursor(0, 220);
      tft.print("OSC2 Fine");
      tft.setTextColor(ST7735_YELLOW);
      tft.setCursor(160, 100);
      switch (glideSW) {
        case 0:
          tft.print("Off");
          break;
        case 1:
          tft.print("On");
          break;
      }
      tft.setCursor(160, 130);
      switch (keyboardMode) {
        case 0:
          tft.print("Poly");
          break;
        case 1:
          tft.print("Mono");
          break;
        case 2:
          tft.print("Unison");
          break;
      }
      tft.setCursor(160, 160);
      switch (NotePriority) {
        case 0:
          tft.print("Top Note");
          break;
        case 1:
          tft.print("Bottom Note");
          break;
        case 2:
          tft.print("Last Note");
          break;
      }
      tft.drawFastHLine(160, 196, 160, ST7735_RED);
      tft.drawFastVLine(160, 190, 12, ST7735_RED);
      tft.drawFastVLine(240, 190, 12, ST7735_RED);
      tft.drawFastVLine(319, 190, 12, ST7735_RED);

      tft.drawFastHLine(160, 226, 160, ST7735_RED);
      tft.drawFastVLine(160, 220, 12, ST7735_RED);
      tft.drawFastVLine(240, 220, 12, ST7735_RED);
      tft.drawFastVLine(319, 220, 12, ST7735_RED);
      
      tft.fillRoundRect(160, 68, int((glideTime * 8) / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect((int(osc1Tune / 6.7) + 160), 188, 8, 16, 2, ST7735_YELLOW);
      tft.fillRoundRect((int(osc2Tune / 6.7) + 160), 218, 8, 16, 2, ST7735_YELLOW);
      break;

    case 6:
      tft.setTextColor(ST7735_WHITE);
      tft.setTextSize(1);
      tft.setCursor(0, 70);
      tft.print("Volume");
      tft.setCursor(0, 100);
      tft.print("AM Depth");
      tft.setCursor(0, 130);
      tft.print("Noise Level");
      tft.setTextColor(ST7735_RED);

      tft.drawFastHLine(160, 136, 160, ST7735_RED);
      tft.drawFastVLine(160, 130, 12, ST7735_RED);
      tft.drawFastVLine(240, 130, 12, ST7735_RED);
      tft.drawFastVLine(319, 130, 12, ST7735_RED);
      tft.fillRoundRect(160, 68, int(volumeControl / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect(160, 98, int(amDepth / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect((int(noiseLevel / 6.7) + 160), 128, 8, 16, 2, ST7735_YELLOW);
      break;

    case 7:  // Osc1
      tft.setTextColor(ST7735_WHITE);
      tft.setTextSize(1);
      tft.setCursor(0, 70);
      tft.print("OSC1 Wave");
      tft.setCursor(0, 100);
      tft.print("OSC1 Mod");
      tft.setCursor(0, 130);
      tft.print("LFO Wave");
      tft.setCursor(0, 160);
      tft.print("OSC1 FM");
      tft.setCursor(0, 190);
      tft.print("OSC1 Level");
      tft.setCursor(0, 220);
      tft.print("Bank");
      tft.setCursor(160, 220);
      tft.print("Octave");

      char vcoDisplay[30];
      switch (osc1Bank) {
        case 0:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(vco01[osc1WaveSelect])));
          break;
        case 1:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(vco02[osc1WaveSelect])));
          break;
        case 2:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(vco03[osc1WaveSelect])));
          break;
      }
      // Check if the pointer is valid
      if (str_ptr != nullptr) {
        // Copy the string from program memory to RAM
        strcpy_P(vcoDisplay, str_ptr);
      } else {
        // Handle the case where the pointer is NULL (if needed)
      }

      tft.setCursor(160, 70);
      tft.setTextColor(ST7735_YELLOW);
      tft.print(vcoDisplay);

      tft.setCursor(80, 220);
      switch (osc1Bank) {
        case 0:
          tft.print("Fold");
          break;
        case 1:
          tft.print("FM");
          break;
        case 2:
          tft.print("AM");
          break;
      }
      tft.setCursor(270, 220);
      switch (oct1) {
        case 0:
          tft.print("-1");
          break;
        case 1:
          tft.print("0");
          break;
        case 2:
          tft.print("+1");
          break;
      }
      tft.setTextColor(ST7735_WHITE);

      tft.fillRoundRect(160, 98, int(osc1WaveMod / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect(160, 128, int(osc1fmWaveMod / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect(160, 158, int(osc1fmDepth / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect(160, 188, int(osc1Level / 6.5), 16, 2, ST7735_YELLOW);
      break;

    case 8:  // Osc2
      tft.setTextColor(ST7735_WHITE);
      tft.setTextSize(1);
      tft.setCursor(0, 70);
      tft.print("OSC2 Wave");
      tft.setCursor(0, 100);
      tft.print("OSC2 Mod");
      tft.setCursor(0, 130);
      tft.print("LFO Wave");
      tft.setCursor(0, 160);
      tft.print("OSC2 FM ");
      tft.setCursor(0, 190);
      tft.print("OSC2 Level");
      tft.setCursor(0, 220);
      tft.print("Bank");
      tft.setCursor(160, 220);
      tft.print("Octave");


      char vco2Display[30];
      switch (osc2Bank) {
        case 0:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(vco01[osc2WaveSelect])));
          break;
        case 1:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(vco02[osc2WaveSelect])));
          break;
        case 2:
          str_ptr = reinterpret_cast<const char *>(pgm_read_ptr(&(vco03[osc2WaveSelect])));
          break;
      }
      // Check if the pointer is valid
      if (str_ptr != nullptr) {
        // Copy the string from program memory to RAM
        strcpy_P(vco2Display, str_ptr);
      } else {
        // Handle the case where the pointer is NULL (if needed)
      }
      tft.setCursor(160, 70);
      tft.setTextColor(ST7735_YELLOW);
      tft.print(vco2Display);
      tft.setCursor(80, 220);
      switch (osc2Bank) {
        case 0:
          tft.print("Fold");
          break;
        case 1:
          tft.print("FM");
          break;
        case 2:
          tft.print("AM");
          break;
      }
      tft.setCursor(270, 220);
      switch (oct2) {
        case 0:
          tft.print("-1");
          break;
        case 1:
          tft.print("0");
          break;
        case 2:
          tft.print("+1");
          break;
      }
      tft.setTextColor(ST7735_WHITE);

      tft.fillRoundRect(160, 98, int(osc2WaveMod / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect(160, 128, int(osc2fmWaveMod / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect(160, 158, int(osc2fmDepth / 6.5), 16, 2, ST7735_YELLOW);
      tft.fillRoundRect(160, 188, int(osc2Level / 6.5), 16, 2, ST7735_YELLOW);
      break;
  }
}


void renderPulseWidth(float value) {
  tft.drawFastHLine(108, 74, 15 + (value * 13), ST7735_CYAN);
  tft.drawFastVLine(123 + (value * 13), 74, 20, ST7735_CYAN);
  tft.drawFastHLine(123 + (value * 13), 94, 16 - (value * 13), ST7735_CYAN);
  if (value < 0) {
    tft.drawFastVLine(108, 74, 21, ST7735_CYAN);
  } else {
    tft.drawFastVLine(138, 74, 21, ST7735_CYAN);
  }
}

void renderVarTriangle(float value) {
  tft.drawLine(110, 94, 123 + (value * 13), 74, ST7735_CYAN);
  tft.drawLine(123 + (value * 13), 74, 136, 94, ST7735_CYAN);
}

void renderEnv(float att, float dec, float sus, float rel) {
  tft.drawLine(100, 94, 100 + (att * 60), 74, ST7735_CYAN);
  tft.drawLine(100 + (att * 60), 74.0, 100 + ((att + dec) * 60), 94 - (sus / 52), ST7735_CYAN);
  tft.drawFastHLine(100 + ((att + dec) * 60), 94 - (sus / 52), 40 - ((att + dec) * 60), ST7735_CYAN);
  tft.drawLine(139, 94 - (sus / 52), 139 + (rel * 60), 94, ST7735_CYAN);
  //  tft.drawLine(100, 94, 100 + (att * 15), 74, ST7735_CYAN);
  //  tft.drawLine(100 + (att * 15), 74.0, 100 + ((att + dec) * 15), 94 - (sus / 52), ST7735_CYAN);
  //  tft.drawFastHLine(100 + ((att + dec) * 15 ), 94 - (sus / 52), 40 - ((att + dec) * 15), ST7735_CYAN);
  //  tft.drawLine(139, 94 - (sus / 52), 139 + (rel * 15), 94, ST7735_CYAN);
}

void renderCurrentParameterPage() {
  switch (state) {
    case PARAMETER:
      tft.fillScreen(ST7735_BLACK);
      tft.setFont(&FreeSans12pt7b);
      tft.setCursor(0, 53);
      tft.setTextColor(ST7735_YELLOW);
      tft.setTextSize(1);
      tft.println(currentParameter);
      tft.drawFastHLine(10, 62, tft.width() - 20, ST7735_RED);
      tft.setCursor(1, 90);
      tft.setTextColor(ST7735_WHITE);
      tft.println(currentValue);
      switch (paramType) {
        case PULSE:
          renderPulseWidth(currentFloatValue);
          break;
        case VAR_TRI:
          renderVarTriangle(currentFloatValue);
          break;
        case FILTER_ENV:
          renderEnv(filterAttack * 0.0001, filterDecay * 0.0001, filterSustain, filterRelease * 0.0001);
          break;
      }
      break;
  }
}

void renderDeletePatchPage() {
  tft.fillScreen(ST7735_BLACK);
  tft.setFont(&FreeSansBold18pt7b);
  tft.setCursor(5, 53);
  tft.setTextColor(ST7735_YELLOW);
  tft.setTextSize(1);
  tft.println("Delete?");
  tft.drawFastHLine(10, 80, tft.width() - 20, ST7735_RED);
  tft.setFont(&FreeSans9pt7b);
  tft.setCursor(0, 96);
  tft.setTextColor(ST7735_YELLOW);
  tft.println(patches.last().patchNo);
  tft.setCursor(35, 96);
  tft.setTextColor(ST7735_WHITE);
  tft.println(patches.last().patchName);
  tft.fillRect(0, 120, tft.width(), 30, ST77XX_RED);
  tft.setCursor(0, 130);
  tft.setTextColor(ST7735_YELLOW);
  tft.println(patches.first().patchNo);
  tft.setCursor(35, 130);
  tft.setTextColor(ST7735_WHITE);
  tft.println(patches.first().patchName);
}

void renderDeleteMessagePage() {
  tft.fillScreen(ST7735_BLACK);
  tft.setFont(&FreeSans12pt7b);
  tft.setCursor(2, 53);
  tft.setTextColor(ST7735_YELLOW);
  tft.setTextSize(1);
  tft.println("Renumbering");
  tft.setCursor(10, 96);
  tft.println("SD Card");
}

void renderSavePage() {
  tft.fillScreen(ST7735_BLACK);
  tft.setFont(&FreeSansBold18pt7b);
  tft.setCursor(5, 53);
  tft.setTextColor(ST7735_YELLOW);
  tft.setTextSize(1);
  tft.println("Save?");
  tft.drawFastHLine(10, 80, tft.width() - 20, ST7735_RED);
  tft.setFont(&FreeSans9pt7b);
  tft.setCursor(0, 96);
  tft.setTextColor(ST7735_YELLOW);
  tft.println(patches[patches.size() - 2].patchNo);
  tft.setCursor(35, 96);
  tft.setTextColor(ST7735_WHITE);
  tft.println(patches[patches.size() - 2].patchName);
  tft.fillRect(0, 120, tft.width(), 30, ST77XX_RED);
  tft.setCursor(0, 130);
  tft.setTextColor(ST7735_YELLOW);
  tft.println(patches.last().patchNo);
  tft.setCursor(35, 130);
  tft.setTextColor(ST7735_WHITE);
  tft.println(patches.last().patchName);
}

void renderReinitialisePage() {
  tft.fillScreen(ST7735_BLACK);
  tft.setFont(&FreeSans12pt7b);
  tft.setTextColor(ST7735_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(5, 53);
  tft.println("Initialise to");
  tft.setCursor(5, 96);
  tft.println("panel setting");
}

void renderPatchNamingPage() {
  tft.fillScreen(ST7735_BLACK);
  tft.setFont(&FreeSans12pt7b);
  tft.setTextColor(ST7735_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(0, 53);
  tft.println("Rename Patch");
  tft.drawFastHLine(10, 80, tft.width() - 20, ST7735_RED);
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(5, 96);
  tft.println(newPatchName);
}

void renderRecallPage() {
  tft.fillScreen(ST7735_BLACK);
  tft.setFont(&FreeSans9pt7b);
  tft.setCursor(0, 45);
  tft.setTextColor(ST7735_YELLOW);
  tft.println(patches.last().patchNo);
  tft.setCursor(35, 45);
  tft.setTextColor(ST7735_WHITE);
  tft.println(patches.last().patchName);

  tft.fillRect(0, 72, tft.width(), 30, ST77XX_RED);
  tft.setCursor(0, 82);
  tft.setTextColor(ST7735_YELLOW);
  tft.println(patches.first().patchNo);
  tft.setCursor(35, 82);
  tft.setTextColor(ST7735_WHITE);
  tft.println(patches.first().patchName);

  tft.setCursor(0, 118);
  tft.setTextColor(ST7735_YELLOW);
  patches.size() > 1 ? tft.println(patches[1].patchNo) : tft.println(patches.last().patchNo);
  tft.setCursor(35, 118);
  tft.setTextColor(ST7735_WHITE);
  patches.size() > 1 ? tft.println(patches[1].patchName) : tft.println(patches.last().patchName);
}

void showRenamingPage(String newName) {
  newPatchName = newName;
}

void renderUpDown(uint16_t x, uint16_t y, uint16_t colour) {
  //Produces up/down indicator glyph at x,y
  tft.setCursor(x, y);
  tft.fillTriangle(x, y, x + 8, y - 8, x + 16, y, colour);
  tft.fillTriangle(x, y + 4, x + 8, y + 12, x + 16, y + 4, colour);
}


void renderSettingsPage() {
  tft.fillScreen(ST7735_BLACK);
  tft.setFont(&FreeSans12pt7b);
  tft.setTextColor(ST7735_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(0, 53);
  tft.println(currentSettingsOption);
  if (currentSettingsPart == SETTINGS) renderUpDown(160, 60, ST7735_YELLOW);
  tft.drawFastHLine(10, 80, tft.width() - 20, ST7735_RED);
  tft.setTextColor(ST7735_WHITE);
  tft.setCursor(5, 96);
  tft.println(currentSettingsValue);
  if (currentSettingsPart == SETTINGSVALUE) renderUpDown(160, 100, ST7735_WHITE);
      tft.setTextSize(1);
      tft.setTextColor(ST7735_WHITE);
      tft.setCursor(0, 160);
      tft.print("Patch Settings");

      tft.setCursor(0, 190);
      tft.print("PB Depth");
      tft.setCursor(160, 190);
      tft.print("MW Depth");
      tft.setCursor(0, 220);
      tft.print("AT Depth");
      tft.setCursor(160, 220);
      tft.print("AT Dest");
      tft.setTextColor(ST7735_YELLOW);
      tft.setCursor(110, 190);
      if (PitchBendLevel == 0 ) {
        tft.print("Off");
      } else {
        tft.print(PitchBendLevel);
      }
      tft.setCursor(280, 190);
      if (modWheelDepth == 0 ) {
        tft.print("Off");
      } else {
        tft.print(modWheelDepth);
      }
      tft.setCursor(260, 220);
      if (AfterTouchDest == 0 ) {
        tft.print("Off");
      } 
      if (AfterTouchDest == 1 ) {
        tft.print("DCO");
      } 
      if (AfterTouchDest == 2 ) {
        tft.print("Cut");
      } 
      if (AfterTouchDest == 3 ) {
        tft.print("VCF");
      } 
      if (AfterTouchDest == 4 ) {
        tft.print("VCA");
      } 
      tft.setCursor(110, 220);
      if (afterTouchDepth == 0 ) {
        tft.print("Off");
      } else {
        tft.print(afterTouchDepth);
      }
}

void showCurrentParameterPage(const char *param, float val, int pType) {
  currentParameter = param;
  currentValue = String(val);
  currentFloatValue = val;
  paramType = pType;
  //startTimer();
}

void showCurrentParameterPage(const char *param, String val, int pType) {
  if (state == SETTINGS || state == SETTINGSVALUE) state = PARAMETER;  //Exit settings page if showing
  currentParameter = param;
  currentValue = val;
  paramType = pType;
  //startTimer();
}

void showCurrentParameterPage(const char *param, String val) {
  showCurrentParameterPage(param, val, PARAMETER);
}

void showPatchPage(String number, String patchName) {
  currentPgmNum = number;
  currentPatchName = patchName;
}

void showSettingsPage(const char *option, const char *value, int settingsPart) {
  currentSettingsOption = option;
  currentSettingsValue = value;
  currentSettingsPart = settingsPart;
}

void displayThread() {
  threads.delay(2000);  //Give bootup page chance to display
  while (1) {
    switch (state) {
      case PARAMETER:
        // if ((millis() - timer) > DISPLAYTIMEOUT) {
          renderCurrentPatchPage();
        // } else {
        //   renderCurrentParameterPage();
        // }
        break;
      case RECALL:
        renderRecallPage();
        break;
      case SAVE:
        renderSavePage();
        break;
      case REINITIALISE:
        renderReinitialisePage();
        tft.updateScreen();  //update before delay
        threads.delay(1000);
        state = PARAMETER;
        break;
      case PATCHNAMING:
        renderPatchNamingPage();
        break;
      case PATCH:
        renderCurrentPatchPage();
        break;
      case DELETE:
        renderDeletePatchPage();
        break;
      case DELETEMSG:
        renderDeleteMessagePage();
        break;
      case SETTINGS:
      case SETTINGSVALUE:
        renderSettingsPage();
        break;
    }
    tft.updateScreen();
  }
}

void setupDisplay() {

  tft.init(240, 320);
  tft.useFrameBuffer(true);
  //tft.initR(INITR_18BLACKTAB);
  tft.setRotation(1);
  tft.invertDisplay(true);
  tft.fillScreen(ST7735_BLACK);
  renderBootUpPage();
  tft.updateScreen();
  threads.addThread(displayThread);
}
