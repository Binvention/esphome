#pragma once

#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"

#include <functional>

namespace esphome {
namespace trf7962a {

class TRF7962ATrigger: public Trigger<std::string> {
  public:
   void process(std::vector<uint8_t> &data);
 };

enum class TAG_EVENT {
  TAG_NOP,
  TAG_PLACED,
  TAG_REMOVED
};
enum class ISO15693_RESULT : uint8_t {
  NO_RESPONSE = 0x00,
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

enum class TRANSFER_STATUS {
  IDLE = 0x00,
  TX_COMPLETE = 0x01,
  RX_COMPLETE = 0x02,
  TX_ERROR = 0x03,
  RX_WAIT = 0x04,
  RX_WAIT_EXTENSION = 0x05,
  TX_WAIT = 0x06,
  PROTOCOL_ERROR = 0x07,
  COLLISION_ERROR = 0x08,
  NO_RESPONSE_RECEIVED = 0x09,
  NO_RESPONSE_RECEIVED_15693 = 0x0A
};
// registers (defaults to write must set type to make it read/continuous)
enum class TRF7962A_REG : uint8_t {
  //main control registers
  CHIP_STAT = 0x00, // Chip Status Control
  ISO_CONTROL = 0x01, // ISO protocol control
  // Protocol Settings
  TX_PULSE_LEN = 0x06, //TX Pulse-Length Control
  RX_NO_RESP_WAIT = 0x07, //RX no response wait time
  RX_WAIT_TIME = 0x08, // RX Wait Time
  MOD_SYS_CLK_CTRL = 0x09, // Modulator and SYS_CLK Control
  RX_SPECIAL_SET = 0x0A, // RX Special Setting
  REG_IO_CTRL =  0x0B, //Regulator and I/O Control
  // Status Registers
  IRQ_STAT = 0x0C, //IRQ Status
  COL_POS_IRQ_MASK = 0x0D, // Collision Position and Interrupt Mask Register
  COL_POS = 0x0E, // Collision Position
  RSSI_LEV_OS_STAT = 0x0F, // RSSI Levels and Oscillator Status
  // FIFO rEGISTERS
  TEST1 = 0x1A, // test
  TEST2 = 0x1B, // test
  FIFO_STAT = 0x1C, //FIFO status
  TX_LEN_B1 = 0x1D, // TX Length Byte 1
  TX_LEN_B2 = 0x1E, // TX Length Byte 2
  FIFO_IO_REG = 0x1F // FIFO I/O rEGISTER
};
//commands given over spi (command bit already included)
enum class TRF7962A_CMD : uint8_t {
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
//type of register transaction (default (0x00) is write)
enum class TRF7962A_TRANS_TYPE: uint8_t {
  READ = 0x40,
  CONTINUOUS = 0x20
};

enum class TRF7962A_IRQ_STAT : uint8_t {
  IDLING = 0x00,
  NO_RESPONSE = 0x01,
  COLLISION_ERROR = 0x02,
  FRAMING_ERROR = 0x04,
  PARITY_ERROR = 0x08,
  CRC_ERROR = 0x10,
  FIFO_HIGH_OR_LOW = 0x20,
  RX_COMPLETE = 0x40,
  TX_COMPLETE = 0x80
};

enum class LOOP_STATUS {
  IDLE,
  INITIALIZE,
  CONNECT_TAG,
  TRY_PASSWORD,
  WAIT_IRQ,
  RESET
};

class TRF7962A : public Component, public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, 
                              spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_4MHZ>{
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void loop() override;

  void set_irq_pin(GPIOPin *irq_pin) { this->irq_pin_ = irq_pin; }

  void register_ontag_trigger(TRF7962ATrigger *trig) { this->triggers_ontag_.push_back(trig); }
  void register_ontagremoved_trigger(TRF7962ATrigger *trig) { this->triggers_ontagremoved_.push_back(trig); }

  uint8_t read_register(TRF7962A_REG reg);
  void write_register(TRF7962A_REG reg, uint8_t value);
  void send_command(TRF7962A_CMD cmd);
  void send_raw(uint8_t* buffer, uint8_t length);
  void send_raw_spi(uint8_t* buffer, uint8_t length, bool continuedSend);
  void read_register_cont(uint8_t* buffer, uint8_t length);
  void read_register_cont(uint8_t reg, uint8_t* buffer, uint8_t length);
  void read_register_cont(TRF7962A_REG reg, uint8_t* buffer, uint8_t length);

  bool is_tag_active();
  ISO15693_RESULT get_last_result();
  TRANSFER_STATUS get_last_transfer_status();

  uint8_t read_irq_register();
  void clear_irq_register();
 protected:
  void turn_field_on();
  void turn_field_off();
  TRANSFER_STATUS wait_rx_data(uint8_t tx_timeout, uint8_t rx_timeout);
  void wait_tx_irq(uint8_t tx_timeout);
  void wait_rx_irq(uint8_t rx_timeout);
  void timeout_irq();
  void check_for_tag();

  ISO15693_RESULT ISO15693_send_single_slot_inventory(uint8_t* uid);
  ISO15693_RESULT ISO15693_get_random_slixl(uint8_t* random);
  ISO15693_RESULT ISO15693_set_pass_slixl(uint8_t pass_id, uint32_t password);
  ISO15693_RESULT ISO15693_read_single_block(uint8_t blockId, uint8_t* blockData);

  TRANSFER_STATUS send_data_tag(uint8_t *send_buffer, uint8_t send_len);
  TRANSFER_STATUS send_data_tag(uint8_t *send_buffer, uint8_t send_len, uint8_t tx_timeout, uint8_t rx_timeout);



  uint32_t knownPasswords[3] = { 0x7FFD6E5B, 0x0F0F0F0F, 0x00000000 };
  GPIOPin *irq_pin_{nullptr};

  std::vector<TRF7962ATrigger *> triggers_ontag_;
  std::vector<TRF7962ATrigger *> triggers_ontagremoved_;

  TRANSFER_STATUS transfer_status;
  uint8_t transfer_buffer[50]; //may reduce size
  uint8_t transfer_offset;
  uint8_t transfer_rx_length;
  
  
  std::vector<uint8_t> tag_uid_;
  TAG_EVENT tag_status_;
  ISO15693_RESULT last_result_;
  LOOP_STATUS loop_status_;

};

}  // namespace trf7962a
}  // namespace esphome
