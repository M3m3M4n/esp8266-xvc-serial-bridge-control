# ***ESP8266 based control unit, modded on EBAZ420X***

## Features
- Remotely hardware bootmode selection and reset
- Remote XVC and serial bridge:
    - ESP8266 XVC inplementation from:
        - https://github.com/gtortone/esp-xvcd
        - https://github.com/kholia/xvc-esp8266

![Features](client/imgs/features.png)

## Pinouts
***ALL PINOUT REFERENCES BASED ON NODEMCU ESP8266, ADAPT IF NEEDED***

![Pinout](imgs/ESP8266-NodeMCU-kit-12-E-pinout-gpio-pin.webp)

### JTAG
- tms = D2;
- tck = D5;
- tdo = D6;
- tdi = D7;

### Serial
- Board's serial connected to ESP8266 serial port 0

### Misc
- D0 connect to MR pin of u65 through a 1k resistor, serve as board reset.
- D1 connect to R2584-U12-IO0 through a 1k resistor, serve as bootmode selector from ESP8266
- D4 has a 20k pullup, connected to a push switch (bootmode selector button) to GND

## Default ports
- Command port: 42069
- Serial passthrough: 2222
- XVC: 2542


