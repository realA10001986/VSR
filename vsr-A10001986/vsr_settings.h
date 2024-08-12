/*
 * -------------------------------------------------------------------
 * Voltage Systems Regulator
 * (C) 2024 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/VSR
 * https://vsr.out-a-ti.me
 *
 * Settings handling
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

#ifndef _VSR_SETTINGS_H
#define _VSR_SETTINGS_H

extern bool haveFS;
extern bool haveSD;
extern bool FlashROMode;

extern bool haveAudioFiles;

extern uint8_t musFolderNum;

#define MS(s) XMS(s)
#define XMS(s) #s

// Default settings

#define DEF_SMOOTHPW        1     // Smooth pushwheel changes
#define DEF_FLUCT           0     // Voltage fluctuations 1=on, 0=off
#define DEF_DISP_BM         1     // Show button mode upon power-up: 1=yes, 0=no
#define DEF_SIG_BM          0     // Signal button mode by permanent button lights
#define DEF_SS_TIMER        0     // "Screen saver" timeout in minutes; 0 = ss off

#define DEF_HOSTNAME        "vsr"
#define DEF_WIFI_RETRY      3     // 1-10; Default: 3 retries
#define DEF_WIFI_TIMEOUT    7     // 7-25; Default: 7 seconds

#define DEF_DI_NM           0     // Night mode: display dimmed(0) or off(1)

#define DEF_TEMP_UNIT       0     // Temperature unit: Fahrenheit (0=default) or Celsius (1)
#define DEF_TEMP_OFFS       0.0   // Temperature offset: Default 0.0

#define DEF_TCD_PRES        0     // 0: No TCD connected, 1: connected via GPIO
#define DEF_NO_ETTO_LEAD    0     // 0: TCD signals TT with ETTO_LEAD lead time; 1 without

#define DEF_TCD_IP          ""    // TCD ip address for networked polling
#define DEF_USE_NM          0     // 0: Ignore TCD night mode; 1: Follow TCD night mode
#define DEF_USE_FPO         0     // 0: Ignore TCD fake power; 1: Follow TCD fake power

#define DEF_BTTFN_TT        1     // 0: "9" (in OPR-mode) and TT button trigger stand-alone TT; 1: They trigger BTTFN-wide TT

#define DEF_PLAY_TT_SND     1     // 1: Play time travel sounds (0: Do not; for use with external equipment)
#define DEF_PLAY_ALM_SND    0     // 1: Play TCD-alarm sound, 0: do not

#define DEF_SHUFFLE         0     // Music Player: Do not shuffle by default

#define DEF_CFG_ON_SD       1     // Save secondary settings on SD card. Default: Yes (1)
#define DEF_SD_FREQ         0     // SD/SPI frequency: Default 16MHz

#define DEF_BRI             15    // Default display brightness

struct Settings {
    char smoothpw[4]        = MS(DEF_SMOOTHPW);
    char fluct[4]           = MS(DEF_FLUCT);
    char displayBM[4]       = MS(DEF_DISP_BM);
    char signalBM[4]        = MS(DEF_SIG_BM);
    char ssTimer[6]         = MS(DEF_SS_TIMER);

    char hostName[32]       = DEF_HOSTNAME;
    char systemID[8]        = "";
    char appw[10]           = "";
    char wifiConRetries[4]  = MS(DEF_WIFI_RETRY);
    char wifiConTimeout[4]  = MS(DEF_WIFI_TIMEOUT);

    char diNmOff[4]         = MS(DEF_DI_NM);

    char tempUnit[4]        = MS(DEF_TEMP_UNIT);
    #ifdef VSR_HAVETEMP
    char tempOffs[6]        = MS(DEF_TEMP_OFFS);
    #endif
    
    char TCDpresent[4]      = MS(DEF_TCD_PRES);
    char noETTOLead[4]      = MS(DEF_NO_ETTO_LEAD);

    char tcdIP[16]          = DEF_TCD_IP;
    char useNM[4]           = MS(DEF_USE_NM);
    char useFPO[4]          = MS(DEF_USE_FPO);
    char bttfnTT[4]         = MS(DEF_BTTFN_TT);
        
    char playTTsnds[4]      = MS(DEF_PLAY_TT_SND);
    char playALsnd[4]       = MS(DEF_PLAY_ALM_SND);

#ifdef VSR_HAVEMQTT  
    char useMQTT[4]         = "0";
    char mqttServer[80]     = "";  // ip or domain [:port]  
    char mqttUser[128]      = "";  // user[:pass] (UTF8)
#endif     

    char shuffle[4]         = MS(DEF_SHUFFLE);

    char CfgOnSD[4]         = MS(DEF_CFG_ON_SD);
    char sdFreq[4]          = MS(DEF_SD_FREQ);

#ifdef HAVE_DISPSELECTION
    char dispType[6]        = MS(VSR_DISP_MIN_TYPE);
#endif
   
    char Vol[6];
    char musicFolder[6];
    char Bri[6];
};

struct IPSettings {
    char ip[20]       = "";
    char gateway[20]  = "";
    char netmask[20]  = "";
    char dns[20]      = "";
};

extern struct Settings settings;
extern struct IPSettings ipsettings;

void settings_setup();

void unmount_fs();

void write_settings();
bool checkConfigExists();

bool loadBrightness();
void saveBrightness(bool useCache = true);

bool loadButtonMode();
void saveButtonMode(bool useCache = true);

bool loadUDispMode();
void saveUDispMode(bool useCache = true);

#ifdef VSR_HAVEAUDIO
bool loadCurVolume();
void saveCurVolume(bool useCache = true);

bool loadMusFoldNum();
void saveMusFoldNum();
#endif

bool loadIpSettings();
void writeIpSettings();
void deleteIpSettings();

void copySettings();

#ifdef VSR_HAVEAUDIO
bool check_if_default_audio_present();
bool prepareCopyAudioFiles();
void doCopyAudioFiles();

bool check_allow_CPA();
void delete_ID_file();
#endif

#ifdef VSR_HAVEAUDIO
#include <FS.h>
bool openACFile(File& file);
size_t writeACFile(File& file, uint8_t *buf, size_t len);
void closeACFile(File& file);
void removeACFile();
#endif

#endif
