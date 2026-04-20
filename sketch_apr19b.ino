#define BLYNK_TEMPLATE_ID "TMPL6Z_W2RFTo"
#define BLYNK_TEMPLATE_NAME "thinghiemiot"
#define BLYNK_AUTH_TOKEN "KxHzlfsl6YpC3ofnAt1zlSxJMJemn5lP"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ================= WIFI =================
char ssid[] = "TP-Link_987E";
char pass[] = "97474074";

// ================= PIN =================
#define DHTPIN 15
#define DHTTYPE DHT11
#define SOIL_PIN 34
#define LIGHT_PIN 32
#define PUMP_PIN 16
#define LED_PIN 17

#define I2C_SDA 25
#define I2C_SCL 26

// ================= PWM =================
const int pwmFreq = 5000;
const int pwmResolution = 8;

// ================= TIMER =================
hw_timer_t *timer = NULL;
volatile bool sensorFlag = false;

// ================= STATE =================
bool isAutoMode = true;
bool lastPumpState = false;
bool lastLEDState = false;

unsigned long lastBlynkReconnect = 0;

// ================= OBJECT =================
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// =====================================================
// 1. TIMER ISR
// =====================================================
void IRAM_ATTR onTimer() {
  sensorFlag = true;
}

// =====================================================
// 2. WIFI CONNECT (RETRY + TIMEOUT)
// =====================================================
void connectWiFi() {
  Serial.println("Connecting WiFi...");
  WiFi.begin(ssid, pass);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
  } else {
    Serial.println("\nWiFi FAILED!");
  }
}

// =====================================================
// 3. BLYNK RECONNECT (NON-BLOCKING)
// =====================================================
void checkBlynk() {
  if (!Blynk.connected()) {
    unsigned long now = millis();
    if (now - lastBlynkReconnect > 5000) {
      lastBlynkReconnect = now;
      Serial.println("Reconnecting Blynk...");
      Blynk.connect();
    }
  }
}

// =====================================================
// 4. CONTROL DEVICE
// =====================================================
void controlPump(bool state, bool manual = false) {
  digitalWrite(PUMP_PIN, state ? HIGH : LOW);

  if (state != lastPumpState) {
    Blynk.virtualWrite(V5, state);

    String msg = state ? "BẬT" : "TẮT";
    msg += manual ? " (Manual)" : " (Auto)";

    Blynk.logEvent(state ? "thong_bao_bat" : "thong_bao_tat", "Pump: " + msg);
    lastPumpState = state;
  }
}

void controlLED(int brightness, bool manual = false) {
  ledcWrite(LED_PIN, brightness);

  bool state = brightness > 0;

  if (state != lastLEDState) {
    Blynk.virtualWrite(V6, brightness);

    String msg = state ? "BẬT" : "TẮT";
    msg += manual ? " (Manual)" : " (Auto)";

    Blynk.logEvent(state ? "thong_bao_bat" : "thong_bao_tat", "LED: " + msg);
    lastLEDState = state;
  }
}

// =====================================================
// 5. SENSOR LOGIC (CORE SYSTEM)
// =====================================================
void runSystem() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // ❗ CHECK NaN
  if (isnan(h) || isnan(t)) {
    Serial.println("DHT ERROR!");
    return;
  }

  int soil = constrain(map(analogRead(SOIL_PIN), 4095, 1000, 0, 100), 0, 100);
  int light = constrain(map(analogRead(LIGHT_PIN), 4095, 0, 0, 100), 0, 100);

  // ===== LCD =====
  lcd.setCursor(0, 0);
  lcd.print("T:" + String((int)t) + " H:" + String((int)h) + "   ");

  lcd.setCursor(0, 1);
  lcd.print("S:" + String(soil) + " L:" + String(light) + "   ");

  // ===== BLYNK =====
  Blynk.virtualWrite(V1, t);
  Blynk.virtualWrite(V2, soil);
  Blynk.virtualWrite(V3, light);
  Blynk.virtualWrite(V4, h);
  Blynk.virtualWrite(V0, isAutoMode);

  // ===== AUTO MODE =====
  if (isAutoMode) {

    // Pump hysteresis
    if (soil < 40) controlPump(true);
    else if (soil > 70) controlPump(false);

    // LED dimming
    if (light < 30) {
      int pwm = map(light, 0, 30, 255, 100);
      controlLED(pwm);
    } else if (light > 70) {
      controlLED(0);
    }
  }
}

// =====================================================
// 6. BLYNK CALLBACK
// =====================================================
BLYNK_WRITE(V0) {
  isAutoMode = param.asInt();
}

BLYNK_WRITE(V5) {
  isAutoMode = false;
  controlPump(param.asInt(), true);
}

BLYNK_WRITE(V6) {
  isAutoMode = false;
  controlLED(param.asInt(), true);
}

// =====================================================
// 7. SETUP
// =====================================================
void setup() {
  Serial.begin(115200);

  Wire.begin(I2C_SDA, I2C_SCL);

  lcd.init();
  lcd.backlight();
  lcd.print("System Boot...");

  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);

  // PWM ESP32 Core 3.x
  ledcAttach(LED_PIN, pwmFreq, pwmResolution);
  ledcWrite(LED_PIN, 0);

  dht.begin();

  connectWiFi();

  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect();

  // TIMER 2s
  timer = timerBegin(1000000);
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, 2000000, true, 0);

  lcd.clear();
  lcd.print("Ready!");
}

// =====================================================
// 8. LOOP
// =====================================================
void loop() {
  Blynk.run();
  checkBlynk();

  if (sensorFlag) {
    sensorFlag = false;
    runSystem();
  }
}
