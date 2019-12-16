/*
   RadioLib SX127x Transmit Example

   This example transmits packets using SX1278 LoRa radio module.
   Each packet contains up to 256 bytes of downlink, in the form of:
    - Arduino String
    - null-terminated char array (C-string)
    - arbitrary binary downlink (byte array)

   Other modules from SX127x/RFM9x family can also be used.

   For full API reference, see the GitHub Pages
   https://jgromes.github.io/RadioLib/
*/

// include the library
#include <RadioLib.h>

// SX1278 has the following connections:
// NSS pin:   10
// DIO0 pin:  2
// DIO1 pin:  6
SX1278 lora = new Module(10, 2, 3); // arduino values
//SX1278 lora = new Module(5, 26, 12); // lolin32dev values

uint8_t downlink[64];
uint8_t uplink[64];
uint8_t downlinklen = 0;
uint16_t pwrcnt = 0; // value for now
uint16_t resetcnt = 47; // value for now
int16_t signedinteger;
uint8_t crc = 0; //calculate checksum for message.

int state;
uint8_t satCmd;
long int theTime;

void setup() {
  Serial.begin(115200);

  theTime = millis();
  
  pinMode(LED_BUILTIN, OUTPUT);

  // initialize SX1278 with default settings
  Serial.println();
  Serial.println("Fossasat-1Transmit");
  
  Serial.print(F("[SX1278] Initializing ... "));
  float CARRIER_FREQUENCY = 436.7f;
  float DEFAULT_CARRIER_FREQUENCY = 436.7f;
  float BANDWIDTH = 125.0f;
  float CONNECTED_BANDWIDTH = 7.8f;
  int   SPREADING_FACTOR = 11;  // defined by fossasat-1
  int   CODING_RATE = 8; // can be 4 or 8.
  char  SYNC_WORD = 0xff;  //0f0f for SX1262
  int   OUTPUT_POWER = 17; // dBm
  // current limit:               100 mA
  // preamble length:             8 symbols
  // amplifier gain:              0 (automatic gain control)

  state = lora.begin(DEFAULT_CARRIER_FREQUENCY, BANDWIDTH, SPREADING_FACTOR, CODING_RATE, SYNC_WORD, OUTPUT_POWER, 100, 8, 0);
  if (state == ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true);
  }
  for (uint8_t i=0; i<63; i++) { 
    downlink[i] = 0x00;
    uplink[i] = 0x00;
  }
}

void loop() {
  if ( pwrcnt == 0 ){
    satCmd=3; // Send data
  }
  else{
    satCmd=5; // no command
  }

  state = lora.receive((uint8_t *)uplink, 63);
  if (state == ERR_NONE) {
    Serial.println("receive uplink from ground");
    
//    <callsign><function ID>(optional data length)(optional data)
    satCmd = uplink[10];
    Serial.print("command=0x0");
    Serial.println(satCmd,HEX);
  }

  if ( satCmd < 5 ) {

    for (uint8_t i = 0; i<64; i++) {
      crc ^= uplink[i];  // crc XOR uplink_element
      if ( uplink[i] > 0x00 ) {
        Serial.print(uplink[i], HEX);
        Serial.print(" ");
      }
    }
    Serial.println();
    for (uint8_t i = 0; i<64; i++) {
      Serial.print(char(uplink[i]));
    }
    Serial.println();
    Serial.print("uplink crc=");
    Serial.println(crc,HEX);

    // reset downlink array
    for (uint8_t i = 0; i<64; i++) { downlink[i] = '\0'; }

    Serial.print(F("[SX1278] Transmitting packet ... type=0x"));
    // Set satellite call sign
    downlink[0]='F';
    downlink[1]='O';
    downlink[2]='S';
    downlink[3]='S';
    downlink[4]='A';
    downlink[5]='S';
    downlink[6]='A';
    downlink[7]='T';
    downlink[8]='-';
    downlink[9]='1';
  
    if ( satCmd == 0x00 ) { // pong received
      downlink[10]= 0x10;
      Serial.println(downlink[10], HEX);
      downlink[11]= '\0';
      downlinklen=12;
    }
    
    if ( satCmd == 0x01 ) { // cmd_retransmit
      downlink[10] = 0x11;
      Serial.println(downlink[10], HEX);
      
      uint8_t datalen = uplink[11];
      uint8_t i;
      for (i = 11; i < datalen + 11; i++ ) {
        downlink[i] = uplink[i];
      }
      downlinklen = datalen + 11;
    }
      
    if ( satCmd == 0x02 ) {
      downlink[10]= 0x12;
      Serial.println(downlink[10], HEX);
      //downlink[11]= 0x15; // bandwidth value (0x00 for 7.8 kHz, 0x07 for 125 kHz)
      //downlink[12]= 0x07; // spreading factor value (0x00 for SF5, 0x07 for SF12)
      //downlink[13]= 0x0C; // coding rate (0x05 to 0x08, inclusive)
      //downlink[14]= 0x06; // preamble length in symbols (0x0000 to 0xFFFF; LSB first)
      //downlink[15]= 0x10;
      //downlink[16]= 0x01; // CRC enabled (0x01) or disabled (0x00)
      //downlink[17]= 0x0F;   // output power in dBm (signed 8-bit integer; -17 to 22)

      uint8_t datalen = downlink[18] - 7;
      downlink[11] = datalen;
      uint8_t i;
      for (i = 0; i < datalen; i++ ) {
        downlink[i+11] = uplink[i+19];
      }
 
      downlinklen = datalen + 12;
    }
    
    if ( satCmd == 0x03 ) {
        //  { 0x06, 0x4F, 0x53, 0x53, 0x41, 0x53, 0x41, 0x54, 0x2D, 0x31, 0x13, 0x28, 0x78, 0x1E, 0x21, 0x1C, 0xD2, 0x04, 0x3C, 0x04, 0x7D, 0x2F, 0x00 };
        // real data 13 0F C4 F1 03 C4 00 00 00 90 01 56 FA 4C 09 00 16   13/12/2019
        //           13 0F C4 3F C4 35 6B XX XX XX 70 9E 07 4F 09 00 16   15/12/2019
        
      // Battery voltage = 3920mV
      // Battery charging current = 10.09mA
      // Solar cell voltage A, B and C = 0mV
      
      downlink[10]= 0x13;
      Serial.println(downlink[10], HEX);
      downlink[11]= 0x0F;  // optionalDataLen
      
      downlink[12]= 0xC4;  // battery charging voltage  * 20 mV
    
      signedinteger = 120; // battery charging current * 10uA 
      downlink[13]= 0xF1; //signedinteger & 0xFF;
      downlink[14]= 0x03; //(signedinteger / 0x100) & 0xFF
    
      downlink[15]= 0xC4; // Battery voltage
      
      downlink[16]= 0x00;  // solar cell A voltage  * 20 mV
      downlink[17]= 0x00;  // solar cell B voltage  * 20 mV
      downlink[18]= 0x00;  // solar cell C voltage  * 20 mV
      
      signedinteger = 1234; // battery temp * 0.01 C
      downlink[19]= 0x90; //signedinteger & 0xFF; 
      downlink[20]= 0x01; //(signedinteger / 0x100) & 0xFF;
    
      signedinteger = 1084; // board temp * 0.01 C
      downlink[21]= 0x56; //signedinteger & 0xFF;
      downlink[22]= 0xFA; //(signedinteger / 0x100) & 0xFF;
      
      downlink[23]= 0x4C;  // mcu temp *0.1 C
    
      signedinteger = resetcnt++;   // reset counter
      downlink[24]= 0x09; //signedinteger & 0xFF;
      downlink[25]= 0x00; //(signedinteger / 0x100) & 0xFF;
    
      downlink[26]= 0x16; //pwr
      downlink[27]= '\0'; 
      downlinklen=28;
    }
    
    if ( satCmd == 0x04 ) {
      downlink[10]= 0x14;
      Serial.println(downlink[10], HEX);
      downlink[11]= (uint8_t)(lora.getSNR() / 4);   // optionaldownlink[0] = SNR * 4 dB
      downlink[12]= (uint8_t)(lora.getRSSI() / -2); // optionaldownlink[1] = RSSI * -2 dBm
      downlink[13]= '\0';
      downlinklen=14;
    }
    digitalWrite(LED_BUILTIN, HIGH);
    state = lora.transmit(downlink, downlinklen);
    digitalWrite(LED_BUILTIN, LOW);
    // checksum of downlink array
    crc = 0;
    for (uint8_t i = 0; i<64; i++) {
      crc ^= downlink[i];  // crc XOR downlink_element
      Serial.print(downlink[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    for (uint8_t i = 0; i<64; i++) {
      Serial.print(char(downlink[i]));
    }
    Serial.println();
    Serial.print("downlink crc=");
    Serial.println(crc,HEX);
   
    if (state == ERR_NONE) {
      // the packet was successfully transmitted
      //Serial.println(F(" success!"));
  
      // print measured data rate
      Serial.print(F("[SX1278] Datarate:\t"));
      Serial.print(lora.getDataRate());
      Serial.println(F(" bps"));
  
    } else if (state == ERR_PACKET_TOO_LONG) {
      // the supplied packet was longer than 256 bytes
      Serial.println(F(" too long!"));
  
    } else if (state == ERR_TX_TIMEOUT) {
      // timeout occured while transmitting packet
      Serial.println(F(" timeout!"));
  
    } else if (state == ERR_SPI_WRITE_FAILED) {
      // Real value in SPI register does not match the expected one.
      Serial.println(F(" SPI write failed!"));
  
    } else {
      // some other error occurred
      Serial.print(F("failed, code "));
      Serial.println(state);
    }
  }
  
  pwrcnt++; // pwr for fun
  if (pwrcnt > 20) {
    Serial.print(theTime);
    Serial.print(" ");
    Serial.print(millis() - theTime);
    Serial.println();
    pwrcnt=0;
  }
    
}
