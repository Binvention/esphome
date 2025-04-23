#include <utility>

#include "automation.h"
#include "trf7962a.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace trf7962a {

static const char *const TAG = "trf7962a";

void TRF7962A::setup() {
  this->spi_setup();
  this->set_mode(write_mode_);  // default to writing mode
  this->irq_pin_->setup();
  this->transfer_status_ = TRANSFER_STATUS::NO_TRANSACTIONS;
  this->send_command(TRF7962A_CMD::SOFT_INIT);
  this->send_command(TRF7962A_CMD::IDLING);
  this->set_timeout(50, [this]() {
    this->send_command(TRF7962A_CMD::RESET_FIFO);
    this->write_register(TRF7962A_REG::ISO_CONTROL, 0b10000010);
    this->write_register(TRF7962A_REG::COL_POS_IRQ_MASK, 0b00111111);
    this->write_register(TRF7962A_REG::MOD_SYS_CLK_CTRL, 0b00100001);
    this->write_register(TRF7962A_REG::TX_PULSE_LEN, 0x80);
    this->write_register(TRF7962A_REG::CHIP_STAT, 0b00100001);
    this->set_timeout(75, [this]() {
      this->turn_field_on_();
      this->search_tag();
    });
  });
  this->tag_uid_[0] = 0;
  this->c_password_ = passwords_.cbegin();
}

void TRF7962A::dump_config() {
  ESP_LOGCONFIG(TAG, "TRF7962A:");
  LOG_PIN("  IRQ pin: ", this->irq_pin_);
}

void TRF7962A::dump_registers() {
  // main control registers
  ESP_LOGD(TAG, "  Chip stat: 0x%02X", this->read_register(CHIP_STAT));
  ESP_LOGD(TAG, "  ISO Control: 0x%02X", this->read_register(ISO_CONTROL));            // ISO protocol control
                                                                                       // Protocol Settings
  ESP_LOGD(TAG, "  TX Pulse Length: 0x%02X", this->read_register(TX_PULSE_LEN));       // TX Pulse-Length Control
  ESP_LOGD(TAG, "  RX_NO_RESP_WAIT: 0x%02X", this->read_register(RX_NO_RESP_WAIT));    // RX no response wait time
  ESP_LOGD(TAG, "  RX_WAIT_TIME: 0x%02X", this->read_register(RX_WAIT_TIME));          // RX Wait Time
  ESP_LOGD(TAG, "  MOD_SYS_CLK_CTRL: 0x%02X", this->read_register(MOD_SYS_CLK_CTRL));  // Modulator and SYS_CLK Control
  ESP_LOGD(TAG, "  RX_SPECIAL_SET: 0x%02X", this->read_register(RX_SPECIAL_SET));      // RX Special Setting
  ESP_LOGD(TAG, "  REG_IO_CTRL: 0x%02X", this->read_register(REG_IO_CTRL));            // Regulator and I/O Control
                                                                                       //  Status Registers
  // ESP_LOGD(TAG, "  IRQ_STAT: 0x%02X", this->read_register(IRQ_STAT)); //IRQ Status not read because it clears the
  // register
  ESP_LOGD(TAG, "  COL_POS_IRQ_MASK: 0x%02X",
           this->read_register(COL_POS_IRQ_MASK));                   // Collision Position and Interrupt Mask Register
  ESP_LOGD(TAG, "  COL_POS: 0x%02X", this->read_register(COL_POS));  // Collision Position
  ESP_LOGD(TAG, "  RSSI_LEV_OS_STAT: 0x%02X", this->read_register(RSSI_LEV_OS_STAT));  // RSSI Levels and Oscillator
                                                                                       // Status FIFO Registers
  ESP_LOGD(TAG, "  TEST1: 0x%02X", this->read_register(TEST1));                        // test
  ESP_LOGD(TAG, "  TEST2: 0x%02X", this->read_register(TEST2));                        // test
  ESP_LOGD(TAG, "  FIFO_STAT: 0x%02X", this->read_register(FIFO_STAT));                // FIFO status
  ESP_LOGD(TAG, "  TX_LEN_B1: 0x%02X", this->read_register(TX_LEN_B1));                // TX Length Byte 1
  ESP_LOGD(TAG, "  TX_LEN_B2: 0x%02X", this->read_register(TX_LEN_B2));                // TX Length Byte 2
}

void TRF7962A::loop() {
  if (irq_pin_->digital_read()) {
    this->enable();
    this->write_byte((uint8_t) IRQ_STAT | (uint8_t) READ);
    this->set_mode(read_mode_);  // switch to reading mode
    this->last_irq_ = this->read_byte();
    this->read_byte();            // dummy read needed to clear irq
    this->set_mode(write_mode_);  // set back to write mode
    this->disable();
    ESP_LOGV(TAG, "IRQ received %0x", this->last_irq_);
    if (last_irq_ & TRF7962A_IRQ_STAT::RX_COMPLETE) {
      uint8_t length = this->read_register(TRF7962A_REG::FIFO_STAT);
      if (length & 0x10) {
        ESP_LOGE(TAG, "Error FIFO overflow detected");
      }
      this->read_rx_bytes(length & 0x0f);
      this->send_command(RESET_FIFO);
    } else if (last_irq_ & TRF7962A_IRQ_STAT::FIFO_HIGH_OR_LOW) {
      uint8_t length = this->read_register(TRF7962A_REG::FIFO_STAT);
      if (length & 0x10) {
        ESP_LOGE(TAG, "Error FIFO overflow detected");
      }
      if (length & 0x40) {
        this->read_rx_bytes(length & 0x0f);
      }
    }
  }
}

void TRF7962A::send_command(TRF7962A_CMD command) {
  this->enable();
  this->write_byte(command);
  this->write_byte(0);
  this->disable();
}

uint8_t TRF7962A::read_register(TRF7962A_REG reg) {
  uint8_t data;
  this->enable();
  this->write_byte((uint8_t) reg | (uint8_t) TRF7962A_TRANS_TYPE::READ);
  this->set_mode(read_mode_);  // set to read mode
  data = this->read_byte();
  this->set_mode(write_mode_);  // set back to write mode
  this->disable();
  ESP_LOGVV(TAG, "read_register_(%x) -> %x", reg, data);
  return data;
}

void TRF7962A::write_register(TRF7962A_REG reg, uint8_t value) {
  this->enable();
  this->write_byte(reg);
  this->write_byte(value);
  this->disable();
  ESP_LOGVV(TAG, "write_register_(%d,%d)", reg, value);
}

void TRF7962A::read_rx_bytes(uint8_t length) {
  this->enable();
  this->write_byte((uint8_t) TRF7962A_REG::FIFO_IO_REG | (uint8_t) TRF7962A_TRANS_TYPE::READ |
                   (uint8_t) TRF7962A_TRANS_TYPE::CONTINUOUS);
  this->set_mode(read_mode_);
  for (uint8_t i = 0; i <= length; i++) {
    this->rx_buff_.push_back(read_byte());
    ESP_LOGD(TAG, "byte received %02x", this->rx_buff_.back());
  }
  this->set_mode(write_mode_);
  this->disable();
}

bool TRF7962A::is_tag_active() { return false; }

ISO15693_RESULT TRF7962A::get_last_result() { return ISO15693_RESULT::RESULT_NO_RESPONSE; }

TRANSFER_STATUS TRF7962A::get_last_transfer_status() { return this->transfer_status_; }

void TRF7962A::turn_field_off_() {
  this->write_register(TRF7962A_REG::CHIP_STAT, 0X01);
  this->field_on_ = false;
  ESP_LOGVV(TAG, "field off");
}

void TRF7962A::turn_field_on_() {
  this->write_register(TRF7962A_REG::CHIP_STAT, 0X21);
  this->field_on_ = true;
  ESP_LOGVV(TAG, "field on");
}

void TRF7962A::ISO15693_send_single_slot_inventory_() {
  this->rx_buff_.clear();
  this->enable();
  this->write_byte(TRF7962A_CMD::RESET_FIFO);
  this->write_byte(TRF7962A_CMD::TRANSMIT_CRC);
  this->write_byte((uint8_t) TRF7962A_REG::TX_LEN_B1 | (uint8_t) TRF7962A_TRANS_TYPE::CONTINUOUS);
  this->write_byte(0x00);
  this->write_byte(0x30);
  this->write_byte(0x26);  // flags
  this->write_byte(0x01);  // inventory
  this->write_byte(0x00);  // mask length = 0 and no afi
  this->disable();
  this->transfer_status_ = TRANSFER_STATUS::WAIT_INVENTORY;
  ESP_LOGD(TAG, "send inventory request");
  this->wait_for_rx();
}

void TRF7962A::ISO15693_get_random_slix_() {
  this->rx_buff_.clear();
  this->enable();
  this->write_byte(TRF7962A_CMD::RESET_FIFO);
  this->write_byte(TRF7962A_CMD::TRANSMIT_CRC);
  this->write_byte((uint8_t) TRF7962A_REG::TX_LEN_B1 | (uint8_t) TRF7962A_TRANS_TYPE::CONTINUOUS);
  this->write_byte(0x00);
  this->write_byte(tag_uid_[0] ? 0xb0 : 0x30);
  this->write_byte(0x02);  // ISO15693_REQ_DATARATE_HIGH
  this->write_byte(0xB2);  // get random number
  this->write_byte(0x04);  // NXP manufacturer
  // if a tag is present check for that tag specifically
  if (tag_uid_[0]) {
    this->write_array(&tag_uid_[0], 8);
  }
  this->disable();
  this->transfer_status_ = TRANSFER_STATUS::WAIT_RANDOM;
  ESP_LOGD(TAG, "send get random number to SLIXL");
  this->wait_for_rx();
}

void TRF7962A::ISO15693_unlock_privacy_slix_(const std::array<uint8_t, 4> password) {
  this->rx_buff_.clear();
  this->enable();
  this->write_byte(TRF7962A_CMD::RESET_FIFO);
  this->write_byte(TRF7962A_CMD::TRANSMIT_CRC);
  this->write_byte((uint8_t) TRF7962A_REG::TX_LEN_B1 | (uint8_t) TRF7962A_TRANS_TYPE::CONTINUOUS);
  this->write_byte(0x00);
  this->write_byte(0x80);
  this->write_byte(0x02);  // ISO15693_REQ_DATARATE_HIGH
  this->write_byte(0xb3);  // set password
  this->write_byte(0x04);  // NXP manufacturer
  this->write_byte(0x04);  // privacy password
  for (uint8_t i = 4; i > 4; i--) {
    this->write_byte(password[i] ^ last_random_[i & 1]);
  }
  this->disable();
  this->transfer_status_ = TRANSFER_STATUS::WAIT_PASSWORD;
  ESP_LOGD(TAG, "Sending Password");
  this->wait_for_rx();
}

void TRF7962A::ISO15693_read_single_block_(uint8_t blockId, uint8_t *blockData) {}

void TRF7962A::wait_for_rx() {
  this->set_retry("rx_wait", 50, 10, [this](const uint8_t attempts) {
    RetryResult result = RetryResult::RETRY;
    if (last_irq_ & TRF7962A_IRQ_STAT::RX_COMPLETE) {
      uint8_t flags = this->rx_buff_.front();
      this->rx_buff_.pop_front();
      switch (this->transfer_status_) {
        case WAIT_RANDOM:
          process_random();
          break;
        case WAIT_INVENTORY:
          process_uid();
          break;
        case WAIT_PASSWORD:
          if (!flags) {
            // no errors so password is successful
            ESP_LOGD(TAG, "Password successful!");
            this->ISO15693_send_single_slot_inventory_();
          } else {
            ESP_LOGD(TAG, "Password failed");
            // reset feild since tag won't respond until it is
            this->turn_field_off_();
            this->last_random_[0] = 0;
            this->set_timeout(50, [this]() {
              this->turn_field_on_();
              this->search_tag();
            });
          }
          break;
        default:
          ESP_LOGE(TAG, "ERROR invalid transaction status");
          this->turn_field_off_();
          this->last_random_[0] = 0;
          this->set_timeout(50, [this]() {
            this->turn_field_on_();
            this->search_tag();
          });
          break;
      }
      this->rx_buff_.clear();
      this->transfer_status_ = TRANSFER_STATUS::NO_TRANSACTIONS;
      result = RetryResult::DONE;
    } else if (attempts == 0) {
      if (tag_uid_[0]) {
        tag_uid_[0] = 0;
        for (auto *trigger : triggers_ontagremoved_) {
          trigger->trigger(0);
        }
      }
      if (last_random_[0]) {
        last_random_[0] = 0;
      }
      is_searching_ = true;
      c_password_ = passwords_.cbegin();
      this->transfer_status_ = NO_TRANSACTIONS;
      this->turn_field_off_();
      this->last_random_[0] = 0;
      this->rx_buff_.clear();
      this->set_timeout(50, [this]() {
        this->turn_field_on_();
        search_tag();
      });
    }
    return result;
  });
}

void TRF7962A::process_random() {
  if (this->rx_buff_.size() != 2) {
    ESP_LOGE(TAG, "only two items should be in the rx buffer but there are actually %0d items in buffer",
             this->rx_buff_.size());
    search_tag();
  } else {
    this->last_random_[1] = this->rx_buff_[0];
    this->last_random_[0] = this->rx_buff_[1];
    ESP_LOGD(TAG, "New random received %x%x", this->last_random_[1], this->last_random_[0]);
    if (!tag_uid_[0]) {
      if (this->passwords_.empty()) {
        ESP_LOGE(TAG, "no passwords provided");
        search_tag();
      } else {
        if (c_password_ != passwords_.end()) {
          this->ISO15693_unlock_privacy_slix_(*c_password_++);
        } else {
          ESP_LOGE(TAG, "No provided passwords worked");
          search_tag();
        }
      }
    } else {
      ESP_LOGE(TAG, "UID already found");
      search_tag();
    }
  }
}

void TRF7962A::process_uid() {
  if (this->rx_buff_.size() != 9) {
    ESP_LOGE(TAG, "only 9 items should be in the rx buffer but there are actually %0d items in buffer",
             this->rx_buff_.size());
    if (this->rx_buff_.size() > 0 && this->rx_buff_.size() < 20) {
      for (auto item : rx_buff_) {
        ESP_LOGVV(TAG, "Data %02x", item);
      }
    }
  } else {
    bool update = false;
    this->rx_buff_.pop_front();  // get rid of DSFID
    if (this->tag_uid_[0]) {
      for (uint8_t i = 0; i < 8; i++) {
        if (this->tag_uid_[i] != this->rx_buff_[i]) {
          for (auto *trigger : triggers_ontagremoved_) {
            trigger->trigger(0);
          }
          update = true;
          break;
        }
      }
    } else {
      this->is_searching_ = false;
      update = true;
    }
    if (update) {
      uint64_t result = 0;
      for (uint64_t offset = 0; offset < 8; offset++) {
        this->tag_uid_[offset] = this->rx_buff_[offset];
        uint64_t temp_uid_part = this->tag_uid_[offset];
        result |= temp_uid_part << (offset * 8);
        ESP_LOGD(TAG, "Tag UID DEBUG: %016x %02x %01d", result, temp_uid_part, offset);
      }
      for (auto *trigger : triggers_ontag_) {
        trigger->trigger(result);
      }
      ESP_LOGD(TAG, "Tag UID: %016x", result);
    }
  }
  search_tag();
}

void TRF7962A::search_tag() {
  static bool toggle_search_type = true;
  if (!this->field_on_) {
    this->turn_field_on_();
  }
  if (is_searching_) {
    if (toggle_search_type) {
      set_timeout(1000, [this]() { this->ISO15693_send_single_slot_inventory_(); });
    } else {
      set_timeout(1000, [this]() { this->ISO15693_get_random_slix_(); });
    }
    toggle_search_type = !toggle_search_type;
  } else {
    set_timeout(1000, [this]() { this->ISO15693_send_single_slot_inventory_(); });
  }
}

void TRF7962A::add_password(uint32_t password) {
  std::array<uint8_t, 4> password_bytes = {(password >> 0) & 0xFF, (password >> 8) & 0xFF, (password >> 16) & 0xFF,
                                           (password >> 24) & 0xFF};
  passwords_.push_back(password_bytes);
}

void TRF7962A::add_slix() { check_slix_ = true; }

void TRF7962A::add_standard() { check_standard_ = true; }

}  // namespace trf7962a
}  // namespace esphome
