#include <EEPROM.h>

#define EEPROM_MIDI_CH 0
#define EEPROM_ENCODER_DIR 1
#define EEPROM_LAST_PATCH 2
#define EEPROM_MIDI_OUT_CH 3
#define EEPROM_CC_TYPE 4
#define EEPROM_UPDATE_PARAMS 5
#define EEPROM_LED_INTENSITY 6
#define EEPROM_SLIDER_INTENSITY 7
#define EEPROM_SEND_NOTES 8

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

int getLEDintensity() {
  byte li = EEPROM.read(EEPROM_LED_INTENSITY);
  if (li < 0 || li > 10) li = 10; //If EEPROM has no intesity stored
  return li;
}

void storeLEDintensity(byte LEDintensity){
  EEPROM.update(EEPROM_LED_INTENSITY, LEDintensity);
}

int getSLIDERintensity() {
  int si = EEPROM.read(EEPROM_SLIDER_INTENSITY);
  if (si < 0 || si > 1) si = 1; //If EEPROM has no intesity stored
  return si;
}

void storeSLIDERintensity(int SLIDERintensity){
  EEPROM.update(EEPROM_SLIDER_INTENSITY, SLIDERintensity);
}

int getCCType() {
  byte ccType = EEPROM.read(EEPROM_CC_TYPE);
  if (ccType < 0 || ccType > 2) ccType = 0;//If EEPROM has no CC type stored
  return ccType;
}

void storeCCType(byte ccType){
  EEPROM.update(EEPROM_CC_TYPE, ccType);
}
