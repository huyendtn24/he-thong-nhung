// 1. Cấu hình cơ bản Blynk
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

// Thông tin WiFi
char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "TP-Link_987E";
char pass[] = "97474074";

// Cấu hình chân
#define DHTPIN 15
#define DHTTYPE DHT11
#define SOIL_PIN 34
#define LIGHT_PIN 32
#define PUMP_PIN 16
#define LED_PIN 17      // Chân này bây giờ nối vào chân SIG của module MOSFET
#define I2C_SDA 25
#define I2C_SCL 26

// --- CẤU HÌNH PWM CHO MOSFET ---
const int pwmFreq = 5000;      // Tần số băm xung 5kHz (Chống mỏi mắt, phù hợp với LED)
const int pwmResolution = 8;   // Độ phân giải 8-bit (Dải giá trị độ sáng từ 0 - 255)
int currentBrightness = 0;     // Biến lưu độ sáng hiện tại

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);
BlynkTimer timer;

// BIẾN QUẢN LÝ CHẾ ĐỘ (Khởi động lên mặc định là Tự động)
bool isAutoMode = true; 

// Cờ nhớ trạng thái (Chống Spam)
bool lastPumpState = false; 
bool lastLEDState = false;

void sendSensorData() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  int soilPercent = constrain(map(analogRead(SOIL_PIN), 4095, 1000, 0, 100), 0, 100);
  int lightPercent = constrain(map(analogRead(LIGHT_PIN), 4095, 0, 0, 100), 0, 100);

  // Cập nhật LCD
  lcd.clear(); 
  lcd.setCursor(0, 0);
  lcd.print("T:"); lcd.print((int)t); lcd.print("C");
  lcd.setCursor(8, 0);
  lcd.print("H:"); lcd.print((int)h); lcd.print("%");
  lcd.setCursor(0, 1);
  lcd.print("S:"); lcd.print(soilPercent); lcd.print("%");
  lcd.setCursor(8, 1);
  lcd.print("L:"); lcd.print(lightPercent); lcd.print("%");

  // Gửi thông số lên App
  Blynk.virtualWrite(V1, t);
  Blynk.virtualWrite(V2, soilPercent);
  Blynk.virtualWrite(V3, lightPercent);
  Blynk.virtualWrite(V4, h);
  
  // Đồng bộ trạng thái nút V0 lên màn hình điện thoại
  Blynk.virtualWrite(V0, isAutoMode ? 1 : 0);

  // ----------------------------------------------------
  // KHỐI LOGIC NÀY CHỈ CHẠY KHI Ở CHẾ ĐỘ TỰ ĐỘNG
  // ----------------------------------------------------
  if (isAutoMode == true) {
    
    // --- Tự động Máy Bơm (Giữ nguyên dùng Relay) ---
    if (soilPercent < 40) {
      digitalWrite(PUMP_PIN, HIGH);
      Blynk.virtualWrite(V5, 1); 
      
      if (lastPumpState == false) { 
        Blynk.logEvent("thong_bao_bat", "Đất khô, tự động BẬT bơm!");
        lastPumpState = true; 
      }
    } 
    else if (soilPercent > 80) {
      digitalWrite(PUMP_PIN, LOW);
      Blynk.virtualWrite(V5, 0); 
      
      if (lastPumpState == true) { 
        Blynk.logEvent("thong_bao_tat", "Đất đủ ẩm, tự động TẮT bơm!");
        lastPumpState = false;
      }
    }

    // --- Tự động Đèn LED (Dùng MOSFET Điều sáng tuyến tính) ---
    if (lightPercent < 30) {
      // Logic Dimming: Ánh sáng tự nhiên càng yếu (<30%), đèn bật càng sáng (từ 100 đến 255)
      currentBrightness = map(lightPercent, 0, 30, 255, 100);
      ledcWrite(LED_PIN, currentBrightness); // Hàm xuất PWM mới của ESP32
      Blynk.virtualWrite(V6, currentBrightness); // Đồng bộ thanh trượt V6 trên App
      
      if (lastLEDState == false) {
        Blynk.logEvent("thong_bao_bat", "Trời tối, tự động BẬT đèn bù sáng!");
        lastLEDState = true;
      }
    } 
    else if (lightPercent > 70) {
      currentBrightness = 0; // Tắt đèn hoàn toàn
      ledcWrite(LED_PIN, currentBrightness);
      Blynk.virtualWrite(V6, currentBrightness); 
      
      if (lastLEDState == true) {
        Blynk.logEvent("thong_bao_tat", "Trời sáng, tự động TẮT đèn!");
        lastLEDState = false;
      }
    }
  }
}

// ----------------------------------------------------
// CÁC HÀM XỬ LÝ NÚT BẤM TRÊN ĐIỆN THOẠI
// ----------------------------------------------------

// Nút V0: Chọn chế độ (Tự Động / Thủ Công)
BLYNK_WRITE(V0) {
  if (param.asInt() == 1) {
    isAutoMode = true;
    Blynk.logEvent("thong_bao_bat", "Hệ thống: Đã bật chế độ TỰ ĐỘNG");
  } else {
    isAutoMode = false;
    Blynk.logEvent("thong_bao_tat", "Hệ thống: Đã bật chế độ THỦ CÔNG");
  }
}

// Nút V5: Điều khiển Bơm thủ công (Relay)
BLYNK_WRITE(V5) {
  isAutoMode = false; 
  Blynk.virtualWrite(V0, 0); 

  int value = param.asInt();
  if (value == 1) {
    digitalWrite(PUMP_PIN, HIGH); 
    Blynk.logEvent("thong_bao_bat", "Đã BẬT máy bơm (Bằng tay)");
    lastPumpState = true;
  } else {
    digitalWrite(PUMP_PIN, LOW); 
    Blynk.logEvent("thong_bao_tat", "Đã TẮT máy bơm (Bằng tay)");
    lastPumpState = false;
  }
}

// Slider V6: Điều khiển Đèn thủ công (MOSFET)
BLYNK_WRITE(V6) {
  // Ghi đè thông minh: Kéo slider V6 là tự động giật quyền về Thủ công
  isAutoMode = false;
  Blynk.virtualWrite(V0, 0); 

  currentBrightness = param.asInt(); // Nhận giá trị từ 0 - 255
  ledcWrite(LED_PIN, currentBrightness); 

  // Xử lý gửi thông báo chống Spam cho Slider
  if (currentBrightness > 0 && lastLEDState == false) {
    Blynk.logEvent("thong_bao_bat", "Đã BẬT đèn LED (Bằng tay)");
    lastLEDState = true;
  } else if (currentBrightness == 0 && lastLEDState == true) {
    Blynk.logEvent("thong_bao_tat", "Đã TẮT đèn LED (Bằng tay)");
    lastLEDState = false;
  }
}

// ----------------------------------------------------
void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init();
  lcd.backlight();
  
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW); // Tắt bơm lúc đầu

  // Khởi tạo PWM cho MOSFET (Sử dụng cú pháp chuẩn ESP32 Core 3.x)
  ledcAttach(LED_PIN, pwmFreq, pwmResolution);
  ledcWrite(LED_PIN, 0); // Đảm bảo đèn tắt khi vừa bật nguồn

  dht.begin();
  Blynk.begin(auth, ssid, pass);
  timer.setInterval(2000L, sendSensorData);
  
  lcd.setCursor(0, 0);
  lcd.print("Blynk Connected!");
}

void loop() {
  Blynk.run();
  timer.run();
}
