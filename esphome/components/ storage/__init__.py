import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.cpp_generator import MockObjClass

storage_ns = cg.esphome_ns.namespace("storage")
Storage = storage_ns.class_("Storage", cg.EntityBase)

STORAGE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Storage),
    }
)


def storage_schema(
    class_: MockObjClass = cv.UNDEFINED,
) -> cv.Schema:
    schema = {}

    if class_ is not cv.UNDEFINED:
        # Not cv.optional
        schema[cv.GenerateID()] = cv.declare_id(class_)

    return STORAGE_SCHEMA.extend(schema)
