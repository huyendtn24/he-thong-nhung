// 1. Dán 3 dòng mã Blynk của bạn vào đây
#define BLYNK_TEMPLATE_ID "TMPL6Z_W2RFTo"
#define BLYNK_TEMPLATE_NAME "thinghiemiot"
#define BLYNK_AUTH_TOKEN "BOWM3FA_jUKbfrCifmy5iONd9x8Se3ub"

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
#define LED_PIN 17
#define I2C_SDA 25
#define I2C_SCL 26

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);
BlynkTimer timer;

// BIẾN QUẢN LÝ CHẾ ĐỘ (Khởi động lên mặc định là Tự động)
bool isAutoMode = true; 

// Cờ nhớ trạng thái
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
    
    // --- Tự động Máy Bơm ---
    if (soilPercent < 40) {
      digitalWrite(PUMP_PIN, HIGH);
      Blynk.virtualWrite(V5, 1); // Bơm chạy thì App cũng phải bật ON
      
      if (lastPumpState == false) { 
        Blynk.logEvent("thong_bao_bat", "Đất khô, tự động BẬT bơm!");
        lastPumpState = true; 
      }
    } 
    else if (soilPercent > 60) {
      digitalWrite(PUMP_PIN, LOW);
      Blynk.virtualWrite(V5, 0); // Bơm tắt thì App cũng phải gạt về OFF
      
      if (lastPumpState == true) { 
        Blynk.logEvent("thong_bao_tat", "Đất đủ ẩm, tự động TẮT bơm!");
        lastPumpState = false;
      }
    }

    // --- Tự động Đèn LED ---
    if (lightPercent < 30) {
      digitalWrite(LED_PIN, HIGH);
      Blynk.virtualWrite(V6, 1); // Đồng bộ nút Đèn ON
      
      if (lastLEDState == false) {
        Blynk.logEvent("thong_bao_bat", "Trời tối, tự động BẬT đèn!");
        lastLEDState = true;
      }
    } 
    else if (lightPercent > 70) {
      digitalWrite(LED_PIN, LOW);
      Blynk.virtualWrite(V6, 0); // Đồng bộ nút Đèn OFF
      
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

// Nút V5: Điều khiển Bơm thủ công
BLYNK_WRITE(V5) {
  // Ghi đè thông minh: Hễ chạm vào nút V5 là tự động giật quyền về Thủ công
  isAutoMode = false; 
  Blynk.virtualWrite(V0, 0); // Gạt luôn nút V0 trên app về OFF (Thủ công)

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

// Nút V6: Điều khiển Đèn thủ công
BLYNK_WRITE(V6) {
  // Ghi đè thông minh: Hễ chạm vào nút V6 là tự động giật quyền về Thủ công
  isAutoMode = false;
  Blynk.virtualWrite(V0, 0); // Gạt luôn nút V0 trên app về OFF (Thủ công)

  int value = param.asInt();
  if (value == 1) {
    digitalWrite(LED_PIN, HIGH); 
    Blynk.logEvent("thong_bao_bat", "Đã BẬT đèn LED (Bằng tay)");
    lastLEDState = true;
  } else {
    digitalWrite(LED_PIN, LOW); 
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
  pinMode(LED_PIN, OUTPUT);
  
  // Khởi động ở trạng thái TẮT
  digitalWrite(PUMP_PIN, LOW); 
  digitalWrite(LED_PIN, LOW);  

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