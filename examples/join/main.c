/*******************************************************************************
 * Copyright (c) 2014-2015 IBM Corporation.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors:
 *    IBM Zurich Research Lab - initial API, implementation and documentation
 *******************************************************************************/

#include "lmic.h"
#include "debug.h"

//////////////////////////////////////////////////
// CONFIGURATION (FOR APPLICATION CALLBACKS BELOW)
//////////////////////////////////////////////////

// application router ID (LSBF)
static const u1_t APPEUI[8]  = { 0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x01, 0xFB, 0x6F };

// unique device ID (LSBF)
static const u1_t DEVEUI[8]  = { 0x00, 0xD5, 0x01, 0x5B, 0x30, 0x02, 0xB5, 0x8A };

// device-specific AES key (derived from device EUI)
static const u1_t DEVKEY[16] = { 0xDC, 0x95, 0x68, 0x95, 0x2A, 0x82, 0x5C, 0xB7, 0xC3, 0x31, 0xAA, 0x9E, 0x0C, 0xC6, 0xA8, 0xF1 };


//////////////////////////////////////////////////
// APPLICATION CALLBACKS
//////////////////////////////////////////////////

// provide application router ID (8 bytes, LSBF)
void os_getArtEui (u1_t* buf) {
    memcpy(buf, APPEUI, 8);
}

// provide device ID (8 bytes, LSBF)
void os_getDevEui (u1_t* buf) {
    memcpy(buf, DEVEUI, 8);
}

// provide device key (16 bytes)
void os_getDevKey (u1_t* buf) {
    memcpy(buf, DEVKEY, 16);
}


//////////////////////////////////////////////////
// MAIN - INITIALIZATION AND STARTUP
//////////////////////////////////////////////////

// initial job
static void initfunc (osjob_t* j) {
    // reset MAC state
    LMIC_reset();
    // start joining
    LMIC_startJoining();
    // init done - onEvent() callback will be invoked...
}


// application entry point
int main () {
    osjob_t initjob;

    // initialize runtime env
    os_init();
    // initialize debug library
    debug_init();
    // setup initial job
    os_setCallback(&initjob, initfunc);
    // execute scheduled jobs and events
    os_runloop();
    // (not reached)
    return 0;
}


//////////////////////////////////////////////////
// UTILITY JOB
//////////////////////////////////////////////////

static osjob_t blinkjob;
static u1_t ledstate = 0;

static void blinkfunc (osjob_t* j) {
    // toggle LED
    ledstate = !ledstate;
    debug_led(ledstate);
    // reschedule blink job
    os_setTimedCallback(j, os_getTime()+ms2osticks(100), blinkfunc);
}


//////////////////////////////////////////////////
// LMIC EVENT CALLBACK
//////////////////////////////////////////////////

void onEvent (ev_t ev) {
    debug_event(ev);

    switch(ev) {

      // starting to join network
      case EV_JOINING:
          // start blinking
          blinkfunc(&blinkjob);
          break;
          
      // network joined, session established
      case EV_JOINED:
          // cancel blink job
          os_clearCallback(&blinkjob);
          // switch on LED
          debug_led(1);
          // (don't schedule any new actions)
          break;
    }
}
