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
3) Upload the Gerber file (.zip, do not decompress!) for the PCB you want to make; leave all options at their defaults. You can choose a PCB color though...
4) Activate "PCM assembly", click "NEXT"
5) Upload the BOM and "PickAndPlace" files, click NEXT
6) Read the remarks regarding the BOM files in each of the folders here
7) Finalize order.

You additionally need:
- 1x NodeMCU ESP32S dev board; 2x 19pin femals headers, 2.54mm pitch
- Screw terminals and XH connector on the back of the Control Board:
  - 2x DG308-2.54-02P-14-00A(H) (or any other; 2.54mm pitch, 2 pins)
  - 1x DG308-2.54-04P-14-00A(H) (or any other; 2.54mm pitch, 4 pins; optional: for time travel button)
  - 1x XH-4AWD connector; this is optional and only for connecting a temperature sensor.
- 3x KM1 pushwheels (22x8mm)
- 3x 9mm/0.36" red single digit LED segment tubes
- 2x 11pin 2mm pitch male and female headers, for LED display PCBs
- 3x 11 or 12 pin 2mm pitch male (90 degrees) and female (0 degrees) pin headers, for pushwheels
- XH 4pin cables to connect the Control Board, the Pushwheels and the LED display
- XH 4pin cables to connect the three buttons (2xbutton, 2xlight)
- the ability to solder through-the-hole parts, and the required tools.
