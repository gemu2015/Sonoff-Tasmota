VL53L0X {
 
  enum regAddr {
    SYSRANGE_START = 0x00,
    SYSTEM_THRESH_HIGH = 0x0C,
    SYSTEM_THRESH_LOW = 0x0E,
    SYSTEM_SEQUENCE_CONFIG = 0x01,
    SYSTEM_RANGE_CONFIG = 0x09,
    SYSTEM_INTERMEASUREMENT_PERIOD = 0x04,
    SYSTEM_INTERRUPT_CONFIG_GPIO = 0x0A,
    GPIO_HV_MUX_ACTIVE_HIGH = 0x84,
    SYSTEM_INTERRUPT_CLEAR = 0x0B,
    RESULT_INTERRUPT_STATUS = 0x13,
    RESULT_RANGE_STATUS = 0x14,
    RESULT_CORE_AMBIENT_WINDOW_EVENTS_RTN = 0xBC,
    RESULT_CORE_RANGING_TOTAL_EVENTS_RTN = 0xC0,
    RESULT_CORE_AMBIENT_WINDOW_EVENTS_REF = 0xD0,
    RESULT_CORE_RANGING_TOTAL_EVENTS_REF = 0xD4,
    RESULT_PEAK_SIGNAL_RATE_REF = 0xB6,
    ALGO_PART_TO_PART_RANGE_OFFSET_MM = 0x28,
    I2C_SLAVE_DEVICE_ADDRESS = 0x8A,
    MSRC_CONFIG_CONTROL = 0x60,
    PRE_RANGE_CONFIG_MIN_SNR = 0x27,
    PRE_RANGE_CONFIG_VALID_PHASE_LOW = 0x56,
    PRE_RANGE_CONFIG_VALID_PHASE_HIGH = 0x57,
    PRE_RANGE_MIN_COUNT_RATE_RTN_LIMIT = 0x64,
    FINAL_RANGE_CONFIG_MIN_SNR = 0x67,
    FINAL_RANGE_CONFIG_VALID_PHASE_LOW = 0x47,
    FINAL_RANGE_CONFIG_VALID_PHASE_HIGH = 0x48,
    FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT = 0x44,
    PRE_RANGE_CONFIG_SIGMA_THRESH_HI = 0x61,
    PRE_RANGE_CONFIG_SIGMA_THRESH_LO = 0x62,
    PRE_RANGE_CONFIG_VCSEL_PERIOD = 0x50,
    PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI = 0x51,
    PRE_RANGE_CONFIG_TIMEOUT_MACROP_LO = 0x52,
    SYSTEM_HISTOGRAM_BIN = 0x81,
    HISTOGRAM_CONFIG_INITIAL_PHASE_SELECT = 0x33,
    HISTOGRAM_CONFIG_READOUT_CTRL = 0x55,
    FINAL_RANGE_CONFIG_VCSEL_PERIOD = 0x70,
    FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI = 0x71,
    FINAL_RANGE_CONFIG_TIMEOUT_MACROP_LO = 0x72,
    CROSSTALK_COMPENSATION_PEAK_RATE_MCPS = 0x20,
    MSRC_CONFIG_TIMEOUT_MACROP = 0x46,
    SOFT_RESET_GO2_SOFT_RESET_N = 0xBF,
    IDENTIFICATION_MODEL_ID = 0xC0,
    IDENTIFICATION_REVISION_ID = 0xC2,
    OSC_CALIBRATE_VAL = 0xF8,
    GLOBAL_CONFIG_VCSEL_WIDTH = 0x32,
    GLOBAL_CONFIG_SPAD_ENABLES_REF_0 = 0xB0,
    GLOBAL_CONFIG_SPAD_ENABLES_REF_1 = 0xB1,
    GLOBAL_CONFIG_SPAD_ENABLES_REF_2 = 0xB2,
    GLOBAL_CONFIG_SPAD_ENABLES_REF_3 = 0xB3,
    GLOBAL_CONFIG_SPAD_ENABLES_REF_4 = 0xB4,
    GLOBAL_CONFIG_SPAD_ENABLES_REF_5 = 0xB5,
    GLOBAL_CONFIG_REF_EN_START_SELECT = 0xB6,
    DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD = 0x4E,
    DYNAMIC_SPAD_REF_EN_START_OFFSET = 0x4F,
    POWER_MANAGEMENT_GO1_POWER_FORCE = 0x80,
    VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV = 0x89,
    ALGO_PHASECAL_LIM = 0x30,
    ALGO_PHASECAL_CONFIG_TIMEOUT = 0x30,
  };
  enum vcselPeriodType { VcselPeriodPreRange, VcselPeriodFinalRange };
  uint8_t last_status;
 MODULE_PART  VL53L0X_VL53L0X(void);
 MODULE_PART  void VL53L0X_setAddress(uint8_t new_addr);
 MODULE_PART  inline uint8_t VL53L0X_getAddress(void) { return address; }
 MODULE_PART  bool VL53L0X_init(bool io_2v8 = true);
 MODULE_PART  void VL53L0X_writeReg(uint8_t reg, uint8_t value);
 MODULE_PART  void VL53L0X_writeReg16Bit(uint8_t reg, uint16_t value);
 MODULE_PART  void VL53L0X_writeReg32Bit(uint8_t reg, uint32_t value);
 MODULE_PART  uint8_t VL53L0X_readReg(uint8_t reg);
 MODULE_PART  uint16_t VL53L0X_readReg16Bit(uint8_t reg);
 MODULE_PART  uint32_t VL53L0X_readReg32Bit(uint8_t reg);
 MODULE_PART  void VL53L0X_writeMulti(uint8_t reg, uint8_t const* src, uint8_t count);
 MODULE_PART  void VL53L0X_readMulti(uint8_t reg, uint8_t* dst, uint8_t count);
 MODULE_PART  bool VL53L0X_setSignalRateLimit(float limit_Mcps);
 MODULE_PART  float VL53L0X_getSignalRateLimit(void);
 MODULE_PART  bool VL53L0X_setMeasurementTimingBudget(uint32_t budget_us);
 MODULE_PART  uint32_t VL53L0X_getMeasurementTimingBudget(void);
 MODULE_PART  bool VL53L0X_setVcselPulsePeriod(vcselPeriodType type, uint8_t period_pclks);
 MODULE_PART  uint8_t VL53L0X_getVcselPulsePeriod(vcselPeriodType type);
 MODULE_PART  void VL53L0X_startContinuous(uint32_t period_ms = 0);
 MODULE_PART  void VL53L0X_stopContinuous(void);
 MODULE_PART  uint16_t VL53L0X_readRangeContinuousMillimeters(void);
 MODULE_PART  uint16_t VL53L0X_readRangeSingleMillimeters(void);
 MODULE_PART  inline void VL53L0X_setTimeout(uint16_t timeout) { io_timeout = timeout; }
 MODULE_PART  inline uint16_t VL53L0X_getTimeout(void) { return io_timeout; }
 MODULE_PART  bool VL53L0X_timeoutOccurred(void);
 
  struct SequenceStepEnables {
    boolean tcc, msrc, dss, pre_range, final_range;
  };
  struct SequenceStepTimeouts {
    uint16_t pre_range_vcsel_period_pclks, final_range_vcsel_period_pclks;
    uint16_t msrc_dss_tcc_mclks, pre_range_mclks, final_range_mclks;
    uint32_t msrc_dss_tcc_us, pre_range_us, final_range_us;
  };
  uint8_t address;
  uint16_t io_timeout;
  bool did_timeout;
  uint16_t timeout_start_ms;
  uint8_t stop_variable;
  uint32_t measurement_timing_budget_us;
 MODULE_PART  bool VL53L0X_getSpadInfo(uint8_t* count, bool* type_is_aperture);
 MODULE_PART  void VL53L0X_getSequenceStepEnables(SequenceStepEnables* enables);
 MODULE_PART  void VL53L0X_getSequenceStepTimeouts(SequenceStepEnables const* enables, SequenceStepTimeouts* timeouts);
 MODULE_PART  bool VL53L0X_performSingleRefCalibration(uint8_t vhv_init_byte);
 MODULE_PART  static uint16_t VL53L0X_decodeTimeout(uint16_t value);
 MODULE_PART  static uint16_t VL53L0X_encodeTimeout(uint16_t timeout_mclks);
 MODULE_PART  static uint32_t VL53L0X_timeoutMclksToMicroseconds(uint16_t timeout_period_mclks, uint8_t vcsel_period_pclks);
 MODULE_PART  static uint32_t VL53L0X_timeoutMicrosecondsToMclks(uint32_t timeout_period_us, uint8_t vcsel_period_pclks);
};
VL53L0X_VL53L0X(void)
    : address(0b0101001),
      io_timeout(0)
      ,
      did_timeout(false) {}
void VL53L0X_setAddress(uint8_t new_addr) {
  VL53L0X_writeReg(I2C_SLAVE_DEVICE_ADDRESS, new_addr & 0x7F);
  address = new_addr;
}
bool VL53L0X_init(bool io_2v8) {
  if (io_2v8) {
    VL53L0X_writeReg(VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV,
             VL53L0X_readReg(VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV) | 0x01);
  }
  VL53L0X_writeReg(0x88, 0x00);
  VL53L0X_writeReg(0x80, 0x01);
  VL53L0X_writeReg(0xFF, 0x01);
  VL53L0X_writeReg(0x00, 0x00);
  stop_variable = VL53L0X_readReg(0x91);
  VL53L0X_writeReg(0x00, 0x01);
  VL53L0X_writeReg(0xFF, 0x00);
  VL53L0X_writeReg(0x80, 0x00);
  VL53L0X_writeReg(MSRC_CONFIG_CONTROL, VL53L0X_readReg(MSRC_CONFIG_CONTROL) | 0x12);
  VL53L0X_setSignalRateLimit(0.25);
  VL53L0X_writeReg(SYSTEM_SEQUENCE_CONFIG, 0xFF);
  uint8_t spad_count;
  bool spad_type_is_aperture;
  if (!VL53L0X_getSpadInfo(&spad_count, &spad_type_is_aperture)) {
    return false;
  }
  uint8_t ref_spad_map[6];
  VL53L0X_readMulti(GLOBAL_CONFIG_SPAD_ENABLES_REF_0, ref_spad_map, 6);
  VL53L0X_writeReg(0xFF, 0x01);
  VL53L0X_writeReg(DYNAMIC_SPAD_REF_EN_START_OFFSET, 0x00);
  VL53L0X_writeReg(DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD, 0x2C);
  VL53L0X_writeReg(0xFF, 0x00);
  VL53L0X_writeReg(GLOBAL_CONFIG_REF_EN_START_SELECT, 0xB4);
  uint8_t first_spad_to_enable = spad_type_is_aperture ? 12 : 0;
  uint8_t spads_enabled = 0;
  for (uint8_t i = 0; i < 48; i++) {
    if (i < first_spad_to_enable || spads_enabled == spad_count) {
      ref_spad_map[i / 8] &= ~(1 << (i % 8));
    } else if ((ref_spad_map[i / 8] >> (i % 8)) & 0x1) {
      spads_enabled++;
    }
  }
  VL53L0X_writeMulti(GLOBAL_CONFIG_SPAD_ENABLES_REF_0, ref_spad_map, 6);
  VL53L0X_writeReg(0xFF, 0x01);
  VL53L0X_writeReg(0x00, 0x00);
  VL53L0X_writeReg(0xFF, 0x00);
  VL53L0X_writeReg(0x09, 0x00);
  VL53L0X_writeReg(0x10, 0x00);
  VL53L0X_writeReg(0x11, 0x00);
  VL53L0X_writeReg(0x24, 0x01);
  VL53L0X_writeReg(0x25, 0xFF);
  VL53L0X_writeReg(0x75, 0x00);
  VL53L0X_writeReg(0xFF, 0x01);
  VL53L0X_writeReg(0x4E, 0x2C);
  VL53L0X_writeReg(0x48, 0x00);
  VL53L0X_writeReg(0x30, 0x20);
  VL53L0X_writeReg(0xFF, 0x00);
  VL53L0X_writeReg(0x30, 0x09);
  VL53L0X_writeReg(0x54, 0x00);
  VL53L0X_writeReg(0x31, 0x04);
  VL53L0X_writeReg(0x32, 0x03);
  VL53L0X_writeReg(0x40, 0x83);
  VL53L0X_writeReg(0x46, 0x25);
  VL53L0X_writeReg(0x60, 0x00);
  VL53L0X_writeReg(0x27, 0x00);
  VL53L0X_writeReg(0x50, 0x06);
  VL53L0X_writeReg(0x51, 0x00);
  VL53L0X_writeReg(0x52, 0x96);
  VL53L0X_writeReg(0x56, 0x08);
  VL53L0X_writeReg(0x57, 0x30);
  VL53L0X_writeReg(0x61, 0x00);
  VL53L0X_writeReg(0x62, 0x00);
  VL53L0X_writeReg(0x64, 0x00);
  VL53L0X_writeReg(0x65, 0x00);
  VL53L0X_writeReg(0x66, 0xA0);
  VL53L0X_writeReg(0xFF, 0x01);
  VL53L0X_writeReg(0x22, 0x32);
  VL53L0X_writeReg(0x47, 0x14);
  VL53L0X_writeReg(0x49, 0xFF);
  VL53L0X_writeReg(0x4A, 0x00);
  VL53L0X_writeReg(0xFF, 0x00);
  VL53L0X_writeReg(0x7A, 0x0A);
  VL53L0X_writeReg(0x7B, 0x00);
  VL53L0X_writeReg(0x78, 0x21);
  VL53L0X_writeReg(0xFF, 0x01);
  VL53L0X_writeReg(0x23, 0x34);
  VL53L0X_writeReg(0x42, 0x00);
  VL53L0X_writeReg(0x44, 0xFF);
  VL53L0X_writeReg(0x45, 0x26);
  VL53L0X_writeReg(0x46, 0x05);
  VL53L0X_writeReg(0x40, 0x40);
  VL53L0X_writeReg(0x0E, 0x06);
  VL53L0X_writeReg(0x20, 0x1A);
  VL53L0X_writeReg(0x43, 0x40);
  VL53L0X_writeReg(0xFF, 0x00);
  VL53L0X_writeReg(0x34, 0x03);
  VL53L0X_writeReg(0x35, 0x44);
  VL53L0X_writeReg(0xFF, 0x01);
  VL53L0X_writeReg(0x31, 0x04);
  VL53L0X_writeReg(0x4B, 0x09);
  VL53L0X_writeReg(0x4C, 0x05);
  VL53L0X_writeReg(0x4D, 0x04);
  VL53L0X_writeReg(0xFF, 0x00);
  VL53L0X_writeReg(0x44, 0x00);
  VL53L0X_writeReg(0x45, 0x20);
  VL53L0X_writeReg(0x47, 0x08);
  VL53L0X_writeReg(0x48, 0x28);
  VL53L0X_writeReg(0x67, 0x00);
  VL53L0X_writeReg(0x70, 0x04);
  VL53L0X_writeReg(0x71, 0x01);
  VL53L0X_writeReg(0x72, 0xFE);
  VL53L0X_writeReg(0x76, 0x00);
  VL53L0X_writeReg(0x77, 0x00);
  VL53L0X_writeReg(0xFF, 0x01);
  VL53L0X_writeReg(0x0D, 0x01);
  VL53L0X_writeReg(0xFF, 0x00);
  VL53L0X_writeReg(0x80, 0x01);
  VL53L0X_writeReg(0x01, 0xF8);
  VL53L0X_writeReg(0xFF, 0x01);
  VL53L0X_writeReg(0x8E, 0x01);
  VL53L0X_writeReg(0x00, 0x01);
  VL53L0X_writeReg(0xFF, 0x00);
  VL53L0X_writeReg(0x80, 0x00);
  VL53L0X_writeReg(SYSTEM_INTERRUPT_CONFIG_GPIO, 0x04);
  VL53L0X_writeReg(GPIO_HV_MUX_ACTIVE_HIGH,
           VL53L0X_readReg(GPIO_HV_MUX_ACTIVE_HIGH) & ~0x10);
  VL53L0X_writeReg(SYSTEM_INTERRUPT_CLEAR, 0x01);
  measurement_timing_budget_us = VL53L0X_getMeasurementTimingBudget();
  VL53L0X_writeReg(SYSTEM_SEQUENCE_CONFIG, 0xE8);
  VL53L0X_setMeasurementTimingBudget(measurement_timing_budget_us);
  VL53L0X_writeReg(SYSTEM_SEQUENCE_CONFIG, 0x01);
  if (!VL53L0X_performSingleRefCalibration(0x40)) {
    return false;
  }
  VL53L0X_writeReg(SYSTEM_SEQUENCE_CONFIG, 0x02);
  if (!VL53L0X_performSingleRefCalibration(0x00)) {
    return false;
  }
  VL53L0X_writeReg(SYSTEM_SEQUENCE_CONFIG, 0xE8);
  return true;
}
void VL53L0X_writeReg(uint8_t reg, uint8_t value) {
  beginTransmission(address);
  write(reg);
  write(value);
  last_status = endTransmission(true);
}
void VL53L0X_writeReg16Bit(uint8_t reg, uint16_t value) {
  beginTransmission(address);
  write(reg);
  write((value >> 8) & 0xFF);
  write(value & 0xFF);
  last_status = endTransmission(true);
}
void VL53L0X_writeReg32Bit(uint8_t reg, uint32_t value) {
  beginTransmission(address);
  write(reg);
  write((value >> 24) & 0xFF);
  write((value >> 16) & 0xFF);
  write((value >> 8) & 0xFF);
  write(value & 0xFF);
  last_status = endTransmission(true);
}
uint8_t VL53L0X_readReg(uint8_t reg) {
  uint8_t value;
  beginTransmission(address);
  write(reg);
  last_status = endTransmission(true);
  requestFrom(address, (uint8_t)1);
  value = read();
  return value;
}
uint16_t VL53L0X_readReg16Bit(uint8_t reg) {
  uint16_t value;
  beginTransmission(address);
  write(reg);
  last_status = endTransmission(true);
  requestFrom(address, (uint8_t)2);
  value = (uint16_t)read() << 8;
  value |= read();
  return value;
}
uint32_t VL53L0X_readReg32Bit(uint8_t reg) {
  uint32_t value;
  beginTransmission(address);
  write(reg);
  last_status = endTransmission(true);
  requestFrom(address, (uint8_t)4);
  value = (uint32_t)read() << 24;
  value |= (uint32_t)read() << 16;
  value |= (uint16_t)read() << 8;
  value |= read();
  return value;
}
void VL53L0X_writeMulti(uint8_t reg, uint8_t const* src, uint8_t count) {
  beginTransmission(address);
  write(reg);
  while (count-- > 0) {
    write(*(src++));
  }
  last_status = endTransmission(true);
}
void VL53L0X_readMulti(uint8_t reg, uint8_t* dst, uint8_t count) {
  beginTransmission(address);
  write(reg);
  last_status = endTransmission(true);
  requestFrom(address, count);
  while (count-- > 0) {
    *(dst++) = read();
  }
}
bool VL53L0X_setSignalRateLimit(float limit_Mcps) {
  if (limit_Mcps < 0 || limit_Mcps > 511.99) {
    return false;
  }
  VL53L0X_writeReg16Bit(FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT, limit_Mcps * (1 << 7));
  return true;
}
float VL53L0X_getSignalRateLimit(void) {
  return (float)VL53L0X_readReg16Bit(FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT) / (1 << 7);
}
bool VL53L0X_setMeasurementTimingBudget(uint32_t budget_us) {
  SequenceStepEnables enables;
  SequenceStepTimeouts timeouts;
  uint16_t const StartOverhead = 1320;
  uint16_t const EndOverhead = 960;
  uint16_t const MsrcOverhead = 660;
  uint16_t const TccOverhead = 590;
  uint16_t const DssOverhead = 690;
  uint16_t const PreRangeOverhead = 660;
  uint16_t const FinalRangeOverhead = 550;
  uint32_t const MinTimingBudget = 20000;
  if (budget_us < MinTimingBudget) {
    return false;
  }
  uint32_t used_budget_us = StartOverhead + EndOverhead;
  VL53L0X_getSequenceStepEnables(&enables);
  VL53L0X_getSequenceStepTimeouts(&enables, &timeouts);
  if (enables.tcc) {
    used_budget_us += (timeouts.msrc_dss_tcc_us + TccOverhead);
  }
  if (enables.dss) {
    used_budget_us += 2 * (timeouts.msrc_dss_tcc_us + DssOverhead);
  } else if (enables.msrc) {
    used_budget_us += (timeouts.msrc_dss_tcc_us + MsrcOverhead);
  }
  if (enables.pre_range) {
    used_budget_us += (timeouts.pre_range_us + PreRangeOverhead);
  }
  if (enables.final_range) {
    used_budget_us += FinalRangeOverhead;
    if (used_budget_us > budget_us) {
      return false;
    }
    uint32_t final_range_timeout_us = budget_us - used_budget_us;
    uint16_t final_range_timeout_mclks =
        VL53L0X_timeoutMicrosecondsToMclks(final_range_timeout_us, timeouts.final_range_vcsel_period_pclks);
    if (enables.pre_range) {
      final_range_timeout_mclks += timeouts.pre_range_mclks;
    }
    VL53L0X_writeReg16Bit(FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI, VL53L0X_encodeTimeout(final_range_timeout_mclks));
    measurement_timing_budget_us = budget_us;
  }
  return true;
}
uint32_t VL53L0X_getMeasurementTimingBudget(void) {
  SequenceStepEnables enables;
  SequenceStepTimeouts timeouts;
  uint16_t const StartOverhead = 1910;
  uint16_t const EndOverhead = 960;
  uint16_t const MsrcOverhead = 660;
  uint16_t const TccOverhead = 590;
  uint16_t const DssOverhead = 690;
  uint16_t const PreRangeOverhead = 660;
  uint16_t const FinalRangeOverhead = 550;
  uint32_t budget_us = StartOverhead + EndOverhead;
  VL53L0X_getSequenceStepEnables(&enables);
  VL53L0X_getSequenceStepTimeouts(&enables, &timeouts);
  if (enables.tcc) {
    budget_us += (timeouts.msrc_dss_tcc_us + TccOverhead);
  }
  if (enables.dss) {
    budget_us += 2 * (timeouts.msrc_dss_tcc_us + DssOverhead);
  } else if (enables.msrc) {
    budget_us += (timeouts.msrc_dss_tcc_us + MsrcOverhead);
  }
  if (enables.pre_range) {
    budget_us += (timeouts.pre_range_us + PreRangeOverhead);
  }
  if (enables.final_range) {
    budget_us += (timeouts.final_range_us + FinalRangeOverhead);
  }
  measurement_timing_budget_us = budget_us;
  return budget_us;
}
bool VL53L0X_setVcselPulsePeriod(vcselPeriodType type, uint8_t period_pclks) {
  uint8_t vcsel_period_reg = (((period_pclks) >> 1) - 1);
  SequenceStepEnables enables;
  SequenceStepTimeouts timeouts;
  VL53L0X_getSequenceStepEnables(&enables);
  VL53L0X_getSequenceStepTimeouts(&enables, &timeouts);
  if (type == VcselPeriodPreRange) {
    switch (period_pclks) {
      case 12:
        VL53L0X_writeReg(PRE_RANGE_CONFIG_VALID_PHASE_HIGH, 0x18);
        break;
      case 14:
        VL53L0X_writeReg(PRE_RANGE_CONFIG_VALID_PHASE_HIGH, 0x30);
        break;
      case 16:
        VL53L0X_writeReg(PRE_RANGE_CONFIG_VALID_PHASE_HIGH, 0x40);
        break;
      case 18:
        VL53L0X_writeReg(PRE_RANGE_CONFIG_VALID_PHASE_HIGH, 0x50);
        break;
      default:
        return false;
    }
    VL53L0X_writeReg(PRE_RANGE_CONFIG_VALID_PHASE_LOW, 0x08);
    VL53L0X_writeReg(PRE_RANGE_CONFIG_VCSEL_PERIOD, vcsel_period_reg);
    uint16_t new_pre_range_timeout_mclks = VL53L0X_timeoutMicrosecondsToMclks(timeouts.pre_range_us, period_pclks);
    VL53L0X_writeReg16Bit(PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI, VL53L0X_encodeTimeout(new_pre_range_timeout_mclks));
    uint16_t new_msrc_timeout_mclks = VL53L0X_timeoutMicrosecondsToMclks(timeouts.msrc_dss_tcc_us, period_pclks);
    VL53L0X_writeReg(MSRC_CONFIG_TIMEOUT_MACROP, (new_msrc_timeout_mclks > 256) ? 255 : (new_msrc_timeout_mclks - 1));
  } else if (type == VcselPeriodFinalRange) {
    switch (period_pclks) {
      case 8:
        VL53L0X_writeReg(FINAL_RANGE_CONFIG_VALID_PHASE_HIGH, 0x10);
        VL53L0X_writeReg(FINAL_RANGE_CONFIG_VALID_PHASE_LOW, 0x08);
        VL53L0X_writeReg(GLOBAL_CONFIG_VCSEL_WIDTH, 0x02);
        VL53L0X_writeReg(ALGO_PHASECAL_CONFIG_TIMEOUT, 0x0C);
        VL53L0X_writeReg(0xFF, 0x01);
        VL53L0X_writeReg(ALGO_PHASECAL_LIM, 0x30);
        VL53L0X_writeReg(0xFF, 0x00);
        break;
      case 10:
        VL53L0X_writeReg(FINAL_RANGE_CONFIG_VALID_PHASE_HIGH, 0x28);
        VL53L0X_writeReg(FINAL_RANGE_CONFIG_VALID_PHASE_LOW, 0x08);
        VL53L0X_writeReg(GLOBAL_CONFIG_VCSEL_WIDTH, 0x03);
        VL53L0X_writeReg(ALGO_PHASECAL_CONFIG_TIMEOUT, 0x09);
        VL53L0X_writeReg(0xFF, 0x01);
        VL53L0X_writeReg(ALGO_PHASECAL_LIM, 0x20);
        VL53L0X_writeReg(0xFF, 0x00);
        break;
      case 12:
        VL53L0X_writeReg(FINAL_RANGE_CONFIG_VALID_PHASE_HIGH, 0x38);
        VL53L0X_writeReg(FINAL_RANGE_CONFIG_VALID_PHASE_LOW, 0x08);
        VL53L0X_writeReg(GLOBAL_CONFIG_VCSEL_WIDTH, 0x03);
        VL53L0X_writeReg(ALGO_PHASECAL_CONFIG_TIMEOUT, 0x08);
        VL53L0X_writeReg(0xFF, 0x01);
        VL53L0X_writeReg(ALGO_PHASECAL_LIM, 0x20);
        VL53L0X_writeReg(0xFF, 0x00);
        break;
      case 14:
        VL53L0X_writeReg(FINAL_RANGE_CONFIG_VALID_PHASE_HIGH, 0x48);
        VL53L0X_writeReg(FINAL_RANGE_CONFIG_VALID_PHASE_LOW, 0x08);
        VL53L0X_writeReg(GLOBAL_CONFIG_VCSEL_WIDTH, 0x03);
        VL53L0X_writeReg(ALGO_PHASECAL_CONFIG_TIMEOUT, 0x07);
        VL53L0X_writeReg(0xFF, 0x01);
        VL53L0X_writeReg(ALGO_PHASECAL_LIM, 0x20);
        VL53L0X_writeReg(0xFF, 0x00);
        break;
      default:
        return false;
    }
    VL53L0X_writeReg(FINAL_RANGE_CONFIG_VCSEL_PERIOD, vcsel_period_reg);
    uint16_t new_final_range_timeout_mclks = VL53L0X_timeoutMicrosecondsToMclks(timeouts.final_range_us, period_pclks);
    if (enables.pre_range) {
      new_final_range_timeout_mclks += timeouts.pre_range_mclks;
    }
    VL53L0X_writeReg16Bit(FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI, VL53L0X_encodeTimeout(new_final_range_timeout_mclks));
  } else {
    return false;
  }
  VL53L0X_setMeasurementTimingBudget(measurement_timing_budget_us);
  uint8_t sequence_config = VL53L0X_readReg(SYSTEM_SEQUENCE_CONFIG);
  VL53L0X_writeReg(SYSTEM_SEQUENCE_CONFIG, 0x02);
  VL53L0X_performSingleRefCalibration(0x0);
  VL53L0X_writeReg(SYSTEM_SEQUENCE_CONFIG, sequence_config);
  return true;
}
uint8_t VL53L0X_getVcselPulsePeriod(vcselPeriodType type) {
  if (type == VcselPeriodPreRange) {
    return (((VL53L0X_readReg(PRE_RANGE_CONFIG_VCSEL_PERIOD)) + 1) << 1);
  } else if (type == VcselPeriodFinalRange) {
    return (((VL53L0X_readReg(FINAL_RANGE_CONFIG_VCSEL_PERIOD)) + 1) << 1);
  } else {
    return 255;
  }
}
void VL53L0X_startContinuous(uint32_t period_ms) {
  VL53L0X_writeReg(0x80, 0x01);
  VL53L0X_writeReg(0xFF, 0x01);
  VL53L0X_writeReg(0x00, 0x00);
  VL53L0X_writeReg(0x91, stop_variable);
  VL53L0X_writeReg(0x00, 0x01);
  VL53L0X_writeReg(0xFF, 0x00);
  VL53L0X_writeReg(0x80, 0x00);
  if (period_ms != 0) {
    uint16_t osc_calibrate_val = VL53L0X_readReg16Bit(OSC_CALIBRATE_VAL);
    if (osc_calibrate_val != 0) {
      period_ms *= osc_calibrate_val;
    }
    VL53L0X_writeReg32Bit(SYSTEM_INTERMEASUREMENT_PERIOD, period_ms);
    VL53L0X_writeReg(SYSRANGE_START, 0x04);
  } else {
    VL53L0X_writeReg(SYSRANGE_START, 0x02);
  }
}
void VL53L0X_stopContinuous(void) {
  VL53L0X_writeReg(SYSRANGE_START, 0x01);
  VL53L0X_writeReg(0xFF, 0x01);
  VL53L0X_writeReg(0x00, 0x00);
  VL53L0X_writeReg(0x91, 0x00);
  VL53L0X_writeReg(0x00, 0x01);
  VL53L0X_writeReg(0xFF, 0x00);
}
uint16_t VL53L0X_readRangeContinuousMillimeters(void) {
  (timeout_start_ms = millis());
  while ((VL53L0X_readReg(RESULT_INTERRUPT_STATUS) & 0x07) == 0) {
    if ((io_timeout > 0 && ((uint16_t)millis() - timeout_start_ms) > io_timeout)) {
      did_timeout = true;
      return 65535;
    }
  }
  uint16_t range = VL53L0X_readReg16Bit(RESULT_RANGE_STATUS + 10);
  VL53L0X_writeReg(SYSTEM_INTERRUPT_CLEAR, 0x01);
  return range;
}
uint16_t VL53L0X_readRangeSingleMillimeters(void) {
  VL53L0X_writeReg(0x80, 0x01);
  VL53L0X_writeReg(0xFF, 0x01);
  VL53L0X_writeReg(0x00, 0x00);
  VL53L0X_writeReg(0x91, stop_variable);
  VL53L0X_writeReg(0x00, 0x01);
  VL53L0X_writeReg(0xFF, 0x00);
  VL53L0X_writeReg(0x80, 0x00);
  VL53L0X_writeReg(SYSRANGE_START, 0x01);
  (timeout_start_ms = millis());
  while (VL53L0X_readReg(SYSRANGE_START) & 0x01) {
    if ((io_timeout > 0 && ((uint16_t)millis() - timeout_start_ms) > io_timeout)) {
      did_timeout = true;
      return 65535;
    }
  }
  return VL53L0X_readRangeContinuousMillimeters();
}
bool VL53L0X_timeoutOccurred() {
  bool tmp = did_timeout;
  did_timeout = false;
  return tmp;
}
bool VL53L0X_getSpadInfo(uint8_t* count, bool* type_is_aperture) {
  uint8_t tmp;
  VL53L0X_writeReg(0x80, 0x01);
  VL53L0X_writeReg(0xFF, 0x01);
  VL53L0X_writeReg(0x00, 0x00);
  VL53L0X_writeReg(0xFF, 0x06);
  VL53L0X_writeReg(0x83, VL53L0X_readReg(0x83) | 0x04);
  VL53L0X_writeReg(0xFF, 0x07);
  VL53L0X_writeReg(0x81, 0x01);
  VL53L0X_writeReg(0x80, 0x01);
  VL53L0X_writeReg(0x94, 0x6b);
  VL53L0X_writeReg(0x83, 0x00);
  (timeout_start_ms = millis());
  while (VL53L0X_readReg(0x83) == 0x00) {
    if ((io_timeout > 0 && ((uint16_t)millis() - timeout_start_ms) > io_timeout)) {
      return false;
    }
  }
  VL53L0X_writeReg(0x83, 0x01);
  tmp = VL53L0X_readReg(0x92);
  *count = tmp & 0x7f;
  *type_is_aperture = (tmp >> 7) & 0x01;
  VL53L0X_writeReg(0x81, 0x00);
  VL53L0X_writeReg(0xFF, 0x06);
  VL53L0X_writeReg(0x83, VL53L0X_readReg(0x83) & ~0x04);
  VL53L0X_writeReg(0xFF, 0x01);
  VL53L0X_writeReg(0x00, 0x01);
  VL53L0X_writeReg(0xFF, 0x00);
  VL53L0X_writeReg(0x80, 0x00);
  return true;
}
void VL53L0X_getSequenceStepEnables(SequenceStepEnables* enables) {
  uint8_t sequence_config = VL53L0X_readReg(SYSTEM_SEQUENCE_CONFIG);
  enables->tcc = (sequence_config >> 4) & 0x1;
  enables->dss = (sequence_config >> 3) & 0x1;
  enables->msrc = (sequence_config >> 2) & 0x1;
  enables->pre_range = (sequence_config >> 6) & 0x1;
  enables->final_range = (sequence_config >> 7) & 0x1;
}
void VL53L0X_getSequenceStepTimeouts(SequenceStepEnables const* enables, SequenceStepTimeouts* timeouts) {
  timeouts->pre_range_vcsel_period_pclks = VL53L0X_getVcselPulsePeriod(VcselPeriodPreRange);
  timeouts->msrc_dss_tcc_mclks = VL53L0X_readReg(MSRC_CONFIG_TIMEOUT_MACROP) + 1;
  timeouts->msrc_dss_tcc_us =
      VL53L0X_timeoutMclksToMicroseconds(timeouts->msrc_dss_tcc_mclks, timeouts->pre_range_vcsel_period_pclks);
  timeouts->pre_range_mclks = VL53L0X_decodeTimeout(VL53L0X_readReg16Bit(PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI));
  timeouts->pre_range_us =
      VL53L0X_timeoutMclksToMicroseconds(timeouts->pre_range_mclks, timeouts->pre_range_vcsel_period_pclks);
  timeouts->final_range_vcsel_period_pclks = VL53L0X_getVcselPulsePeriod(VcselPeriodFinalRange);
  timeouts->final_range_mclks = VL53L0X_decodeTimeout(VL53L0X_readReg16Bit(FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI));
  if (enables->pre_range) {
    timeouts->final_range_mclks -= timeouts->pre_range_mclks;
  }
  timeouts->final_range_us =
      VL53L0X_timeoutMclksToMicroseconds(timeouts->final_range_mclks, timeouts->final_range_vcsel_period_pclks);
}
uint16_t VL53L0X_decodeTimeout(uint16_t reg_val) {
  return (uint16_t)((reg_val & 0x00FF) << (uint16_t)((reg_val & 0xFF00) >> 8)) + 1;
}
uint16_t VL53L0X_encodeTimeout(uint16_t timeout_mclks) {
  uint32_t ls_byte = 0;
  uint16_t ms_byte = 0;
  if (timeout_mclks > 0) {
    ls_byte = timeout_mclks - 1;
    while ((ls_byte & 0xFFFFFF00) > 0) {
      ls_byte >>= 1;
      ms_byte++;
    }
    return (ms_byte << 8) | (ls_byte & 0xFF);
  } else {
    return 0;
  }
}
uint32_t VL53L0X_timeoutMclksToMicroseconds(uint16_t timeout_period_mclks, uint8_t vcsel_period_pclks) {
  uint32_t macro_period_ns = ((((uint32_t)2304 * (vcsel_period_pclks)*1655) + 500) / 1000);
  return ((timeout_period_mclks * macro_period_ns) + (macro_period_ns / 2)) / 1000;
}
uint32_t VL53L0X_timeoutMicrosecondsToMclks(uint32_t timeout_period_us, uint8_t vcsel_period_pclks) {
  uint32_t macro_period_ns = ((((uint32_t)2304 * (vcsel_period_pclks)*1655) + 500) / 1000);
  return (((timeout_period_us * 1000) + (macro_period_ns / 2)) / macro_period_ns);
}
bool VL53L0X_performSingleRefCalibration(uint8_t vhv_init_byte) {
  VL53L0X_writeReg(SYSRANGE_START,
           0x01 | vhv_init_byte);
  (timeout_start_ms = millis());
  while ((VL53L0X_readReg(RESULT_INTERRUPT_STATUS) & 0x07) == 0) {
    if ((io_timeout > 0 && ((uint16_t)millis() - timeout_start_ms) > io_timeout)) {
      return false;
    }
  }
  VL53L0X_writeReg(SYSTEM_INTERRUPT_CLEAR, 0x01);
  VL53L0X_writeReg(SYSRANGE_START, 0x00);
  return true;
}
