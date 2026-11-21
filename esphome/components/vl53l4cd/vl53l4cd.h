#pragma once

#include <vector>

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace vl53l4cd {

// Minimal VL53L4CD driver for ESPHome, built for battery devices that take
// one burst of readings per wake. update() starts continuous ranging, loop()
// collects `samples` valid readings, then ranging stops (sensor drops to
// software standby) and the median distance in mm is published. If no valid
// reading arrives (nothing in range, too little signal) it publishes NAN.
class VL53L4CDSensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_timing_budget_ms(uint32_t ms) { this->timing_budget_ms_ = ms; }
  void set_samples(uint8_t samples) { this->samples_ = samples; }
  void set_xshut_pin(GPIOPin *pin) { this->xshut_pin_ = pin; }

  // True while a burst of readings is in progress.
  bool is_measuring() const { return this->measuring_; }

 protected:
  bool init_sensor_();
  bool set_range_timing_(uint32_t timing_budget_ms);
  bool data_ready_();
  void finish_burst_();

  bool write_u8_(uint16_t reg, uint8_t value);
  bool write_u16_(uint16_t reg, uint16_t value);
  bool write_u32_(uint16_t reg, uint32_t value);
  bool read_u8_(uint16_t reg, uint8_t *value);
  bool read_u16_(uint16_t reg, uint16_t *value);

  uint32_t timing_budget_ms_{100};
  uint8_t samples_{9};
  GPIOPin *xshut_pin_{nullptr};

  bool measuring_{false};
  uint32_t burst_start_ms_{0};
  uint16_t attempts_{0};
  bool discarded_first_{false};
  std::vector<uint16_t> readings_;
  uint8_t last_status_{0};
};

}  // namespace vl53l4cd
}  // namespace esphome
