# ESPHome VL6180X Pololu Component

This is a custom ESPHome component for the **VL6180X** Time-of-Flight (ToF) distance and ambient light sensor. It is specifically designed to work with the [Pololu VL6180X library](https://github.com/pololu/vl6180x-arduino), providing a stable and robust integration for ESP32/ESP8266 devices.

## Features
- **Precise Distance Sensing:** Real-time ToF distance in millimeters.
- **Ambient Light Sensing (ALS):** Measurement of light levels in Lux.
- **Advanced Diagnostics:** Reports hardware error codes (including custom code `199` for disconnected sensors or I2C wiring failures).
- **Configurable Gain:** Adjust light sensitivity directly from your YAML configuration.
- **I2C Address Control:** Support for custom I2C addresses (defaults to `0x29`).
- **ESPHome Native:** Supports filters, sliding windows, and all standard sensor features.

## Hardware Compatibility

- **Official [Pololu VL6180X Time-of-Flight Distance Sensor Carrier](https://www.pololu.com/product/2489):** Fully compatible. Both Distance and Ambient Light Sensor (ALS) work perfectly out of the box. No hardware offset is usually required.
- **Generic / AliExpress Modules:** These clones vary in quality. During testing of one module:
  - **Distance sensing:** Works, but might require a hardware offset (e.g., +50mm) due to calibration differences.
  - **ALS (Light Sensing):** May consistently report `0 Lux` or error codes on some generic modules, likely due to hardware defects in the light-sensing part of the chip.

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
        # address: 0x29 # optional, defaults to 0x29
        
        distance:
          name: "Distance"
          update_interval: 100ms
          # If using a generic clone, you might need an offset:
          # offset: 50.0 # optional, defaults to 0
          filters:
            - median:
                window_size: 5
                send_every: 1
                
        distance_error_code:
          name: "Distance Error Code"

        ambient_light:
          name: "Illuminance"
          update_interval: 5s
          gain: 20 # Options: 1, 10, 20, 40 (higher is more sensitive)
          
        ambient_light_error_code:
          name: "ALS Error Code"
```

## Quick Start Example
For a complete, ready-to-flash configuration, check out the [Basic Test Example](examples/vl6180x-basic-test.yaml). This example includes:
- Default I2C pin configuration.
- Distance sensor with stabilizing filters.
- Pre-configured diagnostic sensors for error codes.
 
## Error Codes Reference

### Distance Errors (Register 0x04D)
- **0:** Success
- **6, 7:** Signal fail / Underflow (Object too far or no reflection)
- **11, 12, 13:** Signal out of range / ECE check failed
- **199:** **Hardware not found** (Check I2C wiring, pins, or power)

### ALS Errors (Register 0x04E)
- **0:** **Success** (The component automatically filters status bits, so `0` means valid data)
- **1-7:** Various ALS specific errors (e.g., overflow or signal saturation)
- **199:** **Hardware not found**

## Credits
This component is a wrapper around the [VL6180X library by Pololu](https://github.com/pololu/vl6180x-arduino). Special thanks to the Pololu team for their excellent work on the core library.

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
