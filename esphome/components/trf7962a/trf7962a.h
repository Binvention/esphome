#pragma once

#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"

#include <functional>

namespace esphome {
namespace trf7962a {

class TRF7962ATrigger : public Trigger<uint64_t> {};
enum ISO15693_RESULT : uint8_t {
  RESULT_NO_RESPONSE = 0x00,
  VALID_RESPONSE = 0x01,
  INVALID_RESPONSE = 0x02,

  INVENTORY_NO_RESPONSE = 0x10,
  INVENTORY_VALID_RESPONSE = 0x11,
  INVENTORY_INVALID_RESPONSE = 0x12,

  GET_RANDOM_NO_RESPONSE = 0x20,
  GET_RANDOM_VALID = 0x21,
  GET_RANDOM_INVALID = 0x22,

  SET_PASSWORD_NO_RESPONSE = 0x30,
  SET_PASSWORD_CORRECT = 0x31,
  SET_PASSWORD_INCORRECT = 0x32,

  READ_SINGLE_BLOCK_NO_RESPONSE = 0x40,
  READ_SINGLE_BLOCK_VALID_RESPONSE = 0x41,
  READ_SINGLE_BLOCK_INVALID_RESPONSE = 0x42,
};

enum TRANSFER_STATUS { NO_TRANSACTIONS = 0x00, WAIT_RANDOM = 0x01, WAIT_INVENTORY = 0x02, WAIT_PASSWORD = 0x03 };
// registers (defaults to write must set type to make it read/continuous)
enum TRF7962A_REG : uint8_t {
  // main control registers
  CHIP_STAT = 0x00,    // Chip Status Control
  ISO_CONTROL = 0x01,  // ISO protocol control
  // Protocol Settings
  TX_PULSE_LEN = 0x06,      // TX Pulse-Length Control
  RX_NO_RESP_WAIT = 0x07,   // RX no response wait time
  RX_WAIT_TIME = 0x08,      // RX Wait Time
  MOD_SYS_CLK_CTRL = 0x09,  // Modulator and SYS_CLK Control
  RX_SPECIAL_SET = 0x0A,    // RX Special Setting
  REG_IO_CTRL = 0x0B,       // Regulator and I/O Control
  // Status Registers
  IRQ_STAT = 0x0C,          // IRQ Status
  COL_POS_IRQ_MASK = 0x0D,  // Collision Position and Interrupt Mask Register
  COL_POS = 0x0E,           // Collision Position
  RSSI_LEV_OS_STAT = 0x0F,  // RSSI Levels and Oscillator Status
  // FIFO Registers
  TEST1 = 0x1A,       // test
  TEST2 = 0x1B,       // test
  FIFO_STAT = 0x1C,   // FIFO status
  TX_LEN_B1 = 0x1D,   // TX Length Byte 1
  TX_LEN_B2 = 0x1E,   // TX Length Byte 2
  FIFO_IO_REG = 0x1F  // FIFO I/O rEGISTER
};
// commands given over spi (command bit already included)
enum TRF7962A_CMD : uint8_t {
  IDLING = 0x80,
  SOFT_INIT = 0x83,
  RESET_FIFO = 0x8F,
  TRANSMIT_NO_CRC = 0x90,
  TRANSMIT_CRC = 0x91,
  TRANSMIT_DELAY_NO_CRC = 0x92,
  TRANSMIT_DELAY_CRC = 0x93,
  TRANSMT_NEXT_SLOT = 0x94,
  BLOCK_RECIEVER = 0x96,
  ENABLE_RECIEVER = 0x97,
  TEST_INTERNAL_RF = 0x98,
  TEST_EXTERNAL_RF = 0x99,
  RECEIVER_GAIN_ADJ = 0x9A
};
// type of register transaction (default (0x00) is write)
enum TRF7962A_TRANS_TYPE : uint8_t { IDLE = 0x00, READ = 0x40, CONTINUOUS = 0x20 };

enum TRF7962A_IRQ_STAT : uint8_t {
  NO_RESPONSE = 0x01,
  COLLISION_ERROR = 0x02,
  FRAMING_ERROR = 0x04,
  PARITY_ERROR = 0x08,
  CRC_ERROR = 0x10,
  FIFO_HIGH_OR_LOW = 0x20,
  RX_COMPLETE = 0x40,
  TX_COMPLETE = 0x80
};

// enum LOOP_STATUS {
//   IDLE,
//   INITIALIZE,
//   CONNECT_TAG,
//   TRY_PASSWORD,
//   WAIT_IRQ,
//   RESET
// };

enum class ISO15693_TAG_TYPE : uint8_t { ICODE_SLIX, STANDARD };

class ISO15693_TAG {
 public:
  ISO15693_TAG(ISO15693_TAG_TYPE type, bool has_password, uint32_t password = 0);
  ISO15693_TAG_TYPE type() const { return type_; }
  bool has_password() const { return has_password_; }
  uint32_t password() const { return password_; }
  ISO15693_TAG_TYPE type_;
  bool has_password_;
  uint32_t password_;
};

class TRF7962A : public Component,
                 public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                       spi::DATA_RATE_4MHZ> {
 public:
  void setup() override;
  void dump_config() override;
  void dump_registers();
  float get_setup_priority() const override { return setup_priority::DATA; }
  void loop() override;

  void set_irq_pin(GPIOPin *irq_pin) { this->irq_pin_ = irq_pin; }

  void register_ontag_trigger(TRF7962ATrigger *trig) { this->triggers_ontag_.push_back(trig); }
  void register_ontagremoved_trigger(TRF7962ATrigger *trig) { this->triggers_ontagremoved_.push_back(trig); }

  void send_command(TRF7962A_CMD command);
  uint8_t read_register(TRF7962A_REG reg);
  void write_register(TRF7962A_REG reg, uint8_t value);
  void read_rx_bytes(uint8_t length);

  bool is_tag_active();
  ISO15693_RESULT get_last_result();
  TRANSFER_STATUS get_last_transfer_status();

  void add_password(uint32_t password);
  void add_slix();
  void add_standard();

 protected:
  void turn_field_on_();
  void turn_field_off_();

  void ISO15693_send_single_slot_inventory_();
  void ISO15693_get_random_slix_();
  void ISO15693_unlock_privacy_slix_(const std::array<uint8_t, 4> password);
  void ISO15693_read_single_block_(uint8_t blockId, uint8_t *blockData);
  void search_tag();
  void wait_for_rx();
  void process_random();
  void process_uid();
  std::vector<std::array<uint8_t, 4>> passwords_;  //= { 0x7FFD6E5B, 0x0F0F0F0F, 0x00000000 };
  std::vector<std::array<uint8_t, 4>>::const_iterator c_password_;
  GPIOPin *irq_pin_{nullptr};

  std::vector<TRF7962ATrigger *> triggers_ontag_;
  std::vector<TRF7962ATrigger *> triggers_ontagremoved_;

  uint8_t tag_uid_[8];
  // TAG_EVENT tag_status_;
  bool field_on_;
  uint8_t last_random_[2];
  TRANSFER_STATUS transfer_status_;
  std::deque<uint8_t> rx_buff_;
  ISO15693_RESULT last_result_;
  // LOOP_STATUS loop_status_;
  const spi::SPIMode read_mode_ = spi::SPIMode::MODE1;
  const spi::SPIMode write_mode_ = spi::SPIMode::MODE0;
  bool check_slix_ = false;
  bool check_standard_ = false;
  bool is_searching_ = true;
  uint8_t last_irq_;
};

}  // namespace trf7962a
}  // namespace esphome
