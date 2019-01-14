/*
--------------------------------------------------------------------------------
  Author       : Gerard Gauthier
  Description  : Fun test on a sine generator of the C diatonic scale
--------------------------------------------------------------------------------
*/

/*
 fuses
  extended: 0xff
  high: 0x89
  low: 0xe4
*/


/*-----------------------------------------
 *      Include declaration
 *-----------------------------------------
 */
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/sfr_defs.h>
#include <avr/wdt.h>

#include "typedef.h"

/*
 *-----------------------------------------
 *      Constant definition
 *-----------------------------------------
 */

 /* number of PWM samples for every sine period */
 #define SINE_WAVE_SAMPLES 20
 /* number of timre0 cycles before input keys are sampled again */
 #define BOUNCE_COUNT_MATURED 5


/*
 * defines top delay for timer 1 which drives PWM for speaker and 20 intermediate delay for synchronous switching of both OC1A and OC1B
 */
 typedef struct {
  word baseDelay;
  word dutyCycles[SINE_WAVE_SAMPLES];
 } sNoteDef;

/*
//  0: C
//  1: D
//  2: E
//  3: F
//  4: G
//  5: A
//  6: B
*/
const sNoteDef noteDef[] PROGMEM = {
  {764, {382, 489, 584, 661, 709, 726, 709, 661, 584, 489, 382, 276, 180, 104, 55, 38, 55, 104, 180, 276}},
  {681, {341, 435, 521, 588, 632, 647, 632, 588, 521, 435, 341, 246, 160, 93, 49, 34, 49, 93, 160, 246}},
  {607, {303, 388, 464, 524, 563, 576, 563, 524, 464, 388, 303, 219, 143, 82, 44, 30, 44, 82, 143, 219}},
  {573, {286, 366, 438, 495, 531, 544, 531, 495, 438, 366, 286, 207, 135, 78, 41, 29, 41, 78, 135, 207}},
  {510, {255, 326, 390, 441, 473, 485, 473, 441, 390, 326, 255, 184, 120, 69, 37, 26, 37, 69, 120, 184}},
  {455, {227, 290, 348, 393, 422, 432, 422, 393, 348, 290, 227, 164, 107, 62, 33, 23, 33, 62, 107, 164}},
  {405, {202, 259, 310, 350, 376, 385, 376, 350, 310, 259, 202, 146, 95, 55, 29, 20, 29, 55, 95, 146}}};


/* number of cycles of timer0 before LED is evaluated, function of note: none, C, D, E... */
const word ledDelays[] PROGMEM = {977, 244, 194, 154, 122, 109, 86, 69, 61, 48, 38, 31, 27, 22, 17};

#define NB_NOTES (sizeof(noteDef)/sizeof(*noteDef))

 /*
  * Storage in EEPROM
  */
EE8Addr eeOscillatorCalAddr = ((EE8Addr)0x7f); /* oscillator calibration */

volatile boolean gotInterrupt0 = TRUE;
volatile boolean gotInterrupt1 = TRUE;
volatile word newDc;  /* new value to use for OC1A and OC1B (duty-cycle) */
volatile word newTop; /* new value to use for ICR1 (period of sample) */

byte note = 255; /* note: index between 0 to NB_NOTES-1, provided note is valid */
boolean secondOctave = FALSE;
/* temporary variables for debouncing input keys */
byte unfilteredNote = 255;
boolean unfilteredSecondOctave = FALSE;

/*------------------------------------------------------------------------------
       test inputs to define note and shift with bouncing rejection
------------------------------------------------------------------------------*/
boolean testNote() 
{
  byte newNote = 255;
  boolean newSecondOctave = FALSE;
  boolean changed = FALSE;
  static boolean initialChange = TRUE;
  if(initialChange) {
   changed = TRUE;
   initialChange = FALSE;
  }
  /* key pushed is read a a zero */
  if((PINA & (1<<PINA1)) == 0) {
    newNote=6;
  }
  if((PINA & (1<<PINA0)) == 0) {
    newNote=5;
  }
  if((PIND & (1<<PIND2)) == 0) {
    newNote=4;
  }
  if((PIND & (1<<PIND3)) == 0) {
    newNote=3;
  }
  if((PIND & (1<<PIND4)) == 0) {
    newNote=2;
  }
  if((PIND & (1<<PIND5)) == 0) {
    newNote=1;
  }
  if((PIND & (1<<PIND6)) == 0) {
    newNote=0;
  }
  if((PINB & (1<<PINB0)) == 0) {
    newSecondOctave=TRUE;
  }
  if(newSecondOctave == unfilteredSecondOctave) {
    secondOctave = newSecondOctave;
    changed = TRUE;
  }
  if(newNote == unfilteredNote) {
    note = newNote;
    changed = TRUE;
  }
  unfilteredNote = newNote;
  unfilteredSecondOctave = newSecondOctave;
  return changed;
}

__attribute__((OS_main)) int main(void)
{
    byte tmpScratch;
    byte dcIndex = 0; /* goes from 0 to SINE_WAVE_SAMP-1 */
    word ledDelay = 100; /* number of times timer0 cycles before LED state is evaluated again */
    word ledCounter = 0; /* internal counter going from 0 to ledDelay - 1 */
    byte ledStep = 0; /* counter increased each time LED delay is evaluated again */
    byte bounceCount = BOUNCE_COUNT_MATURED - 1; /* number of times timer0 cycles before keys are sensed */

    tmpScratch=eeprom_read_byte(eeOscillatorCalAddr);
    if(tmpScratch!=0xff) {
      OSCCAL = tmpScratch;
    }
    /***************** WATCHDOG CONFIGURATION *******************/
    wdt_reset();
    WDTCSR |= (1 << WDCE) | (1 << WDE);               /* Start timed sequence */
    WDTCSR = (1 << WDE) | (1 << WDP2) | (1 << WDP0);  /* Set new prescaler (time-out) value = 64K cycles (~0.5 s) */

    /*------------------ PORTS CONFIGURATION --------------------*/
    DDRA  = 0x00;               // PA0=A, PA1=B, PA2=Reset
    PORTA = 0x00;               // no Pull-ups
    
    DDRB  = 0x1e;               // PB0=second octave, PB1=LED, PB2=LED, PB3=speaker-out, PB4=/speaker-out, PB5=SDA, PB6=MISO, PB7=SCK
    PORTB = 0x00;               // no Pull-ups

    DDRD  = 0x00;               // PD1=not used, PD2=G, PD3=F, PD4=E, PD5=D, PD6=C
    PORTD = 0x03;               // some pull-up's
    /*------------------------------------------------------------*/
    
  /*-- Timer 0: PWM = 8MHz / 64 (presc.) / 256 ~ 488Hz ~ 2ms--*/
  OCR0A  = 0;
  TCCR0A = (1 << WGM01) | (1 << WGM00);     /* T0_Fast_PWM | T0_Normal_PWM */
  TCCR0B = (1 << CS01) | (1 << CS00);     /* Prescal = CLK / 64 */
  /*-- Timer 1: 8MHz -- fast PWM mode with ICR1 as top, pre-scaling at 1, set outputs OC1A/OC1B high/low on capture match */
  TCCR1A = (1<<COM1A1) | (0<<COM1A0) | (1<<COM1B1) | (1<<COM1B0) | (1 << WGM11) | (0 << WGM10);        /* Fast */
  TCCR1B = (1 << WGM13) | (1 << WGM12) | (0 << CS12) | (0 << CS11) | (1 << CS10);
  TIMSK  = 0x82;     /* Timer 0 and 1 Overflow Interrupt Enable */
  ICR1 = pgm_read_word(&noteDef[0].baseDelay);
  OCR1A = OCR1B = 0xffff;
  PORTB = 0x06;
  __enable_interrupt();


  while (TRUE) {
    wdt_reset();
    if(note<NB_NOTES) {
      DDRB = 0x1e; // PB1~PB4 as outputs
    } else  {
      DDRB = 0x06; // PB1~P2 as outputs (so speaker-out are, as inputs, left floating: no sound)
    }
    /* manages speaker */
    if(gotInterrupt1) {
      gotInterrupt1 = FALSE;
      /* calculation of timer1 top and switch value for duty cycle */
      newTop = pgm_read_word(&noteDef[note>=NB_NOTES?0:note].baseDelay);
      newDc = pgm_read_word(noteDef[note>=NB_NOTES?0:note].dutyCycles+dcIndex);
      /* second octave: number of used duty cycle stages is divided by two (skip odd indexes) */
      dcIndex+=secondOctave?2:1;
      if(dcIndex>=SINE_WAVE_SAMPLES) {
        dcIndex = 0;
      }
    }
    /* manages input keys and LED cycles */
    if(gotInterrupt0) {
      gotInterrupt0 = FALSE;
      bounceCount++;
      if(bounceCount >= BOUNCE_COUNT_MATURED) {
        bounceCount = 0;
        if(testNote()) { /* if key changed, re-evaluates LED cycle */
          if(note>=NB_NOTES) {
            ledDelay = pgm_read_word(ledDelays);
          } else {
            ledDelay = pgm_read_word(ledDelays+note+1+secondOctave*7);
          }
        }
      }
      ledCounter++;
      if(ledCounter>=ledDelay) {
        ledCounter = 0;
        PORTB = (ledStep<1) ? 0x00 : 0x06; /* manages PB1 and PB2 */
        ledStep = (ledStep+1) & 1;
      }
    }
  }
}

/*------------------------------------------------------------------------------
   irqHandler for TIMER0
------------------------------------------------------------------------------*/
ISR(TIMER0_OVF_vect)
{
  gotInterrupt0 = TRUE;
}

/*------------------------------------------------------------------------------
   irqHandler for TIMER1
------------------------------------------------------------------------------*/
ISR(TIMER1_OVF_vect)
{
  OCR1A = newDc;
  OCR1B = newDc;
  ICR1 = newTop;
  gotInterrupt1 = TRUE;
}

