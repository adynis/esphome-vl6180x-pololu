#pragma once
#include "esphome.h"
#include <VL6180X.h>
#include <Wire.h>

namespace esphome {
namespace vl6180x_pololu {

/**
 * VL6180XPololuHub: Manages shared I2C resources and sensor initialization.
 */
class VL6180XPololuHub : public Component {
 public:
  VL6180X sensor;
  bool initialized_{false};  // True if the sensor is found and correctly identified
  int sda_pin_, scl_pin_;
  uint8_t address_{0x29};
  int als_gain_{20};
  
  sensor::Sensor *distance_error_sensor_{nullptr};
  sensor::Sensor *als_error_sensor_{nullptr};

  void setup() override;
  void dump_config() override;

  // Configuration setters called by the Python to_code function
  void set_sda_pin(int pin) { sda_pin_ = pin; }
  void set_scl_pin(int pin) { scl_pin_ = pin; }
  void set_address(uint8_t address) { address_ = address; }
  void set_als_gain(int gain) { als_gain_ = gain; }
  void set_distance_error_sensor(sensor::Sensor *s) { distance_error_sensor_ = s; }
  void set_als_error_sensor(sensor::Sensor *s) { als_error_sensor_ = s; }
};

/**
 * VL6180XDistanceSensor: Handles distance measurement and Hardware Offset.
 */
class VL6180XDistanceSensor : public PollingComponent, public sensor::Sensor {
 public:
  VL6180XPololuHub *hub_;
  int offset_{0}; // Hardware offset to be written to register 0x024
  
  void set_hub(VL6180XPololuHub *hub) { hub_ = hub; }
  void set_offset(int offset) { offset_ = offset; }
  
  void setup() override; // Used to apply hardware settings after the Hub is ready
  void update() override;
  void dump_config() override;
};

/**
 * VL6180XALSSensor: Handles ambient light measurement in Lux.
 */
class VL6180XALSSensor : public PollingComponent, public sensor::Sensor {
 public:
  VL6180XPololuHub *hub_;
  void set_hub(VL6180XPololuHub *hub) { hub_ = hub; }
  void update() override;
  void dump_config() override;
};

} // namespace vl6180x_pololu
} // namespace esphome
