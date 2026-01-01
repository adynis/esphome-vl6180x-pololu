#include "vl6180x_pololu.h"

namespace esphome {
namespace vl6180x_pololu {

static const char *TAG = "vl6180x_pololu";

/**
 * VL6180XPololuHub: Manages the shared I2C bus and sensor-level initialization.
 */
void VL6180XPololuHub::setup() {
  ESP_LOGCONFIG(TAG, "Starting VL6180X Hub on SDA:%d, SCL:%d", sda_pin_, scl_pin_);
  
  // 1. Initialize I2C with stability-focused parameters.
  // 50kHz clock speed ensures signal integrity over longer or noisy wires.
  Wire.begin(sda_pin_, scl_pin_);
  Wire.setClock(50000); 

  sensor.setBus(&Wire);
  sensor.setTimeout(50); // Prevents ESP32 from freezing during I2C hangs.
  sensor.setAddress(address_);
  
  sensor.init();
  
  // 2. Identification Check: Register 0x000 must be 0xB4.
  if (sensor.readReg(0x000) == 0xB4) {
    sensor.configureDefault();
    
    // Explicitly set ALS integration period to 100ms (0x0063).
    // This is vital to prevent timeouts when reading light levels.
    sensor.writeReg16Bit(0x040, 0x0063); 

    // Clear the "Fresh out of reset" bit to acknowledge system is ready.
    sensor.writeReg(0x016, 0x00); 

    // Maintain a consistent short timeout for fast distance ranging.
    sensor.setTimeout(50); 
    
    // Gain mapping for Ambient Light Sensing (ALS).
    uint8_t gain_reg = 0x44; // Default 1.67x
    if (als_gain_ <= 1) gain_reg = 0x46; 
    else if (als_gain_ <= 10) gain_reg = 0x43;
    else if (als_gain_ <= 40) gain_reg = 0x41;
    
    sensor.writeReg(VL6180X::SYSALS__ANALOGUE_GAIN, gain_reg);

    this->initialized_ = true;
    this->status_clear_warning();
    ESP_LOGI(TAG, "VL6180X successfully identified and initialized.");
  } else {
    ESP_LOGE(TAG, "HARDWARE FAILURE: VL6180X not found! Check wiring.");
    this->initialized_ = false;
    this->status_set_warning();
  }
}

void VL6180XPololuHub::dump_config() {
  ESP_LOGCONFIG(TAG, "VL6180X Hub Configuration:");
  ESP_LOGCONFIG(TAG, "  I2C Address: 0x%02X", address_);
}

// --- DISTANCE SENSOR LOGIC ---

/**
 * VL6180XDistanceSensor::setup
 * Handles the additive hardware offset logic and persistent state detection.
 */
void VL6180XDistanceSensor::setup() {
  if (hub_->initialized_) {
    /** * HARDWARE PERSISTENCE DETECTION (Magic Marker):
     * Uses Reg 0x011 as a scratchpad. It defaults to 0x00 on power-up.
     * Setting it to 0x12 after the first offset application prevents
     * redundant additive offsets during an ESP32 soft reset.
     */
    uint8_t marker = hub_->sensor.readReg(0x011);

    if (marker != 0x12) {
      // COLD BOOT: Apply additive offset (Factory NVM + User Offset).
      int8_t factory_offset = (int8_t)hub_->sensor.readReg(0x024);
      int8_t total_offset = factory_offset + (int8_t)offset_;
      
      hub_->sensor.writeReg(0x024, (uint8_t)total_offset);
      hub_->sensor.writeReg(0x011, 0x12); // Write marker

      ESP_LOGI(TAG, "Cold Boot: Applied Additive Offset (Total=%d)", total_offset);
    } else {
      // SOFT RESET: Hardware already holds the correct offset.
      ESP_LOGI(TAG, "Soft Reset: Sensor retains previously calibrated offset.");
    }
  }
}

void VL6180XDistanceSensor::update() {
  if (!hub_->initialized_) {
    if (hub_->distance_error_sensor_) hub_->distance_error_sensor_->publish_state(199);
    this->publish_state(NAN);
    return;
  }

  // 1. Initiate a single distance measurement.
  uint8_t range = hub_->sensor.readRangeSingleMillimeters();

  // 2. PHYSICAL BUS RECOVERY (Handling I2C NACK/Timeout - Error 199)
  // This section prevents the ESP32 from losing WiFi connection during bus hangs.
  if (hub_->sensor.timeoutOccurred()) {
    this->consecutive_errors_++;
    if (hub_->distance_error_sensor_) hub_->distance_error_sensor_->publish_state(199);
    
    ESP_LOGW(TAG, "I2C Transaction Failed (Timeout/NACK). Count: %d", this->consecutive_errors_);

    /**
     * MANUAL SCL BUS RECOVERY:
     * If 5 consecutive failures occur, the slave might be holding the SDA line LOW.
     * We switch the pins to GPIO and toggle SCL 9 times to force the slave to release SDA.
     */
    if (this->consecutive_errors_ >= 5) {
      ESP_LOGE(TAG, "I2C Bus frozen. Applying 9-pulse SCL Recovery procedure...");
      
      pinMode(hub_->sda_pin_, INPUT_PULLUP);
      pinMode(hub_->scl_pin_, OUTPUT);
      for (int i = 0; i < 9; i++) {
        digitalWrite(hub_->scl_pin_, LOW);
        delayMicroseconds(10);
        digitalWrite(hub_->scl_pin_, HIGH);
        delayMicroseconds(10);
      }
      
      // Re-start I2C hardware and re-initialize the VL6180X sensor.
      Wire.begin(hub_->sda_pin_, hub_->scl_pin_);
      Wire.setClock(50000);
      hub_->sensor.init();
      hub_->sensor.configureDefault();
      hub_->sensor.setTimeout(50);
      this->consecutive_errors_ = 0;
    }
    this->publish_state(NAN);
    return;
  }

  // 3. LOGICAL ERROR CHECK: Range Status (Register 0x04D).
  // Status bits are in [7:4]. 0 means Success.
  uint8_t raw_status = hub_->sensor.readReg(0x04D);
  uint8_t error_code = raw_status >> 4;
  
  if (hub_->distance_error_sensor_) {
    hub_->distance_error_sensor_->publish_state((float)error_code);
  }

  // 4. Decision Logic for Distance Publishing.
  if (error_code == 0) {
    // SUCCESS: Valid measurement obtained.
    this->consecutive_errors_ = 0;
    this->publish_state((float)range);
  } 
  else if (error_code == 12) {
    // UNDERFLOW: Internal algorithm resulted in < 0mm (Target too close/Crosstalk).
    this->consecutive_errors_++;

    if (this->consecutive_errors_ < 10) {
      // LATCHING: Skip publishing NAN to HA to maintain a clean graph during short glitches.
      ESP_LOGD(TAG, "Error 12 (Underflow) detected. Holding last known state.");
    } else {
      // Persistent Error: Publish NAN and attempt a sensor configuration refresh if it persists.
      this->publish_state(NAN);
      if (this->consecutive_errors_ > 50) {
        ESP_LOGW(TAG, "Persistent Error 12. Refreshing sensor state...");
        hub_->sensor.init();
        hub_->sensor.configureDefault();
        hub_->sensor.setTimeout(50);
        this->consecutive_errors_ = 0;
      }
    }
  } 
  else {
    // CRITICAL LOGICAL ERRORS (6, 7, 11 etc.): Reset latching and publish NAN.
    this->consecutive_errors_ = 0;
    this->publish_state(NAN);
  }
}

void VL6180XDistanceSensor::dump_config() { LOG_SENSOR("  ", "Distance Entity", this); }

// --- AMBIENT LIGHT SENSOR LOGIC ---

void VL6180XALSSensor::update() {
  if (!hub_->initialized_) return;

  // Barrier: Skip update if the I2C bus is currently in a timeout state.
  if (hub_->sensor.timeoutOccurred()) {
      ESP_LOGD(TAG, "ALS update skipped - I2C bus is currently timed out.");
      return;
  }

  // Clear any pending interrupts to ensure a fresh measurement start.
  hub_->sensor.writeReg(0x015, 0x07);

  // DYNAMIC TIMEOUT: ALS integration needs >100ms.
  // We temporarily increase timeout to 250ms for this operation only.
  hub_->sensor.setTimeout(250);

  uint16_t lux = hub_->sensor.readAmbientSingle();
  
  // Restore the fast 50ms timeout for the distance sensor.
  hub_->sensor.setTimeout(50);

  if (hub_->sensor.timeoutOccurred()) {
    if (hub_->als_error_sensor_) hub_->als_error_sensor_->publish_state(199);
    this->publish_state(NAN);
    ESP_LOGW(TAG, "ALS Timeout (199). Light integration failed.");
  } else {
    // Process ALS status bits [5:3] from Register 0x04E.
    uint8_t raw_status = hub_->sensor.readReg(0x04E);
    uint8_t error_code = (raw_status >> 3) & 0x07;
    
    if (hub_->als_error_sensor_) hub_->als_error_sensor_->publish_state((float)error_code);
    this->publish_state((float)lux);
  }
}

void VL6180XALSSensor::dump_config() { LOG_SENSOR("  ", "ALS Entity", this); }

} // namespace vl6180x_pololu
} // namespace esphome
