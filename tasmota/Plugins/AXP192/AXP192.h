#ifndef __AXP192_H__
#define __AXP192_H__

#include <Wire.h>
#include <Arduino.h>

#define SLEEP_MSEC(us) (((uint64_t)us) * 1000L)
#define SLEEP_SEC(us)  (((uint64_t)us) * 1000000L)
#define SLEEP_MIN(us)  (((uint64_t)us) * 60L * 1000000L)
#define SLEEP_HR(us)   (((uint64_t)us) * 60L * 60L * 1000000L)

#define AXP_ADDR 0X34

#define PowerOff(x) SetSleep(x)

class AXP192 {
public:

    enum CHGCurrent{
        kCHG_100mA = 0,
        kCHG_190mA,
        kCHG_280mA,
        kCHG_360mA,
        kCHG_450mA,
        kCHG_550mA,
        kCHG_630mA,
        kCHG_700mA,
        kCHG_780mA,
        kCHG_880mA,
        kCHG_960mA,
        kCHG_1000mA,
        kCHG_1080mA,
        kCHG_1160mA,
        kCHG_1240mA,
        kCHG_1320mA,
    };

  	MODULE_PART AXP192();
  	MODULE_PART void  begin(void);
	MODULE_PART void  ScreenBreath(uint8_t brightness);
	MODULE_PART bool  GetBatState();
  
	MODULE_PART void  EnableCoulombcounter(void);
	MODULE_PART void  DisableCoulombcounter(void);
	MODULE_PART void  StopCoulombcounter(void);
	MODULE_PART void  ClearCoulombcounter(void);
	MODULE_PART uint32_t GetCoulombchargeData(void);
	MODULE_PART uint32_t GetCoulombdischargeData(void);
	MODULE_PART float GetCoulombData(void); 
	MODULE_PART void PowerOff(void);
	MODULE_PART void SetAdcState(bool state);
  	// -- sleep
	MODULE_PART void PrepareToSleep(void);
	MODULE_PART void RestoreFromLightSleep(void);
	MODULE_PART void DeepSleep(uint64_t time_in_us = 0);
	MODULE_PART void LightSleep(uint64_t time_in_us = 0);
  	MODULE_PART uint8_t GetWarningLeve(void);

public:
	// void SetChargeVoltage( uint8_t );
	// void SetChargeCurrent( uint8_t );
	MODULE_PART float GetBatVoltage();
	MODULE_PART float GetBatCurrent();
	MODULE_PART float GetVinVoltage();
	MODULE_PART float GetVinCurrent();
	MODULE_PART float GetVBusVoltage();
	MODULE_PART float GetVBusCurrent();
	MODULE_PART float GetTempInAXP192();
	MODULE_PART float GetBatPower();
	MODULE_PART float GetBatChargeCurrent();
	MODULE_PART float GetAPSVoltage();
	MODULE_PART float GetBatCoulombInput();
	MODULE_PART float GetBatCoulombOut();
  	MODULE_PART uint8_t GetWarningLevel(void);	
    MODULE_PART void SetCoulombClear();
	MODULE_PART void SetLDO2( bool State );
	MODULE_PART void SetDCDC3( bool State );

    MODULE_PART uint8_t AXPInState();
    MODULE_PART bool isACIN();
    MODULE_PART bool isCharging();
    MODULE_PART bool isVBUS();

    MODULE_PART void SetLDOVoltage(uint8_t number , uint16_t voltage);
    MODULE_PART void SetDCVoltage(uint8_t number , uint16_t voltage);
    MODULE_PART void SetESPVoltage(uint16_t voltage);
    MODULE_PART void SetLcdVoltage(uint16_t voltage);
    MODULE_PART void SetLDOEnable( uint8_t number ,bool state );
    MODULE_PART void SetLCDRSet( bool state );
    MODULE_PART void SetBusPowerMode( uint8_t state );
    MODULE_PART void SetLed(uint8_t state);
    MODULE_PART void SetSpkEnable(uint8_t state);
    MODULE_PART void SetCHGCurrent(uint8_t state);

private:
	char buffer[256];
	MODULE_PART void Write1Byte( uint8_t Addr ,  uint8_t Data );
	MODULE_PART uint8_t Read8bit( uint8_t Addr );
	MODULE_PART uint16_t Read12Bit( uint8_t Addr);
	MODULE_PART uint16_t Read13Bit( uint8_t Addr);
	MODULE_PART uint16_t Read16bit( uint8_t Addr );
	MODULE_PART uint32_t Read24bit( uint8_t Addr );
	MODULE_PART uint32_t Read32bit( uint8_t Addr );
	MODULE_PART void ReadBuff( uint8_t Addr , uint8_t Size , uint8_t *Buff );
}; 

#endif
