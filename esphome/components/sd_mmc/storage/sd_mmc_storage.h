#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/components/ storage/storage.h"
#include "esphome/components/sd_mmc/sd_mmc.h"

namespace esphome {
namespace sd_mmc_storage {

class sd_mmc_storage : public storage::Storage, Component {
 public:
  uint8_t direct_read_byte(uint32_t offset);
  void direct_write_byte(uint32_t offset, uint8_t data);
  void direct_read_array(uint32_t offset, uint8_t *data, uint32_t data_length);
  void direct_write_byte_array(uint32_t offset, uint8_t *data, uint32_t data_length);
  void set_file(String file);
  void create_file(String file, size_t max_size = 0);
  void set_sd_mmc(sd_mmc::SdMmc *value) { this->sd_ref_ = value; };

 protected:
  sd_mmc::SdMmc *sd_ref_;
  String current_path_;
};

}  // namespace sd_mmc_storage
}  // namespace esphome