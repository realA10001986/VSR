## VSR Control Board

To have [JCLPCB](https://jlcpcb.com) make your Control Board:
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

1) "ESP32" is the required ESP32 dev board (NodeMCU ESP32S) and it is intentionally missing in the BOM. The ESP32 dev board should not be soldered to the PCB directly; use [1x19P female pin headers](https://www.lcsc.com/product-detail/Female-Headers_CONNFLY-Elec-DS1023-1x19SF11_C7509529.html) (pitch 2.54mm). When JLCPCB complains about a "missing data" after processing the BOM, click "Continue".
2) You need to place _either_ "L1" _or_ "L2", not both. These are two alternative components, and they share the same physical location. In the "Bill of Materials" tab, deselect L1 or L2.
3) When clicking "Next", JLCPCB will complain about "unselected parts". Click "Do not place".
