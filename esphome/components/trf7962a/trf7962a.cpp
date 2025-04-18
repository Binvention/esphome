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
  });
  this->tag_uid_[0] = 0;
  this->c_password_ = passwords_.end();
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
  // check if feild is on
  if (this->field_on_) {
    // check interrupts
    if (irq_pin_->digital_read()) {
      ESP_LOGVV(TAG, "IRQ Pin high");
      this->enable();
      this->write_byte(IRQ_STAT | READ);
      this->set_mode(read_mode_);  // switch to reading mode
      uint8_t irq = this->read_byte();
      this->read_byte();            // dummy read needed to clear irq
      this->set_mode(write_mode_);  // set back to write mode
      this->disable();
      if (irq) {
        ESP_LOGD(TAG, "IRQ received %0x", irq);
        if (irq & TRF7962A_IRQ_STAT::RX_COMPLETE) {
          uint8_t length = this->read_register(TRF7962A_REG::FIFO_STAT);
          if (length & 0x10) {
            ESP_LOGE(TAG, "Error FIFO overflow detected");
          }
          this->read_rx_bytes(length & 0x0f);
          uint8_t flags = this->rx_buff_.front();
          this->rx_buff_.pop_front();
          switch (transfer_status_) {
            case WAIT_RANDOM:
              if (this->rx_buff_.size() != 2) {
                ESP_LOGE(TAG, "only two items should be in the rx buffer but there are actually %0d items in buffer",
                         this->rx_buff_.size());
              } else {
                this->last_random_[0] = this->rx_buff_[0];
                this->last_random_[1] = this->rx_buff_[1];
                ESP_LOGD(TAG, "New random recieved %x", this->last_random_);
                if (is_searching_) {
                  // stop new searches while processing the current tag
                  this->cancel_interval(search_slix_);
                  this->cancel_interval(search_standard_);
                }
                if (!tag_uid_[0]) {
                  if (this->passwords_.empty()) {
                  } else {
                    c_password_ = passwords_.cbegin();
                    this->set_interval(try_password_, 20, [this]() {
                      if (c_password_ != passwords_.end()) {
                        if (last_random_[0]) {
                          this->ISO15693_unlock_privacy_slix_(*c_password_++);
                        } else {
                          this->ISO15693_get_random_slix_();
                        }
                      } else {
                        this->cancel_interval(try_password_);
                        this->is_searching_ = false;
                        ESP_LOGE(TAG, "No provided passwords worked");
                      }
                    });
                  }
                }
              }
              break;
            case WAIT_INVENTORY:
              if (this->rx_buff_.size() != 9) {
                ESP_LOGE(TAG, "only 9 items should be in the rx buffer but there are actually %0d items in buffer",
                         this->rx_buff_.size());
              } else {
                bool update = false;
                rx_buff_.pop_front();  // get rid of DSFID
                if (this->tag_uid_[0]) {
                  for (uint8_t i = 0; i < 8; i++) {
                    if (this->tag_uid_[i] != this->rx_buff_[7 - i]) {
                      // TODO: trigger tag removed events
                      update = true;
                      break;
                    }
                  }
                } else {
                  this->is_searching_ = false;
                  update = true;
                  // TODO: trigger tag added events
                }
                if (update) {
                  for (uint8_t i = 7; i > 7; i--) {
                    tag_uid_[i] = rx_buff_[7 - i];
                  }
                  ESP_LOGD(TAG, "Tag UID: %02X%02X%02X%02X%02X%02X%02X%02X", tag_uid_[7], tag_uid_[6], tag_uid_[5],
                           tag_uid_[4], tag_uid_[3], tag_uid_[2], tag_uid_[1], tag_uid_[0]);
                }
              }
              break;
            case WAIT_PASSWORD:
              if (!flags) {
                // no errors so password is successful
                this->cancel_interval(try_password_);
                ESP_LOGD(TAG, "Password successful");
                this->ISO15693_send_single_slot_inventory_();
              } else {
                // reset feild since tag won't respond until it is
                this->turn_field_off_();
                this->last_random_[0] = 0;
              }
              break;
            default:
              break;
          }
          this->rx_buff_.clear();
          transfer_status_ = TRANSFER_STATUS::NO_TRANSACTIONS;
        } else if (irq & TRF7962A_IRQ_STAT::FIFO_HIGH_OR_LOW) {
          uint8_t length = this->read_register(TRF7962A_REG::FIFO_STAT);
          if (length & 0x10) {
            ESP_LOGE(TAG, "Error FIFO overflow detected");
          }
          if (length & 0x40) {
            this->read_rx_bytes(length & 0x0f);
          }
        } else if (irq & NO_RESPONSE) {
          if (tag_uid_[0]) {
            tag_uid_[0] = 0;
          }
          if (transfer_status_ == TRANSFER_STATUS::WAIT_PASSWORD) {
            // reset field so tag will respond again
            this->turn_field_off_();
          }
          last_random_[0] = 0;
          ESP_LOGD(TAG, "No response received");
          transfer_status_ = TRANSFER_STATUS::NO_TRANSACTIONS;
        }
      }
    }
    if (tag_uid_[0]) {
    } else {
      if (!is_searching_) {
        // offset checks by a bit
        if (this->check_slix_ && passwords_.size() > 0) {
          this->set_timeout(100, [this]() {
            this->set_interval(search_slix_, 1000, [this]() { this->ISO15693_get_random_slix_(); });
          });
        }
        this->set_interval(search_standard_, 1000, [this]() { this->ISO15693_send_single_slot_inventory_(); });
        is_searching_ = true;
      }
    }
  } else {
    this->turn_field_on_();
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
  this->write_byte(reg | TRF7962A_TRANS_TYPE::READ);
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
  this->write_byte(TRF7962A_REG::FIFO_IO_REG | TRF7962A_TRANS_TYPE::READ | TRF7962A_TRANS_TYPE::CONTINUOUS);
  this->set_mode(read_mode_);
  for (uint8_t i = 0; i <= length; i++) {
    this->rx_buff_.push_back(read_byte());
    ESP_LOGVV(TAG, "byte received %02x", this->rx_buff_.back());
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
  ESP_LOGD(TAG, "field off");
}

void TRF7962A::turn_field_on_() {
  this->write_register(TRF7962A_REG::CHIP_STAT, 0X21);
  this->field_on_ = true;
  ESP_LOGD(TAG, "field on");
}

void TRF7962A::ISO15693_send_single_slot_inventory_() {
  this->enable();
  this->write_byte(TRF7962A_CMD::RESET_FIFO);
  this->write_byte(TRF7962A_CMD::TRANSMIT_CRC);
  this->write_byte(TRF7962A_REG::TX_LEN_B1 | TRF7962A_TRANS_TYPE::CONTINUOUS);
  this->write_byte(0x00);
  this->write_byte(0x30);
  this->write_byte(0x26);  // flags
  this->write_byte(0x01);  // inventory
  this->write_byte(0x00);  // mask length = 0 and no afi
  this->disable();
  this->transfer_status_ = TRANSFER_STATUS::WAIT_INVENTORY;
  ESP_LOGD(TAG, "send get random number to SLIXL");
}

void TRF7962A::ISO15693_get_random_slix_() {
  this->enable();
  this->write_byte(TRF7962A_CMD::RESET_FIFO);
  this->write_byte(TRF7962A_CMD::TRANSMIT_CRC);
  this->write_byte(TRF7962A_REG::TX_LEN_B1 | TRF7962A_TRANS_TYPE::CONTINUOUS);
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
}

void TRF7962A::ISO15693_unlock_privacy_slix_(const uint8_t password[4]) {
  this->enable();
  this->write_byte(TRF7962A_CMD::RESET_FIFO);
  this->write_byte(TRF7962A_CMD::TRANSMIT_CRC);
  this->write_byte(TRF7962A_REG::TX_LEN_B1 | TRF7962A_TRANS_TYPE::CONTINUOUS);
  this->write_byte(0x00);
  this->write_byte(0x80);
  this->write_byte(0x02);  // ISO15693_REQ_DATARATE_HIGH
  this->write_byte(0xb3);  // set password
  this->write_byte(0x04);  // NXP manufacturer
  this->write_byte(0x04);  // privacy password
  for (uint8_t i = 0; i < 4; i++) {
    this->write_byte(password[i] ^ last_random_[i & 1]);
  }
  this->disable();
  this->transfer_status_ = TRANSFER_STATUS::WAIT_PASSWORD;
  ESP_LOGD(TAG, "Sending Password");
}

void TRF7962A::ISO15693_read_single_block_(uint8_t blockId, uint8_t *blockData) {}

void TRF7962A::add_password(uint32_t password) {
  passwords_.push_back({0});
  passwords_.back()[0] = password & 0xFF;
  passwords_.back()[1] = (password >> 8) & 0xFF;
  passwords_.back()[2] = (password >> 16) & 0xFF;
  passwords_.back()[3] = (password >> 24) & 0xFF;
}

void TRF7962A::add_slix() { check_slix_ = true; }

void TRF7962A::add_standard() { check_standard_ = true; }

}  // namespace trf7962a
}  // namespace esphome
