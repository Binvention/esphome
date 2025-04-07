from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import spi
import esphome.config_validation as cv
from esphome.const import (
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
TRF7962ATrigger = trf7962a_ns.class_("TRF7962ATrigger", automation.Trigger.template(cg.std_string))




CONFIG_SCHEMA = cv.Schema(
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
        cv.Required(CONF_IRQ_PIN): pins.gpio_input_pin_schema
    }
).extend(cv.COMPONENT_SCHEMA).extend(spi.spi_device_schema(cs_pin_required=False))



async def setup_trf7962a(var, config):
    await cg.register_component(var, config)


    pin = await cg.gpio_pin_expression(config[CONF_IRQ_PIN])
    cg.add(var.set_irq_pin(pin))

    for conf in config.get(CONF_ON_TAG, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_ontag_trigger(trigger))
        await automation.build_automation(
            trigger, [(cg.std_string, "x")], conf
        )

    for conf in config.get(CONF_ON_TAG_REMOVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.register_ontagremoved_trigger(trigger))
        await automation.build_automation(
            trigger, [(cg.std_string, "x")], conf
        )


