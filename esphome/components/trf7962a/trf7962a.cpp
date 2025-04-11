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
  this->set_mode(write_mode_);//default to writing mode
  this->irq_pin_->setup();
  this->transfer_status_ = TRANSFER_STATUS::NO_TRANSACTIONS;
  this->send_command(TRF7962A_CMD::SOFT_INIT);
  this->send_command(TRF7962A_CMD::IDLING);
  this->set_timeout(50,[this](){
    this->send_command(TRF7962A_CMD::RESET_FIFO);
    this->write_register(TRF7962A_REG::ISO_CONTROL,0b10000010);
    this->write_register(TRF7962A_REG::COL_POS_IRQ_MASK,0b00111110);
    this->write_register(TRF7962A_REG::MOD_SYS_CLK_CTRL,0b00100001);
    this->write_register(TRF7962A_REG::TX_PULSE_LEN,0x80);
    this->write_register(TRF7962A_REG::CHIP_STAT,0b00100001);
  });
  this->set_interval("get_random",1000,[this](){
    this->ISO15693_get_random_slixl_();   
  });
}

void TRF7962A::dump_config() {
  ESP_LOGCONFIG(TAG, "TRF7962A:");
  LOG_PIN("  IRQ pin: ", this->irq_pin_);
}

void TRF7962A::dump_registers() {
  //main control registers
  ESP_LOGD(TAG, "  Chip stat: 0x%02X", this->read_register(CHIP_STAT));
  ESP_LOGD(TAG, "  ISO Control: 0x%02X", this->read_register(ISO_CONTROL)); // ISO protocol control
    // Protocol Settings
  ESP_LOGD(TAG, "  TX Pulse Length: 0x%02X", this->read_register(TX_PULSE_LEN)); //TX Pulse-Length Control
  ESP_LOGD(TAG, "  RX_NO_RESP_WAIT: 0x%02X", this->read_register(RX_NO_RESP_WAIT)); //RX no response wait time
  ESP_LOGD(TAG, "  RX_WAIT_TIME: 0x%02X", this->read_register(RX_WAIT_TIME)); // RX Wait Time
  ESP_LOGD(TAG, "  MOD_SYS_CLK_CTRL: 0x%02X", this->read_register(MOD_SYS_CLK_CTRL)); // Modulator and SYS_CLK Control
  ESP_LOGD(TAG, "  RX_SPECIAL_SET: 0x%02X", this->read_register(RX_SPECIAL_SET)); // RX Special Setting
  ESP_LOGD(TAG, "  REG_IO_CTRL: 0x%02X", this->read_register(REG_IO_CTRL)); //Regulator and I/O Control
    // Status Registers
  // ESP_LOGD(TAG, "  IRQ_STAT: 0x%02X", this->read_register(IRQ_STAT)); //IRQ Status not read because it clears the register
  ESP_LOGD(TAG, "  COL_POS_IRQ_MASK: 0x%02X", this->read_register(COL_POS_IRQ_MASK)); // Collision Position and Interrupt Mask Register
  ESP_LOGD(TAG, "  COL_POS: 0x%02X", this->read_register(COL_POS)); // Collision Position
  ESP_LOGD(TAG, "  RSSI_LEV_OS_STAT: 0x%02X", this->read_register(RSSI_LEV_OS_STAT)); // RSSI Levels and Oscillator Status
    // FIFO Registers
  ESP_LOGD(TAG, "  TEST1: 0x%02X", this->read_register(TEST1)); // test
  ESP_LOGD(TAG, "  TEST2: 0x%02X", this->read_register(TEST2)); // test
  ESP_LOGD(TAG, "  FIFO_STAT: 0x%02X", this->read_register(FIFO_STAT)); //FIFO status
  ESP_LOGD(TAG, "  TX_LEN_B1: 0x%02X", this->read_register(TX_LEN_B1)); // TX Length Byte 1
  ESP_LOGD(TAG, "  TX_LEN_B2: 0x%02X", this->read_register(TX_LEN_B2)); // TX Length Byte 2
}

void TRF7962A::loop() {
  //check interrupts
  if(irq_pin_->digital_read()){
    ESP_LOGD(TAG,"IRQ Pin high");
    this->enable();
    this->write_byte(IRQ_STAT|READ);
    this->set_mode(read_mode_); //switch to reading mode
    uint8_t irq = this->read_byte();
    this->read_byte(); //dummy read needed to clear irq
    this->set_mode(write_mode_);//set back to write mode
    this->disable();
    if(irq) {
      ESP_LOGD(TAG,"IRQ received"+irq);
      if(irq & TRF7962A_IRQ_STAT::RX_COMPLETE) {
        uint8_t length = this->read_register(TRF7962A_REG::FIFO_STAT);
        if (length & 0x10) {
          ESP_LOGE(TAG, "Error FIFO overflow detected");
        }
        this->read_rx_bytes(length&0x0f);
        transfer_status_ = TRANSFER_STATUS::RECEIVE_COMPLETE;
      }
      else if (irq & TRF7962A_IRQ_STAT::FIFO_HIGH_OR_LOW) {
        uint8_t length = this->read_register(TRF7962A_REG::FIFO_STAT);
        if (length & 0x10) {
          ESP_LOGE(TAG, "Error FIFO overflow detected");
        }
        if(length & 0x40) {
          this->read_rx_bytes (length &0x0f);
        }
      }
    }
  }
  //check if feild is on
  if(this->field_on_){
    // ISO15693_RESULT result;
    // result = ISO15693_get_random_slixl_(last_random_);

    // switch (loop_status_)
    // {
    // case LOOP_STATUS::IDLE:
    //   if(result == ISO15693_RESULT::GET_RANDOM_VALID) {
        
    //   }
    //   break;
    
    // default:
    //   break;
    // }

    // if (tag_status_ == TAG_EVENT::TAG_PLACED) {
    //   for (uint8_t i=0; i<3; i++) {
    //     if(result == ISO15693_RESULT::GET_RANDOM_VALID) {
    //       break;
    //     }
    //   }
    //   if(result != ISO15693_RESULT::GET_RANDOM_VALID) {
    //     tag_status_ = TAG_EVENT::TAG_REMOVED;

    //     //TODO handle tag event
    //     for (auto *trigger : this->triggers_ontagremoved_)
    //       trigger->process(this->tag_uid_);
    //   }
    // }
    // else {

    // }
  }
  else {
    this->turn_field_on_();
  }
}

void TRF7962A::send_command(TRF7962A_CMD command) {
  this->enable();
  this->write_byte(command);
  this->write_byte(0);
  this->disable();
}

uint8_t TRF7962A::read_register(TRF7962A_REG reg){
  uint8_t data;
  this->enable();
  this->write_byte(reg | TRF7962A_TRANS_TYPE::READ);
  this->set_mode(read_mode_); //set to read mode
  data = this->read_byte();
  this->set_mode(write_mode_); //set back to write mode
  this->disable();
  ESP_LOGD(TAG, "read_register_(%x) -> %d", reg, data);
  return data;
}

void TRF7962A::write_register(TRF7962A_REG reg, uint8_t value){
  this->enable();
  this->write_byte(reg);
  this->write_byte(value);
  this->disable();
  ESP_LOGVV(TAG, "write_register_(%d,%d)",reg,value);
}

void TRF7962A::read_rx_bytes(uint8_t length) {
  this->enable();
  this->write_byte(TRF7962A_REG::FIFO_IO_REG | TRF7962A_TRANS_TYPE::READ | TRF7962A_TRANS_TYPE::CONTINUOUS);
  this->set_mode(read_mode_);
  for(uint8_t i = 0; i <= length; i++) {
    this->rx_buff_.push_back(read_byte());
    ESP_LOGD(TAG,"byte received %02x",this->rx_buff_.back());
  }
  this->set_mode(write_mode_);
  this->disable();
}


bool TRF7962A::is_tag_active(){
  return false;
}

ISO15693_RESULT TRF7962A::get_last_result(){
  return ISO15693_RESULT::RESULT_NO_RESPONSE;
}

TRANSFER_STATUS TRF7962A::get_last_transfer_status(){
  return this->transfer_status_;
}


void TRF7962A::turn_field_off_(){
  this->write_register(TRF7962A_REG::CHIP_STAT,0X01);
  this->field_on_ = false;
  ESP_LOGD(TAG, "field off");
}

void TRF7962A::turn_field_on_(){
  this->write_register(TRF7962A_REG::CHIP_STAT,0X21);
  this->field_on_ = true;
  ESP_LOGD(TAG, "field on");
}

ISO15693_RESULT TRF7962A::ISO15693_send_single_slot_inventory_(uint8_t* uid){
  return RESULT_NO_RESPONSE;
}

void TRF7962A::ISO15693_get_random_slixl_(){
  this->enable();
  this->write_byte(TRF7962A_CMD::RESET_FIFO); 
  this->write_byte(TRF7962A_CMD::TRANSMIT_CRC); 
  this->write_byte(TRF7962A_REG::TX_LEN_B1|TRF7962A_TRANS_TYPE::CONTINUOUS);
  this->write_byte(0x00);
  this->write_byte(0x30);
  this->write_byte(0x02);//ISO15693_REQ_DATARATE_HIGH
  this->write_byte(0xB2); //get random number
  this->write_byte(0x04); //NXP manufacturer 
  this->disable();
  this->transfer_status_ = TRANSFER_STATUS::RECEIVE_WAIT;
  ESP_LOGD(TAG, "send get random number to SLIXL");
}

ISO15693_RESULT TRF7962A::ISO15693_set_pass_slixl_(uint8_t pass_id, uint32_t password){
  return RESULT_NO_RESPONSE;
}

ISO15693_RESULT TRF7962A::ISO15693_read_single_block_(uint8_t blockId, uint8_t* blockData){
  return RESULT_NO_RESPONSE;
}

}  // namespace trf7962a
}  // namespace esphome
