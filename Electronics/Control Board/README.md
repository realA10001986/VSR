
Regarding the BOM (Bill of Materials):

1) "ESP32" is the required ESP32 dev board (NodeMCU ESP32S) and it is intentionally missing in the BOM. The ESP32 dev board should not be soldered to the PCB directly; use [1x19P female pin headers](https://www.lcsc.com/product-detail/Female-Headers_CONNFLY-Elec-DS1023-1x19SF11_C7509529.html) (pitch 2.54mm).
2) You need to place EITHER L1 OR L2, not both. These are two alternative components, and they share the same physical location.
