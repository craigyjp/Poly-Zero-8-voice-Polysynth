#include <hardware/pwm.h>
#include <Adafruit_NeoPixel.h>
#include "Parameters.h"
#include <MIDI.h>

#define MIDI_CHANNEL 1

#define NEO_PIXEL_OUT 16
#define OSC1OR2 8

//#define VOLT_OCT_CV 26
#define PB_CV 26
#define FREQ_CV 27
#define LFO_FM_CV 28
#define MOD_CV 29

#define PWM_OUT 2

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

Adafruit_NeoPixel pixel(1, NEO_PIXEL_OUT, NEO_GRB + NEO_KHZ800);
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

void setup() {

  pinMode(OSC1OR2, INPUT_PULLUP);  //vco1 or 2 select
  timer = micros();

  table_set();  //set wavetable

  //-------------------octave select-------------------------------
  for (int i = 0; i < 1230; i++) {  //Covers 6(=1230) octaves. If it is 1230 or more, the operation becomes unstable.
    freq_table[i] = f0 * pow(2, (voctpow[i]));
  }
  for (int i = 0; i < 2048 - 1230; i++) {
    freq_table[i + 1230] = 6;
  }

  //-------------------PWM setting-------------------------------
  gpio_set_function(PWM_OUT, GPIO_FUNC_PWM);   // set GP2 function PWM
  slice_num = pwm_gpio_to_slice_num(PWM_OUT);  // GP2 PWM slice

  pwm_clear_irq(slice_num);
  pwm_set_irq_enabled(slice_num, true);
  irq_set_exclusive_handler(PWM_IRQ_WRAP, on_pwm_wrap);
  irq_set_enabled(PWM_IRQ_WRAP, true);

  //set PWM frequency
  pwm_set_clkdiv(slice_num, 1);      //=sysclock/((resolution+1)*frequency)
  pwm_set_wrap(slice_num, 1023);     //resolutio
  pwm_set_enabled(slice_num, true);  //PWM output enable

  pixel.begin();
  pixel.setPixelColor(0, pixel.Color(colour[0][0], colour[0][1], colour[0][2]));
  pixel.show();

  MIDI.begin(0);
  MIDI.setHandleControlChange(myControlChange);
  MIDI.setHandleNoteOn(DinHandleNoteOn);
  MIDI.setHandleNoteOff(DinHandleNoteOff);

  osc2 = digitalRead(OSC1OR2);

  delay(2000);
}

void myControlChange(byte channel, byte control, byte value) {
  if (!osc2) {
    switch (control) {
      case 65:
        glidesw = map(value, 0, 1, 1, 0);
        break;
      case 5:
        glidetime = map(value, 0, 127, 0, 4000);
        break;
      case 70:
        waveform = value;
        break;
      case 16:
        mode = value;
        break;
      case 17:
        int oct_sel = map(value, 0, 127, 0, 2);
        switch (oct_sel) {
          case 0:
            oct_sw = 0;
            break;
          case 1:
            oct_sw = 1;
            break;
          case 2:
            oct_sw = 3;
            break;
        }
        break;
    }
  } else if (osc2) {
    switch (control) {
      case 65:
        glidesw = map(value, 0, 1, 1, 0);
        break;
      case 5:
        glidetime = map(value, 0, 127, 0, 4000);
        break;
      case 71:
        waveform = value;
        break;
      case 18:
        mode = value;
        break;
      case 19:
        int oct_sel = map(value, 0, 127, 0, 2);
        switch (oct_sel) {
          case 0:
            oct_sw = 0;
            break;
          case 1:
            oct_sw = 1;
            break;
          case 2:
            oct_sw = 3;
            break;
        }
        break;
    }
  }
}

void DinHandleNoteOn(byte channel, byte note, byte velocity) {
  if (channel == MIDI_CHANNEL) {
    int newNoteVal = note;
    // Reset portamento time when a new note is pressed
    lastGlideTime = millis();
    if (newNoteVal < 36) {
      newNoteVal = 36;
    }
    if (newNoteVal > 103) {
      newNoteVal = 103;
    }
    if (newNoteVal > 35) {
      newNoteVal = newNoteVal - 36;
    }
    if (glidesw) {
      noteVal = newNoteVal;  // Directly set the note value
      target_freq = freq_table[int(newNoteVal * 16.84)];
      current_freq = target_freq;  // Update current frequency immediately in glide mode
      current_freq = constrain(current_freq, 0, 1225);
      prevNoteVal = noteVal;
    } else {
      target_freq = freq_table[int(newNoteVal * 16.84)];
      target_freq = constrain(target_freq, 0, 1225);
    }
  }
}

void DinHandleNoteOff(byte channel, byte note, byte velocity) {
  if (channel == MIDI_CHANNEL) {
  }
}

void on_pwm_wrap() {
  pwm_clear_irq(slice_num);
  f = f + osc_freq;
  if (f > 255) {
    f = 0;
  }
  int k = (int)f;
  pwm_set_chan_level(slice_num, PWM_CHAN_A, mod2_wavetable[k] + 511);
}

// Function to map linear input to exponential curve
float mapToExponential(float x, float exponent) {
  // Ensure x is within the range [0, 4000]
  x = constrain(x, 0, 4000);

  // Map the linear input to exponential curve
  float y = pow(x / 4000.0, exponent) * 4000.0;

  return y;
}

void loop() {

  //------------------- handle the MIDI --------------------------
  MIDI.read();

  float exponentglidetime = mapToExponential(glidetime, exponent);

  // Update current frequency with portamento
  unsigned long currentTime = millis();
  if (!glidesw) {
    float elapsedTime = (currentTime - lastGlideTime) / 1000.0;  // Convert milliseconds to seconds
    if (elapsedTime >= exponentglidetime) {
      current_freq = target_freq;  // Reached the target frequency
    } else {
      // Calculate glide rate based on the difference between current and target frequencies
      float glide_distance = target_freq - current_freq;
      glide_rate = glide_distance / (exponentglidetime / 1000.0);  // Convert glidetime to seconds
      // Update current frequency
      current_freq += glide_rate * elapsedTime;
    }
  }

  // Update previous note value and last glide time
  prevNoteVal = noteVal;
  lastGlideTime = currentTime;

  // Update PWM or other processing based on current frequency
  // ...
  if (!osc2) {
    freq_pot = map(analogRead(FREQ_CV), 0, 1023, 0, 63);
  }

  if (osc2) {
    freq_pot = map(analogRead(FREQ_CV), 0, 1023, 23, 39);
  }

  freq_pot = freq_pot - (1 << 5);
  fm_input = map(analogRead(LFO_FM_CV), 0, 1023, 0, 63);
  fm_input = fm_input - (1 << 5);
  pb_input = map(analogRead(PB_CV), 0, 1023, 0, 63);
  pb_input = pb_input - (1 << 5);
  osc_freq = 256 * (current_freq + fm_input + pb_input + freq_pot) / 122070 * (1 + oct_sw);


  //  -------------------mod parameter set-------------------------------

  if (mode == 0) {                                                   //fold
    if (2 != waveform) {                                             // normal wavefold
      mod = constrain(analogRead(MOD_CV), 0, 1023) * 0.0036 + 0.90;  //28 is CV , 29 is pot
    } else {                                                         // PWM for square wave
      mod = constrain(analogRead(MOD_CV), 0, 1023) / 8;
    }
  } else if (mode == 1) {  //FM
    mod = constrain(analogRead(MOD_CV), 0, 1023) / 8;
  } else if (mode == 2) {  //AM
    mod = 1023 - constrain(analogRead(MOD_CV), 0, 1023);
  }

  if (mode == 0 || mode == 2) {
    table_set();
  }

  // ----------------------- set the neopixel colour -------------------------

  pixel.clear();
  pixel.setPixelColor(0, pixel.Color(colour[waveform][0], colour[waveform][1], colour[waveform][2]));
  pixel.show();


  //void loop1() {//modulation
  if (mode == 0) {  // wavefold
    if (2 == waveform) {
      // PWM rather than wavefolding for square wave
      for (int i = 0; i < 128 + mod; i++) {
        mod2_wavetable[i] = 511;
      }
      for (int i = 128 + mod; i < 256; i++) {
        mod2_wavetable[i] = -511;
      }
    } else {
      for (int i = 0; i < 256; i++) {
        mod_wavetable[i] = wavetable[i] * mod;
      }
      for (int i = 0; i < 256; i++) {
        if (mod_wavetable[i] > 511 && mod_wavetable[i] < 1023 + 512) {
          mod2_wavetable[i] = 1024 - mod_wavetable[i];
        } else if (mod_wavetable[i] < -512 && mod_wavetable[i] > -1024 - 512) {
          mod2_wavetable[i] = -1023 - mod_wavetable[i];
        } else if (mod_wavetable[i] < -1024 - 511) {
          mod2_wavetable[i] = 2048 + mod_wavetable[i];
        } else if (mod_wavetable[i] > 1023 + 511) {
          mod2_wavetable[i] = -2047 + mod_wavetable[i];
        } else {
          mod2_wavetable[i] = mod_wavetable[i];
        }
      }
    }
  } else if (mode == 1) {  //FM
    switch (waveform) {
      case 0:
        for (int i = 0; i < 256; i++) {  //FM1
          mod2_wavetable[i] = (sin(2 * M_PI * i / 256 + mod / 128 * sin(2 * M_PI * 3 * i / 256))) * 511;
        }
        break;

      case 1:
        for (int i = 0; i < 256; i++) {  //FM2
          mod2_wavetable[i] = (sin(2 * M_PI * i / 256 + mod / 128 * sin(2 * M_PI * 3 * i / 256 + mod / 128 * sin(2 * M_PI * 3 * i / 256)))) * 511;
        }
        break;

      case 2:
        for (int i = 0; i < 256; i++) {  //FM3
          mod2_wavetable[i] = (sin(2 * M_PI * i / 256 + mod / 128 * sin(2 * M_PI * 5 * i / 256 + mod / 128 * sin(2 * M_PI * 7 * i / 256)))) * 511;
        }
        break;

      case 3:
        for (int i = 0; i < 256; i++) {  //FM4
          mod2_wavetable[i] = (sin(2 * M_PI * i / 256 + mod / 128 * sin(2 * M_PI * 9 * i / 256 + mod / 128 * sin(2 * M_PI * 5 * i / 256)))) * 511;
        }
        break;

      case 4:
        for (int i = 0; i < 256; i++) {  //FM5
          mod2_wavetable[i] = ((sin(2 * M_PI * i / 256 + mod / 128 * sin(2 * M_PI * 19 * i / 256))) + (sin(2 * M_PI * 3 * i / 256 + mod / 128 * sin(2 * M_PI * 7 * i / 256)))) / 2 * 511;
        }
        break;

      case 5:
        for (int i = 0; i < 256; i++) {  //FM6
          mod2_wavetable[i] = ((sin(2 * M_PI * i / 256 + mod / 128 * sin(2 * M_PI * 7 * i / 124))) + (sin(2 * M_PI * 9 * i / 368 + mod / 128 * sin(2 * M_PI * 11 * i / 256)))) * 511;
        }
        break;

      case 6:
        for (int i = 0; i < 256; i++) {  //FM7
          mod2_wavetable[i] = ((sin(2 * M_PI * i / 256 + mod / 128 * sin(2 * M_PI * 13 * i / 256))) + (sin(2 * M_PI * 17 * i / 111 + mod / 128 * sin(2 * M_PI * 19 * i / 89)))) * 511;
        }
        break;

      case 7:
        for (int i = 0; i < 256; i++) {  //FM8
          mod2_wavetable[i] = (sin(2 * M_PI * i / 256 + mod / 128 * sin(2 * M_PI * 11 * i / 124 + mod / 128 * sin(2 * M_PI * 7 * i / 333 + mod / 128 * sin(2 * M_PI * 9 * i / 56))))) * 511;
        }
        break;
    }
  }

  else if (mode == 2) {  //AM
    if (timer + mod <= micros()) {
      k++;
      if (k > 63) {
        k = 0;
      }
      AM_mod = sin(2 * M_PI * k / 63);  //make modulation sine wave
      for (int i = 0; i < 255; i++) {
        mod2_wavetable[i] = wavetable[i] * AM_mod;  //multiply AM sine wave
      }
      timer = micros();
    }
  }
}

void table_set() {  //make wavetable

  if (mode == 0) {  //wavefold
    switch (waveform) {
      case 0:
        for (int i = 0; i < 256; i++) {  //saw
          wavetable[i] = i * 4 - 512;
        }
        break;

      case 1:
        for (int i = 0; i < 256; i++) {  //sin
          wavetable[i] = (sin(2 * M_PI * i / 256)) * 511;
        }
        break;

      case 2:
        for (int i = 0; i < 128; i++) {  //squ
          wavetable[i] = 511;
          wavetable[i + 128] = -511;
        }
        break;

      case 3:
        for (int i = 0; i < 128; i++) {  //tri
          wavetable[i] = i * 8 - 511;
          wavetable[i + 128] = 511 - i * 8;
        }
        break;

      case 4:
        for (int i = 0; i < 128; i++) {  //oct saw
          wavetable[i] = i * 4 - 512 + i * 2;
          wavetable[i + 128] = i * 2 - 256 + i * 4;
        }
        break;

      case 5:
        for (int i = 0; i < 256; i++) {  //FM1
          wavetable[i] = (sin(2 * M_PI * i / 256 + sin(2 * M_PI * 3 * i / 256))) * 511;
        }
        break;

      case 6:
        for (int i = 0; i < 256; i++) {  //FM2
          wavetable[i] = (sin(2 * M_PI * i / 256 + sin(2 * M_PI * 7 * i / 256))) * 511;
        }
        break;

      case 7:
        for (int i = 0; i < 256; i++) {  //FM3
          wavetable[i] = (sin(2 * M_PI * i / 256 + sin(2 * M_PI * 4 * i / 256 + sin(2 * M_PI * 11 * i / 256)))) * 511;
        }
        break;
    }
  } else if (mode == 2) {  //AM
    switch (waveform) {
      case 0:
        for (int i = 0; i < 256; i++) {  //saw
          wavetable[i] = (sin(2 * M_PI * i / 256)) * 511;
        }
        break;

      case 1:
        for (int i = 0; i < 256; i++) {  //fm1
          wavetable[i] = (sin(2 * M_PI * i / 256 + sin(2 * M_PI * 3 * i / 256))) * 511;
        }
        break;

      case 2:
        for (int i = 0; i < 256; i++) {  //fm2
          wavetable[i] = (sin(2 * M_PI * i / 256 + sin(2 * M_PI * 5 * i / 256))) * 511;
        }
        break;

      case 3:
        for (int i = 0; i < 256; i++) {  //fm3
          wavetable[i] = (sin(2 * M_PI * i / 256 + sin(2 * M_PI * 4 * i / 256 + sin(2 * M_PI * 11 * i / 256)))) * 511;
        }
        break;

      case 4:
        for (int i = 0; i < 256; i++) {  //non-integer multiplets fm1
          wavetable[i] = (sin(2 * M_PI * i / 256 + sin(2 * M_PI * 1.28 * i / 256))) * 511;
        }
        break;

      case 5:
        for (int i = 0; i < 256; i++) {  //non-integer multiplets fm2
          wavetable[i] = (sin(2 * M_PI * i / 256 + sin(2 * M_PI * 3.19 * i / 256))) * 511;
        }
        break;

      case 6:
        for (int i = 0; i < 256; i++) {  //non-integer multiplets fm3
          wavetable[i] = (sin(2 * M_PI * i / 256 + sin(2 * M_PI * 2.3 * i / 256 + sin(2 * M_PI * 7.3 * i / 256)))) * 511;
        }
        break;

      case 7:
        for (int i = 0; i < 256; i++) {  //non-integer multiplets fm3
          wavetable[i] = (sin(2 * M_PI * i / 256 + sin(2 * M_PI * 6.3 * i / 256 + sin(2 * M_PI * 11.3 * i / 256)))) * 511;
        }
        break;
    }
  }
}
