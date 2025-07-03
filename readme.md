# Flipper Zero M-Powered Module App

This app is used to communicate with M-Powered Flipper Zero modules via GPIO.

## State

| Feature  | Mark    |
|----------|---------|
| Compiles | &#9745; |
| Runs     | &#9745; |
| Works    | &#9746; |
| No bugs  | &#9746; |

## Building

1. Install [Python 3](https://www.python.org/downloads/).
2. Setup [µFBT](https://github.com/flipperdevices/flipperzero-ufbt).
3. Run `ufbt` in the root project directory.

## Testing on Flipper Zero

### Making a M-Powered module for the Flipper Zero

#### Prerequisite parts / tools

- Flipper Zero
- Heltec V3 module
  - [Amazon (with antenna and 3000mAh battery included)](https://a.co/d/fCDG76Z).
  - [Official (requires antenna and external power source to be bought separately)](https://heltec.org/project/wifi-lora-32-v3/).
- LoRa band antenna compatible with the Heltec V3
  - In North America you'll likely want a 915 MHz antenna.
  - In the European Union you'll likely want 868 MHz antenna.
- External 3.7v power source for the Heltec V3 module
  - You're probably gonna want to get a 3.7v battery.
- Wire strippers.
- Solder
- Soldering iron (don't cheap out too bad if you don't want a hard time).
  - I recommend a [Pinecil V2](https://pine64.com/product/pinecil-smart-mini-portable-soldering-iron/).
- GPIO connection options
  - "Better solution"
    - A large enough prototyping PCB.
      - [The assortment of prototype PCBs I use](https://a.co/d/6FHHYWm).
    - 90 degree pin headers.
    - Insulated copper wire (30 gauge).

  ***OR***
  - "Solutioning" (Won't look as good and will take longer to setup each time you want to plug it into your Flipper Zero)
    - Insulated copper wire (30 gauge).

#### Wiring guide

1. Safety first (also look up some soldering guides if your not familiar with soldering yet)!
2. Solder wires to the following Heltec V3 GPIO pins **5** (RX), **6** (TX), **1** (GND).
3. These should be connected to the Flipper Zero's pins in the following arrangement:
   - Heltec V3: 5 (RX) -> Flipper Zero: 13 (TX)
   - Heltec V3: 6 (TX) -> Flipper Zero: 14 (RX)
   - Heltec V3: 1 (GND) -> Flipper Zero: 8, 11, or 18 (GND)
