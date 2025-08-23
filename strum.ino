/* Nano + MPU-6050 (GY-521) Strum TX — pipe "IMU01"
   GY-521: VCC 5V, GND, SDA A4, SCL A5  (XDA/XCL unconnected)
   nRF24 : CE D8, CSN D7, SCK 13, MOSI 11, MISO 12, VCC 3.3V, GND
   Packet: {'S', velocity(int8), seq}
*/
#include <Wire.h>
#include <SPI.h>
#include <RF24.h>
#include <math.h>

#define CE_PIN 8
#define CSN_PIN 7
RF24 radio(CE_PIN, CSN_PIN);
const byte ADDR_IMU[6] = "IMU01";
const uint8_t PAYLEN=3;

const uint8_t MPU=0x68;
const float G=16384.0f;
const float THRESH_G=1.8f;       // 1.4..2.2
const uint16_t REFRACT_MS=120;   // ms
const float HP_ALPHA=0.02f;      // high-pass

const uint16_t SEND_BASE_DELAY_MS=40;
const uint8_t  SEND_JITTER_MS_MAX=15;

unsigned long lastHit=0, lastBeat=0;
float ax_dc=0.0f;
uint8_t seq=0, failStreak=0;

bool pending=false;
int8_t pendingVel=0;
unsigned long sendDueMs=0;

void rfInit(){
  if(!radio.begin()){ Serial.println("[IMU] RF24 begin FAILED"); while(1){} }
  radio.setAddressWidth(5);
  radio.setChannel(76);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_LOW);
  radio.setAutoAck(true);
  radio.setCRCLength(RF24_CRC_16);
  radio.setRetries(5,15);
  radio.disableDynamicPayloads();
  radio.setPayloadSize(PAYLEN);
  radio.openWritingPipe(ADDR_IMU);
  radio.stopListening();
}

void setup(){
  Serial.begin(115200);
  Wire.begin();
  pinMode(10, OUTPUT); digitalWrite(10, HIGH);

  // MPU init
  Wire.beginTransmission(MPU); Wire.write(0x6B); Wire.write(0x00); Wire.endTransmission();
  Wire.beginTransmission(MPU); Wire.write(0x1B); Wire.write(0x00); Wire.endTransmission(); // gyro ±250
  Wire.beginTransmission(MPU); Wire.write(0x1C); Wire.write(0x00); Wire.endTransmission(); // accel ±2g
  Wire.beginTransmission(MPU); Wire.write(0x1A); Wire.write(0x03); Wire.endTransmission(); // DLPF ~44Hz
  delay(50);
  long sum=0; for(int i=0;i<200;i++){ Wire.beginTransmission(MPU); Wire.write(0x3B); Wire.endTransmission(false);
    Wire.requestFrom(MPU,6,true); int16_t ax=(Wire.read()<<8)|Wire.read(); sum+=ax; delay(5); }
  ax_dc = (float)sum/(200*G);
  Serial.print("[IMU] ax_dc g="); Serial.println(ax_dc,4);

  rfInit();
  Serial.println("[IMU] ready IMU01 ch76 1Mbps");
}

void queueStrum(int8_t v){
  uint8_t j = random(SEND_JITTER_MS_MAX+1);
  pendingVel=v; sendDueMs=millis()+SEND_BASE_DELAY_MS+j; pending=true;
  Serial.print("[IMU] queued vel="); Serial.print((int)v); Serial.print(" +"); Serial.print(SEND_BASE_DELAY_MS+j); Serial.println("ms");
}

void sendStrumNow(int8_t v){
  uint8_t p[3]={'S',(uint8_t)v,seq++};
  bool ok=radio.write(p,sizeof(p));
  if(!ok) failStreak++; else failStreak=0;
  Serial.print("[IMU] SEND vel="); Serial.print((int)v); Serial.print(" seq="); Serial.print(seq-1);
  Serial.println(ok ? " ACK" : " FAIL");
  if(failStreak>=5){ Serial.println("[IMU] reinit"); rfInit(); failStreak=0; }
}

void loop(){
  unsigned long now=millis();

  if(pending && (long)(now - sendDueMs)>=0){ sendStrumNow(pendingVel); pending=false; }

  static unsigned long t0=0;
  if(now - t0 < 5){ if(now-lastBeat>=2000){ lastBeat=now; Serial.println("[IMU] heartbeat"); } return; }
  t0=now;

  // read accel X
  Wire.beginTransmission(MPU); Wire.write(0x3B); Wire.endTransmission(false);
  Wire.requestFrom(MPU,6,true); int16_t ax=(Wire.read()<<8)|Wire.read(); Wire.read(); Wire.read(); Wire.read(); Wire.read();
  float axg=(float)ax/G;

  ax_dc=(1.0f-HP_ALPHA)*ax_dc + HP_ALPHA*axg;
  float ax_hp=axg-ax_dc;

  if(now-lastHit>REFRACT_MS && fabs(ax_hp)>THRESH_G){
    lastHit=now;
    int v=(int)(ax_hp*60.0f); if(v>100)v=100; if(v<-100)v=-100;
    queueStrum((int8_t)v);
  }
}