// RECEIVER ESP32 CODE with ToF integration (VL53L0X)
// --------------------------------------------------
// Features kept: Manual (Serial 1–5, 10s lock), Pot-based base intensity,
// LDR-driven on/off per side, Center always ON
// New: ToF-based near-object dimming (~90%) on affected side when < 15 cm
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_VL53L0X.h>
// ---------- LED Matrix (MAX7219) ----------
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES   3
#define CS_PIN        5  // Chip select pin for SPI
// Device index mapping (left/center/right) for readability
#define DEV_RIGHT 0
#define DEV_CENTER 1
#define DEV_LEFT 2
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
// ---------- Sensors ----------
#define LDR_LEFT_PIN    34  // LDR controlling leftmost matrix
#define LDR_RIGHT_PIN   35  // LDR controlling rightmost matrix
#define POT_PIN         32  // Potentiometer pin (ADC)
// LDR thresholds (tuned to your setup)
#define LDR_LEFT_THRESHOLD   1500
#define LDR_RIGHT_THRESHOLD  1500
// ---------- ToF (VL53L0X) ----------
Adafruit_VL53L0X tof = Adafruit_VL53L0X();
#define TOF_ACTIVE                1        // set 0 to compile out ToF logic
#define TOF_NEAR_MM               150      // 15 cm threshold
#define TOF_DIM_FACTOR_NUM        1        // ~10% of base level
#define TOF_DIM_FACTOR_DEN        10
// ---------- Manual Intensity Lock ----------
unsigned long manualSetTime = 0; // When manual intensity was last set
bool manualMode = false;
// ---------- Helpers: intensity mapping ----------
/*
 * Intensity level 1–5 -> MAX7219 hardware intensity 0–15
 * Same mapping you used globally, but now we can do per-device, too.
 */
uint8_t levelToHwIntensity(int level) {
  switch (level) {
    case 1: return 0;   // Dimmest
    case 2: return 3;   // Dim
    case 3: return 7;   // Medium
    case 4: return 11;  // Bright
    case 5: return 15;  // Brightest
    default: return 7;  // Safe default
  }
}
/*
 * Clamp 1–5
 */
int clampLevel(int lvl) {
  if (lvl < 1) return 1;
  if (lvl > 5) return 5;
  return lvl;
}
/*
 * Set per-device intensity (1–5), independent of others
 */
void setDeviceIntensity(int deviceIndex, int level) {
  mx.control(deviceIndex, MD_MAX72XX::INTENSITY, levelToHwIntensity(clampLevel(level)));
}
/*
 * Turn off all LEDs on a specific matrix device
 */
void turnOffMatrix(int deviceIndex) {
  mx.clear(deviceIndex);
}

/*
 * Turn on all LEDs on a specific matrix device (all pixels lit)
 */
void turnOnMatrix(int deviceIndex) {
  for (int row = 0; row < 8; row++) {
    mx.setRow(deviceIndex, row, 0xFF);
  }
}
// SETUP
void setup() {
  Serial.begin(115200);
  mx.begin();
  mx.clear();
  Serial.println("\n--- Matrix Control + ToF Ready ---");
  Serial.println("Send 1-5 to set brightness manually (locks for 10s).");
  Serial.println("--------------------------------");
  // I2C + ToF init
  #if TOF_ACTIVE
  Wire.begin(); // use default SDA/SCL pins on ESP32
  if (!tof.begin()) {
    Serial.println("WARNING: VL53L0X not found. ToF dimming disabled.");
  } else {
    Serial.println("VL53L0X initialized.");
  }
  #endif
  // Initial medium intensities
  setDeviceIntensity(DEV_LEFT,   3);
  setDeviceIntensity(DEV_CENTER, 3);
  setDeviceIntensity(DEV_RIGHT,  3);
}
//======================================================================
// LOOP
//======================================================================
void loop() {
  // -------- 1) Manual Serial input 1–5 (locks for 10s) --------
  int baseLevel = 3; // will be overwritten if manual lock inactive
  if (Serial.available() > 0) {
    char input = Serial.read();
    int level = input - '0';
    if (level >= 1 && level <= 5) {
      baseLevel = level;
      manualMode = true;
      manualSetTime = millis();
      Serial.print("Manual base intensity set to ");
      Serial.println(baseLevel);
    }
  }
  // -------- 2) Pot-based base intensity (if manual not locked) --------
  if (manualMode && millis() - manualSetTime < 10000) {
    // keep previous manual base level during lock window
    // (we won't recompute baseLevel here; devices will keep last applied)
  } else {
    manualMode = false;
    int potValue = analogRead(POT_PIN);           // 0–4095 (ESP32 ADC)
    baseLevel   = map(potValue, 0, 4095, 1, 5);   // map to 1–5
  }
  // -------- 3) Read LDRs --------
  int leftLdrValue  = analogRead(LDR_LEFT_PIN);
  int rightLdrValue = analogRead(LDR_RIGHT_PIN);
  // Decide ON/OFF for side matrices based on LDR (original rules)
  bool leftShouldBeOn  = (leftLdrValue  >= LDR_LEFT_THRESHOLD);
  bool rightShouldBeOn = (rightLdrValue >= LDR_RIGHT_THRESHOLD);
  // Center matrix is ALWAYS ON (original rule)
  turnOnMatrix(DEV_CENTER);
  if (leftShouldBeOn)  turnOnMatrix(DEV_LEFT);  else turnOffMatrix(DEV_LEFT);
  if (rightShouldBeOn) turnOnMatrix(DEV_RIGHT); else turnOffMatrix(DEV_RIGHT);
  // -------- 4) ToF read + per-side dim logic --------
  // Base intensity for all three
  int leftLevel   = baseLevel;
  int centerLevel = baseLevel;
  int rightLevel  = baseLevel;
  #if TOF_ACTIVE
  VL53L0X_RangingMeasurementData_t measure;
  bool tofOk = false;
  // Some libs return true/false; Adafruit's uses rangingTest(&measure, debug)
  tof.rangingTest(&measure, false);
  if (measure.RangeStatus == 0) { // 0 means valid
    tofOk = true;
    uint16_t dist = measure.RangeMilliMeter;
    // Debug print
    // Serial.printf("ToF distance: %u mm\n", dist);
    if (dist < TOF_NEAR_MM) {
      // ToF says an object is near. Decide which side to dim by ~90%.
      // Heuristic from README: use LDR to infer side of approaching vehicle (headlight glare).
      // - If left LDR indicates glare (below threshold) -> dim LEFT
      // - Else if right LDR indicates glare -> dim RIGHT
      // - Else dim BOTH sides (fallback)
      auto dimmed = [&](int lvl) {
        int d = (lvl * TOF_DIM_FACTOR_NUM) / TOF_DIM_FACTOR_DEN;
        if (d < 1) d = 1; // never fully off via dimming
        return d;
      };
      bool leftGlare  = (leftLdrValue  < LDR_LEFT_THRESHOLD);
      bool rightGlare = (rightLdrValue < LDR_RIGHT_THRESHOLD);
      if (leftGlare && leftShouldBeOn) {
        leftLevel = dimmed(leftLevel);  // ~10% of base
      }
      if (rightGlare && rightShouldBeOn) {
        rightLevel = dimmed(rightLevel);
      }
      if (!leftGlare && !rightGlare) {
        // No side inferred -> dim both sides slightly to be safe
        if (leftShouldBeOn)  leftLevel  = dimmed(leftLevel);
        if (rightShouldBeOn) rightLevel = dimmed(rightLevel);
      }
      // Center remains at base, per your spec (always on, no directional dim)
    }
  }
  #endif
  // -------- 5) Apply per-device intensities --------
  // If matrix is OFF, intensity setting is harmless but we still set it to keep state coherent.
  setDeviceIntensity(DEV_LEFT,   leftLevel);
  setDeviceIntensity(DEV_CENTER, centerLevel);
  setDeviceIntensity(DEV_RIGHT,  rightLevel);
  delay(10); // loop stability
}
