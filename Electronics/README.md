Electronics for the VSR

The VSR consists of four PCBs:
- a Control Board, carrying the ESP32
- a pushwheel controller, to which the pushwheels are attached
- two PCBs for the displays: a controller and a "tube carrier".

Folders contain each
- Gerbers for production at eg. JLCPCB (.zip)
- BOMs for the PCB's (.csv)
- PickAndPlace files for production

To have JCLPCB make your boards:
1) Create an account at jlcpcb.com
2) Click "Upload Gerber file" or "order now"
3) Upload the Gerber file (.zip) for the PCB you want to make; leave all options at their defaults. You can choose a PCB color though...
4) Activate "PCM assembly", click "NEXT"
5) Upload the BOM and "PickAndPlace" files, click NEXT
6) Read the remarks regarding the BOM files in each of the folders here
7) Finalize order.
