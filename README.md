# uc_divider
Arduino based Clock Divider

![Clock Divider Eurorack Module](https://www.davidhaillant.com/l/uploads/medium/4edd79a91e22cabfbe472f6c330ce4c0.jpg)

Several firmware variants are available.

## uc_divider_24ppqn_polyrhythm
24 PPQN Polyrhythm Divider

* Designed for 24 PPQN clock input
* RESET realigns all phases


## uc_divider_polymeter
Divider / Polymeter

* Independent clock divisions per output
* RESET clears all counters


## uc_ring_counter
Ring Counter

* One active output at a time
* Advances to the next output on each clock pulse
* RESET returns to output 0


## uc_multi_firmware
**New!** Multi-firmware combination (many thanks Kristian!)

Firmware switching:
* Hold RESET + CLOCK together for 5 seconds
* Active firmware is saved to EEPROM and restored on power-up

## Links
* [Schematics and documentation](https://www.davidhaillant.com/uc-clock-divider/)
* [Buy PCB and kits of UC ClockDivier](https://shop.davidhaillant.com/product/uc-clock-divider/)