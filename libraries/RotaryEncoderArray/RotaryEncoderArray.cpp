/*
	RotaryEncoderStatic.h - Library for using an array of up to 3 rotary encoders.
	Static to allow interrupt usage with the ESP8266 environment.
	based on core method of http://www.buxtronix.net/2011/10/rotary-encoders-done-properly.html
	Robert Tidey 2018 robert@tideys.co.uk
*/

#include "RotaryEncoderArray.h"

#define LATCHSTATE 3

int rotaryDebug=0;
int _interruptEnable = 0;
int _limitMin[MAX_ENCODERS];
int _limitMax[MAX_ENCODERS];
int _pin1[MAX_ENCODERS];
int _pin2[MAX_ENCODERS];
int _button[MAX_ENCODERS];
volatile int _oldState[MAX_ENCODERS];
volatile int _position[MAX_ENCODERS];         // Internal position (4 times _positionExt)
volatile int _positionExt[MAX_ENCODERS];      // External position
volatile int _positionExtPrev[MAX_ENCODERS];  // External position (used only for direction checking)
volatile unsigned long _buttonDownTime[MAX_ENCODERS];
volatile unsigned long _buttonUpTime[MAX_ENCODERS];

// The array holds the values –1 for the entries where a position was decremented,
// a 1 for the entries where the position was incremented
// and 0 in all the other (no change or not valid) cases.

const int KNOBDIR[] = {
	0, -1,  1,  0,
	1,  0,  0, -1,
	-1,  0,  0,  1,
	0,  1, -1,  0
	};


// positions: [3] 1 0 2 [3] 1 0 2 [3]
// [3] is the detent positions where my rotary switch detends
// ==> right, count up
// <== left,  count down


//clear encoders and retain interrupEnable set up
void rotaryEncoderInit(int interruptEnable) {
	int i;
	_interruptEnable = interruptEnable;
	for(i=0; i<MAX_ENCODERS; i++) {
		if(_pin1[i] >= 0) detachInterrupt(_pin1[i]);
		if(_pin2[i] >= 0) detachInterrupt(_pin2[i]);
		_pin1[i] = -1;
		_pin2[i] = -1;
		_button[i] = -1;
		_limitMin[i] = 0;
		_limitMax[i] = 100;
	}
}

// Set up pins for one encoder
void setRotaryEncoderPins(int encoder, int pin1, int pin2, int button) {
	_oldState[encoder] = 3;
	_position[encoder] = 0;
	_positionExt[encoder] = 0;
	_positionExtPrev[encoder] = 0;
	pinMode(pin1, INPUT_PULLUP);
	pinMode(pin2, INPUT_PULLUP);
	if(button >= 0) pinMode(button, INPUT_PULLUP);
	_pin2[encoder] = pin2;
	_pin1[encoder] = pin1;
	_button[encoder] = button;
	_buttonDownTime[encoder]	= 0;
	_buttonUpTime[encoder]	= 0;
	if(_interruptEnable) {
		attachInterrupt(pin1, rotaryTick, CHANGE);
		attachInterrupt(pin2, rotaryTick, CHANGE);
		if(button >=0) attachInterrupt(button, buttonTick, CHANGE);
	}
}

//return Pin1 of an encoder ; -1 if not defined
int getEncoderPin1(int encoder) {
	return _pin1[encoder];
}

//return position of one encoder
int  getRotaryPosition(int encoder) {
	int position = _positionExt[encoder];
	if(position < _limitMin[encoder]) {
		position = _limitMin[encoder];
	} else if(position > _limitMax[encoder]) {
		position = _limitMax[encoder];
	}
	if(position != _positionExt[encoder]) {
		setRotaryPosition(encoder, position);
	}
	return position;
}

//return direction of one encoder
int getRotaryDirection(int encoder) {
    int ret = 0;
    
    if( _positionExtPrev[encoder] > _positionExt[encoder] ) {
		ret = -1;
		_positionExtPrev[encoder] = _positionExt[encoder];
	}
    else if( _positionExtPrev[encoder] < _positionExt[encoder] ) {
		ret = 1;
		_positionExtPrev[encoder] = _positionExt[encoder];
	}
	else {
		ret = 0;
		_positionExtPrev[encoder] = _positionExt[encoder];
	}        
	return ret;
}

//read push button if defined
int getRotaryButton(int encoder) {
	if(_button[encoder] >= 0) {
		return digitalRead(_button[encoder]);
	} else {
		return -1;
	}
}

//read push button pulse
//-1=not defined,0=Nothing,1=short push,2=long push
int getRotaryButtonPulse(int encoder) {
	int ret;
	if(_button[encoder] >= 0) {
		if(digitalRead(_button[encoder]) == 1) {
			if((millis() - _buttonUpTime[encoder]) > BUTTON_TIMEOUT) {
				ret = 0;
			} else {
				if(((_buttonUpTime[encoder] - _buttonDownTime[encoder]) > BUTTON_LONG) && (_buttonDownTime[encoder] > 0)) {
					ret = 2;
				} else if(((_buttonUpTime[encoder] - _buttonDownTime[encoder]) > BUTTON_SHORT) && (_buttonDownTime[encoder] > 0)) {
					ret = 1;
				} else{
					ret = 0;
				}
			}
			_buttonDownTime[encoder] = _buttonUpTime[encoder];
		} else {
			ret = 0;
		}
	} else {
		ret = -1;
	}
	return ret;
}

//set position of one encoder
void setRotaryPosition(int encoder, int newPosition) {
	// only adjust the external part of the position.
	_position[encoder] = ((newPosition<<2) | (_position[encoder] & 0x03L));
	_positionExt[encoder] = newPosition;
	_positionExtPrev[encoder] = newPosition;
}

// set limits for encoder positions
void setRotaryLimits(int encoder, int limitMin, int limitMax) {
	_limitMin[encoder] = limitMin;
	_limitMax[encoder] = limitMax;
}

//state machine for all encoders;
void ICACHE_RAM_ATTR rotaryTick(void) {
	int i, sig1, sig2, thisState;
	for(i = 0; i< MAX_ENCODERS; i++) {
		if(_pin1[i] >= 0) {
			sig1 = digitalRead(_pin1[i]);
			sig2 = digitalRead(_pin2[i]);
			thisState = sig1 | (sig2 << 1);
			if (_oldState[i] != thisState) {
				_position[i] += KNOBDIR[thisState | (_oldState[i]<<2)];
				if (thisState == LATCHSTATE) {
					_positionExt[i] = _position[i] >> 2;
				}
				_oldState[i] = thisState;
			}
		}
	}
}

/*
  Button Push interrupt handler
*/
void ICACHE_RAM_ATTR buttonTick(void) {
	int i;
	unsigned long m = millis();
	for(i=0; i< MAX_ENCODERS; i++) {
		if(_button[i] >= 0) {
			if(digitalRead(_button[i]) == 1) {
				if((m - _buttonDownTime[i]) > BUTTON_DEBOUNCE)
					_buttonUpTime[i] = m;
			} else {
				if((m - _buttonUpTime[i]) > BUTTON_DEBOUNCE)
					_buttonDownTime[i] = m;
			}
		}
	}
}

int getRotaryDebug() {
	return rotaryDebug;
}