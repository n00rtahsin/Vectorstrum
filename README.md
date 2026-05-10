<div align="center">

```
 ██╗   ██╗███████╗ ██████╗████████╗ ██████╗ ██████╗ ███████╗████████╗██████╗ ██╗   ██╗███╗   ███╗
 ██║   ██║██╔════╝██╔════╝╚══██╔══╝██╔═══██╗██╔══██╗██╔════╝╚══██╔══╝██╔══██╗██║   ██║████╗ ████║
 ██║   ██║█████╗  ██║        ██║   ██║   ██║██████╔╝███████╗   ██║   ██████╔╝██║   ██║██╔████╔██║
 ╚██╗ ██╔╝██╔══╝  ██║        ██║   ██║   ██║██╔══██╗╚════██║   ██║   ██╔══██╗██║   ██║██║╚██╔╝██║
  ╚████╔╝ ███████╗╚██████╗   ██║   ╚██████╔╝██║  ██║███████║   ██║   ██║  ██║╚██████╔╝██║ ╚═╝ ██║
   ╚═══╝  ╚══════╝ ╚═════╝   ╚═╝    ╚═════╝ ╚═╝  ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝ ╚═╝     ╚═╝
```

**Play air guitar. For real.**

*Motion-sensed wireless air guitar powered by Arduino Nanos, nRF24L01 radios, MPU-6050 IMU, and Karplus–Strong string synthesis in Python.*

---

![License](https://img.shields.io/badge/license-Apache%202.0-blue?style=flat-square)
![Arduino](https://img.shields.io/badge/Arduino-C%2B%2B-teal?style=flat-square&logo=arduino)
![Python](https://img.shields.io/badge/Python-3.8%2B-yellow?style=flat-square&logo=python)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey?style=flat-square)

</div>

---

## 🎸 What is Vectorstrum?

Vectorstrum is a hardware + software system that turns thin air into music. Strap on a keypad to select chords. Hold the IMU unit and *strum* — a real swing of your arm. A wireless radio link carries both signals to a sound engine that synthesizes six-string guitar audio using Karplus–Strong physical modelling, in real time.

No guitar. No strings. Just motion, math, and radio waves.

> **"And Air Guitar with microcontrollers and Python"** — because sometimes the best instrument is the one you mime.

---

## ✨ Features

| Feature | Detail |
|---|---|
| 🎵 **Physical string synthesis** | Karplus–Strong algorithm, 6 independent string voices |
| 🎹 **Chord palette** | 10 chords on a 4×3 keypad (keys `1`–`0`) |
| 📡 **Robust wireless link** | nRF24L01, channel 76, 1 Mbps, AutoAck + retries |
| 🤸 **Strum detection** | MPU-6050 IMU with velocity scaling and direction sensing |
| ⏱️ **Strum gate** | 2-second chord validity window prevents phantom strums |
| 🔇 **Mute & Sustain** | `*` = hard mute, `#` = toggle sustain (longer decay) |
| 🐛 **Debug-friendly** | Verbose serial logs on every node |
| 🖥️ **Dual sound engine** | Option A: Python PC engine · Option B: ESP32 + I²S amp |

---

## 🗺️ System Architecture

```
┌──────────────────┐        nRF24L01        ┌────────────────────────────┐
│  Arduino Nano #1 │  ──── pipe "KEY01" ──►  │                            │
│  4×3 Keypad      │                          │   RF → USB Gateway         │
└──────────────────┘                          │   (Any Arduino + nRF24)    │
                                              │                            │
┌──────────────────┐        nRF24L01         │                            │
│  Arduino Nano #2 │  ──── pipe "IMU01" ──►  │                            │
│  MPU-6050 IMU    │                          └────────────┬───────────────┘
└──────────────────┘                                       │ USB Serial (115200)
                                                           │
                                               ┌───────────▼───────────────┐
                                               │  Python Sound Engine      │
                                               │  (Karplus–Strong × 6)     │
                                               │  sounddevice + numpy       │
                                               └───────────────────────────┘
```

**All radios share identical settings:**

| Parameter | Value |
|---|---|
| Channel | 76 |
| Data rate | 1 Mbps |
| CRC | 16-bit |
| AutoAck | ON |
| Retries | 5 delays, 15 attempts |
| Payload | Fixed 3 bytes |

**Packet format:**

```
Keypad TX  →  [ 'K' | keyChar | seq ]
IMU TX     →  [ 'S' | velInt8 | seq ]    velInt8 ∈ [-100, 100]
```

The receiver drops duplicate `seq` values to prevent double-plucks.

---

## 🛒 Bill of Materials

| Qty | Component | Notes |
|---|---|---|
| 2× | Arduino Nano (ATmega328P) | Old Bootloader variant if needed |
| 2× | nRF24L01(+) radio modules | Add 10–47 µF electrolytic cap per module |
| 1× | 4×3 Matrix Keypad | Standard membrane type |
| 1× | MPU-6050 breakout (GY-521) | I²C IMU |
| 1× | Arduino Nano/Uno/ESP32 | RF→USB Gateway only |
| — | Breadboard, jumpers, USB cables | — |
| *(Optional)* | ESP32 DevKit + MAX98357A | Legacy standalone sound engine |
| *(Optional)* | Small speaker | For ESP32 I²S output |

**Total cost estimate: ~$15–25 USD for the core build.**

---

## 🔌 Wiring Guide

### Nano #1 — Keypad Transmitter

```
Keypad rows  →  D3, D4, D5, D6
Keypad cols  →  D2, D9, A0        ⚠️ Column 3 MUST be A0 — NOT D10
nRF24 CE     →  D8
nRF24 CSN    →  D7
nRF24 SCK    →  D13
nRF24 MOSI   →  D11
nRF24 MISO   →  D12
nRF24 VCC    →  3.3 V  (with 10–47 µF cap to GND)
D10          →  OUTPUT, kept HIGH  (forces SPI master mode on AVR)
```

### Nano #2 — IMU (MPU-6050) Transmitter

```
GY-521 VCC   →  5 V
GY-521 GND   →  GND
GY-521 SDA   →  A4
GY-521 SCL   →  A5
(XDA / XCL leave unconnected)

nRF24        →  same pins as Nano #1
D10          →  OUTPUT, kept HIGH
```

### RF→USB Gateway

Same nRF24 wiring as above. Opens both `KEY01` and `IMU01` pipes in RX mode and forwards raw 3-byte packets over USB serial at 115200 baud.

### *(Optional)* ESP32 + MAX98357A

```
nRF24  CE=D27, CSN=D14, SCK=D18, MOSI=D23, MISO=D19
MAX98357A  BCLK=D26, LRCLK=D25, DIN=D22
MAX98357A  VIN=5V, SD=D21 (keep LOW at boot to mute)
```

---

## ⚡ Quickstart

### 1. Flash the Transmitters

Install these Arduino libraries first (via Library Manager):
- **RF24** by TMRh20
- **Keypad** by Mark Stanley & Alexander Brevig

Then upload:

```
chord.ino / strum.ino  →  Nano #1 (Keypad TX)
soundengine.ino        →  Nano #2 (IMU TX)
```

Open Serial Monitor (115200). You should see heartbeat messages. Press a key → `key=...  ACK`. Shake the IMU → `SEND vel=...  ACK`.

### 2. Flash the RF→USB Gateway

Upload the gateway sketch (see [Gateway Sketch](#-rfusb-gateway-sketch)) to any spare Arduino or ESP32.

Serial Monitor should print:

```
RF GATEWAY listening: pipe0=KEY01 pipe1=IMU01 ch76 1Mbps
```

### 3. Install Python Dependencies

```bash
pip install sounddevice numpy pyserial
```

### 4. Run the Sound Engine

Edit `gyt.py` and set your gateway's serial port:

```python
COM_PORT = "COM5"   # Windows: "COM5" | Linux/Mac: "/dev/ttyUSB0"
```

Then run:

```bash
python gyt.py
```

You'll see `Audio stream running…`. Select a chord on the keypad, then strum the IMU — and hear it.

---

## 🎛️ Controls Reference

| Key | Action |
|---|---|
| `1` | C major |
| `2` | D minor |
| `3` | E minor |
| `4` | F major |
| `5` | G major |
| `6` | A minor |
| `7` | B minor |
| `8` | C major (alt voicing) |
| `9` | D major |
| `0` | E major |
| `*` | Hard mute (silence all strings) |
| `#` | Toggle sustain (extended decay) |

### Chord Voicings (low E → high E, fret numbers, `-1` = muted)

```
C  major  →  -1, 3, 2, 0, 1, 0
D  minor  →  -1,-1, 0, 2, 3, 1
E  minor  →   0, 2, 2, 0, 0, 0
F  major  →   1, 3, 3, 2, 1, 1
G  major  →   3, 2, 0, 0, 0, 3
A  minor  →  -1, 0, 2, 2, 1, 0
B  minor  →  -1, 2, 4, 4, 3, 2
D  major  →  -1,-1, 0, 2, 3, 2
E  major  →   0, 2, 2, 1, 0, 0
```

> **Strum gate:** IMU packets only trigger plucks within **2 seconds** of the last chord selection. This prevents stray strums from playing the wrong chord or firing after a long pause.

---

## 📡 RF→USB Gateway Sketch

```cpp
#include <SPI.h>
#include <RF24.h>

#define CE  9
#define CSN 10

RF24 radio(CE, CSN);

const byte ADDR_KEY[6] = "KEY01";
const byte ADDR_IMU[6] = "IMU01";

void setup() {
  Serial.begin(115200);
  if (!radio.begin()) {
    Serial.println("RF begin FAIL");
    while (1) {}
  }
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

void loop() {
  uint8_t pipe;
  while (radio.available(&pipe)) {
    uint8_t p[3];
    radio.read(p, 3);
    Serial.write(p, 3);   // Forward raw 3 bytes to Python engine
  }
}
```

> **ESP32 gateway note:** If CE/CSN on D4/D5 cause upload issues, move to CE=D27, CSN=D14.

---

## 🧪 Smoke Tests

Before running the full system, verify the radio link with a bare counter test:

- `firmware/test_tx_counter/` — Nano TX that sends a `uint32_t` counter
- `firmware/test_rx_counter/` — Nano or ESP32 RX that prints received counter

Both nodes must be on **channel 76, 1 Mbps, address `"TST01"`**. If this fails, fix wiring and power before moving on.

---

## 🩺 Troubleshooting

<details>
<summary><strong>🔴 Keypad prints heartbeat but no key events</strong></summary>

Column 3 of the keypad **must** connect to `A0`, not `D10`. Pin `D10` must be set `OUTPUT` and held `HIGH` at all times to maintain SPI master mode on the AVR.
</details>

<details>
<summary><strong>🔴 TX shows FAIL — no ACK from gateway</strong></summary>

- Ensure all nodes share a **common GND**
- Radio VCC must be **3.3 V only** — never 5 V
- Add a 10–47 µF capacitor directly across each radio's VCC and GND pins
- Double-check CE/CSN wiring and that all radios use the same channel (76) and data rate (1 Mbps)
- Try `RF24_PA_MIN` for bench testing at close range
</details>

<details>
<summary><strong>🔴 ESP32 won't enter upload mode</strong></summary>

Radios on boot-strap pins block flashing. Either unplug the radio during upload or move to CE=D27, CSN=D14. To force bootloader: hold **BOOT**, tap **EN**, release **BOOT** once upload starts.
</details>

<details>
<summary><strong>🔴 Python audio is clicky or has high latency</strong></summary>

- Try `BLOCK=512` (more stable) or `BLOCK=128` (lower latency)
- Reduce `MIX_GAIN` (e.g., `0.18`) to prevent clipping
- On Windows, prefer **ASIO** or **WASAPI** audio backends
- List available devices: `python -c "import sounddevice as sd; print(sd.query_devices())"`
</details>

<details>
<summary><strong>🔴 IMU too sensitive / not sensitive enough</strong></summary>

Tune these constants in the IMU firmware:
- `THRESH_G` — acceleration threshold to trigger a strum (try `1.4`–`2.2`)
- `REFRACT_MS` — cooldown after a strum to prevent double-plucks (try `80`–`150`)
</details>

---

## ⚙️ Build Configuration

### Arduino IDE Settings

| Setting | Value |
|---|---|
| Board (keypad/IMU) | Arduino Nano (ATmega328P, Old Bootloader) |
| Board (gateway/legacy) | ESP32 Dev Module |
| Baud rate | 115200 (all sketches) |
| Required libraries | RF24 (TMRh20), Keypad (Mark Stanley & Alexander Brevig) |

### Python Engine Defaults

| Parameter | Default |
|---|---|
| Sample rate | 22050 Hz |
| Block size | 256 |
| Latency | `'low'` |
| Mix gain | `0.20` |

---

## 📁 Repository Structure

```
Vectorstrum/
├── chord.ino               # Keypad chord selection + nRF24 TX (Nano #1)
├── strum.ino               # IMU strum detection + nRF24 TX (Nano #2)
├── soundengine.ino         # ESP32 legacy sound engine (I²S + MAX98357A)
├── gyt.py                  # Python Karplus–Strong sound engine
├── pinout.html             # Visual wiring reference
├── vectorstrum workflow.jpg # System architecture diagram
└── COLOURBOX54744368.png   # Project image asset
```

---

## 🛣️ Roadmap

- [ ] On-board OLED displaying current chord name and strum velocity
- [ ] Web UI for live threshold tuning and custom chord mapping
- [ ] MIDI output mode (play DAWs and virtual instruments)
- [ ] Battery-powered enclosure with 3D-printable case
- [ ] Left-hand / right-hand mode toggle

---

## 📜 License

Distributed under the **Apache License 2.0**. See [`LICENSE`](LICENSE) for full terms.

---

## 🙌 Credits

**Built by [Md Nazmun Nur](https://github.com/n00rtahsin)**

- [RF24 library](https://github.com/nRF24/RF24) by TMRh20 & contributors
- [Keypad library](https://playground.arduino.cc/Code/Keypad/) by Mark Stanley & Alexander Brevig
- Karplus–Strong plucked-string synthesis — *Karplus & Strong, 1983*

---

<div align="center">

*Pick up the air. Play the music.*

**⭐ Star this repo if it made you smile (or strum)**

</div>
