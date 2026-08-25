#include <stdint.h>

#ifndef BLINK__H
#define BLINK__H

#define BLINK_RED    2
#define BLINK_GREEN  0
#define BLINK_BLUE   1

void blinkHandler();
void blink(uint8_t color, uint8_t count);
void blinkAlternate(uint8_t count);
void blinkAll(uint8_t count);
void blinkSetFreq();

#endif
