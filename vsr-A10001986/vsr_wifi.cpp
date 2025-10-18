/*
 * -------------------------------------------------------------------
 * Voltage Systems Regulator
 * (C) 2024-2025 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/VSR
 * https://vsr.out-a-ti.me
 *
 * WiFi and Config Portal handling
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

#include "src/WiFiManager/WiFiManager.h"

#ifndef WM_MDNS
#define VSR_MDNS
#include <ESPmDNS.h>
#endif

#include "vsrdisplay.h"
#include "vsr_audio.h"
#include "vsr_settings.h"
#include "vsr_wifi.h"
#include "vsr_main.h"
#ifdef VSR_HAVEMQTT
#include "mqtt.h"
#endif

#define STRLEN(x) (sizeof(x)-1)

Settings settings;

IPSettings ipsettings;

WiFiManager wm;
bool wifiSetupDone = false;

#ifdef VSR_HAVEMQTT
WiFiClient mqttWClient;
PubSubClient mqttClient(mqttWClient);
#endif

static const char R_updateacdone[] = "/uac";

static const char acul_part3[]  = "</head><body><div class='wrap'><h1>";
static const char acul_part5[]  = "</h1><h3>";
static const char acul_part6[]  = "</h3><div id='lc' class='msg";
static const char acul_part7[]  = " S'><strong>Upload successful.</strong><br/>Device rebooting.";
static const char acul_part7a[] = "<br>Installation will proceed after reboot.";
static const char acul_part71[] = " D'><strong>Upload failed.</strong><br>";
static const char acul_part8[]  = "</div></div></body></html>";
static const char *acul_errs[]  = { 
    "Can't open file on SD",
    "No SD card found",
    "Write error",
    "Bad file",
    "Not enough memory",
    "Unrecognized type",
    "Extraneous .bin file"
};

static const char *apChannelCustHTMLSrc[14] = {
    "<div class='cmp0'><label for='apchnl'>WiFi channel</label><select class='sel0' value='",
    "apchnl",
    ">Random%s1'",
    ">1%s2'",
    ">2%s3'",
    ">3%s4'",
    ">4%s5'",
    ">5%s6'",
    ">6%s7'",
    ">7%s8'",
    ">8%s9'",
    ">9%s10'",
    ">10%s11'",
    ">11%s"
};

static const char *wmBuildApChnl(const char *dest);
static const char *wmBuildBestApChnl(const char *dest);
static const char *wmBuildHaveSD(const char *dest);

static const char *osde = "</option></select></div>";
static const char *ooe  = "</option><option value='";
static const char custHTMLSel[] = " selected";

static const char *aco = "autocomplete='off'";

// double-% since this goes through sprintf!
static const char bestAP[]   = "<div class='c' style='background-color:#%s;color:#fff;font-size:80%%;border-radius:5px'>Proposed channel at current location: %d<br>%s(Non-WiFi devices not taken into account)</div>";
static const char badWiFi[]  = "<br><i>Operating in AP mode not recommended</i>";

static const char haveNoSD[] = "<div class='c' style='background-color:#dc3630;color:#fff;font-size:80%;border-radius:5px'><i>No SD card present</i></div>";

// WiFi Configuration

#if defined(VSR_MDNS) || defined(VSR_WM_HAS_MDNS)
#define HNTEXT "Hostname<br><span style='font-size:80%'>The Config Portal is accessible at http://<i>hostname</i>.local<br>(Valid characters: a-z/0-9/-)</span>"
#else
#define HNTEXT "Hostname<br><span style='font-size:80%'>(Valid characters: a-z/0-9/-)</span>"
#endif
WiFiManagerParameter custom_hostName("hostname", HNTEXT, settings.hostName, 31, "pattern='[A-Za-z0-9\\-]+' placeholder='Example: vsr'");
WiFiManagerParameter custom_wifiConRetries("wifiret", "Connection attempts (1-10)", settings.wifiConRetries, 2, "type='number' min='1' max='10' autocomplete='off'", WFM_LABEL_BEFORE);
WiFiManagerParameter custom_wifiConTimeout("wificon", "Connection timeout (7-25[seconds])", settings.wifiConTimeout, 2, "type='number' min='7' max='25'");

WiFiManagerParameter custom_sysID("sysID", "Network name (SSID) appendix<br><span style='font-size:80%'>Will be appended to \"VSR-AP\" to create a unique SSID if multiple VSRs are in range. [a-z/0-9/-]</span>", settings.systemID, 7, "pattern='[A-Za-z0-9\\-]+'");
WiFiManagerParameter custom_appw("appw", "Password<br><span style='font-size:80%'>Password to protect VSR-AP. Empty or 8 characters [a-z/0-9/-]<br><b>Write this down, you might lock yourself out!</b></span>", settings.appw, 8, "minlength='8' pattern='[A-Za-z0-9\\-]+'");
WiFiManagerParameter custom_apch(wmBuildApChnl);
WiFiManagerParameter custom_bapch(wmBuildBestApChnl);

// Settings

WiFiManagerParameter custom_aood("<div class='msg P'>Please <a href='/update'>install/update</a> sound pack</div>");

WiFiManagerParameter custom_smoothpw("smp", "Smooth voltage changes", settings.smoothpw, 1, "autocomplete='off' title='Check this to adapt display smoothly to pushwheel values' type='checkbox' class='mt5'", WFM_LABEL_AFTER);
WiFiManagerParameter custom_fluct("flu", "Voltage fluctuations", settings.fluct, 1, "autocomplete='off' title='Check this to show random voltage fluctuations' type='checkbox'", WFM_LABEL_AFTER);
WiFiManagerParameter custom_displayBM("dbm", "Display button mode on power-up", settings.displayBM, 1, "autocomplete='off' type='checkbox'", WFM_LABEL_AFTER);
WiFiManagerParameter custom_signalBM("sbm", "Lights indicate button mode", settings.signalBM, 1, "autocomplete='off' type='checkbox'", WFM_LABEL_AFTER);
WiFiManagerParameter custom_playTTSnd("plyTTS", "Play time travel sounds", settings.playTTsnds, 1, "autocomplete='off' title='Check to have the device play time travel sounds. Uncheck if other props provide time travel sound.' type='checkbox'", WFM_LABEL_AFTER);
WiFiManagerParameter custom_playALSnd("plyALS", "Play TCD-alarm sound", settings.playALsnd, 1, "autocomplete='off' title='Check to have the device play a sound then the TCD alarm sounds.' type='checkbox' class='mb10'", WFM_LABEL_AFTER);
WiFiManagerParameter custom_Bri("Bri", "Brightness level (0-15)", settings.Bri, 2, "type='number' min='0' max='15' autocomplete='off'", WFM_LABEL_BEFORE);
WiFiManagerParameter custom_ssDelay("ssDel", "Screen saver timer (minutes; 0=off)", settings.ssTimer, 3, "type='number' min='0' max='999' autocomplete='off'", WFM_LABEL_BEFORE);

#ifdef VSR_HAVEVOLKNOB
WiFiManagerParameter custom_FixV("FixV", "Disable volume knob", settings.FixV, 1, "autocomplete='off' title='Check this if the audio volume should be set by software; if unchecked, the volume knob is used' type='checkbox' class='mt5'", WFM_LABEL_AFTER);
WiFiManagerParameter custom_Vol("Vol", "Software volume level (0-19)", settings.Vol, 2, "type='number' min='0' max='19' autocomplete='off'", WFM_LABEL_BEFORE);
#else
WiFiManagerParameter custom_Vol("Vol", "Volume level (0-19)", settings.Vol, 2, "type='number' min='0' max='19' autocomplete='off'", WFM_LABEL_BEFORE);
#endif

WiFiManagerParameter custom_musicFolder("mfol", "Music folder (0-9)", settings.musicFolder, 2, "type='number' min='0' max='9'");
WiFiManagerParameter custom_shuffle("musShu", "Shuffle mode enabled at startup", settings.shuffle, 1, "type='checkbox' class='mt5'", WFM_LABEL_AFTER);

WiFiManagerParameter custom_diNmOff("dinMOff", "Display off", settings.diNmOff, 1, "title='Dimmed if unchecked' type='checkbox' class='mt5'", WFM_LABEL_AFTER);

WiFiManagerParameter custom_tempUnit("tUnt", "Show temperature in °Celsius", settings.tempUnit, 1, "title='Fahrenheit if unchecked' type='checkbox' class='mt5 mb10'", WFM_LABEL_AFTER);
#ifdef VSR_HAVETEMP
WiFiManagerParameter custom_tempOffs("tOffs", "Sensor offset (-3.0-3.0)", settings.tempOffs, 4, "type='number' min='-3.0' max='3.0' step='0.1' title='Correction value to add to temperature as read from sensor. Ignored when temperature from TCD is displayed.' autocomplete='off'");
#endif // VSR_HAVETEMP

#ifdef BTTFN_MC
WiFiManagerParameter custom_tcdIP("tcdIP", "IP address or hostname of TCD", settings.tcdIP, 31, "pattern='(^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)\\.?\\b){4}$)|([A-Za-z0-9\\-]+)' placeholder='Example: 192.168.4.1'");
#else
WiFiManagerParameter custom_tcdIP("tcdIP", "IP address of TCD", settings.tcdIP, 31, "pattern='^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)\\.?\\b){4}$' placeholder='Example: 192.168.4.1'");
#endif
WiFiManagerParameter custom_uNM("uNM", "Follow TCD night-mode<br><span style='font-size:80%'>If checked, display and lights will be off or dimmed in night-mode.</span>", settings.useNM, 1, "autocomplete='off' type='checkbox' class='mb0'", WFM_LABEL_AFTER);
WiFiManagerParameter custom_uFPO("uFPO", "Follow TCD fake power", settings.useFPO, 1, "autocomplete='off' type='checkbox'", WFM_LABEL_AFTER);
WiFiManagerParameter custom_bttfnTT("bttfnTT", "TT buttons trigger BTTFN-wide TT<br><span style='font-size:80%'>If checked, pressing the Time Travel buttons triggers a BTTFN-wide TT</span>", settings.bttfnTT, 1, "autocomplete='off' type='checkbox' class='mb0'", WFM_LABEL_AFTER);
WiFiManagerParameter custom_ignTT("ignTT", "Ignore network-wide TTs<br><span style='font-size:80%'>If checked, the VSR will not take part in BTTFN/MQTT-wide time travels</span>", settings.ignTT, 1, "autocomplete='off' type='checkbox' class='mb0'", WFM_LABEL_AFTER);

#ifdef VSR_HAVEMQTT
WiFiManagerParameter custom_useMQTT("uMQTT", "Use Home Assistant (MQTT 3.1.1)", settings.useMQTT, 1, "type='checkbox' class='mt5 mb10'", WFM_LABEL_AFTER);
WiFiManagerParameter custom_mqttServer("ha_server", "Broker IP[:port] or domain[:port]", settings.mqttServer, 79, "pattern='[a-zA-Z0-9\\.:\\-]+' placeholder='Example: 192.168.1.5'");
WiFiManagerParameter custom_mqttUser("ha_usr", "User[:Password]", settings.mqttUser, 63, "placeholder='Example: ronald:mySecret'");
#endif // HAVEMQTT

WiFiManagerParameter custom_TCDpresent("TCDpres", "TCD connected by wire", settings.TCDpresent, 1, "autocomplete='off' title='Check this if you have a Time Circuits Display connected via wire' type='checkbox' style='margin-top:5px;'", WFM_LABEL_AFTER);
WiFiManagerParameter custom_noETTOL("uEtNL", "TCD signals Time Travel without 5s lead", settings.noETTOLead, 1, "autocomplete='off' type='checkbox' class='mt5' style='margin-left:20px;'", WFM_LABEL_AFTER);

WiFiManagerParameter custom_haveSD(wmBuildHaveSD);
WiFiManagerParameter custom_CfgOnSD("CfgOnSD", "Save secondary settings on SD<br><span style='font-size:80%'>Check this to avoid flash wear</span>", settings.CfgOnSD, 1, "autocomplete='off' type='checkbox' class='mt5 mb0'", WFM_LABEL_AFTER);
//WiFiManagerParameter custom_sdFrq("sdFrq", "4MHz SD clock speed<br><span style='font-size:80%'>Checking this might help in case of SD card problems</span>", settings.sdFreq, 1, "autocomplete='off' type='checkbox' style='margin-top:12px'", WFM_LABEL_AFTER);

WiFiManagerParameter custom_sectstart_head("<div class='sects'>");
WiFiManagerParameter custom_sectstart("</div><div class='sects'>");
WiFiManagerParameter custom_sectend("</div>");

WiFiManagerParameter custom_sectstart_wifi("</div><div class='sects'><div class='headl'>WiFi connection: Other settings</div>");

WiFiManagerParameter custom_sectstart_mp("</div><div class='sects'><div class='headl'>MusicPlayer</div>");
WiFiManagerParameter custom_sectstart_ap("</div><div class='sects'><div class='headl'>Access point (AP) mode</div>");
WiFiManagerParameter custom_sectstart_nm("</div><div class='sects'><div class='headl'>Night mode</div>");
WiFiManagerParameter custom_sectstart_te("</div><div class='sects'><div class='headl'>Temperature display</div>");
WiFiManagerParameter custom_sectstart_nw("</div><div class='sects'><div class='headl'>Wireless communication (BTTF-Network)</div>");

WiFiManagerParameter custom_sectend_foot("</div><p></p>");

#define TC_MENUSIZE 7
static const int8_t wifiMenu[TC_MENUSIZE] = { 
    WM_MENU_WIFI,
    WM_MENU_PARAM,
    WM_MENU_SEP,
    WM_MENU_UPDATE,
    WM_MENU_SEP,
    WM_MENU_CUSTOM,
    WM_MENU_END
};

#define AA_TITLE "VSR"
#define AA_ICON "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQBAMAAADt3eJSAAAAFVBMVEVJSkrMy8e2v73a5eP20zTyJT0AAAC7naKPAAAALklEQVQI12MQhAIGAQYGZgMGBkZkBpOSkgKEoRoEZaglKcCk0BlO5IiAASPcGQArMAeen41yugAAAABJRU5ErkJggg=="
#define AA_CONTAINER "VSRA"
#define UNI_VERSION VSR_VERSION 
#define UNI_VERSION_EXTRA VSR_VERSION_EXTRA
#define WEBHOME "vsr"

static const char myTitle[] = AA_TITLE;
static const char apName[]  = "VSR-AP";
static const char myHead[]  = "<link rel='shortcut icon' type='image/png' href='data:image/png;base64," AA_ICON "'><script>window.onload=function(){xx=false;document.title=xxx='" AA_TITLE "';id=-1;ar=['/u','/uac','/wifisave','/paramsave'];ti=['Firmware upload','','WiFi Configuration','Settings'];if(ge('s')&&ge('dns')){xx=true;yyy=ti[2]}if(ge('uploadbin')||(id=ar.indexOf(wlp()))>=0){xx=true;if(id>=2){yyy=ti[id]}else{yyy=ti[0]};aa=gecl('wrap');if(aa.length>0){if(ge('uploadbin')){aa[0].style.textAlign='center';}aa=getn('H3');if(aa.length>0){aa[0].remove()}aa=getn('H1');if(aa.length>0){aa[0].remove()}}}if(ge('ttrp')||wlp()=='/param'){xx=true;yyy=ti[3];}if(ge('ebnew')){xx=true;bb=getn('H3');aa=getn('H1');yyy=bb[0].innerHTML;ff=aa[0].parentNode;ff.style.position='relative';}if(xx){zz=(Math.random()>0.8);dd=document.createElement('div');dd.classList.add('tpm0');dd.innerHTML='<div class=\"tpm\" onClick=\"window.location=\\'/\\'\"><div class=\"tpm2\"><img src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABAAQMAAACQp+OdAAAABlBMVEUAAABKnW0vhlhrAAAAAXRSTlMAQObYZgAAA'+(zz?'GBJREFUKM990aEVgCAABmF9BiIjsIIbsJYNRmMURiASePwSDPD0vPT12347GRejIfaOOIQwigSrRHDKBK9CCKoEqQF2qQMOSQAzEL9hB9ICNyMv8DPKgjCjLtAD+AV4dQM7O4VX9m1RYAAAAABJRU5ErkJggg==':'HtJREFUKM990bENwyAUBuFnuXDpNh0rZIBIrJUqMBqjMAIlBeIihQIF/fZVX39229PscYG32esCzeyjsXUzNHZsI0ocxJ0kcZIOsoQjnxQJT3FUiUD1NAloga6wQQd+4B/7QBQ4BpLAOZAn3IIy4RfUibCgTTDq+peG6AvsL/jPTu1L9wAAAABJRU5ErkJggg==')+'\" class=\"tpm3\"></div><H1 class=\"tpmh1\"'+(zz?' style=\"margin-left:1.4em\"':'')+'>'+xxx+'</H1>'+'<H3 class=\"tpmh3\"'+(zz?' style=\"padding-left:5em\"':'')+'>'+yyy+'</div></div>';}if(ge('ebnew')){bb[0].remove();aa[0].replaceWith(dd);}else if(xx){aa=gecl('wrap');if(aa.length>0){aa[0].insertBefore(dd,aa[0].firstChild);aa[0].style.position='relative';}}var lc=ge('lc');if(lc){lc.style.transform='rotate('+(358+[0,1,3,4,5][Math.floor(Math.random()*4)])+'deg)'}}</script><style type='text/css'>H1,H2{margin-top:0px;margin-bottom:0px;text-align:center;}H3{margin-top:0px;margin-bottom:5px;text-align:center;}button{transition-delay:250ms;margin-top:10px;margin-bottom:10px;font-variant-caps:all-small-caps;border-bottom:0.2em solid #225a98}input{border:thin inset}em > small{display:inline}form{margin-block-end:0;}.tpm{cursor:pointer;border:1px solid black;border-radius:5px;padding:0 0 0 0px;min-width:18em;}.tpm2{position:absolute;top:-0.7em;z-index:130;left:0.7em;}.tpm3{width:4em;height:4em;}.tpmh1{font-variant-caps:all-small-caps;font-weight:normal;margin-left:2.2em;overflow:clip;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI Semibold',Roboto,'Helvetica Neue',Verdana,Helvetica}.tpmh3{background:#000;font-size:0.6em;color:#ffa;padding-left:7.2em;margin-left:0.5em;margin-right:0.5em;border-radius:5px}.tpm0{position:relative;width:20em;padding:5px 0px 5px 0px;margin:0 auto 0 auto;}.cmp0{margin:0;padding:0;}.sel0{font-size:90%;width:auto;margin-left:10px;vertical-align:baseline;}.mt5{margin-top:5px!important}.mb10{margin-bottom:10px!important}.mb0{margin-bottom:0px!important}.mb15{margin-bottom:15px!important}</style>";
static const char* myCustMenu = "<img id='ebnew' style='display:block;margin:10px auto 5px auto;' src='data:image/png;base64,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'><div style='font-size:0.9em;line-height:0.9em;font-weight:bold;margin:5px auto 0px auto;text-align:center;font-variant:all-small-caps'>" UNI_VERSION " (" UNI_VERSION_EXTRA ")<br>Powered by A10001986 <a href='https://" WEBHOME ".out-a-ti.me' style='text-decoration:underline' target=_blank>[Home]</a></div>";

const char menu_myNoSP[] = "<hr><div style='margin-left:auto;margin-right:auto;text-align:center;'>Please <a href='/update'>install</a> sound pack</div><hr>";

static int  shouldSaveConfig = 0;
static bool shouldSaveIPConfig = false;
static bool shouldDeleteIPConfig = false;

// WiFi power management in AP mode
bool          wifiInAPMode = false;
bool          wifiAPIsOff = false;
unsigned long wifiAPModeNow;
unsigned long wifiAPOffDelay = 0;     // default: never

// WiFi power management in STA mode
bool          wifiIsOff = false;
unsigned long wifiOnNow = 0;
unsigned long wifiOffDelay     = 0;   // default: never
unsigned long origWiFiOffDelay = 0;

static File acFile;
static bool haveACFile = false;
static bool haveAC = false;
static int  numUploads = 0;
static int  *ACULerr = NULL;
static int  *opType = NULL;

#ifdef VSR_HAVEMQTT
#define       MQTT_SHORT_INT  (30*1000)
#define       MQTT_LONG_INT   (5*60*1000)
static const char emptyStr[1] = { 0 };
bool          useMQTT = false;
char          *mqttUser = (char *)emptyStr;
char          *mqttPass = (char *)emptyStr;
char          *mqttServer = (char *)emptyStr;
uint16_t      mqttPort = 1883;
bool          pubMQTT = false;
static unsigned long mqttReconnectNow = 0;
static unsigned long mqttReconnectInt = MQTT_SHORT_INT;
static uint16_t      mqttReconnFails = 0;
static bool          mqttSubAttempted = false;
static bool          mqttOldState = true;
static bool          mqttDoPing = true;
static bool          mqttRestartPing = false;
static bool          mqttPingDone = false;
static unsigned long mqttPingNow = 0;
static unsigned long mqttPingInt = MQTT_SHORT_INT;
static uint16_t      mqttPingsExpired = 0;
#endif

static void wifiConnect(bool deferConfigPortal = false);
static void saveParamsCallback();
static void preSaveWiFiCallback();
static void saveWiFiCallback(const char *ssid, const char *pass);
static void preUpdateCallback();
static void postUpdateCallback(bool);
static int  menuOutLenCallback();
static void menuOutCallback(String& page);
static void wifiDelayReplacement(unsigned int mydel);
static void gpCallback(int);
static bool preWiFiScanCallback();

static void setupStaticIP();
static void ipToString(char *str, IPAddress ip);
static IPAddress stringToIp(char *str);

static void getParam(String name, char *destBuf, size_t length, int defaultVal);
static bool myisspace(char mychar);
static char* strcpytrim(char* destination, const char* source, bool doFilter = false);
static void mystrcpy(char *sv, WiFiManagerParameter *el);
static void strcpyCB(char *sv, WiFiManagerParameter *el);
static void setCBVal(WiFiManagerParameter *el, char *sv);

static void setupWebServerCallback();
static void handleUploadDone();
static void handleUploading();
static void handleUploadDone();

#ifdef VSR_HAVEMQTT
static void strcpyutf8(char *dst, const char *src, unsigned int len);
static void mqttPing();
static bool mqttReconnect(bool force = false);
static void mqttLooper();
static void mqttCallback(char *topic, byte *payload, unsigned int length);
static void mqttSubscribe();
#endif

/*
 * wifi_setup()
 *
 */
void wifi_setup()
{
    int temp;

    WiFiManagerParameter *wifiParmArray[] = {

      &custom_sectstart_head, 
      &custom_hostName,

      &custom_sectstart_wifi,
      &custom_wifiConRetries, 
      &custom_wifiConTimeout, 

      &custom_sectstart_ap,
      &custom_sysID,
      &custom_appw,
      &custom_apch,
      &custom_bapch,

      &custom_sectend_foot,

      NULL
      
    };

    WiFiManagerParameter *parmArray[] = {

      &custom_aood,

      &custom_sectstart_head,// 9
      &custom_smoothpw,
      &custom_fluct,
      &custom_displayBM,
      &custom_signalBM,
      &custom_playTTSnd,
      &custom_playALSnd,
      &custom_Bri,
      &custom_ssDelay,
  
      &custom_sectstart,     // 2 (3)
      #ifdef VSR_HAVEVOLKNOB
      &custom_FixV,
      #endif
      &custom_Vol,
  
      &custom_sectstart_mp,  // 3
      &custom_musicFolder,
      &custom_shuffle,
  
      &custom_sectstart_nm,  // 2
      &custom_diNmOff,
  
      &custom_sectstart_te,  // 3
      &custom_tempUnit, 
      #ifdef VSR_HAVETEMP
      &custom_tempOffs,
      #endif
 
      &custom_sectstart_nw,  // 6
      &custom_tcdIP,
      &custom_uNM,
      &custom_uFPO,
      &custom_bttfnTT,
      &custom_ignTT,
     
      #ifdef VSR_HAVEMQTT
      &custom_sectstart,     // 4
      &custom_useMQTT,
      &custom_mqttServer,
      &custom_mqttUser,
      #endif
      
      &custom_sectstart,     // 3
      &custom_TCDpresent,
      &custom_noETTOL, 
      
      &custom_sectstart,     // 2 (3)
      &custom_haveSD,
      &custom_CfgOnSD,
      //&custom_sdFrq,
     
      &custom_sectend_foot,  // 1

      NULL
    };

    // Transition from NVS-saved data to own management:
    if(!settings.ssid[0] && settings.ssid[1] == 'X') {
        
        // Read NVS-stored WiFi data
        wm.getStoredCredentials(settings.ssid, sizeof(settings.ssid), settings.pass, sizeof(settings.pass));

        #ifdef VSR_DBG
        Serial.printf("WiFi Transition: ssid '%s' pass '%s'\n", settings.ssid, settings.pass);
        #endif

        write_settings();
    }

    wm.setHostname(settings.hostName);

    wm.showUploadContainer(haveSD, AA_CONTAINER, true);
    
    wm.setPreSaveWiFiCallback(preSaveWiFiCallback);
    wm.setSaveWiFiCallback(saveWiFiCallback);
    wm.setSaveParamsCallback(saveParamsCallback);
    wm.setPreOtaUpdateCallback(preUpdateCallback);
    wm.setPostOtaUpdateCallback(postUpdateCallback);
    wm.setWebServerCallback(setupWebServerCallback);
    wm.setMenuOutLenCallback(menuOutLenCallback);
    wm.setMenuOutCallback(menuOutCallback);
    wm.setDelayReplacement(wifiDelayReplacement);
    wm.setGPCallback(gpCallback);
    wm.setPreWiFiScanCallback(preWiFiScanCallback);
    
    // Our style-overrides, the page title
    wm.setCustomHeadElement(myHead);
    wm.setTitle(myTitle);

    // Hack version number into WiFiManager main page
    wm.setCustomMenuHTML(myCustMenu);

    temp = atoi(settings.apChnl);
    if(temp < 0) temp = 0;
    if(temp > 11) temp = 11;
    if(!temp) temp = random(1, 11);
    wm.setWiFiAPChannel(temp);

    temp = atoi(settings.wifiConTimeout);
    if(temp < 7) temp = 7;
    if(temp > 25) temp = 25;
    wm.setConnectTimeout(temp);

    temp = atoi(settings.wifiConRetries);
    if(temp < 1) temp = 1;
    if(temp > 10) temp = 10;
    wm.setConnectRetries(temp);

    #ifdef WIFIMANAGER_2_0_17
    wm._preloadwifiscan = false;
    wm._asyncScan = true;
    #endif

    wm.setMenu(wifiMenu, TC_MENUSIZE, false);

    wm.allocParms((sizeof(parmArray) / sizeof(WiFiManagerParameter *)) - 1);

    temp = haveAudioFiles ? 1 : 0;
    while(parmArray[temp]) {
        wm.addParameter(parmArray[temp]);
        temp++;
    }

    wm.allocWiFiParms((sizeof(wifiParmArray) / sizeof(WiFiManagerParameter *)) - 1);

    temp = 0;
    while(wifiParmArray[temp]) {
        wm.addWiFiParameter(wifiParmArray[temp]);
        temp++;
    }

    updateConfigPortalValues();

    #ifdef VSR_HAVEMQTT
    useMQTT = (atoi(settings.useMQTT) > 0);
    #endif

    // See if we have a configured WiFi network to connect to.
    // If we detect "TCD-AP" as the SSID, we make sure that we retry
    // at least 2 times so we have a chance to catch the TCD's AP if 
    // both are powered up at the same time.
    // Also, we disable MQTT if connected to the TCD-AP.
    if(settings.ssid[0] != 0) {
        if(!strncmp("TCD-AP", settings.ssid, 6)) {
            if(wm.getConnectRetries() < 2) {
                wm.setConnectRetries(2);
            }
            // Delay to give the TCD some time
            // (differs accross the props)
            delay(900);
            #ifdef VSR_HAVEMQTT
            useMQTT = false;
            #endif
        }      
    } else {
        // No point in retry when we have no WiFi config'd
        wm.setConnectRetries(1);
    }

    // No WiFi powersave features here
    wifiOffDelay = 0;
    wifiAPOffDelay = 0;
    
    // Configure static IP
    if(loadIpSettings()) {
        setupStaticIP();
    }
    
    wifi_setup2();
}

void wifi_setup2()
{
    // Connect, but defer starting the CP
    wifiConnect(true);

    #ifdef VSR_MDNS
    if(MDNS.begin(settings.hostName)) {
        MDNS.addService("http", "tcp", 80);
    }
    #endif

#ifdef VSR_HAVEMQTT
    if((!settings.mqttServer[0]) || // No server -> no MQTT
       (wifiInAPMode))              // WiFi in AP mode -> no MQTT
        useMQTT = false;  
    
    if(useMQTT) {

        bool mqttRes = false;
        char *t;
        int tt;

        // No WiFi power save if we're using MQTT
        origWiFiOffDelay = wifiOffDelay = 0;

        if((t = strchr(settings.mqttServer, ':'))) {
            size_t ts = (t - settings.mqttServer) + 1;
            mqttServer = (char *)malloc(ts);
            memset(mqttServer, 0, ts);
            strncpy(mqttServer, settings.mqttServer, t - settings.mqttServer);
            tt = atoi(t + 1);
            if(tt > 0 && tt <= 65535) {
                mqttPort = tt;
            }
        } else {
            mqttServer = settings.mqttServer;
        }

        if(isIp(mqttServer)) {
            mqttClient.setServer(stringToIp(mqttServer), mqttPort);
        } else {
            IPAddress remote_addr;
            if(WiFi.hostByName(mqttServer, remote_addr)) {
                mqttClient.setServer(remote_addr, mqttPort);
            } else {
                mqttClient.setServer(mqttServer, mqttPort);
                // Disable PING if we can't resolve domain
                mqttDoPing = false;
                Serial.printf("MQTT: Failed to resolve '%s'\n", mqttServer);
            }
        }
        
        mqttClient.setCallback(mqttCallback);
        mqttClient.setLooper(mqttLooper);

        if(settings.mqttUser[0] != 0) {
            if((t = strchr(settings.mqttUser, ':'))) {
                size_t ts = strlen(settings.mqttUser) + 1;
                mqttUser = (char *)malloc(ts);
                strcpy(mqttUser, settings.mqttUser);
                mqttUser[t - settings.mqttUser] = 0;
                mqttPass = mqttUser + (t - settings.mqttUser + 1);
            } else {
                mqttUser = settings.mqttUser;
            }
        }

        #ifdef VSR_DBG
        Serial.printf("MQTT: server '%s' port %d user '%s' pass '%s'\n", mqttServer, mqttPort, mqttUser, mqttPass);
        #endif
            
        mqttReconnect(true);
        // Rest done in loop
            
    } else {

        #ifdef VSR_DBG
        Serial.println("MQTT: Disabled");
        #endif

    }
#endif

    // Start the Config Portal
    if(WiFi.status() == WL_CONNECTED) {
        wifiStartCP();
    }

    wifiSetupDone = true;
}

/*
 * wifi_loop()
 *
 */
void wifi_loop()
{
    char oldCfgOnSD = 0;

#ifdef VSR_HAVEMQTT
    if(useMQTT) {
        if(mqttClient.state() != MQTT_CONNECTING) {
            if(!mqttClient.connected()) {
                if(mqttOldState || mqttRestartPing) {
                    // Disconnection first detected:
                    mqttPingDone = mqttDoPing ? false : true;
                    mqttPingNow = mqttRestartPing ? millis() : 0;
                    mqttOldState = false;
                    mqttRestartPing = false;
                    mqttSubAttempted = false;
                }
                if(mqttDoPing && !mqttPingDone) {
                    audio_loop();
                    mqttPing();
                    audio_loop();
                }
                if(mqttPingDone) {
                    audio_loop();
                    mqttReconnect();
                    audio_loop();
                }
            } else {
                // Only call Subscribe() if connected
                mqttSubscribe();
                mqttOldState = true;
            }
        }
        mqttClient.loop();
    }
#endif    
    
    if(shouldSaveIPConfig) {

        #ifdef VSR_DBG
        Serial.println("WiFi: Saving IP config");
        #endif

        mp_stop();
        stopAudio();

        writeIpSettings();

        shouldSaveIPConfig = false;

    } else if(shouldDeleteIPConfig) {

        #ifdef VSR_DBG
        Serial.println("WiFi: Deleting IP config");
        #endif

        mp_stop();
        stopAudio();

        deleteIpSettings();

        shouldDeleteIPConfig = false;

    }

    if(shouldSaveConfig) {

        int temp;

        // Save settings and restart esp32

        mp_stop();
        stopAudio();

        #ifdef VSR_DBG
        Serial.println("Config Portal: Saving config");
        #endif

        // Only read parms if the user actually clicked SAVE on the wifi config or params pages
        if(shouldSaveConfig == 1) {

            // Parameters on WiFi Config page

            // Note: Parameters that need to grabbed from the server directly
            // through getParam() must be handled in preSaveWiFICallback().

            // ssid, pass copied to settings in saveWiFiCallback()

            strcpytrim(settings.hostName, custom_hostName.getValue(), true);
            if(strlen(settings.hostName) == 0) {
                strcpy(settings.hostName, DEF_HOSTNAME);
            } else {
                char *s = settings.hostName;
                for ( ; *s; ++s) *s = tolower(*s);
            }
            mystrcpy(settings.wifiConRetries, &custom_wifiConRetries);
            mystrcpy(settings.wifiConTimeout, &custom_wifiConTimeout);
            
            strcpytrim(settings.systemID, custom_sysID.getValue(), true);
            strcpytrim(settings.appw, custom_appw.getValue(), true);
            if((temp = strlen(settings.appw)) > 0) {
                if(temp < 8) {
                    settings.appw[0] = 0;
                }
            }

        } else { 

            // Parameters on Settings page

            // Save volume setting
            #ifdef VSR_HAVEVOLKNOB
            strcpyCB(settings.FixV, &custom_FixV);
            #endif

            mystrcpy(settings.Vol, &custom_Vol);
            
            #ifdef VSR_HAVEVOLKNOB
            if(settings.FixV[0] == '0') {
                if(curSoftVol != 255) {
                    curSoftVol = 255;
                    saveCurVolume();
                }
            } else if(settings.FixV[0] == '1') {
            #endif  // HAVEVOLKNOB
                if(strlen(settings.Vol) > 0) {
                    temp = atoi(settings.Vol);
                    if(temp >= 0 && temp <= 19) {
                        curSoftVol = temp;
                        saveCurVolume();
                    }
                }
            #ifdef VSR_HAVEVOLKNOB
            }
            #endif

            // Save music folder number
            if(haveSD) {
                mystrcpy(settings.musicFolder, &custom_musicFolder);
                if(strlen(settings.musicFolder) > 0) {
                    temp = atoi(settings.musicFolder);
                    if(temp >= 0 && temp <= 9) {
                        musFolderNum = temp;
                        saveMusFoldNum();
                    }
                }
            }
            
            // Save brightness setting
            mystrcpy(settings.Bri, &custom_Bri);
            if(strlen(settings.Bri) > 0) {
                temp = atoi(settings.Bri);
                if(temp >= 0 && temp <= 15) {
                    vsrdisplay.setBrightness(temp);
                    saveBrightness();
                }
            }

            strcpyCB(settings.smoothpw, &custom_smoothpw);
            strcpyCB(settings.fluct, &custom_fluct);
            strcpyCB(settings.displayBM, &custom_displayBM);
            strcpyCB(settings.signalBM, &custom_signalBM);
            strcpyCB(settings.playTTsnds, &custom_playTTSnd);
            strcpyCB(settings.playALsnd, &custom_playALSnd);
            mystrcpy(settings.ssTimer, &custom_ssDelay);

            strcpyCB(settings.shuffle, &custom_shuffle);

            strcpyCB(settings.diNmOff, &custom_diNmOff);

            strcpyCB(settings.tempUnit, &custom_tempUnit);
            #ifdef VSR_HAVETEMP
            mystrcpy(settings.tempOffs, &custom_tempOffs);
            #endif

            strcpytrim(settings.tcdIP, custom_tcdIP.getValue());
            if(strlen(settings.tcdIP) > 0) {
                char *s = settings.tcdIP;
                for ( ; *s; ++s) *s = tolower(*s);
            }
            strcpyCB(settings.useNM, &custom_uNM);
            strcpyCB(settings.useFPO, &custom_uFPO);
            strcpyCB(settings.bttfnTT, &custom_bttfnTT);
            strcpyCB(settings.ignTT, &custom_ignTT);

            #ifdef VSR_HAVEMQTT
            strcpyCB(settings.useMQTT, &custom_useMQTT);
            strcpytrim(settings.mqttServer, custom_mqttServer.getValue());
            strcpyutf8(settings.mqttUser, custom_mqttUser.getValue(), sizeof(settings.mqttUser));
            #endif

            strcpyCB(settings.TCDpresent, &custom_TCDpresent);
            strcpyCB(settings.noETTOLead, &custom_noETTOL);

            oldCfgOnSD = settings.CfgOnSD[0];
            strcpyCB(settings.CfgOnSD, &custom_CfgOnSD);
            //strcpyCB(settings.sdFreq, &custom_sdFrq);          

            // Copy volume/speed/IR/etc settings to other medium if
            // user changed respective option
            if(oldCfgOnSD != settings.CfgOnSD[0]) {
                copySettings();
            }

        }

        // Write settings if requested, or no settings file exists
        if(shouldSaveConfig >= 1 || !checkConfigExists()) {
            write_settings();
        }
        
        shouldSaveConfig = 0;

        // Reset esp32 to load new settings
        
        #ifdef VSR_DBG
        Serial.println("Config Portal: Restarting ESP....");
        #endif
        Serial.flush();

        prepareReboot();
        delay(500);
        esp_restart();
    }

    // For some reason (probably memory allocation doing some
    // garbage collection), the first HTTPSend (triggered in
    // wm.process()) after initiating mp3 playback takes
    // unusally long, in bad cases 100s of ms. It's worse 
    // closer to playback start.
    // This hack skips Webserver-handling inside wm.process() 
    // if an mp3 playback started within the last 3 seconds.
    wm.process(!checkAudioStarted());

    // WiFi power management
    // If a delay > 0 is configured, WiFi is powered-down after timer has
    // run out. The timer starts when the device is powered-up/boots.
    // There are separate delays for AP mode and STA mode.
    // WiFi can be re-enabled for the configured time by holding '7'
    // on the keypad.
    // NTP requests will re-enable WiFi (in STA mode) for a short
    // while automatically.
    if(wifiInAPMode) {
        // Disable WiFi in AP mode after a configurable delay (if > 0)
        if(wifiAPOffDelay > 0) {
            if(!wifiAPIsOff && (millis() - wifiAPModeNow >= wifiAPOffDelay)) {
                wifiOff();
                wifiAPIsOff = true;
                wifiIsOff = false;
                #ifdef VSR_DBG
                Serial.println("WiFi (AP-mode) is off.");
                #endif
            }
        }
    } else {
        // Disable WiFi in STA mode after a configurable delay (if > 0)
        if(origWiFiOffDelay > 0) {
            if(!wifiIsOff && (millis() - wifiOnNow >= wifiOffDelay)) {
                wifiOff();
                wifiIsOff = true;
                wifiAPIsOff = false;
                #ifdef VSR_DBG
                Serial.println("WiFi (STA-mode) is off.");
                #endif
            }
        }
    }

}

static void wifiConnect(bool deferConfigPortal)
{
    char realAPName[16];

    strcpy(realAPName, apName);
    if(settings.systemID[0]) {
        strcat(realAPName, settings.systemID);
    }
    
    // Automatically connect using saved credentials if they exist
    // If connection fails it starts an access point with the specified name
    if(wm.autoConnect(settings.ssid, settings.pass, realAPName, settings.appw)) {
        #ifdef VSR_DBG
        Serial.println("WiFi connected");
        #endif

        // Since WM 2.0.13beta, starting the CP invokes an async
        // WiFi scan. This interferes with network access for a 
        // few seconds after connecting. So, during boot, we start
        // the CP later, to allow a quick NTP update.
        if(!deferConfigPortal) {
            wm.startWebPortal();
        }

        // Allow modem sleep:
        // WIFI_PS_MIN_MODEM is the default, and activated when calling this
        // with "true". When this is enabled, received WiFi data can be
        // delayed for as long as the DTIM period.
        // Disable modem sleep, don't want delays accessing the CP or
        // with BTTFN/MQTT.
        WiFi.setSleep(false);

        // Set transmit power to max; we might be connecting as STA after
        // a previous period in AP mode.
        #ifdef VSR_DBG
        {
            wifi_power_t power = WiFi.getTxPower();
            Serial.printf("WiFi: Max TX power in STA mode %d\n", power);
        }
        #endif
        WiFi.setTxPower(WIFI_POWER_19_5dBm);

        wifiInAPMode = false;
        wifiIsOff = false;
        wifiOnNow = millis();
        wifiAPIsOff = false;  // Sic! Allows checks like if(wifiAPIsOff || wifiIsOff)

    } else {
        #ifdef VSR_DBG
        Serial.println("Config portal running in AP-mode");
        #endif

        {
            #ifdef VSR_DBG
            int8_t power;
            esp_wifi_get_max_tx_power(&power);
            Serial.printf("WiFi: Max TX power %d\n", power);
            #endif

            // Try to avoid "burning" the ESP when the WiFi mode
            // is "AP" by reducing the max. transmit power.
            // The choices are:
            // WIFI_POWER_19_5dBm    = 19.5dBm
            // WIFI_POWER_19dBm      = 19dBm
            // WIFI_POWER_18_5dBm    = 18.5dBm
            // WIFI_POWER_17dBm      = 17dBm
            // WIFI_POWER_15dBm      = 15dBm
            // WIFI_POWER_13dBm      = 13dBm
            // WIFI_POWER_11dBm      = 11dBm
            // WIFI_POWER_8_5dBm     = 8.5dBm
            // WIFI_POWER_7dBm       = 7dBm     <-- proven to avoid any issues
            // WIFI_POWER_5dBm       = 5dBm
            // WIFI_POWER_2dBm       = 2dBm
            // WIFI_POWER_MINUS_1dBm = -1dBm
            WiFi.setTxPower(WIFI_POWER_7dBm);

            #ifdef VSR_DBG
            esp_wifi_get_max_tx_power(&power);
            Serial.printf("WiFi: Max TX power set to %d\n", power);
            #endif
        }

        wifiInAPMode = true;
        wifiAPIsOff = false;
        wifiAPModeNow = millis();
        wifiIsOff = false;    // Sic!
    }
}

void wifiOff()
{
    if( (!wifiInAPMode && wifiIsOff) ||
        (wifiInAPMode && wifiAPIsOff) ) {
        return;
    }

    wm.stopWebPortal();
    wm.disconnect();
    WiFi.mode(WIFI_OFF);
}

void wifiOn(unsigned long newDelay, bool alsoInAPMode, bool deferCP)
{
    unsigned long desiredDelay;
    unsigned long Now = millis();

    if(wifiInAPMode && !alsoInAPMode) return;

    if(wifiInAPMode) {
        if(wifiAPOffDelay == 0) return;   // If no delay set, auto-off is disabled
        wifiAPModeNow = Now;              // Otherwise: Restart timer
        if(!wifiAPIsOff) return;
    } else {
        if(origWiFiOffDelay == 0) return; // If no delay set, auto-off is disabled
        desiredDelay = (newDelay > 0) ? newDelay : origWiFiOffDelay;
        if((Now - wifiOnNow >= wifiOffDelay) ||                    // If delay has run out, or
           (wifiOffDelay - (Now - wifiOnNow))  < desiredDelay) {   // new delay exceeds remaining delay:
            wifiOffDelay = desiredDelay;                           // Set new timer delay, and
            wifiOnNow = Now;                                       // restart timer
            #ifdef VSR_DBG
            Serial.printf("Restarting WiFi-off timer; delay %d\n", wifiOffDelay);
            #endif
        }
        if(!wifiIsOff) {
            // If WiFi is not off, check if user wanted
            // to start the CP, and do so, if not running
            if(!deferCP) {
                if(!wm.getWebPortalActive()) {
                    wm.startWebPortal();
                }
            }
            return;
        }
    }

    wifiConnect(deferCP);
}

// Check if WiFi is on; used to determine if a 
// longer interruption due to a re-connect is to
// be expected.
bool wifiIsOn()
{
    if(wifiInAPMode) {
        if(wifiAPOffDelay == 0) return true;
        if(!wifiAPIsOff) return true;
    } else {
        if(origWiFiOffDelay == 0) return true;
        if(!wifiIsOff) return true;
    }
    return false;
}

void wifiStartCP()
{
    if(wifiInAPMode || wifiIsOff)
        return;

    wm.startWebPortal();
}

// This is called when the WiFi config is to be saved. We set
// a flag for the loop to read out and save the new WiFi config.
// SSID and password are copied to settings here.
static void saveWiFiCallback(const char *ssid, const char *pass)
{
    // ssid is the (new?) ssid to connect to, pass the password.
    // (We don't need to compare to the old ones since the
    // settings are saved in any case)
    // This is also used to "forget" a saved WiFi network, in
    // which case ssid and pass are empty strings.
    memset(settings.ssid, 0, sizeof(settings.ssid));
    memset(settings.pass, 0, sizeof(settings.pass));
    if(*ssid) {
        strncpy(settings.ssid, ssid, sizeof(settings.ssid) - 1);
        strncpy(settings.pass, pass, sizeof(settings.pass) - 1);
    }

    #ifdef VSR_DBG
    Serial.printf("saveWiFiCallback: New ssid '%s'\n", settings.ssid);
    Serial.printf("saveWiFiCallback: New pass '%s'\n", settings.pass);
    #endif
    
    shouldSaveConfig = 1;
}

// This is the callback from the actual Params page. We read out
// thew WM "Settings" parameters and save them.
static void saveParamsCallback()
{
    shouldSaveConfig = 2;
}

// This is called before a firmware updated is initiated.
// Disable WiFi-off-timers, switch off audio, show "wait"
static void preUpdateCallback()
{
    wifiAPOffDelay = 0;
    origWiFiOffDelay = 0;

    mp_stop();
    stopAudio();

    flushDelayedSave();

    showWaitSequence();
}

// This is called after a firmware updated has finished.
// parm = true of ok, false if error. WM reboots only 
// if the update worked, ie when res is true.
static void postUpdateCallback(bool res)
{
    Serial.flush();
    prepareReboot();

    // WM does not reboot on OTA update errors.
    // However, don't bother for that really
    // rare case to put code here to restore
    // under all possible circumstances, like
    // fake-off, time-travel going on, ss, ....
    if(!res) {
        delay(1000);
        esp_restart();
    }
}

// Grab static IP and other parameters from WiFiManager's server.
// Since there is no public method for this, we steal the HTML
// form parameters in this callback.
static void preSaveWiFiCallback()
{
    char ipBuf[20] = "";
    char gwBuf[20] = "";
    char snBuf[20] = "";
    char dnsBuf[20] = "";
    bool invalConf = false;

    #ifdef VSR_DBG
    Serial.println("preSaveConfigCallback");
    #endif

    // clear as strncpy might leave us unterminated
    memset(ipBuf, 0, 20);
    memset(gwBuf, 0, 20);
    memset(snBuf, 0, 20);
    memset(dnsBuf, 0, 20);

    String temp;
    temp.reserve(16);
    if((temp = wm.server->arg(FPSTR(WMS_ip))) != "") {
        strncpy(ipBuf, temp.c_str(), 19);
    } else invalConf |= true;
    if((temp = wm.server->arg(FPSTR(WMS_sn))) != "") {
        strncpy(snBuf, temp.c_str(), 19);
    } else invalConf |= true;
    if((temp = wm.server->arg(FPSTR(WMS_gw))) != "") {
        strncpy(gwBuf, temp.c_str(), 19);
    } else invalConf |= true;
    if((temp = wm.server->arg(FPSTR(WMS_dns))) != "") {
        strncpy(dnsBuf, temp.c_str(), 19);
    } else invalConf |= true;

    #ifdef VSR_DBG
    if(strlen(ipBuf) > 0) {
        Serial.printf("IP:%s / SN:%s / GW:%s / DNS:%s\n", ipBuf, snBuf, gwBuf, dnsBuf);
    } else {
        Serial.println("Static IP unset, using DHCP");
    }
    #endif

    if(!invalConf && isIp(ipBuf) && isIp(gwBuf) && isIp(snBuf) && isIp(dnsBuf)) {

        #ifdef VSR_DBG
        Serial.println("All IPs valid");
        #endif

        shouldSaveIPConfig = (strcmp(ipsettings.ip, ipBuf)      ||
                              strcmp(ipsettings.gateway, gwBuf) ||
                              strcmp(ipsettings.netmask, snBuf) ||
                              strcmp(ipsettings.dns, dnsBuf));
          
        if(shouldSaveIPConfig) {
            strcpy(ipsettings.ip, ipBuf);
            strcpy(ipsettings.gateway, gwBuf);
            strcpy(ipsettings.netmask, snBuf);
            strcpy(ipsettings.dns, dnsBuf);
        }

    } else {

        #ifdef VSR_DBG
        if(strlen(ipBuf) > 0) {
            Serial.println("Invalid IP");
        }
        #endif

        shouldDeleteIPConfig = true;

    }

    // Other parameters on WiFi Config page that
    // need grabbing directly from the server

    getParam("apchnl", settings.apChnl, 2, DEF_AP_CHANNEL);
}

static void setupStaticIP()
{
    IPAddress ip;
    IPAddress gw;
    IPAddress sn;
    IPAddress dns;

    if(strlen(ipsettings.ip) > 0 &&
        isIp(ipsettings.ip) &&
        isIp(ipsettings.gateway) &&
        isIp(ipsettings.netmask) &&
        isIp(ipsettings.dns)) {

        ip = stringToIp(ipsettings.ip);
        gw = stringToIp(ipsettings.gateway);
        sn = stringToIp(ipsettings.netmask);
        dns = stringToIp(ipsettings.dns);

        wm.setSTAStaticIPConfig(ip, gw, sn, dns);
    }
}

static int menuOutLenCallback()
{
    int mySize = 0;

    if(!haveAudioFiles) {
        mySize += STRLEN(menu_myNoSP);
    }
    return mySize;
}

static void menuOutCallback(String& page)
{       
    if(!haveAudioFiles) {
        page += menu_myNoSP;
    }
}

static bool preWiFiScanCallback()
{
    // Do not allow a WiFi scan under some circumstances (as
    // it may disrupt sequences)
    
    if(TTrunning || networkAlarm)
        return false;

    return true;
}

static void wifiDelayReplacement(unsigned int mydel)
{
    if((mydel > 30) && audioInitDone) {
        unsigned long startNow = millis();
        while(millis() - startNow < mydel) {
            audio_loop();
            delay(20);
        }
    } else {
        delay(mydel);
    }
}

void gpCallback(int reason)
{
    // Called when WM does stuff that might
    // take some time, like before and after
    // HTTPSend().
    // MUST NOT call wifi_loop() !!!
    
    if(audioInitDone) {
        switch(reason) {
        case WM_LP_PREHTTPSEND:
            if(wifiInAPMode) {
                if(checkMP3Running()) {
                    mp_stop();
                    stopAudio();
                    return;
                }
            } // fall through
        case WM_LP_NONE:
        case WM_LP_POSTHTTPSEND:
            audio_loop();
            yield();
            break;
        }
    }
}

void updateConfigPortalValues()
{
    // Make sure the settings form has the correct values

    custom_hostName.setValue(settings.hostName, 31);
    custom_wifiConTimeout.setValue(settings.wifiConTimeout, 2);
    custom_wifiConRetries.setValue(settings.wifiConRetries, 2);
    
    custom_sysID.setValue(settings.systemID, 7);
    // ap channel done on-the-fly
    custom_appw.setValue(settings.appw, 8);

    setCBVal(&custom_smoothpw, settings.smoothpw);
    setCBVal(&custom_fluct, settings.fluct);
    setCBVal(&custom_displayBM, settings.displayBM);
    setCBVal(&custom_signalBM, settings.signalBM);
    setCBVal(&custom_playTTSnd, settings.playTTsnds);
    setCBVal(&custom_playALSnd, settings.playALsnd);
    custom_ssDelay.setValue(settings.ssTimer, 3);

    setCBVal(&custom_shuffle, settings.shuffle);

    setCBVal(&custom_diNmOff, settings.diNmOff);

    setCBVal(&custom_tempUnit, settings.tempUnit);
    #ifdef VSR_HAVETEMP
    custom_tempOffs.setValue(settings.tempOffs, 4);
    #endif
    
    custom_tcdIP.setValue(settings.tcdIP, 31);
    setCBVal(&custom_uNM, settings.useNM);
    setCBVal(&custom_uFPO, settings.useFPO);
    setCBVal(&custom_bttfnTT, settings.bttfnTT);
    setCBVal(&custom_ignTT, settings.ignTT);
    
    #ifdef VSR_HAVEMQTT
    setCBVal(&custom_useMQTT, settings.useMQTT);
    custom_mqttServer.setValue(settings.mqttServer, 79);
    custom_mqttUser.setValue(settings.mqttUser, 63);
    #endif
    
    setCBVal(&custom_TCDpresent, settings.TCDpresent);
    setCBVal(&custom_noETTOL, settings.noETTOLead);

    setCBVal(&custom_CfgOnSD, settings.CfgOnSD);
    //setCBVal(&custom_sdFrq, settings.sdFreq);
}

void updateConfigPortalVolValues()
{
    #ifdef VSR_HAVEVOLKNOB
    if(curSoftVol == 255) {
        strcpy(settings.FixV, "0");
        strcpy(settings.Vol, "6");
    } else {
        strcpy(settings.FixV, "1");
        sprintf(settings.Vol, "%d", curSoftVol);
    }
    #else
    if(curSoftVol > 19) curSoftVol = DEFAULT_VOLUME;
    sprintf(settings.Vol, "%d", curSoftVol);
    #endif
    
    #ifdef VSR_HAVEVOLKNOB
    setCBVal(&custom_FixV, settings.FixV);
    #endif
    
    custom_Vol.setValue(settings.Vol, 2);
}

void updateConfigPortalMFValues()
{
    sprintf(settings.musicFolder, "%d", musFolderNum);
    custom_musicFolder.setValue(settings.musicFolder, 2);
}

void updateConfigPortalBriValues()
{
    sprintf(settings.Bri, "%d", vsrdisplay.getBrightness());
    custom_Bri.setValue(settings.Bri, 2);
}

static void buildSelectMenu(char *target, const char **theHTML, int cnt, char *setting)
{
    int sr = atoi(setting);
    
    strcat(target, theHTML[0]);
    strcat(target, setting);
    sprintf(target + strlen(target), "' name='%s' id='%s' autocomplete='off'><option value='0'", theHTML[1], theHTML[1]);
    for(int i = 0; i < cnt - 2; i++) {
        if(sr == i) strcat(target, custHTMLSel);
        sprintf(target + strlen(target), 
            theHTML[i+2], (i == cnt - 3) ? osde : ooe);
    }
}

static const char *wmBuildApChnl(const char *dest)
{
    if(dest) {
      free((void *)dest);
      return NULL;
    }
    
    char *str = (char *)malloc(600);    // actual length 564

    str[0] = 0;
    buildSelectMenu(str, apChannelCustHTMLSrc, 14, settings.apChnl);
    
    return str;
}

static const char *wmBuildBestApChnl(const char *dest)
{
    if(dest) {
      free((void *)dest);
      return NULL;
    }

    int32_t mychan = 0;
    int qual = 0;

    if(wm.getBestAPChannel(mychan, qual)) {
        char *str = (char *)malloc(STRLEN(bestAP) + 4 + 7 + STRLEN(badWiFi) + 1);
        sprintf(str, bestAP, qual < 0 ? "dc3630" : (qual > 0 ? "609b71" : "777"), mychan, qual < 0 ? badWiFi : "");
        return str;
    }

    return NULL;
}

static const char *wmBuildHaveSD(const char *dest)
{
    if(dest || haveSD)
        return NULL;

    return haveNoSD;
}

/*
 * Audio data uploader
 */
static void doReboot()
{
    delay(1000);
    prepareReboot();
    delay(500);
    esp_restart();
}

static void allocUplArrays()
{
    if(opType) free((void *)opType);
    opType = (int *)malloc(MAX_SIM_UPLOADS * sizeof(int));
    if(ACULerr) free((void *)ACULerr);
    ACULerr = (int *)malloc(MAX_SIM_UPLOADS * sizeof(int));;
    memset(opType, 0, MAX_SIM_UPLOADS * sizeof(int));
    memset(ACULerr, 0, MAX_SIM_UPLOADS * sizeof(int));
}

static void setupWebServerCallback()
{
    wm.server->on(R_updateacdone, HTTP_POST, &handleUploadDone, &handleUploading);
}

static void doCloseACFile(int idx, bool doRemove)
{
    if(haveACFile) {
        closeACFile(acFile);
        haveACFile = false;
    }
    if(doRemove) removeACFile(idx);  
}

static void handleUploading()
{
    HTTPUpload& upload = wm.server->upload();

    if(upload.status == UPLOAD_FILE_START) {

          String c = upload.filename;
          const char *illChrs = "|~><:*?\" ";
          int temp;
          char tempc;

          if(numUploads >= MAX_SIM_UPLOADS) {
            
              haveACFile = false;

          } else {

              c.toLowerCase();
    
              // Remove path and some illegal characters
              tempc = '/';
              for(int i = 0; i < 2; i++) {
                  if((temp = c.lastIndexOf(tempc)) >= 0) {
                      if(c.length() - 1 > temp) {
                          c = c.substring(temp);
                      } else {
                          c = "";
                      }
                      break;
                  }
                  tempc = '\\';
              }
              for(int i = 0; i < strlen(illChrs); i++) {
                  c.replace(illChrs[i], '_');
              }
              if(!c.indexOf("..")) {
                  c.replace("..", "");
              }
    
              if(!numUploads) {
                  allocUplArrays();
                  preUpdateCallback();
              }
    
              haveACFile = openUploadFile(c, acFile, numUploads, haveAC, opType[numUploads], ACULerr[numUploads]);

              if(haveACFile && opType[numUploads] == 1) {
                  haveAC = true;
              }

          }

    } else if(upload.status == UPLOAD_FILE_WRITE) {

          if(haveACFile) {
              if(writeACFile(acFile, upload.buf, upload.currentSize) != upload.currentSize) {
                  doCloseACFile(numUploads, true);
                  ACULerr[numUploads] = UPL_WRERR;
              }
          }

    } else if(upload.status == UPLOAD_FILE_END) {

        if(numUploads < MAX_SIM_UPLOADS) {

            doCloseACFile(numUploads, false);
    
            if(opType[numUploads] >= 0) {
                renameUploadFile(numUploads);
            }
    
            numUploads++;

        }
      
    } else if(upload.status == UPLOAD_FILE_ABORTED) {

        if(numUploads < MAX_SIM_UPLOADS) {
            doCloseACFile(numUploads, true);
        }

        doReboot();

    }

    delay(0);
}

static void handleUploadDone()
{
    const char *ebuf = "ERROR";
    const char *dbuf = "DONE";
    char *buf = NULL;
    bool haveErrs = false;
    bool haveAC = false;
    int titStart = -1;
    int buflen  = strlen(wm.getHTTPSTART(titStart)) +
                  STRLEN(myTitle)    +
                  strlen(wm.getHTTPSCRIPT()) +
                  strlen(wm.getHTTPSTYLE()) +
                  STRLEN(myHead)     +
                  STRLEN(acul_part3) +
                  STRLEN(myTitle)    +
                  STRLEN(acul_part5) +
                  STRLEN(apName)     +
                  STRLEN(acul_part6) +
                  STRLEN(acul_part8) +
                  1;

    for(int i = 0; i < numUploads; i++) {
        if(opType[i] > 0) {
            haveAC = true;
            if(!ACULerr[i]) {
                if(!check_if_default_audio_present()) {
                    haveAC = false;
                    ACULerr[i] = UPL_BADERR;
                    removeACFile(i);
                }
            }
            break;
        }
    }    

    if(!haveSD && numUploads) {
      
        buflen += (STRLEN(acul_part71) + strlen(acul_errs[1]));
        
    } else {

        for(int i = 0; i < numUploads; i++) {
            if(ACULerr[i]) haveErrs = true;
        }
        if(haveErrs) {
            buflen += STRLEN(acul_part71);
            for(int i = 0; i < numUploads; i++) {
                if(ACULerr[i]) {
                    buflen += getUploadFileNameLen(i);
                    buflen += 2; // :_
                    buflen += strlen(acul_errs[ACULerr[i]-1]);
                    buflen += 4; // <br>
                }
            }
        } else {
            buflen += strlen(wm.getHTTPSTYLEOK());
            buflen += STRLEN(acul_part7);
        }
        if(haveAC) {
            buflen += STRLEN(acul_part7a);
        }
    }

    buflen += 8;

    if(!(buf = (char *)malloc(buflen))) {
        buf = (char *)(haveErrs ? ebuf : dbuf);
    } else {
        strcpy(buf, wm.getHTTPSTART(titStart));
        if(titStart >= 0) {
            strcpy(buf + titStart, myTitle);
            strcat(buf, "</title>");
        }
        strcat(buf, wm.getHTTPSCRIPT());
        strcat(buf, wm.getHTTPSTYLE());
        if(!haveErrs) {
            strcat(buf, wm.getHTTPSTYLEOK());
        }
        strcat(buf, myHead);
        strcat(buf, acul_part3);
        strcat(buf, myTitle);
        strcat(buf, acul_part5);
        strcat(buf, apName);
        strcat(buf, acul_part6);

        if(!haveSD && numUploads) {

            strcat(buf, acul_part71);
            strcat(buf, acul_errs[1]);
            
        } else {
            
            if(haveErrs) {
                strcat(buf, acul_part71);
                for(int i = 0; i < numUploads; i++) {
                    if(ACULerr[i]) {
                        char *t = getUploadFileName(i);
                        if(t) {
                            strcat(buf, t);
                        }
                        strcat(buf, ": ");
                        strcat(buf, acul_errs[ACULerr[i]-1]);
                        strcat(buf, "<br>");
                    }
                }
            } else {
                strcat(buf, acul_part7);
            }
            if(haveAC) {
                strcat(buf, acul_part7a);
            }
        }

        strcat(buf, acul_part8);
    }

    freeUploadFileNames();
    /* Pointless, we reboot
    numUploads = 0;
    for(int i = 0; i < MAX_SIM_UPLOADS; i++) {
        opType[i] = 0;
        ACULerr[i] = 0;
    }
    */
    
    String str(buf);
    wm.server->send(200, "text/html", str);

    // Reboot required even for mp3 upload, because for most files, we check
    // during boot if they exist (to avoid repeatedly failing open() calls)

    doReboot();
}

bool wifi_getIP(uint8_t& a, uint8_t& b, uint8_t& c, uint8_t& d)
{
    IPAddress myip;

    switch(WiFi.getMode()) {
      case WIFI_MODE_STA:
          myip = WiFi.localIP();
          break;
      case WIFI_MODE_AP:
      case WIFI_MODE_APSTA:
          myip = WiFi.softAPIP();
          break;
      default:
          a = b = c = d = 0;
          return true;
    }

    a = myip[0];
    b = myip[1];
    c = myip[2];
    d = myip[3];

    return true;
}

// Check if String is a valid IP address
bool isIp(char *str)
{
    int segs = 0;
    int digcnt = 0;
    int num = 0;

    while(*str) {

        if(*str == '.') {

            if(!digcnt || (++segs == 4))
                return false;

            num = digcnt = 0;
            str++;
            continue;

        } else if((*str < '0') || (*str > '9')) {

            return false;

        }

        if((num = (num * 10) + (*str - '0')) > 255)
            return false;

        digcnt++;
        str++;
    }

    if(segs == 3) 
        return true;

    return false;
}

// IPAddress to string
static void ipToString(char *str, IPAddress ip)
{
    sprintf(str, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

// String to IPAddress
static IPAddress stringToIp(char *str)
{
    int ip1, ip2, ip3, ip4;

    sscanf(str, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4);

    return IPAddress(ip1, ip2, ip3, ip4);
}

/*
 * Read parameter from server, for customhmtl input
 */
static void getParam(String name, char *destBuf, size_t length, int defaultVal)
{
    memset(destBuf, 0, length+1);
    if(wm.server->hasArg(name)) {
        strncpy(destBuf, wm.server->arg(name).c_str(), length);
    }
    if(!*destBuf) {
        sprintf(destBuf, "%d", defaultVal);
    }
}

static bool myisspace(char mychar)
{
    return (mychar == ' ' || mychar == '\n' || mychar == '\t' || mychar == '\v' || mychar == '\f' || mychar == '\r');
}

static bool myisgoodchar(char mychar)
{
    return ((mychar >= '0' && mychar <= '9') || (mychar >= 'a' && mychar <= 'z') || (mychar >= 'A' && mychar <= 'Z') || mychar == '-');
}

static char* strcpytrim(char* destination, const char* source, bool doFilter)
{
    char *ret = destination;
    
    while(*source) {
        if(!myisspace(*source) && (!doFilter || myisgoodchar(*source))) *destination++ = *source;
        source++;
    }
    
    *destination = 0;
    
    return ret;
}

static void mystrcpy(char *sv, WiFiManagerParameter *el)
{
    strcpy(sv, el->getValue());
}

static void strcpyCB(char *sv, WiFiManagerParameter *el)
{
    strcpy(sv, (atoi(el->getValue()) > 0) ? "1" : "0");
}

static void setCBVal(WiFiManagerParameter *el, char *sv)
{
    const char makeCheck[] = "1' checked a='";
    
    el->setValue((atoi(sv) > 0) ? makeCheck : "1", 14);
}

#ifdef VSR_HAVEMQTT
static void strcpyutf8(char *dst, const char *src, unsigned int len)
{
    strncpy(dst, src, len - 1);
    dst[len - 1] = 0;
}

static void mqttLooper()
{
    audio_loop();
}

static void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    int i = 0, j, ml = (length <= 255) ? length : 255;
    char tempBuf[256];
    static const char *cmdList[] = {
      "TIMETRAVEL",       // 0
      "DISPLAY_PW",
      "DISPLAY_TEMP",
      "DISPLAY_SPEED",
      "WWW",              // [Placeholder]
      "MP_SHUFFLE_ON",    // 5 
      "MP_SHUFFLE_OFF",   // 6
      "MP_PLAY",          // 7
      "MP_STOP",          // 8
      "MP_NEXT",          // 9
      "MP_PREV",          // 10
      "MP_FOLDER_",       // 11  MP_FOLDER_0..MP_FOLDER_9
      NULL
    };
    static const char *cmdList2[] = {
      "PREPARE",          // 0
      "TIMETRAVEL",       // 1
      "REENTRY",          // 2
      "ABORT_TT",         // 3
      "ALARM",            // 4
      "WAKEUP",           // 5
      NULL
    };

    if(!length) return;

    memcpy(tempBuf, (const char *)payload, ml);
    tempBuf[ml] = 0;
    for(j = 0; j < ml; j++) {
        if(tempBuf[j] >= 'a' && tempBuf[j] <= 'z') tempBuf[j] &= ~0x20;
    }

    if(!strcmp(topic, "bttf/tcd/pub")) {

        // Commands from TCD

        while(cmdList2[i]) {
            j = strlen(cmdList2[i]);
            if((length >= j) && !strncmp((const char *)tempBuf, cmdList2[i], j)) {
                break;
            }
            i++;          
        }

        if(!cmdList2[i]) return;

        switch(i) {
        case 0:
            // Prepare for TT. Comes at some undefined point,
            // an undefined time before the actual tt, and may
            // now come at all.
            // We disable our Screen Saver and start the flux
            // sound (if to be played)
            // We don't ignore this if TCD is connected by wire,
            // because this signal does not come via wire.
            if(!ignTT && !TTrunning) {
                prepareTT();
            }
            break;
        case 1:
            // Trigger Time Travel (if not running already)
            // Ignore command if TCD is connected by wire
            if(!ignTT && !TCDconnected && !TTrunning) {
                networkTimeTravel = true;
                networkTCDTT = true;
                networkReentry = false;
                networkAbort = false;
                networkLead = ETTO_LEAD;
            }
            break;
        case 2:   // Re-entry
            // Start re-entry (if TT currently running)
            // Ignore command if TCD is connected by wire
            if(!ignTT && !TCDconnected && TTrunning && networkTCDTT) {
                networkReentry = true;
            }
            break;
        case 3:   // Abort TT (TCD fake-powered down during TT)
            // Ignore command if TCD is connected by wire
            // (mainly because this is no network-triggered TT)
            if(!ignTT && !TCDconnected && TTrunning && networkTCDTT) {
                networkAbort = true;
            }
            break;
        case 4:   // Alarm
            networkAlarm = true;
            // Eval this at our convenience
            break;
        case 5:   // Wakeup
            if(!TTrunning) {
                wakeup();
            }
            break;
        }
       
    } else if(!strcmp(topic, "bttf/vsr/cmd")) {

        // User commands

        // Not taking commands under these circumstances:
        if(TTrunning || !FPBUnitIsOn)
            return;
        
        while(cmdList[i]) {
            j = strlen(cmdList[i]);
            if((length >= j) && !strncmp((const char *)tempBuf, cmdList[i], j)) {
                break;
            }
            i++;          
        }

        if(!cmdList[i]) return;

        switch(i) {
        case 0:
            // Trigger Time Travel; treated like button, not
            // like TT from TCD.
            networkTimeTravel = true;
            networkTCDTT = false;
            break;
        case 1:
            userDispMode = LDM_WHEELS;
            break;
        case 2:
            userDispMode = LDM_TEMP;
            break;
        case 3:
            userDispMode = LDM_GPS;
            break;
        case 4:
            // Placeholder
            break;
        case 5:
        case 6:
            if(haveMusic) mp_makeShuffle((i == 5));
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
        case 11:
            if(haveSD) {
                if(strlen(tempBuf) > j && tempBuf[j] >= '0' && tempBuf[j] <= '9') {
                    switchMusicFolder((uint8_t)(tempBuf[j] - '0'));
                }
            }
            break;
        }
            
    } 
}

#ifdef VSR_DBG
#define MQTT_FAILCOUNT 6
#else
#define MQTT_FAILCOUNT 120
#endif

static void mqttPing()
{
    switch(mqttClient.pstate()) {
    case PING_IDLE:
        if(WiFi.status() == WL_CONNECTED) {
            if(!mqttPingNow || (millis() - mqttPingNow > mqttPingInt)) {
                mqttPingNow = millis();
                if(!mqttClient.sendPing()) {
                    // Mostly fails for internal reasons;
                    // skip ping test in that case
                    mqttDoPing = false;
                    mqttPingDone = true;  // allow mqtt-connect attempt
                }
            }
        }
        break;
    case PING_PINGING:
        if(mqttClient.pollPing()) {
            mqttPingDone = true;          // allow mqtt-connect attempt
            mqttPingNow = 0;
            mqttPingsExpired = 0;
            mqttPingInt = MQTT_SHORT_INT; // Overwritten on fail in reconnect
            // Delay re-connection for 5 seconds after first ping echo
            mqttReconnectNow = millis() - (mqttReconnectInt - 5000);
        } else if(millis() - mqttPingNow > 5000) {
            mqttClient.cancelPing();
            mqttPingNow = millis();
            mqttPingsExpired++;
            mqttPingInt = MQTT_SHORT_INT * (1 << (mqttPingsExpired / MQTT_FAILCOUNT));
            mqttReconnFails = 0;
        }
        break;
    } 
}

static bool mqttReconnect(bool force)
{
    bool success = false;

    if(useMQTT && (WiFi.status() == WL_CONNECTED)) {

        if(!mqttClient.connected()) {
    
            if(force || !mqttReconnectNow || (millis() - mqttReconnectNow > mqttReconnectInt)) {

                #ifdef VSR_DBG
                Serial.println("MQTT: Attempting to (re)connect");
                #endif
    
                if(strlen(mqttUser)) {
                    success = mqttClient.connect(settings.hostName, mqttUser, strlen(mqttPass) ? mqttPass : NULL);
                } else {
                    success = mqttClient.connect(settings.hostName);
                }
    
                mqttReconnectNow = millis();
                
                if(!success) {
                    mqttRestartPing = true;  // Force PING check before reconnection attempt
                    mqttReconnFails++;
                    if(mqttDoPing) {
                        mqttPingInt = MQTT_SHORT_INT * (1 << (mqttReconnFails / MQTT_FAILCOUNT));
                    } else {
                        mqttReconnectInt = MQTT_SHORT_INT * (1 << (mqttReconnFails / MQTT_FAILCOUNT));
                    }
                    #ifdef VSR_DBG
                    Serial.printf("MQTT: Failed to reconnect (%d)\n", mqttReconnFails);
                    #endif
                } else {
                    mqttReconnFails = 0;
                    mqttReconnectInt = MQTT_SHORT_INT;
                    #ifdef VSR_DBG
                    Serial.println("MQTT: Connected to broker, waiting for CONNACK");
                    #endif
                }
    
                return success;
            } 
        }
    }
      
    return true;
}

static void mqttSubscribe()
{
    // Meant only to be called when connected!
    if(!mqttSubAttempted) {
        if(!mqttClient.subscribe("bttf/vsr/cmd", "bttf/tcd/pub")) {
            #ifdef VSR_DBG
            Serial.println("MQTT: Failed to subscribe to command topics");
            #endif
        }
        mqttSubAttempted = true;
    }
}

bool mqttState()
{
    return (useMQTT && mqttClient.connected());
}

void mqttPublish(const char *topic, const char *pl, unsigned int len)
{
    if(useMQTT) {
        mqttClient.publish(topic, (uint8_t *)pl, len, false);
    }
}           

#endif
