# T&P Valve Testing System

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

This Arduino-based system tests Temperature and Pressure (T&P) relief valves commonly used in water heaters and pressure vessels. It provides both pressure and temperature testing capabilities with a user-friendly touchscreen interface.

## Overview

The T&P Valve Testing System is designed to verify the proper operation of safety relief valves by:
1. Testing pressure release functionality with a controlled air pressure system
2. Testing temperature release functionality with a controlled water heating system

The system provides real-time monitoring and visual feedback through a TFT touchscreen display, with alerts for both successful valve operation and failure conditions.

## Features

- **Dual Testing Modes**:
  - Pressure testing with controlled air pressure
  - Temperature testing with controlled water heating
  
- **User Interface**:
  - 480x320 TFT color touchscreen with intuitive interface
  - Visual indicators for test status and results
  - Simple navigation with touchscreen buttons
  
- **Safety Features**:
  - Visual and audible alarms for test failures and high-pressure/temperature conditions
  - Automatic shutdown when thresholds are exceeded
  - Status LEDs for quick visual status indication
  
- **Sensors**:
  - Dual pressure sensors (main tank and relief valve)
  - Waterproof DS18B20 temperature sensor for water temperature
  - DHT11 temperature and humidity sensor for steam/release detection
  
- **Control System**:
  - Relay controls for pressure pump and heating element
  - Adjustable thresholds for testing parameters

## Hardware Requirements

- Arduino Mega 2560 (or compatible)
- 3.5" or larger TFT touchscreen display compatible with MCUFRIEND library
- 2x Pressure sensors (0-300 PSI)
- DS18B20 waterproof temperature sensor
- DHT11 temperature and humidity sensor
- 2x Relay modules
- Buzzer
- Status LEDs (red and green)
- Power supply
- Pressure pump (for air testing)
- Heating element (for temperature testing)

## Pin Configuration

```
PRESSURE_SENSOR_PIN: A15       // First Pressure Sensor (Air Tank Pressure)
SECOND_PRESSURE_SENSOR_PIN: A14  // Second Pressure Sensor (Air Tank T&P Valve)
DHT_PIN: 23                    // DHT11 data pin for Water Tank
RELAY_PIN: 24                  // Relay control for air pressure
RELAY_PIN2: 25                 // Relay control for water temperature
BUZZER_PIN: 28                 // Buzzer
GREEN_LED_PIN: 26              // Green LED (default ON)
RED_LED_PIN: 27                // Red LED
TEMP_SENSOR_PIN: 22            // DS18B20 Data Pin for Water Tank
```

Touchscreen pins (standard MCUFRIEND configuration):
```
YP: A1
XM: A2
YM: 7
XP: 6
```

## Required Libraries

- MCUFRIEND_kbv
- Adafruit_GFX
- OneWire
- DallasTemperature
- DHT11
- TouchScreen

## Installation

1. Install all required libraries through the Arduino Library Manager
2. Connect all hardware components according to the pin configuration
3. Upload the code to your Arduino Mega
4. Calibrate the touchscreen if needed (adjust TS_LEFT, TS_RT, TS_TOP, TS_BOT constants)

## Usage

1. **Starting the System**:
   - Power on the device
   - Wait for the splash screen to finish
   - Press "START" on the main screen

2. **Running Tests**:
   - Select "PRESSURE TEST" or "TEMPERATURE TEST" from the test selection screen
   - Monitor real-time sensor readings during the test
   - The system will automatically detect valve releases and display results

3. **Interpreting Results**:
   - For pressure tests, a successful result shows the pressure at which the valve released
   - For temperature tests, a successful result shows the temperature at which the valve released
   - Failed tests will indicate why the valve did not operate correctly

4. **After Testing**:
   - Choose "YES" to run another test
   - Choose "NO" to return to the start screen
   - Choose "DONE" to reset the system

## Test Parameters

The system uses the following default parameters:

- Maximum pressure limit: 160.0 PSI
- High temperature threshold: 100.0°C
- High pressure threshold: 10.0 PSI (for relay control)
- Pressure release detection threshold: 2.0 PSI
- Humidity threshold for water release detection: 96.0%

These parameters can be adjusted in the code by modifying the threshold constants.

## Troubleshooting

- **Touchscreen Not Responding**: Verify touchscreen calibration values
- **Inaccurate Pressure Readings**: Check sensor connections and calibration
- **Temperature Sensor Errors**: Ensure DS18B20 is properly connected with correct pullup resistor
- **DHT11 Reading Failures**: Check wiring and ensure power supply is stable

## Safety Notes

- This system is designed for testing T&P relief valves only
- Always follow proper safety procedures when working with pressurized systems
- Never exceed the maximum rated pressure of your testing equipment
- Ensure adequate ventilation when testing temperature release valves
- Keep water away from electronic components

## License

[Include your preferred license information here]

## Contributing

Contributions to improve the system are welcome. Please feel free to submit a pull request or open an issue to discuss potential changes or improvements.
