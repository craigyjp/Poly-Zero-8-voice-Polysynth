#include <EEPROM.h>

#define EEPROM_MIDI_CH 0
#define EEPROM_ENCODER_DIR 1
#define EEPROM_LAST_PATCH 2
#define EEPROM_MIDI_OUT_CH 3

#define EEPROM_UPDATE_PARAMS 5

#define EEPROM_SEND_NOTES 8
#define EEPROM_AFTERTOUCH 9
#define EEPROM_PITCHBEND 10
#define EEPROM_AFTERTOUCH_DEPTH 11
#define EEPROM_MODWHEEL_DEPTH 12

int getMIDIChannel() {
  byte midiChannel = EEPROM.read(EEPROM_MIDI_CH);
  if (midiChannel < 0 || midiChannel > 16) midiChannel = MIDI_CHANNEL_OMNI;//If EEPROM has no MIDI channel stored
  return midiChannel;
}

void storeMidiChannel(byte channel)
{
  EEPROM.update(EEPROM_MIDI_CH, channel);
}

boolean getEncoderDir() {
  byte ed = EEPROM.read(EEPROM_ENCODER_DIR); 
  if ( ed < 0 || ed > 1 )return true; //If EEPROM has no encoder direction stored
  return ed == 1 ? true : false;
}

void storeEncoderDir(byte encoderDir)
{
  EEPROM.update(EEPROM_ENCODER_DIR, encoderDir);
}

boolean getUpdateParams() {
  byte params = EEPROM.read(EEPROM_UPDATE_PARAMS); 
  if ( params < 0 || params > 1 )return true; //If EEPROM has no encoder direction stored
  return params == 1 ? true : false;
}

float getAfterTouch() {
  byte AfterTouchDest = EEPROM.read(EEPROM_AFTERTOUCH);
  if (AfterTouchDest < 0  || AfterTouchDest > 10) return 0;
  return AfterTouchDest; //If EEPROM has no key tracking stored
}

void storeAfterTouch(int AfterTouchDest)
{
  EEPROM.update(EEPROM_AFTERTOUCH, AfterTouchDest);
}

float getModWheelDepth() {
  int mw = EEPROM.read(EEPROM_MODWHEEL_DEPTH);
  if (mw < 0 || mw > 10) return modWheelDepth; //If EEPROM has no mod wheel depth stored
  return mw;
}

void storeModWheelDepth(int mw)
{
  EEPROM.update(EEPROM_MODWHEEL_DEPTH, mw);
}

float getPitchBendDepth() {
  int PBDepth = EEPROM.read(EEPROM_PITCHBEND);
  if (PBDepth < 0 || PBDepth > 2) return PBDepth; //If EEPROM has no mod wheel depth stored
  return PBDepth;
}

void storePitchBendDepth(int PBDepth)
{
  EEPROM.update(EEPROM_PITCHBEND, PBDepth);
}

float getafterTouchDepth() {
  byte afterTouchDepth = EEPROM.read(EEPROM_AFTERTOUCH_DEPTH);
  if (afterTouchDepth < 0 || afterTouchDepth > 10) return afterTouchDepth; //If EEPROM has no mod wheel depth stored
  return afterTouchDepth;
}

void storeafterTouchDepth(int afterTouchDepth)
{
  EEPROM.update(EEPROM_AFTERTOUCH_DEPTH, afterTouchDepth);
}

void storeUpdateParams(byte updateParameters)
{
  EEPROM.update(EEPROM_UPDATE_PARAMS, updateParameters);
}

boolean getSendNotes() {
  byte sn = EEPROM.read(EEPROM_SEND_NOTES); 
  if ( sn < 0 || sn > 1 )return true; //If EEPROM has no encoder direction stored
  return sn == 1 ? true : false;
}

void storeSendNotes(byte sendUSBNotes)
{
  EEPROM.update(EEPROM_SEND_NOTES, sendUSBNotes);
}

int getLastPatch() {
  int lastPatchNumber = EEPROM.read(EEPROM_LAST_PATCH);
  if (lastPatchNumber < 1 || lastPatchNumber > 999) lastPatchNumber = 1;
  return lastPatchNumber;
}

void storeLastPatch(int lastPatchNumber)
{
  EEPROM.update(EEPROM_LAST_PATCH, lastPatchNumber);
}

int getMIDIOutCh() {
  byte mc = EEPROM.read(EEPROM_MIDI_OUT_CH);
  if (mc < 0 || midiOutCh > 16) mc = 0;//If EEPROM has no MIDI channel stored
  return mc;
}

void storeMidiOutCh(byte midiOutCh){
  EEPROM.update(EEPROM_MIDI_OUT_CH, midiOutCh);
}

