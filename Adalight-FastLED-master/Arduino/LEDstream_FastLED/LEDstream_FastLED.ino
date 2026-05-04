/*
 *  Project     Adalight FastLED
 *  @author     David Madison
 *  @link       github.com/dmadison/Adalight-FastLED
 *  @license    LGPL - Copyright (c) 2017 David Madison
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <Arduino.h>

const uint16_t 
	Num_Leds   =  60;        
const uint8_t
	Brightness =  255;        


#define LED_TYPE     WS2812B
#define COLOR_ORDER  GRB
#define PIN_DATA     6

const unsigned long
	SerialSpeed    = 115200;
const uint16_t
	SerialTimeout  = 60;

#define SERIAL_FLUSH
#include <FastLED.h>
CRGB leds[Num_Leds];
uint8_t * ledsRaw = (uint8_t *)leds;

const uint8_t magic[] = {
	'A','d','a'};
#define MAGICSIZE  sizeof(magic)

#define HICHECK    (MAGICSIZE)
#define LOCHECK    (MAGICSIZE + 1)
#define CHECKSUM   (MAGICSIZE + 2)

enum processModes_t {Header, Data} mode = Header;

int16_t c;
uint16_t outPos;
uint32_t bytesRemaining;
unsigned long t, lastByteTime, lastAckTime;

void headerMode();
void dataMode();
void timeouts();

#ifdef SERIAL_FLUSH
	#undef SERIAL_FLUSH
	#define SERIAL_FLUSH while(Serial.available() > 0) { Serial.read(); }
#else
	#define SERIAL_FLUSH
#endif

#ifdef DEBUG_LED
	#define ON  1
	#define OFF 0

	#define D_LED(x) do {digitalWrite(DEBUG_LED, x);} while(0)
#else
	#define D_LED(x)
#endif

#ifdef DEBUG_FPS
	#define D_FPS do {digitalWrite(DEBUG_FPS, HIGH); digitalWrite(DEBUG_FPS, LOW);} while (0)
#else
	#define D_FPS
#endif

void setup(){
	#ifdef DEBUG_LED
		pinMode(DEBUG_LED, OUTPUT);
		digitalWrite(DEBUG_LED, LOW);
	#endif

	#ifdef DEBUG_FPS
		pinMode(DEBUG_FPS, OUTPUT);
	#endif

	#if defined(PIN_CLOCK) && defined(PIN_DATA)
		FastLED.addLeds<LED_TYPE, PIN_DATA, PIN_CLOCK, COLOR_ORDER>(leds, Num_Leds);
	#elif defined(PIN_DATA)
		FastLED.addLeds<LED_TYPE, PIN_DATA, COLOR_ORDER>(leds, Num_Leds);
	#else
		#error "No LED output pins defined. Check your settings at the top."
	#endif
	
	FastLED.setBrightness(Brightness);

	#ifdef CLEAR_ON_START
		FastLED.show();
	#endif

	Serial.begin(SerialSpeed);
	Serial.print("Ada\n");

	lastByteTime = lastAckTime = millis();
}

void loop(){ 
	t = millis();


	if((c = Serial.read()) >= 0){
		lastByteTime = lastAckTime = t;

		switch(mode) {
			case Header:
				headerMode();
				break;
			case Data:
				dataMode();
				break;
		}
	}
	else {
	
		timeouts();
	}
}

void headerMode(){
	static uint8_t
		headPos,
		hi, lo, chk;

	if(headPos < MAGICSIZE){
		
		if(c == magic[headPos]) {headPos++;}
		else {headPos = 0;}
	}
	else{
		
		switch(headPos){
			case HICHECK:
				hi = c;
				headPos++;
				break;
			case LOCHECK:
				lo = c;
				headPos++;
				break;
			case CHECKSUM:
				chk = c;
				if(chk == (hi ^ lo ^ 0x55)) {
					
					D_LED(ON);
					bytesRemaining = 3L * (256L * (long)hi + (long)lo + 1L);
					outPos = 0;
					memset(leds, 0, Num_Leds * sizeof(struct CRGB));
					mode = Data;
				}
				headPos = 0;
				break;
		}
	}
}

void dataMode(){

	if (outPos < sizeof(leds)){
		ledsRaw[outPos++] = c;
	}
	bytesRemaining--;
 
	if(bytesRemaining == 0) {
		mode = Header;
		FastLED.show();
		D_FPS;
		D_LED(OFF);
		SERIAL_FLUSH;
	}
}

void timeouts(){
	if((t - lastAckTime) >= 1000) {
		Serial.print("Ada\n");
		lastAckTime = t;

		
		if(SerialTimeout != 0 && (t - lastByteTime) >= (uint32_t) SerialTimeout * 1000) {
			memset(leds, 0, Num_Leds * sizeof(struct CRGB));
			FastLED.show();
			mode = Header;
			lastByteTime = t;
		}
	}
}
