

#include <Arduino.h>

#include "LoRa_lowlevel.h"
#include "LoRa_SX127x.h"
#include "LoRa_SX1276.h"
#include "LoRa_SX1278.h"

#include "FreeRTOS.h"
#include "esp32-hal-timer.h"

#define DEBUG

static SemaphoreHandle_t timer_sem;

hw_timer_t * timer = NULL;

//hw_timer_t SX127xDriver::timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

extern uint32_t TXdoneMicros;

TaskHandle_t TimerTaskTX_handle = NULL;

uint32_t volatile SX127xDriver::PacketCount = 0;

void (*SX127xDriver::RXdoneCallback)() = NULL;
void (*SX127xDriver::TXdoneCallback)() = NULL;

void (*SX127xDriver::TXtimeout)() = NULL;
void (*SX127xDriver::RXtimeout)() = NULL;

/////setup some default variables//////////////

volatile bool SX127xDriver::headerExplMode = false;

volatile uint32_t SX127xDriver::TXContInterval = 20000;
volatile uint8_t SX127xDriver::TXbuffLen = 8;
volatile uint8_t SX127xDriver::RXbuffLen = 8;

//////////////////Hardware Pin Variable defaults////////////////

uint8_t SX127xDriver::_RXenablePin = 13;
uint8_t SX127xDriver::_TXenablePin = 12;

uint8_t SX127xDriver::SX127x_nss = 5;
uint8_t SX127xDriver::SX127x_dio0 = 26;
uint8_t SX127xDriver::SX127x_dio1 = 25;

uint8_t SX127xDriver::SX127x_MOSI = 23;
uint8_t SX127xDriver::SX127x_MISO = 19;
uint8_t SX127xDriver::SX127x_SCK = 18;
uint8_t SX127xDriver::SX127x_RST = 18;

/////////////////////////////////////////////////////////////////

uint32_t SX127xDriver::TimeOnAir = 0;
uint32_t SX127xDriver::LastTXdoneMicros = 0;
uint32_t SX127xDriver::TXstartMicros = 0;
int8_t SX127xDriver::LastPacketRSSI = 0;
float SX127xDriver::LastPacketSNR = 0;
uint32_t SX127xDriver::TXspiTime  = 0;

Bandwidth SX127xDriver::_bw = BW_500_00_KHZ;
SpreadingFactor SX127xDriver::_sf = SF_6;
CodingRate SX127xDriver::_cr = CR_4_5;
uint8_t SX127xDriver::_syncWord = SX127X_SYNC_WORD;

float SX127xDriver::_freq = 915000000;

uint8_t volatile SX127xDriver::TXdataBuffer[256];
uint8_t volatile SX127xDriver::RXdataBuffer[256];

ContinousMode SX127xDriver::ContMode = CONT_OFF;
RFmodule_ SX127xDriver::RFmodule = RFMOD_SX1276;
RadioState_ SX127xDriver::RadioState = RADIO_IDLE;

//////////////////////////////////////////////

uint8_t SX127xDriver::Begin(){
  uint8_t status;

  pinMode(SX127x_RST, OUTPUT);
  digitalWrite(SX127x_RST, 0);
  delay(100);
  digitalWrite(SX127x_RST, 1);
  delay(100);


  static uint8_t SX127x_SCK;
  
  if (RFmodule == RFMOD_SX1278) {
    status = SX1278begin(SX127x_nss, SX127x_dio0, SX127x_dio1);
  } else {
    status = SX1276begin(SX127x_nss, SX127x_dio0, SX127x_dio1);
  }
  
  return(status);
  
}


uint8_t SX127xDriver::setBandwidth(Bandwidth bw) {
  uint8_t state = SX127xConfig(bw, _sf, _cr, _freq, _syncWord);
  if (state == ERR_NONE) {
    _bw = bw;
  }
  return (state);
}

uint8_t SX127xDriver::setSyncWord(uint8_t syncWord) {

  uint8_t status = setRegValue(SX127X_REG_SYNC_WORD, syncWord);
  if (status != ERR_NONE) {
    return (status);
  } else { 
    return(ERR_NONE);
  }
}

uint8_t SX127xDriver::SetOutputPower(uint8_t Power) {
  //todo make function turn on PA_BOOST ect
  uint8_t status = setRegValue(SX127X_REG_PA_CONFIG, 0b00000000, 3, 0);

  if (status != ERR_NONE) {
    return (status);
  } else { 
    return(ERR_NONE);
  }
}

uint8_t SX127xDriver::SetPreambleLength(uint8_t PreambleLen) {
  //status = setRegValue(SX127X_REG_PREAMBLE_MSB, SX127X_PREAMBLE_LENGTH_MSB);
  uint8_t status = setRegValue(SX127X_REG_PREAMBLE_LSB, PreambleLen);
  if (status != ERR_NONE) {
    return (status);
  } else { 
    return(ERR_NONE);
  }
}


uint8_t SX127xDriver::SetSpreadingFactor(SpreadingFactor sf) {
  uint8_t status;
  if (sf == SX127X_SF_6) {
    status = setRegValue(SX127X_REG_MODEM_CONFIG_2, SX127X_SF_6 | SX127X_TX_MODE_SINGLE, 7, 3);
    status = setRegValue(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_6, 2, 0);
    status = setRegValue(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_6);
  } else {
    status = setRegValue(SX127X_REG_MODEM_CONFIG_2, sf | SX127X_TX_MODE_SINGLE, 7, 3);
    status = setRegValue(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_7_12, 2, 0);
    status = setRegValue(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_7_12);
  }
  if (status == ERR_NONE) {
    _sf = sf;
  }
  return (status);
}

uint8_t SX127xDriver::SetCodingRate(CodingRate cr) {
  uint8_t state = SX127xConfig(_bw, _sf, cr, _freq, _syncWord);
  if (state == ERR_NONE) {
    _cr = cr;
  }
  return (state);
}

uint8_t SX127xDriver::SetFrequency(float freq) {

  uint8_t status = ERR_NONE;

  status = SetMode(SX127X_SLEEP);
  if (status != ERR_NONE) {
    return (status);
  }

#define FREQ_STEP                                   61.03515625

  uint32_t FRQ = ( uint32_t )( ( double )freq / ( double )FREQ_STEP );
  status = setRegValue( SX127X_REG_FRF_MSB, ( uint8_t )( ( FRQ >> 16 ) & 0xFF ) );
  status = setRegValue( SX127X_REG_FRF_MID, ( uint8_t )( ( FRQ >> 8 ) & 0xFF ) );
  status = setRegValue( SX127X_REG_FRF_LSB, ( uint8_t )( FRQ & 0xFF ) );

  // set carrier frequency  CHANGED
  // uint32_t base = 2;
  // uint32_t FRF = (freq * (base << 18)) / 32.0;
  // status = setRegValue(SX127X_REG_FRF_MSB, (FRF & 0xFF0000) >> 16);
  // status = setRegValue(SX127X_REG_FRF_MID, (FRF & 0x00FF00) >> 8);
  // status = setRegValue(SX127X_REG_FRF_LSB, FRF & 0x0000FF);

  if (status != ERR_NONE) {
    return (status);
  }

  status = SetMode(SX127X_STANDBY);
  return (status);

}



//uint8_t SX127xsetPower() {
//
//  //SX1276Read( REG_LR_PACONFIG, &SX1276LR->RegPaConfig );
//  //SX1276Read( REG_LR_PADAC, &SX1276LR->RegPaDac );
//
//  //read SX127X_REG_PA_CONFIG
//  //read SX1278_REG_PA_DAC
//
//  if ( ( SX1276LR->RegPaConfig & RFLR_PACONFIG_PASELECT_PABOOST ) == RFLR_PACONFIG_PASELECT_PABOOST )
//  {
//    if ( ( SX1276LR->RegPaDac & 0x87 ) == 0x87 )
//    {
//      if ( power < 5 )
//      {
//        power = 5;
//      }
//      if ( power > 20 )
//      {
//        power = 20;
//      }
//      SX1276LR->RegPaConfig = ( SX1276LR->RegPaConfig & RFLR_PACONFIG_MAX_POWER_MASK ) | 0x70;
//      SX1276LR->RegPaConfig = ( SX1276LR->RegPaConfig & RFLR_PACONFIG_OUTPUTPOWER_MASK ) | ( uint8_t )( ( uint16_t )( power - 5 ) & 0x0F );
//    }
//    else
//    {
//      if ( power < 2 )
//      {
//        power = 2;
//      }
//      if ( power > 17 )
//      {
//        power = 17;
//      }
//      SX1276LR->RegPaConfig = ( SX1276LR->RegPaConfig & RFLR_PACONFIG_MAX_POWER_MASK ) | 0x70;
//      SX1276LR->RegPaConfig = ( SX1276LR->RegPaConfig & RFLR_PACONFIG_OUTPUTPOWER_MASK ) | ( uint8_t )( ( uint16_t )( power - 2 ) & 0x0F );
//    }
//  }
//  else
//  {
//    if ( power < -1 )
//    {
//      power = -1;
//    }
//    if ( power > 14 )
//    {
//      power = 14;
//    }
//    SX1276LR->RegPaConfig = ( SX1276LR->RegPaConfig & RFLR_PACONFIG_MAX_POWER_MASK ) | 0x70;
//    SX1276LR->RegPaConfig = ( SX1276LR->RegPaConfig & RFLR_PACONFIG_OUTPUTPOWER_MASK ) | ( uint8_t )( ( uint16_t )( power + 1 ) & 0x0F );
//  }
//  SX1276Write( REG_LR_PACONFIG, SX1276LR->RegPaConfig );
//  LoRaSettings.Power = power;
//}
//
//}


uint8_t SX127xDriver::SX127xBegin() {
  uint8_t i = 0;
  bool flagFound = false;
  while ((i < 10) && !flagFound) {
    uint8_t version = readRegister(SX127X_REG_VERSION);
    if (version == 0x12) {
      flagFound = true;
    } else {
#ifdef DEBUG
      //Serial.print(SX127xgetChipName());
      Serial.print(" not found! (");
      Serial.print(i + 1);
      Serial.print(" of 10 tries) REG_VERSION == ");

      char buffHex[5];
      sprintf(buffHex, "0x%02X", version);
      Serial.print(buffHex);
      Serial.println();
#endif
      delay(1000);
      i++;
    }
  }

  if (!flagFound) {
#ifdef DEBUG
    //Serial.print(SX127xgetChipName());
    Serial.println(" not found!");
#endif
    //SPI.end();
    return (ERR_CHIP_NOT_FOUND);
  }
#ifdef DEBUG
  else {
    //Serial.print(SX127xgetChipName());
    Serial.println(" found! (match by REG_VERSION == 0x12)");
  }
#endif
  return (ERR_NONE);
}

uint8_t SX127xDriver::TX(uint8_t* data, uint8_t length) {
  SetMode(SX127X_STANDBY);

  setRegValue(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_TX_DONE, 7, 6);

  ClearIRQFlags();

  if (length >= 256) {
    return (ERR_PACKET_TOO_LONG);
  }

  setRegValue(SX127X_REG_PAYLOAD_LENGTH, length);
  setRegValue(SX127X_REG_FIFO_TX_BASE_ADDR, SX127X_FIFO_TX_BASE_ADDR_MAX);
  setRegValue(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_TX_BASE_ADDR_MAX);

  writeRegisterBurstStr((uint8_t)SX127X_REG_FIFO, data, (uint8_t)length);

#if defined(ARDUINO_ESP32_DEV)
  digitalWrite(_RXenablePin, LOW);
  digitalWrite(_TXenablePin, HIGH); //the larger TX/RX modules require that the TX/RX enable pins are toggled
#endif

  SetMode(SX127X_TX);

  unsigned long start = millis();
  while (!digitalRead(SX127x_dio0)) {
    delay(1);
    //TODO: calculate timeout dynamically based on modem settings
    if (millis() - start > (length * 1500)) {
      ClearIRQFlags();
      return (ERR_TX_TIMEOUT);
    }
  }

  ClearIRQFlags();

  return (ERR_NONE);

#if defined(ARDUINO_ESP32_DEV)
  digitalWrite(_RXenablePin, LOW);
  digitalWrite(_TXenablePin, LOW); //the larger TX/RX modules require that the TX/RX enable pins are toggled
#endif

}


///////////////////////////////////Functions for Continous TX Packet mode/////////////////////////////////////////

void IRAM_ATTR SX127xDriver::TimerTaskTX_ISRhandler(){
  static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if(RadioState==RADIO_IDLE){
  xSemaphoreGiveFromISR(timer_sem, &xHigherPriorityTaskWoken);
  if ( xHigherPriorityTaskWoken) {
    SX127xDriver::TXstartMicros = micros();
    portYIELD_FROM_ISR(); // this wakes up sample_timer_task immediately
  }
  }
}


void IRAM_ATTR SX127xDriver::TimerTaskTX(void *param){
  timer_sem = xSemaphoreCreateBinary();

  while (1) {
    xSemaphoreTake(timer_sem, portMAX_DELAY);

      RadioState = RADIO_BUSY;

      TXnb(TXdataBuffer, TXbuffLen); // send the packet....

      PacketCount = PacketCount +1;

      SX127xDriver::TXspiTime = micros() - SX127xDriver::TXstartMicros; //measure how long it take to send SPI data. 
      
  }
}

void SX127xDriver::StopContTX(){
  detachInterrupt(digitalPinToInterrupt(SX127x_dio0));
  vTaskDelete(TimerTaskTX_handle);

}

void SX127xDriver::StartContTX() {

  attachInterrupt(digitalPinToInterrupt(SX127x_dio0), TXnbISR, RISING);

  SetMode(SX127X_STANDBY);
  setRegValue(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_TX_DONE, 7, 6);

  xTaskCreate(
    TimerTaskTX,          /* Task function. */
    "TimerTaskTX",        /* String with name of task. */
    1000,            /* Stack size in words. */
    NULL,             /* Parameter passed as input of the task */
    10,                /* Priority of the task. */
    &TimerTaskTX_handle);            /* Task handle. */

    timer = timerBegin(0, 40, true);
    timerAttachInterrupt(timer, &TimerTaskTX_ISRhandler, true);
    timerAlarmWrite(timer, TXContInterval, true);
    timerAlarmEnable(timer);
}
////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////TX functions/////////////////////////////////////////

void IRAM_ATTR SX127xDriver::TXnbISR() {
  //Serial.println("done ISR");
#if defined(ARDUINO_ESP32_DEV)

  digitalWrite(_TXenablePin, LOW); //the larger TX/RX modules require that the TX/RX enable pins are toggled
  //detachInterrupt(dio0);
#endif
  TXdoneMicros = micros();
  CalcOnAirTime();
  RadioState = RADIO_IDLE;
  
  }

uint8_t IRAM_ATTR SX127xDriver::TXnb(const volatile uint8_t * data, uint8_t length) {

  SetMode(SX127X_STANDBY);

  ClearIRQFlags();

  setRegValue(SX127X_REG_PAYLOAD_LENGTH, length);
  setRegValue(SX127X_REG_FIFO_TX_BASE_ADDR, SX127X_FIFO_TX_BASE_ADDR_MAX);
  setRegValue(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_TX_BASE_ADDR_MAX);

  writeRegisterBurstStr((uint8_t)SX127X_REG_FIFO, data, length);
  //writeRegisterFIFO((uint8_t)SX127X_REG_FIFO, data, length);

#if defined(ARDUINO_ESP32_DEV)

  digitalWrite(_RXenablePin, LOW);
  digitalWrite(_TXenablePin, HIGH); //the larger TX/RX modules require that the TX/RX enable pins are toggled
  //attachInterrupt(digitalPinToInterrupt(SX127x_dio0), TXnbISR, RISING);

#endif

  SetMode(SX127X_TX);

  return (ERR_NONE);

}

///////////////////////////////////RX Functions Non-Blocking///////////////////////////////////////////

void IRAM_ATTR SX127xDriver::RXnbISR() {
  //Serial.println("rxISRprocess");

  //    if(getRegValue(SX127X_REG_IRQ_FLAGS, 5, 5) == SX127X_CLEAR_IRQ_FLAG_PAYLOAD_CRC_ERROR) {
  //    Serial.println("CRC MISMTACH");
  //        return(ERR_CRC_MISMATCH);
  //    }
  //
  // if (headerExplMode) {
  //   RXbuffLen = getRegValue(SX127X_REG_RX_NB_BYTES);
  // }

  readRegisterBurst((uint8_t)SX127X_REG_FIFO, (uint8_t)RXbuffLen, RXdataBuffer);
  //CalcCRC16();

  (RXdoneCallback)();

  ClearIRQFlags();
  //Serial.println(".");

}


void IRAM_ATTR SX127xDriver::StartContRX(){
  SX127xDriver::RXnb();
}

void IRAM_ATTR SX127xDriver::StopContRX(){
  SX127xDriver::SetMode(SX127X_SLEEP);
}

uint8_t SX127xDriver::RXnb() {  //ADDED CHANGED

  // attach interrupt to DIO0
  //RX continuous mode

#if defined(ARDUINO_ESP32_DEV)
  digitalWrite(_TXenablePin, LOW); //the larger TX/RX modules require that the TX/RX enable pins are toggled
  digitalWrite(_RXenablePin, HIGH);
#endif

  SetMode(SX127X_STANDBY);

  ClearIRQFlags();

  if (headerExplMode == false) {

    setRegValue(SX127X_REG_PAYLOAD_LENGTH, RXbuffLen);
  }

  setRegValue(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_RX_DONE | SX127X_DIO1_RX_TIMEOUT, 7, 4);

  setRegValue(SX127X_REG_FIFO_RX_BASE_ADDR, SX127X_FIFO_RX_BASE_ADDR_MAX);
  setRegValue(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_RX_BASE_ADDR_MAX);

  SetMode(SX127X_RXCONTINUOUS);

  attachInterrupt(digitalPinToInterrupt(SX127x_dio0), RXnbISR, RISING);

  return (ERR_NONE);

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

// uint8_t SX127xDriver::RXsingle(uint8_t* data, uint8_t length) {

//   SetMode(SX127X_STANDBY);

//   setRegValue(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_RX_DONE | SX127X_DIO1_RX_TIMEOUT, 7, 4);
//   ClearIRQFlags();

//   setRegValue(SX127X_REG_FIFO_RX_BASE_ADDR, SX127X_FIFO_RX_BASE_ADDR_MAX);
//   setRegValue(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_RX_BASE_ADDR_MAX);

//   SetMode(SX127X_RXSINGLE);

//   while (!digitalRead(SX127x_dio0)) {
//     if (digitalRead(SX127x_dio1)) {
//       ClearIRQFlags();
//       return (ERR_RX_TIMEOUT);
//     }
//   }

//   if (getRegValue(SX127X_REG_IRQ_FLAGS, 5, 5) == SX127X_CLEAR_IRQ_FLAG_PAYLOAD_CRC_ERROR) {
//     return (ERR_CRC_MISMATCH);
//   }

//   if (headerExplMode) {
//     *length = getRegValue(SX127X_REG_RX_NB_BYTES);
//   }

//   readRegisterBurst((uint8_t)SX127X_REG_FIFO, length, data);

//   ClearIRQFlags();

//   return (ERR_NONE);
// }

uint8_t SX127xDriver::RunCAD() {
  SetMode(SX127X_STANDBY);

  setRegValue(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_CAD_DONE | SX127X_DIO1_CAD_DETECTED, 7, 4);
  ClearIRQFlags();

  SetMode(SX127X_CAD);

  while (!digitalRead(SX127x_dio0)) {
    if (digitalRead(SX127x_dio1)) {
      ClearIRQFlags();
      return (PREAMBLE_DETECTED);
    }
  }

  ClearIRQFlags();
  return (CHANNEL_FREE);
}

uint8_t IRAM_ATTR SX127xDriver::SetMode(uint8_t mode) { //if radio is not already in the required mode set it to the requested mode

  //if (!(_opmode == mode)) {
  setRegValue(SX127X_REG_OP_MODE, mode, 2, 0);
  // _opmode = (RadioOPmodes)mode;
  return (ERR_NONE);
  //  } else {
  //    if (DebugVerbosity >= DEBUG_3) {
  //      Serial.print("OPMODE was already at requested value: ");
  //      printOPMODE(mode);
  //      Serial.println();
  //    }
  //  }
}

uint8_t SX127xDriver::Config(Bandwidth bw, SpreadingFactor sf, CodingRate cr, float freq, uint8_t syncWord) {

  if(RFmodule==RFMOD_SX1276){
    SX1278config(bw, sf, cr, freq, syncWord);
  }

  if(RFmodule==RFmodule){
    SX1278config(bw, sf, cr, freq, syncWord);
  }

}


uint8_t SX127xDriver::SX127xConfig(uint8_t bw, uint8_t sf, uint8_t cr, float freq, uint8_t syncWord) {

  uint8_t status = ERR_NONE;

  // set mode to SLEEP
  status = SetMode(SX127X_SLEEP);
  if (status != ERR_NONE) {
    return (status);
  }

  // set LoRa mode
  status = setRegValue(SX127X_REG_OP_MODE, SX127X_LORA, 7, 7);
  if (status != ERR_NONE) {
    return (status);
  }

  SetFrequency(freq);


  // output power configuration
  status = setRegValue(SX127X_REG_PA_CONFIG, SX127X_PA_SELECT_BOOST | SX127X_OUTPUT_POWER);
  status = setRegValue(SX127X_REG_OCP, SX127X_OCP_ON | SX127X_OCP_TRIM, 5, 0);
  status = setRegValue(SX127X_REG_LNA, SX127X_LNA_GAIN_1 | SX127X_LNA_BOOST_ON);
  if (status != ERR_NONE) {
    return (status);
  }

  // turn off frequency hopping
  status = setRegValue(SX127X_REG_HOP_PERIOD, SX127X_HOP_PERIOD_OFF);
  if (status != ERR_NONE) {
    return (status);
  }

  // basic setting (bw, cr, sf, header mode and CRC)
  if (sf == SX127X_SF_6) {
    status = setRegValue(SX127X_REG_MODEM_CONFIG_2, SX127X_SF_6 | SX127X_TX_MODE_SINGLE, 7, 3);
    status = setRegValue(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_6, 2, 0);
    status = setRegValue(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_6);
  } else {
    status = setRegValue(SX127X_REG_MODEM_CONFIG_2, sf | SX127X_TX_MODE_SINGLE, 7, 3);
    status = setRegValue(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_7_12, 2, 0);
    status = setRegValue(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_7_12);
  }

  if (status != ERR_NONE) {
    return (status);
  }

  // set the sync word
  status = setRegValue(SX127X_REG_SYNC_WORD, syncWord);
  if (status != ERR_NONE) {
    return (status);
  }

  // set default preamble length
  status = setRegValue(SX127X_REG_PREAMBLE_MSB, SX127X_PREAMBLE_LENGTH_MSB);
  status = setRegValue(SX127X_REG_PREAMBLE_LSB, SX127X_PREAMBLE_LENGTH_LSB);
  if (status != ERR_NONE) {
    return (status);
  }

  // set mode to STANDBY
  status = SetMode(SX127X_STANDBY);
  return (status);
}


int8_t IRAM_ATTR SX127xDriver::GetLastPacketRSSI() {
  return (-157 + getRegValue(SX127X_REG_PKT_RSSI_VALUE));
}

float IRAM_ATTR SX127xDriver::GetLastPacketSNR() {
  int8_t rawSNR = (int8_t)getRegValue(SX127X_REG_PKT_SNR_VALUE);
  return (rawSNR / 4.0);
}

void IRAM_ATTR SX127xDriver::ClearIRQFlags() {
  writeRegister(SX127X_REG_IRQ_FLAGS, 0b11111111);
}

// const char* SX127xgetChipName() {
//   const char* names[] = {"SX1272", "SX1273", "SX1276", "SX1277", "SX1278", "SX1279"};
//   //return(names[_ch]);
//   return 0;
// }

void IRAM_ATTR SX127xDriver::CalcOnAirTime() {
  //Serial.println(LastTXdoneMicros - TXstartMicros);
  TimeOnAir = TXdoneMicros - TXstartMicros;
}


void IRAM_ATTR SX127xDriver::GetPacketRSSI_SNR() {
  LastPacketRSSI = GetLastPacketRSSI();
  LastPacketSNR = GetLastPacketSNR();

}

