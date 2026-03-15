import esphome.codegen as cg
from esphome.components import audio, cx_audio, speaker
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["cx_audio"]
CODEOWNERS = ["@andreibodrov"]

CONF_CX_AUDIO_ID = "cx_audio_id"

cx_i2s_ns = cg.esphome_ns.namespace("cx_i2s")
CXI2SSpeaker = cx_i2s_ns.class_("CXI2SSpeaker", speaker.Speaker, cg.Component)
CXI2SSpeaker.header = "esphome/components/cx_audio/cx_i2s.h"


def _set_stream_limits(config):
    audio.set_stream_limits(
        min_bits_per_sample=16,
        max_bits_per_sample=16,
        min_channels=1,
        max_channels=2,
        min_sample_rate=16000,
        max_sample_rate=48000,
    )(config)
    return config


CONFIG_SCHEMA = cv.All(
    speaker.SPEAKER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(CXI2SSpeaker),
            cv.GenerateID(CONF_CX_AUDIO_ID): cv.use_id(cx_audio.CXAudio),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _set_stream_limits,
)


async def to_code(config):
    cg.add_global(cx_i2s_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await speaker.register_speaker(var, config)

    parent = await cg.get_variable(config[CONF_CX_AUDIO_ID])
    cg.add(var.set_cx_audio(parent))
