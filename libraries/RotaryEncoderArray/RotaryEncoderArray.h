/*
	RotaryEncoderStatic.h - Library for using an array of up to 3 rotary encoders.
	Static to allow interrupt usage with the ESP8266 environment.
	based on core method of http://www.buxtronix.net/2011/10/rotary-encoders-done-properly.html
	Robert Tidey 2018 robert@tideys.co.uk
*/
#ifndef RotaryEncoderArray_h
#define RotaryEncoderArray_h
#include "Arduino.h"
#define MAX_ENCODERS 3

//button press times in milliseconds
#define BUTTON_TIMEOUT 5000
#define BUTTON_LONG 1500
#define BUTTON_SHORT 50
#define BUTTON_DEBOUNCE 20

extern void rotaryEncoderInit(int interruptEnable);
extern void setRotaryEncoderPins(int encoder, int pin1,int pin2, int button);
extern int getRotaryPosition(int encoder); //0 = None, 1 = CW, -1 = CCW
int getRotaryDirection(int encoder);
int getRotaryButton(int encoder);
int getRotaryButtonPulse(int encoder);
int getEncoderPin1(int encoder);
int getRotaryDebug();
void setRotaryPosition(int encoder, int newPosition);
void setRotaryLimits(int encoder, int limitMin, int limitMax);
void  ICACHE_RAM_ATTR rotaryTick(void); //statemachine for encoders; polled or interrupt driven.
void  ICACHE_RAM_ATTR buttonTick(void); //handler for button presses
#endif
