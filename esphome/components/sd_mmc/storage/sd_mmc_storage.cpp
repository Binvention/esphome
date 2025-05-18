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

void sd_mmc_storage::direct_read_array(size_t offset, uint8_t *data, size_t data_length) {
  if (this->sd_ref_ != nullptr) {
    size_t read = this->sd_ref_->read_file_chunk(this->current_path_, offset, data, data_length);
  }
}

}  // namespace sd_mmc_storage
}  // namespace esphome
