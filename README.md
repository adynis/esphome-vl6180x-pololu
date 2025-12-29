# ESPHome VL6180X Pololu Component

This is a custom ESPHome component for the **VL6180X** Time-of-Flight (ToF) distance and ambient light sensor. It is specifically designed to work with the [Pololu VL6180X library](https://github.com/pololu/vl6180x-arduino), providing a stable and robust integration for ESP32/ESP8266 devices.

## Features
- **Precise Distance Sensing:** Real-time ToF distance in millimeters.
- **Ambient Light Sensing (ALS):** Measurement of light levels in Lux.
- **Additive Hardware Offset:** Adjust the 0mm point while preserving factory NVM calibration.
- **Advanced Diagnostics:** Reports hardware error codes (including custom code 199 for disconnected sensors).
- **Configurable Gain:** Adjust light sensitivity directly from your YAML configuration.
- **I2C Address Control:** Support for custom I2C addresses (defaults to 0x29).

## Hardware Compatibility

- **Official Pololu VL6180X Carrier:** Fully compatible. Usually comes with a factory NVM offset (e.g., ~21mm). 
- **Generic / AliExpress Modules:** These clones vary in quality. During testing, we found some clones have a very high internal factory offset (e.g., ~52mm), making them report NaN or Errors at close range unless corrected with the offset parameter.

## Installation

Add this to your ESPHome YAML configuration:
```yaml
    external_components:
      - source:
          type: git
          url: [https://github.com/adynis/esphome-vl6180x-pololu](https://github.com/adynis/esphome-vl6180x-pololu)
        components: [ vl6180x_pololu ]

    sensor:
      - platform: vl6180x_pololu
        sda_pin: 23
        scl_pin: 22
        
        distance:
          name: "Distance"
          offset: 50 # Additive hardware offset
          update_interval: 100ms
          filters:
            - median:
                window_size: 5
                send_every: 1
                
        distance_error_code:
          name: "Distance Error Code"

        ambient_light:
          name: "Illuminance"
          update_interval: 5s
          gain: 20
          
        ambient_light_error_code:
          name: "ALS Error Code"
```
For a full working example, see examples/vl6180x-basic-test.yaml.

## Configuration Variables

### VL6180X Hub (Main)
- **sda_pin** (Required, int): The GPIO pin for I2C SDA.
- **scl_pin** (Required, int): The GPIO pin for I2C SCL.
- **address** (Optional, hex): The I2C address of the sensor. Defaults to 0x29.

### Distance Sensor (distance:)
- **name** (Required, string): The name of the distance sensor.
- **update_interval** (Optional, Time): Defaults to 100ms.
- **offset** (Optional, int): Hardware offset in mm to adjust the 0mm point. Defaults to 0. 
  - Note: This uses additive hardware logic. It reads the factory calibration from the sensor and adds your value to it.

### Ambient Light Sensor (ambient_light:)
- **name** (Required, string): The name of the light sensor.
- **update_interval** (Optional, Time): Defaults to 5s.
- **gain** (Optional, int): Options: 1, 10, 20, 40. Defaults to 20.

## Error Codes Reference

### Distance Errors (Register 0x04D)
- **0:** Success
- **6, 7:** Signal fail / Underflow (Object too far or hardware limit reached)
- **11, 12, 13:** Signal out of range / Ambient light saturation
- **199:** Hardware not found (Check wiring or power)

### ALS Errors (Register 0x04E)
- **0:** Success (The component filters status bits, so 0 means valid data)
- **1-7:** Various ALS specific errors (e.g., overflow or saturation)
- **199:** Hardware not found

## Troubleshooting & Notes

> [!IMPORTANT]
> **Power Cycle Required for Offset Changes:** The offset parameter is written to the sensor's hardware registers during setup. Because I2C sensors often stay powered even when the ESP32 restarts, a physical power cycle (unplugging the power) is required to ensure the hardware register reloads its internal calibration and applies the new additive offset.

> [!TIP]
> **Factory Calibration:** During testing, we found that original Pololu sensors have a factory offset (e.g., 21), while generic clones can have much higher values (e.g., 52). Our additive logic preserves these factory-tuned values while allowing you to compensate for housing or clone-specific inaccuracies.

## Credits
This component is a wrapper around the VL6180X library by Pololu. 

## License
MIT License. See LICENSE for details.
