#include "storage.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/component.h"

namespace esphome {
namespace storage {

static const char *const TAG = "storage";
FileInfo::FileInfo(String const &path, size_t size, bool is_directory)
    : path(path), size(size), is_directory(is_directory) {
  this->read_offset = 0;
}

void Storage::set_file(String file) {
  this->current_file_ = this->get_file_info(file);
  if (this->current_file_.path == "") {
    this->current_file_.path = file;
  }
  this->current_file_.read_offset = 0;
  this->direct_set_file(file);
}

uint8_t Storage::read() {
  uint8_t data;
  data = this->direct_read_byte(this->current_file_.read_offset);
  this->update_offset(1);
  return data;
}

bool Storage::write(uint8_t data) {
  //    if(this->buffer_ != nullptr) {
  //       this->buffer_[this->current_offset_] = data;
  //    } else {
  ESP_LOGW(TAG, "Buffer not available using direct access instead");
  return this->direct_write_byte(data);
  //    }
}

bool Storage::append(uint8_t data) { return this->direct_append_byte(data); }

size_t Storage::read_array(uint8_t *data, size_t data_length) {
  size_t num_bytes_read = this->direct_read_byte_array(this->current_file_.read_offset, data, data_length);
  this->update_offset(num_bytes_read);
  return num_bytes_read;
}

bool Storage::write_array(uint8_t *data, size_t data_length) {
  return this->direct_write_byte_array(data, data_length);
}

bool Storage::append_array(uint8_t *data, size_t data_length) {
  return this->direct_write_byte_array(data, data_length);
}
// void Storage::allocate_buffer(uint32_t buffer_size) {
//    if(buffer_size) {
//       if(this->buffer_ == nullptr) {
//          RAMAllocator<uint8_t> allocator(RAMAllocator<uint8_t>::NONE);
//          this->buffer_ = allocator.allocate(buffer_size);
//       }
//       if(this->buffer_ == nullptr) {
//          ESP_LOGE(TAG, "Unable to Allocate memory for storage buffer");
//       }
//    }
// }

void Storage::update_offset(size_t value) {
  this->current_file_.read_offset += value;
  //    if (this->current_offset_ > this->buffer_size_){
  //       if(this->max_offset_ && (this->base_offset_ + this->current_offset_ > this->max_offset_)) {
  //          if(this->buffer_offset_ != this->base_offset_){
  //             this->load_buffer(base_offset_,0,this->buffer_size_);
  //          }
  //       } else {
  //          uint32_t new_buffer_offset = this->buffer_offset_+this->current_offset_;
  //          if(this->max_offset_ && (new_buffer_offset+this->buffer_size_-1 > this->max_offset_)) {
  //             this->load_buffer(this->buffer_offset_+this->current_offset_,0,this->max_offset_-new_buffer_offset+1);
  //          } else {
  //             this->load_buffer(this->buffer_offset_+this->current_offset_,0,this->buffer_size_);
  //          }
  //       }
  //       this->current_offset_ = 0;
  //    }
}

void Storage::set_read_offset(size_t offset) { this->current_file_.read_offset = offset; }

}  // namespace storage
}  // namespace esphome