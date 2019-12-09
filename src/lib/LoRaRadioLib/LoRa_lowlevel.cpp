#include "LoRa_lowlevel.h"
#include <SPI.h>

Verbosity_ DebugVerbosity = DEBUG_1;

void initModule(uint8_t nss, uint8_t dio0, uint8_t dio1)
{
  pinMode(SX127xDriver::SX127x_nss, OUTPUT);
  pinMode(SX127xDriver::SX127x_dio0, INPUT);
  pinMode(SX127xDriver::SX127x_dio1, INPUT);

  pinMode(SX127xDriver::SX127x_MOSI, OUTPUT);
  pinMode(SX127xDriver::SX127x_MISO, INPUT);
  pinMode(SX127xDriver::SX127x_SCK, OUTPUT);
  pinMode(SX127xDriver::SX127x_RST, OUTPUT);

  digitalWrite(SX127xDriver::SX127x_nss, HIGH);

#ifdef PLATFORM_ESP32
  SPI.begin(SX127xDriver::SX127x_SCK, SX127xDriver::SX127x_MISO, SX127xDriver::SX127x_MOSI, -1); // sck, miso, mosi, ss (ss can be any GPIO)
#else
  SPI.begin();
#endif

  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setFrequency(10000000);
}

uint8_t ICACHE_RAM_ATTR getRegValue(uint8_t reg, uint8_t msb, uint8_t lsb)
{
  if ((msb > 7) || (lsb > 7) || (lsb > msb))
  {
    return (ERR_INVALID_BIT_RANGE);
  }
  uint8_t rawValue = readRegister(reg);
  uint8_t maskedValue = rawValue & ((0b11111111 << lsb) & (0b11111111 >> (7 - msb)));
  return (maskedValue);
}

uint8_t ICACHE_RAM_ATTR readRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t *inBytes)
{
  char OutByte;
  digitalWrite(SX127xDriver::SX127x_nss, LOW);
  OutByte = (reg | SPI_READ);

  SPI.write(OutByte);

  for (uint8_t i = 0; i < numBytes; i++)
  {
    inBytes[i] = SPI.transfer(reg);
  }
  digitalWrite(SX127xDriver::SX127x_nss, HIGH);

  if (DebugVerbosity >= DEBUG_4)
  {
    Serial.print("SPI: Read Burst ");
    Serial.print("REG: ");
    Serial.print(reg);
    Serial.print(" LEN: ");
    Serial.print(numBytes);
    Serial.print(" DATA: ");

    for (int i = 0; i < numBytes; i++)
    {
      Serial.print(inBytes[i]);
    }

    Serial.println();
  }

  return (ERR_NONE);
}

uint8_t ICACHE_RAM_ATTR readRegisterBurst(uint8_t reg, uint8_t numBytes, volatile uint8_t *inBytes)
{
  char OutByte;

  digitalWrite(SX127xDriver::SX127x_nss, LOW);

  OutByte = (reg | SPI_READ);

  SPI.write(OutByte);

  for (uint8_t i = 0; i < numBytes; i++)
  {
    inBytes[i] = SPI.transfer(reg);
  }

  digitalWrite(SX127xDriver::SX127x_nss, HIGH);

  if (DebugVerbosity >= DEBUG_4)
  {
    Serial.print("SPI: Read Burst ");
    Serial.print("REG: ");
    Serial.print(reg);
    Serial.print(" LEN: ");
    Serial.print(numBytes);
    Serial.print(" DATA: ");

    for (int i = 0; i < numBytes; i++)
    {
      Serial.print(inBytes[i]);
    }

    Serial.println();
  }

  return (ERR_NONE);
}

uint8_t ICACHE_RAM_ATTR readRegisterBurst(uint8_t reg, uint8_t numBytes, char *inBytes)
{
  digitalWrite(SX127xDriver::SX127x_nss, LOW);

  SPI.write(reg | SPI_READ);

  for (uint8_t i = 0; i < numBytes; i++)
  {
    inBytes[i] = SPI.transfer(reg);
  }

  digitalWrite(SX127xDriver::SX127x_nss, HIGH);

  if (DebugVerbosity >= DEBUG_4)
  {
    Serial.print("SPI: Read BurstStr ");
    Serial.print("REG: ");
    Serial.print(reg);
    Serial.print(" LEN: ");
    Serial.print(numBytes);
    Serial.print(" DATA: ");

    for (int i = 0; i < numBytes; i++)
    {
      Serial.print(inBytes[i]);
    }

    Serial.println();
  }

  return (ERR_NONE);
}

uint8_t ICACHE_RAM_ATTR readRegister(uint8_t reg)
{
  uint8_t InByte;
  uint8_t OutByte;
  digitalWrite(SX127xDriver::SX127x_nss, LOW);

  OutByte = (reg | SPI_READ);

  SPI.write(OutByte);

  InByte = SPI.transfer(0x00);

  digitalWrite(SX127xDriver::SX127x_nss, HIGH);

  return (InByte);
}

uint8_t ICACHE_RAM_ATTR setRegValue(uint8_t reg, uint8_t value, uint8_t msb, uint8_t lsb)
{
  if ((msb > 7) || (lsb > 7) || (lsb > msb))
  {
    return (ERR_INVALID_BIT_RANGE);
  }

  uint8_t currentValue = readRegister(reg);
  uint8_t mask = ~((0b11111111 << (msb + 1)) | (0b11111111 >> (8 - lsb)));
  uint8_t newValue = (currentValue & ~mask) | (value & mask);
  writeRegister(reg, newValue);
  return (ERR_NONE);
}

void ICACHE_RAM_ATTR writeRegisterBurstStr(uint8_t reg, uint8_t *data, uint8_t numBytes)
{
  digitalWrite(SX127xDriver::SX127x_nss, LOW);

  SPI.write(reg | SPI_WRITE);
  SPI.writeBytes(data, numBytes);

  digitalWrite(SX127xDriver::SX127x_nss, HIGH);
}

void ICACHE_RAM_ATTR writeRegisterBurstStr(uint8_t reg, const volatile uint8_t *data, uint8_t numBytes)
{
  digitalWrite(SX127xDriver::SX127x_nss, LOW);

  SPI.write(reg | SPI_WRITE);
  SPI.writeBytes((uint8_t *)data, numBytes);

  digitalWrite(SX127xDriver::SX127x_nss, HIGH);
}

void ICACHE_RAM_ATTR writeRegister(uint8_t reg, uint8_t data)
{
  digitalWrite(SX127xDriver::SX127x_nss, LOW);

  SPI.write(reg | SPI_WRITE);

  SPI.write(data);

  digitalWrite(SX127xDriver::SX127x_nss, HIGH);

  // if (DebugVerbosity >= DEBUG_4)
  // {
  //   Serial.print("SPI: Write ");
  //   Serial.print("REG: ");
  //   Serial.print(reg, HEX);
  //   Serial.print(" VAL: ");
  //   Serial.println(data, HEX);
  // }
}