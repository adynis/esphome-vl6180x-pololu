#include "vl6180x_pololu.h"

namespace esphome {
namespace vl6180x_pololu {

static const char *TAG = "vl6180x_pololu";

void VL6180XPololuHub::setup() {
  ESP_LOGCONFIG(TAG, "Initializing VL6180X Hub at pins SDA:%d, SCL:%d", sda_pin_, scl_pin_);
  
  // Initialize I2C with provided pins
  Wire.begin(sda_pin_, scl_pin_);
  sensor.setBus(&Wire);
  sensor.setAddress(address_);
  
  // Basic library initialization (resets some internal state)
  sensor.init();
  
  // Mandatory identification check: Reg 0x000 must be 0xB4 for VL6180X
  if (sensor.readReg(0x000) == 0xB4) {
    sensor.configureDefault();
    sensor.setTimeout(500);
    
    // Convert human-readable Gain (1, 10, 20, 40) to VL6180X Gain Registers
    // Register 0x03F (SYSALS__ANALOGUE_GAIN) mapping:
    uint8_t gain_reg = 0x44; // Default 1.67x
    if (als_gain_ <= 1) gain_reg = 0x46;      // Gain 1.01x
    else if (als_gain_ <= 10) gain_reg = 0x43; // Gain 2.50x
    else if (als_gain_ <= 40) gain_reg = 0x41; // Gain 10.1x
    sensor.writeReg(VL6180X::SYSALS__ANALOGUE_GAIN, gain_reg);

    this->initialized_ = true;
    this->status_clear_warning(); // Clear any previous 'Hardware not found' warnings
    ESP_LOGI(TAG, "VL6180X successfully initialized at address 0x%02X", address_);
  } else {
    // If ID check fails, we assume wiring/pin issues
    ESP_LOGE(TAG, "HARDWARE FAILURE: VL6180X not found at 0x%02X! Check your wires.", address_);
    this->initialized_ = false;
    this->status_set_warning(); // Mark device with warning in ESPHome Dashboard
  }
}

void VL6180XPololuHub::dump_config() {
  ESP_LOGCONFIG(TAG, "VL6180X Hub Configuration:");
  ESP_LOGCONFIG(TAG, "  SDA Pin: %d", sda_pin_);
  ESP_LOGCONFIG(TAG, "  SCL Pin: %d", scl_pin_);
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", address_);
}

// --- RANGE (DISTANCE) UPDATE ---
void VL6180XDistanceSensor::update() {
  if (!hub_->initialized_) {
    // Custom Error 199: Hardware not available at boot
    if (hub_->distance_error_sensor_) hub_->distance_error_sensor_->publish_state(199);
    this->publish_state(NAN);
    ESP_LOGW(TAG, "Range update skipped: Sensor disconnected (Code 199)");
    return;
  }

  uint8_t range = hub_->sensor.readRangeSingleMillimeters();
  
  // Read Register 0x04D (RESULT__RANGE_STATUS)
  // Error codes are in bits [7:4]. We shift right by 4 to get the actual code (0-15).
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

// --- AMBIENT LIGHT (ALS) UPDATE ---
void VL6180XALSSensor::update() {
  if (!hub_->initialized_) {
    if (hub_->als_error_sensor_) hub_->als_error_sensor_->publish_state(199);
    this->publish_state(NAN);
    ESP_LOGW(TAG, "ALS update skipped: Sensor disconnected (Code 199)");
    return;
  }

  uint16_t lux = hub_->sensor.readAmbientSingle();
  
  // Read Register 0x04E (RESULT__ALS_STATUS)
  // Status flags (Done, Ready) are in bits [2:0]. 
  // Error codes are in bits [5:3].
  // Logic: Shift right by 3 to drop status bits, then AND with 0x07 (binary 111) to mask out higher bits.
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