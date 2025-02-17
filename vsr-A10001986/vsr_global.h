/*
 * -------------------------------------------------------------------
 * Voltage Systems Regulator
 * (C) 2024-2025 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/VSR
 * https://vsr.out-a-ti.me
 *
 * Global definitions
 */

#ifndef _VSR_GLOBAL_H
#define _VSR_GLOBAL_H

/*************************************************************************
 ***                          Version Strings                          ***
 *************************************************************************/

#define VSR_VERSION "V1.14.1"
#define VSR_VERSION_EXTRA "FEB112025"

//#define VSR_DBG              // debug output on Serial

/*************************************************************************
 ***                     mDNS (Bonjour) support                        ***
 *************************************************************************/

// Supply mDNS service
// Allows accessing the Config Portal via http://hostname.local
// <hostname> is configurable in the Config Portal
// This needs to be commented if WiFiManager provides mDNS
#define VSR_MDNS
// Uncomment this if WiFiManager has mDNS enabled
//#define VSR_WM_HAS_MDNS

/*************************************************************************
 ***             Configuration for hardware/peripherals                ***
 *************************************************************************/

// Uncomment for audio support
#define VSR_HAVEAUDIO

// Uncomment for support of a temperature/humidity sensor (MCP9808, BMx280, 
// SI7021, SHT4x, TMP117, AHT20, HTU31D, MS8607, HDC302X) connected via i2c.
// Will be used to display ambient temperature on display when idle.
// See sensors.cpp for supported i2c slave addresses
#define VSR_HAVETEMP

/*************************************************************************
 ***                           Miscellaneous                           ***
 *************************************************************************/

// Uncomment for HomeAssistant MQTT protocol support
#define VSR_HAVEMQTT

// External time travel lead time, as defined by TCD firmware
// If VSR is connected to TCD by wire, and the option "Signal Time Travel 
// without 5s lead" is set on the TCD, the VSR option "TCD signals without 
// lead" must be set, too.
#define ETTO_LEAD 5000

// Uncomment to include BTTFN discover support (multicast)
#define BTTFN_MC

// Uncomment if hardware has a volume knob
//#define VSR_HAVEVOLKNOB

// Use SPIFFS (if defined) or LittleFS (if undefined; esp32-arduino >= 2.x)
//#define USE_SPIFFS

// Enable LED display test at boot
//#define VSR_DIAG
//#define VSR_DIAG2

/*************************************************************************
 ***                  esp32-arduino version detection                  ***
 *************************************************************************/

#if defined __has_include && __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#ifdef ESP_ARDUINO_VERSION_MAJOR
    #if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(2,0,8)
    #define HAVE_GETNEXTFILENAME
    #endif
#endif
#endif

/*************************************************************************
 ***                             GPIO pins                             ***
 *************************************************************************/

// I2S audio pins
#define I2S_BCLK_PIN      26
#define I2S_LRCLK_PIN     25
#define I2S_DIN_PIN       33

// SD Card pins
#define SD_CS_PIN          5
#define SPI_MOSI_PIN      23
#define SPI_MISO_PIN      19
#define SPI_SCK_PIN       18

// -------- Buttons and LEDs

#define TT_IN_PIN         13      // Time Travel button (or TCD tt trigger input) (has internal PU/PD) (PD on CB)
#define YET_UNUSED_PIN    14      // Accessible GPIO, yet unused                  (has internal PU/PD)

#define BUTTON1_IO_PIN    15      // Button GPIO pins (if not via I2C)            (has internal PU)
#define BUTTON2_IO_PIN    27      //                                              (has internal PU)
#define BUTTON3_IO_PIN    32      //                                              (has no internal PU?; PU on CB)

#define BUTTON1_PWM_PIN   12      // Button LEDs
#define BUTTON2_PWM_PIN   16
#define BUTTON3_PWM_PIN   17

#define VOLUME_PIN        39      // (unused on OEM board)

#endif
