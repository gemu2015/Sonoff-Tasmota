
enum
    {
        CCS811_STATUS = 0x00,
        CCS811_MEAS_MODE = 0x01,
        CCS811_ALG_RESULT_DATA = 0x02,
        CCS811_RAW_DATA = 0x03,
        CCS811_ENV_DATA = 0x05,
        CCS811_NTC = 0x06,
        CCS811_THRESHOLDS = 0x10,
        CCS811_BASELINE = 0x11,
        CCS811_HW_ID = 0x20,
        CCS811_HW_VERSION = 0x21,
        CCS811_FW_BOOT_VERSION = 0x23,
        CCS811_FW_APP_VERSION = 0x24,
        CCS811_ERROR_ID = 0xE0,
        CCS811_SW_RESET = 0xFF,
    };

	//bootloader registers
	enum
	{
		CCS811_BOOTLOADER_APP_ERASE = 0xF1,
		CCS811_BOOTLOADER_APP_DATA = 0xF2,
		CCS811_BOOTLOADER_APP_VERIFY = 0xF3,
		CCS811_BOOTLOADER_APP_START = 0xF4
	};

	enum
	{
		CCS811_DRIVE_MODE_IDLE = 0x00,
		CCS811_DRIVE_MODE_1SEC = 0x01,
		CCS811_DRIVE_MODE_10SEC = 0x02,
		CCS811_DRIVE_MODE_60SEC = 0x03,
		CCS811_DRIVE_MODE_250MS = 0x04,
	};





#define CCS811_HW_ID_CODE			0x81
#define CCS811_REF_RESISTOR			100000

MODULE_PART void CCS811_SWReset();
MODULE_PART uint8_t CCS811_read8(byte reg);
MODULE_PART void CCS811_read(uint8_t reg, uint8_t *buf, uint8_t num);
MODULE_PART void CCS811_write8(byte reg, byte value);
MODULE_PART void CCS811_write(uint8_t reg, uint8_t *buf, uint8_t num);
MODULE_PART void CCS811_disableInterrupt();
MODULE_PART void CCS811_setDriveMode(uint8_t mode);


/**************************************************************************/
/*!
    @brief  Setups the I2C interface and hardware and checks for communication.
    @param  addr Optional I2C address the sensor can be found on. Default is 0x5A
    @returns True if device is set up, false on any failure
*/
/**************************************************************************/
MODULE_PART sint8_t CCS811_begin(uint8_t addr) {
	SETREGS
	ccs.i2c_addr = addr;
#ifdef ESP8266
	setClockStretchLimit(1000);
#endif

	CCS811_SWReset();

	delay(100);

	//check that the HW id is correct
	uint8_t hwvers = CCS811_read8(CCS811_HW_ID);

	if (hwvers != CCS811_HW_ID_CODE) {
		return 1;
	}

	CCS811_write(CCS811_BOOTLOADER_APP_START,NULL,0);

	delay(100);
	
	ccs.stat.data = CCS811_read8(CCS811_STATUS);

	if (ccs.stat.ERROR) {
		return 2;
	}

	if (!ccs.stat.FW_MODE) {
		return 3;
	}

	CCS811_disableInterrupt();

	CCS811_setDriveMode(CCS811_DRIVE_MODE_1SEC);

	return 0;
}

MODULE_PART uint16_t CCS811_getTVOC() {
	SETREGS
	return ccs._TVOC;
}
MODULE_PART uint16_t CCS811_geteCO2() {
	SETREGS
	return ccs._eCO2;
}

/**************************************************************************/
/*!
    @brief  sample rate of the sensor.
    @param  mode one of CCS811_DRIVE_MODE_IDLE, CCS811_DRIVE_MODE_1SEC, CCS811_DRIVE_MODE_10SEC, CCS811_DRIVE_MODE_60SEC, CCS811_DRIVE_MODE_250MS.
*/
MODULE_PART void CCS811_setDriveMode(uint8_t mode) {
	SETREGS
	ccs.meas.DRIVE_MODE = mode;
	CCS811_write8(CCS811_MEAS_MODE, ccs.meas.data);
}

/**************************************************************************/
/*!
    @brief  enable the data ready interrupt pin on the device.
*/
/**************************************************************************/
MODULE_PART void CCS811_enableInterrupt() {
	SETREGS
	ccs.meas.INTERRUPT = 1;
	CCS811_write8(CCS811_MEAS_MODE, ccs.meas.data);
}

/**************************************************************************/
/*!
    @brief  disable the data ready interrupt pin on the device
*/
/**************************************************************************/
MODULE_PART void CCS811_disableInterrupt() {
	SETREGS
	ccs.meas.INTERRUPT = 0;
	CCS811_write8(CCS811_MEAS_MODE, ccs.meas.data);
}

/**************************************************************************/
/*!
    @brief  checks if data is available to be read.
    @returns True if data is ready, false otherwise.
*/
/**************************************************************************/
MODULE_PART bool CCS811_available() {
	//_status.set(read8(CCS811_STATUS));
	SETREGS
	ccs.stat.data = CCS811_read8(CCS811_STATUS);
	if (!ccs.stat.DATA_READY)
		return false;
	else return true;
}

/**************************************************************************/
/*!
    @brief  read and store the sensor data. This data can be accessed with getTVOC() and geteCO2()
    @returns 0 if no error, error code otherwise.
*/
/**************************************************************************/
MODULE_PART uint8_t CCS811_readData() {
	SETREGS

	if (!CCS811_available())
		return false;
	else {
		uint8_t buf[8];
		CCS811_read(CCS811_ALG_RESULT_DATA, buf, 8);

		ccs._eCO2 = ((uint16_t)buf[0] << 8) | ((uint16_t)buf[1]);
		ccs._TVOC = ((uint16_t)buf[2] << 8) | ((uint16_t)buf[3]);

		//if (ccs.stat.ERROR)
		//	return buf[5];

		//else return 0;
		return 0;
	}
}

/**************************************************************************/
/*!
    @brief  set the humidity and temperature compensation for the sensor.
    @param humidity the humidity data as a percentage. For 55% humidity, pass in integer 55.
    @param temperature the temperature in degrees C as a decimal number. For 25.5 degrees C, pass in 25.5
*/
/**************************************************************************/
MODULE_PART void CCS811_setEnvironmentalData(uint8_t humidity, float temperature) {
	SETREGS
	/* Humidity is stored as an unsigned 16 bits in 1/512%RH. The
	default value is 50% = 0x64, 0x00. As an example 48.5%
	humidity would be 0x61, 0x00.*/

	/* Temperature is stored as an unsigned 16 bits integer in 1/512
	degrees; there is an offset: 0 maps to -25°C. The default value is
	25°C = 0x64, 0x00. As an example 23.5% temperature would be
	0x61, 0x00.
	The internal algorithm uses these values (or default values if
	not set by the application) to compensate for changes in
	relative humidity and ambient temperature.*/

	uint8_t hum_perc = humidity << 1;

	// crashes always ?????
	// float temp;
	// float frac = modff(temperature, &temp);

	float frac = 0;

	uint16_t temp_high = ((tmod__fixunssfsi(temperature) + 25) << 9);

	
	float divs = tmod__divsf3(frac, 0.001953125);
	uint16_t temp_low = (tmod__fixunssfsi(divs) & 0x1FF);

	uint16_t temp_conv = (temp_high | temp_low);

	uint8_t buf[] = {hum_perc, 0x00, (uint8_t)((temp_conv >> 8) & 0xFF), (uint8_t)(temp_conv & 0xFF)};

	CCS811_write(CCS811_ENV_DATA, buf, 4);

}

/**************************************************************************/
/*!
    @brief  calculate the temperature using the onboard NTC resistor.
    @returns temperature as a double.
*/
/**************************************************************************/
#if 0
MODULE_PART double CCS811_calculateTemperature() {
	SETREGS
	uint8_t buf[4];
	CCS811_read(CCS811_NTC, buf, 4);

	uint32_t vref = ((uint32_t)buf[0] << 8) | buf[1];
	uint32_t vntc = ((uint32_t)buf[2] << 8) | buf[3];

	//from ams ccs811 app note
	uint32_t rntc = vntc * CCS811_REF_RESISTOR / vref;

	double ntc_temp;
	ntc_temp = log((double)rntc / CCS811_REF_RESISTOR); // 1
	ntc_temp /= 3380; // 2
	ntc_temp += 1.0 / (25 + 273.15); // 3
	ntc_temp = 1.0 / ntc_temp; // 4
	ntc_temp -= 273.15; // 5
	//return ntc_temp - _tempOffset;
	return ntc_temp;

}
#endif
/**************************************************************************/
/*!
    @brief  set interrupt thresholds
    @param low_med the level below which an interrupt will be triggered.
    @param med_high the level above which the interrupt will ge triggered.
    @param hysteresis optional histeresis level. Defaults to 50
*/
/**************************************************************************/
MODULE_PART void CCS811_setThresholds(uint16_t low_med, uint16_t med_high, uint8_t hysteresis) {
	SETREGS
	uint8_t buf[] = {(uint8_t)((low_med >> 8) & 0xF), (uint8_t)(low_med & 0xF),
	(uint8_t)((med_high >> 8) & 0xF), (uint8_t)(med_high & 0xF), hysteresis};

	CCS811_write(CCS811_THRESHOLDS, buf, 5);
}

/**************************************************************************/
/*!
    @brief  trigger a software reset of the device
*/
/**************************************************************************/
MODULE_PART void CCS811_SWReset() {
	SETREGS
	//reset sequence from the datasheet
	uint8_t seq[] = {0x11, 0xE5, 0x72, 0x8A};
	CCS811_write(CCS811_SW_RESET, seq, 4);
}

/**************************************************************************/
/*!
    @brief   read the status register and store any errors.
    @returns the error bits from the status register of the device.
*/
/**************************************************************************/
MODULE_PART bool CCS811_checkError() {
	SETREGS
	ccs.stat.data = CCS811_read8(CCS811_STATUS);
	return ccs.stat.ERROR;
}

/**************************************************************************/
/*!
    @brief  write one byte of data to the specified register
    @param  reg the register to write to
    @param  value the value to write
*/
/**************************************************************************/
MODULE_PART void CCS811_write8(byte reg, byte value) {
	SETREGS
	CCS811_write(reg, &value, 1);
}

/**************************************************************************/
/*!
    @brief  read one byte of data from the specified register
    @param  reg the register to read
    @returns one byte of register data
*/
/**************************************************************************/
MODULE_PART uint8_t CCS811_read8(byte reg) {
	SETREGS
	uint8_t ret;
	CCS811_read(reg, &ret, 1);
	return ret;
}

MODULE_PART void CCS811_read(uint8_t reg, uint8_t *buf, uint8_t num) {
	SETREGS
	uint8_t value;
	uint8_t pos = 0;

	//on arduino we need to read in 32 byte chunks
	while( pos < num) {

		uint8_t read_now = min((uint8_t)32, (uint8_t)(num - pos));
		beginTransmission(ccs.i2c_addr);
		write(reg + pos);
		endTransmission(false);
		requestFrom(ccs.i2c_addr, read_now);

		for (int i=0; i<read_now; i++) {
			buf[pos] = read();
			pos++;
		}
	}
}

MODULE_PART void CCS811_write(uint8_t reg, uint8_t *buf, uint8_t num) {
	SETREGS
	beginTransmission(ccs.i2c_addr);
	write(reg);
	writen(buf, num);
	endTransmission(false);
}
