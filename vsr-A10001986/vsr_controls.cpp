/*
 * -------------------------------------------------------------------
 * Voltage Systems Regulator
 * (C) 2024-2025 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/VSR
 * https://vsr.out-a-ti.me
 *
 * Input controls handling
 *
 * -------------------------------------------------------------------
 * License: MIT NON-AI
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
 * In addition, the following restrictions apply:
 * 
 * 1. The Software and any modifications made to it may not be used 
 * for the purpose of training or improving machine learning algorithms, 
 * including but not limited to artificial intelligence, natural 
 * language processing, or data mining. This condition applies to any 
 * derivatives, modifications, or updates based on the Software code. 
 * Any usage of the Software in an AI-training dataset is considered a 
 * breach of this License.
 *
 * 2. The Software may not be included in any dataset used for 
 * training or improving machine learning algorithms, including but 
 * not limited to artificial intelligence, natural language processing, 
 * or data mining.
 *
 * 3. Any person or organization found to be in violation of these 
 * restrictions will be subject to legal action and may be held liable 
 * for any damages resulting from such use.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#include "vsr_global.h"

#include <Arduino.h>

#include "vsrdisplay.h"
#include "input.h"
#include "vsr_audio.h"
#include "vsr_main.h"
#include "vsr_controls.h"
#include "vsr_settings.h"
#include "vsr_wifi.h"

#define MCP23017_ADDR    0x21    // I2C address of the MCP23017 port expander

#define BUTTON_DEBOUNCE    50    // button debounce time in ms
#define BUTTON_PRESS_TIME 200    // button will register a short press
#define BUTTON_HOLD_TIME 2000    // time in ms holding the button will count as a long press

// The Pushwheel/Button object
Pushwheel_I2C vsrControls(1,
            (uint8_t[1*2]){ MCP23017_ADDR, PW_MCP23017,
                          });

static unsigned long timeNow = 0;

static unsigned long lastKeyPressed = 0;

enum {
    VSRB_IDLE = 0,
    VSRB_PRESSED,
    VSRB_HELD
};
static uint8_t butState[3]      = { VSRB_IDLE, VSRB_IDLE, VSRB_IDLE };
static bool    ignkey[3]        = { false, false, false };
static const int otherKey[3][3] = { { 1, 2 }, { 0, 2 }, { 0, 1 } };

uint8_t        buttonMode = VBM_LIGHT;
static uint8_t buttonModeChg = 0;
static const char *bmodesText[NUM_BM+1] = {
    "LGT",
    "OPR",
    "MUS",
    "ADM",
    "---"
};

static unsigned long enterDelay = 0;

static void controlsEvent(int idx, ButState bstate);

/*
 * controls_setup()
 */
void controls_setup()
{
    signalBM = (atoi(settings.signalBM) > 0);
    
    // Set up the pushwheels and buttons
    vsrControls.begin(50, BUTTON_HOLD_TIME, myCustomDelay_KP);

    vsrControls.addEventListener(controlsEvent);
}

/*
 * scanControls(): Scan pushwheels and buttons
 */
bool scanControls()
{
    return vsrControls.scanControls();
}

/*
 *  The controls event handler: Only called for Buttons, not PW changes
 */
static void controlsEvent(int idx, ButState bstate)
{       
    if(TTrunning) 
        return;
        
    ssRestartTimer();

    switch(bstate) {
    case VSRBUS_PRESSED:
        switch(butState[idx]) {
        case VSRB_IDLE:
            // First button press (of any button) when ss is active 
            // only de-activates the ss
            if(ssActive) {
                ignkey[idx] = true;
                ssEnd();
            } else {
                ignkey[idx] = false;
                // Key press-down action
                // Only immediate reactions, we don't know yet if
                // user (short-)pressed or longpressed the button
                if(signalBM && buttonMode != VBM_LIGHT) {
                    vsrLEDs.setStates(vsrLEDs.getStates() ^ (1 << idx));
                } else {
                    vsrLEDs.setStates(vsrLEDs.getStates() | (1 << idx));
                }
                if(butState[otherKey[idx][0]] == VSRB_IDLE && butState[otherKey[idx][1]] == VSRB_IDLE) {
                    // Ugly hack to avoid button sound for volume adjustment; in that
                    // case a sound is played AFTER changing the volume level
                    if(buttonMode != VBM_ADMIN || curSoftVol == 255 || idx == 2) {
                        play_button_sound();
                    }
                }
            }
            butState[idx] = VSRB_PRESSED;
            break;
        default:
            #ifdef VSR_DBG
            Serial.printf("PRESSED: Bad key state idx %d bstate %d ButState %d\n", idx, butState[idx], bstate);
            #endif
            break;
        }
        break;

    case VSRBUS_RELEASED:
        switch(butState[idx]) {
        case VSRB_PRESSED:
            if(!ignkey[idx]) {
                if(signalBM && buttonMode != VBM_LIGHT) {
                    vsrLEDs.setStates(vsrLEDs.getStates() ^ (1 << idx));
                } else {
                    vsrLEDs.setStates(vsrLEDs.getStates() & (~(1 << idx)));
                }
                // Key (short)press-up action
                // So we now know that the user only short-pressed the button
                switch(buttonMode) {
                case VBM_LIGHT:
                    // Nothing, light just goes off (as done above)
                    break;
                case VBM_OP:
                    switch(idx) {
                    case 0:
                        if(!bttfnTT || !bttfn_trigger_tt()) {
                            timeTravel(false);
                        }
                        break;
                    case 1:
                        #ifdef VSR_HAVEAUDIO
                        play_key(3);
                        #endif
                        break;
                    case 2:
                        #ifdef VSR_HAVEAUDIO
                        play_key(6);
                        #endif
                        break;
                    }
                    break;
                case VBM_MP:
                    switch(idx) {
                    case 0:
                        #ifdef VSR_HAVEAUDIO
                        if(haveMusic) {
                            mp_prev(mpActive);
                        } else 
                            play_button_bad();
                        #endif
                        break;
                    case 1:
                        #ifdef VSR_HAVEAUDIO
                        if(haveMusic) {
                            if(mpActive) {
                                mp_stop();
                            } else {
                                mp_play();
                            }
                        } else 
                            play_button_bad();
                        #endif
                        break;
                    case 2:
                        #ifdef VSR_HAVEAUDIO
                        if(haveMusic) {
                            mp_next(mpActive);
                        } else
                            play_button_bad();
                        #endif
                        break;
                    }
                    break;
                case VBM_ADMIN:
                    switch(idx) {
                    case 0:
                        #ifdef VSR_HAVEAUDIO
                        increaseVolume();
                        if(!mpActive) {
                            play_volchg_sound();
                        }
                        #endif
                        break;
                    case 1:
                        #ifdef VSR_HAVEAUDIO
                        decreaseVolume();
                        if(!mpActive) {
                            play_volchg_sound();
                        }
                        #endif
                        break;
                    case 2:
                        // TODO
                        break;
                    }
                    break;
                }
            }
            butState[idx] = VSRB_IDLE;
            break;
        default:
            #ifdef VSR_DBG
            Serial.printf("RELEASED: Bad key state idx %d bstate %d ButState %d\n", idx, butState[idx], bstate);
            #endif
            break;
        }
        
        break;

    case VSRBUS_HOLD:
        switch(butState[idx]) {
        case VSRB_PRESSED:
            butState[idx] = VSRB_HELD;
            if(!ignkey[idx]) {
                // Button-held-start action
                // Only check for buttonmode-changes here
                // "Normal" hold action is now in Hold-end
                int idx1 = otherKey[idx][0], idx2 = otherKey[idx][1];
                if(butState[idx1] == VSRB_HELD && butState[idx2] == VSRB_HELD) {
                    buttonMode = VBM_ADMIN;
                    buttonModeChg = 7;
                    if(signalBM) {
                        vsrLEDs.setStates(buttonMode + 4);
                    }
                } else if(butState[idx1] == VSRB_HELD) {    // 0+1-1=0, 0+2-1=1; 1+2-1=2
                    buttonMode = idx + idx1 - 1;
                    buttonModeChg = (1 << idx) | (1 << idx1);
                    if(signalBM) {
                        vsrLEDs.setStates(buttonMode + 4);
                    } else {
                        vsrLEDs.setStates(vsrLEDs.getStates() & (~(1 << idx2)));
                    }
                } else if(butState[idx2] == VSRB_HELD) {
                    buttonMode = idx + idx2 - 1;
                    buttonModeChg = (1 << idx) | (1 << idx2);
                    if(signalBM) {
                        vsrLEDs.setStates(buttonMode + 4);
                    } else {
                        vsrLEDs.setStates(vsrLEDs.getStates() & (~(1 << idx1)));
                    }
                } else if(butState[idx1] == VSRB_IDLE && butState[idx2] == VSRB_IDLE) {
                    play_buttonl_sound();
                }
                showBM = !!buttonModeChg;
            }
            break;
        default:
            #ifdef VSR_DBG
            Serial.printf("HOLD: Bad key state idx %d bstate %d ButState %d\n", idx, butState[idx], bstate);
            #endif
            break;
        }
        
        break;

    case VSRBUS_HOLDEND:
        switch(butState[idx]) {
        case VSRB_HELD:
            if(!buttonModeChg) {
                showBM = false;
                // (Normal) Hold-End action: 
                switch(buttonMode) {
                case VBM_LIGHT:     // LGT button mode: Do nothing, leave light on
                    break;
                case VBM_OP:        // OP button mode: Select display mode
                    if(signalBM) {
                        vsrLEDs.setStates(buttonMode + 4);
                    } else {
                        vsrLEDs.setStates(vsrLEDs.getStates() & (~(1 << idx)));
                    }
                    switch(idx) {
                    case 0:
                        userDispMode = LDM_WHEELS;
                        break;
                    case 1:
                        userDispMode = LDM_TEMP;
                        break;
                    case 2:
                        userDispMode = LDM_GPS;
                        break;
                    }
                    udispchanged = true;
                    udispchgnow = millis();
                    break;
                case VBM_MP:        // MP mode: Shuffle off/goto 0/shuffle on
                    if(signalBM) {
                        vsrLEDs.setStates(buttonMode + 4);
                    } else {
                        vsrLEDs.setStates(vsrLEDs.getStates() & (~(1 << idx)));
                    }
                    switch(idx) {
                    case 0:
                        #ifdef VSR_HAVEAUDIO
                        if(haveMusic) {
                            mp_makeShuffle(false);
                            displaySysMsg("ORD", 1000);
                        } else 
                            play_button_bad();
                        #endif
                        break;
                    case 1:
                        #ifdef VSR_HAVEAUDIO
                        if(haveMusic) {
                            mp_makeShuffle(true);
                            displaySysMsg("SHU", 1000);
                        } else 
                            play_button_bad();
                        #endif
                        break;
                    case 2:
                        #ifdef VSR_HAVEAUDIO
                        if(haveMusic) {
                            mp_gotonum(0, mpActive);
                        } else 
                            play_button_bad();
                        #endif
                        break;
                    }
                    break;
                case VBM_ADMIN:     // Admin mode: Display IP / - / Delete IP config+Clear AP pw
                    if(signalBM) {
                        vsrLEDs.setStates(buttonMode + 4);
                    } else {
                        vsrLEDs.setStates(vsrLEDs.getStates() & (~(1 << idx)));
                    }
                    switch(idx) {
                    case 0:
                        display_ip();
                        ssRestartTimer();
                        break;
                    case 1:
                        
                        break;
                    case 2:
                        Serial.println("Deleting ip config; clearing AP mode WiFi password\n");
                        #ifdef VSR_HAVEAUDIO
                        waitAudioDone();
                        #endif
                        flushDelayedSave();
                        deleteIpSettings();
                        if(settings.appw[0]) {
                            settings.appw[0] = 0;
                            write_settings();
                        }
                        ssRestartTimer();
                        displaySysMsg("RST", 1000);
                        break;
                    }
                    break;
                }
            } else {
                if(signalBM && buttonMode == VBM_LIGHT) {
                    vsrLEDs.setStates(0);
                } else if(!signalBM) {
                    if(buttonModeChg & (1 << idx)) {
                        vsrLEDs.setStates(vsrLEDs.getStates() & (~(1 << idx)));
                    }
                }
                buttonModeChg &= ~(1 << idx);
                if(!buttonModeChg) showBM = false;
            }
            butState[idx] = VSRB_IDLE;
            break;
        default:
            #ifdef VSR_DBG
            Serial.printf("HOLDEND: Bad key state idx %d bstate %d ButState %d\n", idx, butState[idx], bstate);
            #endif
            break;
        }
        
        break;
    }
}

void resetBLEDandBState()
{
    for(int i = 0; i < 3; i++) {
        butState[i] = VSRB_IDLE;
    }
    if(signalBM && buttonMode != VBM_LIGHT) {
        vsrLEDs.setStates(buttonMode + 4);
    } else {
        if(vsrLEDs.getStates()) {
            vsrLEDs.setStates(0);
        }
    }
}

const char *getBMString()
{
    int i = buttonMode;
    if(i < 0 || i >= NUM_BM) i = VBM_UNKNOWN;
    return bmodesText[i];
}
