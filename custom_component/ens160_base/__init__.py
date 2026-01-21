import esphome.codegen as cg
from esphome.components import sensor, select, text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_COMPENSATION,
    CONF_ECO2,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_TEMPERATURE,
    CONF_TVOC,
    DEVICE_CLASS_AQI,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
    ICON_CHEMICAL_WEAPON,
    ICON_MOLECULE_CO2,
    ICON_RADIATOR,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_BILLION,
    UNIT_PARTS_PER_MILLION,
)

CODEOWNERS = ["@vincentscode", "@latonita"]

ens160_ns = cg.esphome_ns.namespace("ens160_base")
OperationMode = ens160_ns.enum("ENS160Component.OperationMode")

CONF_AQI = "aqi"
UNIT_INDEX = "index"
CONF_FAST_WARMUP_DEBUG = "fast_warmup_debug"
CONF_OPERATION_MODE = "operation_mode"
CONF_STATE = "state"

OPMODE_OPTIONS = {
    "Deep Sleep": OperationMode.OPMODE_DEEP_SLEEP,
    "Idle": OperationMode.OPMODE_IDLE,
    "Measure": OperationMode.OPMODE_STD,
}

CONFIG_SCHEMA_BASE = cv.Schema(
    {
        cv.Optional(CONF_ECO2): sensor.sensor_schema(
            unit_of_measurement=UNIT_PARTS_PER_MILLION,
            icon=ICON_MOLECULE_CO2,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_CARBON_DIOXIDE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TVOC): sensor.sensor_schema(
            unit_of_measurement=UNIT_PARTS_PER_BILLION,
            icon=ICON_RADIATOR,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_AQI): sensor.sensor_schema(
            icon=ICON_CHEMICAL_WEAPON,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_AQI,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_COMPENSATION): cv.Schema(
            {
                cv.Required(CONF_TEMPERATURE): cv.use_id(sensor.Sensor),
                cv.Required(CONF_HUMIDITY): cv.use_id(sensor.Sensor),
            }
        ),
        cv.Optional(CONF_FAST_WARMUP_DEBUG, default=False): cv.boolean,
        # TU PRIDANÉ class_=select.Select
        cv.Optional(CONF_OPERATION_MODE): select.select_schema(
            class_=select.Select,
            entity_category="config",
        ),
        cv.Optional(CONF_STATE): text_sensor.text_sensor_schema(),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code_base(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if eco2_config := config.get(CONF_ECO2):
        sens = await sensor.new_sensor(eco2_config)
        cg.add(var.set_co2(sens))
    if tvoc_config := config.get(CONF_TVOC):
        sens = await sensor.new_sensor(tvoc_config)
        cg.add(var.set_tvoc(sens))
    if aqi_config := config.get(CONF_AQI):
        sens = await sensor.new_sensor(aqi_config)
        cg.add(var.set_aqi(sens))

    if compensation_config := config.get(CONF_COMPENSATION):
        sens = await cg.get_variable(compensation_config[CONF_TEMPERATURE])
        cg.add(var.set_temperature(sens))
        sens = await cg.get_variable(compensation_config[CONF_HUMIDITY])
        cg.add(var.set_humidity(sens))

    if config.get(CONF_FAST_WARMUP_DEBUG) is not None:
        cg.add(var.set_fast_warmup_debug(config[CONF_FAST_WARMUP_DEBUG]))

    if operation_select_cfg := config.get(CONF_OPERATION_MODE):
        sel = await select.new_select(operation_select_cfg, options=list(OPMODE_OPTIONS.keys()))
        cg.add(var.set_operation_mode_select(sel))

        # Asynchrónne nastavenie režimu cez scheduler – žiadna rekurzia, žiadny priamy I2C v callbacku
        lambda_code = r"""
            [c = {var}](std::string state, size_t) {{
              using Mode = esphome::ens160_base::ENS160Component::OperationMode;
              esphome::App.scheduler.set_timeout(c, "ens160_set_mode", 1, [c, state]() {{
                if (state == "Deep Sleep")      c->set_operation_mode_from_select(Mode::OPMODE_DEEP_SLEEP);
                else if (state == "Idle")       c->set_operation_mode_from_select(Mode::OPMODE_IDLE);
                else if (state == "Measure")    c->set_operation_mode_from_select(Mode::OPMODE_STD);
              }});
            }}
        """.format(var=var)
        cg.add(sel.add_on_state_callback(cg.RawExpression(lambda_code)))

    if state_ts_cfg := config.get(CONF_STATE):
        ts = await text_sensor.new_text_sensor(state_ts_cfg)
        cg.add(var.set_state_text_sensor(ts))

    return var