## Electronics for the VSR

The VSR electronics consist of four PCBs:
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

You additionally need:
- NodeMCU ESP32S dev board
- Screw terminals and XH connector on the back of the Control Board; JLCPCB can't provide them because they are on the back of the board:
  - 2x DG308-2.54-02P-14-00A(H) (or any other; 2.54mm pitch)
  - 1x DG308-2.54-04P-14-00A(H) (or any other; 2.54mm pitch)
  - 1x XH-4AWD connector; this is optional and only for connecting a temperature sensor.
- XH 4pin cables to connect the Control Board, the Pushwheels and the LED display
- XH 4pin cables to connect the three buttons (2xbutton, 2xlight)
- 3x KM1 pushwheels (22x8mm)
