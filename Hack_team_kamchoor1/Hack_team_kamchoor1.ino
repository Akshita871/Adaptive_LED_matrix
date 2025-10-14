// SENDER ESP32 CODE

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <BluetoothSerial.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run make menuconfig to enable it.
#endif

// ======================= Global Definitions =======================
// Wi-Fi Credentials
const char* ssid = "Kanak's GALAXY S24FE";
const char* password = "112233445566";

// OpenWeatherMap API
const char* openWeatherMapApiKey = "0fe99458cc8e078c460decdae76b73a7";
String weatherApiUrl = "http://api.openweathermap.org/data/2.5/weather?";

// LED Matrix (MAX7219)
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 3
#define CLK_PIN 18
#define DATA_PIN 23
#define CS_PIN 5
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// PWM Settings for LED Brightness
#define LED_PWM_PIN 27  // Choose an unused GPIO pin for PWM
#define LED_PWM_CHANNEL 0
#define LED_PWM_FREQ 5000
#define LED_PWM_RESOLUTION 8 // 8-bit resolution (0-255)

// Bluetooth
BluetoothSerial SerialBT;

// Global Shared State Variables
volatile double latitude = 0.0;
volatile double longitude = 0.0;
bool weather_data_available = false;
int display_intensity = 0; // New variable for dynamic intensity

// ======================= Helper Functions =======================
void setLedIntensity(int intensity) {
  // Map the 0-15 scale to the 0-255 PWM scale
  int pwm_value = map(intensity, 0, 15, 0, 255);
  ledcWrite(LED_PWM_CHANNEL, pwm_value);
}

void displayNumber(int number) {
  mx.clear();
  char buffer[3]; // A buffer to hold the number as a string (max 2 digits + null terminator)
  itoa(number, buffer, 10); // Convert integer to string in base 10

  // Display each digit of the number
  int len = strlen(buffer);
  for (int i = 0; i < len; i++) {
    mx.setChar(MAX_DEVICES - len + i, buffer[i]);
  }
}

// ======================= Main Program Logic =======================
void setup() {
  Serial.begin(115200);
  
  // Set up PWM for brightness control
  ledcSetup(LED_PWM_CHANNEL, LED_PWM_FREQ, LED_PWM_RESOLUTION);
  ledcAttachPin(LED_PWM_PIN, LED_PWM_CHANNEL);
  
  mx.begin();
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");

  SerialBT.begin("ESP32_WEATHER");
  Serial.println("Bluetooth started. Ready to pair.");
  mx.clear();
  displayNumber(5); // Display initial intensity
}

void loop() {
  // Check for new GPS data from Bluetooth
  if (SerialBT.available()) {
    String gpsData = SerialBT.readStringUntil('\n');
    gpsData.trim();
    
    int commaIndex = gpsData.indexOf(',');
    if (commaIndex != -1) {
      String latStr = gpsData.substring(0, commaIndex);
      String lonStr = gpsData.substring(commaIndex + 1);

      latitude = latStr.toDouble();
      longitude = lonStr.toDouble();
      Serial.printf("Received GPS: %.4f, %.4f\n", latitude, longitude);
    }
  }

  // Only make an API call if valid GPS data is received
  if (latitude != 0.0 || longitude != 0.0) {
    HTTPClient http;
    String serverPath = weatherApiUrl + "lat=" + String(latitude, 4) + "&lon=" + String(longitude, 4) + "&appid=" + openWeatherMapApiKey;
    
    http.begin(serverPath.c_str());
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      String payload = http.getString();
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (!error && doc["weather"][0]["main"]) {
        weather_data_available = true;
        // Adjust intensity based on weather condition
        String weatherMain = doc["weather"][0]["main"].as<String>();
        weatherMain.toLowerCase();
        if (weatherMain.indexOf("clear") != -1) {
          display_intensity = 15;
        } else if (weatherMain.indexOf("rain") != -1 || weatherMain.indexOf("drizzle") != -1) {
          display_intensity = 5;
        } else if (weatherMain.indexOf("clouds") != -1 || weatherMain.indexOf("mist") != -1) {
          display_intensity = 10;
        } else {
          display_intensity = 8;
        }
      } else {
        weather_data_available = false;
        display_intensity = 5; // Default low intensity on error
      }
    } else {
      weather_data_available = false;
      display_intensity = 5; // Default low intensity on error
    }
    http.end();
  } else {
    // If no GPS data, set intensity to a low value
    display_intensity = 5;
  }

  // Set the LED intensity and display the intensity value
  setLedIntensity(display_intensity);
  displayNumber(display_intensity);

  delay(300000); // Wait 5 minutes before the next API call
}