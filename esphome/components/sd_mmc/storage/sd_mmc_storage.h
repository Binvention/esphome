#pragma once
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/components/storage/storage.h"
#include "esphome/components/sd_mmc/sd_mmc.h"

namespace esphome {
namespace sd_mmc_storage {

class sd_mmc_storage : public storage::Storage, Component {
 public:
  uint8_t direct_read_byte(size_t offset);
  bool direct_write_byte(uint8_t data);
  bool direct_append_byte(uint8_t data);
  size_t direct_read_byte_array(size_t offset, uint8_t *data, size_t data_length);
  bool direct_write_byte_array(uint8_t *data, size_t data_length);
  bool direct_append_byte_array(uint8_t *data, size_t data_length);
  void direct_set_file(String file);
  std::vector<storage::FileInfo> direct_list_directory(String path);
  storage::FileInfo direct_get_file_info(String path);
  void set_sd_mmc(sd_mmc::SdMmc *value) { this->sd_ref_ = value; };

 protected:
  sd_mmc::SdMmc *sd_ref_;
  storage::FileInfo current_file_;
};

}  // namespace sd_mmc_storage
}  // namespace esphome