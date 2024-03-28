int transpose = -12;
int realoctave = -36;
int keyboardMode = 0;
int noteMsg;
unsigned int velmV;
int readRes = 1023;
int parameterGroup = 0;


//Values below are just for initialising and will be changed when synth is initialised to current panel controls & EEPROM settings
byte midiChannel = MIDI_CHANNEL_OMNI;//(EEPROM)
byte midiOutCh = 1;//(EEPROM)
byte LEDintensity = 10;//(EEPROM)
byte oldLEDintensity;
int SLIDERintensity = 1;//(EEPROM)
int oldSLIDERintensity;
int i = 0;

//Delayed LFO
int numberOfNotes = 0;
int oldnumberOfNotes = 0;
unsigned long previousMillis = 0;
unsigned long interval = 1; //10 seconds
long delaytime  = 0;

int resolutionFrig = 3;
boolean recallPatchFlag = false;
boolean learning = false;
boolean noteArrived = false;
int setCursorPos = 0;

int CC_ON = 127;
int CC_OFF = 127;

int MIDIThru = midi::Thru::Off;//(EEPROM)
String patchName = INITPATCHNAME;
boolean encCW = true;//This is to set the encoder to increment when turned CW - Settings Option
boolean updateParams = false;  //(EEPROM)
boolean sendNotes = false;  //(EEPROM)

int oldmidiChannel = 0;
int oldAfterTouchDest = 0;
int oldClockSource = 0;
int oldafterTouchDepth = 0;
int afterTouchDepth = 0;
int NotePriority = 0;
int FilterLoop = 0;
int AmpLoop = 0;
int ClockSource = 0;

// polykit parameters in order of mux

float osc1Tune = 0;
int osc1Tunestr = 0; // for display
float initialosc1Tune = 448;
float osc1fmDepth = 0;
int osc1fmDepthstr = 0;
float osc2fmDepth = 0;
int osc2fmDepthstr = 0;
float osc2Tune = 0;
int osc2Tunestr = 0;
float initialosc2Tune = 384;
float osc1WaveMod = 0;
int osc1WaveModstr = 0;
float osc2WaveMod = 0;
int osc2WaveModstr = 0;
int osc1WaveSelect = 0;
int osc1WaveSelectstr = 0;
int oct1 = 0;
int oct2 = 0;
int osc2WaveSelect = 0;
int osc2WaveSelectstr = 0;
float osc1WaveA = 0;
float osc1WaveB = 0;
float osc1WaveC = 0;
float osc2WaveA = 0;
float osc2WaveB = 0;
float osc2WaveC = 0;
float glideTime = 0;
int glideTimestr = 0;
float glideTimedisplay = 0;
int glideSW = 0;
float noiseLevel = 0;
int noiseLevelstr = 0;
float noiseLeveldisplay = 0.00;
int osc2Levelstr = 0; //for display
float osc2Level = 0;
float osc1Level = 0;
int osc1Levelstr = 0;
float osc1fmWaveMod = 0;
int osc1fmWaveModstr = 0; // for display
float osc2fmWaveMod = 0;
int osc2fmWaveModstr = 0; // for display

float filterCutoff = 0;
float oldfilterCutoff = 0;
int filterCutoffstr = 0; // for display
float filterLFO = 0;
int filterLFOstr = 0; // for display
float filterRes = 0;
int filterResstr = 0;
int filterType = 0;
float filterA = 0;
float filterB = 0;
float filterC = 0;
float filterEGlevel = 0;
int filterEGlevelstr = 0;
float LFORate = 0;
int LFORatestr = 0;
float LFORatedisplay = 0;
int LFODelayGo = 0;
float lfoDelay = 0;
int lfoDelaystr = 0; //for display
String StratusLFOWaveform = "                ";
String Oscillator1Waveform = "                ";
String Oscillator2Waveform = "                ";
int LFOWaveform = 0;
int LFOWaveCV = 0;
float filterAttack = 0;
int filterAttackstr = 0;
float filterDecay = 0;
int filterDecaystr = 0;
float filterSustain = 0;
int filterSustainstr = 0;
float filterRelease = 0;
int filterReleasestr = 0;
int ampAttack = 0;
int oldampAttack = 0;
int ampAttackstr = 0;
int ampDecay = 0;
int oldampDecay = 0;
int ampDecaystr = 0;
int ampSustain = 0;
int oldampSustain = 0;
int ampSustainstr = 0;
int ampRelease = 0;
int oldampRelease = 0;
int ampReleasestr = 0;
int AmpGatedSW = 0;
float amDepth = 0;
int amDepthstr = 0;
float volumeControl = 0;
int volumeControlstr = 0; // for display
float keyTrack = 0;
float keyTrackMult = 0.00;
int keyTrackstr = 0;
float effect1 = 0;
int effect1str = 0;
float effect2 = 0;
int effect2str = 0;
float effect3 = 0;
int effect3str = 0;
int mixa = 0;
int mixastr = 0;
int mixbstr = 0;
int osc1Bank = 0;
int osc1BankB = 0;
int osc2Bank = 0;
int osc2BankB = 0;
int lfoAlt = 0;
int lfoMult = 0;
int LFOMultCV = 0;
int filterPoleSW = 0;
int filterEGinv = 0;
int PitchBendLevel = 0;
int PitchBendLevelstr = 0; // for display
int modWheelDepth = 0;
float modWheelLevel = 0;
int modWheelLevelstr = 0;
int filterLogLin = 0;
int ampLogLin = 0;
float afterTouch = 0;
int AfterTouchDest = 0;
float masterTune = 511;

int filterVelSW = 0;
int ampVelSW = 0;
int monoMultiSW = 0;

int effectBankSW = 0;
int currentbank = 0;
int effectNumSW = 0;

int envLinLogSW = 0;

int returnvalue = 0;

//Pick-up - Experimental feature
//Control will only start changing when the Knob/MIDI control reaches the current parameter value
//Prevents jumps in value when the patch parameter and control are different values
boolean pickUp = false;//settings option (EEPROM)
boolean pickUpActive = false;
#define TOLERANCE 2 //Gives a window of when pick-up occurs, this is due to the speed of control changing and Mux reading

//#define NOTE_SF 547.00f
#define NOTE_SF 271.5
#define VEL_SF 256.0

uint32_t int_ref_on_flexible_mode = 0b00001001000010100000000000000000;  // { 0000 , 1001 , 0000 , 1010000000000000 , 0000 }

uint32_t sample_data = 0b00000000000000000000000000000000;
uint32_t sample_data1 = 0b00000000000000000000000000000000;
uint32_t channel_a = 0b00000010000000000000000000000000;
uint32_t channel_b = 0b00000010000100000000000000000000;
uint32_t channel_c = 0b00000010001000000000000000000000;
uint32_t channel_d = 0b00000010001100000000000000000000;
uint32_t channel_e = 0b00000010010000000000000000000000;
uint32_t channel_f = 0b00000010010100000000000000000000;
uint32_t channel_g = 0b00000010011000000000000000000000;
uint32_t channel_h = 0b00000010011100000000000000000000;
