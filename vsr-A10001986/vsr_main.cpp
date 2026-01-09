/*
 * -------------------------------------------------------------------
 * Voltage Systems Regulator
 * (C) 2024-2026 Thomas Winischhofer (A10001986)
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
#define HDC302X_ADDR   0x45 // [non-default]

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
tempSensor tempSens(9, 
            (uint8_t[9*2]){ MCP9808_ADDR, MCP9808,
                            BMx280_ADDR,  BMx280,
                            SHT40_ADDR,   SHT40,
                            MS8607_ADDR,  MS8607,   // before Si7021
                            SI7021_ADDR,  SI7021,
                            TMP117_ADDR,  TMP117,
                            AHT20_ADDR,   AHT20,
                            HTU31_ADDR,   HTU31,
                            HDC302X_ADDR, HDC302X
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

static bool tcdIsBusy  = false;
bool        vsrBusy    = false;

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
static float   prevTemperature = -32767.0f;

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
static bool diNmOff = false;
static bool nmOverruled = false;

bool        bttfnTT = false;
bool        ignTT = false;

bool doPrepareTT = false;
bool doWakeup = false;

// Time travel status flags etc.
#define RELAY_AHEAD  1000                // Relay-click ahead of reaching 88 (in ms) (used for sync'd tts only)
bool                 TTrunning = false;  // TT sequence is running
bool                 TTrunningIOonly = false;
static bool          extTT = false;      // TT was triggered by TCD
static unsigned long TTstart = 0;
static unsigned long P0duration = ETTO_LEAD;
static unsigned long P1_maxtimeout = 10000;
static bool          TTP0 = false;
static bool          TTP1 = false;
static bool          TTP2 = false;
static unsigned long TTFInt = 0;
static unsigned long TTFDelay = 0;
#define TT_V_LEV     0.7f

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
bool                 blockScan = false;

// BTTF network
#define BTTFN_VERSION              1
#define BTTFN_SUP_MC            0x80
#define BTTFN_SUP_ND            0x40
#define BTTF_PACKET_SIZE          48
#define BTTF_DEFAULT_LOCAL_PORT 1338
#define BTTFN_POLL_INT          1100
#define BTTFN_POLL_INT_FAST      700
#define BTTFN_RESPONSE_TO        700
#define BTTFN_KA_OFFSET           11
#define BTTFN_KA_INTERVAL  ((60+BTTFN_KA_OFFSET)*1000)
#define BTTFN_DATA_TO          18600
#define BTTFN_TYPE_ANY     0    // Any, unknown or no device
#define BTTFN_TYPE_FLUX    1    // Flux Capacitor
#define BTTFN_TYPE_SID     2    // SID
#define BTTFN_TYPE_PCG     3    // Dash Gauges
#define BTTFN_TYPE_VSR     4    // VSR
#define BTTFN_TYPE_AUX     5    // Aux (user custom device)
#define BTTFN_TYPE_REMOTE  6    // Futaba remote control
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
#define BTTFN_NOT_SPD      15
#define BTTFN_NOT_INFO     16
#define BTTFN_NOT_DATA     128  // bit only, not value
#define BTTFN_REMCMD_KEEPALIVE 101
#define BTTFN_SSRC_NONE         0
#define BTTFN_SSRC_GPS          1
#define BTTFN_SSRC_ROTENC       2
#define BTTFN_SSRC_REM          3
#define BTTFN_SSRC_P0           4
#define BTTFN_SSRC_P1           5
#define BTTFN_SSRC_P2           6
#define BTTFN_TCDI1_NOREM   0x0001
#define BTTFN_TCDI1_NOREMKP 0x0002
#define BTTFN_TCDI1_EXT     0x0004
#define BTTFN_TCDI1_OFF     0x0008
#define BTTFN_TCDI1_NM      0x0010
#define BTTFN_TCDI2_BUSY    0x0001
static const uint8_t BTTFUDPHD[4] = { 'B', 'T', 'T', 'F' };
static bool          useBTTFN = false;
static WiFiUDP       bttfUDP;
static UDP*          vsrUDP;
static WiFiUDP       bttfMcUDP;
static UDP*          vsrMcUDP;
static byte          BTTFUDPBuf[BTTF_PACKET_SIZE];
static byte          BTTFUDPTBuf[BTTF_PACKET_SIZE];
static unsigned long BTTFNUpdateNow = 0;
static unsigned long bttfnVSRPollInt = BTTFN_POLL_INT;
static unsigned long BTTFNTSRQAge = 0;
static unsigned long BTTFNLastCmdSent = 0;
static bool          BTTFNPacketDue = false;
static bool          BTTFNWiFiUp = false;
static uint8_t       BTTFNfailCount = 0;
static uint32_t      BTTFUDPID = 0;
static unsigned long lastBTTFNpacket = 0;
static unsigned long lastBTTFNKA = 0;
static unsigned long bttfnLastNotData = 0;
static bool          BTTFNBootTO = false;
static bool          haveTCDIP = false;
static IPAddress     bttfnTcdIP;
static uint32_t      bttfnTCDSeqCnt = 0;
static uint32_t      bttfnTCDDataSeqCnt = 0;
static uint32_t      bttfnSessionID = 0;
static uint8_t       bttfnReqStatus = 0x56; // Request capabilities, status, temperature, speed
static bool          TCDSupportsNOTData = false;
static bool          bttfnDataNotEnabled = false;
static uint32_t      tcdHostNameHash = 0;
static byte          BTTFMCBuf[BTTF_PACKET_SIZE];
static IPAddress     bttfnMcIP(224, 0, 0, 224);
static uint32_t      bttfnSeqCnt = 1;

static int      iCmdIdx = 0;
static int      oCmdIdx = 0;
static uint32_t commandQueue[16] = { 0 };

#ifdef ESP32
/*  "warning: taking address of packed member of 'struct <anonymous>' may 
 *  result in an unaligned pointer value"
 *  "GCC will issue this warning when accessing an unaligned member of 
 *  a packed struct due to the incurred penalty of unaligned memory 
 *  access. However, all ESP chips (on both Xtensa and RISC-V 
 *  architectures) allow for unaligned memory access and incur no extra 
 *  penalty."
 *  https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32s3/migration-guides/release-5.x/5.0/gcc.html
 */
#define GET32(a,b)    *((uint32_t *)((a) + (b)))
#define SET32(a,b,c)  *((uint32_t *)((a) + (b))) = c
#else
#define GET32(a,b)          \
    (((a)[b])            |  \
    (((a)[(b)+1]) << 8)  |  \
    (((a)[(b)+2]) << 16) |  \
    (((a)[(b)+3]) << 24))   
#define SET32(a,b,c)                        \
    (a)[b]       = ((uint32_t)(c)) & 0xff;  \
    ((a)[(b)+1]) = ((uint32_t)(c)) >> 8;    \
    ((a)[(b)+2]) = ((uint32_t)(c)) >> 16;   \
    ((a)[(b)+3]) = ((uint32_t)(c)) >> 24; 
#endif

static void setTTOUT(uint8_t stat);

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
static void bttfn_loop_quick();
static bool bttfn_checkmc();
static void BTTFNCheckPacket();
static bool BTTFNSendRequest();
static void BTTFNPreparePacketTemplate();
static void BTTFNSendPacket();

void main_boot()
{
    
}

void main_boot2()
{
    // Init LED segment display
    int temp = 0;

    // Reset TT-OUT
    pinMode(TT_OUT_PIN, OUTPUT);
    digitalWrite(TT_OUT_PIN, LOW);

    if(!vsrdisplay.begin(temp)) {
        Serial.println("Display not found");
    } else {
        loadBrightness();
    }

    #ifdef VSR_DIAG
    vsrdisplay.lampTest();
    delay(10*1000);
        while(1);
    #endif
    #ifdef VSR_DIAG2
    while(1) {
        for(int i = 0; i < 10; i++) {
            char buf[8];
            sprintf(buf, "%d%d%d.", i, i, i);
            vsrdisplay.setText(buf);
            vsrdisplay.show();
            delay(300);
            sprintf(buf, "%d%d.%d", i, 9-i, i);
            vsrdisplay.setText(buf);
            vsrdisplay.show();
            delay(300);
            sprintf(buf, "%d.%d%d", 9-i, i, 9-i);
            vsrdisplay.setText(buf);
            vsrdisplay.show();
            delay(300);
        }
    }
    #endif

    showWaitSequence();
}

void main_setup()
{   
    Serial.println("Voltage Systems Regulator version " VSR_VERSION " " VSR_VERSION_EXTRA);

    updateConfigPortalBriValues();

    loadButtonMode();
    loadUDispMode();

    // Init button lights/LEDs
    vsrLEDs.begin(3);

    // Invoke audio file installer if SD content qualifies
    #ifdef VSR_DBG
    Serial.println("Probing for audio data on SD");
    #endif
    if(check_allow_CPA()) {
        showWaitSequence();
        if(prepareCopyAudioFiles()) {
            play_file("/_installing.mp3", PA_ALLOWSD, 1.0f);
            waitAudioDone();
        }
        doCopyAudioFiles();
        // We never return here. The ESP is rebooted.
    }

    smoothChg = (atoi(settings.smoothpw) > 0);
    doFluct = (atoi(settings.fluct) > 0);
    displayBM = (atoi(settings.displayBM) > 0);
    ssDelay = ssOrigDelay = atoi(settings.ssTimer) * 60 * 1000;

    diNmOff = (atoi(settings.diNmOff) > 0);
    vsrdisplay.setNMOff(diNmOff);

    ignTT = (atoi(settings.ignTT) > 0);
    playTTsounds = (atoi(settings.playTTsnds) > 0);

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
        Serial.println("Temperature sensor not detected");
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

    if(!haveAudioFiles) {
        #ifdef VSR_DBG
        Serial.println("Current audio data not installed");
        #endif
        vsrdisplay.on();
        vsrdisplay.setText("ISP");
        vsrdisplay.show();
        delay(1000);
        vsrdisplay.clearBuf();
        vsrdisplay.show();
    }

    // Init music player (don't check for SD here)
    switchMusicFolder(musFolderNum, true);

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
    Serial.println("main_setup() done");
    #endif

}

void main_loop()
{
    unsigned long now = millis();
    bool forceDispUpd = false;
    bool wheelsChanged = false;

    // Reset polling interval; will be overruled below if applicable
    bttfnVSRPollInt = BTTFN_POLL_INT;

    // Follow TCD fake power
    if(useFPO && (tcdFPO != fpoOld)) {
        if(tcdFPO) {
            // Power off:
            FPBUnitIsOn = false;

            // Stop musicplayer & audio in general
            mp_stop();
            stopAudio();
            
            if(TTrunning) {
                TTrunning = false;
                // Reset to idle
                vsrdisplay.setBrightness(255);
                // ...
            }
            TTrunningIOonly = false;
            setTTOUT(LOW);
            
            // Display OFF
            vsrdisplay.off();
            vsrLEDs.off();

            nmOverruled = false;

            flushDelayedSave();

            doPrepareTT = false;
            doWakeup = false;
            
            // FIXME - anything else?
            
        } else {
            // Power on: 
            FPBUnitIsOn = true;
            
            // Display ON, idle
            vsrdisplay.clearDisplay();
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

    // Eval flags set in handle_tcd_notification
    if(doPrepareTT) {
        if(FPBUnitIsOn && !ignTT && !TTrunning) {
            prepareTT();
        }
        doPrepareTT = false;
    }
    if(doWakeup) {
        if(FPBUnitIsOn && !TTrunning) {
            wakeup();
        }
        doWakeup = false;
    }

    // Execute remote commands from TCD or MQTT
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
        // Not ssEnd(), ss will only end if display in pw mode
    }

    // Handle pushwheel values
    if(wheelsChanged) {
        vsrControls.getValues(curra, currb, currc);
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
                if(TCDconnected || !bttfnTT || !bttfn_trigger_tt()) {
                    // stand-alone TT with P0_DUR lead, not ETTO_LEAD
                    timeTravel(TCDconnected, (TCDconnected && noETTOLead) ? 0 : (TCDconnected ? ETTO_LEAD : P0_DUR));
                }
            }
        }
    
        // Check for BTTFN/MQTT-induced TT
        if(networkTimeTravel) {
            networkTimeTravel = false;
            if(!networkAbort) {
                /*if(!ignTT)*/ ssEnd();
                timeTravel(networkTCDTT, networkLead, networkP1);
            }
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

                    if(now - TTstart >= TTFDelay) {
                        if(TTFInt) {
                            if(playTTsounds) {
                                play_file("/ttstart.mp3", PA_INTRMUS|PA_ALLOWSD, TT_V_LEV);
                                append_file("/humm.wav", PA_LOOP|PA_WAV|PA_INTRMUS|PA_ALLOWSD, TT_V_LEV);
                            }
                            TTFInt = 0;
                        }
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

                    if(!networkAbort) {

                        setTTOUT(HIGH);

                        // If we have missed P0, play it now
                        if(TTFInt) {
                            if(playTTsounds) {
                                play_file("/ttstart.mp3", PA_INTRMUS|PA_ALLOWSD, TT_V_LEV);
                                append_file("/humm.wav", PA_LOOP|PA_WAV|PA_INTRMUS|PA_ALLOWSD, TT_V_LEV);
                            }
                        }

                        vsrdisplay.setText("1.21");
                        vsrdisplay.show();

                        TTP0 = false;
                        TTP1 = true;
      
                        TTstart = now;
                        TTFInt = 1;
                        
                    } else {

                        // We were aborted: Handle sound gracefully, skip P1.

                        if(!TTFInt) {
                            // If "humm" is still pending, replace by ttend
                            // Otherwise interrupt "humm" by ttend
                            if(append_pending()) {
                                append_file("/ttend.mp3", PA_INTRMUS|PA_ALLOWSD, TT_V_LEV);
                            } else {
                                play_file("/ttend.mp3", PA_INTRMUS|PA_ALLOWSD, TT_V_LEV);
                            }
                        }

                        TTP0 = false;
                        TTP2 = true;

                        TTstart = now;
                      
                    }

                }
            }
            if(TTP1) {   // Peak/"time tunnel" - ends with pin going LOW or MQTT "REENTRY" (or a long timeout)

                if(((networkTCDTT && (!networkReentry && !networkAbort)) || 
                    (!networkTCDTT && digitalRead(TT_IN_PIN)))               &&
                    (millis() - TTstart <  P1_maxtimeout) ) {

                   // Wait...
                    
                } else {

                    TTP1 = false;
                    TTP2 = true;

                    setTTOUT(LOW);

                    if(playTTsounds) {
                        play_file("/ttend.mp3", PA_INTRMUS|PA_ALLOWSD, TT_V_LEV);
                    }

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
          
            if(TTP0) {   // Acceleration - runs for P0duration ms

                if(now - TTstart < P0duration) {

                    if(now - TTstart >= TTFDelay) {
                        if(TTFInt) {
                            if(playTTsounds) {
                                play_file("/ttstart.mp3", PA_INTRMUS|PA_ALLOWSD, TT_V_LEV);
                                append_file("/humm.wav", PA_LOOP|PA_WAV|PA_INTRMUS|PA_ALLOWSD, TT_V_LEV);
                            }
                            TTFInt = 0;
                        }
                    }

                } else {

                    TTP0 = false;
                    TTP1 = true;

                    setTTOUT(HIGH);

                    vsrdisplay.setText("1.21");
                    vsrdisplay.show();
                    
                    TTstart = now;
                    TTFInt = 1;
                    
                }
            }
            
            if(TTP1) {   // Peak/"time tunnel" - runs for P1_DUR ms

                if(now - TTstart < P1_DUR) {

                     // Wait...
                    
                } else {

                    setTTOUT(LOW);

                    TTP1 = false;
                    TTP2 = true; 

                    if(playTTsounds) {
                        play_file("/ttend.mp3", PA_INTRMUS|PA_ALLOWSD, TT_V_LEV);
                    }

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

        if(TTrunningIOonly) {
            if(TTP0) {
                if(networkAbort) {
                    TTP0 = false;
                    TTrunningIOonly = false;
                } else if(now - TTstart >= P0duration) {
                    setTTOUT(HIGH);
                    TTP0 = false;
                    TTP1 = true;
                    TTstart = now;
                }
            }
            if(TTP1) {
                if(networkReentry || networkAbort || (millis() - TTstart >= P1_maxtimeout)) {
                    setTTOUT(LOW);
                    TTrunningIOonly = false;
                }
            }
        }

        if(networkAlarm) {

            ssEnd();

            if(atoi(settings.playALsnd) > 0) {
                play_file("/alarm.mp3", PA_INTRMUS|PA_ALLOWSD|PA_DYNVOL, 1.0f);
            }

            if(!FPBUnitIsOn) {
                vsrdisplay.clearBuf();
                vsrdisplay.show();
                vsrdisplay.on();
            }

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
            
            if(!FPBUnitIsOn) {
                vsrdisplay.off();
            }

            networkAlarm = false;
        
        } else {

            // Wake up on RotEnc/Remote speed changes; on GPS only if old speed was <=0
            if(gpsSpeed != oldGpsSpeed) {
                if(FPBUnitIsOn && (spdIsRotEnc || oldGpsSpeed <= 0) && gpsSpeed >= 0) {
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

                    if((forceDispUpd = (dispMode == LDM_BM))) {
                        nmOverruled = false;
                    }

                    if(sysMsg && (now - sysMsgNow < sysMsgTimeout)) {

                        forceDispUpd |= (dispMode != LDM_SYS);
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
                  
                    if((forceDispUpd = (dispMode != LDM_BM))) {
                        if(useNM && diNmOff && vsrdisplay.getNightMode()) {
                            vsrdisplay.setNightMode(false);
                            nmOld = !tcdNM;  // Trigger update as soon as Overruled = false
                            nmOverruled = true;
                        }
                    }
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
                    if(!bttfnTCDSeqCnt) {
                        bttfnVSRPollInt = BTTFN_POLL_INT_FAST;
                    }
                    break;
                case LDM_TEMP:
                    #ifdef VSR_HAVETEMP
                    if(haveTempSens) {
                        temperature = tempSens.readLastTemp();
                    } else if(haveTCDTemp) {
                        float t = (float)TCDtemperature / 100.0f;
                        if(tempUnit) {
                            if(!TCDtempIsC) {
                                t = (t - 32.0f) * 5.0f / 9.0f;
                            }
                        } else {
                            if(TCDtempIsC) {
                                t = t * 9.0f / 5.0f + 32.0f;
                            }
                        }
                        temperature = t;
                    } else {
                        temperature = NAN;
                    }
                    #endif
                    if(forceDispUpd || 
                       (isnan(temperature) != isnan(prevTemperature)) || 
                       (temperature != prevTemperature)) {
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
    if(useNM && !nmOverruled && (tcdNM != nmOld)) {
        setNightMode(tcdNM);
        nmOld = tcdNM;
    }

    now = millis();
    
    // If network is interrupted, return to stand-alone
    if(useBTTFN) {
        if( !bttfnDataNotEnabled &&
            ((lastBTTFNpacket && (now - lastBTTFNpacket > 30*1000)) ||
             (!BTTFNBootTO && !lastBTTFNpacket && (now - powerupMillis > 60*1000))) ) {
            tcdNM = false;
            tcdFPO = false;
            gpsSpeed = -1;
            TCDtemperature = -32768;
            haveTCDTemp = false;
            lastBTTFNpacket = 0;
            BTTFNBootTO = true;
        }
    }

    if(!TTrunning) {
        // Save secondary settings 10 seconds after last change
        if(volchanged && (now - volchgnow > 10000)) {
            volchanged = false;
            saveCurVolume();
        } else if(brichanged && (now - brichgnow > 10000)) {
            brichanged = false;
            saveBrightness();
        } else if(butchanged && (now - butchgnow > 10000)) {
            butchanged = false;
            saveButtonMode();
        } else if(udispchanged && (now - udispchgnow > 10000)) {
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
    if(volchanged) {
        volchanged = false;
        saveCurVolume();
    }
    if(butchanged) {
        butchanged = false;
        saveButtonMode();
    }
    if(udispchanged) {
        udispchanged = false;
        saveUDispMode();
    }
}

static void chgVolume(int d)
{
    char buf[8];

    int nv = curSoftVol;
    nv += d;
    if(nv < 0 || nv > 19)
        nv = curSoftVol;
        
    curSoftVol = nv;

    sprintf(buf, "%3d", nv);
    displaySysMsg(buf, 1000);

    volchanged = true;
    volchgnow = millis();
}

void increaseVolume()
{
    chgVolume(1);
}

void decreaseVolume()
{
    chgVolume(-1);
}

/*
 * Time travel
 * 
 */

void timeTravel(bool TCDtriggered, uint16_t P0Dur, uint16_t P1Dur)
{
    if(TTrunning)
        return;

    flushDelayedSave();

    nmOverruled = false;

    setTTOUT(LOW);

    TTstart = millis();

    if(TCDtriggered && ignTT) {
        TTrunningIOonly = true;
    } else {
        TTrunning = true;
        TTrunningIOonly = false;
    }

    TTP0 = true;   // phase 0
    TTP1 = TTP2 = false;

    // P1Dur, even if coming from TCD, is not used for timing, 
    // but only to calculate steps and for a max timeout
    if(!P1Dur) {
        P1Dur = TCDtriggered ? P1_DUR_TCD : P1_DUR;
    }
    P1_maxtimeout = P1Dur + 3000;

    P0duration = P0Dur;
    
    if(TCDtriggered) {    // TCD-triggered TT (GPIO, BTTFN or MQTT) (synced with TCD)
      
        extTT = true;
        
        if(P0duration > RELAY_AHEAD) {
            TTFDelay = P0duration - RELAY_AHEAD;
        } else if(P0duration > 600) {
            TTFDelay = P0duration - 600;
        } else {
            TTFDelay = 0;
        }
        TTFInt = 1;
        
    } else {              // button-triggered TT (stand-alone)
      
        extTT = false;
        
        if(P0duration >= 600) {
            TTFDelay = P0_DUR - 600;
        } else {
            TTFDelay = 0;
        }
        
        TTFInt = 1;
                    
    }
    
    #ifdef VSR_DBG
    Serial.printf("P0 duration is %d\n", P0duration);
    Serial.printf("TTFDelay %d  TTFInt %d\n", TTFDelay, TTFInt);
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

    blockScan = true;

    play_file("/startup.mp3", PA_ALLOWSD, 1.0f);

    while(!checkAudioDone() || !j) {
        vsrdisplay.setText((char *)intro[i++]);
        vsrdisplay.show();
        myloop();
        delay(50);
        if(i > 9) { i = 0; j++; }
    }
    vsrdisplay.setText((char *)intro[10]);

    blockScan = false;
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

void allOff()
{
    vsrdisplay.off();
    vsrLEDs.off();
}

void prepareReboot()
{
    mp_stop();
    stopAudio();

    setTTOUT(LOW);
    
    allOff();
    
    flushDelayedSave();
    
    delay(500);
    unmount_fs();
    delay(100);
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

void showNumber(int num)
{
    char buf[8];
    sprintf(buf, "%3d", num);
    vsrdisplay.setText(buf);
    vsrdisplay.show();
    doForceDispUpd = true;
}

void prepareTT()
{
    ssEnd();

    doPrepareTT = false;
}

// Wakeup: Sent by TCD upon entering dest date,
// return from tt, triggering delayed tt via ETT
// For audio-visually synchronized behavior
// Also called when RotEnc speed is changed
void wakeup()
{
    // End screen saver
    ssEnd();

    doWakeup = false;
}

static void setTTOUT(uint8_t stat)
{
    digitalWrite(TT_OUT_PIN, stat);
    #ifdef VSR_DBG
    Serial.printf("Setting TT_OUT %d\n", stat);
    #endif
}

static void execute_remote_command()
{
    uint32_t command = commandQueue[oCmdIdx];
    bool     injected = false;

    // We are only called if ON
    // To check here:
    // No command execution during time travel
    // No command execution during timed sequences
    // ssActive checked by individual command

    if(!command || TTrunning)
        return;

    commandQueue[oCmdIdx] = 0;
    oCmdIdx++;
    oCmdIdx &= 0x0f;

    if(command & 0x80000000) {
        injected = true;
        command &= ~0x80000000;
        // Allow user to directly use TCD code
        if(command >= 8000 && command <= 8999) {
            command -= 8000;
        } else if(command >= 8000000 && command <= 8999999) {
            command -= 8000000;
        }
        if(!command) return;
    }

    if(command < 10) {                                // 800x
        switch(command) {
        case 1:                                       // 8001: play "key1.mp3"
            play_key(1);
            break;
        case 2:                                       // 8002: Prev song
            if(haveMusic) {
                mp_prev(mpActive);
            }
            break;
        case 3:                                       // 8003: play "key3.mp3"
            play_key(3);
            break;
        case 4:                                       // 8004: play "key4.mp3"
            play_key(4);
            break;
        case 5:                                       // 8005: Play/stop
            if(haveMusic) {
                if(mpActive) {
                    mp_stop();
                } else {
                    mp_play();
                }
            }
            break;
        case 6:                                       // 8006: Play "key6.mp3"
            play_key(6);
            break;
        case 7:                                       // 8007: play "key7.mp3"
            play_key(7);
            break;
        case 8:                                       // 8008: Next song
            if(haveMusic) {
                mp_next(mpActive);
            }
            break;
        case 9:                                       // 8009: play "key9.mp3"                   
            play_key(9);
            break;
        }
      
    } else if(command < 100) {                        // 80xx

        if(command >= 10 && command <= 19) {          // 8010-8012: Set display mode
            switch(command - 10) {
            case 0:
                userDispMode = LDM_WHEELS;
                ssEnd();
                break;
            case 1:
                userDispMode = LDM_TEMP;
                ssEnd();
                break;
            case 2:
                userDispMode = LDM_GPS;
                ssEnd();
                break;
            }

        } else if(command == 90) {                    // 8090: Display IP address

            flushDelayedSave();
            ssEnd();
            display_ip();
            ssRestartTimer();
          
        } else if(command >= 50 && command <= 59) {   // 8050-8059: Set music folder number
            
            if(haveSD) {
                ssEnd();
                switchMusicFolder((uint8_t)command - 50);
                ssRestartTimer();
            }
            
        }
        
    } else if (command < 1000) {                      // 8xxx

        if(command >= 300 && command <= 399) {

            command -= 300;                       // 8300-8319/8399: Set fixed volume level / enable knob
            if(command == 99) {
                // nada
            } else if(command <= 19) {
                curSoftVol = command;
                volchanged = true;
                volchgnow = millis();
            }

        } else if(command >= 400 && command <= 415) {

            command -= 400;                       // 8400-8415: Set brightness
            if(!TTrunning) {
                ssEnd();
                vsrdisplay.setBrightness(command);
                brichanged = true;
                brichgnow = millis();
                updateConfigPortalBriValues();
            }

        } else if(command >= 501 && command <= 509) {

            play_key(command - 500);              // 8501-8509: Play keyX

        } else {

            switch(command) {
            case 222:                             // 8222/8555 Disable/enable shuffle
            case 555:
                if(haveMusic) {
                    mp_makeShuffle((command == 555));
                }
                break;
            case 888:                             // 8888 go to song #0
                if(haveMusic) {
                    mp_gotonum(0, mpActive);
                }
                break;
            }
        }

    } else if(command < 10000) {                  // 1000-9999 MQTT commands

        // All below only when we're ON
        // Commands allowed while off must be handled in mqttCallback()

        #ifdef VSR_HAVEMQTT
        if(!injected) {

            command -= 1000;
    
            switch(command) {
            case 0:
                // Trigger stand-alone Time Travel
                // We have no "acceleration", hence P0_DUR, not ETTO_LEAD
                ssEnd();
                timeTravel(false, P0_DUR);    
                break;
            case 7:    
                if(haveMusic) mp_play();
                break;
            case 8:
                if(haveMusic && mpActive) {
                    mp_stop();
                }
                break;
            case 9:
                if(haveMusic) mp_next(mpActive);
                break;
            case 10:
                if(haveMusic) mp_prev(mpActive);
                break;
            case 13:
                stop_key();
                break;
            }

        }
        #endif
        
    } else {
      
        switch(command) {
        case 64738:                               // 8064738: reboot
            if(!injected) {
                prepareReboot();
                delay(500);
                esp_restart();
            }
            break;
        case 123456:
            flushDelayedSave();
            if(!injected) {
                deleteIpSettings();               // 8123456: deletes IP settings
                if(settings.appw[0]) {
                    settings.appw[0] = 0;         // and clears AP mode WiFi password
                    write_settings();
                }
                ssRestartTimer();
            }
            break;
        default:                                  // 8888xxx: goto song #xxx
            if((command / 1000) == 888) {
                uint16_t num = command - 888000;
                num = mp_gotonum(num, mpActive);
            }
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

    blockScan = true;

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

    blockScan = false;

    doForceDispUpd = true;
}

bool switchMusicFolder(uint8_t nmf, bool isSetup)
{
    bool waitShown = false;

    if(nmf > 9) return false;

    vsrBusy = true;
    
    if((musFolderNum != nmf) || isSetup) {

        blockScan = true;
        
        if(!isSetup) {
            musFolderNum = nmf;
            // Initializing the MP can take a while;
            // need to stop all audio before calling
            // mp_init()
            if(haveMusic && mpActive) {
                mp_stop();
            }
            stopAudio();
        }
        if(haveSD) {
            if(mp_checkForFolder(musFolderNum) == -1) {
                if(!isSetup) flushDelayedSave();
                showWaitSequence();
                waitShown = true;
                play_file("/renaming.mp3", PA_INTRMUS|PA_ALLOWSD);
                waitAudioDone();
            }
        }
        if(!isSetup) {
            saveMusFoldNum();
            updateConfigPortalMFValues();
        }
        mp_init(isSetup);
        if(waitShown) {
            endWaitSequence();
        }

        blockScan = false;
    }

    vsrBusy = false;

    return waitShown;
}

void waitAudioDone()
{
    int timeout = 400;

    while(!checkAudioDone() && timeout--) {
        mydelay(10);
    }
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
    unsigned long tui = tempUpdInt;
    
    if(!haveTempSens)
        return;

    if(tempSens.lastTempNan() && (millis() - powerupMillis < 15*1000)) {
        tui = 5 * 1000;
    }
    
    if(force || (millis() - tempReadNow >= tui)) {
        tempSens.readTemp(tempUnit);
        tempReadNow = millis();
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
    bttfn_loop_quick();
}

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

static bool check_packet(uint8_t *buf)
{
    // Basic validity check
    if(memcmp(buf, BTTFUDPHD, 4))
        return false;

    uint8_t a = 0;
    for(int i = 4; i < BTTF_PACKET_SIZE - 1; i++) {
        a += buf[i] ^ 0x55;
    }

    return (buf[BTTF_PACKET_SIZE - 1] == a);
}

void addCmdQueue(uint32_t command)
{
    if(!command) return;

    commandQueue[iCmdIdx] = command;
    iCmdIdx++;
    iCmdIdx &= 0x0f;
}

static void bttfn_eval_response(uint8_t *buf, bool checkCaps)
{
    if(checkCaps && (buf[5] & 0x40)) {
        bttfnReqStatus &= ~0x40;     // Do no longer poll capabilities
        if(buf[31] & 0x01) {
            bttfnReqStatus &= ~0x02; // Do no longer poll speed, comes over multicast
        }
        if(buf[31] & 0x10) {
            TCDSupportsNOTData = true;
        }
    }
    
    if(buf[5] & 0x02) {
        gpsSpeed = (int16_t)(buf[18] | (buf[19] << 8));
        if(gpsSpeed > 88) gpsSpeed = 88;
        spdIsRotEnc = !!(buf[26] & (0x80|0x20));    // Speed is from RotEnc or Remote
    }

    if(buf[5] & 0x04) {
        TCDtemperature = (int16_t)(buf[20] | (buf[21] << 8));
        haveTCDTemp = (TCDtemperature != -32768);
        TCDtempIsC = !!(buf[26] & 0x40);
    } else {
        haveTCDTemp = false;
    }

    if(buf[5] & 0x10) {
        tcdNM  = !!(buf[26] & 0x01);
        tcdFPO = !!(buf[26] & 0x02);
        tcdIsBusy = !!(buf[26] & 0x10); 
    } else {
        tcdNM = false;
        tcdFPO = false;
    }
}

static void handle_tcd_notification(uint8_t *buf)
{
    uint32_t seqCnt;

    // Note: This might be called while we are in a
    // wait-delay-loop. Best to just set flags here
    // that are evaluated synchronously (=later).
    // Do not stuff that messes with display, input,
    // etc.

    if(buf[5] & BTTFN_NOT_DATA) {
        if(TCDSupportsNOTData) {
            bttfnDataNotEnabled = true;
            bttfnLastNotData = millis();
            seqCnt = GET32(buf, 27);
            if(bttfnSessionID && (bttfnSessionID != seqCnt)) {
                lastBTTFNKA = bttfnLastNotData - BTTFN_KA_INTERVAL + (BTTFN_KA_OFFSET*1000);
                bttfnTCDDataSeqCnt = 1;
            }
            bttfnSessionID = seqCnt;
            seqCnt = GET32(buf, 6);
            if(seqCnt > bttfnTCDDataSeqCnt || seqCnt == 1) {
                #ifdef VSR_DBG
                Serial.println("Valid NOT_DATA packet received");
                #endif
                bttfn_eval_response(buf, false);
            } else {
                #ifdef VSR_DBG
                Serial.printf("Out-of-sequence NOT_DATA packet received %d %d\n", seqCnt, bttfnTCDDataSeqCnt);
                #endif
            }
            bttfnTCDDataSeqCnt = seqCnt;
        }
        return;
    }
    
    switch(buf[5]) {
    case BTTFN_NOT_SPD:
        seqCnt = GET32(buf, 12);
        if(seqCnt > bttfnTCDSeqCnt || seqCnt == 1) {
            switch(buf[8] | (buf[9] << 8)) {
            case BTTFN_SSRC_GPS:
                spdIsRotEnc = false;
                break;
            case BTTFN_SSRC_P1:
                // If packets come out-of-order, we might
                // get this one before TTrunning, and we
                // don't want a switch to usingGPSS only 
                // because of P1 speed
                if(!TTrunning) return;
                // fall through
            default:
                spdIsRotEnc = true;
            }
            gpsSpeed = (int16_t)(buf[6] | (buf[7] << 8));
            if(gpsSpeed > 88) gpsSpeed = 88;
            #ifdef VSR_DBG
            Serial.printf("TCD sent speed %d\n", gpsSpeed);
            #endif
        } else {
            #ifdef VSR_DBG
            Serial.printf("Out-of-sequence packet received from TCD %d %d\n", seqCnt, bttfnTCDSeqCnt);
            #endif
        }
        bttfnTCDSeqCnt = seqCnt;
        break;
    case BTTFN_NOT_PREPARE:
        // Prepare for TT. Comes at some undefined point,
        // an undefined time before the actual tt, and
        // may not come at all.
        // We disable our Screen Saver.
        // We don't ignore this if TCD is connected by wire,
        // because this signal does not come via wire.
        doPrepareTT = true;
        break;
    case BTTFN_NOT_TT:
        // Trigger Time Travel (if not running already)
        // Ignore command if TCD is connected by wire
        if(!TCDconnected && !TTrunning && !TTrunningIOonly && !vsrBusy) {
            networkLead = buf[6] | (buf[7] << 8);
            networkP1 = buf[8] | (buf[9] << 8);
            networkReentry = false;
            networkAbort = false;
            networkTimeTravel = true;
            networkTCDTT = true;
        }
        break;
    case BTTFN_NOT_REENTRY:
        // Start re-entry (if TT currently running)
        // Ignore command if TCD is connected by wire
        if(!TCDconnected && (TTrunning || TTrunningIOonly || networkTimeTravel) && networkTCDTT) {
            networkReentry = true;
        }
        break;
    case BTTFN_NOT_ABORT_TT:
        // Abort TT (if TT currently running)
        // Ignore command if TCD is connected by wire
        if(!TCDconnected && (TTrunning || TTrunningIOonly || networkTimeTravel) && networkTCDTT) {
            networkAbort = true;
        }
        break;
    case BTTFN_NOT_ALARM:
        networkAlarm = true;
        break;
    case BTTFN_NOT_VSR_CMD:
        if(!vsrBusy) {
            addCmdQueue(GET32(buf, 6));
        }
        break;
    case BTTFN_NOT_WAKEUP:
        doWakeup = true;
        break;
    case BTTFN_NOT_INFO:
        {
            uint16_t tcdi1 = buf[6] | (buf[7] << 8);
            uint16_t tcdi2 = buf[8] | (buf[9] << 8);
            if(tcdi1 & BTTFN_TCDI1_EXT) {
                tcdNM  = !!(tcdi1 & BTTFN_TCDI1_NM);
                tcdFPO = !!(tcdi1 & BTTFN_TCDI1_OFF);
            }
            tcdIsBusy = !!(tcdi2 & BTTFN_TCDI2_BUSY);
        }
        break;
    }
}

// Check for pending MC packet and parse it
static bool bttfn_checkmc()
{
    int psize = vsrMcUDP->parsePacket();

    if(!psize) {
        return false;
    }

    // This returns true as long as a packet was received
    // regardless whether it was for us or not. Point is
    // to clear the receive buffer.
    
    vsrMcUDP->read(BTTFMCBuf, BTTF_PACKET_SIZE);

    #ifdef VSR_DBG
    Serial.printf("Received multicast packet from %s\n", vsrMcUDP->remoteIP().toString());
    #endif

    if(haveTCDIP) {
        if(bttfnTcdIP != vsrMcUDP->remoteIP())
            return true;
    } else {
        // Do not use tcdHostNameHash; let DISCOVER do its work
        // and wait for a result.
        return true;
    }

    if(!check_packet(BTTFMCBuf))
        return true;

    if((BTTFMCBuf[4] & 0x4f) == (BTTFN_VERSION | 0x40)) {

        // A notification from the TCD
        handle_tcd_notification(BTTFMCBuf);
    
    }

    return true;
}

// Check for pending packet and parse it
static void BTTFNCheckPacket()
{
    unsigned long mymillis = millis();
    
    int psize = vsrUDP->parsePacket();
    if(!psize) {
        if(!bttfnDataNotEnabled && BTTFNPacketDue) {
            if((mymillis - BTTFNTSRQAge) > BTTFN_RESPONSE_TO) {
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

    if(!check_packet(BTTFUDPBuf))
        return;

    if((BTTFUDPBuf[4] & 0x4f) == (BTTFN_VERSION | 0x40)) {

        // A notification from the TCD
        handle_tcd_notification(BTTFUDPBuf);
      
    } else {

        // (Possibly) a response packet
    
        if(GET32(BTTFUDPBuf, 6) != BTTFUDPID)
            return;
    
        // Response marker missing or wrong version, bail
        if((BTTFUDPBuf[4] & 0x8f) != (BTTFN_VERSION | 0x80))
            return;

        BTTFNfailCount = 0;
    
        // If it's our expected packet, no other is due for now
        BTTFNPacketDue = false;

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

        // TCD did register us, so use current millis as
        // baseline for KEEP_ALIVE (lastBTTFNKA)
        lastBTTFNpacket = lastBTTFNKA = mymillis;

        bttfn_eval_response(BTTFUDPBuf, true);
    }
}

static void BTTFNPreparePacketTemplate()
{
    memset(BTTFUDPTBuf, 0, BTTF_PACKET_SIZE);

    // ID
    memcpy(BTTFUDPTBuf, BTTFUDPHD, 4);

    // Tell the TCD about our hostname
    // 13 bytes total. If hostname is longer, last in buf is '.'
    memcpy(BTTFUDPTBuf + 10, settings.hostName, 13);
    if(strlen(settings.hostName) > 13) BTTFUDPTBuf[10+12] = '.';

    BTTFUDPTBuf[10+13] = BTTFN_TYPE_VSR;

    // Version, MC-marker, ND-marker
    BTTFUDPTBuf[4] = BTTFN_VERSION | BTTFN_SUP_MC | BTTFN_SUP_ND;
}

static void BTTFNPreparePacket()
{
    memcpy(BTTFUDPBuf, BTTFUDPTBuf, BTTF_PACKET_SIZE);               
}

static void BTTFNDispatch()
{
    uint8_t a = 0;
    for(int i = 4; i < BTTF_PACKET_SIZE - 1; i++) {
        a += BTTFUDPBuf[i] ^ 0x55;
    }
    BTTFUDPBuf[BTTF_PACKET_SIZE - 1] = a;

    if(haveTCDIP) {
        vsrUDP->beginPacket(bttfnTcdIP, BTTF_DEFAULT_LOCAL_PORT);       
    } else {
        #ifdef VSR_DBG
        Serial.printf("Sending multicast (hostname hash %x)\n", tcdHostNameHash);
        #endif
        vsrUDP->beginPacket(bttfnMcIP, BTTF_DEFAULT_LOCAL_PORT + 1);
    }
    vsrUDP->write(BTTFUDPBuf, BTTF_PACKET_SIZE);
    vsrUDP->endPacket();
}

// Send a new data request
static bool BTTFNSendRequest()
{
    BTTFNPacketDue = false;

    BTTFNUpdateNow = millis();

    if(WiFi.status() != WL_CONNECTED) {
        BTTFNWiFiUp = false;
        return false;
    }

    BTTFNWiFiUp = true;

    // Send new packet
    BTTFNPreparePacket();
    
    // Serial
    BTTFUDPID = (uint32_t)millis();
    SET32(BTTFUDPBuf, 6, BTTFUDPID);

    // Request status, temperature, speed
    BTTFUDPBuf[5] = bttfnReqStatus;

    if(!haveTCDIP) {
        BTTFUDPBuf[5] |= 0x80;
        SET32(BTTFUDPBuf, 31, tcdHostNameHash);
    }

    BTTFNDispatch();

    BTTFNTSRQAge = millis();
    
    BTTFNPacketDue = true;
    
    return true;
}

static bool bttfn_connected()
{
    if(!useBTTFN)
        return false;

    if(!haveTCDIP)
        return false;

    if(WiFi.status() != WL_CONNECTED)
        return false;

    if(!lastBTTFNpacket)
        return false;

    return true;
}

bool bttfn_trigger_tt()
{
    if(!bttfn_connected())
        return false;

    if(TTrunning || tcdIsBusy)
        return false;

    BTTFNPreparePacket();

    // Trigger BTTFN-wide TT
    BTTFUDPBuf[5] = 0x80;

    BTTFNDispatch();

    return true;
}

static bool bttfn_send_command(uint8_t cmd, uint8_t p1, uint8_t p2)
{
    if(cmd != BTTFN_REMCMD_KEEPALIVE)
        return false;
        
    if(!bttfn_connected())
        return false;

    BTTFNPreparePacket();
    
    //BTTFUDPBuf[5] = 0x00; // 0 already

    SET32(BTTFUDPBuf, 6, bttfnSeqCnt);         // Seq counter
    bttfnSeqCnt++;
    if(!bttfnSeqCnt) bttfnSeqCnt++;

    BTTFUDPBuf[25] = cmd;                      // Cmd + parms
    BTTFUDPBuf[26] = p1;
    BTTFUDPBuf[27] = p2;

    BTTFNDispatch();

    #ifdef VSR_DBG
    Serial.printf("Sent command %d\n", cmd);
    #endif

    BTTFNLastCmdSent = millis();

    return true;
}

static void bttfn_setup()
{
    useBTTFN = false;

    // string empty? Disable BTTFN.
    if(!settings.tcdIP[0])
        return;

    haveTCDIP = isIp(settings.tcdIP);
    
    if(!haveTCDIP) {
        tcdHostNameHash = 0;
        unsigned char *s = (unsigned char *)settings.tcdIP;
        for ( ; *s; ++s) tcdHostNameHash = 37 * tcdHostNameHash + tolower(*s);
    } else {
        bttfnTcdIP.fromString(settings.tcdIP);
    }
    
    vsrUDP = &bttfUDP;
    vsrUDP->begin(BTTF_DEFAULT_LOCAL_PORT);

    vsrMcUDP = &bttfMcUDP;
    vsrMcUDP->beginMulticast(bttfnMcIP, BTTF_DEFAULT_LOCAL_PORT + 2);

    BTTFNPreparePacketTemplate();
    
    BTTFNfailCount = 0;
    useBTTFN = true;
}

void bttfn_loop()
{
    if(!useBTTFN)
        return;

    int t = 100;
    
    while(bttfn_checkmc() && t--) {}

    unsigned long now = millis();

    BTTFNCheckPacket();   
    
    if(bttfnDataNotEnabled) {
        if(now - lastBTTFNKA > BTTFN_KA_INTERVAL) {
            if(!BTTFNLastCmdSent || (now - BTTFNLastCmdSent > (BTTFN_KA_INTERVAL/2))) {
                bttfn_send_command(BTTFN_REMCMD_KEEPALIVE, 0, 0);
            }
            BTTFNLastCmdSent = 0;
            do {
                lastBTTFNKA += BTTFN_KA_INTERVAL;
            } while(now - lastBTTFNKA >= BTTFN_KA_INTERVAL);
        }
        if(now - bttfnLastNotData > BTTFN_DATA_TO) {
            // Return to polling if no NOT_DATA for too long
            bttfnDataNotEnabled = false;
            bttfnTCDDataSeqCnt = 1;
            // Re-do DISCOVER, TCD might have got new IP address
            if(tcdHostNameHash) {
                haveTCDIP = false;
            }
            #ifdef VSR_DBG
            Serial.println("NOT_DATA timeout, returning to polling");
            #endif
        }
    } else if(!BTTFNPacketDue) {
        // If WiFi status changed, trigger immediately
        if(!BTTFNWiFiUp && (WiFi.status() == WL_CONNECTED)) {
            BTTFNUpdateNow = 0;
        }
        if((!BTTFNUpdateNow) || (millis() - BTTFNUpdateNow > bttfnVSRPollInt)) {
            BTTFNSendRequest();
        }
    }
}

static void bttfn_loop_quick()
{
    if(!useBTTFN)
        return;

    int t = 100;
    
    while(bttfn_checkmc() && t--) {}
}
