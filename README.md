# VSR (Delorean Time Machine)

This [repository](https://vsr.out-a-ti.me) holds the most current firmware for a replica of the Voltage Systems Regulator ("VSR") known from the Delorean Time Machine. There is no kit available yet; however, the electronics are done and you can make your own circuit boards using the files in the [Electronics](https://github.com/realA10001986/VSR/tree/main/Electronics) folder. Below are some pictures of a fully functional prototype, still with minor optical imperfections (such as the buttons being located to far up and an early display bezel):

[<img src="img/vsrpt_4s.jpg">](img/vsrpt_4.jpg)
[<img src="img/vsrpt_2s.jpg">](img/vsrpt_2.jpg)
[<img src="img/vsrpt_3s.jpg">](img/vsrpt_3.jpg)

Features include:

- Various display modes, selectable by buttons: Pushwheel values, temperature (from own sensor or from [Time Circuits Display](https://tcd.out-a-ti.me) via BTTFN), or speed (from [Time Circuits Display](https://tcd.out-a-ti.me) via BTTFN, through GPS receiver or rotary encoder for speed connected to TCD)
- [Time Travel](#time-travel) function, triggered by button, [Time Circuits Display](https://tcd.out-a-ti.me) or via [MQTT](#home-assistant--mqtt)
- [Music player](#the-music-player): Play mp3 files located on an SD card, controlled by buttons or [Time Circuits Display](https://tcd.out-a-ti.me) keypad via BTTFN
- [SD card](#sd-card) support for custom audio files for effects, and music for the Music Player
- Advanced network-accessible [Config Portal](#the-config-portal) for setup (http://vsr.local, hostname configurable)
- [Wireless communication](#bttf-network-bttfn) with [Time Circuits Display](https://tcd.out-a-ti.me); used for synchronized time travels, alarm, night mode, fake power, temperature display and remote control through keypad
- [Home Assistant](#home-assistant--mqtt) (MQTT 3.1.1) support

## Firmware Installation

If a previous version of the VSR firmware is installed on your device, you can update easily using the pre-compiled binary. Enter the [Config Portal](#the-config-portal), click on "Update" and select the pre-compiled binary file provided in this repository ([install/vsr-A10001986.ino.nodemcu-32s.bin](https://github.com/realA10001986/VSR/blob/main/install/vsr-A10001986.ino.nodemcu-32s.bin)). 

If you are using a fresh ESP32 board, please see [vsr-A10001986.ino](https://github.com/realA10001986/VSR/blob/main/vsr-A10001986/vsr-A10001986.ino) for detailed build and upload information, or, if you don't want to deal with source code, compilers and all that nerd stuff, go [here](https://install.out-a-ti.me) and follow the instructions.

### Audio data installation

The firmware comes with some audio data ("sound-pack") which needs to be installed separately. The audio data is not updated as often as the firmware itself. If you have previously installed the latest version of the sound-pack, you normally don't have to re-install the audio data when you update the firmware. Only if the VSR displays "AUD" briefly during boot, an update of the audio data is needed.

The first step is to download "install/sound-pack-xxxxxxxx.zip" and extract it. It contains one file named "VSRA.bin".

Then there are two alternative ways to proceed. Note that both methods *require an SD card*.

1) Through the [Config Portal](#the-config-portal). Click on *Update*, select the "VSRA.bin" file in the bottom file selector and click on *Upload*. Note that an SD card must be in the slot during this operation.

2) Via SD card:
- Copy "VSRA.bin" to the root directory of of a FAT32 formatted SD card;
- power down the VSR,
- insert this SD card into the slot and 
- power up the VSR; the audio data will be installed automatically.

After installation, the SD card can be re-used for [other purposes](#sd-card).

## Initial Configuration

>The following instructions only need to be followed once, on fresh VSRs. They do not need to be repeated after a firmware update.

The first step is to establish access to the VSR's configuration web site ("Config Portal") in order to configure your VSR:

- Power up the VSR and wait until it has finished booting.
- Connect your computer or handheld device to the WiFi network "VSR-AP".
- Navigate your browser to http://vsr.local or http://192.168.4.1 to enter the Config Portal.

#### Connecting to a WiFi network

As long as the device is unconfigured, it creates a WiFi network of its own named "VSR-AP". This is called "Access point mode", or "AP-mode". In this mode, other WiFi devices can connect to the VSR.

It is ok to leave the VSR in this mode, especially if it run stand-alone. In a typical home setup and/or if you also have a [Time Circuits Display](https://tcd.out-a-ti.me), however, you might want to connect the VSR to a WiFi network (in case of using it together with a TCD: to the same WiFi network the TCD is connected to). If you have your VSR, along with a Time Circuits Display, mounted in a car, you might want to connect the VSR to the TCD's very own WiFi network "TCD-AP"; see [here](#car-setup).

In order to connect your VSR to a WiFi network, click on "Configure WiFi". The bare minimum is to select an SSID (WiFi network name) and a WiFi password.

>Note that the VSR requests an IP address via DHCP, unless you entered valid data in the fields for static IP addresses (IP, gateway, netmask, DNS). If the device is inaccessible as a result of incorrect static IPs, wait until the VSR has completed its startup sequence, then hold all three buttons until "ADM" is displayed, then _hold_ button "4C". Afterwards power-down the VSR. Upon power-up, the device is reset to DHCP.

After saving the WiFi network settings, the VSR reboots and tries to connect to your configured WiFi network. If that fails, it will again start in access point mode.

After completing this step, your VSR is basically ready for use; you can also continue configuring it to your personal preferences through the Config Portal.

## The Config Portal

The "Config Portal" is the VSR's configuration web site. 

| ![The Config Portal](img/cpm.png) |
|:--:| 
| *The Config Portal's main page* |

It can be accessed as follows:

#### If VSR is in AP mode

- Connect your computer or handheld device to the WiFi network "VSR-AP".
- Navigate your browser to http://vsr.local or http://192.168.4.1 to enter the Config Portal.

#### If VSR is connected to WiFi network

- Connect your hand-held/computer to the same WiFi network to which the VSR is connected, and
- navigate your browser to http://vsr.local

  Accessing the Config Portal through this address requires the operating system of your hand-held/computer to support Bonjour/mDNS: Windows 10 version TH2     (1511) [other sources say 1703] and later, Android 13 and later; MacOS and iOS since the dawn of time.

  If connecting to http://vsr.local fails due to a name resolution error, you need to find out the VSR's IP address: Hold all three buttons until "ADM" is displayed, release all buttons and afterwards _hold_ button "9". The VSR will display its current IP address (a. - b. - c. - d). Then, on your handheld or computer, navigate to http://a.b.c.d (a.b.c.d being the IP address as displayed on the VSR) in order to enter the Config Portal.

In the main menu, click on "Setup" to configure your VSR. 

| [<img src="img/cps-frag.png">](img/cp_setup.png) |
|:--:| 
| *Click for full screenshot* |

A full reference of the Config Portal is [here](#appendix-a-the-config-portal).

## Basic Operation

By default, the display shows the value selected by the pushwheels, with slight fluctuations (which can be disabled in the Config Portal). Changing the pushwheels results in the display adapting to the new value, which is done smoothly (which also can be disabled in the Config Portal).

### Display modes

The three-digit display can be used for the pushwheel number, temperature or speed.

In order to display temperature, a temperature sensor needs to be connected to either the VSR or your Time Circuits Display (TCD), and in the latter case the VSR needs to be connected to the TCD via BTTFN.

In order to display speed, a TCD with either a GPS receiver or a rotary encoder for speed is required, and the VSR needs to be connected to the TCD via BTTFN.

Display modes are chosen by selecting the _Operation_ button mode, and then _holding_ a single button until two beeps are emitted from the VSR, as described below.

### Button modes

>Buttons can be _pressed_, which means a brief press-and-release, or _held_, meaning pressing and holding the button for 2 seconds.

There are four button modes: Light ("LGT"), Operation ("OPR"), Music Player ("MUS") and Admin ("ADM"). In order to select a button mode, two or three buttons must be _held_ simultaneously until the display shows the selected mode.

<table>
    <tr>
    <td align="center"><img src="img/bmode0.png"></td><td><a href="#light-mode">Light mode</a></td>
    </tr>
    <tr>
    <td align="center"><img src="img/bmode1.png"></td><td><a href="#operation-mode">Operation mode</a></td>
    </tr>
    <tr>  
    <td align="center"><img src="img/bmode2.png"></td><td><a href="#music-player-mode">MusicPlayer mode</a></td>
    </tr>
    <tr>
    <td align="center"><img src="img/bmode3.png"></td><td><a href="#admin-mode">Admin mode</a></td>
    </tr>
</table>

#### Light mode 

In this mode, the buttons light up when briefly pressed, and stay lit after _holding_ a button. At the next press, the light will go off again. This mode is only for playing with the lights, the buttons have no other function.

#### Operation mode

  <table>
    <tr><td></td><td>Brief press</td><td>Hold</td></tr>
    <tr>
     <td align="center"><img src="img/b9.png"></td><td>Trigger time travel</td>
     <td><a href="#display-modes">Display mode</a>: Pushwheel number</td>
    <tr>
     <td align="center"><img src="img/b10.png"></td><td>Play "<a href="#additional-custom-sounds">key3.mp3</a>" on SD</td>
      <td><a href="#display-modes">Display mode</a>: <a href="#temperature-display">Temperature</a></td>
    </tr>
    <tr>
     <td align="center"><img src="img/b4c.png"></td><td>Play "<a href="#additional-custom-sounds">key6.mp3</a>" on SD</td>
      <td><a href="#display-modes">Display mode</a>: <a href="#speed-display">Speed</a></td>
    </tr>
</table>

#### Music Player mode

  <table>
     <tr><td></td><td>Brief press</td><td>Hold</td></tr>
    <tr>
     <td align="center"><img src="img/b9.png"></td><td>Previous song</td>
      <td>Disable shuffle mode</td>
    </tr>
    <tr>
     <td align="center"><img src="img/b10.png"></td><td>Play/Stop</td>
      <td>Enable shuffle mode</td>
    </tr>
    <tr>
     <td align="center"><img src="img/b4c.png"></td><td>Next song</td>
      <td>Go to song #0</td>
    </tr>
</table>

#### Admin mode

  <table>
    <tr><td></td><td>Brief press</td><td>Hold</td></tr>
    <tr>
     <td align="center"><img src="img/b9.png"></td><td>Volume up</td><td>Display IP address</td>
    </tr>
    <tr>
     <td align="center"><img src="img/b10.png"></td><td>Volume down</td><td>-</td>
    </tr>
    <tr>
     <td align="center"><img src="img/b4c.png"></td><td></td><td>Delete static IP and AP password</td>
    </tr>
</table>

### TCD remote command reference

<table>
   <tr><td>Function</td><td>Code on TCD</td></tr>
    <tr>
     <td align="left">Select pushwheel display mode</td>
     <td align="left">8010&#9166;</td>
    </tr>
    <tr>
     <td align="left">Select <a href="#temperature-display">temperature</a> display mode</td>
     <td align="left">8011&#9166;</td>
    </tr>
    <tr>
     <td align="left">Select <a href="#speed-display">speed</a> display mode</td>
     <td align="left">8012&#9166;</td>
    </tr>
    <tr>
     <td align="left">Set volume level (00-19)</td>
     <td align="left">8300&#9166; - 8319&#9166;</td>
    </tr>
    <tr>
     <td align="left">Set brightness level (00-15)</td>
     <td align="left"<td>8400&#9166; - 8415&#9166;</td>
    </tr>
    <tr>
     <td align="left"><a href="#the-music-player">Music Player</a>: Play/Stop</td>
     <td align="left">8005&#9166;</td>
    </tr>
    <tr>
     <td align="left"><a href="#the-music-player">Music Player</a>: Previous song</td>
     <td align="left">8002&#9166;</td>
    </tr>
    <tr>
     <td align="left"><a href="#the-music-player">Music Player</a>: Next song</td>
     <td align="left">8008&#9166;</td>
    </tr>
    <tr>
     <td align="left"><a href="#the-music-player">Music Player</a>: Select music folder (0-9)</td>
     <td align="left">8050&#9166; - 8059&#9166;</td>
    </tr>
    <tr>
     <td align="left"><a href="#the-music-player">Music Player</a>: Shuffle off</td>
     <td align="left">8222&#9166;</td>
    </tr>
    <tr>
     <td align="left"><a href="#the-music-player">Music Player</a>: Shuffle on</td>
     <td align="left">8555&#9166;</td>
    </tr> 
    <tr>
     <td align="left"><a href="#the-music-player">Music Player</a>: Go to song 0</td>
     <td align="left">8888&#9166;</td>
    </tr>
    <tr>
     <td align="left"><a href="#the-music-player">Music Player</a>: Go to song xxx</td>
     <td align="left">8888xxx&#9166;</td>
    </tr>
    <tr>
     <td align="left">Play "<a href="#additional-custom-sounds">key1.mp3</a>"</td>
     <td align="left">8001&#9166;</td>
    </tr>
    <tr>
     <td align="left">Play "<a href="#additional-custom-sounds">key3.mp3</a>"</td>
     <td align="left">8003&#9166;</td>
    </tr>
    <tr>
     <td align="left">Play "<a href="#additional-custom-sounds">key4.mp3</a>"</td>
     <td align="left">8004&#9166;</td>
    </tr>
    <tr>
     <td align="left">Play "<a href="#additional-custom-sounds">key6.mp3</a>"</td>
     <td align="left">8006&#9166;</td>
    </tr>
    <tr>
     <td align="left">Play "<a href="#additional-custom-sounds">key7.mp3</a>"</td>
     <td align="left">8007&#9166;</td>
    </tr>
    <tr>
     <td align="left">Play "<a href="#additional-custom-sounds">key9.mp3</a>"</td>
     <td align="left">8009&#9166;</td>
    </tr>
    <tr>
     <td align="left">Display current IP address</td>
     <td align="left">8090&#9166;</td>
    </tr>   
    <tr>
     <td align="left">Reboot the device</td>
     <td align="left">8064738&#9166;</td>
    </tr>
     <tr>
     <td align="left">Delete static IP address<br>and WiFi-AP password</td>
     <td align="left">8123456&#9166;</td>
    </tr>
</table>

## Time travel

To trigger a "time travel" stand-alone, select the _Operation_ [button mode](#button-modes), and press "9".

You can also connect a dedicated time travel button to your VSR; the button must connect "TT" to "3.3V" on the "Time Travel" connector. Pressing this button briefly will trigger a time travel.

Other ways of triggering a time travel are available if a [Time Circuits Display](#connecting-a-time-circuits-display) is connected.

## Temperature display

As mentioned above, the VSR can display temperature. The temperature value can come from either a connected temperature sensor, or from the TCD - if the latter has a sensor connected and is connected to the VSR via [BTTFN](#bttf-network-bttfn).

The following sensor types are supported for direct (i2c) connection to the VSR: 
- [MCP9808](https://www.adafruit.com/product/1782) (address 0x18 - non-default),
- [BMx280](https://www.adafruit.com/product/2652) (0x77),
- [SI7021](https://www.adafruit.com/product/3251),
- [SHT40](https://www.adafruit.com/product/4885) (0x44),
- [SHT45](https://www.adafruit.com/product/5665) (0x44),
- [TMP117](https://www.adafruit.com/product/4821) (0x49),
- [AHT20/AM2315C](https://www.adafruit.com/product/4566),
- [HTU31D](https://www.adafruit.com/product/4832) (0x41 - non-default),
- [MS8607](https://www.adafruit.com/product/4716)

All of those are readily available on breakout boards from Adafruit or Seeed (Grove); the links in above list lead to tested example products.

The sensor needs to be wired as follows:

<table>
    <tr>
     <td align="center">Sensor PCB</td><td align="center">VSR control board</td>
    </tr>    
    <tr>
     <td align="center">GND or "-"</td>
     <td align="center">GND</td>
    </tr>
    <tr>
     <td align="center">VIN or 5V or "+"</a></td>
     <td align="center">5V</td>
    </tr>
    <tr>
     <td align="center">SDA<br>(SDI on BME280)</td>
     <td align="center">SDA</td>
    </tr>
    <tr>
     <td align="center">SCL<br>(SCK on BME280)</td>
     <td align="center">SCL</td>
    </tr>
</table>

For longer cables, ie >50cm (>20in), I recommend using a twisted pair cable, and to connect it as follows:

![i2clongcable](img/i2clongcable.png)

>Important: The VSR control board delivers and drives the i2c bus on 5V. Most sensors operate on 3.3V. Therefore, you cannot connect the chips directly to the VSR control board without a level-shifter. This goes for the power supply as well as the i2c bus signals (SDA, SCL). I only use readily available sensor breakout boards that include level-shifters.

## Speed display

The VSR can also display speed, provided a TCD is connected through [BTTFN](#bttf-network-bttfn), and the TCD has either a GPS receiver, or a rotary encoder configured for speed. Please see [here](https://tcd.out-a-ti.me) for more information.

## SD card

Preface note on SD cards: For unknown reasons, some SD cards simply do not work with this device. For instance, I had no luck with Sandisk Ultra 32GB and  "Intenso" cards. If your SD card is not recognized, check if it is formatted in FAT32 format (not exFAT!). Also, the size must not exceed 32GB (as larger cards cannot be formatted with FAT32). Transcend SDHC cards and those work fine in my experience.

The SD card, apart from being required for [installing](#audio-data-installation) of the built-in audio data, can be used for substituting built-in sound effects and for music played back by the [Music player](#the-music-player). Also, it is _strongly recommended_ to store [secondary settings](#-save-secondary-settings-on-sd) on the SD card to minimize [Flash Wear](#flash-wear).

Note that the SD card must be inserted before powering up the device. It is not recognized if inserted while the VSR is running. Furthermore, do not remove the SD card while the device is powered.

### Sound substitution

The VSR's built-in sound effects can be substituted by your own sound files on a FAT32-formatted SD card. These files will be played back directly from the SD card during operation, so the SD card has to remain in the slot.

Your replacements need to be put in the root (top-most) directory of the SD card, be in mp3 format (128kbps max) and named as follows:
- "startup.mp3". Played when the VSR is connected to power and has finished booting;
- ...
- "alarm.mp3". Played when the alarm sounds (triggered by a Time Circuits Display via BTTFN or MQTT);

### Additional Custom Sounds

The firmware supports some additional user-provided sound effects, which it will load from the SD card. If the respective file is present, it will be used. If that file is absent, no sound will be played.

- "key3.mp3"/"key6.mp3": Will be played when you press "10" or "4C" in _Operation_ [button mode](#button-modes), or type 8003 / 8006 on the TCD's keypad (connected through BTTFN);
- "key1.mp3", "key4.mp3", "key7.mp3", "key9.mp3": Will be played when you type 8001 / 8004 / 8007 / 8009 on the TCD's keypad (connected through BTTFN).

> The seemingly odd numbering is because of synchronicity with other props, especially the TCD and its keymap where the MusicPlayer occupies keys 2, 5, 8.

Those files are not provided here. You can use any mp3, with a bitrate of 128kpbs or less.

## The Music Player

The firmware contains a simple music player to play mp3 files located on the SD card. 

In order to be recognized, your mp3 files need to be organized in music folders named *music0* through *music9*. The folder number is 0 by default, ie the player starts searching for music in folder *music0*. This folder number can be changed in the Config Portal or through the TCD keypad (805x).

The names of the audio files must only consist of three-digit numbers, starting at 000.mp3, in consecutive order. No numbers should be left out. Each folder can hold up to 1000 files (000.mp3-999.mp3). *The maximum bitrate is 128kpbs.*

Since manually renaming mp3 files is somewhat cumbersome, the firmware can do this for you - provided you can live with the files being sorted in alphabetical order: Just copy your files with their original filenames to the music folder; upon boot or upon selecting a folder containing such files, they will be renamed following the 3-digit name scheme (as mentioned: in alphabetic order). You can also add files to a music folder later, they will be renamed properly; when you do so, delete the file "TCD_DONE.TXT" from the music folder on the SD card so that the firmware knows that something has changed. The renaming process can take a while (10 minutes for 1000 files in bad cases). Mac users are advised to delete the ._ files from the SD before putting it back into the control board as this speeds up the process.

To start and stop music playback, press button "10" while in _MusicPlayer_ [button mode](#button-modes). Pressing "9" jumps to the previous song, pressing "4C" to the next one. (The same can be achieved by entering codes on the TCD's keypad: 8002 = previous song, 8005 = play/stop, 8008 = next song).

By default, the songs are played in order, starting at 000.mp3, followed by 001.mp3 and so on. _Holding_ button "10" enables Shuffle mode, button "9" disables Shuffle mode. _Holding_ "4C" restarts the player at song number 0. The power-up Shuffle mode can be set up in the Config Portal.

See [here](#music-player-mode) and [here](#tcd-remote-command-reference) for a list of controls of the music player.

While the music player is playing music, other sound effects are disabled/muted. Initiating a time travel stops the music player. The TCD-triggered alarm will, if so configured, sound and stop the music player.

## Connecting a Time Circuits Display

### BTTF-Network ("BTTFN")

The TCD can communicate with the VSR wirelessly, via the built-in "**B**asic-**T**elematics-**T**ransmission-**F**ramework" over WiFi. It can send out information about a time travel and an alarm, and the VSR queries the TCD for speed, temperature and some other data. Furthermore, the TCD's keypad can be used to remote-control the VSR.

| [![Watch the video](https://img.youtube.com/vi/u9oTVXUIOXA/0.jpg)](https://youtu.be/u9oTVXUIOXA) |
|:--:|
| Click to watch the video |

Note that the TCD's firmware must be up to date for BTTFN. You can use [this](http://tcd.out-a-ti.me) one or CircuitSetup's release 2.9 or later.

![BTTFN connection](img/family-wifi-bttfn.png)

In order to connect your VSR to the TCD using BTTFN, just enter the TCD's IP address or hostname in the **_IP address or hostname of TCD_** field in the VSR's Config Portal. On the TCD, no special configuration is required. 
  
Afterwards, the VSR and the TCD can communicate wirelessly and 
- play time travel sequences in sync,
- both play an alarm-sequence when the TCD's alarm occurs,
- the VSR can be remote controlled through the TCD's keypad (command codes 8xxx),
- the VSR queries the TCD for temperature and GPS speed for display,
- the VSR queries the TCD for fake power and night mode, in order to react accordingly if so configured,
- the VSR's Time Travel button can trigger a synchronized Time Travel on all BTTFN-connected devices, just like if that Time Travel was triggered through the TCD.

You can use BTTF-Network and MQTT at the same time, see [below](#home-assistant--mqtt).

### Connecting a TCD by wire

>Note that a wired connection only allows for synchronized time travel sequences, no other communication takes place. A wireless connection over BTTFN/WiFi is much more powerful and therefore recommended over a wired connection.

For a connection by wire, connect GND and GPIO on the VSR's "Time Travel" connector to the TCD like in the table below:

<table>
    <tr>
     <td align="center">VSR</td>
     <td align="center">TCD with control board >=1.3</td>
     <td align="center">TCD with control board 1.2</td>
    </tr>
   <tr>
     <td align="center">GND of "Time Travel" connector</td>
     <td align="center">GND of "Time Travel" connector</td>
     <td align="center">GND of "IO14" connector</td>
    </tr>
    <tr>
     <td align="center">TT of "Time Travel" connector</td>
     <td align="center">TT OUT of "Time Travel" connector</td>
     <td align="center">IO14 of "IO14" connector</td>
    </tr>
</table>

_Do not connect 3V3 to the TCD!_

Next, head to the Config Portal and set the option **_TCD connected by wire_**. On the TCD, the option "Control props connected by wire" must be set.

>You can connect both the TCD and a button to the TT connector. However, the button should not be pressed when the option **_TCD connected by wire_** is set, as it might yield unwanted results. Also, note that the button connects to IO13 and 3_3V (not GND!).

## Home Assistant / MQTT

The VSR supports the MQTT protocol version 3.1.1 for the following features:

### Control the VSR via MQTT

The VSR can - to some extent - be controlled through messages sent to topic **bttf/vsr/cmd**. Support commands are
- TIMETRAVEL: Start a [time travel](#time-travel)
- DISPLAY_PW: Set [display mode](#display-modes) to "pushwheels"
- DISPLAY_TEMP: Set [display mode](#display-modes) to "temperature"
- DISPLAY_SPEED: Set [display mode](#display-modes) to "speed"
- MP_PLAY: Starts the [Music Player](#the-music-player)
- MP_STOP: Stops the [Music Player](#the-music-player)
- MP_NEXT: Jump to next song
- MP_PREV: Jump to previous song
- MP_SHUFFLE_ON: Enables shuffle mode in [Music Player](#the-music-player)
- MP_SHUFFLE_OFF: Disables shuffle mode in [Music Player](#the-music-player)
- MP_FOLDER_x: x being 0-9, set Music Folder number for [Music Player](#the-music-player)

### Receive commands from Time Circuits Display

If both TCD and VSR are connected to the same broker, and the option **_Send event notifications_** is checked on the TCD's side, the VSR will receive information on time travel and alarm and play their sequences in sync with the TCD. Unlike BTTFN, however, no other communication takes place.

![MQTT connection](img/family-wifi-mqtt.png)

MQTT and BTTFN can co-exist. However, the TCD only sends out time travel and alarm notifications through either MQTT or BTTFN, never both. If you have other MQTT-aware devices listening to the TCD's public topic (bttf/tcd/pub) in order to react to time travel or alarm messages, use MQTT (ie check **_Send event notifications_**). If only BTTFN-aware devices are to be used, uncheck this option to use BTTFN as it has less latency.

### Setup

In order to connect to a MQTT network, a "broker" (such as [mosquitto](https://mosquitto.org/), [EMQ X](https://www.emqx.io/), [Cassandana](https://github.com/mtsoleimani/cassandana), [RabbitMQ](https://www.rabbitmq.com/), [Ejjaberd](https://www.ejabberd.im/), [HiveMQ](https://www.hivemq.com/) to name a few) must be present in your network, and its address needs to be configured in the Config Portal. The broker can be specified either by domain or IP (IP preferred, spares us a DNS call). The default port is 1883. If a different port is to be used, append a ":" followed by the port number to the domain/IP, such as "192.168.1.5:1884". 

If your broker does not allow anonymous logins, a username and password can be specified.

Limitations: MQTT Protocol version 3.1.1; TLS/SSL not supported; ".local" domains (MDNS) not supported; server/broker must respond to PING (ICMP) echo requests. For proper operation with low latency, it is recommended that the broker is on your local network. 

## Car setup

If your VSR, along with a [Time Circuits Display](https://tcd.out-a-ti.me/), is mounted in a car, the following network configuration is recommended:

#### TCD

- Run your TCD in [*car mode*](https://tcd.out-a-ti.me/#car-mode);
- disable WiFi power-saving on the TCD by setting **_WiFi power save timer (AP-mode)_** to 0 (zero).

#### VSR

Enter the Config Portal on the VSR, click on *Setup* and
  - enter *192.168.4.1* into the field **_IP address or hostname of TCD_**
  - check the option **_Follow TCD fake power_** if you have a fake power switch for the TCD (like eg a TFC switch)
  - click on *Save*.

After the VSR has restarted, re-enter the VSR's Config Portal (while the TCD is powered and in *car mode*) and
  - click on *Configure WiFi*,
  - select the TCD's access point name in the list at the top or enter *TCD-AP* into the *SSID* field; if you password-protected your TCD's AP, enter this password in the *password* field. Leave all other fields empty,
  - click on *Save*.

Using this setup enables the VSR to receive notifications about time travel and alarm wirelessly, and to query the TCD for data. Also, the TCD keypad can be used to remote-control the VSR.

In order to access the VSR's Config Portal in your car, connect your hand held or computer to the TCD's WiFi access point ("TCD-AP"), and direct your browser to http://vsr.local ; if that does not work, go to the TCD's keypad menu, press ENTER until "BTTFN CLIENTS" is shown, hold ENTER, and look for the VSR's IP address there; then direct your browser to that IP by using the URL http://a.b.c.d (a-d being the IP address displayed on the TCD display).

## Flash Wear

Flash memory has a somewhat limited life-time. It can be written to only between 10.000 and 100.000 times before becoming unreliable. The firmware writes to the internal flash memory when saving settings and other data. Every time you change settings, data is written to flash memory.

In order to reduce the number of write operations and thereby prolong the life of your VSR, it is recommended to use a good-quality SD card and to check **_[Save secondary settings on SD](#-save-secondary-settings-on-sd)_** in the Config Portal; secondary settings are then stored on the SD card (which also suffers from wear but is easy to replace). See [here](#-save-secondary-settings-on-sd) for more information.

## Appendix A: The Config Portal

### Main page

##### &#9654; Configure WiFi

Clicking this leads to the WiFi configuration page. On that page, you can connect your VSR to your WiFi network by selecting/entering the SSID (WiFi network name) as well as a password (WPA2). By default, the VSR requests an IP address via DHCP. However, you can also configure a static IP for the VSR by entering the IP, netmask, gateway and DNS server. All four fields must be filled for a valid static IP configuration. If you want to stick to DHCP, leave those four fields empty.

Note that this page has nothing to do with Access Point mode; it is strictly for connecting your VSR to an existing WiFi network as a client.

##### &#9654; Setup

This leads to the [Setup page](#setup-page).

##### &#9654; Update

This leads to the firmware and audio update page. 

In order to upload a new firmware binary (such as the ones published here in the install/ folder), select that image file in the top file selector and click "Update".

You can also install the VSR's audio data on this page; download the current sound-pack, extract it and select the resulting VSRA.bin file in the bottom file selector. Finally, click "Upload". Note that an SD card is required for this operation.

Note that either a firmware or audio data can be uploaded at once, not both at the same time.

##### &#9654; Erase WiFi Config

Clicking this (and saying "yes" in the confirmation dialog) erases the WiFi configuration (WiFi network and password) and reboots the device; it will restart in "access point" mode. See [here](#short-summary-of-first-steps).

---

### Setup page

#### Basic settings

##### &#9654; Smooth voltage changes

If this option is checked, changing the pushwheels slowly and smoothly adapts the display to the new chosen value. If this is disabled, the display follows the pushwheels immediately.

##### &#9654; Voltage fluctuations

If this option is checked, the displayed pushwheel value slightly and randomly fluctuates. If this option is unchecked, the pushwheel value is statically displayed, without any fluctuations.

##### &#9654; Display button mode on power-up

If this is checked, the VSR briefly shows the current [button mode](#button-modes) upon power-up.

##### &#9654; Lights indicate button mode

If this is checked, the buttons lights permanently reflect the current [button mode](#button-modes), except for _light mode_.

##### &#9654; Brightness level

This selects brightness level for the LED display.

##### &#9654; Screen saver timer

Enter the number of minutes until the Screen Saver should become active when the VSR is idle.

The Screen Saver, when active, disables all lights and the display, until 
- a button on the VSR pressed,
- the time travel button is briefly pressed (the first press when the screen saver is active will not trigger a time travel),
- on a connected TCD, a destination date is entered (only if TCD is wirelessly connected) or a time travel event is triggered (also when wired).

The music player will continue to run.
 
#### Hardware configuration settings

##### Volume level (0-19)

Enter a value between 0 (mute) or 19 (very loud) here. This is your starting point; you can change the volume via the VSR's buttons in _Admin_ [button mode](#button-modes) or via the TCD's keypad (83xx); the new volume will also be saved (and appear in this field when the page is reloaded in your browser).

#### Network settings

##### &#9654; Hostname

The device's hostname in the WiFi network. Defaults to 'vsr'. This also is the domain name at which the Config Portal is accessible from a browser in the same local network. The URL of the Config Portal then is http://<i>hostname</i>.local (the default is http://vsr.local)

If you have more than one VSR in your local network, please give them unique hostnames.

##### &#9654; AP Mode: Network name appendix

By default, if the VSR creates a WiFi network of its own ("AP-mode"), this network is named "VSR-AP". In case you have multiple VSRs in your vicinity, you can have a string appended to create a unique network name. If you, for instance, enter "-ABC" here, the WiFi network name will be "VSR-AP-ABC". Characters A-Z, a-z, 0-9 and - are allowed.

##### &#9654; AP Mode: WiFi password

By default, and if this field is empty, the VSR's own WiFi network ("AP-mode") will be unprotected. If you want to protect your VSR access point, enter your password here. It needs to be 8 characters in length and only characters A-Z, a-z, 0-9 and - are allowed.

If you forget this password and are thereby locked out of your VSR, select _Admin_ [button mode](#button-modes), and _hold_ "4C"; this deletes the WiFi password. Then power-down and power-up your VSR and the access point will start unprotected.

##### &#9654; WiFi connection attempts

Number of times the firmware tries to reconnect to a WiFi network, before falling back to AP-mode. See [here](#short-summary-of-first-steps)

##### &#9654; WiFi connection timeout

Number of seconds before a timeout occurs when connecting to a WiFi network. When a timeout happens, another attempt is made (see immediately above), and if all attempts fail, the device falls back to AP-mode. See [here](#short-summary-of-first-steps)

#### Settings for Night Mode

##### &#9654; Display off

If this is checked, the LED display is switched off in night mode. Otherwise, it will be dimmed.

#### Settings for temperature display

##### &#9654; Display in °Celsius

Selects between Fahrenheit and Celsius for temperature display. This settings is used for temperature both from a sensor connected to the VSR as well as temperature transmitted by a TCD.

##### &#9654; Sensor Offset

This offset, which can range from -3.0 to 3.0, is added to the sensor measurement, in order to compensate sensor inaccuracy or suboptimal sensor placement. This offset is only applied to a value read from a sensor connected to the VSR, and ignored when temperature is transmitted from a TCD; the TCD has its own Offset setting which is applied to the temperature reading before transmission.

#### Settings for prop communication/synchronization

##### &#9654; TCD connected by wire

Check this if you have a Time Circuits Display connected by wire. Note that a wired connection only allows for synchronized time travel sequences, no other communication takes place.

While you can connect both a button and the TCD to the "time travel" connector on the VSR, the button should not be pressed when this option is set, as it might yield unwanted effects.

Do NOT check this option if your TCD is connected wirelessly (BTTFN, MQTT).

##### &#9654; TCD signals Time Travel without 5s lead

Usually, the TCD signals a time travel with a 5 seconds lead, in order to give a prop a chance to play an acceleration sequence before the actual time travel takes place. Since this 5 second lead is unique to CircuitSetup props, and people sometimes want to connect third party props to the TCD, the TCD has the option of skipping this 5 seconds lead. If that is the case, and your VSR is connected by wire, you need to set this option.

If your VSR is connected wirelessly, this option has no effect.

##### &#9654; IP address or hostname of TCD

If you want to have your VSR to communicate with a Time Circuits Display wirelessly ("BTTF-Network"), enter the IP address of the TCD here. If your TCD is running firmware version 2.9.1 or later, you can also enter the TCD's hostname here instead (eg. 'timecircuits').

If you connect your VSR to the TCD's access point ("TCD-AP"), the TCD's IP address is 192.168.4.1.

##### &#9654; Follow TCD night-mode

If this option is checked, and your TCD goes into night mode, the VSR will disable or dim the display and the button lights, and reduce its audio volume.

##### &#9654; Follow TCD fake power

If this option is checked, and your TCD is equipped with a fake power switch, the VSR will also fake-power up/down. If fake power is off, the VSR will be dark and it will ignore all input from buttons.

##### &#9654; TT buttons trigger BTTFN-wide TT

If the VSR is connected to a TCD through BTTFN, this option allows to trigger a synchronized time travel on all BTTFN-connected devices when pressing "9" in _Operation_ [button mode](#button-modes), or pressing the Time Travel button, just as if the Time Travel was triggered by the TCD. If this option is unchecked, these actions only trigger a Time Travel sequence on the VSR.

#### Audio-visual options

##### &#9654; Play time travel sounds

If other props are connected, they might bring their own time travel sound effects. In this case, you can uncheck this to disable the VSR's own time travel sounds. Note that this only covers sounds played during time travel, not other sound effects.

##### &#9654; Play TCD-alarm sounds

If a TCD is connected via BTTFN or MQTT, the VSR visually signals when the TCD's alarm sounds. If you want the VSR to play an alarm sound, check this option.

#### Home Assistant / MQTT settings

##### &#9654; Use Home Assistant (MQTT 3.1.1)

If checked, the VSR will connect to the broker (if configured) and send and receive messages via [MQTT](#home-assistant--mqtt)

##### &#9654; Broker IP[:port] or domain[:port]

The broker server address. Can be a domain (eg. "myhome.me") or an IP address (eg "192.168.1.5"). The default port is 1883. If different port is to be used, it can be specified after the domain/IP and a colon ":", for example: "192.168.1.5:1884". Specifying the IP address is preferred over a domain since the DNS call adds to the network overhead. Note that ".local" (MDNS) domains are not supported.

##### &#9654; User[:Password]

The username (and optionally the password) to be used when connecting to the broker. Can be left empty if the broker accepts anonymous logins.

#### Music Player settings

##### &#9654; Music folder

Selects the current music folder, can be 0 through 9. This can also be set/changed through a TCD keypad via BTTFN.

##### &#9654; Shuffle at startup

When checked, songs are shuffled when the device is booted. When unchecked, songs will be played in order.

#### Other settings

##### &#9654; Save secondary settings on SD

If this is checked, secondary settings (volume, brightness, button mode) are stored on the SD card (if one is present). This helps to minimize write operations to the internal flash memory and to prolong the lifetime of your VSR. See [Flash Wear](#flash-wear).

Apart from Flash Wear, there is another reason for using an SD card for settings: Writing data to internal flash memory can cause delays of up to 1.5 seconds, which interrupt sound playback and have other undesired effects. The VSR needs to save data from time to time, so in order for a smooth experience without unexpected and unwanted delays, please use an SD card and check this option.

It is safe to have this option checked even with no SD card present.

If you want copy settings from one SD card to another, do as follows:
- With the old SD card still in the slot, enter the Config Portal, turn off _Save secondary settings on SD_, and click "SAVE".
- After the VSR has rebooted, power it down, and swap the SD card for your new one.
- Power-up the VSR, enter the Config Portal, re-enable _Save secondary settings on SD_, and click "SAVE".

This procedure ensures that all your settings are copied from the old to the new SD card.


_Text & images: (C) Thomas Winischhofer ("A10001986"). See LICENSE._ Source: https://vsr.out-a-ti.me
