## Electronics for the VSR

The VSR electronics consist of four PCBs:
- a Control Board, carrying the ESP32
- a pushwheel controller, to which the pushwheels are attached
- two PCBs for the display: a controller and a "tube carrier".

Folders contain each
- Gerbers for production at eg. JLCPCB (.zip)
- BOMs for the PCB's (.csv)
- PickAndPlace files for production

To have [JCLPCB](https://jlcpcb.com) make your boards, see the READMEs in the folders.

You additionally need:
- 1x NodeMCU ESP32S dev board; 2x [19pin femals headers](https://www.lcsc.com/product-detail/Female-Headers_CONNFLY-Elec-DS1023-1x19SF11_C7509529.html), 2.54mm pitch. If you can't get them for exactly 19 pins, get some longer ones and cut them off.
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
