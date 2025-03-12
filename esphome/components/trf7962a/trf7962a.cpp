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
  this->cs_->digital_write(false);
  this->irq_pin_->setup();
  this->send_command(TRF7962A_CMD::SOFT_INIT);
  this->send_command(TRF7962A_CMD::IDLING);
  this->send_command(TRF7962A_CMD::RESET_FIFO);
  this->write_register(TRF7962A_REG::ISO_CONTROL,0b10000010);
  this->write_register(TRF7962A_REG::COL_POS_IRQ_MASK,0b00111110);
  this->write_register(TRF7962A_REG::MOD_SYS_CLK_CTRL,0b00100001);
  this->write_register(TRF7962A_REG::TX_PULSE_LEN,0x80);
  this->write_register(TRF7962A_REG::CHIP_STAT,0b00100001);

}

void TRF7962A::dump_config() {
  ESP_LOGCONFIG(TAG, "TRF7962A:");
  LOG_PIN("  IRQ pin: ", this->irq_pin_);
}

void TRF7962A::loop() {

  ISO15693_RESULT result;

  switch (loop_status_)
  {
  case LOOP_STATUS::IDLE:
    this->check_for_tag();
    break;
  
  default:
    break;
  }

  if (tag_status_ == TAG_EVENT::TAG_PLACED) {
    for (uint8_t i=0; i<3; i++) {
      result = ISO15693_get_random_slixl(NULL);
      if(result == ISO15693_RESULT::GET_RANDOM_VALID) {
        break;
      }
    }
    if(result != ISO15693_RESULT::GET_RANDOM_VALID) {
      tag_status_ = TAG_EVENT::TAG_REMOVED;

      //TODO handle tag event
      for (auto *trigger : this->triggers_ontagremoved_)
        trigger->process(this->tag_uid_);
    }
  }
  else {

  }
}

uint8_t TRF7962A::read_register(TRF7962A_REG reg){
  uint8_t data;
  enable();
  transfer_byte((uint8_t)reg | (uint8_t)TRF7962A_TRANS_TYPE::READ);
  data = read_byte();
  disable();
  ESP_LOGVV(TAG, "read_register_(%d) -> %d", reg, data);
  return data;
}

void TRF7962A::write_register(TRF7962A_REG reg, uint8_t value){
  enable();
  transfer_byte((uint8_t)reg);
  transfer_byte(value);
  disable();
  ESP_LOGVV(TAG, "write_register_(%d,%d)",reg,value);
}

void TRF7962A::send_command(TRF7962A_CMD command){
  enable();
  transfer_byte((uint8_t) command);
  disable();
}

void TRF7962A::send_raw(uint8_t* buffer, uint8_t length){

}

void TRF7962A::turn_field_off(){
  this->write_register(TRF7962A_REG::CHIP_STAT,0X01);
}

void TRF7962A::turn_field_on(){
  this->write_register(TRF7962A_REG::CHIP_STAT,0X21);
}

}  // namespace trf7962a
}  // namespace esphome
