from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import spi
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_IRQ_PIN,
    CONF_ON_TAG,
    CONF_ON_TAG_REMOVED,
    CONF_TRIGGER_ID,
)

AUTO_LOAD = ["binary_sensor"]
CODEOWNERS = ["@binvention"]
DEPENDENCIES = ["spi"]

trf7962a_ns = cg.esphome_ns.namespace("trf7962a")
TRF7962A = trf7962a_ns.class_("TRF7962A", cg.Component, spi.SPIDevice)
TRF7962ATrigger = trf7962a_ns.class_(
    "TRF7962ATrigger", automation.Trigger.template(cg.std_string)
)

ISO15693_tag_types = trf7962a_ns.enum("TAG_TYPES")
ISO15693_TAG_TYPES = {
    "STANDARD": ISO15693_tag_types.STANDARD,
    "ICODE_SLIX": ISO15693_tag_types.ICODE_SLIX,
}


CONF_TAG_TYPES = "tag_types"


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TRF7962A),
            cv.Optional(CONF_ON_TAG): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TRF7962ATrigger),
                }
            ),
            cv.Optional(CONF_ON_TAG_REMOVED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TRF7962ATrigger),
                }
            ),
            cv.Required(CONF_IRQ_PIN): pins.gpio_input_pin_schema,
            cv.Required(CONF_TAG_TYPES): cv.ensure_list(
                {
                    cv.Required("type"): cv.enum(ISO15693_TAG_TYPES),
                    cv.Optional("password"): cv.hex_int,
                }
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=False, default_mode=0))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_IRQ_PIN])
    cg.add(var.set_irq_pin(pin))
    for type in config[CONF_TAG_TYPES]:
        if type["type"] == ISO15693_tag_types.ICODE_SLIX:
            cg.add(var.add_slix())
            if type["password"]:
                cg.add(var.add_password(type["password"]))
        else:
            cg.add(var.add_standard())

    for conf in config.get(CONF_ON_TAG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_ontag_trigger(trigger))
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

    for conf in config.get(CONF_ON_TAG_REMOVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_ontagremoved_trigger(trigger))
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)
