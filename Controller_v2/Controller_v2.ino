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
#define HIGH_PRESSURE_THRESHOLD 50.0  // Threshold for pressure alarm (psi)
#define HIGH_TEMP_THRESHOLD 110.0      // Threshold for temperature alarm (°C)
#define RELEASE_DETECTION_THRESHOLD 1.86  // Threshold to detect pressure release (psi)
#define PRESSURE_LIMIT 50.0          // Maximum acceptable release pressure (psi)
#define WATER_HUMIDITY_THRESHOLD 96.0  // Threshold for high humidity in water tank
#define WATER_TEMP_DIFF_THRESHOLD 50.0  // Significant temp difference between sensors

// Display & Timing Constants
#define NORMAL_MESSAGE_DURATION 10000  // Duration to show normal messages (ms)
#define ERROR_MESSAGE_DURATION 60000   // Duration to show error messages (ms)
#define REFRESH_INTERVAL 500           // Display refresh interval (ms)
#define SENSOR_READ_INTERVAL 100       // Sensor reading interval (ms)
#define DHT_READ_RETRY_DELAY 1000      // Time between DHT read retries
// #define SPLASH_SCREEN_DURATION 5000    // Duration to show splash screen (ms)

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
DHT11 dht(DHT_PIN);  // Modified for DHT11.h library

// Button objects
Adafruit_GFX_Button reset_btn;
Adafruit_GFX_Button start_btn;
Adafruit_GFX_Button pressure_btn;
Adafruit_GFX_Button temp_btn;
Adafruit_GFX_Button yes_btn;
Adafruit_GFX_Button no_btn;
Adafruit_GFX_Button done_btn;

// Global variables for touch screen
int pixel_x, pixel_y;     // Touch coordinates

// Air tank variables
float lastPressureReading = 0.0;    // Store last valid pressure from the first sensor
bool lastPressureStored = false;    // Flag to store pressure only once
bool pressureReleaseDetected = false; // Flag for pressure release
unsigned long releaseDetectionTime = 0;
bool pressureLimitExceeded = false;

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
unsigned long splashScreenStartTime = 0;  // Splash screen timing

// Global variables for recent readings
float currentAirPressure = 0.0;
float currentAirValvePressure = 0.0;
float currentWaterTempC = 0.0;  
float currentWaterTempF = 0.0;
float currentHumidity = 0.0;
float currentDhtTempC = 0.0;
bool currentDhtSuccess = false;

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
bool readDHT(float &humidity, float &tempC);
void updatePressureDisplay(float pressure);
void updateTemperatureDisplay(float tempC, float tempF);
void updateDHTDisplay(float humidity, float tempC);
void updateAirTankStatus(float pressure2, float lastPressureReading);
void updateWaterTankStatus(float waterTemp, float dhtTemp, float humidity);
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
    pressure_btn.initButton(&tft, 240, 120, 340, 70, WHITE, BLUE, BLACK, "PSI TEST", 2);
    temp_btn.initButton(&tft, 240, 220, 340, 70, WHITE, RED, BLACK, "TEMP TEST", 2);
    
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

        tft.setTextSize(2);
        tft.setCursor(30, 120);
        tft.setTextColor(GREEN);
        tft.print("Humidity:");
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
    if (currentTime - lastDhtReadTime >= DHT_READ_RETRY_DELAY) {
        lastDhtReadTime = currentTime;

        int intHumidity, intTemp;
        
        // For DHT11.h library
        int result = dht.readTemperatureHumidity(intTemp, intHumidity);
        
        // Check if reading was successful
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
    tft.fillRect(180, 90, 100, 30, BLACK);
    tft.setCursor(180, 90);
    tft.setTextColor(CYAN);
    tft.print(pressure, 1);
    tft.print(" psi");
}

// Update TFT display with DS18B20 temperature readings
void updateTemperatureDisplay(float tempC, float tempF) {
    tft.setTextSize(2);
    tft.fillRect(180, 90, 250, 30, BLACK);
    tft.setCursor(180, 90);
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
    tft.fillRect(180, 120, 120, 30, BLACK);
    
    tft.setCursor(180, 120);
    tft.setTextColor(GREEN);
    
    if (humidity <= 0 || tempC <= 0) {  // Basic validation for DHT11.h library
        tft.print("Error");
    } else {
        tft.print(humidity, 1);
        tft.print("% ");
        tft.print(tempC, 1);
        tft.print("C");
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
void updateWaterTankStatus(float waterTemp, float dhtTemp, float humidity) {
    // Logic to detect water tank T&P valve release:
    // High humidity is a strong indicator
    bool releaseCondition = (humidity > WATER_HUMIDITY_THRESHOLD) && 
                     (dhtTemp > WATER_TEMP_DIFF_THRESHOLD);
    
    // Check for high temperature with no release
    bool highTempNoRelease = (waterTemp >= HIGH_TEMP_THRESHOLD) && !releaseCondition;
    
    // Handle normal release condition
    if (releaseCondition && !waterTankReleaseDetected) {
        waterTankReleaseDetected = true;
        waterTankReleaseTemp = waterTemp;
        
        // Display release information
        tft.setTextSize(2);
        tft.fillRect(0, 170, 480, 40, GREEN);
        tft.setCursor(20, 180);
        tft.setTextColor(BLACK);
        tft.print("Water Tank Released at ");
        tft.print(waterTankReleaseTemp, 1);
        tft.print("C");
        
        // Create success result
        testResult = "TEMPERATURE TEST PASSED: Water tank released at " + String(waterTankReleaseTemp, 1) + "°C.";
        testSuccess = true;

        triggerAlarm();
    }
    // Handle high temperature with no release error condition
    else if (highTempNoRelease && !waterHighTempNoReleaseError) {
        waterHighTempNoReleaseError = true;
        
        // Display error information
        tft.setTextSize(2);
        tft.fillRect(0, 170, 480, 40, RED);
        tft.setCursor(20, 180);
        tft.setTextColor(WHITE);
        tft.print("WATER TANK ERROR: High temp, no release!");
        
        // Create failure result
        testResult = "TEMPERATURE TEST FAILED: Water tank reached " + String(waterTemp, 1) + "°C.";
        testSuccess = false;

        // Trigger the alarm for error release
        triggerAlarmError();
    }
    
    // If already detected a release or error, transition to results after a short delay
    if ((waterTankReleaseDetected || waterHighTempNoReleaseError) && currentState == TESTING_TEMPERATURE) {
        if (millis() - stateStartTime > 3000) {  // 3 second delay after detection
            currentState = RESULTS_SCREEN;
            stateStartTime = millis();
            drawResultsScreen();
        }
    }
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
        
        // Trigger alarm
        triggerAlarmError();
        
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
            digitalWrite(RELAY_PIN2, LOW);
        }
        return;
    }
    
    // Only control relay if we're in the TESTING_TEMPERATURE state
    if (currentState == TESTING_TEMPERATURE) {
        if (temperatureC >= HIGH_TEMP_THRESHOLD) {
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
    if (temperatureC >= HIGH_TEMP_THRESHOLD) {
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
    currentAirValvePressure = readPressure(SECOND_PRESSURE_SENSOR_PIN);
    
    // Update display
    updatePressureDisplay(currentAirPressure);
    
    // Check for pressure release
    updateAirTankStatus(currentAirValvePressure, currentAirPressure);
    
    // Control relay
    controlPressureRelay(currentAirPressure);
}

void performTemperatureTest() {
    // Read water temperature sensors
    currentWaterTempC = readTemperatureC();
    currentWaterTempF = (currentWaterTempC == TEMP_ERROR_VALUE) ? 
                        TEMP_ERROR_VALUE : convertToFahrenheit(currentWaterTempC);
    
    // Read humidity sensor
    currentDhtSuccess = readDHT(currentHumidity, currentDhtTempC);
    
    // Update displays
    updateTemperatureDisplay(currentWaterTempC, currentWaterTempF);
    
    if (currentDhtSuccess) {
        updateDHTDisplay(currentHumidity, currentDhtTempC);
        
        // Check for water tank release
        if (currentWaterTempC != TEMP_ERROR_VALUE) {
            updateWaterTankStatus(currentWaterTempC, currentDhtTempC, currentHumidity);
        }
    } else {
        // Show error if DHT reading failed
        updateDHTDisplay(0, 0);
    }
    
    // Control temperature relay
    controlTemperatureRelay(currentWaterTempC);
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
                // Read all sensors for logging purposes even when not displaying
                float airPressure = readPressure(PRESSURE_SENSOR_PIN);
                float airValvePressure = readPressure(SECOND_PRESSURE_SENSOR_PIN);
                float waterTempC = readTemperatureC();
                float waterTempF = (waterTempC == TEMP_ERROR_VALUE) ? 
                        TEMP_ERROR_VALUE : convertToFahrenheit(waterTempC);
        
                float humidity = 0.0;
                float dhtTempC = 0.0;
                bool dhtSuccess = readDHT(humidity, dhtTempC);

                // Log readings to serial
                Serial.print("Air Tank Pressure: ");
                Serial.print(airPressure);
                Serial.print(" psi | Air Valve: ");
                Serial.print(airValvePressure);
                Serial.print(" psi | Water Temp: ");
        
                if (waterTempC == TEMP_ERROR_VALUE) {
                    Serial.println("ERROR - Sensor not connected");
                } else {
                    Serial.print(waterTempC);
                    Serial.println(" °C");
                }
        
                if (dhtSuccess) {
                    Serial.print("Water Tank DHT11: ");
                    Serial.print(dhtTempC);
                    Serial.print("°C | Humidity: ");
                    Serial.print(humidity);
                    Serial.println("%");
                } else {
                    Serial.println("DHT11 Reading Failed");
                }
        
                lastSensorReadTime = currentTime;
            }
}