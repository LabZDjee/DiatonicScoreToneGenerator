/*
--------------------------------------------------------------------------------
  Author       : Gerard Gauthier
  Description  : Common definitions
--------------------------------------------------------------------------------
*/

#ifndef TYPEDEF_H
#define TYPEDEF_H


typedef unsigned char   byte;
typedef unsigned char   boolean;
typedef unsigned char   uchar;
typedef unsigned short  ushort;
typedef unsigned int    uint;
typedef unsigned long   ulong;
typedef unsigned long   dword;
typedef unsigned short  word;

typedef byte* EE8Addr;

#define FALSE               0
#define TRUE                (!FALSE)

#define TICK_DIVIDER        (((CRYSTAL_FREQ/TIMER_PRESCALER)/255UL)/TICK_FREQUENCY)

/*
 *------------------------------------------------------
 *     Definition for timing management
 *------------------------------------------------------
 */
#define CRYSTAL_FREQ       (8000000UL)
#define TIMER_PRESCALER    (1UL)

#define __enable_interrupt   sei
#define __disable_interrupt  cli

#endif
