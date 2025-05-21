#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include <vector>

namespace esphome {
namespace storage {

struct FileInfo {
  String path;
  size_t size;
  bool is_directory;

  FileInfo(String const &path, size_t size, bool is_directory);
};

class Storage : public EntityBase {
 public:
  virtual uint8_t direct_read_byte(uint32_t offset);
  virtual bool direct_write_byte(uint8_t data);
  virtual bool direct_append_byte(uint8_t data);
  virtual size_t direct_read_byte_array(uint32_t offset, uint8_t *data, uint32_t data_length);
  virtual bool direct_write_byte_array(uint8_t *data, uint32_t data_length);
  virtual bool direct_append_byte_array(uint8_t *data, uint32_t data_length);
  virtual void set_file(String file);
  virtual std::vector<FileInfo> list_directory(String path);
  virtual FileInfo get_file_info(String path);
  // uint8_t read();
  // void write(uint8_t data);
  // uint8_t read_byte (uint32_t offset);
  // void write_byte (uint32_t offset, uint8_t data);
  // void read_byte_array(uint32_t offset, uint8_t * data, uint32_t data_length);
  // void write_byte_array(uint32_t offset, uint8_t * data, uint32_t data_length);
  // void write_buffer();
  // void refresh_buffer(uint32_t offset = 0);
  // uint8_t & operator[] (size_t index);
  // uint32_t get_buffer_size();
  // void write_on_shutdown(bool value);

 protected:
  // void load_buffer (uint32_t offset, uint32_t buffer_offset, uint32_t length);
  // void write_buffer (uint32_t offset, uint32_t buffer_offset, uint32_t length);
  // void allocate_buffer(uint32_t buffer_size);
  // void update_offset();
  // uint8_t * buffer_;
  // uint32_t buffer_size_;
  // uint32_t buffer_offset_;
  // uint32_t current_offset_;
  // uint32_t base_offset_;
  // uint32_t max_offset_;
  // bool write_on_shutdown_;
};

}  // namespace storage
}  // namespace esphome
