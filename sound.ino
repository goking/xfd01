#include "pitches.h"

// notes in the alert melody:
const int alertMelody[] = {
  NOTE_B6, NOTE_G6, NOTE_B6, NOTE_G6, NOTE_B6, NOTE_G6, NOTE_B6, NOTE_G6 
};

// note durations: 4 = quarter note, 8 = eighth note, etc.:
const int alertNoteDurations[] = {
  4, 4, 4, 4, 4, 4, 4, 4 
};

const int clearMelody[] = {
  NOTE_C7, NOTE_E7, NOTE_C7, NOTE_C7, NOTE_G7,
};

const int clearNoteDurations[] = {
  8, 16, 16, 8, 8
};

boolean playAlert() {
   return playMelody(alertMelody, alertNoteDurations);
}

boolean playClear() {
   return playMelody(clearMelody, clearNoteDurations); 
}

boolean playMelody(const int* melody, const int* noteDurations) {
  int toneCount = 0;
  // iterate over the notes of the melody:
  for (int thisNote = 0; thisNote < sizeof(melody); thisNote++) {
    // to calculate the note duration, take one second 
    // divided by the note type.
    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 1000 / noteDurations[thisNote];
    if (melody[thisNote] > 0) {
      tone(BUZZER_PIN, melody[thisNote]);
    }  

    boolean cont = waitUntilBreak(noteDuration);
    noTone(BUZZER_PIN);
    if (!cont) {
      break;
    }
    int pauseBetweenNotes = noteDuration * 0.30;
    if (!waitUntilBreak(pauseBetweenNotes)) {
      break; 
    }
    toneCount++;
  }
  return toneCount == sizeof(melody);
}

boolean waitUntilBreak(int duration) {
  int count = duration / 5;
  for (int i = 0; i < count; i++) {
    if (interrupted) {
      return false;
    }
    delay(5);
  }
  return true;
}

