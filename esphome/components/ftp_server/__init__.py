import esphome.codegen as cg
from esphome.components import storage, web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import coroutine_with_priority

CONF_URL_PREFIX = "url_prefix"
CONF_ROOT_PATH = "root_path"
CONF_ENABLE_DELETION = "enable_deletion"
CONF_ENABLE_DOWNLOAD = "enable_download"
CONF_ENABLE_UPLOAD = "enable_upload"

AUTO_LOAD = ["web_server_base"]
DEPENDENCIES = ["storage"]

file_server_ns = cg.esphome_ns.namespace("file_server")
FileServer = file_server_ns.class_("FileServer", cg.Component)
storage_ns = storage.storage_ns

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(FileServer),
            cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
                web_server_base.WebServerBase
            ),
            cv.Optional(CONF_ENABLE_DELETION, default=False): cv.boolean,
            cv.Optional(CONF_ENABLE_DOWNLOAD, default=False): cv.boolean,
            cv.Optional(CONF_ENABLE_UPLOAD, default=False): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
)


@coroutine_with_priority(45.0)
async def to_code(config):
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    var = cg.new_Pvariable(config[CONF_ID], paren)
    await cg.register_component(var, config)
    cg.add(var.set_deletion_enabled(config[CONF_ENABLE_DELETION]))
    cg.add(var.set_download_enabled(config[CONF_ENABLE_DOWNLOAD]))
    cg.add(var.set_upload_enabled(config[CONF_ENABLE_UPLOAD]))
