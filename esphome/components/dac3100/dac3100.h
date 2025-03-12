#pragma once

#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace dac3100 {

// DAC3100 Register Addresses
// Page 0
static const uint8_t DAC3100_PAGE_CTRL = 0x00;     // Register 0  - Page Control
static const uint8_t DAC3100_SW_RST = 0x01;        // Register 1  - Software Reset
static const uint8_t DAC3100_OT_FLAG = 0x03;       // Register 3  - Over Tempurature Flag (only valid if amp is powered up) only bit 1 is used
static const uint8_t DAC3100_CLK_PLL1 = 0x04;      // Register 4  - Clock Setting Register 1, Multiplexers
static const uint8_t DAC3100_CLK_PLL2 = 0x05;      // Register 5  - Clock Setting Register 2, P and R values
static const uint8_t DAC3100_CLK_PLL3 = 0x06;      // Register 6  - Clock Setting Register 3, J values
static const uint8_t DAC3100_CLK_PLL4 = 0x07;      // Register 7  - Clock Setting Register 4, D-Value MSB
static const uint8_t DAC3100_CLK_PLL5 = 0x08;      // Register 8  - Clock Setting Register 5, D-Value LSB
static const uint8_t DAC3100_NDAC = 0x0B;          // Register 11 - NDAC Divider Value
static const uint8_t DAC3100_MDAC = 0x0C;          // Register 12 - MDAC Divider Value
static const uint8_t DAC3100_DOSR_MSB = 0x0D;      // Register 13 - DOSR Divider Value (MSB 2 bits)
static const uint8_t DAC3100_DOSR = 0x0E;          // Register 14 - DOSR Divider Value (LS Byte)
static const uint8_t DAC3100_CLK_OUT_MUX = 0x19;   // Register 25 - Clock output mux
static const uint8_t DAC3100_CLK_M_DIV = 0x1A;   // Register 26 - Clock Output M Divider Value
static const uint8_t DAC3100_CODEC_IF_1 = 0x1B;    // Register 27 - CODEC Interface Control 1
static const uint8_t DAC3100_DATA_SLOT_OFF = 0x1C; // Register 28 - Data-Slot Offset Programmability
static const uint8_t DAC3100_CODEC_IF_2 = 0x1D;    // Register 29 - CODEC Interface Control 2
static const uint8_t DAC3100_BCLK_N_VAL = 0x1E;    // Register 30 - BCLK N Value
static const uint8_t DAC3100_CODEC_IF_3 = 0x1F;    // Register 31 - Codec Secondary Interface Control 1
static const uint8_t DAC3100_CODEC_IF_4 = 0x20;    // Register 32 - Codec Secondary Interface Control 2
static const uint8_t DAC3100_CODEC_IF_5 = 0x21;    // Register 33 - Codec Secondary Interface Control 3
static const uint8_t DAC3100_I2C_BUS_COND = 0x22;  // Register 34 - I2C Bus Condition
static const uint8_t DAC3100_DAC_FLAG_1 = 0x25;    // Register 37 - DAC Flag register
static const uint8_t DAC3100_DAC_FLAG_2 = 0x26;    // Register 38 - DAC Flag register
static const uint8_t DAC3100_OVERFLOW_FLAG = 0x27; // Register 37 - DAC Overflow Flags
static const uint8_t DAC3100_DAC_INT_FLAG_1 = 0x2C;// Register 44 - DAC Interrupt Flags
static const uint8_t DAC3100_DAC_INT_FLAG_2 = 0x2E;// Register 46 - DAC Interrupt Flags
static const uint8_t DAC3100_INT_CTRL_1 = 0x30;    // Register 48 - Interrupt control register 1
static const uint8_t DAC3100_INT_CTRL_2 = 0x31;    // Register 49 - Interrupt control register 2
static const uint8_t DAC3100_GPIO_1_CTRL = 0x33;   // Register 51 - GPIO 1 control register
static const uint8_t DAC3100_DIN_PIN_CTRL = 0x36;  // Register 54 - DIN input pin control register
static const uint8_t DAC3100_DAC_SIG_PROC = 0x3C;  // Register 60 - DAC Sig Processing Block Control
static const uint8_t DAC3100_DAC_DATA_SET = 0x3F;  // Register 63 - DAC Data Path Setup
static const uint8_t DAC3100_DAC_VOL_CTRL = 0x40;  // Register 64 - DAC Volume Control
static const uint8_t DAC3100_DACL_VOL_D = 0x41;    // Register 65 - DAC Left Digital Vol Control
static const uint8_t DAC3100_DACR_VOL_D = 0x42;    // Register 66 - DAC Right Digital Vol Control
static const uint8_t DAC3100_HEADSET_DETECT = 0x43;// Register 67 - Headset Detection
static const uint8_t DAC3100_DRC_CTRL_1 = 0x44;    // Register 68 - DRC Control 1
static const uint8_t DAC3100_DRC_CTRL_2 = 0x45;    // Register 69 - DRC Control 2
static const uint8_t DAC3100_DRC_CTRL_3 = 0x46;    // Register 70 - DRC Control 3
static const uint8_t DAC3100_LFT_BEEP_GEN = 0x47;  // Register 71 - Left Beep Generator
static const uint8_t DAC3100_RT_BEEP_GEN = 0x48;   // Register 72 - Right Beep Generator
static const uint8_t DAC3100_BEEP_LEN_MSB = 0x49;  // Register 73 - Beep Length MSB
static const uint8_t DAC3100_BEEP_LEN_MID = 0x4a;  // Register 74 - Beep Length Middle Bits
static const uint8_t DAC3100_BEEP_LEN_LSB = 0x4b;  // Register 75 - Beep Length LSB
static const uint8_t DAC3100_VOL_MIC_DET_CTRL = 0x74;   // Register 116 - Volume Control / Mic Detect pin control
static const uint8_t DAC3100_VOV_MIC_DET_GAIN = 0x75;  // Register 117 - Volume Control / Mic Detect pin gain

// Page 1
static const uint8_t DAC3100_HP_SPK_AMP_ERR_CTRL = 0x1e;       // Register 30  - Headphone/Speaker Amplifier Error Control
static const uint8_t DAC3100_HP_DRIVER = 0x1f;      // Register 31  - Headphone driver
static const uint8_t DAC3100_SPK_AMP = 0x20;     // Register 32  - Class D Speaker Amplifier Control
static const uint8_t DAC3100_HP_POP_REM = 0x21;     // Register 33  - Headphone pop removal settings
static const uint8_t DAC3100_PGA_RAMP_DOWN = 0x22;     // Register 34  - Output Driver PGA Ramp-Down Period Control
static const uint8_t DAC3100_DAC_OUT_MIX = 0x23;     // Register 35  - DAC_L and DAC_R Output Mixer Routing
static const uint8_t DAC3100_HPL_AN_VOL = 0x24;     // Register 36  - Left Analog Volume to Headphone Left
static const uint8_t DAC3100_HPR_AN_VOL = 0x25;     // Register 37  - Right Analog Volume to Headphone Right
static const uint8_t DAC3100_L_TO_SPK_AN_VOL = 0x26;     // Register 38  - Left Analog Volume to Class D Speaker Amp
static const uint8_t DAC3100_HPL_DRIVER = 0x28;     // Register 40  - Headphone Left Driver
static const uint8_t DAC3100_HPR_DRIVER = 0x29;     // Register 41  - Headphone Right Driver
static const uint8_t DAC3100_SPK_DRIVER = 0x2a;     // Register 42  - Class-D Speaker Driver
static const uint8_t DAC3100_HP_DRIVER_CTRL = 0x2c;     // Register 44  - Headphone Driver Control
static const uint8_t DAC3100_MIC_BIAS = 0x2e;     // Register 46  - Mic Bias
static const uint8_t DAC3100_IN_CM = 0x32;     // Register 50  - Input CM settings

//Page 3
static const uint8_t DAC3100_TIMER_MCLK_DIV = 0x10;     // Register 16  - Timer Clock MCLK Divider


//Pages 8 and 9 are used for DAC Coefficients see documentation for usage



class DAC3100 : public audio_dac::AudioDac, public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  bool set_mute_off() override;
  bool set_mute_on() override;
  bool set_volume(float volume) override;

  bool is_muted() override;
  float volume() override;

 protected:
  bool write_mute_();
  bool write_volume_();
  void config_dac_();
 
  int start_attempt_;
  uint8_t auto_mute_mode_{0};
  float volume_{0};
};

}  // namespace dac3100
}  // namespace esphome
