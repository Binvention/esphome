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
  ERROR_CHECK(this->write_byte(DAC3100_CLK_PLL1,0x01),"Clock Config Failed");
  // Power up NDAC and set to 2
  ERROR_CHECK(this->write_byte(DAC3100_NDAC, 0x82), "Set NDAC failed");
  // Power up MDAC and set to 2
  ERROR_CHECK(this->write_byte(DAC3100_MDAC, 0x82), "Set MDAC failed");
  //ERROR_CHECK(this->write_byte(DAC3100_CODEC_IF_1, 0x00), "Set CODEC_IF failed"); //defaults to i2s 16bit
  // Program the DAC processing block to be used - PRB_P1
  //ERROR_CHECK(this->write_byte(DAC3100_DAC_SIG_PROC, 0x01), "Set DAC_SIG_PROC failed"); //defaults to PRB_P1
  
  //setup dac datapath for left data through both dacs
  ERROR_CHECK(this->write_byte(DAC3100_DAC_DATA_SET,0x44), "Set DAC_DATA_SET failed");

  
  // *** Select Page 1 ***
  ERROR_CHECK(this->write_byte(DAC3100_PAGE_CTRL, 0x01), "Set page 1 failed");
  
  //route dac left to all other outputs
  // route dac to drivers
  ERROR_CHECK(this->write_byte(DAC3100_DAC_OUT_MIX, 0x88), "Set DAC_OUT_MIX failed");

  // 
  // ERROR_CHECK(this->write_byte(DAC3100_HP_POP_REM, 0X4E), "Set DAC3100_HP_POP_REM");//default should be fine
  // Unmute drivers
  ERROR_CHECK(this->write_byte(DAC3100_HPL_DRIVER, 0x06), "Set HPL_DRIVER failed");
  ERROR_CHECK(this->write_byte(DAC3100_HPR_DRIVER, 0x06), "Set HPR_DRIVER failed");
  ERROR_CHECK(this->write_byte(DAC3100_SPK_DRIVER, 0x04), "Set SPK_DRIVER failed");
  
  // Power up the drivers
  ERROR_CHECK(this->write_byte(DAC3100_HP_DRIVER, 0xc4), "Set HP_DRIVER failed");
  ERROR_CHECK(this->write_byte(DAC3100_SPK_AMP, 0x86), "Set SPK_AMP failed");
  // *** Select Page 0 *** 
  //power up the dac   
  ERROR_CHECK(this->write_byte(DAC3100_DAC_DATA_SET,0xd4), "Set DAC_DATA_SET failed");
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
