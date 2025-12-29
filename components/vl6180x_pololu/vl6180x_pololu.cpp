#include "vl6180x_pololu.h"

namespace esphome {
namespace vl6180x_pololu {

static const char *TAG = "vl6180x_pololu";

void VL6180XPololuHub::setup() {
  ESP_LOGCONFIG(TAG, "Starting VL6180X on SDA:%d, SCL:%d", sda_pin_, scl_pin_);
  
  // Custom I2C start with user-defined pins
  Wire.begin(sda_pin_, scl_pin_);
  sensor.setBus(&Wire);
  sensor.setAddress(address_);
  
  // Library initialization (resets sensor internal state)
  sensor.init();
  
  // Mandatory identification check: Reg 0x000 (IDENTIFICATION__MODEL_ID) must be 0xB4
  if (sensor.readReg(0x000) == 0xB4) {
    sensor.configureDefault();
    sensor.setTimeout(500);
    
    // Convert Lux Gain (human readable) to VL6180X register values (Register 0x03F)
    uint8_t gain_reg = 0x44; // Default 1.67x
    if (als_gain_ <= 1) gain_reg = 0x46;      // Gain 1.01x
    else if (als_gain_ <= 10) gain_reg = 0x43; // Gain 2.50x
    else if (als_gain_ <= 40) gain_reg = 0x41; // Gain 10.1x
    sensor.writeReg(VL6180X::SYSALS__ANALOGUE_GAIN, gain_reg);

    this->initialized_ = true;
    this->status_clear_warning(); // Clear any "Hardware not found" status
    ESP_LOGI(TAG, "VL6180X successfully initialized");
  } else {
    ESP_LOGE(TAG, "HARDWARE FAILURE: VL6180X not found at 0x%02X! Check wiring.", address_);
    this->initialized_ = false;
    this->status_set_warning(); // Set warning state in ESPHome Dashboard
  }
}

void VL6180XPololuHub::dump_config() {
  ESP_LOGCONFIG(TAG, "VL6180X Hub Configuration:");
  ESP_LOGCONFIG(TAG, "  I2C Address: 0x%02X", address_);
}

// --- DISTANCE SENSOR LOGIC ---

void VL6180XDistanceSensor::setup() {
  if (hub_->initialized_) {
    // SYSRANGE__PART_TO_PART_RANGE_OFFSET (Register 0x024)
    // This shifts the "zero" point of the sensor hardware. 
    // It's essential for clones that report 0mm when an object is still 50mm away.
    hub_->sensor.writeReg(0x024, (uint8_t)offset_);
    ESP_LOGI(TAG, "Hardware Range Offset applied: %d mm", offset_);
  }
}

void VL6180XDistanceSensor::update() {
  if (!hub_->initialized_) {
    // Code 199: Custom error code for disconnected hardware at boot
    if (hub_->distance_error_sensor_) hub_->distance_error_sensor_->publish_state(199);
    this->publish_state(NAN);
    return;
  }

  uint8_t range = hub_->sensor.readRangeSingleMillimeters();
  
  // Status check: RESULT__RANGE_STATUS (Register 0x04D)
  // Error codes are stored in bits [7:4]. We shift right by 4 to get the code (0-15).
  uint8_t raw_status = hub_->sensor.readReg(0x04D);
  uint8_t error_code = raw_status >> 4;
  
  if (hub_->distance_error_sensor_) hub_->distance_error_sensor_->publish_state(error_code);
  
  // If error code is 0, measurement is valid
  if (error_code == 0) {
    this->publish_state((float)range);
  } else {
    this->publish_state(NAN);
  }
}

void VL6180XDistanceSensor::dump_config() { LOG_SENSOR("  ", "Distance Entity", this); }

// --- AMBIENT LIGHT SENSOR LOGIC ---

void VL6180XALSSensor::update() {
  if (!hub_->initialized_) {
    if (hub_->als_error_sensor_) hub_->als_error_sensor_->publish_state(199);
    this->publish_state(NAN);
    return;
  }

  uint16_t lux = hub_->sensor.readAmbientSingle();
  
  // Status check: RESULT__ALS_STATUS (Register 0x04E)
  // Error codes are in bits [5:3]. Status bits (Done/Ready) are [2:0].
  // Logic: Shift right by 3 to drop status bits, then AND with 0x07 (binary 111) to isolate the error.
  uint8_t raw_status = hub_->sensor.readReg(0x04E);
  uint8_t error_code = (raw_status >> 3) & 0x07;
  
  if (hub_->als_error_sensor_) hub_->als_error_sensor_->publish_state(error_code);
  
  if (hub_->sensor.timeoutOccurred()) {
    this->publish_state(NAN);
  } else {
    this->publish_state((float)lux);
  }
}

void VL6180XALSSensor::dump_config() { LOG_SENSOR("  ", "ALS Entity", this); }

} // namespace vl6180x_pololu
} // namespace esphome
