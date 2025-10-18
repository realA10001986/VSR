/*
 * -------------------------------------------------------------------
 * Voltage Systems Regulator
 * (C) 2024-2025 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/VSR
 * https://vsr.out-a-ti.me
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

/*
 * Build instructions (for Arduino IDE)
 * 
 * - Install the Arduino IDE
 *   https://www.arduino.cc/en/software
 *    
 * - This firmware requires the "ESP32-Arduino" framework. To install this framework, 
 *   in the Arduino IDE, go to "File" > "Preferences" and add the URL   
 *   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
 *   - or (if the URL above does not work) -
 *   https://espressif.github.io/arduino-esp32/package_esp32_index.json
 *   to "Additional Boards Manager URLs". The list is comma-separated.
 *   
 * - Go to "Tools" > "Board" > "Boards Manager", then search for "esp32", and install 
 *   the latest 2.x version by Espressif Systems. Versions >=3.x are not supported.
 *   Detailed instructions for this step:
 *   https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html
 *   
 * - Go to "Tools" > "Board: ..." -> "ESP32 Arduino" and select your board model (the
 *   CircuitSetup original boards are "NodeMCU-32S")
 *   
 * - Connect your ESP32 board using a suitable USB cable.
 *   Note that NodeMCU ESP32 boards come in two flavors that differ in which serial 
 *   communications chip is used: Either SLAB CP210x USB-to-UART or CH340. Installing
 *   a driver might be required.
 *   Mac: 
 *   For the SLAB CP210x (which is used by NodeMCU-boards distributed by CircuitSetup)
 *   installing a driver is required:
 *   https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers?tab=downloads
 *   The port ("Tools -> "Port") is named /dev/cu.SLAB_USBtoUART, and the maximum
 *   upload speed ("Tools" -> "Upload Speed") can be used.
 *   The CH340 is supported out-of-the-box since Mojave. The port is named 
 *   /dev/cu.usbserial-XXXX (XXXX being some random number), and the maximum upload 
 *   speed is 460800.
 *   Windows:
 *   For the SLAB CP210x (which is used by NodeMCU-boards distributed by CircuitSetup)
 *   installing a driver is required:
 *   https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers?tab=downloads
 *   After installing this driver, connect your ESP32, start the Device Manager, 
 *   expand the "Ports (COM & LPT)" list and look for the port with the ESP32 name.
 *   Choose this port under "Tools" -> "Port" in Arduino IDE.
 *   For the CH340, another driver is needed. Try connecting the ESP32 and have
 *   Windows install a driver automatically; otherwise search google for a suitable
 *   driver. Note that the maximum upload speed is either 115200, or perhaps 460800.
 *
 * - Install required libraries. In the Arduino IDE, go to "Tools" -> "Manage Libraries" 
 *   and install the following libraries:
 *   - ArduinoJSON (>= 6.19): https://arduinojson.org/v6/doc/installation/
 *
 * - Download the complete firmware source code:
 *   https://github.com/realA10001986/VSR/archive/refs/heads/main.zip
 *   Extract this file somewhere. Enter the "vsr-A10001986" folder and 
 *   double-click on "vsr-A10001986.ino". This opens the firmware in the
 *   Arduino IDE.
 *
 * - Go to "Sketch" -> "Upload" to compile and upload the firmware to your ESP32 board.
 *
 * - Install the sound-pack: 
 *   Method 1:
 *   - Go to Config Portal, click "Update" and upload the sound-pack (VSRA.bin, extracted
 *     from install/sound-pack-xxxxxxxx.zip) through the bottom file selector.
 *     A FAT32 (not ExFAT!) formatted SD card must be present in the slot during this 
 *     operation.
 *   Method 2:
 *   - Copy VSRA.bin to the top folder of a FAT32 (not ExFAT!) formatted SD card (max 
 *     32GB) and put this card into the slot while the VSR is powered down. 
 *   - Now power-up. The sound-pack will now be installed. When finished, the VSR will 
 *     reboot.
 */

/*  Changelog
 *  
 *  2025/10/17 (A10001986) [1.20]
 *    - Wipe flash FS if alien VER found; in case no VER is present, check
 *      available space for audio files, and wipe if not enough.
 *  2025/10/16 (A10001986)
 *    - Minor code optim (settings)
 *    - WM: More event-based waiting instead of delays
 *  2025/10/15 (A10001986)  
 *    - Some more WM changes. Number of scanned networks listed is now restricted in 
 *      order not to run out of memory.
 *  2025/10/14 (A10001986) [1.19.2]
 *    - WM: Do not garble UTF8 SSID; skip SSIDs with non-printable characters
 *    - Fix regression in CP ("show password")
 *  2025/10/13 (A10001986)
 *    - Config Portal: Minor restyling (message boxes)
 *  2025/10/11 (A10001986) [1.19.1]
 *    - More WM changes: Simplify "Forget" using a checkbox; redo signal quality
 *      assessment; remove over-engineered WM debug stuff.
 *  2025/10/08 (A10001986)   
 *    - WM: Set "world safe" country info, limiting choices to 11 channels
 *    - WM: Add "show all", add channel info (when all are shown) and
 *      proposed AP WiFi channel on WiFi Configuration page.
 *    - Experimental: Change bttfn_checkmc() to return true as long as 
 *      a packet was received (as opposed to false if a packet was received
 *      but not for us, malformed, etc). Also, change the max packet counter
 *      in bttfn_loop(_quick)() from 10 to 100 to get more piled-up old 
 *      packets out of the way.
 *    - Add a delay when connecting to TCD-AP so not all props hammer the
 *      TCD-AP at the very same time
 *    - WM: Use events when connecting, instead of delays
 *  2025/10/07 (A10001986) [1.19]
 *    - Add emergency firmware update via SD (for dev purposes)
 *    - WM fixes (Upload, etc)
 *  2025/10/06 (A10001986)
 *    - WM: Skip setting static IP params in Save
 *    - Add "No SD present" banner in Config Portal if no SD present
 *  2025/10/05 (A10001986)
 *    - CP: Show msg instead of upload file input if no sd card is present
 *  2025/10/03-05 (A10001986) [1.18]
 *    - More WiFiManager changes. We no longer use NVS-stored WiFi configs, 
 *      all is managed by our own settings. (No details are known, but it
 *      appears as if the core saves some data to NVS on every reboot, this
 *      is totally not needed for our purposes, nor in the interest of 
 *      flash longevity.)
 *    - Save static IP only if changed
 *    - Disable MQTT when connected to "TCD-AP"
 *    - Let DNS server in AP mode only resolve our domain (hostname)
 *  2025/09/22-10/03 (A10001986)
 *    - WiFi Manager overhaul; many changes to Config Portal.
 *      WiFi-related settings moved to WiFi Configuration page.
 *      Note: If the VSR is in AP-mode, mp3 playback will be stopped when
 *      accessing Config Portal web pages from now on.
 *      This had lead to sound stutter and incomplete page loads in the past.
 *    - Various code optimizations to minimize code size and used RAM
 *  2025/09/22 (A10001986) [1.17]
 *    - Config Portal: Re-order settings; remove non-checkbox-code
 *    - Fix TCD hostname length field
 *  2025/09/20 (A10001986)
 *    - Config Portal: Add "install sound pack" banner to main menu
 *  2025/09/19 (A10001986) [1.16]
 *    - Extend mp3 upload by allowing multiple (max 16) mp3 files to be uploaded
 *      at once. The VSRA.bin file can be uploaded at the same time as well.
 *    - Remove HAVE_AUDIO conditional
 *  2025/09/17 (A10001986)
 *    - WiFi Manager: Reduce page size by removing "quality icon" styles where
 *      not needed.
 *  2025/09/15 (A10001986) [1.15]
 *    - Refine mp3 upload facility; allow deleting files from SD by prefixing
 *      filename with "delete-".
 *    - WiFi manager: Remove lots of <br> tags; makes Safari display the
 *      pages better.
 *  2025/09/14 (A10001986)
 *    - Allow uploading .mp3 files to SD through config portal. Uses the same
 *      interface as audio container upload. Files are stored in the root
 *      folder of the SD; hence not suitable for music player.
 *    - WiFi manager: Remove (ie skip compilation of) unused code
 *    - WiFi manager: Add callback to Erase WiFi settings, before reboot
 *    - WiFi manager: Build param page with fixed size string to avoid memory 
 *      fragmentation; add functions to calculate String size beforehand.
 *  2025/02/11 (A10001986) [1.14.1]
 *    - Change default volume to 10
 *    - Delete temp file after audio installation
 *  2025/01/17 (A10001986) [1.14]
 *    - Fix audio logic errors
 *    - Fix sound-pack (volchg volume reduction)
 *  2025/01/11-15 (A10001986) [1.13]
 *    - Add option to ignore network-wide time travels. After all, the VSR is 
 *      never shown during time travel in the movies, so some might prefer to
 *      skip the made-up time travel sequence.
 *    - New "volume change" sample sound (new sound-pack)
 *    - BTTFN: Minor code optimization
 *    - Optimize play_key; keyX will be stopped instead of (re)started if it is 
 *      currently played when repeatedly triggered
 *  2025/01/10 (A10001986) [1.12]
 *    - Add support for HDC302x temperature sensor
 *    - Interrupt night mode for button mode changes if display is configured to
 *      be off in night mode.
 *  2024/10/28 (A10001986)
 *    - Increase i2c speed to 400kHz
 *  2024/10/27 (A10001986) [1.11]
 *    - Minor changes (bttfn_loop in delay-loops; fix alarm when fp is off; etc)
 *  2024/10/26 (A10001986) [1.10]
 *    - Add support for TCD multicast notifications: This brings more immediate speed 
 *      updates (no more polling; TCD sends out speed info when appropriate), and 
 *      less network traffic in time travel sequences.
 *      The TCD supports this since Oct 26, 2024.
 *  2024/10/02 (A10001986)
 *    - Minor (cosmetic) fix to display code
 *  2024/09/30 (A10001986)
 *    - Minor temperature-related fix
 *  2024/09/11 (A10001986)
 *    - Fix C99-compliance
 *  2024/09/09 (A10001986)
 *    - Tune BTTFN poll interval
 *  2024/09/01 (A10001986)
 *    - Stop audio when fake-powered off
 *  2024/08/31 (A10001986)
 *    - Fix brightness change not displayed in Config Portal
 *  2024/08/28 (A10001986)
 *    - Treat TCD-speed from Remote like speed from RotEnc
 *    - Fix some typos
 *  2024/08/11 (A10001986)
 *    - In addition to the custom "key3" and "key6" sounds, now also "key1", 
 *      "key4", "key7" and "key9" are available/supported; command codes are 
 *      8001, 8004, 8007, 8009.
 *  2024/08/05 (A10001986)
 *    - Display volume when pressing up/down key even if limit has been reached
 *    - TT: Minimize duration of P0 in stand-alone mode
 *    - TT: Relay-click earlier in sync'd tt
 *    - Save user-selected display mode
 *    - Display "AUD" if audio files need to be installed/updated
 *    - Fix volume control by buttons (update CP; save)
 *  2024/07/11 (A10001986)
 *    - MQTT: Add MP_FOLDER_x commands to set music folder number
 *  2024/06/05 (A10001986)
 *    - Minor fixes for WiFiManager
 *    * Switched to esp32-arduino 2.0.17 for pre-compiled binary.
 *  2024/05/09 (A10001986)
 *    - TT button: Activate internal pull-down (in addition to external pd)
 *  2024/05/08 (A10001986)
 *    - Make button mode indication by lights optional
 *    - Switch button lights from PWM to LOW/HIGH to reflect actual hardware
 *  2024/05/03 (A10001986)
 *    - Adapt to Control Board 1.05
 *  2024/04/25 (A10001986)
 *    - Remove "display type" selection from CP
 *    - Make buttons active low by default
 *  2024/04/10 (A10001986)
 *    - Initial version
 *
 */

#include "vsr_global.h"

#include <Arduino.h>
#include <Wire.h>

#include "vsrdisplay.h"
#include "vsr_audio.h"
#include "vsr_controls.h"
#include "vsr_settings.h"
#include "vsr_main.h"
#include "vsr_wifi.h"

void setup()
{
    powerupMillis = millis();

    Serial.begin(115200);
    Serial.println();

    // I2C init
    Wire.begin(-1, -1, 400000);

    main_boot();
    settings_setup();
    main_boot2();
    wifi_setup();
    audio_setup();
    controls_setup();
    main_setup();
}

void loop()
{
    audio_loop();
    main_loop();
    audio_loop();
    wifi_loop();
    audio_loop();
    bttfn_loop();
}
