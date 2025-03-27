#include <MCUFRIEND_kbv.h>
#include <Adafruit_GFX.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define PRESSURE_SENSOR_PIN A15  // First Pressure Sensor (Tank Pressure)
#define SECOND_PRESSURE_SENSOR_PIN A14  // Second Pressure Sensor (T&P Valve)
#define RELAY_PIN 24            // Relay control for pressure
#define RELAY_PIN2 25           // Relay control for temperature
#define BUZZER_PIN 28           // Buzzer
#define GREEN_LED_PIN 26        // Green LED (default ON)
#define RED_LED_PIN 27          // Red LED
#define TEMP_SENSOR_PIN 22      // DS18B20 Data Pin

MCUFRIEND_kbv tft;
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);

#define WHITE  0xFFFF
#define BLACK  0x0000
#define RED    0xF800
#define GREEN  0x07E0
#define BLUE   0x001F
#define CYAN   0x07FF
#define YELLOW 0xFFE0
#define ORANGE 0xFD20

const float SENSOR_MAX_PRESSURE = 300.0; // Max sensor pressure (psi)
const float SENSOR_MAX_VOLTAGE = 5.0;    // Sensor output at max pressure
const float VOLTAGE_OFFSET = 0.5;        // Sensor offset voltage
const int ADC_RESOLUTION = 1023;         // ADC resolution

float lastPressureReading = 0.0; // Store last valid pressure from the first sensor
bool lastPressureStored = false; // Flag to store pressure only once

void setup() {
    Serial.begin(9600);
    
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(RELAY_PIN2, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);

    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(RELAY_PIN2, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(RED_LED_PIN, LOW);

    tempSensor.begin();

    uint16_t ID = tft.readID();
    tft.begin(ID);
    tft.setRotation(1);
    tft.fillScreen(BLACK);

    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.setCursor(50, 20);
    tft.print("Sensor Readings");

    tft.setCursor(30, 80);
    tft.setTextColor(CYAN);
    tft.print("Pressure:");

    tft.setCursor(30, 120);
    tft.setTextColor(YELLOW);
    tft.print("Temperature:");
}

// Read pressure sensor value and convert to PSI
float readPressure(int pin) {
    int sensorValue = analogRead(pin);
    float voltage = (sensorValue / (float)ADC_RESOLUTION) * SENSOR_MAX_VOLTAGE;
    float pressure = ((voltage - VOLTAGE_OFFSET) / (SENSOR_MAX_VOLTAGE - VOLTAGE_OFFSET)) * SENSOR_MAX_PRESSURE;
    return pressure;
}

// Read temperature in Celsius
float readTemperatureC() {
    tempSensor.requestTemperatures();
    return tempSensor.getTempCByIndex(0);
}

// Convert Celsius to Fahrenheit
float convertToFahrenheit(float tempC) {
    return (tempC * 9.0 / 5.0) + 32.0;
}

// Update TFT display with new pressure reading
void updatePressureDisplay(float pressure) {
    tft.fillRect(180, 80, 100, 30, BLACK);
    tft.setCursor(180, 80);
    tft.setTextColor(CYAN);
    tft.print(pressure, 1);
    tft.print(" psi");
}

// Update TFT display with new temperature readings
void updateTemperatureDisplay(float tempC, float tempF) {
    tft.fillRect(180, 120, 100, 30, BLACK);
    tft.setCursor(180, 120);
    tft.setTextColor(YELLOW);
    tft.print(tempC, 1);
    tft.print(" C");

    tft.fillRect(180, 140, 100, 30, BLACK);
    tft.setCursor(180, 140);
    tft.setTextColor(YELLOW);
    tft.print(tempF, 1);
    tft.print(" F");
}

// Update TFT display when tank releases pressure
void updateTankStatus(float pressure2, float lastPressureReading) {
    if (pressure2 >= 2.0 && lastPressureReading <= 160) { // Detect release at around 2 psi
        tft.setTextSize(2);  // Increase font size

        tft.fillRect(0, 220, 500, 40, GREEN);
        tft.setCursor(50, 230);
        tft.setTextColor(BLACK);
        tft.print("Tank Status:");
        tft.setCursor(200, 230);
        tft.setTextColor(BLACK);
        tft.print("Released at ");
        tft.print(lastPressureReading, 1);
        tft.print(" psi");

        Serial.print("Tank Released at: ");
        Serial.print(lastPressureReading);
        Serial.println(" psi");

    } else if (pressure2 >= 2.0 && lastPressureReading > 160) {
      tft.setTextSize(2);  // Increase font size

        tft.fillRect(0, 220, 500, 40, RED);
        tft.setCursor(30, 230);
        tft.setTextColor(WHITE);
        tft.print("RELEASE ERROR ");
        tft.print(lastPressureReading, 1);
        tft.print(" psi");
        tft.print(" EXEEDS LIMIT!");

        Serial.print("Released at: ");
        Serial.print(lastPressureReading);
        Serial.println(" psi");
    }
}

// Control relay based on pressure
void controlPressureRelay(float pressure) {
    if (pressure >= 10.0) {
        digitalWrite(RELAY_PIN, HIGH);
        digitalWrite(RED_LED_PIN, HIGH);
        digitalWrite(GREEN_LED_PIN, LOW);

        for (int i = 0; i < 2; i++) {
            digitalWrite(BUZZER_PIN, HIGH);
            delay(200);
            digitalWrite(BUZZER_PIN, LOW);
            delay(200);
        }
    } else {
        digitalWrite(RELAY_PIN, LOW);
        digitalWrite(RED_LED_PIN, LOW);
        digitalWrite(GREEN_LED_PIN, HIGH);
    }
}

// Control relay based on temperature
void controlTemperatureRelay(float temperatureC) {
    if (temperatureC >= 33.00) {
        digitalWrite(RELAY_PIN2, HIGH);
        digitalWrite(RED_LED_PIN, HIGH);
        digitalWrite(GREEN_LED_PIN, LOW);

        for (int i = 0; i < 2; i++) {
            digitalWrite(BUZZER_PIN, HIGH);
            delay(200);
            digitalWrite(BUZZER_PIN, LOW);
            delay(200);
        }
    } else {
        digitalWrite(RELAY_PIN2, LOW);
        digitalWrite(RED_LED_PIN, LOW);
        digitalWrite(GREEN_LED_PIN, HIGH);
    }
}

void loop() {
    float pressure = readPressure(PRESSURE_SENSOR_PIN);
    float pressure2 = readPressure(SECOND_PRESSURE_SENSOR_PIN);
    float temperatureC = readTemperatureC();
    float temperatureF = convertToFahrenheit(temperatureC);

    Serial.print("Pressure: ");
    Serial.print(pressure);
    Serial.print(" psi | Temperature: ");
    Serial.print(temperatureC);
    Serial.println(" °C");

    updatePressureDisplay(pressure);
    updateTemperatureDisplay(temperatureC, temperatureF);

    // If release is detected (pressure2 >= 2.0), store last pressure reading only once
    if (pressure2 >= 30.0 && !lastPressureStored) {    //  Change the psi value with the right value
        lastPressureReading = pressure;
        lastPressureStored = true;  // Prevent storing again
        updateTankStatus(pressure2, lastPressureReading);
    } 
    
    // Reset flag when release is not detected
    if (pressure2 < 30.0) {    //  Change the psi value with the right value
        lastPressureStored = false;  
    }

    controlPressureRelay(pressure);
    controlTemperatureRelay(temperatureC);

    delay(500);
}

