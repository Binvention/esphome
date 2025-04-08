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
  this->irq_pin_->setup();
  this->send_command(TRF7962A_CMD::SOFT_INIT);
  this->send_command(TRF7962A_CMD::IDLING);
  set_timeout(50,[this](){
    this->send_command(TRF7962A_CMD::RESET_FIFO);
    this->write_register(TRF7962A_REG::ISO_CONTROL,0b10000010);
    this->write_register(TRF7962A_REG::COL_POS_IRQ_MASK,0b00111110);
    this->write_register(TRF7962A_REG::MOD_SYS_CLK_CTRL,0b00100001);
    this->write_register(TRF7962A_REG::TX_PULSE_LEN,0x80);
    this->write_register(TRF7962A_REG::CHIP_STAT,0b00100001);
  });
  this->transfer_status_ = TRANSFER_STATUS::NO_TRANSACTIONS;
  set_interval("get_random",1000,[this](){
    ISO15693_get_random_slixl_();
    ESP_LOGD(TAG,"Chip stat %02x",this->read_register(CHIP_STAT));
    ESP_LOGD(TAG,"FIFO STAT %02x",this->read_register(FIFO_STAT));
  });
}

void TRF7962A::dump_config() {
  ESP_LOGCONFIG(TAG, "TRF7962A:");
  LOG_PIN("  IRQ pin: ", this->irq_pin_);
}

void TRF7962A::loop() {
  //check interrupts
  uint8_t irq = read_register(TRF7962A_REG::IRQ_STAT);
  if(irq) {
    ESP_LOGD(TAG,"IRQ received"+irq);
    if(irq & TRF7962A_IRQ_STAT::RX_COMPLETE) {
      uint8_t length = read_register(TRF7962A_REG::FIFO_STAT);
      if (length & 0x10) {
        ESP_LOGE(TAG, "Error FIFO overflow detected");
      }
      read_rx_bytes(length&0x0f);
      transfer_status_ = TRANSFER_STATUS::RECEIVE_COMPLETE;
    }
    else if (irq & TRF7962A_IRQ_STAT::FIFO_HIGH_OR_LOW) {
      uint8_t length = read_register(TRF7962A_REG::FIFO_STAT);
      if (length & 0x10) {
        ESP_LOGE(TAG, "Error FIFO overflow detected");
      }
      if(length & 0x40) {
        read_rx_bytes (length &0x0f);
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
  enable();
  transfer_byte(command);
  disable();
}

uint8_t TRF7962A::read_register(TRF7962A_REG reg){
  uint8_t data;
  enable();
  transfer_byte(reg | TRF7962A_TRANS_TYPE::READ);
  transfer_byte(TRF7962A_TRANS_TYPE::IDLE);
  data = read_byte();
  disable();
  ESP_LOGVV(TAG, "read_register_(%d) -> %d", reg, data);
  return data;
}

void TRF7962A::write_register(TRF7962A_REG reg, uint8_t value){
  enable();
  transfer_byte(reg);
  transfer_byte(value);
  disable();
  ESP_LOGVV(TAG, "write_register_(%d,%d)",reg,value);
}

void TRF7962A::read_rx_bytes(uint8_t length) {
  enable();
  for(uint8_t i = 0; i <= length; i++) {
    transfer_byte(TRF7962A_REG::FIFO_IO_REG | TRF7962A_TRANS_TYPE::READ);
    rx_buff_.push_back(transfer_byte(TRF7962A_TRANS_TYPE::IDLE));
    ESP_LOGD(TAG,"byte received %02x",rx_buff_.back());
  }
  disable();
}


bool TRF7962A::is_tag_active(){
  return false;
}

ISO15693_RESULT TRF7962A::get_last_result(){
  return ISO15693_RESULT::RESULT_NO_RESPONSE;
}

TRANSFER_STATUS TRF7962A::get_last_transfer_status(){
  return transfer_status_;
}


void TRF7962A::turn_field_off_(){
  this->write_register(TRF7962A_REG::CHIP_STAT,0X01);
  field_on_ = false;
  ESP_LOGD(TAG, "field off");
}

void TRF7962A::turn_field_on_(){
  this->write_register(TRF7962A_REG::CHIP_STAT,0X21);
  field_on_ = true;
  ESP_LOGD(TAG, "field on");
}

ISO15693_RESULT TRF7962A::ISO15693_send_single_slot_inventory_(uint8_t* uid){
  return RESULT_NO_RESPONSE;
}

void TRF7962A::ISO15693_get_random_slixl_(){
  enable();
  transfer_byte(TRF7962A_CMD::RESET_FIFO); 
  transfer_byte(TRF7962A_CMD::TRANSMIT_CRC); 
  transfer_byte(TRF7962A_REG::TX_LEN_B1|TRF7962A_TRANS_TYPE::CONTINUOUS);
  transfer_byte(0x00);
  transfer_byte(0x30);
  transfer_byte(0x02);//ISO15693_REQ_DATARATE_HIGH
  transfer_byte(0xB2); //get random number
  transfer_byte(0x04); //NXP manufacturer 
  disable();
  transfer_status_ = TRANSFER_STATUS::RECEIVE_WAIT;
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
