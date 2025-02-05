This folder holds all files necessary for immediate installation on your VSR. Here you'll find
- a binary of the current firmware, ready for upload to the device;
- the latest audio data

## Firmware Installation

If a previous version of the VSR firmware is installed on your device, you can update easily using the pre-compiled binary. Enter the Config Portal, click on "Update" and select the pre-compiled binary file provided in this repository ([install/vsr-A10001986.ino.nodemcu-32s.bin](https://github.com/realA10001986/VSR/blob/main/install/vsr-A10001986.ino.nodemcu-32s.bin)). 

If you are using a fresh ESP32 board, please see [vsr-A10001986.ino](https://github.com/realA10001986/VSR/blob/main/vsr-A10001986/vsr-A10001986.ino) for detailed build and upload information, or, if you don't want to deal with source code, compilers and all that nerd stuff, go [here](https://install.out-a-ti.me) and follow the instructions.

## Audio data installation

The firmware comes with some audio data ("sound-pack") which needs to be installed separately. The audio data is not updated as often as the firmware itself. If you have previously installed the latest version of the sound-pack, you normally don't have to re-install the audio data when you update the firmware. Only if the VSR displays "AUD" briefly during boot, an update of the audio data is needed.

The first step is to download "install/sound-pack-xxxxxxxx.zip" and extract it. It contains one file named "VSRA.bin".

Then there are two alternative ways to proceed. Note that both methods *require an SD card*.

1) Through the Config Portal. Click on *Update*, select the "VSRA.bin" file in the bottom file selector and click on *Upload*. Note that an SD card must be in the slot during this operation.

2) Via SD card:
- Copy "VSRA.bin" to the root directory of of a FAT32 formatted SD card,
- power down the VSR,
- insert this SD card into the slot and 
- power up the VSR; the audio data will be installed automatically.

See also [here](https://github.com/realA10001986/VSR/blob/main/README.md#audio-data-installation).
