// VL53L4CD driver for ESPHome.
//
// The init sequence, default register configuration and timing math are
// ported from Pololu's vl53l4cd-arduino library, which is based on ST's
// VL53L4CD ULD API (STSW-IMG026). Their license terms apply to this file:
//
//   Copyright (c) 2023 STMicroelectronics
//   Copyright (c) 2024 Pololu Corporation
//   All Rights Reserved
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted (subject to the limitations in the disclaimer
//   below) provided that the following conditions are met:
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//   * Neither the name of the copyright holder nor the names of its
//     contributors may be used to endorse or promote products derived from
//     this software without specific prior written permission.
//
//   NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
//   THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
//   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
//   NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
//   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "vl53l4cd.h"

#include <algorithm>
#include <cmath>

#include "esphome/core/log.h"

namespace esphome {
namespace vl53l4cd {

static const char *const TAG = "vl53l4cd";

// Registers (from the ULD API header).
static const uint16_t REG_VHV_CONFIG_TIMEOUT_MACROP_LOOP_BOUND = 0x0008;
static const uint16_t REG_OSC_FREQUENCY = 0x0006;
static const uint16_t REG_GPIO_TIO_HV_STATUS = 0x0031;
static const uint16_t REG_RANGE_CONFIG_A = 0x005E;
static const uint16_t REG_RANGE_CONFIG_B = 0x0061;
static const uint16_t REG_INTERMEASUREMENT_MS = 0x006C;
static const uint16_t REG_SYSTEM_INTERRUPT_CLEAR = 0x0086;
static const uint16_t REG_SYSTEM_START = 0x0087;
static const uint16_t REG_RESULT_RANGE_STATUS = 0x0089;
static const uint16_t REG_FIRMWARE_SYSTEM_STATUS = 0x00E5;
static const uint16_t REG_IDENTIFICATION_MODEL_ID = 0x010F;

static const uint16_t MODEL_ID = 0xEBAA;

// Default configuration for registers 0x2D..0x87 (ULD VL53L4CD_DEFAULT_CONFIGURATION).
// 0x2E/0x2F = 0x01: I2C and GPIO pulled up to AVDD (2V8), which matches the
// Pololu carrier's on-board 2.8 V regulator and level shifters.
static const uint8_t DEFAULT_CONFIGURATION[] = {
    0x00,  // 0x2d : I2C fast mode plus off
    0x01,  // 0x2e : I2C pulled up at AVDD
    0x01,  // 0x2f : GPIO pulled up at AVDD
    0x11,  // 0x30 : interrupt active low
    0x02,  // 0x31
    0x00, 0x02, 0x08, 0x00, 0x08, 0x10, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00,  // 0x32-0x3f
    0x0F, 0x00, 0x00, 0x00, 0x00, 0x00,                                                // 0x40-0x45
    0x20,                                                                              // 0x46 : new sample ready
    0x0b, 0x00, 0x00, 0x02, 0x14, 0x21, 0x00, 0x00, 0x05,                              // 0x47-0x4f
    0x00, 0x00, 0x00, 0x00, 0xc8, 0x00, 0x00, 0x38, 0xff, 0x01, 0x00, 0x08, 0x00, 0x00, 0x01,
    0xcc,                                                                              // 0x50-0x5f
    0x07, 0x01, 0xf1, 0x05,                                                            // 0x60-0x63
    0x00, 0xa0,                                                                        // 0x64-0x65 : sigma threshold
    0x00, 0x80,                                                                        // 0x66-0x67 : min count rate
    0x08, 0x38, 0x00, 0x00,                                                            // 0x68-0x6b
    0x00, 0x00, 0x0f, 0x89,                                                            // 0x6c-0x6f : inter-measurement
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                                                // 0x70-0x75
    0x00, 0x01, 0x07, 0x05, 0x06, 0x06, 0x00, 0x00, 0x02, 0xc7,                        // 0x76-0x7f
    0xff, 0x9B, 0x00, 0x00, 0x00, 0x01,                                                // 0x80-0x85
    0x00,                                                                              // 0x86 : clear interrupt
    0x00,                                                                              // 0x87 : ranging stopped
};
static_assert(sizeof(DEFAULT_CONFIGURATION) == 0x87 - 0x2D + 1, "VL53L4CD default config must cover 0x2D..0x87");

void VL53L4CDSensor::setup() {
  if (this->xshut_pin_ != nullptr) {
    this->xshut_pin_->setup();
    this->xshut_pin_->digital_write(true);
    delay(2);  // tBOOT is 1.2 ms max
  }
  if (!this->init_sensor_()) {
    ESP_LOGE(TAG, "Sensor init failed (I2C status %u)", this->last_status_);
    this->mark_failed();
    return;
  }
  this->readings_.reserve(this->samples_);
}

bool VL53L4CDSensor::init_sensor_() {
  uint16_t model_id = 0;
  if (!this->read_u16_(REG_IDENTIFICATION_MODEL_ID, &model_id)) {
    ESP_LOGE(TAG, "No response at address 0x%02X", this->address_);
    return false;
  }
  if (model_id != MODEL_ID) {
    ESP_LOGE(TAG, "Unexpected model ID 0x%04X (expected 0x%04X)", model_id, MODEL_ID);
    return false;
  }

  // Wait for the sensor firmware to boot.
  uint32_t start = millis();
  uint8_t status = 0;
  while (!this->read_u8_(REG_FIRMWARE_SYSTEM_STATUS, &status) || status != 0x03) {
    if (millis() - start > 500) {
      ESP_LOGE(TAG, "Timed out waiting for boot");
      return false;
    }
    delay(1);
  }

  // Load the default configuration in one block write.
  auto err = this->write_register16(0x002D, DEFAULT_CONFIGURATION, sizeof(DEFAULT_CONFIGURATION));
  if (err != i2c::ERROR_OK) {
    this->last_status_ = err;
    return false;
  }

  // Start VHV (one-time temperature calibration) and wait for it to finish.
  if (!this->write_u8_(REG_SYSTEM_START, 0x40))
    return false;
  start = millis();
  while (!this->data_ready_()) {
    if (millis() - start > 1000) {
      ESP_LOGE(TAG, "Timed out waiting for VHV calibration");
      return false;
    }
    delay(1);
  }
  this->write_u8_(REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
  this->write_u8_(REG_SYSTEM_START, 0x80);  // stop
  this->write_u8_(REG_VHV_CONFIG_TIMEOUT_MACROP_LOOP_BOUND, 0x09);
  this->write_u8_(0x000B, 0x00);
  this->write_u16_(0x0024, 0x0500);

  if (!this->set_range_timing_(this->timing_budget_ms_)) {
    ESP_LOGE(TAG, "Could not set timing budget");
    return false;
  }
  return true;
}

// Continuous mode (inter-measurement period 0), based on VL53L4CD_SetRangeTiming().
bool VL53L4CDSensor::set_range_timing_(uint32_t timing_budget_ms) {
  uint16_t osc_frequency = 0;
  if (!this->read_u16_(REG_OSC_FREQUENCY, &osc_frequency) || osc_frequency == 0)
    return false;

  uint32_t timing_budget_us = timing_budget_ms * 1000;
  uint32_t macro_period_us = ((uint32_t) 2304 * (0x40000000 / osc_frequency)) >> 6;

  if (!this->write_u32_(REG_INTERMEASUREMENT_MS, 0))
    return false;
  timing_budget_us -= 2500;
  timing_budget_us <<= 12;

  auto encode = [&](uint32_t factor) -> uint16_t {
    uint32_t tmp = (macro_period_us * factor) >> 6;
    uint32_t ls_byte = ((timing_budget_us + (tmp >> 1)) / tmp) - 1;
    uint8_t ms_byte = 0;
    while (ls_byte > 0xFF) {
      ls_byte >>= 1;
      ms_byte++;
    }
    return (uint16_t) ms_byte << 8 | (uint16_t) ls_byte;
  };

  return this->write_u16_(REG_RANGE_CONFIG_A, encode(16)) && this->write_u16_(REG_RANGE_CONFIG_B, encode(12));
}

bool VL53L4CDSensor::data_ready_() {
  uint8_t value = 0;
  if (!this->read_u8_(REG_GPIO_TIO_HV_STATUS, &value))
    return false;
  return (value & 0x01) == 0;  // interrupt is active low
}

void VL53L4CDSensor::update() {
  if (this->is_failed() || this->measuring_)
    return;
  this->readings_.clear();
  this->attempts_ = 0;
  this->discarded_first_ = false;
  this->write_u8_(REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
  if (!this->write_u8_(REG_SYSTEM_START, 0x21)) {  // start continuous ranging
    ESP_LOGW(TAG, "Could not start ranging");
    this->publish_state(NAN);
    return;
  }
  this->burst_start_ms_ = millis();
  this->measuring_ = true;
}

void VL53L4CDSensor::loop() {
  if (!this->measuring_)
    return;

  // Give up if the burst takes far longer than it should.
  const uint32_t limit_ms = (this->samples_ * 2u + 2u) * (this->timing_budget_ms_ + 10u) + 500u;
  if (millis() - this->burst_start_ms_ > limit_ms) {
    ESP_LOGW(TAG, "Burst timed out with %u valid readings", (unsigned) this->readings_.size());
    this->finish_burst_();
    return;
  }

  if (!this->data_ready_())
    return;

  // Read range status (0x89) through distance (0x97) in one go.
  uint8_t buf[15];
  auto err = this->read_register16(REG_RESULT_RANGE_STATUS, buf, sizeof(buf));
  this->write_u8_(REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
  if (err != i2c::ERROR_OK)
    return;

  // The first reading after starting is often off; skip it.
  if (!this->discarded_first_) {
    this->discarded_first_ = true;
    return;
  }

  static const uint8_t STATUS_RTN[24] = {255, 255, 255, 5, 2, 4, 1, 7, 3, 0, 255, 255,
                                         9, 13, 255, 255, 255, 255, 10, 6, 255, 255, 11, 12};
  uint8_t status = buf[0] & 0x1F;
  if (status < 24)
    status = STATUS_RTN[status];
  uint16_t range_mm = (uint16_t) buf[13] << 8 | buf[14];
  uint16_t signal_kcps = ((uint16_t) buf[5] << 8 | buf[6]) * 8;

  this->attempts_++;
  if (status == 0) {
    this->readings_.push_back(range_mm);
  } else {
    ESP_LOGV(TAG, "Rejected reading: status %u, %u mm, signal %u kcps", status, range_mm, signal_kcps);
    (void) signal_kcps;
  }

  if (this->readings_.size() >= this->samples_ || this->attempts_ >= this->samples_ * 2u)
    this->finish_burst_();
}

void VL53L4CDSensor::finish_burst_() {
  this->write_u8_(REG_SYSTEM_START, 0x80);  // stop ranging -> software standby
  this->measuring_ = false;

  if (this->readings_.empty()) {
    ESP_LOGW(TAG, "No valid readings (nothing in range or too little signal)");
    this->publish_state(NAN);
    return;
  }
  std::sort(this->readings_.begin(), this->readings_.end());
  size_t n = this->readings_.size();
  float median = (n % 2) ? this->readings_[n / 2] : (this->readings_[n / 2 - 1] + this->readings_[n / 2]) / 2.0f;
  ESP_LOGD(TAG, "Median of %u readings: %.0f mm (min %u, max %u)", (unsigned) n, median, this->readings_.front(),
           this->readings_.back());
  this->publish_state(median);
}

void VL53L4CDSensor::dump_config() {
  LOG_SENSOR("", "VL53L4CD", this);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_PIN("  XSHUT Pin: ", this->xshut_pin_);
  ESP_LOGCONFIG(TAG, "  Timing budget: %u ms", (unsigned) this->timing_budget_ms_);
  ESP_LOGCONFIG(TAG, "  Samples: %u", this->samples_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Communication with the sensor failed");
  }
}

bool VL53L4CDSensor::write_u8_(uint16_t reg, uint8_t value) {
  auto err = this->write_register16(reg, &value, 1);
  this->last_status_ = err;
  return err == i2c::ERROR_OK;
}

bool VL53L4CDSensor::write_u16_(uint16_t reg, uint16_t value) {
  uint8_t data[2] = {(uint8_t) (value >> 8), (uint8_t) value};
  auto err = this->write_register16(reg, data, 2);
  this->last_status_ = err;
  return err == i2c::ERROR_OK;
}

bool VL53L4CDSensor::write_u32_(uint16_t reg, uint32_t value) {
  uint8_t data[4] = {(uint8_t) (value >> 24), (uint8_t) (value >> 16), (uint8_t) (value >> 8), (uint8_t) value};
  auto err = this->write_register16(reg, data, 4);
  this->last_status_ = err;
  return err == i2c::ERROR_OK;
}

bool VL53L4CDSensor::read_u8_(uint16_t reg, uint8_t *value) {
  auto err = this->read_register16(reg, value, 1);
  this->last_status_ = err;
  return err == i2c::ERROR_OK;
}

bool VL53L4CDSensor::read_u16_(uint16_t reg, uint16_t *value) {
  uint8_t data[2];
  auto err = this->read_register16(reg, data, 2);
  this->last_status_ = err;
  if (err != i2c::ERROR_OK)
    return false;
  *value = (uint16_t) data[0] << 8 | data[1];
  return true;
}

}  // namespace vl53l4cd
}  // namespace esphome
