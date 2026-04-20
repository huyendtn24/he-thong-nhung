/*
 * Project: Hệ thống giám sát Nhà Nấm IoT
 * Version: FINAL (ESP32 Core 3.x - Stable)
 * Description:
 *  - Hệ thống sử dụng ESP32 để giám sát nhiệt độ, độ ẩm, ánh sáng, độ ẩm đất
 *  - Điều khiển bơm và đèn theo chế độ Auto/Manual
 *  - Giao tiếp với Blynk Cloud và hiển thị LCD
 */

#define BLYNK_TEMPLATE_ID "TMPL6Z_W2RFTo"
#define BLYNK_TEMPLATE_NAME "thinghiemiot"
#define BLYNK_AUTH_TOKEN "KxHzlfsl6YpC3ofnAt1zlSxJMJemn5lP"

#define BLYNK_PRINT Serial

// ================= LIBRARY =================
#include <WiFi.h>                  // WiFi ESP32
#include <BlynkSimpleEsp32.h>     // Blynk IoT
#include <Wire.h>                 // I2C
#include <LiquidCrystal_I2C.h>    // LCD I2C
#include <DHT.h>                  // Cảm biến DHT

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
const int pwmFreq = 5000;      // Tần số PWM
const int pwmResolution = 8;   // Độ phân giải (0-255)

// ================= TIMER =================
hw_timer_t *timer = NULL;
volatile bool sensorFlag = false;   // Cờ báo đọc sensor (set từ ISR)

// ================= STATE =================
bool isAutoMode = true;        // Chế độ Auto / Manual
bool lastPumpState = false;    // Trạng thái trước đó của bơm
bool lastLEDState = false;     // Trạng thái trước đó của LED

unsigned long lastBlynkReconnect = 0;

// ================= OBJECT =================
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// =====================================================
// 1. TIMER ISR
// =====================================================
/*
 * Ngắt phần cứng Timer
 * - Chạy mỗi 2 giây
 * - Không xử lý logic tại đây (tránh crash)
 * - Chỉ set cờ để loop xử lý
 */
void IRAM_ATTR onTimer() {
  sensorFlag = true;
}

// =====================================================
// 2. WIFI CONNECT (RETRY + TIMEOUT)
// =====================================================
/*
 * Kết nối WiFi có giới hạn retry
 * - Tránh treo hệ thống nếu WiFi lỗi
 * - Timeout sau ~10 giây
 */
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
/*
 * Tự động reconnect Blynk
 * - Không dùng delay → non-blocking
 * - Thử lại mỗi 5 giây
 */
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
/*
 * Điều khiển bơm
 * - Có chống spam (chỉ gửi khi state thay đổi)
 * - Có phân biệt Auto / Manual
 */
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

/*
 * Điều khiển LED bằng PWM
 * - brightness: 0–255
 */
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
/*
 * Hàm xử lý trung tâm:
 * - Đọc sensor
 * - Xử lý dữ liệu
 * - Hiển thị
 * - Điều khiển thiết bị
 */
void runSystem() {

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // ❗ CHECK lỗi DHT (NaN)
  if (isnan(h) || isnan(t)) {
    Serial.println("DHT ERROR!");
    return;
  }

  // Convert ADC → %
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

    // Hysteresis chống bật/tắt liên tục
    if (soil < 40) controlPump(true);
    else if (soil > 70) controlPump(false);

    // Điều chỉnh độ sáng LED
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
/*
 * Nhận dữ liệu từ App Blynk
 * - V0: Auto Mode
 * - V5: Pump
 * - V6: LED
 */

// Auto Mode
BLYNK_WRITE(V0) {
  isAutoMode = param.asInt();
}

// Manual Pump → override Auto
BLYNK_WRITE(V5) {
  isAutoMode = false;
  controlPump(param.asInt(), true);
}

// Manual LED → override Auto
BLYNK_WRITE(V6) {
  isAutoMode = false;
  controlLED(param.asInt(), true);
}

/*
 * Giám sát WiFi
 * - Phát hiện mất kết nối
 * - Tự reconnect
 */
void checkWiFi() {
  static bool wasConnected = true;

  if (WiFi.status() != WL_CONNECTED) {
    if (wasConnected) {
      Serial.println("WiFi LOST!");
      wasConnected = false;
    }

    Serial.println("Reconnecting WiFi...");
    WiFi.disconnect();
    WiFi.begin(ssid, pass);

    delay(500);
  } else {
    if (!wasConnected) {
      Serial.println("WiFi RECONNECTED!");
      wasConnected = true;
    }
  }
}

// =====================================================
// 7. SETUP
// =====================================================
/*
 * Khởi tạo hệ thống:
 * - Serial, LCD
 * - GPIO
 * - PWM
 * - WiFi + Blynk
 * - Timer interrupt
 */
void setup() {
  Serial.begin(115200);

  Wire.begin(I2C_SDA, I2C_SCL);

  lcd.init();
  lcd.backlight();
  lcd.print("System Boot...");

  // Fail-safe: tắt bơm khi start
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);

  // PWM (ESP32 Core 3.x)
  ledcAttach(LED_PIN, pwmFreq, pwmResolution);
  ledcWrite(LED_PIN, 0);

  dht.begin();

  connectWiFi();

  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect();

  // Timer 2 giây
  timer = timerBegin(1000000);
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, 2000000, true, 0);

  lcd.clear();
  lcd.print("Ready!");
}

// =====================================================
// 8. LOOP
// =====================================================
/*
 * Vòng lặp chính:
 * - Không blocking
 * - Điều phối hệ thống
 */
void loop() {
  Blynk.run();        // xử lý Blynk
  checkBlynk();       // reconnect Blynk
  checkWiFi();        // reconnect WiFi

  // Chạy theo event từ Timer
  if (sensorFlag) {
    sensorFlag = false;
    runSystem();
  }
}
