from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import i2c

# from esphome.components import i2s_audio
from esphome.components.audio_dac import AudioDac
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID, 
    CONF_MODE,
    CONF_IRQ_PIN)

CODEOWNERS = ["@binvention"]
DEPENDENCIES = ["i2c"]

CONF_RESET_PIN = "reset_pin"

dac3100_ns = cg.esphome_ns.namespace("dac3100")
DAC3100 = dac3100_ns.class_("DAC3100", AudioDac, cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DAC3100),
            cv.Required(CONF_RESET_PIN) : pins.gpio_output_pin_schema,
            cv.Required(CONF_IRQ_PIN) : pins.gpio_input_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x18))
)


async def dac3100_set_volume_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)


    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
    cg.add(var.set_irq_pin(pin))
    pin = await cg.gpio_pin_expression(config[CONF_IRQ_PIN])
    cg.add(var.set_reset_pin(pin))
