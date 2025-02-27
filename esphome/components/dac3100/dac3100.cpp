#include "dac3100.h"

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dac3100 {

static const char *const TAG = "dac3100";

#define ERROR_CHECK(err, msg) \
  if (!(err)) { \
    ESP_LOGE(TAG, msg); \
    this->mark_failed(); \
    return; \
  }

void DAC3100::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DAC3100...");

  // Set register page to 0
  ERROR_CHECK(this->write_byte(DAC3100_PAGE_CTRL, 0x00), "Set page 0 failed");
  // Initiate SW reset (PLL is powered off as part of reset)
  ERROR_CHECK(this->write_byte(DAC3100_SW_RST, 0x01), "Software reset failed");
  // *** Program clock settings ***
  //sets the clock to the bclk
  ERROR_CHECK(this->write_byte(DAC3100_CLK_PLL1,0x07),"Clock Config Failed");
  ERROR_CHECK(this->write_byte(DAC3100_CLK_PLL3,0x20),"Clock Config Failed");
  ERROR_CHECK(this->write_byte(DAC3100_CLK_PLL4,0x0),"Clock Config Failed");
  ERROR_CHECK(this->write_byte(DAC3100_CLK_PLL5,0x0),"Clock Config Failed");
  ERROR_CHECK(this->write_byte(DAC3100_CLK_PLL2,0x96),"Clock Config Failed");

  // Power up NDAC and set to 2
  ERROR_CHECK(this->write_byte(DAC3100_NDAC, 0x84), "Set NDAC failed");
  // Power up MDAC and set to 2
  ERROR_CHECK(this->write_byte(DAC3100_MDAC, 0x86), "Set MDAC failed");

  ERROR_CHECK(this->write_byte(DAC3100_DOSR_MSB,0x01),"Clock Config Failed");
  ERROR_CHECK(this->write_byte(DAC3100_DOSR,0x00),"Clock Config Failed");

  ERROR_CHECK(this->write_byte(DAC3100_CODEC_IF_1, 0x00), "Set CODEC_IF failed"); //defaults to i2s 16bit
  // Program the DAC processing block to be used - PRB_P1
  ERROR_CHECK(this->write_byte(DAC3100_DAC_SIG_PROC, 0x19), "Set DAC_SIG_PROC failed"); //defaults to PRB_P1
  

  
  // *** Select Page 1 ***
  ERROR_CHECK(this->write_byte(DAC3100_PAGE_CTRL, 0x01), "Set page 1 failed");

  // 
  ERROR_CHECK(this->write_byte(DAC3100_HP_POP_REM, 0X4E), "Set DAC3100_HP_POP_REM failed");
  ERROR_CHECK(this->write_byte(DAC3100_PGA_RAMP_DOWN, 0X70), "Set DAC3100_HP_POP_REM failed");
  ERROR_CHECK(this->write_byte(DAC3100_DAC_OUT_MIX,0x44),"Set output mixing failed")
  ERROR_CHECK(this->write_byte(DAC3100_MIC_BIAS,0x0B),"Set mic bias failed")
  ERROR_CHECK(this->write_byte(DAC3100_HP_DRIVER_CTRL,0xE0),"Set output mixing failed")
  ERROR_CHECK(this->write_byte(DAC3100_L_TO_SPK_AN_VOL, 0x00), "Set driver volumefailed");
  ERROR_CHECK(this->write_byte(DAC3100_HPL_AN_VOL, 0x92), "Set driver volume failed");
  ERROR_CHECK(this->write_byte(DAC3100_HPR_AN_VOL, 0x92), "Set driver volume failed");
  ERROR_CHECK(this->write_byte(DAC3100_HPL_DRIVER, 0x06), "Set HPL_DRIVER failed");
  ERROR_CHECK(this->write_byte(DAC3100_HPR_DRIVER, 0x06), "Set HPR_DRIVER failed");
  ERROR_CHECK(this->write_byte(DAC3100_SPK_DRIVER, 0x04), "Set SPK_DRIVER failed");
  ERROR_CHECK(this->write_byte(DAC3100_SPK_AMP, 0x86), "Set HPL_DRIVER failed");
  ERROR_CHECK(this->write_byte(DAC3100_HP_DRIVER, 0xc4), "Set HP_DRIVER failed");
  

  // *** Select Page 3
  ERROR_CHECK(this->write_byte(DAC3100_PAGE_CTRL, 0x03), "Set page 3 failed");
  ERROR_CHECK(this->write_byte(DAC3100_TIMER_MCLK_DIV, 0x01), "Set timer mclk div failed");


  
  // // Power up the drivers
  // ERROR_CHECK(this->write_byte(DAC3100_SPK_AMP, 0x86), "Set SPK_AMP failed");
  // *** Select Page 0 *** 
  ERROR_CHECK(this->write_byte(DAC3100_PAGE_CTRL, 0x00), "Set page 1 failed");
  ERROR_CHECK(this->write_byte(DAC3100_HEADSET_DETECT, 0x8C), "Set headset detection failed");
  ERROR_CHECK(this->write_byte(DAC3100_INT_CTRL_1, 0x80), "Set interrupt control failed");
  ERROR_CHECK(this->write_byte(DAC3100_GPIO_1_CTRL, 0x14), "Set page 1 failed");



  //power up the dac   
  ERROR_CHECK(this->write_byte(DAC3100_DAC_DATA_SET,0xd8), "Set DAC_DATA_SET failed");
  // Set left and right DAC digital volume control
  ERROR_CHECK(this->write_volume_(), "Set volume failed");
  // Unmute left and right channels
  ERROR_CHECK(this->write_mute_(), "Set mute failed");
}

void DAC3100::dump_config() {
  ESP_LOGCONFIG(TAG, "DAC3100:");
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with DAC3100 failed");
  }
}

bool DAC3100::set_mute_off() {
  this->is_muted_ = false;
  return this->write_mute_();
}

bool DAC3100::set_mute_on() {
  this->is_muted_ = true;
  return this->write_mute_();
}

bool DAC3100::set_volume(float volume) {
  this->volume_ = clamp<float>(volume, 0.0, 1.0);
  return this->write_volume_();
}

bool DAC3100::is_muted() { return this->is_muted_; }

float DAC3100::volume() { return this->volume_; }

bool DAC3100::write_mute_() {
  uint8_t mute_mode_byte = 0x02;  // keep right volume linked with left

  mute_mode_byte |= this->is_muted_ ? 0x0c : 0x00;      // mute bits are 2-3
  if (!this->write_byte(DAC3100_PAGE_CTRL, 0x00) || !this->write_byte(DAC3100_DAC_VOL_CTRL, mute_mode_byte)) {
    ESP_LOGE(TAG, "Writing mute modes failed");
    return false;
  }
  return true;
}

bool DAC3100::write_volume_() {
  const int8_t dvc_min_byte = -127;
  const int8_t dvc_max_byte = 48;

  int8_t volume_byte = dvc_min_byte + (this->volume_ * (dvc_max_byte - dvc_min_byte));
  volume_byte = clamp<int8_t>(volume_byte, dvc_min_byte, dvc_max_byte);

  ESP_LOGVV(TAG, "Setting volume to 0x%.2x", volume_byte & 0xFF);

  if ((!this->write_byte(DAC3100_PAGE_CTRL, 0x00)) || (!this->write_byte(DAC3100_DACL_VOL_D, volume_byte)) ||
      (!this->write_byte(DAC3100_DACR_VOL_D, volume_byte))) {
    ESP_LOGE(TAG, "Writing volume failed");
    return false;
  }
  return true;
}

}  // namespace dac3100
}  // namespace esphome
