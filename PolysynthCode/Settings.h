#include "SettingsService.h"

void settingsMIDICh();
void settingsMIDIOutCh();
void settingsEncoderDir();
void settingsPitchBendDepth();
void settingsModWheelDepth();
void settingsAfterTouch();
void settingsafterTouchDepth();
void settingsUpdateParams();
void settingsSendNotes();

int currentIndexMIDICh();
int currentIndexMIDIOutCh();
int currentIndexEncoderDir();
int currentIndexPitchBendDepth();
int currentIndexModWheelDepth();
int currentIndexAfterTouch();
int currentIndexafterTouchDepth();
int currentIndexUpdateParams();
int currentIndexSendNotes();

void settingsMIDICh(int index, const char *value) {
  if (strcmp(value, "ALL") == 0) {
    midiChannel = MIDI_CHANNEL_OMNI;
  } else {
    midiChannel = atoi(value);
  }
  storeMidiChannel(midiChannel);
}

void settingsMIDIOutCh(int index, const char *value) {
  if (strcmp(value, "Off") == 0) {
    midiOutCh = 0;
  } else {
    midiOutCh = atoi(value);
  }
  storeMidiOutCh(midiOutCh);
}

void settingsEncoderDir(int index, const char *value) {
  if (strcmp(value, "Type 1") == 0) {
    encCW = true;
  } else {
    encCW = false;
  }
  storeEncoderDir(encCW ? 1 : 0);
}

void settingsPitchBendDepth(int index, const char *value) {
  if (strcmp(value, "Off") == 0) {
    PitchBendLevel = 0;
  } else {
    PitchBendLevel = atoi(value);
  }
  storePitchBendDepth(PitchBendLevel);
}

void settingsModWheelDepth(int index, const char *value) {
  if (strcmp(value, "Off") == 0) {
    modWheelDepth = 0;
  } else {
    modWheelDepth = atoi(value);
  }
  storeModWheelDepth(modWheelDepth);
}

void settingsAfterTouch(int index, const char *value) {
  if (strcmp(value, "Off") == 0) AfterTouchDest = 0;
  if (strcmp(value, "DCO Mod") == 0) AfterTouchDest = 1;
  if (strcmp(value, "CutOff Freq") == 0) AfterTouchDest = 2;
  if (strcmp(value, "VCF Mod") == 0) AfterTouchDest = 3;
  if (strcmp(value, "AM Mod") == 0) AfterTouchDest = 4;
  storeAfterTouch(AfterTouchDest);
}

void settingsafterTouchDepth(int index, const char *value) {
  if (strcmp(value, "Off") == 0) {
    afterTouchDepth = 0;
  } else {
    afterTouchDepth = atoi(value);
  }
  storeafterTouchDepth(afterTouchDepth);
}

void settingsUpdateParams(int index, const char *value) {
  if (strcmp(value, "Send Params") == 0) {
    updateParams = true;
  } else {
    updateParams = false;
  }
  storeUpdateParams(updateParams ? 1 : 0);
}

void settingsSendNotes(int index, const char *value) {
  if (strcmp(value, "Send Notes") == 0) {
    sendNotes = true;
  } else {
    sendNotes = false;
  }
  storeSendNotes(sendNotes ? 1 : 0);
}

int currentIndexMIDICh() {
  return getMIDIChannel();
}

int currentIndexMIDIOutCh() {
  return getMIDIOutCh();
}

int currentIndexEncoderDir() {
  return getEncoderDir() ? 0 : 1;
}

int currentIndexPitchBendDepth() {
  return getPitchBendDepth();
}

int currentIndexModWheelDepth() {
  return getModWheelDepth();
}

int currentIndexAfterTouch() {
  return getAfterTouch();
}

int currentIndexafterTouchDepth() {
  return getafterTouchDepth();
}

int currentIndexUpdateParams() {
  return getUpdateParams() ? 1 : 0;
}

int currentIndexSendNotes() {
  return getSendNotes() ? 1 : 0;
}

// add settings to the circular buffer
void setUpSettings() {
  settings::append(settings::SettingsOption{ "MIDI Ch.", { "All", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "\0" }, settingsMIDICh, currentIndexMIDICh });
  settings::append(settings::SettingsOption{ "MIDI Out Ch.", { "Off", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "\0" }, settingsMIDIOutCh, currentIndexMIDIOutCh });
  settings::append(settings::SettingsOption{ "Encoder", { "Type 1", "Type 2", "\0" }, settingsEncoderDir, currentIndexEncoderDir });
  settings::append(settings::SettingsOption{ "PB Depth", { "Off", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "\0" }, settingsPitchBendDepth, currentIndexPitchBendDepth });
  settings::append(settings::SettingsOption{ "MW Depth", { "Off", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "\0" }, settingsModWheelDepth, currentIndexModWheelDepth });
  settings::append(settings::SettingsOption{ "AT Destination", { "Off", "DCO Mod", "CutOff Freq", "VCF Mod", "AM Mod", "\0" }, settingsAfterTouch, currentIndexAfterTouch });
  settings::append(settings::SettingsOption{ "AT Depth", { "Off", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "\0" }, settingsafterTouchDepth, currentIndexafterTouchDepth });
  settings::append(settings::SettingsOption{ "USB Params", { "Off", "Send Params", "\0" }, settingsUpdateParams, currentIndexUpdateParams });
  settings::append(settings::SettingsOption{ "USB Notes", { "Off", "Send Notes", "\0" }, settingsSendNotes, currentIndexSendNotes });
}
