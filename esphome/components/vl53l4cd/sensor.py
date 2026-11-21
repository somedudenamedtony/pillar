from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_DISTANCE,
    ICON_ARROW_EXPAND_VERTICAL,
    STATE_CLASS_MEASUREMENT,
)

DEPENDENCIES = ["i2c"]

CONF_TIMING_BUDGET = "timing_budget"
CONF_SAMPLES = "samples"
CONF_XSHUT_PIN = "xshut_pin"

vl53l4cd_ns = cg.esphome_ns.namespace("vl53l4cd")
VL53L4CDSensor = vl53l4cd_ns.class_(
    "VL53L4CDSensor", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        VL53L4CDSensor,
        unit_of_measurement="mm",
        icon=ICON_ARROW_EXPAND_VERTICAL,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_DISTANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            # Integration time per reading. Longer = less noise, more power.
            cv.Optional(CONF_TIMING_BUDGET, default="100ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=cv.TimePeriod(milliseconds=10),
                    max=cv.TimePeriod(milliseconds=200),
                ),
            ),
            # Valid readings collected per update; the median is published.
            cv.Optional(CONF_SAMPLES, default=9): cv.int_range(min=1, max=31),
            # Optional: sensor XSHUT pin, driven high to wake the sensor.
            cv.Optional(CONF_XSHUT_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x29))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_timing_budget_ms(config[CONF_TIMING_BUDGET].total_milliseconds))
    cg.add(var.set_samples(config[CONF_SAMPLES]))
    if CONF_XSHUT_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_XSHUT_PIN])
        cg.add(var.set_xshut_pin(pin))
