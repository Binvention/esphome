#include "sd_mmc_storage.h"

namespace esphome {
namespace sd_mmc_storage {

static const char *TAG = "SD_MMC_STORAGE";

uint8_t sd_mmc_storage::direct_read_byte(size_t offset) {
  if (this->sd_ref_ != nullptr) {
    uint8_t value;
    size_t read = this->sd_ref_->read_file_chunk(this->current_path_, offset, &value, (size_t) 1);
    if (read == 1) {
      return value;
    }
  }
  ESP_LOGE(TAG, "Unable to properly read value from sd card");
  return 0;
}

size_t sd_mmc_storage::direct_read_byte_array(size_t offset, uint8_t *data, size_t data_length) {
  if (this->sd_ref_ != nullptr) {
    return this->sd_ref_->read_file_chunk(this->current_path_, offset, data, data_length);
  }
  ESP_LOGE(TAG, "Unable to properly read value from sd card");
  return 0;
}

bool sd_mmc_storage::direct_write_byte(uint8_t data) {
  if (this->sd_ref_ != nullptr) {
    this->sd_ref_->write_file(this->current_path_.c_str(), &data, 1);
    return true;
  }
  return false;
}

bool sd_mmc_storage::direct_write_byte_array(uint8_t *data, size_t data_length) {
  if (this->sd_ref_ != nullptr) {
    this->sd_ref_->write_file(this->current_path_.c_str(), data, data_length);
    return true;
  }
  return false;
}

bool sd_mmc_storage::direct_append_byte(uint8_t data) {
  if (this->sd_ref_ != nullptr) {
    this->sd_ref_->append_file(this->current_path_.c_str(), &data, 1);
    return true;
  }
  return false;
}

bool sd_mmc_storage::direct_append_byte_array(uint8_t *data, size_t data_length) {
  if (this->sd_ref_ != nullptr) {
    this->sd_ref_->append_file(this->current_path_.c_str(), data, data_length);
    return true;
  }
  return false;
}

}  // namespace sd_mmc_storage
}  // namespace esphome
