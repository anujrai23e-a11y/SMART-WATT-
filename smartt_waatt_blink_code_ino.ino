#define BLYNK_TEMPLATE_ID "TMPL3L1Xd1lKa"
#define BLYNK_TEMPLATE_NAME "SMART WATT"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Blynk iot
char auth[] = "VKo-p8p7Ds1smbdIWuVn3O7dlkRB14d6";
char ssid[] = "DELL5";
char pass[] = "anujrai@";

// Pins define
#define CURRENT_SENSOR 35
#define VOLTAGE_SENSOR 34
#define RELAY_PIN 26
#define RED_LED 25     // PWM LED pin

// Calibration factor
float zmptFactor = 590.0;
float sensitivity = 0.185;

// Variables decleartion 
BlynkTimer timer;
float energy = 0;
unsigned long lastTime = 0;
int powerLimit = 1000;
bool alertSent = false;
float ratePerUnit = 8.0;
float cost = 0;
int screen = 0;

// Manual relay control
bool manualState = true;

// PWM setup
const int ledChannel = 0;
const int freq = 5000;
const int resolution = 8;

// Relay Control from Blynk (V3)
BLYNK_WRITE(V3) {
  manualState = param.asInt();
}

// Power limit slider (V4)
BLYNK_WRITE(V4) {
  powerLimit = param.asInt();
}

// Voltage Reading
float readVoltage() {
  float offset = 0;

  for (int i = 0; i < 500; i++) {
    offset += analogRead(VOLTAGE_SENSOR);
  }
  offset /= 500;

  float sum = 0;

  for (int i = 0; i < 500; i++) {
    float v = analogRead(VOLTAGE_SENSOR) - offset;
    float voltage = (v * 3.3 / 4095.0);
    sum += voltage * voltage;
    delayMicroseconds(200);
  }

  float Vrms = sqrt(sum / 500);
  return Vrms * zmptFactor;
}

// Current Reading
float readCurrent() {
  float offset = 0;

  for (int i = 0; i < 300; i++) {
    offset += analogRead(CURRENT_SENSOR);
  }
  offset /= 300;

  float sum = 0;

  for (int i = 0; i < 300; i++) {
    float v = analogRead(CURRENT_SENSOR) - offset;
    float current = (v * 3.3 / 4095.0) / sensitivity;
    sum += current * current;
  }

  float Irms = sqrt(sum / 300);

  if (Irms < 0.01) Irms = 0;
  return Irms;
}

// Main Function
void sendData() {

  float voltage = readVoltage();
  float current = readCurrent();
  float power = voltage * current;

  if (voltage < 50) {
    voltage = 0;
    current = 0;
    power = 0;
  }

  if (power < 5) power = 0;

  // Energy calculation
  unsigned long now = millis();
  float timeH = (now - lastTime) / 3600000.0;
  lastTime = now;

  if (power > 0) {
    energy += (power * timeH) / 1000.0;
  }

  cost = energy * ratePerUnit;

  // Relay Logic
  bool relayON = false;

  if (power > powerLimit) {
    digitalWrite(RELAY_PIN, HIGH);   // OFF

    if (!alertSent) {
      Blynk.logEvent("overload_alert", " Overload detected!");
      alertSent = true;
    }

    relayON = false;
  } else {
    digitalWrite(RELAY_PIN, manualState ? LOW : HIGH);
    relayON = manualState;
    alertSent = false;
  }

  // LED Brightness Control
  if (relayON) {
    int brightness = map((int)power, 0, powerLimit, 30, 255);
    brightness = constrain(brightness, 30, 255);
    ledcWrite(RED_LED, brightness);
  } else {
    ledcWrite(RED_LED, 0);
  }

  // Send to Blynk
  Blynk.virtualWrite(V0, voltage);
  Blynk.virtualWrite(V1, current);
  Blynk.virtualWrite(V2, power);
  Blynk.virtualWrite(V5, energy);

  // LCD
  lcd.clear();
  lcd.setCursor(0, 0);

  if (screen == 0) {
    lcd.print("V:");
    lcd.print(voltage, 0);
    lcd.print(" I:");
    lcd.print(current, 2);

    lcd.setCursor(0, 1);
    lcd.print("P:");
    lcd.print(power, 0);
    lcd.print("W");
  }
  else if (screen == 1) {
    lcd.print("Energy:");
    lcd.print(energy, 2);

    lcd.setCursor(0, 1);
    lcd.print("Cost:Rs");
    lcd.print(cost, 1);
  }
  else {
    lcd.print("SMART WATT");

    lcd.setCursor(0, 1);
    lcd.print(relayON ? "Relay ON" : "Relay OFF");
  }

  screen++;
  if (screen > 2) screen = 0;
}

// Setup
void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  // PWM LED setup
 ledcAttach(RED_LED, freq, resolution);

  lcd.init();
  lcd.backlight();

  Blynk.begin(auth, ssid, pass);

  lastTime = millis();

  timer.setInterval(2000L, sendData);
}

// Loop
void loop() {
  Blynk.run();
  timer.run();
}