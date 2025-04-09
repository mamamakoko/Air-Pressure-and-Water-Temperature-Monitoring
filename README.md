# Air-Pressure-and-Water-Temperature-Monitoring

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A comprehensive Arduino-based monitoring system for air and water tanks, featuring pressure and temperature alerts, automatic valve control, and a touchscreen interface.

## Overview

This project provides a complete solution for monitoring and controlling both air and water tanks, with a focus on safety and early detection of pressure or temperature issues. The system uses an Arduino Mega with a TFT touchscreen display to show real-time sensor data and alert users to potential problems.

## Features

- **Dual Tank Monitoring**: 
  - Air tank pressure monitoring via two pressure sensors
  - Water tank temperature and humidity monitoring via DS18B20 and DHT11 sensors

- **Safety Features**:
  - Automatic detection of pressure release events
  - High-pressure and high-temperature alerts
  - Visual and audible alarms for critical conditions
  - Relay control for automated system response

- **User Interface**:
  - TFT touchscreen display with real-time readings
  - Color-coded status messages
  - On-screen reset button
  - Serial output for debugging and data logging

- **Non-blocking Design**:
  - Efficient sensor reading and display updates
  - Fail-safe error handling for sensor connectivity issues
  - Cached readings for unreliable sensors

## Hardware Requirements

- Arduino Mega 2560
- MCUFRIEND TFT LCD Display Shield (or compatible)
- 2× Pressure sensors (0-300 PSI range with 0.5-4.5V output)
- DS18B20 temperature sensor
- DHT11 temperature and humidity sensor  
- 2× Relay modules
- Green and red LEDs
- Buzzer
- 4.7KΩ pull-up resistor (for DS18B20)
- Breadboard and jumper wires

## Pin Connections

| Component | Pin |
|-----------|-----|
| First Pressure Sensor | A15 |
| Second Pressure Sensor | A14 |
| DHT11 Data | 23 |
| Air Pressure Relay | 24 |
| Water Temperature Relay | 25 |
| Green LED | 26 |
| Red LED | 27 |
| Buzzer | 28 |
| DS18B20 Temperature Sensor | 22 |
| Touchscreen | A1, A2, 7, 6 |

## Software Dependencies

The following libraries are required:

- [MCUFRIEND_kbv](https://github.com/prenticedavid/MCUFRIEND_kbv)
- [Adafruit_GFX](https://github.com/adafruit/Adafruit-GFX-Library)
- [OneWire](https://github.com/PaulStoffregen/OneWire)
- [DallasTemperature](https://github.com/milesburton/Arduino-Temperature-Control-Library)
- [DHT11](https://github.com/adidax/dht11) (or compatible DHT sensor library)
- [TouchScreen](https://github.com/adafruit/Adafruit_TouchScreen)

## Installation

1. Install all required libraries using the Arduino Library Manager
2. Connect all components according to the pin configuration above
3. Upload the code to your Arduino Mega
4. Calibrate the touchscreen if necessary by updating the `TS_LEFT`, `TS_RT`, `TS_TOP`, and `TS_BOT` constants

## Configuration

You can customize the system by modifying these constants in the code:

### Sensor Configuration
```cpp
#define SENSOR_MAX_PRESSURE 300.0    // Max sensor pressure (psi)
#define SENSOR_MAX_VOLTAGE 5.0       // Sensor output at max pressure
#define VOLTAGE_OFFSET 0.5           // Sensor offset voltage
```

### Alarm Thresholds
```cpp
#define HIGH_PRESSURE_THRESHOLD 10.0  // Threshold for pressure alarm (psi)
#define HIGH_TEMP_THRESHOLD 33.0      // Threshold for temperature alarm (°C)
#define RELEASE_DETECTION_THRESHOLD 2.0  // Threshold to detect pressure release (psi)
#define PRESSURE_LIMIT 160.0          // Maximum acceptable release pressure (psi)
#define WATER_HUMIDITY_THRESHOLD 85.0  // Threshold for high humidity in water tank
#define WATER_TEMP_DIFF_THRESHOLD 5.0  // Significant temp difference between sensors
```

### Timing Configuration
```cpp
#define NORMAL_MESSAGE_DURATION 10000  // Duration to show normal messages (ms)
#define ERROR_MESSAGE_DURATION 60000   // Duration to show error messages (ms)
#define REFRESH_INTERVAL 500           // Display refresh interval (ms)
#define SENSOR_READ_INTERVAL 100       // Sensor reading interval (ms)
```

## Usage

Once powered on, the system will:

1. Display real-time readings for air pressure, water temperature, and humidity
2. Monitor for high-pressure or high-temperature conditions
3. Activate relays to control external devices when thresholds are exceeded
4. Sound the buzzer and illuminate the red LED during alarm conditions
5. Display status messages when pressure release events are detected
6. Output all readings to the serial monitor at 9600 baud

Pressing the RESET button on the touchscreen will restart the Arduino.

## Troubleshooting

- **Sensor Errors**: If "Error" is displayed instead of a reading, check the sensor connections
- **Touchscreen Calibration**: If touch input is not working correctly, adjust the touchscreen calibration constants
- **Pressure Reading Issues**: Verify the pressure sensor power supply and ground connections
- **DHT11 Reading Failures**: Ensure the DHT11 is powered correctly and the pull-up resistor is installed

## Contributing

Contributions to improve the system are welcome. Please feel free to submit a pull request or open an issue to discuss potential improvements.

## License

This project is released under the MIT License - see the LICENSE file for details.

## Acknowledgments

- The Arduino community for library support
- MCUFRIEND for the display shield
- All contributors to the dependent libraries
