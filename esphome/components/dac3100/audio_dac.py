from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
# from esphome.components import i2s_audio
from esphome.components.audio_dac import AudioDac
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODE

CODEOWNERS = ["@binvention"]
DEPENDENCIES = ["i2c"]

dac3100_ns = cg.esphome_ns.namespace("dac3100")
DAC3100 = dac3100_ns.class_("DAC3100", AudioDac, cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DAC3100),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x30))
)


async def dac3100_set_volume_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)


    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
