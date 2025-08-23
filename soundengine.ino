// ESP32 SOUND ENGINE — RX two pipes + 2s strum gate + KS synth -> MAX98357A (I2S)
#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include <driver/i2s.h>
#include <math.h>
#include <string.h>

// ====== nRF24 CE/CSN pins ======
// STRAP (works with your existing wiring, may need BOOT/EN trick while flashing)
#define RF_CE   4    // D4  (strap pin)
#define RF_CSN  5    // D5  (strap pin)
// SAFE (uncomment if you rewire; avoids boot issues)
// #define RF_CE   27   // D27
// #define RF_CSN  14   // D14

// ====== I2S/MAX98357A pins ======
#define I2S_BCLK 26   // D26
#define I2S_LRCK 25   // D25
#define I2S_DATA 22   // D22
#define AMP_SD   21   // D21

RF24 radio(RF_CE, RF_CSN);
const byte ADDR_KEY[6]="KEY01";
const byte ADDR_IMU[6]="IMU01";
const uint8_t RF_LEN=3;

const int SR=22050, BLOCK_N=256, STRINGS=6, MAX_DELAY=1024;
const float MIX_GAIN=0.22f;
const uint32_t STRUM_TIMEOUT_MS=2000;

const int baseMidi[STRINGS]={40,45,50,55,59,64}; // E2 A2 D3 G3 B3 E4
static inline float noteHz(float m){ return 440.0f*powf(2.0f,(m-69.0f)/12.0f); }

void fretsForKey(char k,int out[STRINGS]){
  switch(k){
    case '1':{int t[6]={-1,3,2,0,1,0}; memcpy(out,t,sizeof t);}break; // C
    case '2':{int t[6]={-1,-1,0,2,3,1}; memcpy(out,t,sizeof t);}break;// Dm
    case '3':{int t[6]={0,2,2,0,0,0}; memcpy(out,t,sizeof t);}break;  // Em
    case '4':{int t[6]={1,3,3,2,1,1}; memcpy(out,t,sizeof t);}break;  // F
    case '5':{int t[6]={3,2,0,0,0,3}; memcpy(out,t,sizeof t);}break;  // G
    case '6':{int t[6]={-1,0,2,2,1,0}; memcpy(out,t,sizeof t);}break; // Am
    case '7':{int t[6]={-1,2,4,4,3,2}; memcpy(out,t,sizeof t);}break; // Bm
    case '8':{int t[6]={-1,3,2,0,1,0}; memcpy(out,t,sizeof t);}break; // C dup
    case '9':{int t[6]={-1,-1,0,2,3,2}; memcpy(out,t,sizeof t);}break;// D
    case '0':{int t[6]={0,2,2,1,0,0}; memcpy(out,t,sizeof t);}break;  // E
    default:{int t[6]={-1,-1,-1,-1,-1,-1}; memcpy(out,t,sizeof t);}
  }
}

// -------- Karplus–Strong --------
struct KS{ float buf[MAX_DELAY]; int len=0,idx=0; float damp=0.996f; bool on=false;
  void setFreq(float hz){ if(hz<=0){on=false;len=0;return;} int L=(int)((float)SR/hz+0.5f); if(L<2)L=2; if(L>MAX_DELAY)L=MAX_DELAY; len=L; if(idx>=len)idx=0; }
  void pluck(float amp,uint32_t seed){ if(len<2){on=false;return;} uint32_t r=seed?seed:1; for(int i=0;i<len;i++){ r^=r<<13; r^=r>>17; r^=r<<5; float n=((int32_t)(r&0xFFFF)-32768)/32768.0f; buf[i]=n*amp; } idx=0; on=true; }
  float tick(){ if(!on||len<2)return 0.0f; int i1=idx,i2=(idx+1==len)?0:idx+1; float y=0.5f*(buf[i1]+buf[i2]); y*=damp; buf[i1]=y; idx=i2; return y; }
} V[STRINGS];

char currentKey='1'; bool sustain=false, hardMute=false;
uint8_t lastSeqK=255,lastSeqS=255; uint32_t lastChordMs=0,lastBeat=0;

void setChord(char key){
  currentKey=key; int f[STRINGS]; fretsForKey(key,f);
  Serial.print("[ESP] setChord="); Serial.println(key);
  for(int s=0;s<STRINGS;s++){ if(f[s]<0){ V[s].setFreq(0.0f); } else { V[s].setFreq(noteHz(baseMidi[s]+f[s])); } }
}
void pluckChord(float v){
  if(hardMute) return; float amp=0.7f*v+0.25f; float d = sustain?0.9995f:(0.993f+0.004f*v);
  uint32_t seed=millis()*2654435761UL;
  Serial.print("[ESP] pluck v="); Serial.println(v,2);
  for(int s=0;s<STRINGS;s++){ if(V[s].len<2) continue; V[s].damp=d; V[s].pluck(amp,seed+s*97); }
}

void i2sInit(){
  i2s_config_t cfg={ (i2s_mode_t)(I2S_MODE_MASTER|I2S_MODE_TX), SR, I2S_BITS_PER_SAMPLE_16BIT,
                     I2S_CHANNEL_FMT_ONLY_LEFT, I2S_COMM_FORMAT_STAND_MSB, 0, 8, BLOCK_N, false, true, 0 };
  i2s_pin_config_t pins={ I2S_BCLK, I2S_LRCK, I2S_DATA, I2S_PIN_NO_CHANGE };
  i2s_driver_install(I2S_NUM_0,&cfg,0,NULL);
  i2s_set_pin(I2S_NUM_0,&pins);
  i2s_zero_dma_buffer(I2S_NUM_0);
  Serial.println("[ESP] I2S ready");
}

void rfInit(){
  SPI.begin(18,19,23); // VSPI SCK,MISO,MOSI
  if(!radio.begin()){ Serial.println("[ESP] RF24 begin FAILED"); delay(2000); }
  radio.setAddressWidth(5);
  radio.setChannel(76);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_LOW);
  radio.setAutoAck(true);
  radio.setCRCLength(RF24_CRC_16);
  radio.setRetries(5,15);
  radio.disableDynamicPayloads();
  radio.setPayloadSize(RF_LEN);
  radio.openReadingPipe(0, ADDR_KEY);
  radio.openReadingPipe(1, ADDR_IMU);
  radio.startListening();
  radio.flush_rx(); radio.flush_tx();
  Serial.println("[ESP] RF ready: pipe0=KEY01 pipe1=IMU01 ch76 1Mbps");
}

void renderBlock(){
  static int16_t out[BLOCK_N];
  for(int i=0;i<BLOCK_N;i++){
    float s=0; if(!hardMute){ for(int k=0;k<STRINGS;k++) s+=V[k].tick(); }
    s*=MIX_GAIN; if(s>1)s=1; else if(s<-1)s=-1; out[i]=(int16_t)(s*14000.0f);
  }
  size_t wr; i2s_write(I2S_NUM_0,out,sizeof(out),&wr,portMAX_DELAY);
}

void setup(){
  Serial.begin(115200);
  pinMode(AMP_SD,OUTPUT); digitalWrite(AMP_SD,LOW);
  i2sInit(); rfInit();
  setChord('1'); lastChordMs=millis();
  delay(20); digitalWrite(AMP_SD,HIGH);
  Serial.println("[ESP] Sound engine ready");
}

void loop(){
  unsigned long now=millis();
  if(now-lastBeat>=2000){ lastBeat=now; Serial.println("[ESP] heartbeat"); }

  uint8_t pipe;
  while(radio.available(&pipe)){
    uint8_t p[RF_LEN]; radio.read(p,RF_LEN);
    Serial.print("[ESP][RX] pipe="); Serial.print(pipe);
    Serial.print(" type="); Serial.write((char)p[0]);
    Serial.print(" b1="); Serial.print((int)p[1]);
    Serial.print(" seq="); Serial.println((int)p[2]);

    if(pipe==0 && p[0]=='K'){
      if(p[2]==lastSeqK){ Serial.println("[ESP] dup K"); continue; }
      lastSeqK=p[2];
      char key=(char)p[1];
      if(key=='*'){ hardMute=true; digitalWrite(AMP_SD,LOW); Serial.println("[ESP] MUTE ON"); }
      else if(key=='#'){ sustain=!sustain; if(!hardMute) digitalWrite(AMP_SD,HIGH); Serial.print("[ESP] sustain="); Serial.println(sustain?"ON":"OFF"); }
      else { hardMute=false; digitalWrite(AMP_SD,HIGH); setChord(key); lastChordMs=now; }
      continue;
    }

    if(pipe==1 && p[0]=='S'){
      if(p[2]==lastSeqS){ Serial.println("[ESP] dup S"); continue; }
      lastSeqS=p[2];
      if(now - lastChordMs <= STRUM_TIMEOUT_MS){
        int8_t vel=(int8_t)p[1]; float v=fabsf(vel)/100.0f; if(v<0.1f)v=0.1f;
        pluckChord(v);
      } else {
        Serial.println("[ESP] strum ignored (no recent chord)");
      }
      continue;
    }
  }
  renderBlock();
}