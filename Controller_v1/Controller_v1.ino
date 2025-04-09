#include <MCUFRIEND_kbv.h>
#include <Adafruit_GFX.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT11.h>
#include <TouchScreen.h>

// Pin Definitions
#define PRESSURE_SENSOR_PIN A15      // First Pressure Sensor (Air Tank Pressure)
#define SECOND_PRESSURE_SENSOR_PIN A14  // Second Pressure Sensor (Air Tank T&P Valve)
#define DHT_PIN 23                   // DHT11 data pin for Water Tank
#define RELAY_PIN 24                 // Relay control for air pressure
#define RELAY_PIN2 25                // Relay control for water temperature
#define BUZZER_PIN 28                // Buzzer
#define GREEN_LED_PIN 26             // Green LED (default ON)
#define RED_LED_PIN 27               // Red LED
#define TEMP_SENSOR_PIN 22           // DS18B20 Data Pin for Water Tank

// Touch Screen Pin Definitions
#define YP A1
#define XM A2
#define YM 7
#define XP 6
#define MINPRESSURE 200
#define MAXPRESSURE 1000

// Touch Screen Calibration Values
const int TS_LEFT = 970;
const int TS_RT = 183;
const int TS_TOP = 183;
const int TS_BOT = 954;

// Sensor Constants
#define SENSOR_MAX_PRESSURE 300.0    // Max sensor pressure (psi)
#define SENSOR_MAX_VOLTAGE 5.0       // Sensor output at max pressure
#define VOLTAGE_OFFSET 0.5           // Sensor offset voltage
#define ADC_RESOLUTION 1023          // ADC resolution

// Threshold Constants
#define HIGH_PRESSURE_THRESHOLD 10.0  // Threshold for pressure alarm (psi)
#define HIGH_TEMP_THRESHOLD 33.0      // Threshold for temperature alarm (°C)
#define RELEASE_DETECTION_THRESHOLD 2.0  // Threshold to detect pressure release (psi)
#define PRESSURE_LIMIT 160.0          // Maximum acceptable release pressure (psi)
#define WATER_HUMIDITY_THRESHOLD 85.0  // Threshold for high humidity in water tank
#define WATER_TEMP_DIFF_THRESHOLD 5.0  // Significant temp difference between sensors

// Display & Timing Constants
#define NORMAL_MESSAGE_DURATION 10000  // Duration to show normal messages (ms)
#define ERROR_MESSAGE_DURATION 60000   // Duration to show error messages (ms)
#define REFRESH_INTERVAL 500           // Display refresh interval (ms)
#define SENSOR_READ_INTERVAL 100       // Sensor reading interval (ms)
#define DHT_READ_RETRY_DELAY 1000      // Time between DHT read retries

// Special Values
#define TEMP_ERROR_VALUE -999.0        // Value to indicate temperature sensor error

// Display Colors
#define WHITE  0xFFFF
#define BLACK  0x0000
#define RED    0xF800
#define GREEN  0x07E0
#define BLUE   0x001F
#define CYAN   0x07FF
#define YELLOW 0xFFE0
#define ORANGE 0xFD20

// Initialize objects
MCUFRIEND_kbv tft;
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
Adafruit_GFX_Button reset_btn;
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);
DHT11 dht(DHT_PIN);  // Modified for DHT11.h library

// Global variables for touch screen
int pixel_x, pixel_y;     // Touch coordinates

// Air tank variables
float lastPressureReading = 0.0;    // Store last valid pressure from the first sensor
bool lastPressureStored = false;    // Flag to store pressure only once

// Water tank variables
float lastWaterTankTemp = 0.0;      // Last water tank temperature
float lastHumidity = 0.0;           // Last humidity reading
bool waterTankReleaseDetected = false; // Flag for water tank release
float waterTankReleaseTemp = 0.0;   // Temperature at release

// DHT sensor variables
float lastValidHumidity = 0.0;      // Last valid humidity reading
float lastValidDhtTemp = 0.0;       // Last valid DHT temperature reading
unsigned long lastDhtSuccessTime = 0; // When we last got a good DHT reading
unsigned long lastDhtReadTime = 0;    // When we last attempted to read DHT

// Display timing variables
unsigned long displayStartTime = 0;  // Tracks when the message started
bool displayActive = false;          // Tracks if the message is currently displayed
bool errorDisplayed = false;         // Tracks if an error message is displayed
unsigned long lastSensorReadTime = 0;  // For non-blocking sensor reading
unsigned long lastRefreshTime = 0;     // For non-blocking display updates
bool waterTankDisplayActive = false;   // Water tank display status
unsigned long waterDisplayStartTime = 0; // Water tank display timing

// Global variables for recent readings
float currentAirPressure = 0.0;
float currentAirValvePressure = 0.0;
float currentWaterTempC = 0.0;  
float currentWaterTempF = 0.0;
float currentHumidity = 0.0;
float currentDhtTempC = 0.0;
bool currentDhtSuccess = false;

// Function prototypes
bool Touch_getXY(void);
float readPressure(int pin);
float readTemperatureC();
float convertToFahrenheit(float tempC);
bool readDHT(float &humidity, float &tempC);
void updatePressureDisplay(float pressure);
void updateTemperatureDisplay(float tempC, float tempF);
void updateDHTDisplay(float humidity, float tempC);
void updateAirTankStatus(float pressure2, float lastPressureReading);
void updateWaterTankStatus(float waterTemp, float dhtTemp, float humidity);
void controlPressureRelay(float pressure);
void controlTemperatureRelay(float temperatureC);
void triggerAlarm();
void setupDisplay();
void resetArduino();

// Get touch screen coordinates
bool Touch_getXY(void) {
    TSPoint p = ts.getPoint();
    pinMode(YP, OUTPUT);      //restore shared pins
    pinMode(XM, OUTPUT);
    digitalWrite(YP, HIGH);   //because TFT control pins
    digitalWrite(XM, HIGH);
    bool pressed = (p.z > MINPRESSURE && p.z < MAXPRESSURE);
    if (pressed) {
        // For landscape mode
        pixel_x = map(p.y, TS_LEFT, TS_RT, 0, 480);
        pixel_y = map(p.x, TS_TOP, TS_BOT, 0, 320);
    }
    return pressed;
}

void setup() {
    Serial.begin(9600);
    
    // Initialize pins
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(RELAY_PIN2, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);

    // Set initial states
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(RELAY_PIN2, LOW);
    digitalWrite(BUZZER_PIN, LOW);

    // Initialize sensors
    tempSensor.begin();
    // DHT11.h may not need initialization or may use a different method
    // Check your library documentation

    // Initialize display
    setupDisplay();
}

void setupDisplay() {
    uint16_t ID = tft.readID();
    tft.begin(ID);
    tft.setRotation(1);  // Landscape mode
    tft.fillScreen(BLACK);

    // Title
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.setCursor(50, 20);
    tft.print("Dual Tank Monitoring");

    // Air Tank Section
    tft.setTextSize(2);
    tft.setTextColor(WHITE);
    tft.setCursor(20, 50);
    tft.print("Air Tank:");

    tft.setTextSize(2);
    tft.setCursor(30, 80);
    tft.setTextColor(CYAN);
    tft.print("Pressure:");

    // Water Tank Section
    tft.setTextSize(2);
    tft.setTextColor(WHITE);
    tft.setCursor(20, 120);
    tft.print("Water Tank:");

    tft.setTextSize(2);
    tft.setCursor(30, 150);
    tft.setTextColor(YELLOW);
    tft.print("Temperature:");

    tft.setTextSize(2);
    tft.setCursor(30, 180);
    tft.setTextColor(GREEN);
    tft.print("Humidity:");
    
    // Initialize reset button (position at top right corner)
    reset_btn.initButton(&tft, 400, 40, 150, 70, WHITE, CYAN, BLACK, "RESET", 3);
    reset_btn.drawButton(false);
}

// Reset Arduino function
void resetArduino() {
    // Jump to address 0 (reset vector)
    asm volatile ("jmp 0");
}

// Read pressure sensor value and convert to PSI
float readPressure(int pin) {
    int sensorValue = analogRead(pin);
    float voltage = (sensorValue / (float)ADC_RESOLUTION) * SENSOR_MAX_VOLTAGE;
    float pressure = ((voltage - VOLTAGE_OFFSET) / (SENSOR_MAX_VOLTAGE - VOLTAGE_OFFSET)) * SENSOR_MAX_PRESSURE;
    
    // Validate reading and handle potential errors
    if (pressure < 0) return 0.0;
    if (pressure > SENSOR_MAX_PRESSURE) return SENSOR_MAX_PRESSURE;
    
    return pressure;
}

// Read temperature in Celsius with error handling
float readTemperatureC() {
    tempSensor.requestTemperatures();
    float temp = tempSensor.getTempCByIndex(0);
    
    // Check for common error values
    if (temp == -127.0 || temp == 85.0) {
        // Return special error indicator
        return TEMP_ERROR_VALUE; // Special value to indicate error
    }
    
    return temp;
}

// Read DHT11 data with error handling and caching
bool readDHT(float &humidity, float &tempC) {
    unsigned long currentTime = millis();
    
    // Only try to read DHT11 if sufficient time has passed since last attempt
    // DHT11 sensors need time between reads for stability
    if (currentTime - lastDhtReadTime >= DHT_READ_RETRY_DELAY) {
        lastDhtReadTime = currentTime;

        int intHumidity, intTemp;
        
        // For DHT11.h library - check your library's specific function and return codes
        int result = dht.readTemperatureHumidity(intTemp, intHumidity);
        
        // Check if reading was successful (typically returns 0 on success)
        if (result == 0 && intHumidity > 0 && intTemp > 0) {  // Basic validation
            humidity = intHumidity;
            tempC = intTemp;

            lastValidHumidity = humidity;
            lastValidDhtTemp = tempC;
            lastDhtSuccessTime = currentTime;
            return true;
        }
    }
    
    // If read failed or not time to read yet, check if we have recent valid data
    if (currentTime - lastDhtSuccessTime < 30000) {  // Use cache for up to 30 seconds
        humidity = lastValidHumidity;
        tempC = lastValidDhtTemp;
        return true;
    }
    
    return false;
}

// Convert Celsius to Fahrenheit
float convertToFahrenheit(float tempC) {
    return (tempC * 9.0 / 5.0) + 32.0;
}

// Update TFT display with new pressure reading
void updatePressureDisplay(float pressure) {
    tft.setTextSize(2);
    tft.fillRect(180, 80, 100, 30, BLACK);
    tft.setCursor(180, 80);
    tft.setTextColor(CYAN);
    tft.print(pressure, 1);
    tft.print(" psi");
}

// Update TFT display with DS18B20 temperature readings
void updateTemperatureDisplay(float tempC, float tempF) {
    tft.setTextSize(2);
    tft.fillRect(180, 150, 100, 30, BLACK);
    tft.setCursor(180, 150);
    tft.setTextColor(YELLOW);
    
    if (tempC == TEMP_ERROR_VALUE) {
        // Display error message instead of temperature
        tft.print("Error");
    } else {
        // Normal temperature display
        tft.print(tempC, 1);
        tft.print(" C / ");
        tft.print(tempF, 1);
        tft.print(" F");
    }
}

// Update TFT display with DHT11 readings
void updateDHTDisplay(float humidity, float tempC) {
    tft.setTextSize(2);
    // Clear previous values
    tft.fillRect(180, 180, 120, 30, BLACK);
    
    tft.setCursor(180, 180);
    tft.setTextColor(GREEN);
    
    if (humidity <= 0 || tempC <= 0) {  // Basic validation for DHT11.h library
        tft.print("Error");
    } else {
        tft.print(tempC, 1);
        tft.print("C ");
        tft.print(humidity, 1);
        tft.print("%");
    }
}

// Update TFT display when air tank releases pressure
void updateAirTankStatus(float pressure2, float lastPressureReading) {
    unsigned long currentTime = millis();

    // Check if air tank T&P valve is releasing pressure
    if (pressure2 >= RELEASE_DETECTION_THRESHOLD) {
        if (!displayActive) {
            displayStartTime = currentTime;  // Store start time
            displayActive = true;
            errorDisplayed = (lastPressureReading > PRESSURE_LIMIT);
        }

        // Different display duration for normal vs error conditions
        unsigned long displayDuration = errorDisplayed ? ERROR_MESSAGE_DURATION : NORMAL_MESSAGE_DURATION;
        
        if (currentTime - displayStartTime < displayDuration) {
            tft.setTextSize(2);
            
            // Air tank status message - upper part
            if (lastPressureReading <= PRESSURE_LIMIT) {
                // Normal release message
                tft.fillRect(0, 220, 500, 40, GREEN);
                tft.setCursor(20, 230);
                tft.setTextColor(BLACK);
                tft.print("Air Tank:");
                tft.setCursor(150, 230);
                tft.setTextColor(BLACK);
                tft.print("Released at ");
                tft.print(lastPressureReading, 1);
                tft.print(" psi");
            } else {
                // Error release message
                tft.fillRect(0, 220, 500, 40, RED);
                tft.setCursor(20, 230);
                tft.setTextColor(WHITE);
                tft.print("AIR TANK ERROR ");
                tft.print(lastPressureReading, 1);
                tft.print(" psi!");
            }
        } else {
            displayActive = false;
            // Clear status area after message timeout
            tft.fillRect(0, 220, 500, 40, BLACK);
        }
    }
}

// Update TFT display when water tank shows signs of release
void updateWaterTankStatus(float waterTemp, float dhtTemp, float humidity) {
    unsigned long currentTime = millis();
    
    // Logic to detect water tank T&P valve release:
    // 1. High humidity is a strong indicator
    // 2. Significant temperature difference between sensors may also indicate release
    
    bool releaseCondition = (humidity > WATER_HUMIDITY_THRESHOLD) || 
                            (abs(waterTemp - dhtTemp) > WATER_TEMP_DIFF_THRESHOLD);
    
    if (releaseCondition) {
        if (!waterTankDisplayActive) {
            waterDisplayStartTime = currentTime;
            waterTankDisplayActive = true;
            waterTankReleaseTemp = waterTemp;
        }
        
        if (currentTime - waterDisplayStartTime < NORMAL_MESSAGE_DURATION) {
            tft.setTextSize(2);
            
            // Water tank status message - lower part
            tft.fillRect(0, 270, 500, 40, BLUE);
            tft.setCursor(20, 280);
            tft.setTextColor(WHITE);
            tft.print("Water Tank:");
            tft.setCursor(150, 280);
            tft.print("Release at ");
            tft.print(waterTankReleaseTemp, 1);
            tft.print("C");
        } else {
            waterTankDisplayActive = false;
            // Clear status area after message timeout
            tft.fillRect(0, 270, 500, 40, BLACK);
        }
    }
}

// Trigger the alarm for alerts
void triggerAlarm() {
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);

    // Beep twice
    for (int i = 0; i < 2; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(200);
        digitalWrite(BUZZER_PIN, LOW);
        delay(200);
    }
}

// Control relay based on air tank pressure
void controlPressureRelay(float pressure) {
    if (pressure >= HIGH_PRESSURE_THRESHOLD) {
        digitalWrite(RELAY_PIN, HIGH);
        triggerAlarm();
    } else {
        digitalWrite(RELAY_PIN, LOW);
        if (readTemperatureC() < HIGH_TEMP_THRESHOLD) {
            // Only reset LEDs if both conditions are normal
            digitalWrite(RED_LED_PIN, LOW);
            digitalWrite(GREEN_LED_PIN, HIGH);
        }
    }
}

// Control relay based on water tank temperature
void controlTemperatureRelay(float temperatureC) {
    // Skip relay control if temperature sensor has error
    if (temperatureC == TEMP_ERROR_VALUE) {
        // Just leave relay in current state when sensor has error
        return;
    }
    
    if (temperatureC >= HIGH_TEMP_THRESHOLD) {
        digitalWrite(RELAY_PIN2, HIGH);
        triggerAlarm();
    } else {
        digitalWrite(RELAY_PIN2, LOW);
        if (readPressure(PRESSURE_SENSOR_PIN) < HIGH_PRESSURE_THRESHOLD) {
            // Only turn off LED if pressure is also normal
            digitalWrite(RED_LED_PIN, LOW);
            digitalWrite(GREEN_LED_PIN, HIGH);
        }
    }
}

void loop() {
    unsigned long currentTime = millis();
    
    // Handle touch screen input
    bool down = Touch_getXY();
    reset_btn.press(down && reset_btn.contains(pixel_x, pixel_y));
    
    if (reset_btn.justReleased()) {
        reset_btn.drawButton();
    }
    
    if (reset_btn.justPressed()) {
        reset_btn.drawButton(true);  // Show button pressed
        
        // Visual feedback before reset
        tft.fillRect(120, 120, 240, 80, RED);
        tft.setCursor(140, 150);
        tft.setTextColor(WHITE);
        tft.setTextSize(2);
        tft.print("RESETTING...");
        delay(500);  // Brief delay for visual feedback
        
        // Reset the Arduino
        resetArduino();
    }
    
    // Read sensors at regular intervals (non-blocking)
    if (currentTime - lastSensorReadTime >= SENSOR_READ_INTERVAL) {
        // Air tank readings
        currentAirPressure = readPressure(PRESSURE_SENSOR_PIN);
        currentAirValvePressure = readPressure(SECOND_PRESSURE_SENSOR_PIN);
        
        // Water tank readings
        currentWaterTempC = readTemperatureC();
        currentWaterTempF = (currentWaterTempC == TEMP_ERROR_VALUE) ? 
                            TEMP_ERROR_VALUE : convertToFahrenheit(currentWaterTempC);
        
        // DHT11 readings (ambient near water tank)
        currentDhtSuccess = readDHT(currentHumidity, currentDhtTempC);
        
        // Log readings to serial
        Serial.print("Air Tank Pressure: ");
        Serial.print(currentAirPressure);
        Serial.print(" psi | Air Valve: ");
        Serial.print(currentAirValvePressure);
        Serial.print(" psi | Water Temp: ");
        
        if (currentWaterTempC == TEMP_ERROR_VALUE) {
            Serial.println("ERROR - Sensor not connected");
        } else {
            Serial.print(currentWaterTempC);
            Serial.println(" °C");
        }
        
        if (currentDhtSuccess) {
            Serial.print("Water Tank DHT11: ");
            Serial.print(currentDhtTempC);
            Serial.print("°C | Humidity: ");
            Serial.print(currentHumidity);
            Serial.println("%");
            
            lastHumidity = currentHumidity;
        } else {
            Serial.println("DHT11 Reading Failed");
        }
        
        // Control systems based on readings
        controlPressureRelay(currentAirPressure);
        controlTemperatureRelay(currentWaterTempC);
        
        // Detect air tank pressure release events
        if (currentAirValvePressure >= RELEASE_DETECTION_THRESHOLD && !lastPressureStored) {
            lastPressureReading = currentAirPressure;
            lastPressureStored = true;  // Prevent storing again
        } 
        
        // Reset flag when air valve release is not detected
        if (currentAirValvePressure < RELEASE_DETECTION_THRESHOLD) {
            lastPressureStored = false;
        }
        
        lastSensorReadTime = currentTime;
    }
    
    // Update display at regular intervals (non-blocking)
    if (currentTime - lastRefreshTime >= REFRESH_INTERVAL) {
        // Update displays using the stored readings
        updatePressureDisplay(currentAirPressure);
        updateTemperatureDisplay(currentWaterTempC, currentWaterTempF);
        
        if (currentDhtSuccess) {
            updateDHTDisplay(currentHumidity, currentDhtTempC);
            
            // Check for water tank valve release
            if (currentWaterTempC != TEMP_ERROR_VALUE) {
                updateWaterTankStatus(currentWaterTempC, currentDhtTempC, currentHumidity);
            }
        } else {
            // Use updateDHTDisplay with values that will trigger error display
            updateDHTDisplay(0, 0);
        }
        
        // Update air tank status display
        updateAirTankStatus(currentAirValvePressure, lastPressureReading);
        
        lastRefreshTime = currentTime;
    }
}
