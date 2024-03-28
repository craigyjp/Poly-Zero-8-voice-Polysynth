//PRESET 1st line
const char str01[8][14] PROGMEM = {//str1 is effect name of 1st line
 "Chorus", "Flange", "Tremolo", "Pitch", "Pitch", "No Effect", "Reverb", "Reverb"
};
const char* const name01[] PROGMEM = {
 str01[0], str01[1], str01[2], str01[3], str01[4], str01[5], str01[6], str01[7],
};

//PRESET 2nd line
const char str02[8][14] PROGMEM = {//str1 is effect name of 1st line
 "Reverb", "Reverb", "Reverb", "Shift", "Echo", " ", "1", "2"
};
const char* const name02[] PROGMEM = {
 str02[0], str02[1], str02[2], str02[3], str02[4], str02[5], str02[6], str02[7],
};

//PRESET param1
const char str03[8][14] PROGMEM = {//str1 is effect name of 1st line
 "Reverb Mix", "Reverb Mix", "Reverb Mix", "Pitch", "Pitch", "-", "Reverb Time", "Reverb Time"
};
const char* const name03[] PROGMEM = {
 str03[0], str03[1], str03[2], str03[3], str03[4], str03[5], str03[6], str03[7],
};

//PRESET param2
const char str04[8][14] PROGMEM = {//str1 is effect name of 1st line
 "Chorus Rate", "Flanger Rate", "Tremelo Rate", "-", "Echo Delay", "-", "HF Filter", "HF Filter"
};
const char* const name04[] PROGMEM = {
 str04[0], str04[1], str04[2], str04[3], str04[4], str04[5], str04[6], str04[7],
};

//PRESET param3
const char str05[8][14] PROGMEM = {//str1 is effect name of 1st line
 "Chorus Mix", "Flanger Mix", "Tremelo Mix", "-", "Echo Mix", "-", "LF Filter", "LF Filter"
};
const char* const name05[] PROGMEM = {
 str05[0], str05[1], str05[2], str05[3], str05[4], str05[5], str05[6], str05[7],
};

//ROM1 1st line
const char str11[8][14] PROGMEM = {//str1 is effect name of 1st line
"Qubit","SuperNova","Modulated","Echo","Shimmer","Sample &","Modulated","Glitch Bit"
};
const char* const name11[] PROGMEM = {
 str11[0], str11[1], str11[2], str11[3], str11[4], str11[5], str11[6], str11[7],
};

//ROM1 2nd line
const char str12[8][14] PROGMEM = {//str1 is effect name of 1st line
"Delay","Delay","Delay","Reverb","Reverb","Hold","Reverb","Delay"
};
const char* const name12[] PROGMEM = {
 str12[0], str12[1], str12[2], str12[3], str12[4], str12[5], str12[6], str12[7],
};

//ROM1 param1
const char str13[8][14] PROGMEM = {//str1 is effect name of 1st line
"Time","Time","Time","Reverb Level","Dwell","Time","Dwell","Delay 1"

};
const char* const name13[] PROGMEM = {
 str13[0], str13[1], str13[2], str13[3], str13[4], str13[5], str13[6], str13[7],
};

//ROM1 param2
const char str14[8][14] PROGMEM = {//str1 is effect name of 1st line
"Modulation","Filter","Feedback","Delay Time","Pitch","Feedback","Depth","Delay 2"

};
const char* const name14[] PROGMEM = {
 str14[0], str14[1], str14[2], str14[3], str14[4], str14[5], str14[6], str14[7],
};

//ROM1 param3
const char str15[8][14] PROGMEM = {//str1 is effect name of 1st line
"Feedback","Feedback","Modulation","Echo Level","Blend","Modulation","Rate","Feedback"

};
const char* const name15[] PROGMEM = {
 str15[0], str15[1], str15[2], str15[3], str15[4], str15[5], str15[6], str15[7],
};

//ROM2 1st line
const char str21[8][14] PROGMEM = {//str1 is effect name of 1st line
"Daydream","Starfield","Dual Pitch","Triple","Reverse","Wah","Vibrato","Phaser"
};
const char* const name21[] PROGMEM = {
 str21[0], str21[1], str21[2], str21[3], str21[4], str21[5], str21[6], str21[7],
};

//ROM2 2nd line
const char str22[8][14] PROGMEM = {//str1 is effect name of 1st line
"Delay","Delay","Shift","Delay","Delay","Reverb","Reverb","Reverb"
};
const char* const name22[] PROGMEM = {
 str22[0], str22[1], str22[2], str22[3], str22[4], str22[5], str22[6], str22[7],
};

//ROM2 param1
const char str23[8][14] PROGMEM = {//str1 is effect name of 1st line
"Time","Time","Pitch 1","Delay 1","Sample","Reverb","Reverb","Reverb"
};
const char* const name23[] PROGMEM = {
 str23[0], str23[1], str23[2], str23[3], str23[4], str23[5], str23[6], str23[7],
};

//ROM2 param2
const char str24[8][14] PROGMEM = {//str1 is effect name of 1st line
"Feedback","Feedback","1-Mix-2","Delay 2","Feedback","Rate","Rate","Rate"
};
const char* const name24[] PROGMEM = {
 str24[0], str24[1], str24[2], str24[3], str24[4], str24[5], str24[6], str24[7],
};

//ROM2 param3
const char str25[8][14] PROGMEM = {//str1 is effect name of 1st line
"Filter","Phaser","Pitch 2","Delay 3","Delay","Wah","Vibrato","Phaser"
};
const char* const name25[] PROGMEM = {
 str25[0], str25[1], str25[2], str25[3], str25[4], str25[5], str25[6], str25[7],
};

//ROM3 1st line
const char str31[8][14] PROGMEM = {//str1 is effect name of 1st line
"Phaser","Flanger","VP330","Cathedral","Rotor","Ensemble","Leslie","Wah Wah"
};
const char* const name31[] PROGMEM = {
 str31[0], str31[1], str31[2], str31[3], str31[4], str31[5], str31[6], str31[7],
};

//ROM3 2nd line
const char str32[8][14] PROGMEM = {//str1 is effect name of 1st line
"Bass","Bass","Ensemble","","Effect","Effect","Effect",""
};
const char* const name32[] PROGMEM = {
 str32[0], str32[1], str32[2], str32[3], str32[4], str32[5], str32[6], str32[7],
};

//ROM3 param1
const char str33[8][14] PROGMEM = {//str1 is effect name of 1st line
"Rate","Rate","Reverb Level","Reverb Level","Reverb Level","Reverb Level","Reverb Level","Reverb Level"
};
const char* const name33[] PROGMEM = {
 str33[0], str33[1], str33[2], str33[3], str33[4], str33[5], str33[6], str33[7],
};

//ROM3 param2
const char str34[8][14] PROGMEM = {//str1 is effect name of 1st line
"Depth","Depth","Cho/Ens Mix","Feedback","Depth","Ens Mix","Filter Freq","Filter Q"
};
const char* const name34[] PROGMEM = {
 str34[0], str34[1], str34[2], str34[3], str34[4], str34[5], str34[6], str34[7],
};

//ROM3 param3
const char str35[8][14] PROGMEM = {//str1 is effect name of 1st line
"Feedback","Feedback","Ensemble","Speed","Speed","Treble","Speed","Sensitivity"
};
const char* const name35[] PROGMEM = {
 str35[0], str35[1], str35[2], str35[3], str35[4], str35[5], str35[6], str35[7],
};

//filter01
const char str40[8][14] PROGMEM = {//str1 is effect name of 1st line
"4P LowPass","2P LowPass","4P HighPass","2P HighPass","4P BandPass","2P BandPass","3P AllPass","Notch"
};
const char* const filter02[] PROGMEM = {
 str40[0], str40[1], str40[2], str40[3], str40[4], str40[5], str40[6], str40[7],
};

//filter02
const char str41[8][14] PROGMEM = {//str1 is effect name of 1st line
"3P LowPass","1P LowPass","3P HP + 1P LP","1P HP + 1P LP","2P HP + 1P LP","2P BP + 1P LP","3P AP + 1P LP","2P Notch + LP"
};
const char* const filter01[] PROGMEM = {
 str41[0], str41[1], str41[2], str41[3], str41[4], str41[5], str41[6], str41[7],
};

//filter01
const char str42[8][14] PROGMEM = {//str1 is effect name of 1st line
"Sawtooth Up","Sawtooth Dn","Square","Triangle","Sine","Sweep","Lumps","Random"
};
const char* const lfo01[] PROGMEM = {
 str42[0], str42[1], str42[2], str42[3], str42[4], str42[5], str42[6], str42[7],
};

//filter02
const char str43[8][14] PROGMEM = {//str1 is effect name of 1st line
"Saw + Octave","Quad Saw","Quad Pulse","Tri Step","Sine + Octave","Sine + 3rd","Sine + 4th","Slopes"
};
const char* const lfo02[] PROGMEM = {
 str43[0], str43[1], str43[2], str43[3], str43[4], str43[5], str43[6], str43[7],
};