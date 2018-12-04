#ifndef LoRa_SX127x
#define LoRa_SX127x

#include <Arduino.h>

#include <cstdint>

#include "LoRa_SX127x_Regs.h"

#include "FreeRTOS.h"
#include "esp32-hal-timer.h"

typedef enum {CURR_OPMODE_FSK_OOK = 0b00000000, CURR_OPMODE_LORA = 0b10000000, //removed CURR_OPMODE_ACCESS_SHARED_REG_OFF and CURR_OPMODE_ACCESS_SHARED_REG_ON for now
CURR_OPMODE_SLEEP = 0b00000000, CURR_OPMODE_STANDBY = 0b00000001, CURR_OPMODE_FSTX = 0b00000010, CURR_OPMODE_TX = 0b00000011, CURR_OPMODE_FSRX = 0b00000100,
CURR_OPMODE_RXCONTINUOUS = 0b00000101, CURR_OPMODE_RXSINGLE = 0b00000110, CURR_OPMODE_CAD = 0b00000111} RadioOPmodes;


typedef enum {CH_SX1272, CH_SX1273, CH_SX1276, CH_SX1277, CH_SX1278, CH_SX1279} Chip;
typedef enum {BW_7_80_KHZ, BW_10_40_KHZ, BW_15_60_KHZ, BW_20_80_KHZ, BW_31_25_KHZ, BW_41_70_KHZ, BW_62_50_KHZ, BW_125_00_KHZ, BW_250_00_KHZ, BW_500_00_KHZ} Bandwidth;
typedef enum {SF_6, SF_7, SF_8, SF_9, SF_10, SF_11, SF_12} SpreadingFactor;
typedef enum {CR_4_5, CR_4_6, CR_4_7, CR_4_8} CodingRate;
typedef enum {RFMOD_SX1278, RFMOD_SX1276} RFmodule_;
typedef enum {CONT_OFF, CONT_TX, CONT_RX} ContinousMode;
typedef enum {RADIO_IDLE, RADIO_BUSY} RadioState_;



class SX127xDriver{

    public:

        ///////Callback Function Pointers/////

        static void (*RXdoneCallback)(); //function pointer for callback
        static void (*TXdoneCallback)(); //function pointer for callback

        static void (*TXtimeout)(); //function pointer for callback
        static void (*RXtimeout)(); //function pointer for callback

        static TaskHandle_t TimertaskTX_handle;  //Task Handle for ContTX mode

        //static void (*TXcallback)();

        ////////Hardware/////////////
        static uint8_t _RXenablePin;
        static uint8_t _TXenablePin;

        static uint8_t SX127x_nss;
        static uint8_t SX127x_dio0;
        static uint8_t SX127x_dio1;

        static uint8_t SX127x_MOSI;
        static uint8_t SX127x_MISO;
        static uint8_t SX127x_SCK;
        static uint8_t SX127x_RST;

        /////////////////////////////

        ///////////Radio Variables////////
        static volatile uint8_t TXdataBuffer[256];
        static volatile uint8_t RXdataBuffer[256];

        static volatile uint8_t TXbuffLen;
        static volatile uint8_t RXbuffLen;

        static volatile uint32_t PacketCount;

        static volatile bool headerExplMode;

        static volatile uint32_t TXContInterval; //20ms default for now.
        


        static float _freq;
        static uint8_t _syncWord;

        static ContinousMode ContMode;
        static RFmodule_ RFmodule;
        static Bandwidth _bw;
        static SpreadingFactor _sf;
        static CodingRate _cr;
        static RadioOPmodes _opmode;
        static RadioState_ RadioState;
        ///////////////////////////////////

        /////////////Packet Stats//////////
        static int8_t LastPacketRSSI;
        static float LastPacketSNR;
        static float PacketLossRate;

        static uint32_t TimeOnAir;
        static uint32_t TXstartMicros;
        static uint32_t TXspiTime;
        static uint32_t LastTXdoneMicros;
        //////////////////////////////////


        //////////Functions//////////////
        void timer0_ISRtx(void);
        void timer0_ISRrx(void);

        //const char* getChipName();

        ////////////////Configuration Functions/////////////
        static uint8_t Begin();
        static uint8_t Config(Bandwidth bw, SpreadingFactor sf, CodingRate cr, float freq, uint8_t syncWord);        
        static uint8_t SX127xConfig(uint8_t bw, uint8_t sf, uint8_t cr, float freq, uint8_t syncWord);
        static uint8_t setBandwidth(Bandwidth bw);
        static uint8_t setSpreadingFactor(SpreadingFactor sf);
        static uint8_t setSyncWord(uint8_t syncWord);
        static uint8_t SetOutputPower(uint8_t Power);
        static uint8_t SetPreambleLength(uint8_t PreambleLen);
        static uint8_t SetSpreadingFactor(SpreadingFactor sf);
        static uint8_t SetCodingRate(CodingRate cr);
        static uint8_t SetFrequency(float freq);
        static uint8_t SX127xBegin();
        static uint8_t SetMode(uint8_t mode);
        static uint8_t TX(uint8_t* data, uint8_t length);
        ////////////////////////////////////////////////////

        /////////////////Utility Funcitons//////////////////
        static void ClearIRQFlags();
   


        //////////////RX related Functions/////////////////

        
        static uint8_t RXsingle(uint8_t* data, uint8_t* length);

        static uint8_t RunCAD();

       
        static void SX127xclearIRQFlags();


        static int8_t IRAM_ATTR GetLastPacketRSSI();
        static float IRAM_ATTR GetLastPacketSNR();
        static void IRAM_ATTR CalcOnAirTime();
        static void IRAM_ATTR GetPacketRSSI_SNR();

       

       
       ////////////Non-blocking TX related Functions/////////////////

        static void IRAM_ATTR StartContTX(); //Start Cont TX mode, sends data continuiously 
        static void IRAM_ATTR StopContTX();
        static uint8_t IRAM_ATTR TXnb(const volatile uint8_t * data, uint8_t length);
        

        static void IRAM_ATTR TXnbISR();  //ISR for non-blocking TX routine
        static void IRAM_ATTR TimerTaskTX_ISRhandler();
        static void IRAM_ATTR TimerTaskTX(void *param);

        /////////////Non-blocking RX related Functions///////////////
        
        static void IRAM_ATTR StartContRX();
        static void IRAM_ATTR StopContRX();
        static uint8_t RXnb();

        static void IRAM_ATTR RXnbISR();  //ISR for non-blocking RC routine

        //static uint8_t rxSingle(char* data, uint8_t* length);
        static uint8_t rxContinuous(char* data, uint8_t* length);
        static uint8_t rxISRprocess(char* data, uint8_t* length);

        

    private:
        // static void timer_isr_handler();
        // static void IRAM_ATTR TimerTask(void *param);
            //static hw_timer_t * timer;






};













//enum RadioOPmodes {CURR_OPMODE_FSK_OOK = 0b00000000, CURR_OPMODE_LORA = 0b10000000, CURR_OPMODE_ACCESS_SHARED_REG_OFF = 0b00000000, CURR_OPMODE_ACCESS_SHARED_REG_ON = 0b01000000,
//                  CURR_OPMODE_SLEEP = 0b00000000, CURR_OPMODE_STANDBY = 0b00000001, CURR_OPMODE_FSTX = 0b00000010, CURR_OPMODE_TX = 0b00000011, CURR_OPMODE_FSRX = 0b00000100,
//                  CURR_OPMODE_RXCONTINUOUS = 0b00000101, CURR_OPMODE_RXSINGLE = 0b00000110, CURR_OPMODE_CAD = 0b00000111
//                 };



//#define SX127X_ACCESS_SHARED_REG_ON                   0b01000000  //  6     6     access FSK registers (0x0D:0x3F) in LoRa mode
//#define SX127X_SLEEP                                    //  2     0     sleep
//#define SX127X_STANDBY                                  //  2     0     standby
//#define SX127X_FSTX                                     //  2     0     frequency synthesis TX
//#define SX127X_TX                                       //  2     0     transmit
//#define SX127X_FSRX                                     //  2     0     frequency synthesis RX
//#define SX127X_RXCONTINUOUS                             //  2     0     receive continuous
//#define SX127X_RXSINGLE                                 //  2     0     receive single
//#define SX127X_CAD                                      //  2     0     channel activity detection







// uint8_t SX127xBegin();

// uint8_t SX127xTX(char* data, uint8_t length);
// uint8_t SX127xrxSingle(char* data, uint8_t* length, bool headerExplMode);
// uint8_t SX127xrxContinuous(char* data, uint8_t* length, bool headerExplMode); //ADDED CHANGED
// uint8_t SX127xrxISRprocess(char* data, uint8_t* length, bool headerExplMode); //ADDED CHANGED

// void SX127xrxISR(); //ADDED CHANGED

// void SX127xTXnbISR();
// uint8_t SX127xTXnb(char* data, uint8_t length);
// uint8_t SX127xrunCAD();

// uint8_t SX127xsetMode(uint8_t mode);
// uint8_t SX127xconfig(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq, uint8_t syncWord); //CHANGED TO uint32_t freq instead of float freq
// int8_t SX127xgetLastPacketRSSI();
// float SX127xgetLastPacketSNR();


// uint8_t SX127xconfigCommon(uint8_t bw, uint8_t sf, uint8_t cr, float freq, uint8_t syncWord);


////  private:
//Chip _ch;
//int _dio0;
//int _dio1;
//





#endif



