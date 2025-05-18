#include "storage.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/component.h"

namespace esphome {
namespace storage {

static const char *const TAG = "storage";

// uint8_t Storage::read() {
//    uint8_t data;
//    data = this->read_byte(this->current_offset_);
//    this->update_offset();
//    return data;
// }

// void Storage::write(uint8_t data) {
//    if(this->buffer_ != nullptr) {
//       this->buffer_[this->current_offset_] = data;
//    } else {
//       ESP_LOGW(TAG, "Buffer not available using direct access instead");
//       this->direct_write_byte(this->base_offset_+this->current_offset_,data);
//    }
//    this->update_offset();
// }

// uint8_t Storage::read_byte(uint32_t offset) {
//    uint8_t data;
//    if(this->buffer_ != nullptr) {
//       data = this->buffer_[offset];
//    }
//    else {
//       ESP_LOGW(TAG, "Buffer not available using direct access instead");
//       data = this->direct_read_byte(this->base_offset_+offset);
//    }
//    return data;
// }

// void Storage::write_byte(uint32_t offset, uint8_t data) {
//    if(this->buffer_ != nullptr) {
//       this->buffer_[offset] = data;
//    } else {
//       ESP_LOGW(TAG, "Buffer not available using direct access instead");
//       this->direct_write_byte(this->base_offset_+offset,data);
//    }
// }

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

// void Storage::update_offset() {
//    this->current_offset_++;
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
// }

}  // namespace storage
}  // namespace esphome