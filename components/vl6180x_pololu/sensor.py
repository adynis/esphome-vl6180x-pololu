import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID, CONF_ADDRESS, STATE_CLASS_MEASUREMENT, UNIT_MILLIMETER, UNIT_LUX,
    ICON_RULER, DEVICE_CLASS_DISTANCE, DEVICE_CLASS_ILLUMINANCE,
)
from . import vl6180x_pololu_ns

# Define the C++ classes for the Hub and the Sensors
VL6180XPololuHub = vl6180x_pololu_ns.class_("VL6180XPololuHub", cg.Component)
VL6180XDistanceSensor = vl6180x_pololu_ns.class_("VL6180XDistanceSensor", cg.PollingComponent, sensor.Sensor)
VL6180XALSSensor = vl6180x_pololu_ns.class_("VL6180XALSSensor", cg.PollingComponent, sensor.Sensor)

# Configuration keys for the YAML schema
CONF_DISTANCE = "distance"
CONF_DISTANCE_ERROR = "distance_error_code"
CONF_AMBIENT_LIGHT = "ambient_light"
CONF_ALS_ERROR = "ambient_light_error_code"
CONF_GAIN = "gain"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(VL6180XPololuHub),
    cv.Required("sda_pin"): cv.int_,
    cv.Required("scl_pin"): cv.int_,
    cv.Optional(CONF_ADDRESS, default=0x29): cv.i2c_address,
    
    # Range Sensor configuration
    cv.Optional(CONF_DISTANCE): sensor.sensor_schema(
        VL6180XDistanceSensor, unit_of_measurement=UNIT_MILLIMETER, icon=ICON_RULER, 
        accuracy_decimals=0, device_class=DEVICE_CLASS_DISTANCE, state_class=STATE_CLASS_MEASUREMENT
    ).extend(cv.polling_component_schema("100ms")),
    
    cv.Optional(CONF_DISTANCE_ERROR): sensor.sensor_schema(
        unit_of_measurement="", icon="mdi:alert-circle-outline", accuracy_decimals=0
    ),
    
    # Ambient Light Sensor configuration with adjustable Gain
    cv.Optional(CONF_AMBIENT_LIGHT): sensor.sensor_schema(
        VL6180XALSSensor, unit_of_measurement=UNIT_LUX, icon="mdi:brightness-5", 
        accuracy_decimals=1, device_class=DEVICE_CLASS_ILLUMINANCE, state_class=STATE_CLASS_MEASUREMENT
    ).extend(cv.polling_component_schema("5s")).extend({
        cv.Optional(CONF_GAIN, default=20): cv.int_,
    }),
    
    cv.Optional(CONF_ALS_ERROR): sensor.sensor_schema(
        unit_of_measurement="", icon="mdi:alert-circle-outline", accuracy_decimals=0
    ),
})

async def to_code(config):
    # Instantiate the Hub component
    hub = cg.new_Pvariable(config[cv.GenerateID()])
    await cg.register_component(hub, config)
    
    # Set base I2C parameters
    cg.add(hub.set_sda_pin(config["sda_pin"]))
    cg.add(hub.set_scl_pin(config["scl_pin"]))
    cg.add(hub.set_address(config[CONF_ADDRESS]))

    # Link error sensors if defined in YAML
    if CONF_DISTANCE_ERROR in config:
        cg.add(hub.set_distance_error_sensor(await sensor.new_sensor(config[CONF_DISTANCE_ERROR])))

    if CONF_ALS_ERROR in config:
        cg.add(hub.set_als_error_sensor(await sensor.new_sensor(config[CONF_ALS_ERROR])))

    # Initialize Distance sensor
    if CONF_DISTANCE in config:
        var = await sensor.new_sensor(config[CONF_DISTANCE])
        await cg.register_component(var, config[CONF_DISTANCE])
        cg.add(var.set_hub(hub))

    # Initialize ALS sensor and pass the Gain value
    if CONF_AMBIENT_LIGHT in config:
        conf = config[CONF_AMBIENT_LIGHT]
        var = await sensor.new_sensor(conf)
        await cg.register_component(var, conf)
        cg.add(var.set_hub(hub))
        cg.add(hub.set_als_gain(conf[CONF_GAIN]))
    
    cg.add_library("pololu/VL6180X", "1.3.1")