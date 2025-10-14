# Adaptive_LED_matrix
Adaptive LED matrix Headlight System:


**Built with** ESP32{2}, ToF sensor, LDR sensor, 8x8 LED matrix


**Software**: Arduino IDE


******************************************************************
**Overview of the project:**

This document describes an Adaptive LED matrix system designed to dynamically modify the brightness and intensity of an LED matrix. The system utilizes a Time-of-Flight (ToF) sensor to detect the presence of approaching vehicles. Additionally, the LED brightness is adjusted in real time based on prevailing weather conditions, with this information being integrated via mobile GPS from an online weather API.



******************************************************************
**Features:**

1. The system detects approaching vehicles within a range of 15 cm, utilizing Time-of-Flight (ToF) and Light Dependent Resistor (LDR) sensors.
2. The LED matrix is dimmed by up to 90% in the direction of the detected approaching vehicle.
3. The intensity of the LED illumination is dynamically adjusted based on weather data accessed via a weather API, alongside mobile GPS information.



********************************************************************
**Transmitter (ESP32):**

1. The device connects to Wi-Fi and retrieves real-time weather data from OpenWeatherMap.
2. It receives GPS coordinates from a mobile device via Bluetooth.
3. The brightness of the LED is controlled dynamically using Pulse Width Modulation (PWM) in response to weather conditions:
   - Clear: Maximum brightness (255)
   - Cloudy/Mist: Medium brightness (15)
   - Rain/Drizzle: Low brightness (0)
4. The current brightness level is displayed on an 8×8 LED matrix.

**KEY CODE FEATURES:**

**// Fetch weather data**

HTTPClient http;

http.begin(serverPath.c_str());

int httpResponseCode = http.GET();



**// Adjust LED brightness based on weather**

if (weatherMain.indexOf("clear") != -1) display_intensity = 15;



**// PWM brightness control
**
ledcWrite(LED_PWM_CHANNEL, pwm_value);


***********************************************************************
**Receiver (ESP32):**

1. The device reads ambient light levels through LDR sensors.
2. It utilizes a potentiometer for manual adjustment of brightness.
3. It controls three 8×8 LED matrices:
   - The central matrix remains constantly illuminated.
   - The left and right matrices respond to readings from the LDR sensors.
4. The system supports a manual intensity mode via Serial input (1–5) for a duration of 10 seconds.

**KEY CODE FEATURE:**

**// Set intensity on MAX7219 LED matrices
**
mx.control(MD_MAX72XX::INTENSITY, hardwareValue);



**// LDR-based automatic control
**
if (leftLdrValue < LDR_LEFT_THRESHOLD) turnOffMatrix(2);



**// Manual override via Serial input**

if (Serial.available() > 0) { ... }


*************************************************************************

**System Flow Overview:**

1) The mobile device transmits GPS data via Bluetooth to the Transmitter, which is powered by the ESP32 microcontroller.
2) The Transmitter subsequently retrieves weather data from the OpenWeatherMap API.
3) Based on the acquired weather data, the Transmitter calculates the appropriate intensity for the LED displays.
4) The Receiver, also utilizing an ESP32, then adjusts the brightness of the LED matrix automatically. This adjustment can alternatively be influenced by a manual override, which includes inputs from a potentiometer and light-dependent resistor (LDR) readings. 
5) The LED matrices are designed to convey intensity levels and dynamically respond to variations in environmental conditions.


**********************************************************************

**Future work:**
1) Integration of the camera module and ML model to detect the size and type of vehicle approaching.
2) Extend the range and precision of ToF and LDR sensor.
3) Using MQTT for live update and monitoring of the system.
