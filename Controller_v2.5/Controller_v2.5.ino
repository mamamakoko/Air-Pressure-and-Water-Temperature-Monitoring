#include <MCUFRIEND_kbv.h>
#include <Adafruit_GFX.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT11.h>
#include <TouchScreen.h>
#include <Adafruit_BMP085.h>

// Pin Definitions
#define PRESSURE_SENSOR_PIN A15      // First Pressure Sensor (Air Tank Pressure)
#define RELAY_PIN 24                 // Relay control for air pressure
#define RELAY_PIN2 25                // Relay control for water temperature
#define BUZZER_PIN 28                // Buzzer
#define GREEN_LED_PIN 26             // Green LED (default ON)
#define RED_LED_PIN 27               // Red LED
#define TEMP_SENSOR_PIN 22           // DS18B20 Data Pin for Water Tank
#define DHT_PIN 23                   // DHT11
#define DHT_TYPE DHT11               // Define sensor type

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
#define HIGH_PRESSURE_THRESHOLD 50.0        // Threshold for pressure alarm (psi)
#define HIGH_TEMP_THRESHOLD 85.0            // Threshold for temperature alarm (°C)
#define RELEASE_DETECTION_THRESHOLD 15.0     // Threshold to detect pressure release (psi)
#define PRESSURE_LIMIT 50.0                 // Maximum acceptable release pressure (psi)
#define TEMPERATURE_LIMIT 99.4             // Maximum acceptable water temperature (°C)
#define MINIMUM_TEMPERATURE 70.0            // Minimum temperature to maintain
#define WATER_TEMP_RELEASE 35.0             // Temp to considered as release

// Display & Timing Constants
#define NORMAL_MESSAGE_DURATION 10000  // Duration to show normal messages (ms)
#define ERROR_MESSAGE_DURATION 60000   // Duration to show error messages (ms)
#define REFRESH_INTERVAL 500           // Display refresh interval (ms)
#define SENSOR_READ_INTERVAL 100       // Sensor reading interval (ms)
#define DHT_MIN_INTERVAL 2000   // Minimum time between DHT11 readings (ms)

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
#define LIGHTGRAY 0xCE79

// Application states
enum AppState {
    SPLASH_SCREEN,
    START_SCREEN,
    TEST_SELECTION,
    TESTING_PRESSURE,
    TESTING_TEMPERATURE,
    RESULTS_SCREEN
};

// Initialize objects
MCUFRIEND_kbv tft;
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);

// Button objects
Adafruit_GFX_Button reset_btn;
Adafruit_GFX_Button start_btn;
Adafruit_GFX_Button pressure_btn;
Adafruit_GFX_Button temp_btn;
Adafruit_GFX_Button yes_btn;
Adafruit_GFX_Button no_btn;
Adafruit_GFX_Button done_btn;

// BMP085 sensor object
Adafruit_BMP085 bmp;

// DHT sensor object
DHT11 dht11(DHT_PIN);

// Global variables for touch screen
int pixel_x, pixel_y;     // Touch coordinates

// Air tank variables
float lastPressureReading = 0.0;        // Store last valid pressure from the first sensor
bool lastPressureStored = false;        // Flag to store pressure only once
bool pressureReleaseDetected = false;   // Flag for pressure release
unsigned long releaseDetectionTime = 0;
bool pressureLimitExceeded = false;

// Water tank variables
float lastWaterTankTemp = 0.0;          // Last water tank temperature
bool waterTankReleaseDetected = false;  // Flag for water tank release
float waterTankReleaseTemp = 0.0;       // Temperature at release
unsigned long lastDHTReadTime = 0;

// Display timing variables
unsigned long displayStartTime = 0;         // Tracks when the message started
bool displayActive = false;                 // Tracks if the message is currently displayed
bool errorDisplayed = false;                // Tracks if an error message is displayed
unsigned long lastSensorReadTime = 0;       // For non-blocking sensor reading
unsigned long lastRefreshTime = 0;          // For non-blocking display updates
bool waterTankDisplayActive = false;        // Water tank display status
unsigned long waterDisplayStartTime = 0;    // Water tank display timing
unsigned long splashScreenStartTime = 0;    // Splash screen timing

// Global variables for recent readings
float currentAirPressure = 0.0;
float currentAirValvePressure = 0.0;
float currentWaterTempC = 0.0;  
float currentWaterTempF = 0.0;

// Global variable for water temperature error
bool waterHighTempNoReleaseError = false;
unsigned long waterErrorDisplayStartTime = 0;
bool waterErrorDisplayActive = false;

// Application state control
AppState currentState = SPLASH_SCREEN;
unsigned long stateStartTime = 0;
String testResult = "";
bool testSuccess = false;

// Function prototypes
bool Touch_getXY(void);
float readPressure(int pin);
float readTemperatureC();
float convertToFahrenheit(float tempC);
void updatePressureDisplay(float pressure);
void updateTemperatureDisplay(float tempC, float tempF);
void updateAirTankStatus(float pressure2, float lastPressureReading);
void controlPressureRelay(float pressure);
void controlTemperatureRelay(float temperatureC);
void triggerAlarm();
void triggerAlarmError();
void setupDisplay();
void resetArduino();

// New function prototypes for UI states
void drawSplashScreen();
void drawStartScreen();
void drawTestSelectionScreen();
void drawTestingScreen(bool isPressureTest);
void drawResultsScreen();
void performPressureTest();
void performTemperatureTest();

bool bmpSensorError = false; // Add a global flag for BMP085 sensor error

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

        digitalWrite(BUZZER_PIN, HIGH);
        delay(50);  // Short 50ms beep
        digitalWrite(BUZZER_PIN, LOW);
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
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(RELAY_PIN2, HIGH);
    digitalWrite(BUZZER_PIN, LOW);

    // Initialize sensors
    tempSensor.begin();

    // Initialize BMP180
    if (!bmp.begin()) {
        Serial.println("Could not find a valid BMP180 sensor!");
        bmpSensorError = true;
    }

    // Initialize display
    uint16_t ID = tft.readID();
    tft.begin(ID);
    tft.setRotation(1);  // Landscape mode
    tft.fillScreen(BLACK);
    
    // Initialize state variables
    currentState = SPLASH_SCREEN;
    stateStartTime = millis();
    splashScreenStartTime = millis();
    
    // Draw the splash screen
    drawSplashScreen();
}

void drawSplashScreen() {
    tft.fillScreen(BLACK);
    
    // Draw title
    tft.setTextSize(4);
    tft.setTextColor(YELLOW);
    tft.setCursor(40, 60);
    tft.print("T&P Valve Tester");
    
    // Draw subtitle
    tft.setTextSize(2);
    tft.setTextColor(WHITE);
    tft.setCursor(100, 140);
    tft.print("Pressure & Temperature");
    tft.setCursor(150, 170);
    tft.print("Testing System");
    
    // Initialize and draw the splash screen start button
    start_btn.initButton(&tft, 240, 230, 240, 60, WHITE, BLUE, WHITE, "START", 2);
    start_btn.drawButton();
}

void drawStartScreen() {
    tft.fillScreen(BLACK);
    
    // Add warning message above the button
    tft.setTextSize(2);
    tft.setTextColor(YELLOW);
    tft.setCursor(80, 70);
    tft.print("Please ensure that the valve");
    tft.setCursor(100, 100);
    tft.print("is installed correctly.");
    
    // Initialize and draw the start button
    start_btn.initButton(&tft, 240, 200, 240, 80, WHITE, GREEN, BLACK, "PROCEED", 2);
    start_btn.drawButton();
}

void drawTestSelectionScreen() {
    tft.fillScreen(BLACK);
    
    // Draw header
    tft.setTextSize(3);
    tft.setTextColor(WHITE);
    tft.setCursor(70, 50);
    tft.print("Select Test Type");
    
    // Initialize and draw test buttons
    pressure_btn.initButton(&tft, 240, 120, 340, 70, WHITE, BLUE, WHITE, "PSI TEST", 2);
    temp_btn.initButton(&tft, 240, 220, 340, 70, WHITE, RED, WHITE, "TEMP TEST", 2);
    
    pressure_btn.drawButton();
    temp_btn.drawButton();
}

void drawTestingScreen(bool isPressureTest) {
    tft.fillScreen(BLACK);
    
    // Draw header
    tft.setTextSize(2);
    tft.setTextColor(WHITE);
    tft.setCursor(50, 20);
    if (isPressureTest) {
        tft.print("PRESSURE TEST IN PROGRESS");
    } else {
        tft.print("TEMPERATURE TEST IN PROGRESS");
    }
    
    // Draw separator line
    tft.drawFastHLine(0, 50, 480, WHITE);
    
    // Initialize reset button
    reset_btn.initButton(&tft, 400, 290, 150, 50, WHITE, RED, BLACK, "ABORT", 2);
    reset_btn.drawButton();
    
    if (isPressureTest) {
        // Air Tank Section
        tft.setTextSize(2);
        tft.setTextColor(WHITE);
        tft.setCursor(20, 60);
        tft.print("Air Tank:");

        tft.setTextSize(2);
        tft.setCursor(30, 90);
        tft.setTextColor(CYAN);
        tft.print("Pressure:");
    } else {
        // Water Tank Section
        tft.setTextSize(2);
        tft.setTextColor(WHITE);
        tft.setCursor(20, 60);
        tft.print("Water Tank:");

        tft.setTextSize(2);
        tft.setCursor(30, 90);
        tft.setTextColor(YELLOW);
        tft.print("Temperature:");
    }
}

void drawResultsScreen() {
    tft.fillScreen(BLACK);
    
    // Draw header
    tft.setTextSize(3);
    tft.setTextColor(WHITE);
    tft.setCursor(160, 40);
    tft.print("RESULT");
    
    // Draw separator line
    tft.drawFastHLine(0, 70, 480, WHITE);
    
    // Draw result text
    tft.setTextSize(2);
    if (testSuccess) {
        tft.setTextColor(GREEN);
    } else {
        tft.setTextColor(RED);
    }
    
    // Format and display the test result
    // Break result text into multiple lines if needed
    int yPos = 100;
    int maxCharsPerLine = 40;  // Approximate max chars per line at text size 2
    
    for (int i = 0; i < testResult.length(); i += maxCharsPerLine) {
        String line = testResult.substring(i, min(i + maxCharsPerLine, (int)testResult.length()));
        
        // Try to break at a space if possible
        if (i + maxCharsPerLine < testResult.length() && line.charAt(line.length() - 1) != ' ') {
            int lastSpace = line.lastIndexOf(' ');
            if (lastSpace != -1) {
                line = line.substring(0, lastSpace);
                i = i - (maxCharsPerLine - lastSpace);  // Adjust i to account for the shorter line
            }
        }
        
        tft.setCursor(20, yPos);
        tft.print(line);
        yPos += 30;  // Move down for next line
    }
    
    // Draw question
    tft.setTextColor(WHITE);
    tft.setCursor(80, 200);
    tft.print("Run another test?");
    
    // Initialize and draw yes/no/done buttons
    yes_btn.initButton(&tft, 120, 250, 120, 60, WHITE, GREEN, BLACK, "YES", 2);
    no_btn.initButton(&tft, 240, 250, 120, 60, WHITE, RED, BLACK, "NO", 2);
    done_btn.initButton(&tft, 360, 250, 120, 60, WHITE, BLUE, BLACK, "DONE", 2);
    
    yes_btn.drawButton();
    no_btn.drawButton();
    done_btn.drawButton();
}

// Reset Arduino function
void resetArduino() {
    // Jump to address 0 (reset vector)
    asm volatile ("jmp 0");
}

// Read pressure sensor value and convert to PSI
float readPressure(int pin) {
    if (pin == PRESSURE_SENSOR_PIN) {
        // Read pressure from the first analog sensor
        int sensorValue = analogRead(pin);
        float voltage = (sensorValue / (float)ADC_RESOLUTION) * SENSOR_MAX_VOLTAGE;
        float pressure = ((voltage - VOLTAGE_OFFSET) / (SENSOR_MAX_VOLTAGE - VOLTAGE_OFFSET)) * SENSOR_MAX_PRESSURE;
        return pressure < 0 ? 0.0 : (pressure > SENSOR_MAX_PRESSURE ? SENSOR_MAX_PRESSURE : pressure);
    } else {
        // Read pressure from BMP180/085
        float temp = bmp.readTemperature(); // Read temperature first
        float pressure = bmp.readPressure(); // Read pressure in Pa
        
        if (pressure != 0) {
            return (pressure / 100.0) * 0.0145038; // Convert Pa to PSI
        }
        return -1; // Return error value if reading fails
    }
}

// Read temperature in Celsius with error handling
float readTemperatureC() {
    tempSensor.requestTemperatures();
    float temp = tempSensor.getTempCByIndex(0);
    
    // Check for common error values
    if (temp == -127.0 || temp == 200.0) {
        // Return special error indicator
        return TEMP_ERROR_VALUE; // Special value to indicate error
    }
    
    return temp;
}

// Read temperature in Celsius with error handling using the second sensor
float readSecondTemperatureC() {
    int temperature = dht11.readTemperature();
    if (temperature != DHT11::ERROR_CHECKSUM && temperature != DHT11::ERROR_TIMEOUT) {
        return (float)temperature;  // Convert to float but keep Celsius value
    }
    return TEMP_ERROR_VALUE;
}

// Convert Celsius to Fahrenheit
float convertToFahrenheit(float tempC) {
    return (tempC * 9.0 / 5.0) + 32.0;
}

// Update TFT display with new pressure reading
void updatePressureDisplay(float pressure) {
    tft.setTextSize(2);
    
    // Air Tank Pressure
    tft.fillRect(180, 90, 100, 30, BLACK);
    tft.setCursor(180, 90);
    tft.setTextColor(CYAN);
    tft.print(pressure, 1);
    tft.print(" psi");
    
    // BMP180 Pressure
    tft.setCursor(30, 120);
    tft.setTextColor(WHITE);
    tft.print("Air Valve:");
    
    tft.fillRect(180, 120, 100, 30, BLACK);
    tft.setCursor(180, 120);
    tft.setTextColor(YELLOW);
    tft.print(currentAirValvePressure, 1);
    tft.print(" psi");
}

// Update TFT display with DS18B20 temperature readings
void updateTemperatureDisplay(float tempC1, float tempC2) {
    tft.setTextSize(2);
    tft.fillRect(180, 90, 250, 60, BLACK);  // Increased height to accommodate two readings
    tft.setCursor(180, 90);
    tft.setTextColor(YELLOW);
    
    // Display first sensor
    if (tempC1 == TEMP_ERROR_VALUE) {
        tft.setTextColor(RED);
        tft.print("Sensor 1: Error");
    } else {
        tft.print("S1: ");
        tft.print(tempC1, 1);
        tft.print("C/");
        tft.print(convertToFahrenheit(tempC1), 1);
        tft.print("F");
    }
    
    // Display second sensor
    tft.setCursor(180, 120);
    if (tempC2 == TEMP_ERROR_VALUE) {
        tft.setTextColor(RED);
        tft.print("Sensor 2: Error");
    } else {
        tft.print("S2: ");
        tft.print(tempC2, 1);
        tft.print("C/");
        tft.print(convertToFahrenheit(tempC2), 1);
        tft.print("F");
    }
}

// Trigger the alarm for alerts
void triggerAlarm() {
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);

    // Beep twice
    for (int i = 0; i < 1; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(1000);
        digitalWrite(BUZZER_PIN, LOW);
        delay(200);
        digitalWrite(BUZZER_PIN, HIGH);
        delay(100);
        digitalWrite(BUZZER_PIN, LOW);
        delay(100);
        digitalWrite(BUZZER_PIN, HIGH);
        delay(100);
        digitalWrite(BUZZER_PIN, LOW);
        delay(100);
    }
}

// Trigger the alarm for error release
void triggerAlarmError() {
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);

    // Beep twice
    for (int i = 0; i < 5; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(500);
        digitalWrite(BUZZER_PIN, LOW);
        delay(500);
    }
}

// Update TFT display when air tank releases pressure
void updateAirTankStatus(float pressure2, float pressureReading) {
    // Check if air tank T&P valve is releasing pressure
    if (pressure2 >= RELEASE_DETECTION_THRESHOLD) {
        // Store the pressure at which release occurred
        if (!lastPressureStored) {
            lastPressureReading = pressureReading;
            lastPressureStored = true;
            pressureReleaseDetected = true;
            releaseDetectionTime = millis(); // Record when release was detected
            
            // Show status message
            tft.setTextSize(2);
            if (lastPressureReading <= PRESSURE_LIMIT) {
                // Normal release message
                tft.fillRect(0, 170, 480, 40, GREEN);
                tft.setCursor(20, 180);
                tft.setTextColor(BLACK);
                tft.print("Air Tank Released at ");
                tft.print(lastPressureReading, 1);
                tft.print(" psi");
                
                // Create success result
                testResult = "PRESSURE TEST PASSED: Air tank released at " + String(lastPressureReading, 1) + " psi (below limit of " + String(PRESSURE_LIMIT, 1) + " psi).";
                testSuccess = true;

                // Start alarm
                triggerAlarm();
            } else {
                // Error release message
                tft.fillRect(0, 170, 480, 40, RED);
                tft.setCursor(20, 180);
                tft.setTextColor(WHITE);
                tft.print("AIR TANK ERROR ");
                tft.print(lastPressureReading, 1);
                tft.print(" psi!");
                
                // Create failure result
                testResult = "PRESSURE TEST FAILED: Air tank released at " + String(lastPressureReading, 1) + " psi (above limit of " + String(PRESSURE_LIMIT, 1) + " psi).";
                testSuccess = false;

                // Start alarm
                triggerAlarmError();
            }
        }
    }
    
    // Always check if we need to transition to results screen after detection
    if (lastPressureStored && currentState == TESTING_PRESSURE) {
        // If already detected a release, transition to results after a short delay
        if (millis() - releaseDetectionTime > 3000) {  // 3 second delay after detection
            currentState = RESULTS_SCREEN;
            stateStartTime = millis();
            drawResultsScreen();
        }
    }
}

// Update TFT display when water tank shows signs of release
void updateWaterTankStatus(float waterTemp, float secondTemp) {
    static bool errorMessageShown = false;
    static unsigned long errorStartTime = 0;
    static bool releaseMessageShown = false;
    static bool releaseDetected = false;

    // Print sensor values for debugging
    Serial.print("Water Tank Temps: ");
    Serial.print(waterTemp);
    Serial.print("F, ");
    Serial.print(secondTemp);
    Serial.println("F");

    // Check if either sensor has an error
    if (waterTemp <= TEMP_ERROR_VALUE || secondTemp <= TEMP_ERROR_VALUE) {
        // Reset all test flags
        waterTankReleaseDetected = false;
        waterHighTempNoReleaseError = false;
        waterTankReleaseTemp = 0;
        releaseDetected = false;
        releaseMessageShown = false;
        
        // Show error message if not already shown
        if (!errorMessageShown) {
            tft.setTextSize(2);
            tft.fillRect(0, 170, 480, 40, RED);
            tft.setCursor(20, 180);
            tft.setTextColor(WHITE);
            tft.print("SENSOR ERROR - Check connections");
            
            errorMessageShown = true;
            errorStartTime = millis();
            
            // Update test result
            testResult = "TEMPERATURE TEST FAILED: Temperature sensor error detected";
            testSuccess = false;

            // Trigger error alarm
            triggerAlarmError();
        }

        // Transition to results screen after delay if in test mode
        if (currentState == TESTING_TEMPERATURE && (millis() - errorStartTime > 3000)) {
            currentState = RESULTS_SCREEN;
            stateStartTime = millis();
            drawResultsScreen();
        }
        return;  // Exit function if there's a sensor error
    }

    // Reset error message flag if sensors are working
    errorMessageShown = false;

    // Only proceed with release detection if both sensors are working properly
    if (waterTemp != TEMP_ERROR_VALUE && secondTemp != TEMP_ERROR_VALUE) {
        float tempValve = secondTemp;
        bool releaseCondition = (tempValve >= WATER_TEMP_RELEASE);
        bool highTempNoRelease = ((waterTemp >= TEMPERATURE_LIMIT || secondTemp >= TEMPERATURE_LIMIT) && !releaseCondition);

        // Handle normal release condition
        if (releaseCondition && !waterTankReleaseDetected && !releaseMessageShown) {
            waterTankReleaseDetected = true;
            releaseMessageShown = true;
            waterTankReleaseTemp = max(waterTemp, secondTemp);
            releaseDetectionTime = millis();

            tft.setTextSize(2);
            tft.fillRect(0, 170, 480, 40, GREEN);
            tft.setCursor(20, 180);
            tft.setTextColor(BLACK);
            tft.print("Water Tank Released at ");
            tft.print(waterTankReleaseTemp, 1);
            tft.print("F");

            testResult = "TEMPERATURE TEST PASSED: Water tank released at " + 
                        String(waterTankReleaseTemp, 1) + "F (Valve temp: " + 
                        String(tempValve, 1) + "F)";
            testSuccess = true;

            triggerAlarm();
        }
        // Handle high temperature with no release error condition
        else if (highTempNoRelease && !waterHighTempNoReleaseError) {
            waterHighTempNoReleaseError = true;
            releaseDetectionTime = millis();
            
            tft.setTextSize(2);
            tft.fillRect(0, 170, 480, 40, RED);
            tft.setCursor(20, 180);
            tft.setTextColor(WHITE);
            tft.print("ERROR: High temp without release!");

            testResult = "TEMPERATURE TEST FAILED: High temperature (" + 
                        String(max(waterTemp, secondTemp), 1) + "F) without valve release";
            testSuccess = false;

            triggerAlarmError();
        }

        // Transition to results screen after detection and delay
        if ((waterTankReleaseDetected || waterHighTempNoReleaseError) && 
            currentState == TESTING_TEMPERATURE && 
            (millis() - releaseDetectionTime > 3000)) {
            currentState = RESULTS_SCREEN;
            stateStartTime = millis();
            drawResultsScreen();
        }
    }

    // Debug output
    Serial.print("Release Detected: ");
    Serial.println(waterTankReleaseDetected ? "Yes" : "No");
    Serial.print("Error State: ");
    Serial.println(errorMessageShown ? "Yes" : "No");
}

// Control relay based on air tank pressure - only during pressure test
void controlPressureRelay(float pressure) {
    // Check if pressure limit was exceeded
    if (pressure >= PRESSURE_LIMIT && !pressureLimitExceeded) {
        pressureLimitExceeded = true;
        
        // Show error message
        tft.setTextSize(2);
        tft.fillRect(0, 170, 480, 40, RED);
        tft.setCursor(20, 180);
        tft.setTextColor(WHITE);
        tft.print("PRESSURE LIMIT EXCEEDED: ");
        tft.print(pressure, 1);
        tft.print(" psi!");
        
        // Create failure result
        testResult = "PRESSURE TEST FAILED: Pressure limit exceeded (" + String(pressure, 1) + " psi) without valve release.";
        testSuccess = false;
        
        // Transition to results screen after a delay
        if (currentState == TESTING_PRESSURE) {
            // Set timer to transition to results
            releaseDetectionTime = millis();
            lastPressureStored = true;  // Use this flag to initiate transition
            
            // Once we've exceeded limit and recorded it, we'll transition to results after delay
            if (millis() - releaseDetectionTime > 3000) {  // 3 second delay after detection
                currentState = RESULTS_SCREEN;
                stateStartTime = millis();
                drawResultsScreen();
            }
        }
    }
    
    // Only control relay if we're in the TESTING_PRESSURE state
    if (currentState == TESTING_PRESSURE) {
        if (pressure >= HIGH_PRESSURE_THRESHOLD || pressureLimitExceeded) {
            digitalWrite(RELAY_PIN, HIGH);  // Turn relay OFF
            
            // Trigger alarm
            triggerAlarmError();
        } else if (!pressureLimitExceeded) {
            digitalWrite(RELAY_PIN, LOW); // Only turn relay ON if limit not exceeded
        }
    } else {
        // Turn off relay when not in testing state
        digitalWrite(RELAY_PIN, HIGH);
    }
    
    // LED control can remain separate from relay control if desired
    if (pressure >= HIGH_PRESSURE_THRESHOLD) {
        digitalWrite(RED_LED_PIN, HIGH);
        digitalWrite(GREEN_LED_PIN, LOW);
    } else if (!pressureLimitExceeded) {  // Only reset LEDs if limit not exceeded
        digitalWrite(RED_LED_PIN, LOW);
        digitalWrite(GREEN_LED_PIN, HIGH);
    }
}

// Control relay based on water tank temperature - only during temperature test
void controlTemperatureRelay(float temperatureC) {
    // Skip relay control if temperature sensor has error
    if (temperatureC == TEMP_ERROR_VALUE) {
        // Just turn relay off when sensor has error
        if (currentState != TESTING_TEMPERATURE) {
            digitalWrite(RELAY_PIN2, HIGH); //  HIGH means the relay is turned off
        }
        return;
    }
    
    // Only control relay if we're in the TESTING_TEMPERATURE state
    if (currentState == TESTING_TEMPERATURE) {
        if (temperatureC >= TEMPERATURE_LIMIT) {
            delay(5000);
            digitalWrite(RELAY_PIN2, HIGH);
        } else {
            digitalWrite(RELAY_PIN2, LOW);
        }
    } else {
        // Turn off relay when not in testing state
        digitalWrite(RELAY_PIN2, HIGH);
    }
    
    // LED control can remain separate from relay control if desired
    if (temperatureC >= TEMPERATURE_LIMIT) {
        digitalWrite(RED_LED_PIN, HIGH);
        digitalWrite(GREEN_LED_PIN, LOW);
    } else {
        digitalWrite(RED_LED_PIN, LOW);
        digitalWrite(GREEN_LED_PIN, HIGH);
    }
}

void performPressureTest() {
    // Read pressure sensors
    currentAirPressure = readPressure(PRESSURE_SENSOR_PIN);
    currentAirValvePressure = readPressure(-1); // Use -1 for BMP180 reading

    // Update display
    updatePressureDisplay(currentAirPressure);

    // Check for pressure release
    updateAirTankStatus(currentAirValvePressure, currentAirPressure);

    // Control relay
    controlPressureRelay(currentAirPressure);
}

void performTemperatureTest() {
    float waterTempC = readTemperatureC();
    float waterValveTempC = readSecondTemperatureC();
    
    // Update display
    updateTemperatureDisplay(waterTempC, waterValveTempC);
    
    // Pass Celsius values directly instead of converting to Fahrenheit
    updateWaterTankStatus(waterTempC, waterValveTempC);
    
    // Control relay
    controlTemperatureRelay(waterTempC);
}

void loop() {
    unsigned long currentTime = millis();
    bool down = Touch_getXY();  // Get touch input
    
    // State machine
    switch (currentState) {
        case SPLASH_SCREEN:
        // Check for button press instead of using a timer
        start_btn.press(down && start_btn.contains(pixel_x, pixel_y));
    
        if (start_btn.justReleased()) {
            start_btn.drawButton();
        }
    
        if (start_btn.justPressed()) {
            start_btn.drawButton(true);  // Show pressed state
            delay(200);  // Visual feedback
        
            // Transition to start screen
            currentState = START_SCREEN;
            stateStartTime = currentTime;
            drawStartScreen();
        }
        break;
            
        case START_SCREEN:
            // Check for button press
            start_btn.press(down && start_btn.contains(pixel_x, pixel_y));
            
            if (start_btn.justReleased()) {
                start_btn.drawButton();
            }
            
            if (start_btn.justPressed()) {
                start_btn.drawButton(true);  // Show pressed state
                delay(200);  // Visual feedback
                
                // Transition to test selection
                currentState = TEST_SELECTION;
                stateStartTime = currentTime;
                drawTestSelectionScreen();
            }
            break;
            
        case TEST_SELECTION:
            // Check for button presses
            pressure_btn.press(down && pressure_btn.contains(pixel_x, pixel_y));
            temp_btn.press(down && temp_btn.contains(pixel_x, pixel_y));
            
            if (pressure_btn.justPressed()) {
                pressure_btn.drawButton(true);  // Show pressed state
                delay(200);  // Visual feedback

                // Start pressure test
                currentState = TESTING_PRESSURE;
                stateStartTime = currentTime;

                // Reset test variables
                lastPressureStored = false;
                pressureReleaseDetected = false;
                pressureLimitExceeded = false;  // Reset the pressure limit flag

                // Ensure temperature relay is off when starting pressure test
                digitalWrite(RELAY_PIN, HIGH);

                drawTestingScreen(true);  // Draw pressure test screen
            }
            
            if (temp_btn.justPressed()) {
                temp_btn.drawButton(true);  // Show pressed state
                delay(200);  // Visual feedback
    
                // Start temperature test
                currentState = TESTING_TEMPERATURE;
                stateStartTime = currentTime;
    
                // Reset test variables
                waterTankReleaseDetected = false;
                waterHighTempNoReleaseError = false;
    
                // Ensure pressure relay is off when starting temperature test
                digitalWrite(RELAY_PIN, HIGH);
    
                drawTestingScreen(false);  // Draw temperature test screen
            }
            break;
            
        case TESTING_PRESSURE:
            // Check for reset/abort button
            reset_btn.press(down && reset_btn.contains(pixel_x, pixel_y));
            
            // if (reset_btn.justReleased()) {
            //     reset_btn.drawButton();
            // }
            
            if (reset_btn.justPressed()) {
                reset_btn.drawButton(true);  // Show pressed state
                delay(200);  // Visual feedback
    
                // Turn off both relays when aborting a test
                digitalWrite(RELAY_PIN, HIGH);
                digitalWrite(RELAY_PIN2, HIGH);
    
                // Abort test and return to selection
                currentState = TEST_SELECTION;
                stateStartTime = currentTime;
                drawTestSelectionScreen();
            } else {
                // Perform pressure test
                performPressureTest();
            }
            break;
            
        case TESTING_TEMPERATURE:
            // Check for reset/abort button
            reset_btn.press(down && reset_btn.contains(pixel_x, pixel_y));
            
            // if (reset_btn.justReleased()) {
            //     reset_btn.drawButton();
            // }
            
            if (reset_btn.justPressed()) {
                reset_btn.drawButton(true);  // Show pressed state
                delay(200);  // Visual feedback
    
                // Turn off both relays when aborting a test
                digitalWrite(RELAY_PIN, HIGH);
                digitalWrite(RELAY_PIN2, HIGH);
    
                // Abort test and return to selection
                currentState = TEST_SELECTION;
                stateStartTime = currentTime;
                drawTestSelectionScreen();
            } else {
                // Perform temperature test
                performTemperatureTest();
            }
            break;
            
        case RESULTS_SCREEN:
            // Check for yes/no/done buttons
            yes_btn.press(down && yes_btn.contains(pixel_x, pixel_y));
            no_btn.press(down && no_btn.contains(pixel_x, pixel_y));
            done_btn.press(down && done_btn.contains(pixel_x, pixel_y));
    
            if (yes_btn.justReleased()) {
                yes_btn.drawButton();
            }       
    
            if (no_btn.justReleased()) {
                no_btn.drawButton();
            }
    
            if (done_btn.justReleased()) {
                done_btn.drawButton();
            }
    
            if (yes_btn.justPressed()) {
                yes_btn.drawButton(true);  // Show pressed state
                delay(200);  // Visual feedback
        
                // Go back to test selection
                currentState = TEST_SELECTION;
                stateStartTime = currentTime;
                drawTestSelectionScreen();
            }
    
            if (no_btn.justPressed()) {
                no_btn.drawButton(true);  // Show pressed state
                delay(200);  // Visual feedback

                // Turn off both relays when ending testing
                digitalWrite(RELAY_PIN, HIGH);
                digitalWrite(RELAY_PIN2, HIGH);
        
                // Go back to start screen
                currentState = START_SCREEN;
                stateStartTime = currentTime;
                drawStartScreen();
            }
    
            if (done_btn.justPressed()) {
                done_btn.drawButton(true);  // Show pressed state
                delay(200);  // Visual feedback

                // Turn off both relays when ending testing
                digitalWrite(RELAY_PIN, HIGH);
                digitalWrite(RELAY_PIN2, HIGH);
        
                // End testing and return to splash screen
                currentState = SPLASH_SCREEN;
                stateStartTime = currentTime;
                drawSplashScreen();
            }
            break;
            
            // Check for yes/no buttons
            yes_btn.press(down && yes_btn.contains(pixel_x, pixel_y));
            no_btn.press(down && no_btn.contains(pixel_x, pixel_y));
            
            if (yes_btn.justReleased()) {
                yes_btn.drawButton();
            }
            
            if (no_btn.justReleased()) {
                no_btn.drawButton();
            }
            
            if (yes_btn.justPressed()) {
                yes_btn.drawButton(true);  // Show pressed state
                delay(200);  // Visual feedback
                
                // Go back to test selection
                currentState = TEST_SELECTION;
                stateStartTime = currentTime;
                drawTestSelectionScreen();
            }
            
            if (no_btn.justPressed()) {
                no_btn.drawButton(true);  // Show pressed state
                delay(200);  // Visual feedback
                
                // Go back to start screen
                currentState = START_SCREEN;
                stateStartTime = currentTime;
                drawStartScreen();
            }
            break;
    }
    
    // Non-blocking sensor reading regardless of state
    if (currentTime - lastSensorReadTime >= SENSOR_READ_INTERVAL) {
        // Read pressure sensors
        float airPressure = readPressure(PRESSURE_SENSOR_PIN);
        
        // Read BMP180 values (original readings)
        float bmpTempC = bmp.readTemperature();
        float bmpPressurePa = bmp.readPressure();
        float airValvePressure = (bmpPressurePa / 100.0) * 0.0145038; // Convert to PSI
        
        // Read primary temperature sensor
        float waterTempC = readTemperatureC();
        
        // Read DHT11 (respecting minimum interval)
        float waterValveTempC;
        if (currentTime - lastDHTReadTime >= DHT_MIN_INTERVAL) {
            waterValveTempC = readSecondTemperatureC();
            lastDHTReadTime = currentTime;
        }
        
        // Convert temperatures to Fahrenheit for display
        float waterTempF = (waterTempC != TEMP_ERROR_VALUE) ? convertToFahrenheit(waterTempC) : waterTempC;
        float waterValveTempF = (waterValveTempC != TEMP_ERROR_VALUE) ? convertToFahrenheit(waterValveTempC) : waterValveTempC;
    
        // Log readings to serial with BMP180 original values
        Serial.print("Air Tank Pressure: ");
        Serial.print(airPressure);
        Serial.print(" psi | Air Valve: ");
        Serial.print(airValvePressure);
        Serial.print(" psi (");
        Serial.print(bmpPressurePa);
        Serial.print(" Pa, ");
        Serial.print(bmpTempC);
        Serial.print("°C) | Water Tank: ");
        Serial.print(waterTempC);
        Serial.print("°C/");
        Serial.print(waterTempF);
        Serial.print("F | Water Valve: ");
        Serial.print(waterValveTempC);
        Serial.print("°C/");
        Serial.print(waterValveTempF);
        Serial.println("F");
    
        // Update global variables
        currentAirPressure = airPressure;
        currentAirValvePressure = airValvePressure;
        currentWaterTempC = waterTempC;
    
        lastSensorReadTime = currentTime;
    }
    
    // Maintain water temperature at HIGH_TEMP_THRESHOLD value when not testing
    if (currentState != TESTING_TEMPERATURE) {
        float currentTemp = readTemperatureC();
        if (currentTemp != TEMP_ERROR_VALUE) {
            if (currentTemp >= HIGH_TEMP_THRESHOLD) {
                digitalWrite(RELAY_PIN2, HIGH);  // Turn off heater
            } else if (currentTemp <= MINIMUM_TEMPERATURE) {
                digitalWrite(RELAY_PIN2, LOW);   // Turn on heater
            }
        }
    }
}