/* Nano + nRF24 (Keypad TX) — pipe "KEY01"
   Keypad: rows D3,D4,D5,D6 ; cols D2,D9,A0  (do NOT use D10)
   nRF24 : CE D8, CSN D7, SCK 13, MOSI 11, MISO 12, VCC 3.3V, GND
   Packet: {'K', key, seq}
*/
#include <Keypad.h>
#include <SPI.h>
#include <RF24.h>

#define CE_PIN 8
#define CSN_PIN 7
RF24 radio(CE_PIN, CSN_PIN);
const byte ADDR_KEY[6] = "KEY01";

const byte ROWS=4, COLS=3;
char keys[ROWS][COLS]={{'1','2','3'},{'4','5','6'},{'7','8','9'},{'*','0','#'}};
byte rowPins[ROWS]={3,4,5,6};
byte colPins[COLS]={2,9,A0}; // A0 instead of D10

Keypad kpd(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

uint8_t seq=0, failStreak=0;
char lastKey=0;
unsigned long lastSend=0, lastBeat=0;

void rfInit(){
  if(!radio.begin()){ Serial.println("[KEYPAD] RF24 begin FAILED"); while(1){} }
  radio.setAddressWidth(5);
  radio.setChannel(76);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_LOW);
  radio.setAutoAck(true);
  radio.setCRCLength(RF24_CRC_16);
  radio.setRetries(5,15);
  radio.disableDynamicPayloads();
  radio.setPayloadSize(3);
  radio.openWritingPipe(ADDR_KEY);
  radio.stopListening();
}

void setup(){
  Serial.begin(115200);
  pinMode(10, OUTPUT); digitalWrite(10, HIGH);
  rfInit();
  Serial.println("[KEYPAD] ready KEY01 ch76 1Mbps");
}

void sendKeyOnce(char k){
  uint8_t p[3] = { 'K', (uint8_t)k, seq++ };
  bool ok = radio.write(p, sizeof(p));
  if(!ok) failStreak++; else failStreak=0;
  Serial.print("[KEYPAD] key="); Serial.print(k); Serial.print(" seq="); Serial.print(seq-1);
  Serial.println(ok ? " ACK" : " FAIL");
  if(failStreak>=5){ Serial.println("[KEYPAD] reinit"); rfInit(); failStreak=0; }
}
void sendKeyBurst(char k){ for(int i=0;i<3;i++){ sendKeyOnce(k); delay(15); } }

void loop(){
  char k = kpd.getKey();
  unsigned long now = millis();
  if(k && (k!=lastKey || now-lastSend>=500)){
    sendKeyBurst(k);
    lastKey=k; lastSend=now;
  }
  if(!k) lastKey=0;
  if(now-lastBeat>=2000){ lastBeat=now; Serial.println("[KEYPAD] heartbeat"); }
}
