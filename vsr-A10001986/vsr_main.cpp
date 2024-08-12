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
#include <WiFi.h>
#include "vsrdisplay.h"
#include "input.h"
#ifdef VSR_HAVETEMP
#include "sensors.h"
#endif

#include "vsr_main.h"
#include "vsr_controls.h"
#include "vsr_settings.h"
#include "vsr_audio.h"
#include "vsr_wifi.h"

// i2c slave addresses

#define MCP23017_ADDR  0x21 // MCP23017 16 bit port expander for pushwheels and buttons (used in controls.cpp)

#define DISPLAY_ADDR   0x70 // LED segment display
#define PCF8574_ADDR   0x22 // PCF8574 for Button LEDs

                            // temperature sensors
#define MCP9808_ADDR   0x18 // [default]
#define BMx280_ADDR    0x77 // [default]
#define SHT40_ADDR     0x44 // Only SHT4x-Axxx, not SHT4x-Bxxx
#define SI7021_ADDR    0x40 //
#define TMP117_ADDR    0x49 // [non-default]
#define AHT20_ADDR     0x38 //
#define HTU31_ADDR     0x41 // [non-default]
#define MS8607_ADDR    0x76 // +0x40

unsigned long powerupMillis = 0;

// The VSR LED segment display object
vsrDisplay vsrdisplay = vsrDisplay(DISPLAY_ADDR);

// The VSR Button-LEDs object
vsrBLEDs vsrLEDs(1, 
            (uint8_t[1*2]){ PCF8574_ADDR, BLHW_PCF8574,
                          });

// The tt button / TCD tt trigger object
static VSRButton TTKey = VSRButton();

#ifdef VSR_HAVETEMP
tempSensor tempSens(8, 
            (uint8_t[8*2]){ MCP9808_ADDR, MCP9808,
                            BMx280_ADDR,  BMx280,
                            SHT40_ADDR,   SHT40,
                            MS8607_ADDR,  MS8607,   // before Si7021
                            SI7021_ADDR,  SI7021,
                            TMP117_ADDR,  TMP117,
                            AHT20_ADDR,   AHT20,
                            HTU31_ADDR,   HTU31
                          });
#endif

#define TT_DEBOUNCE    50    // tt button debounce time in ms
#define TT_HOLD_TIME 5000    // time in ms holding the tt button will count as a long press
static bool isTTKeyPressed = false;
static bool isTTKeyHeld = false;

int                  userDispMode = LDM_WHEELS;
static int           dispMode = LDM_WHEELS;
static int           lastDispMode = -1;
static bool          displayBM = true;
bool                 signalBM = true;

static bool          doForceDispUpd = false;

static bool          doFluct = false;
static bool          smoothChg = false;
static int           curra = 0, currb = 0, currc = 0;
static unsigned long lastFluct = 0;
static unsigned long fluctInt =  500;
static int           targetNum = 0;
static int           dispNum = 0;
static int           prevDispNum = -1;
static unsigned long lastSmooth = 0;

static char          sysMsgBuf[8] = { 0 };
static char          prevSysMsgBuf[8] = { 0 };
static bool          sysMsg = false;
static unsigned long sysMsgTimeout = 1000;
static unsigned long sysMsgNow = 0;

static bool          playTTsounds = true;

bool networkTimeTravel = false;
bool networkTCDTT      = false;
bool networkReentry    = false;
bool networkAbort      = false;
bool networkAlarm      = false;
uint16_t networkLead   = ETTO_LEAD;
uint16_t networkP1     = 6600;

#define GPS_GRACE_PERIOD 1000
static bool usingGPSS   = false;
static int16_t gpsSpeed = -1;
static int16_t prevGPSSpeed = -2;
static int16_t oldGpsSpeed  = -2;
static bool spdIsRotEnc = false;
static bool GPSended    = false;
static unsigned long GPSendedNow = 0;
static unsigned long gpsGracePeriod = 0;

static bool    usingTEMP       = false;
static int16_t TCDtemperature  = -32768;
static bool    TCDtempIsC      = false;
bool           haveTCDTemp     = false;
static float   temperature     = NAN;
static float   prevTemperature = -32767.0;

// Temperature update intervals
#define TEMP_UPD_INT_L (2*60*1000)

bool                 haveTempSens = false;
static bool          tempUnit = DEF_TEMP_UNIT;
#ifdef VSR_HAVETEMP
static unsigned long tempReadNow = 0;
static unsigned long tempUpdInt = TEMP_UPD_INT_L;
#endif

bool showBM = false;
static uint8_t prevButtonMode = 99;

static bool useNM = false;
static bool tcdNM = false;
bool        vsrNM = false;
static bool useFPO = false;
static bool tcdFPO = false;

bool        bttfnTT = false;

// Time travel status flags etc.
#define RELAY_AHEAD  1000                // Relay-click ahead of reaching 88 (in ms) (used for sync'd tts only)
bool                 TTrunning = false;  // TT sequence is running
static bool          extTT = false;      // TT was triggered by TCD
static unsigned long TTstart = 0;
static unsigned long P0duration = ETTO_LEAD;
static bool          TTP0 = false;
static bool          TTP1 = false;
static bool          TTP2 = false;
static unsigned long TTFInt = 0;
static unsigned long TTFDelay = 0;
static unsigned long TTfUpdNow = 0;
static int           TTcnt = 0;
#define TT_V_LEV     0.7

bool         TCDconnected = false;
static bool  noETTOLead = false;

static bool          volchanged = false;
static unsigned long volchgnow = 0;
static bool          brichanged = false;
static unsigned long brichgnow = 0;
static bool          butchanged = false;
static unsigned long butchgnow = 0;
bool                 udispchanged = false;
unsigned long        udispchgnow = 0;

static unsigned long ssLastActivity = 0;
static unsigned long ssDelay = 0;
static unsigned long ssOrigDelay = 0;
bool                 ssActive = false;

static bool          nmOld = false;
static bool          fpoOld = false;
bool                 FPBUnitIsOn = true;

// BTTF network
#define BTTFN_VERSION              1
#define BTTF_PACKET_SIZE          48
#define BTTF_DEFAULT_LOCAL_PORT 1338
#define BTTFN_NOT_PREPARE  1
#define BTTFN_NOT_TT       2
#define BTTFN_NOT_REENTRY  3
#define BTTFN_NOT_ABORT_TT 4
#define BTTFN_NOT_ALARM    5
#define BTTFN_NOT_REFILL   6
#define BTTFN_NOT_FLUX_CMD 7
#define BTTFN_NOT_SID_CMD  8
#define BTTFN_NOT_PCG_CMD  9
#define BTTFN_NOT_WAKEUP   10
#define BTTFN_NOT_AUX_CMD  11
#define BTTFN_NOT_VSR_CMD  12
#define BTTFN_TYPE_ANY     0    // Any, unknown or no device
#define BTTFN_TYPE_FLUX    1    // Flux Capacitor
#define BTTFN_TYPE_SID     2    // SID
#define BTTFN_TYPE_PCG     3    // Plutonium gauge panel
#define BTTFN_TYPE_VSR     4    // VSR
#define BTTFN_TYPE_AUX     5    // Aux (user custom device)
static const uint8_t BTTFUDPHD[4] = { 'B', 'T', 'T', 'F' };
static bool          useBTTFN = false;
static WiFiUDP       bttfUDP;
static UDP*          vsrUDP;
static byte          BTTFUDPBuf[BTTF_PACKET_SIZE];
static unsigned long BTTFNUpdateNow = 0;
static unsigned long BTFNTSAge = 0;
static unsigned long BTTFNTSRQAge = 0;
static bool          BTTFNPacketDue = false;
static bool          BTTFNWiFiUp = false;
static uint8_t       BTTFNfailCount = 0;
static uint32_t      BTTFUDPID = 0;
static unsigned long lastBTTFNpacket = 0;
static bool          BTTFNBootTO = false;
static bool          haveTCDIP = false;
static IPAddress     bttfnTcdIP;
#ifdef BTTFN_MC
static uint32_t      tcdHostNameHash = 0;
#endif  

static int      iCmdIdx = 0;
static int      oCmdIdx = 0;
static uint32_t commandQueue[16] = { 0 };

static void execute_remote_command();

static void play_startup();
static void displayButtonMode();

static void setNightMode(bool nmode);

static void ttkeyScan();
static void TTKeyPressed(int i, ButState j);
static void TTKeyHeld(int i, ButState j);

static void ssStart();

#ifdef VSR_HAVETEMP
static void myCustomDelay_Sens(unsigned long mydel);
static void updateTemperature(bool force = false);
#endif

static void myloop();

static void bttfn_setup();
static void BTTFNCheckPacket();
static bool BTTFNTriggerUpdate();
static void BTTFNSendPacket();


void main_boot()
{
    
}

void main_boot2()
{   
    // Init LED segment display
    int temp = 0;
    
    #ifdef HAVE_DISPSELECTION
    int temp = atoi(settings.dispType);
    if(temp < VSR_DISP_MIN_TYPE || temp >= VSR_DISP_NUM_TYPES) {
        Serial.println("Bad display type");
    } else {
    #endif  
        if(!vsrdisplay.begin(temp)) {
            Serial.println("Display not found");
        } else {
            loadBrightness();
        }
    #ifdef HAVE_DISPSELECTION        
    }
    #endif

    showWaitSequence();
}

void main_setup()
{
    unsigned long now = millis();
    
    Serial.println(F("Voltage Systems Regulator version " VSR_VERSION " " VSR_VERSION_EXTRA));

    updateConfigPortalBriValues();

    loadButtonMode();
    loadUDispMode();

    // Init button lights/LEDs
    vsrLEDs.begin(3);

    // Invoke audio file installer if SD content qualifies
    #ifdef VSR_HAVEAUDIO
    #ifdef VSR_DBG
    Serial.println(F("Probing for audio data on SD"));
    #endif
    if(check_allow_CPA()) {
        showWaitSequence();
        if(prepareCopyAudioFiles()) {
            play_file("/_installing.mp3", PA_ALLOWSD, 1.0);
            waitAudioDone();
        }
        doCopyAudioFiles();
        // We never return here. The ESP is rebooted.
    }
    #endif

    smoothChg = (atoi(settings.smoothpw) > 0);
    doFluct = (atoi(settings.fluct) > 0);
    displayBM = (atoi(settings.displayBM) > 0);
    ssDelay = ssOrigDelay = atoi(settings.ssTimer) * 60 * 1000;

    vsrdisplay.setNMOff((atoi(settings.diNmOff) > 0));

    #ifdef VSR_HAVEAUDIO
    playTTsounds = (atoi(settings.playTTsnds) > 0);
    #endif

    // Determine if Time Circuits Display is connected
    // via wire, and is source of GPIO tt trigger
    TCDconnected = (atoi(settings.TCDpresent) > 0);
    noETTOLead = (atoi(settings.noETTOLead) > 0);

    // Eval other options
    useNM = (atoi(settings.useNM) > 0);
    useFPO = (atoi(settings.useFPO) > 0);
    bttfnTT = (atoi(settings.bttfnTT) > 0);

    tempUnit = (atoi(settings.tempUnit) > 0);
    #ifdef VSR_HAVETEMP
    if(tempSens.begin(powerupMillis, myCustomDelay_Sens)) {
        haveTempSens = true;
        tempSens.setOffset((float)strtof(settings.tempOffs, NULL));
        updateTemperature(true);
    } else {
        haveTempSens = false;
        #ifdef VSR_DBG
        Serial.println(F("Temperature sensor not detected"));
        #endif
    }
    #else
    haveTempSens = false;
    #endif

    // Set up TT button / TCD trigger
    TTKey.begin(
        TT_IN_PIN, 
        false,    // Button is active HIGH
        false,    // Disable internal pull-up resistor
        true      // Enable internal pull-down resistor
    );
    TTKey.attachPressEnd(TTKeyPressed);
    if(!TCDconnected) {
        // If we are in fact a physical button, we need
        // reasonable values for debounce and press
        TTKey.setTiming(TT_DEBOUNCE, TT_HOLD_TIME);
        TTKey.attachLongPressStart(TTKeyHeld);
    } else {
        // If the TCD is connected, we can go more to the edge
        TTKey.setTiming(5, 100000);
        // Long press ignored when TCD is connected
    }

    #ifdef VSR_HAVEAUDIO
    if(!haveAudioFiles) {
        #ifdef VSR_DBG
        Serial.println(F("Current audio data not installed"));
        #endif
        vsrdisplay.on();
        vsrdisplay.setText("AUD");
        vsrdisplay.show();
        delay(1000);
        vsrdisplay.clearBuf();
        vsrdisplay.show();
    }
    #endif

    // Initialize BTTF network
    bttfn_setup();
    bttfn_loop();

    if(useBTTFN && useFPO && (WiFi.status() == WL_CONNECTED)) {
      
        FPBUnitIsOn = false;
        tcdFPO = fpoOld = true;

        vsrdisplay.on();

        // Light up center dot for 500ms
        vsrdisplay.setText("  .");
        vsrdisplay.show();
        mydelay(500);
        vsrdisplay.clearBuf();
        vsrdisplay.show();

        vsrdisplay.off();
        vsrLEDs.off();

        Serial.println("Waiting for TCD fake power on");
    
    } else {

        // Otherwise boot:
        FPBUnitIsOn = true;

        // Play startup sequence
        play_startup();

        // Reset BLED states, button states
        resetBLEDandBState();
            
        if(displayBM) {
            displayButtonMode();
        }
        
        ssRestartTimer();
        
    }

    #ifdef VSR_DBG
    Serial.println(F("main_setup() done"));
    #endif

}

void main_loop()
{
    unsigned long now = millis();
    bool forceDispUpd = false;
    bool wheelsChanged = false;

    // Follow TCD fake power
    if(useFPO && (tcdFPO != fpoOld)) {
        if(tcdFPO) {
            // Power off:
            FPBUnitIsOn = false;
            
            if(TTrunning) {
                // Reset to idle
                vsrdisplay.setBrightness(255);
                // ...
            }
            TTrunning = false;
            
            // Display OFF
            vsrdisplay.off();
            vsrLEDs.off();

            flushDelayedSave();
            
            // FIXME - anything else?
            
        } else {
            // Power on: 
            FPBUnitIsOn = true;
            
            // Display ON, idle
            vsrdisplay.setBrightness(255);
            vsrdisplay.on();

            vsrLEDs.on();

            isTTKeyHeld = isTTKeyPressed = false;
            networkTimeTravel = false;

            // Play startup sequence
            play_startup();

            // Reset BLED states, button states
            resetBLEDandBState();

            if(displayBM) {
                displayButtonMode();
            }

            ssRestartTimer();
            ssActive = false;

            // Force display update
            doForceDispUpd = true;

            // FIXME - anything else?
 
        }
        fpoOld = tcdFPO;
    }

    // Execute remote commands
    if(FPBUnitIsOn) {
        execute_remote_command();
    }

    // Update temp sensor reading
    #ifdef VSR_HAVETEMP
    updateTemperature();
    #endif

    // Need to call scan every time due to buttons
    if((wheelsChanged = scanControls())) {
        ssRestartTimer();
    }

    // Handle pushwheel values
    if(wheelsChanged) {
        vsrControls.getValues(curra, currb, currc);
        //Serial.printf("%d %d %d\n", curra, currb, currc);
        if(curra >= 0 && currb >= 0 && currc >= 0) {
            targetNum = (curra * 100) + (currb * 10) + currc;
            lastSmooth = now - 250;
        }
    }
    if(smoothChg && (dispNum != targetNum)) {
        if(now - lastSmooth > 500) {
            if(abs(targetNum - dispNum) > 2) {
                dispNum = (targetNum + dispNum) / 2;
            } else {
                dispNum = targetNum;
            }
            lastSmooth = now;
        }
    } else {
        dispNum = targetNum;
    }

    // TT button evaluation
    if(FPBUnitIsOn && !TTrunning) {
        ttkeyScan();
        if(isTTKeyHeld) {
            ssEnd();
            isTTKeyHeld = isTTKeyPressed = false;
            // ...?
        } else if(isTTKeyPressed) {
            isTTKeyPressed = false;
            if(!TCDconnected && ssActive) {
                // First button press when ss is active only deactivates SS
                ssEnd();
            } else {
                if(TCDconnected) {
                    ssEnd();
                }
                if(TCDconnected || !bttfnTT || !BTTFNTriggerTT()) {
                    timeTravel(TCDconnected, noETTOLead ? 0 : ETTO_LEAD);
                }
            }
        }
    
        // Check for BTTFN/MQTT-induced TT
        if(networkTimeTravel) {
            networkTimeTravel = false;
            ssEnd();
            timeTravel(networkTCDTT, networkLead);
        }
    }

    now = millis();
    
    // TT sequence logic: TT is mutually exclusive with stuff below (like SID)
    
    if(TTrunning) {

        if(extTT) {

            // ***********************************************************************************
            // TT triggered by TCD (BTTFN, GPIO or MQTT) *****************************************
            // ***********************************************************************************

            if(TTP0) {   // Acceleration - runs for ETTO_LEAD ms by default

                if(!networkAbort && (now - TTstart < P0duration)) {

                    // TT "acceleration"

                    if(!TTFDelay || (now - TTfUpdNow >= TTFDelay)) {

                        TTFDelay = 0;
                        if(TTFInt) {
                            TTFInt = 0;
                            #ifdef VSR_HAVEAUDIO
                            if(playTTsounds) {
                                play_file("/ttstart.mp3", PA_INTRMUS|PA_ALLOWSD, TT_V_LEV);
                                append_file("/humm.wav", PA_LOOP|PA_WAV|PA_INTRMUS|PA_ALLOWSD, TT_V_LEV);
                            }
                            #endif

                        }
                        TTfUpdNow = now;
                       
                    } 
                      
                    // Update display during "acceleration"
                    switch(dispMode) {
                    case LDM_GPS:
                        if(gpsSpeed != prevGPSSpeed) {
                            vsrdisplay.setSpeed(gpsSpeed);
                            vsrdisplay.show();
                            prevGPSSpeed = gpsSpeed;
                        }
                        break;
                    }

                } else {

                    if(TTstart == TTfUpdNow) {
                        #ifdef VSR_HAVEAUDIO
                        if(playTTsounds) {
                            // If we have skipped P0, play it now
                            play_file("/ttstart.mp3", PA_INTRMUS|PA_ALLOWSD, TT_V_LEV);
                            append_file("/humm.wav", PA_LOOP|PA_WAV|PA_INTRMUS|PA_ALLOWSD, TT_V_LEV);
                        }
                        #endif
                    }

                    TTP0 = false;
                    TTP1 = true;

                    TTstart = TTfUpdNow = now;
                    TTFInt = 1;

                }
            }
            if(TTP1) {   // Peak/"time tunnel" - ends with pin going LOW or MQTT "REENTRY"

                if( (networkTCDTT && (!networkReentry && !networkAbort)) || (!networkTCDTT && digitalRead(TT_IN_PIN))) 
                {

                    if(TTFInt) {
                        vsrdisplay.setText("1.21");
                        vsrdisplay.show();
                        TTFInt = 0;
                    }
                    
                } else {

                    TTP1 = false;
                    TTP2 = true; 

                    #ifdef VSR_HAVEAUDIO
                    if(playTTsounds) {
                        //append_file("/ttend.mp3", PA_INTRMUS|PA_ALLOWSD, TT_V_LEV);
                        //stopAudioAtLoopEnd();
                        play_file("/ttend.mp3", PA_INTRMUS|PA_ALLOWSD, TT_V_LEV);
                    }
                    #endif

                    TTstart = now;
                    
                }
            }
            if(TTP2) {   // Reentry - up to us
                
                if(now - TTstart > 500) {
                    TTP2 = false;
                    TTrunning = false;
                    isTTKeyHeld = isTTKeyPressed = false;
                    ssRestartTimer();
                    doForceDispUpd = true;
                }
                
            }
          
        } else {

            // ****************************************************************************
            // TT triggered by button (if TCD not connected), MQTT ************************
            // ****************************************************************************
          
            if(TTP0) {   // Acceleration - runs for P0_DUR ms

                if(now - TTstart < P0_DUR) {

                    // TT "acceleration"

                    if(!TTFDelay || (now - TTfUpdNow >= TTFDelay)) {

                        TTFDelay = 0;
                        if(TTFInt) {
                            #ifdef VSR_HAVEAUDIO
                            if(playTTsounds) {
                                play_file("/ttstart.mp3", PA_INTRMUS|PA_ALLOWSD, TT_V_LEV);
                                append_file("/humm.wav", PA_LOOP|PA_WAV|PA_INTRMUS|PA_ALLOWSD, TT_V_LEV);
                            }
                            #endif
                            TTfUpdNow = now;
                            TTFInt = 0;
                        }
                       
                    }

                } else {

                    TTP0 = false;
                    TTP1 = true;
                    
                    TTstart = TTfUpdNow = now;
                    TTFInt = 1;
                    
                }
            }
            
            if(TTP1) {   // Peak/"time tunnel" - runs for P1_DUR ms

                if(now - TTstart < P1_DUR) {

                    if(TTFInt) {
                        vsrdisplay.setText("1.21");
                        vsrdisplay.show();
                        TTFInt = 0;
                    }
                    
                } else {

                    TTP1 = false;
                    TTP2 = true; 

                    #ifdef VSR_HAVEAUDIO
                    if(playTTsounds) {
                        //append_file("/ttend.mp3", PA_INTRMUS|PA_ALLOWSD, TT_V_LEV);
                        //stopAudioAtLoopEnd();
                        play_file("/ttend.mp3", PA_INTRMUS|PA_ALLOWSD, TT_V_LEV);
                    }
                    #endif

                    TTstart = now;
                    
                }
            }
            
            if(TTP2) {   // Reentry - up to us

                if(now - TTstart > 500) {
                    TTP2 = false;
                    TTrunning = false;
                    isTTKeyHeld = isTTKeyPressed = false;
                    ssRestartTimer();
                    doForceDispUpd = true;
                }

            }

        }

    } else {    // No TT currently

        if(networkAlarm && !TTrunning) {

            networkAlarm = false;
            
            ssEnd();

            #ifdef VSR_HAVEAUDIO
            if(atoi(settings.playALsnd) > 0) {
                play_file("/alarm.mp3", PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL, 1.0);
            }
            #endif

            // play alarm sequence
            for(int i = 0; i < 40; i++) {
                vsrdisplay.setText(" \x08 ");
                vsrdisplay.show();
                mydelay(50);
                vsrdisplay.setText(" \x09 ");
                vsrdisplay.show();
                mydelay(50);
            }
            vsrdisplay.clearBuf();
            vsrdisplay.show();
            doForceDispUpd = true;
        
        } else {

            // Wake up on GPS/RotEnc speed changes
            if(gpsSpeed != oldGpsSpeed) {
                if(FPBUnitIsOn && spdIsRotEnc && gpsSpeed >= 0) {
                    wakeup();
                }
                oldGpsSpeed = gpsSpeed;
            }

            now = millis();

            // "Screen saver"
            if(FPBUnitIsOn) {
                if(!ssActive && ssDelay && (now - ssLastActivity > ssDelay)) {
                    ssStart();
                }
            }
          
            if(FPBUnitIsOn) {

                now = millis();

                if(!showBM) {

                    forceDispUpd = (dispMode == LDM_BM);

                    if(sysMsg && (now - sysMsgNow < sysMsgTimeout)) {

                        forceDispUpd = (dispMode != LDM_SYS);
                        dispMode = LDM_SYS;
                      
                    } else {

                        sysMsg = false;
                        forceDispUpd |= (dispMode == LDM_SYS);
                        
                        // Translate user display mode to system display mode
                        switch(userDispMode) {
                          
                        case LDM_TEMP:  
                            if(haveTempSens || haveTCDTemp) {
                                usingTEMP = true;
                                dispMode = LDM_TEMP;
                            } else {
                                usingTEMP = false;
                            }
                            break;
        
                        case LDM_GPS:
                            if(gpsSpeed >= 0) {
                                usingGPSS = true;
                                GPSended = false;
                                dispMode = LDM_GPS;
                            } else {
                                if(usingGPSS) {
                                    usingGPSS = false;
                                    GPSended = true;
                                    GPSendedNow = now;
                                    gpsGracePeriod = spdIsRotEnc ? 0 : GPS_GRACE_PERIOD;
                                    dispMode = LDM_GPS;
                                } else if(GPSended) {
                                    dispMode = LDM_GPS;
                                } else {
                                    dispMode = LDM_WHEELS;
                                }
                            }
                            break;
        
                        case LDM_WHEELS:
                            dispMode = LDM_WHEELS;
                            break;
                        }

                    }

                } else {
                  
                    forceDispUpd = (dispMode != LDM_BM);
                    dispMode = LDM_BM;
                    
                }

                forceDispUpd |= doForceDispUpd;
                doForceDispUpd = false;

                // Sanitize
                switch(dispMode) {
                case LDM_GPS:
                    if(GPSended && (now - GPSendedNow >= gpsGracePeriod)) {
                        GPSended = false;
                        dispMode = LDM_WHEELS;
                    }
                    break;
                case LDM_TEMP:
                    if(!usingTEMP) {
                        dispMode = LDM_WHEELS;
                    }
                    break;
                }

                forceDispUpd |= (dispMode != lastDispMode);

                // Display
                switch(dispMode) {
                case LDM_GPS:
                    if(forceDispUpd || (gpsSpeed != prevGPSSpeed)) {
                        vsrdisplay.setSpeed(gpsSpeed);
                        vsrdisplay.show();
                        prevGPSSpeed = gpsSpeed;
                    }
                    break;
                case LDM_TEMP:
                    #ifdef VSR_HAVETEMP
                    if(haveTempSens) {
                        temperature = tempSens.readLastTemp();
                    } else {
                        float t = (float)TCDtemperature / 100.0;
                        if(TCDtemperature != -32768) {
                            if(tempUnit) {
                                if(!TCDtempIsC) {
                                    t = (t - 32.0) * 5.0 / 9.0;
                                }
                            } else {
                                if(TCDtempIsC) {
                                    t = t * 9.0 / 5.0 + 32.0;
                                }
                            }
                            temperature = t;
                        } else {
                            temperature = NAN;
                        }
                    }
                    #endif
                    if(forceDispUpd || (temperature != prevTemperature)) {
                        vsrdisplay.setTemperature(temperature);
                        vsrdisplay.show();
                        prevTemperature = temperature;
                    }
                    break;
                case LDM_BM:
                    if(forceDispUpd || (buttonMode != prevButtonMode)) {
                        vsrdisplay.setText(getBMString());
                        vsrdisplay.show();
                        butchanged = (buttonMode != prevButtonMode);
                        butchgnow = millis();
                        prevButtonMode = buttonMode;
                    }
                    break;
                case LDM_SYS:
                    if(forceDispUpd || strcmp(sysMsgBuf, prevSysMsgBuf)) {
                        vsrdisplay.setText(sysMsgBuf);
                        vsrdisplay.show();
                        strcpy(prevSysMsgBuf, sysMsgBuf);
                    }
                    break;
                case LDM_WHEELS:
                default:
                    if(forceDispUpd || wheelsChanged || (dispNum != prevDispNum)) {
                        if(wheelsChanged) {
                            ssEnd();
                        }
                        prevDispNum = dispNum;
                        if(smoothChg) {
                            vsrdisplay.setNumber(dispNum);
                        } else {
                            vsrdisplay.setNumbers(curra, currb, currc);
                        }
                        vsrdisplay.show();
                        lastFluct = millis();
                        fluctInt = 3000 + ((esp_random() % 3000) - 1000);
                    } else if(doFluct && (millis() - lastFluct > fluctInt)) {
                        if(curra >= 0 && currb >= 0 && currc >= 0) {
                            int num = ((curra * 100) + (currb * 10) + currc) + ((esp_random() % 3) - 1);
                            if(num > 999) num = 999;
                            else if(num < 0) num = 0;
                            vsrdisplay.setNumber(num);
                            vsrdisplay.show();
                        }
                        lastFluct = millis();
                        fluctInt = 1250 + ((esp_random() % 800) - 200);
                    }

                }

                lastDispMode = dispMode;

            }

        }
        
    }

    // Follow TCD night mode
    if(useNM && (tcdNM != nmOld)) {
        setNightMode(tcdNM);
        nmOld = tcdNM;
    }

    now = millis();
    
    // If network is interrupted, return to stand-alone
    if(useBTTFN) {
        if( (lastBTTFNpacket && (now - lastBTTFNpacket > 30*1000)) ||
            (!BTTFNBootTO && !lastBTTFNpacket && (now - powerupMillis > 60*1000)) ) {
            tcdNM = false;
            tcdFPO = false;
            gpsSpeed = -1;
            if(!haveTempSens) temperature = -32768;
            haveTCDTemp = false;
            lastBTTFNpacket = 0;
            BTTFNBootTO = true;
        }
    }

    if(!TTrunning) {
        // Save secondary settings 10 seconds after last change
        if(brichanged && (now - brichgnow > 10000)) {
            brichanged = false;
            saveBrightness();
        }
        #ifdef VSR_HAVEAUDIO
        if(volchanged && (now - volchgnow > 10000)) {
            volchanged = false;
            saveCurVolume();
        }
        #endif
        if(butchanged && (now - butchgnow > 10000)) {
            butchanged = false;
            saveButtonMode();
        }
        if(udispchanged && (now - udispchgnow > 10000)) {
            udispchanged = false;
            saveUDispMode();
        }
    }

}

void flushDelayedSave()
{
    if(brichanged) {
        brichanged = false;
        saveBrightness();
    }
    #ifdef VSR_HAVEAUDIO
    if(volchanged) {
        volchanged = false;
        saveCurVolume();
    }
    #endif
    if(butchanged) {
        butchanged = false;
        saveButtonMode();
    }
    if(udispchanged) {
        udispchanged = false;
        saveUDispMode();
    }
}

#ifdef VSR_HAVEAUDIO
static void chgVolume(int d)
{
    char buf[8];

    #ifdef VSR_HAVEVOLKNOB
    if(curSoftVol == 255)
        return;
    #endif

    int nv = curSoftVol;
    nv += d;
    if(nv < 0 || nv > 19)
        nv = curSoftVol;
        
    curSoftVol = nv;

    sprintf(buf, "%3d", nv);
    displaySysMsg(buf, 1000);

    volchanged = true;
    volchgnow = millis();
    updateConfigPortalVolValues();
}

void increaseVolume()
{
    chgVolume(1);
}

void decreaseVolume()
{
    chgVolume(-1);
}
#endif

/*
 * Time travel
 * 
 */

void timeTravel(bool TCDtriggered, uint16_t P0Dur)
{
    if(TTrunning)
        return;

    flushDelayedSave();
        
    TTrunning = true;
    TTstart = TTfUpdNow = millis();
    TTP0 = true;   // phase 0
    
    if(TCDtriggered) {    // TCD-triggered TT (GPIO, BTTFN or MQTT) (synced with TCD)
      
        extTT = true;
        P0duration = P0Dur;
        #ifdef VSR_DBG
        Serial.printf("P0 duration is %d\n", P0duration);
        #endif
        if(P0duration > RELAY_AHEAD) {
            TTFDelay = P0duration - RELAY_AHEAD;
        } else if(P0duration > 600) {
            TTFDelay = P0duration - 600;
        } else {
            TTFDelay = 0;
        }
        TTFInt = 1;
        
    } else {              // button/IR-triggered TT (stand-alone)
      
        extTT = false;
        TTFDelay = P0_DUR - 600;
        TTFInt = 1;
                    
    }
    
    #ifdef VSR_DBG
    Serial.printf("TTFDelay %d  TTFInt %d  TTcnt %d\n", TTFDelay, TTFInt, TTcnt);
    #endif
}

static void play_startup()
{
    int i = 0, j = 0;
    const int8_t intro[11][4] = {
        {  1,  0, 0, 0 },
        { 32,  1, 0, 0 },
        { 32, 32, 1, 0 },
        { 32, 32, 2, 0 },
        { 32, 32, 3, 0 },
        { 32, 32, 4, 0 },
        { 32,  4, 0, 0 },
        {  4,  0, 0, 0 },
        {  5,  0, 0, 0 },
        {  6,  0, 0, 0 },
        {  0,  0, 0, 0 }
    };

    play_file("/startup.mp3", PA_ALLOWSD, 1.0);

    while(!checkAudioDone() || !j) {
        vsrdisplay.setText((char *)intro[i++]);
        vsrdisplay.show();
        myloop();
        delay(50);
        if(i > 9) { i = 0; j++; }
    }
    vsrdisplay.setText((char *)intro[10]);
}

static void displayButtonMode()
{        
    vsrdisplay.setText(getBMString());
    vsrdisplay.show();
    mydelay(1000);
    vsrdisplay.clearBuf();
    vsrdisplay.show();
    mydelay(500);
}

void displaySysMsg(const char *msg, unsigned long timeout)
{
    strncpy(sysMsgBuf, msg, 7);
    prevSysMsgBuf[0] = 0;
    sysMsgTimeout = timeout;
    sysMsg = true;
    sysMsgNow = millis();
}

static void setNightMode(bool nmode)
{
    vsrdisplay.setNightMode(nmode);
    vsrLEDs.setNightMode(nmode);
    vsrNM = nmode;
}

void toggleNightMode()
{
    setNightMode(!vsrNM);
}

static void ttkeyScan()
{
    TTKey.scan();
}

static void TTKeyPressed(int i, ButState j)
{
    isTTKeyPressed = true;
}

static void TTKeyHeld(int i, ButState j)
{
    isTTKeyHeld = true;
}

// "Screen saver"

static void ssStart()
{
    if(ssActive)
        return;

    // SS: display off; button lights off
    vsrdisplay.off();
    vsrLEDs.off();

    ssActive = true;
}

void ssRestartTimer()
{
    ssLastActivity = millis();
}

void ssEnd()
{
    if(!FPBUnitIsOn)
        return;
        
    ssRestartTimer();
    
    if(!ssActive)
        return;

    vsrdisplay.on();
    vsrLEDs.on();

    ssActive = false;
}

/*
 * Show special signals
 */

void showWaitSequence()
{
    vsrdisplay.setText("\78\7");
    vsrdisplay.show();
}

void endWaitSequence()
{
    vsrdisplay.clearBuf();
    vsrdisplay.show();
    doForceDispUpd = true;
}

void showCopyError()
{
    vsrdisplay.setText("ERR");
    vsrdisplay.show();
    doForceDispUpd = true;
}

void prepareTT()
{
    ssEnd();
}

// Wakeup: Sent by TCD upon entering dest date,
// return from tt, triggering delayed tt via ETT
// For audio-visually synchronized behavior
// Also called when RotEnc speed is changed
void wakeup()
{
    // End screen saver
    ssEnd();
}

static void execute_remote_command()
{
    uint32_t command = commandQueue[oCmdIdx];

    // We are only called if ON
    // No command execution during time travel
    // No command execution during timed sequences
    // ssActive checked by individual command

    if(!command || TTrunning)
        return;

    commandQueue[oCmdIdx] = 0;
    oCmdIdx++;
    oCmdIdx &= 0x0f;

    if(command < 10) {                                // 800x
        switch(command) {
        case 1:
            #ifdef VSR_HAVEAUDIO                      // 8001: play "key1.mp3"
            play_file("/key1.mp3", PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL);
            #endif
            break;
        case 2:
            #ifdef VSR_HAVEAUDIO
            if(haveMusic) {
                mp_prev(mpActive);                    // 8002: Prev song
            }
            #endif
            break;
        case 3:
            #ifdef VSR_HAVEAUDIO                      // 8003: play "key3.mp3"
            play_file("/key3.mp3", PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL);
            #endif
            break;
        case 4:
            #ifdef VSR_HAVEAUDIO                      // 8004: play "key4.mp3"
            play_file("/key4.mp3", PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL);
            #endif
            break;
        case 5:                                       // 8005: Play/stop
            #ifdef VSR_HAVEAUDIO
            if(haveMusic) {
                if(mpActive) {
                    mp_stop();
                } else {
                    mp_play();
                }
            }
            #endif
            break;
        case 6:                                       // 8006: Play "key6.mp3"
            #ifdef VSR_HAVEAUDIO
            play_file("/key6.mp3", PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL);
            #endif
            break;
        case 7:
            #ifdef VSR_HAVEAUDIO                      // 8007: play "key7.mp3"
            play_file("/key7.mp3", PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL);
            #endif
            break;
        case 8:                                       // 8008: Next song
            #ifdef VSR_HAVEAUDIO
            if(haveMusic) {
                mp_next(mpActive);
            }
            #endif
            break;
        case 9:
            #ifdef VSR_HAVEAUDIO                      // 8009: play "key9.mp3"
            play_file("/key9.mp3", PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL);
            #endif
            break;
        }
      
    } else if(command < 100) {                        // 80xx

        if(command >= 10 && command <= 19) {          // 8010-8012: Set display mode
            switch(command - 10) {
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

        } else if(command == 90) {                    // 8090: Display IP address

            flushDelayedSave();
            display_ip();
            ssRestartTimer();
          
        } else if(command >= 50 && command <= 59) {   // 8050-8059: Set music folder number
            
            #ifdef VSR_HAVEAUDIO
            if(haveSD) {
                switchMusicFolder((uint8_t)command - 50);
            }
            #endif
            
        }
        
    } else if (command < 1000) {                      // 8xxx

        if(command >= 300 && command <= 399) {

            #ifdef VSR_HAVEAUDIO
            command -= 300;                       // 8300-8319/8399: Set fixed volume level / enable knob
            if(command == 99) {
                #ifdef VSR_HAVEVOLKNOB
                curSoftVol = 255;
                volchanged = true;
                volchgnow = millis();
                updateConfigPortalVolValues();
                #endif
            } else if(command <= 19) {
                curSoftVol = command;
                volchanged = true;
                volchgnow = millis();
                updateConfigPortalVolValues();
            }
            #endif

        } else if(command >= 400 && command <= 415) {

            command -= 400;                       // 8400-8415: Set brightness
            if(!TTrunning) {
                vsrdisplay.setBrightness(command);
                brichanged = true;
                brichgnow = millis();
            }

        } else {

            switch(command) {
            case 222:                             // 8222/8555 Disable/enable shuffle
            case 555:
                #ifdef VSR_HAVEAUDIO
                if(haveMusic) {
                    mp_makeShuffle((command == 555));
                }
                #endif
                break;
            case 888:                             // 8888 go to song #0
                #ifdef VSR_HAVEAUDIO
                if(haveMusic) {
                    mp_gotonum(0, mpActive);
                }
                #endif
                break;
            }
        }
        
    } else {
      
        switch(command) {
        case 64738:                               // 8064738: reboot
            vsrdisplay.off();
            mp_stop();
            stopAudio();
            flushDelayedSave();
            unmount_fs();
            delay(500);
            esp_restart();
            break;
        case 123456:
            flushDelayedSave();
            deleteIpSettings();                   // 8123456: deletes IP settings
            if(settings.appw[0]) {
                settings.appw[0] = 0;             // and clears AP mode WiFi password
                write_settings();
            }
            ssRestartTimer();
            break;
        default:                                  // 8888xxx: goto song #xxx
            #ifdef VSR_HAVEAUDIO
            if((command / 1000) == 888) {
                uint16_t num = command - 888000;
                num = mp_gotonum(num, mpActive);
            }
            #endif
            break;
        }

    }
}

void display_ip()
{
    uint8_t a[4];
    char buf[8];

    vsrdisplay.setText("IP");
    vsrdisplay.show();

    mydelay(500);

    wifi_getIP(a[0], a[1], a[2], a[3]);

    for(int i = 0; i < 4; i++) {
        sprintf(buf, "%3d%s", a[i], (i < 3) ? "." : "");
        vsrdisplay.setText(buf);
        vsrdisplay.show();
        mydelay(1000);
    }
    
    vsrdisplay.clearBuf();
    vsrdisplay.show();
    mydelay(500);

    doForceDispUpd = true;
}

#ifdef VSR_HAVEAUDIO
void switchMusicFolder(uint8_t nmf)
{
    bool waitShown = false;

    if(nmf > 9) return;
    
    if(musFolderNum != nmf) {
        musFolderNum = nmf;
        // Initializing the MP can take a while;
        // need to stop all audio before calling
        // mp_init()
        if(haveMusic && mpActive) {
            mp_stop();
        }
        stopAudio();
        if(mp_checkForFolder(musFolderNum) == -1) {
            flushDelayedSave();
            showWaitSequence();
            waitShown = true;
            play_file("/renaming.mp3", PA_INTRMUS|PA_ALLOWSD);
            waitAudioDone();
        }
        saveMusFoldNum();
        updateConfigPortalMFValues();
        mp_init(false);
        if(waitShown) {
            endWaitSequence();
        }
    }
}

void waitAudioDone()
{
    int timeout = 400;

    while(!checkAudioDone() && timeout--) {
        mydelay(10);
    }
}
#endif

/*
 *  Do this whenever we are caught in a while() loop
 *  This allows other stuff to proceed
 */
static void myloop()
{
    wifi_loop();
    audio_loop();
}

/*
 * Delay function for external modules
 * (controls, ...). 
 * Do not call wifi_loop() here!
 */
static void myCustomDelay_int(unsigned long mydel, uint32_t gran)
{
    unsigned long startNow = millis();
    audio_loop();
    while(millis() - startNow < mydel) {
        delay(gran);
        audio_loop();
    }
}
void myCustomDelay_KP(unsigned long mydel)
{
    myCustomDelay_int(mydel, 1);
}

#ifdef VSR_HAVETEMP
static void myCustomDelay_Sens(unsigned long mydel)
{
    myCustomDelay_int(mydel, 5);
}

static void updateTemperature(bool force)
{
    if(!haveTempSens)
        return;
        
    if(force || (millis() - tempReadNow >= tempUpdInt)) {
        tempSens.readTemp(tempUnit);
        tempReadNow = millis();
    }
}
#endif

/*
 * MyDelay:
 * Calls myloop() periodically
 */
void mydelay(unsigned long mydel)
{
    unsigned long startNow = millis();
    myloop();
    while(millis() - startNow < mydel) {
        delay(10);
        myloop();
    }
}

/*
 * Basic Telematics Transmission Framework (BTTFN)
 */

static void addCmdQueue(uint32_t command)
{
    if(!command) return;

    commandQueue[iCmdIdx] = command;
    iCmdIdx++;
    iCmdIdx &= 0x0f;
}

static void bttfn_setup()
{
    useBTTFN = false;

    // string empty? Disable BTTFN.
    if(!settings.tcdIP[0])
        return;

    haveTCDIP = isIp(settings.tcdIP);
    
    if(!haveTCDIP) {
        #ifdef BTTFN_MC
        tcdHostNameHash = 0;
        unsigned char *s = (unsigned char *)settings.tcdIP;
        for ( ; *s; ++s) tcdHostNameHash = 37 * tcdHostNameHash + tolower(*s);
        #else
        return;
        #endif
    } else {
        bttfnTcdIP.fromString(settings.tcdIP);
    }
    
    vsrUDP = &bttfUDP;
    vsrUDP->begin(BTTF_DEFAULT_LOCAL_PORT);
    BTTFNfailCount = 0;
    useBTTFN = true;
}

void bttfn_loop()
{
    if(!useBTTFN)
        return;
        
    BTTFNCheckPacket();
    
    if(!BTTFNPacketDue) {
        // If WiFi status changed, trigger immediately
        if(!BTTFNWiFiUp && (WiFi.status() == WL_CONNECTED)) {
            BTTFNUpdateNow = 0;
        }
        if((!BTTFNUpdateNow) || (millis() - BTTFNUpdateNow > 1100)) {
            BTTFNTriggerUpdate();
        }
    }
}

// Check for pending packet and parse it
static void BTTFNCheckPacket()
{
    unsigned long mymillis = millis();
    
    int psize = vsrUDP->parsePacket();
    if(!psize) {
        if(BTTFNPacketDue) {
            if((mymillis - BTTFNTSRQAge) > 700) {
                // Packet timed out
                BTTFNPacketDue = false;
                // Immediately trigger new request for
                // the first 10 timeouts, after that
                // the new request is only triggered
                // in greater intervals via bttfn_loop().
                if(haveTCDIP && BTTFNfailCount < 10) {
                    BTTFNfailCount++;
                    BTTFNUpdateNow = 0;
                }
            }
        }
        return;
    }
    
    vsrUDP->read(BTTFUDPBuf, BTTF_PACKET_SIZE);

    // Basic validity check
    if(memcmp(BTTFUDPBuf, BTTFUDPHD, 4))
        return;

    uint8_t a = 0;
    for(int i = 4; i < BTTF_PACKET_SIZE - 1; i++) {
        a += BTTFUDPBuf[i] ^ 0x55;
    }
    if(BTTFUDPBuf[BTTF_PACKET_SIZE - 1] != a)
        return;

    if(BTTFUDPBuf[4] == (BTTFN_VERSION | 0x40)) {

        // A notification from the TCD

        switch(BTTFUDPBuf[5]) {
        case BTTFN_NOT_PREPARE:
            // Prepare for TT. Comes at some undefined point,
            // an undefined time before the actual tt, and
            // may not come at all.
            // We disable our Screen Saver.
            // We don't ignore this if TCD is connected by wire,
            // because this signal does not come via wire.
            if(!TTrunning) {
                prepareTT();
            }
            break;
        case BTTFN_NOT_TT:
            // Trigger Time Travel (if not running already)
            // Ignore command if TCD is connected by wire
            if(!TCDconnected && !TTrunning) {
                networkTimeTravel = true;
                networkTCDTT = true;
                networkReentry = false;
                networkAbort = false;
                networkLead = BTTFUDPBuf[6] | (BTTFUDPBuf[7] << 8);
            }
            break;
        case BTTFN_NOT_REENTRY:
            // Start re-entry (if TT currently running)
            // Ignore command if TCD is connected by wire
            if(!TCDconnected && TTrunning && networkTCDTT) {
                networkReentry = true;
            }
            break;
        case BTTFN_NOT_ABORT_TT:
            // Abort TT (if TT currently running)
            // Ignore command if TCD is connected by wire
            if(!TCDconnected && TTrunning && networkTCDTT) {
                networkAbort = true;
            }
            break;
        case BTTFN_NOT_ALARM:
            networkAlarm = true;
            // Eval this at our convenience
            break;
        case BTTFN_NOT_VSR_CMD:
            addCmdQueue( BTTFUDPBuf[6] | (BTTFUDPBuf[7] << 8) |
                        (BTTFUDPBuf[8] | (BTTFUDPBuf[9] << 8)) << 16);
            break;
        case BTTFN_NOT_WAKEUP:
            if(!TTrunning) {
                wakeup();
            }
            break;
        }
      
    } else {

        // (Possibly) a response packet
    
        if(*((uint32_t *)(BTTFUDPBuf + 6)) != BTTFUDPID)
            return;
    
        // Response marker missing or wrong version, bail
        if(BTTFUDPBuf[4] != (BTTFN_VERSION | 0x80))
            return;

        BTTFNfailCount = 0;
    
        // If it's our expected packet, no other is due for now
        BTTFNPacketDue = false;

        #ifdef BTTFN_MC
        if(BTTFUDPBuf[5] & 0x80) {
            if(!haveTCDIP) {
                bttfnTcdIP = vsrUDP->remoteIP();
                haveTCDIP = true;
                #ifdef VSR_DBG
                Serial.printf("Discovered TCD IP %d.%d.%d.%d\n", bttfnTcdIP[0], bttfnTcdIP[1], bttfnTcdIP[2], bttfnTcdIP[3]);
                #endif
            } else {
                #ifdef VSR_DBG
                Serial.println("Internal error - received unexpected DISCOVER response");
                #endif
            }
        }
        #endif

        if(BTTFUDPBuf[5] & 0x02) {
            gpsSpeed = (int16_t)(BTTFUDPBuf[18] | (BTTFUDPBuf[19] << 8));
            if(gpsSpeed > 88) gpsSpeed = 88;
            spdIsRotEnc = (BTTFUDPBuf[26] & 0x80) ? true : false;
        }
        if(BTTFUDPBuf[5] & 0x04) {
            TCDtemperature = (int16_t)(BTTFUDPBuf[20] | (BTTFUDPBuf[21] << 8));
            haveTCDTemp = (temperature != -32768);
            TCDtempIsC = !!(BTTFUDPBuf[26] & 0x40);
        }
        if(BTTFUDPBuf[5] & 0x10) {
            tcdNM  = (BTTFUDPBuf[26] & 0x01) ? true : false;
            tcdFPO = (BTTFUDPBuf[26] & 0x02) ? true : false;   // 1 means fake power off
        } else {
            tcdNM = false;
            tcdFPO = false;
        }

        lastBTTFNpacket = mymillis;
    }
}

// Send a new data request
static bool BTTFNTriggerUpdate()
{
    BTTFNPacketDue = false;

    BTTFNUpdateNow = millis();

    if(WiFi.status() != WL_CONNECTED) {
        BTTFNWiFiUp = false;
        return false;
    }

    BTTFNWiFiUp = true;

    // Send new packet
    BTTFNSendPacket();
    BTTFNTSRQAge = millis();
    
    BTTFNPacketDue = true;
    
    return true;
}

static void BTTFNSendPacket()
{
    memset(BTTFUDPBuf, 0, BTTF_PACKET_SIZE);

    // ID
    memcpy(BTTFUDPBuf, BTTFUDPHD, 4);

    // Serial
    *((uint32_t *)(BTTFUDPBuf + 6)) = BTTFUDPID = (uint32_t)millis();

    // Tell the TCD about our hostname (0-term., 13 bytes total)
    strncpy((char *)BTTFUDPBuf + 10, settings.hostName, 12);
    BTTFUDPBuf[10+12] = 0;

    BTTFUDPBuf[10+13] = BTTFN_TYPE_VSR;

    BTTFUDPBuf[4] = BTTFN_VERSION;  // Version
    BTTFUDPBuf[5] = 0x16;           // Request status, temperature, GPS speed

    #ifdef BTTFN_MC
    if(!haveTCDIP) {
        BTTFUDPBuf[5] |= 0x80;
        memcpy(BTTFUDPBuf + 31, (void *)&tcdHostNameHash, 4);
    }
    #endif

    uint8_t a = 0;
    for(int i = 4; i < BTTF_PACKET_SIZE - 1; i++) {
        a += BTTFUDPBuf[i] ^ 0x55;
    }
    BTTFUDPBuf[BTTF_PACKET_SIZE - 1] = a;

    #ifdef BTTFN_MC
    if(haveTCDIP) {
    #endif
        vsrUDP->beginPacket(bttfnTcdIP, BTTF_DEFAULT_LOCAL_PORT);
    #ifdef BTTFN_MC    
    } else {
        #ifdef VSR_DBG
        //Serial.printf("Sending multicast (hostname hash %x)\n", tcdHostNameHash);
        #endif
        vsrUDP->beginPacket("224.0.0.224", BTTF_DEFAULT_LOCAL_PORT + 1);
    }
    #endif
    vsrUDP->write(BTTFUDPBuf, BTTF_PACKET_SIZE);
    vsrUDP->endPacket();
}

bool BTTFNTriggerTT()
{
    if(!useBTTFN)
        return false;

    #ifdef BTTFN_MC
    if(!haveTCDIP)
        return false;
    #endif

    if(WiFi.status() != WL_CONNECTED)
        return false;

    if(!lastBTTFNpacket)
        return false;

    if(TTrunning)
        return false;

    memset(BTTFUDPBuf, 0, BTTF_PACKET_SIZE);

    // ID
    memcpy(BTTFUDPBuf, BTTFUDPHD, 4);

    // Tell the TCD about our hostname (0-term., 13 bytes total)
    strncpy((char *)BTTFUDPBuf + 10, settings.hostName, 12);
    BTTFUDPBuf[10+12] = 0;

    BTTFUDPBuf[10+13] = BTTFN_TYPE_VSR;

    BTTFUDPBuf[4] = BTTFN_VERSION;  // Version
    BTTFUDPBuf[5] = 0x80;           // Trigger BTTFN-wide TT

    uint8_t a = 0;
    for(int i = 4; i < BTTF_PACKET_SIZE - 1; i++) {
        a += BTTFUDPBuf[i] ^ 0x55;
    }
    BTTFUDPBuf[BTTF_PACKET_SIZE - 1] = a;
        
    vsrUDP->beginPacket(bttfnTcdIP, BTTF_DEFAULT_LOCAL_PORT);
    vsrUDP->write(BTTFUDPBuf, BTTF_PACKET_SIZE);
    vsrUDP->endPacket();

    #ifdef VSR_DBG
    Serial.println("Triggered BTTFN-wide TT");
    #endif

    return true;
}
