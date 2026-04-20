#define BLYNK_TEMPLATE_ID "TMPL6Z_W2RFTo"
#define BLYNK_TEMPLATE_NAME "thinghiemiot"
#define BLYNK_AUTH_TOKEN "KxHzlfsl6YpC3ofnAt1zlSxJMJemn5lP"

#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// --- THÔNG TIN KẾT NỐI ---
char ssid[] = "TP-Link_987E";
char pass[] = "97474074";

// --- CẤU HÌNH CHÂN PIN ---
#define DHTPIN 15
#define DHTTYPE DHT11
#define SOIL_PIN 34
#define LIGHT_PIN 32
#define PUMP_PIN 16
#define LED_PIN 17 
#define I2C_SDA 25
#define I2C_SCL 26

// --- CẤU HÌNH PWM (LEDC) ---
const int pwmFreq = 5000;
const int pwmResolution = 8;

// --- QUẢN LÝ NGẮT CỨNG (Cú pháp Core 3.x) ---
hw_timer_t * hwtimer = NULL;
volatile bool sensorTaskFlag = false; 

// --- BIẾN TRẠNG THÁI ---
bool isAutoMode = true; 
bool lastPumpState = false;
bool lastLEDState = false;

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// =========================================================
// 1. HÀM PHỤC VỤ NGẮT (ISR)
// =========================================================
void IRAM_ATTR onTimerInterrupt() {
  sensorTaskFlag = true; 
}

// =========================================================
// 2. HÀM ĐIỀU KHIỂN THIẾT BỊ (ĐÃ TỐI ƯU)
// =========================================================
void controlPump(bool turnOn, bool isManual = false) {
  digitalWrite(PUMP_PIN, turnOn ? HIGH : LOW);
  
  if (turnOn != lastPumpState) {
    Blynk.virtualWrite(V5, turnOn ? 1 : 0);
    String action = turnOn ? "BẬT" : "TẮT";
    String mode = isManual ? " (Thủ công)" : " (Tự động)";
    Blynk.logEvent(turnOn ? "thong_bao_bat" : "thong_bao_tat", "Máy bơm: " + action + mode);
    lastPumpState = turnOn;
  }
}

void controlLED(int brightness, bool isManual = false) {
  ledcWrite(LED_PIN, brightness);
  bool currentState = (brightness > 0);
  
  if (currentState != lastLEDState) {
    Blynk.virtualWrite(V6, brightness);
    String action = currentState ? "BẬT" : "TẮT";
    String mode = isManual ? " (Thủ công)" : " (Tự động)";
    Blynk.logEvent(currentState ? "thong_bao_bat" : "thong_bao_tat", "Đèn LED: " + action + mode);
    lastLEDState = currentState;
  }
}

// =========================================================
// 3. LOGIC XỬ LÝ TRUNG TÂM
// =========================================================
void executeSensorLogic() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  // Map ADC 12-bit (0-4095) về %, xử lý giá trị biên
  int soilPercent = constrain(map(analogRead(SOIL_PIN), 4095, 1000, 0, 100), 0, 100);
  int lightPercent = constrain(map(analogRead(LIGHT_PIN), 4095, 0, 0, 100), 0, 100);

  // Cập nhật LCD (In đè để tránh clear màn hình gây nháy)
  lcd.setCursor(0, 0); lcd.print("T:" + String((int)t) + "C  H:" + String((int)h) + "%   ");
  lcd.setCursor(0, 1); lcd.print("S:" + String(soilPercent) + "%  L:" + String(lightPercent) + "%   ");

  // Đẩy dữ liệu lên Cloud
  Blynk.virtualWrite(V1, t);
  Blynk.virtualWrite(V2, soilPercent);
  Blynk.virtualWrite(V3, lightPercent);
  Blynk.virtualWrite(V4, h);
  Blynk.virtualWrite(V0, isAutoMode ? 1 : 0);

  // Logic Auto Mode
  if (isAutoMode) {
    // Bơm: Hysteresis 40% - 60%
    if (soilPercent < 40) controlPump(true);
    else if (soilPercent > 70) controlPump(false);

    // Đèn LED: Dimming khi sáng < 30%
    if (lightPercent < 30) {
      int brightness = map(lightPercent, 0, 30, 255, 100);
      controlLED(brightness);
    } else if (lightPercent > 70) {
      controlLED(0);
    }
  }
}

// =========================================================
// 4. CALL BACKS TỪ BLYNK APP
// =========================================================
BLYNK_WRITE(V0) {
  isAutoMode = (param.asInt() == 1);
  Blynk.logEvent(isAutoMode ? "thong_bao_bat" : "thong_bao_tat", 
                 isAutoMode ? "Hệ thống: TỰ ĐỘNG" : "Hệ thống: THỦ CÔNG");
}

BLYNK_WRITE(V5) {
  isAutoMode = false; 
  controlPump(param.asInt() == 1, true);
}

BLYNK_WRITE(V6) {
  isAutoMode = false;
  controlLED(param.asInt(), true);
}

// =========================================================
// 5. SETUP & LOOP
// =========================================================
void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  
  lcd.init();
  lcd.backlight();
  lcd.print("ESP32 Core 3.x");

  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW); // Fail-safe: Tắt bơm khi khởi động

  // Khởi tạo PWM (Chuẩn mới cho Core 3.x)
  ledcAttach(LED_PIN, pwmFreq, pwmResolution);
  ledcWrite(LED_PIN, 0);

  dht.begin();
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // --- CẤU HÌNH HARDWARE TIMER (CÚ PHÁP MỚI CORE 3.X) ---
  // Tần số 1MHz => 1 nhịp đếm = 1 micro-giây
  hwtimer = timerBegin(1000000); 
  
  // Gán hàm ngắt
  timerAttachInterrupt(hwtimer, &onTimerInterrupt);

  // Đặt báo thức 2 giây (2,000,000 micro-giây), tự động nạp lại
  timerAlarm(hwtimer, 2000000, true, 0); 

  lcd.clear();
  lcd.print("System Ready!");
  
}

void loop() {
  Blynk.run();
  
  // Kiểm tra cờ từ Hardware Timer
  if (sensorTaskFlag) {
    sensorTaskFlag = false; 
    executeSensorLogic();   
  }
}
