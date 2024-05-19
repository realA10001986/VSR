## The Pushwheel carrier PCB

<img src="img/pw1.jpg">

The KM1 (22x8mm) pushwheels can be attached by using 2mm pitched pin sockets and 90 deg headers. There are two rows of solder pads; for the pin sockets, use the left one with 12 pads (12 because 12-pin pin sockets are more easily available than 11-pin versions, and you don't have to make ugle cuts)

<img src="img/pw2.jpg">

The two four-pin XH connectors are for connecting the LED display and the Control Board.

To have [JCLPCB](https://jlcpcb.com) make your Pushwheels Board:
1) Create an account at jlcpcb.com
2) Click "Upload Gerber file" or "order now"
3) Upload the Gerber file (.zip, do not decompress!) for the PCB you want to make; leave all options at their defaults. You can choose a PCB color though...
4) Activate "PCB assembly", click "NEXT"
5) Enjoy a view of the PCB, click "NEXT"
6) Upload the BOM and "PickAndPlace" (CPL) files, click "Process BOM & CPL"
7) Read the remarks regarding the BOM below
8) Enjoy a nice 2D or 3D view of your future board, click "NEXT". (If the display stalls at "Processing files", click "NEXT" regardless).
9) Select a "product description" (eg. "Movie prop") and click "Save to cart". Then finalize your order.

#### Remarks on BOM (Bill of Materials):

- After processing BOM & CPL, JLCPCB will complain about "missing data" for PW1-PW3 and BUTTONS. PW1-PW3 are 12-pin 2mm pitch pin sockers, and they can't be assembled by JLCPCB. BUTTONS is not needed. Click "Continue".

