#include "pitches.h"

// notes in the melody:
const int melody[] = {
  960, 770, 960, 770, 960, 770, 960, 770 };

// note durations: 4 = quarter note, 8 = eighth note, etc.:
const int noteDurations[] = {
  4, 4, 4, 4, 4, 4, 4, 4 };

boolean playTone() {
  int toneCount = 0;
  // iterate over the notes of the melody:
  for (int thisNote = 0; thisNote < sizeof(melody); thisNote++) {
    // to calculate the note duration, take one second 
    // divided by the note type.
    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 1000/noteDurations[thisNote];
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
