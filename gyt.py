import serial
import numpy as np
import sounddevice as sd
import re

# -------- CONFIG --------
PORT = "COM9"
BAUD = 115200
SR = 22050
BLOCK = 256
STRINGS = 6
MIX_GAIN = 0.22

# -------- FRETS / CHORDS --------
baseMidi = [40, 45, 50, 55, 59, 64]  # E2 A2 D3 G3 B3 E4

def noteHz(m):
    return 440.0 * (2.0**((m-69.0)/12.0))

def fretsForKey(k):
    chords = {
        '1': [-1,3,2,0,1,0],   # C
        '2': [-1,-1,0,2,3,1],  # Dm
        '3': [0,2,2,0,0,0],    # Em
        '4': [1,3,3,2,1,1],    # F
        '5': [3,2,0,0,0,3],    # G
        '6': [-1,0,2,2,1,0],   # Am
        '7': [-1,2,4,4,3,2],   # Bm
        '8': [-1,3,2,0,1,0],   # C (dup)
        '9': [-1,-1,0,2,3,2],  # D
        '0': [0,2,2,1,0,0],    # E
    }
    return chords.get(k, [-1]*6)

# -------- Karplus-Strong String --------
class KS:
    def __init__(self):
        self.buf = np.zeros(2048, dtype=np.float32)
        self.len = 0
        self.idx = 0
        self.damp = 0.996
        self.on = False
        self.color = "white"

    def setFreq(self, hz):
        if hz <= 0:
            self.len = 0
            self.on = False
            return
        L = int(SR / hz + 0.5)
        L = max(2, min(L, 2048))
        self.len = L
        self.idx = 0
        self.on = True

    def pluck(self, amp=0.8):
        if self.len < 2:
            return

        # Different timbres depending on "color"
        if self.color == "white":
            noise = np.random.uniform(-1, 1, self.len)
        elif self.color == "pink":
            noise = np.cumsum(np.random.uniform(-1,1,self.len))
            noise /= np.max(np.abs(noise))
        elif self.color == "saw":
            noise = np.linspace(-1, 1, self.len) * np.random.choice([-1,1])
        else:
            noise = np.random.uniform(-1, 1, self.len)

        self.buf[:self.len] = (noise.astype(np.float32)) * amp
        self.idx = 0
        self.on = True

    def tick(self):
        if not self.on or self.len < 2:
            return 0.0
        i1 = self.idx
        i2 = 0 if (self.idx+1)==self.len else self.idx+1
        y = 0.5 * (self.buf[i1] + self.buf[i2])
        y *= self.damp
        self.buf[i1] = y
        self.idx = i2
        return y

# -------- Setup --------
strings = [KS() for _ in range(STRINGS)]
currentChord = '1'
sustain = False
hardMute = False

# Stereo panning
pans = np.linspace(-0.7, 0.7, STRINGS)  

# Each key has unique "timbre settings"
keyProfiles = {
    '1': {"damp":0.996, "color":"white"},
    '2': {"damp":0.994, "color":"pink"},
    '3': {"damp":0.997, "color":"saw"},
    '4': {"damp":0.992, "color":"white"},
    '5': {"damp":0.995, "color":"pink"},
    '6': {"damp":0.993, "color":"white"},
    '7': {"damp":0.996, "color":"saw"},
    '8': {"damp":0.994, "color":"pink"},
    '9': {"damp":0.995, "color":"white"},
    '0': {"damp":0.992, "color":"saw"},
}

def setChord(key):
    global currentChord
    currentChord = key
    frets = fretsForKey(key)

    profile = keyProfiles.get(key, {"damp":0.996,"color":"white"})
    for s in range(STRINGS):
        if frets[s] < 0:
            strings[s].setFreq(0)
        else:
            hz = noteHz(baseMidi[s] + frets[s])
            hz *= 1.0 + np.random.uniform(-0.002,0.002)  # tiny detune
            strings[s].setFreq(hz)
            strings[s].damp = profile["damp"]
            strings[s].color = profile["color"]

    print(f"[PC] Chord={key} frets={frets} profile={profile}")

def pluckChord(vel):
    if hardMute:
        return
    amp = 0.25 + 0.75*vel
    for s in strings:
        if s.len > 0:
            s.pluck(amp)
    print(f"[PC] Pluck vel={vel:.2f}")

# -------- Serial read --------
ser = serial.Serial(PORT, BAUD, timeout=0.05)

def handle_line(line):
    global hardMute, sustain
    if "pipe=0" in line and "type=K" in line:
        m = re.search(r"b1=(\d+)", line)
        if m:
            keychr = chr(int(m.group(1)))
            if keychr == "*":
                hardMute = True
                print("[PC] MUTE ON")
            elif keychr == "#":
                sustain = not sustain
                print("[PC] sustain", sustain)
            else:
                hardMute = False
                setChord(keychr)

    elif "pipe=1" in line and "type=S" in line:
        m = re.search(r"b1=(-?\d+)", line)
        if m:
            vel = abs(int(m.group(1)))/100.0
            vel = max(0.1, min(vel,1.0))
            pluckChord(vel)

# -------- Audio callback --------
def audio_callback(outdata, frames, timeinfo, status):
    left = np.zeros(frames, dtype=np.float32)
    right = np.zeros(frames, dtype=np.float32)

    if not hardMute:
        for i in range(frames):
            sL, sR = 0.0, 0.0
            for idx, ks in enumerate(strings):
                val = ks.tick()
                pan = pans[idx]
                sL += val * (1-pan)
                sR += val * (1+pan)
            left[i] = sL * MIX_GAIN
            right[i] = sR * MIX_GAIN

    out = np.stack([left, right], axis=1)
    outdata[:] = out

# -------- Run --------
print("[PC] AirGuitar ready. Listening on COM9...")
stream = sd.OutputStream(channels=2, callback=audio_callback,
                         samplerate=SR, blocksize=BLOCK)
with stream:
    while True:
        try:
            line = ser.readline().decode(errors="ignore").strip()
            if line:
                handle_line(line)
        except KeyboardInterrupt:
            break