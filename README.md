VectorStrum — Motion-Sensed Air Guitar

VectorStrum is a wireless “air guitar” you can actually play.
A keypad selects chords, an IMU detects your strums, and a sound engine turns motion into guitar audio.

TX 1 (Keypad Nano): 4×3 keypad → nRF24L01

TX 2 (IMU Nano): MPU-6050 motion (strum) → nRF24L01

Sound Engine:

Option A: Python on your PC (recommended) via a simple RF→USB Gateway

Option B: ESP32 + MAX98357A I²S amp (legacy firmware still included)

https://github.com/n00rtahsin/Vectorstrum

✨ Features

Real-time plucked-string synthesis (Karplus–Strong, 6 strings)

Chord palette on 4×3 keypad (1..0, *=mute, #=sustain)

Robust radio link (nRF24): channel 76, 1 Mbps, AutoAck, retries

IMU strum detection with velocity + collision-avoidant delayed send

2-second “recent chord” gate (prevents stray strums from old chords)

Verbose serial logs on all nodes for easy debugging



🧩 System Overview
[4x3 Keypad] --(GPIO)--> [Nano #1] --(nRF24 pipe "KEY01")--> 
                                                           \
                                                            \ 
                                                             >==[ RF Gateway ]==USB==> [Python Sound Engine]
                                                            /
[MPU-6050 IMU] --(I²C)--> [Nano #2] --(nRF24 pipe "IMU01")-/


Radio settings (all radios identical):

Address/Pipes: KEY01 (keypad) and IMU01 (IMU)

Channel: 76

Data rate: 1 Mbps

CRC: 16-bit

AutoAck: ON

Retries: (5,15)

Payloads: fixed 3 bytes

Keypad: {'K', keyChar, seq}

IMU: {'S', velInt8, seq}

🛒 Bill of Materials

2× Arduino Nano (ATmega328P)

2× nRF24L01(+) radio modules (+ 10–47 µF electrolytic cap each)

1× 4×3 matrix keypad

1× MPU-6050 (GY-521)

Option A (Python): 1× Arduino (Nano/Uno/ESP32) + nRF24 for the RF Gateway

Option B (ESP32): 1× ESP32 DevKit (CP2102) + MAX98357A I²S DAC/amp + speaker

Breadboard, jumpers, micro-USB cables

🔌 Wiring (exact pin maps)
Nano #1 — Keypad Transmitter

Keypad rows → D3, D4, D5, D6

Keypad cols → D2, D9, A0 (do NOT use D10)

nRF24 → CE D8, CSN D7, SCK D13, MOSI D11, MISO D12, VCC 3.3 V, GND

Keep D10 = OUTPUT + HIGH (forces SPI master on AVR)

Add 10–47 µF cap across radio 3.3 V–GND

Nano #2 — IMU (MPU-6050) Transmitter

GY-521 → VCC 5 V, GND, SDA A4, SCL A5 (XDA/XCL unconnected)

nRF24 → CE D8, CSN D7, SCK D13, MOSI D11, MISO D12, VCC 3.3 V, GND

D10 = OUTPUT + HIGH; 10–47 µF cap on radio

RF→USB Gateway (for Python engine)

Any Arduino (Nano/Uno/ESP32) + nRF24 wired like above

Opens two RX pipes (KEY01 / IMU01) and forwards raw 3-byte packets to the PC over USB serial (115200)

(Optional) ESP32 + MAX98357A (legacy)

nRF24 CE=D27, CSN=D14, SCK D18, MOSI D23, MISO D19, 3.3 V

MAX98357A: BCLK D26, LRCLK D25, DIN D22, VIN 5 V, GND, SD= D21 (mute LOW at boot)

🧪 Quickstart (Python Sound Engine)

Flash the transmitters (Arduino IDE):

Install libraries: RF24, Keypad

Open and upload:

firmware/keypad_nano_tx/Keypad_Nano_TX.ino

firmware/imu_nano_tx_delayed/IMU_Nano_TX_Delayed.ino

Serial Monitors should show heartbeats. Press keypad → see key=… ACK. Shake IMU → SEND vel=… ACK.

Flash the RF Gateway:

Open firmware/rf_gateway/RF_Gateway.ino and upload to your gateway Arduino.

Serial Monitor should say:
RF GATEWAY listening: pipe0=KEY01 pipe1=IMU01 ch76 1Mbps

Install Python deps:

pip install sounddevice numpy pyserial


Run engine:

Edit python/VectorStrum_engine.py: set COM_PORT to your gateway’s port (e.g., COM5).

Start:

python python/VectorStrum_engine.py


You’ll see Audio stream running…. Press keypad (choose a chord) then strum IMU → sound!

🎛️ Controls

Keypad 1..0: set chord

*: hard mute (Python prints MUTE ON)

#: toggle sustain (longer decay)

Strum gate: IMU S packets only pluck if within 2 s of last chord

Chord map (low E..high E, -1=mute, 0=open):
1 C: {-1,3,2,0,1,0}
2 Dm: {-1,-1,0,2,3,1}
3 Em: {0,2,2,0,0,0}
4 F: {1,3,3,2,1,1}
5 G: {3,2,0,0,0,3}
6 Am: {-1,0,2,2,1,0}
7 Bm: {-1,2,4,4,3,2}
8 C: {-1,3,2,0,1,0}
9 D: {-1,-1,0,2,3,2}
0 E: {0,2,2,1,0,0}

🛰️ Packet Protocol

All packets are 3 bytes:

Keypad: {'K', keyChar, seq}

Example: 'K', '5', 12

IMU: {'S', velInt8, seq}

velInt8 ∈ [-100,100] (sign = strum direction, magnitude = velocity)

Each transmitter maintains its own 8-bit seq; the receiver drops dupes.

⚙️ Build Details
Arduino IDE settings

Boards:

Arduino Nano (328P, Old Bootloader if needed)

ESP32 Dev Module (if using legacy engine)

Baud: 115200 (all sketches)

Libraries:

RF24 (by TMRh20)

Keypad (by Mark Stanley & Alexander Brevig)

Python engine

sounddevice (PortAudio), numpy, pyserial

Defaults: SR=22050, BLOCK=256, latency='low'

To choose audio device:

import sounddevice as sd
print(sd.query_devices())


Then set DEVICE = index in the script.

🔧 RF→USB Gateway (sketch)

This tiny gateway opens both pipes and forwards raw packets to USB serial (no parsing):

// firmware/rf_gateway/RF_Gateway.ino
#include <SPI.h>
#include <RF24.h>
#define CE 9
#define CSN 10
RF24 radio(CE, CSN);
const byte ADDR_KEY[6]="KEY01", ADDR_IMU[6]="IMU01";
void setup(){
  Serial.begin(115200);
  if(!radio.begin()){ Serial.println("RF begin FAIL"); while(1){} }
  radio.setAddressWidth(5);
  radio.setChannel(76);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_LOW);
  radio.setAutoAck(true);
  radio.setCRCLength(RF24_CRC_16);
  radio.disableDynamicPayloads();
  radio.setPayloadSize(3);
  radio.openReadingPipe(0, ADDR_KEY);
  radio.openReadingPipe(1, ADDR_IMU);
  radio.startListening();
  Serial.println("RF GATEWAY listening: pipe0=KEY01 pipe1=IMU01 ch76 1Mbps");
}
void loop(){
  uint8_t pipe;
  while (radio.available(&pipe)){
    uint8_t p[3]; radio.read(p,3);
    // forward exactly 3 bytes to host; prefix pipe id for logging if you want
    Serial.write(p,3);
  }
}


Use any AVR/ESP32 for the gateway. If using ESP32 and upload issues occur with CE/CSN on D4/D5, move to D27/D14.

🧪 Smoke Tests (optional but handy)

If radios act up, first verify the link with a bare counter TX/RX pair:

firmware/test_tx_counter/ (Nano) → sends uint32_t counter

firmware/test_rx_counter/ (ESP32 or Nano) → prints received counter

Both should be ch76, 1 Mbps, ADDR "TST01". If this fails, fix wiring/power before trying VectorStrum.

🩺 Troubleshooting

Keypad prints heartbeat only → 3rd column must be A0 (not D10). D10 must be OUTPUT/HIGH.

TX shows FAIL (no ACK):

Common GND everywhere; radio 3.3 V only; add 10–47 µF cap at module

Check CE/CSN pins & addresses/pipes; all radios on ch76, 1 Mbps

Try closer range; set RF24_PA_MIN for bench testing

ESP32 won’t flash (boot mode error):

Unplug radio (CE/CSN on strap pins) or use CE=D27, CSN=D14 and rewire

Hold BOOT, tap EN, keep holding BOOT until upload starts

Python audio clicks/latency:

Try BLOCK=512 or BLOCK=128

Lower MIX_GAIN (e.g., 0.18)

Pick the right device (ASIO/WASAPI on Windows)

IMU too sensitive / not sensitive:

Tweak THRESH_G (1.4…2.2) and REFRACT_MS (80…150)



MIT (or your preferred license—update this section accordingly).

🙌 Credits
Md Nazmun Nur

RF24 by TMRh20

Keypad lib by Mark Stanley & Alexander Brevig

Inspiration: classic Karplus–Strong plucked-string synthesis



On-board OLED chord/velocity HUD

Web UI to configure thresholds and chord sets
