/*
 * -------------------------------------------------------------------
 * Voltage Systems Regulator
 * (C) 2024 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/VSR
 * https://vsr.out-a-ti.me
 *
 * Main
 *
 * -------------------------------------------------------------------
 * License: MIT
 * 
 * Permission is hereby granted, free of charge, to any person 
 * obtaining a copy of this software and associated documentation 
 * files (the "Software"), to deal in the Software without restriction, 
 * including without limitation the rights to use, copy, modify, 
 * merge, publish, distribute, sublicense, and/or sell copies of the 
 * Software, and to permit persons to whom the Software is furnished to 
 * do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be 
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _VSR_MAIN_H
#define _VSR_MAIN_H

// LED display modes
enum {
   LDM_WHEELS = 0,    // display pushwheel values
   LDM_GPS,           // display GPS/RotEnc speed
   LDM_TEMP,          // display temperature
   LDM_BM,            // display button mode  (not for userDispMode)
   LDM_SYS            // display system message (with time-out)
};
extern int userDispMode;

// Durations of tt phases for internal tt
#define P0_DUR          5000    // acceleration phase
#define P1_DUR          5000    // time tunnel phase
#define P2_DUR          3000    // re-entry phase

extern unsigned long powerupMillis;

extern vsrDisplay vsrdisplay;
extern vsrBLEDs vsrLEDs;

extern bool signalBM;

extern bool haveTCDTemp;
extern bool haveTempSens;

extern bool showBM;

extern bool TCDconnected;

extern bool FPBUnitIsOn;
extern bool vsrNM;

extern bool TTrunning;

extern bool bttfnTT;

extern bool networkTimeTravel;
extern bool networkTCDTT;
extern bool networkReentry;
extern bool networkAbort;
extern bool networkAlarm;
extern uint16_t networkLead;

extern bool ssActive;
 
void main_boot();
void main_boot2();
void main_setup();
void main_loop();

void flushDelayedSave();
#ifdef VSR_HAVEAUDIO
void increaseVolume();
void decreaseVolume();
#endif

void timeTravel(bool TCDtriggered, uint16_t P0Dur = P0_DUR);

void displaySysMsg(const char *msg, unsigned long timeout);

void toggleNightMode();

void ssEnd();
void ssRestartTimer();

void showWaitSequence();
void endWaitSequence();
void showCopyError();

void prepareTT();
void wakeup();

void display_ip();

void myCustomDelay_KP(unsigned long mydel);
void mydelay(unsigned long mydel);

#ifdef VSR_HAVEAUDIO
void waitAudioDone();
#endif

void bttfn_loop();
bool BTTFNTriggerTT();

#endif
