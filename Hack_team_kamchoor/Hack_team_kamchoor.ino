//RECEIVER ESP32 CODE
// Include the required libraries
#include <MD_MAX72xx.h>
#include <SPI.h>

//Pin definitions for the LED matrix (MAX7219)
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES   3
#define CS_PIN        5  // Chip select pin for SPI

// Create a new instance of the MD_MAX72XX class for hardware SPI
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

//Pin definitions for the sensors
#define LDR_LEFT_PIN   34  // LDR controlling leftmost matrix
#define LDR_RIGHT_PIN  35  // LDR controlling rightmost matrix
#define POT_PIN        32  // Potentiometer pin (ADC)

//LDR Thresholds
#define LDR_LEFT_THRESHOLD   1500
#define LDR_RIGHT_THRESHOLD  1500

//Manual Intensity Timer
unsigned long manualSetTime = 0; // When manual intensity was last set
bool manualMode = false;

//======================================================================
// SETUP FUNCTION
//======================================================================
void setup() {
  Serial.begin(115200);
  mx.begin();
  mx.clear();

  Serial.println("\n--- Matrix Control Ready ---");
  Serial.println("Send 1-5 to set brightness manually (locks for 10s).");
  Serial.println("--------------------------------");

  // Set an initial medium brightness
  setIntensity(3);
}

//======================================================================
// LOOP FUNCTION
//======================================================================
void loop() {
  // Part 1: Check for Serial Input to change brightness
  if (Serial.available() > 0) {
    char input = Serial.read();
    int level = input - '0';
    if (level >= 1 && level <= 5) {
      setIntensity(level);
      manualMode = true;
      manualSetTime = millis(); // Record time of manual input
    }
  }

  // Part 2: Handle potentiometer intensity (only if not in manual lock)
  if (manualMode && millis() - manualSetTime < 10000) {
    // Stay in manual mode for at least 10 seconds
  } else {
    manualMode = false; // Back to potentiometer mode
    int potValue = analogRead(POT_PIN);     // 0–4095 (ESP32 ADC)
    int level = map(potValue, 0, 4095, 1, 5); // Map to 1–5 intensity levels
    setIntensity(level);
  }

  //  Part 3: Read LDR sensors 
  int leftLdrValue = analogRead(LDR_LEFT_PIN);
  int rightLdrValue = analogRead(LDR_RIGHT_PIN);

  // Rule 1: The middle matrix (index 1) is ALWAYS ON.
  turnOnMatrix(1);

  // Rule 2: Control the LEFTMOST matrix (index 2) with the LEFT LDR.
  if (leftLdrValue < LDR_LEFT_THRESHOLD) {
    turnOffMatrix(2); // Bright light → OFF
  } else {
    turnOnMatrix(2);  // No light → ON
  }

  // Rule 3: Control the RIGHTMOST matrix (index 0) with the RIGHT LDR.
  if (rightLdrValue < LDR_RIGHT_THRESHOLD) {
    turnOffMatrix(0);
  } else {
    turnOnMatrix(0);
  }

  delay(10); // Stability delay
}

//======================================================================
// HELPER FUNCTIONS
//======================================================================

/**
 * @brief Sets the global intensity for all matrices.
 */
void setIntensity(int level) {
  int hardwareValue = 0;
  switch (level) {
    case 1: hardwareValue = 0;  break; // Dimmest
    case 2: hardwareValue = 3;  break; // Dim
    case 3: hardwareValue = 7;  break; // Medium
    case 4: hardwareValue = 11; break; // Bright
    case 5: hardwareValue = 15; break; // Brightest
  }
  Serial.print("Intensity set to level ");
  Serial.println(level);
  mx.control(MD_MAX72XX::INTENSITY, hardwareValue);
}

/**
 * @brief Turns off all LEDs on a single specified matrix.
 */
void turnOffMatrix(int deviceIndex) {
  mx.clear(deviceIndex);
}

/**
 * @brief Turns on all LEDs on a single specified matrix.
 */
void turnOnMatrix(int deviceIndex) {
  for (int row = 0; row < 8; row++) {
    mx.setRow(deviceIndex, row, 0xFF);
  }
}