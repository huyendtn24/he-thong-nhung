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
int counter = 0;
void sendSensorData() {
  counter++;
  Serial.println("-----------------------");
  Serial.print("Lan gui thu: "); Serial.println(counter);
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  int soilPercent = constrain(map(analogRead(SOIL_PIN), 4095, 1000, 0, 100), 0, 100);
  int lightPercent = constrain(map(analogRead(LIGHT_PIN), 4095, 0, 0, 100), 0, 100);
  Serial.print("Nhiet do: "); Serial.print(t); Serial.println("C");
  Serial.print("Do am dat: "); Serial.print(soilPercent); Serial.println("%");
  Serial.print("Anh sang moi truong: "); Serial.print(lightPercent); Serial.println("%");
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
      Serial.println(">> TRANG THAI: Dang BAT May Bom");
      Blynk.virtualWrite(V5, 1); 
      if (lastPumpState == false) { 
        Blynk.logEvent("thong_bao_bat", "Đất khô, tự động BẬT bơm!");
        lastPumpState = true; 
      }
    } 
    else if (soilPercent > 60) {
      digitalWrite(PUMP_PIN, LOW);
      Blynk.virtualWrite(V5, 0); 
      if (lastPumpState == true) { 
        Blynk.logEvent("thong_bao_tat", "Đất đủ ẩm, tự động TẮT bơm!");
        lastPumpState = false;
      }
    }

    // --- Tự động Đèn LED (Khóa công suất 20%) ---
    int SENSOR_THRESHOLD = 20;  // Ngưỡng cảm biến để tắt đèn (20%)
    int LED_MAX_POWER = 51;     // Công suất tối đa của LED (20% của 255 là 51)
    int STEP = 17;              // Bước nhảy để đèn sáng/tối từ từ

    static int autoPWM = 0; 

    if (lightPercent > SENSOR_THRESHOLD) {
      // 1. Nếu có ánh sáng lạ chiếu vào làm cảm biến > 20% -> Tắt đèn dần dần
      autoPWM -= STEP;
      if (autoPWM < 0) autoPWM = 0;     
    }
    else { 
      // 2. Nếu cảm biến đọc thấp (< 20%, ví dụ 0%) -> Tăng đèn dần dần đến mức 20%
      autoPWM += STEP;
      // Chặn trần ở mức 51 (20% công suất), không cho tăng thêm dù cảm biến vẫn đọc là 0%
      if (autoPWM > LED_MAX_POWER) autoPWM = LED_MAX_POWER; 
    }

    // Xuất lệnh ra phần cứng
    analogWrite(LED_PIN, autoPWM);
    
    // Logic báo sự kiện
    if (autoPWM > 0) { 
      if (lastLEDState == false) {
        Blynk.logEvent("thong_bao_bat", "Trời tối, tự động BẬT đèn sáng 20%!");
        lastLEDState = true;
      }
    } else { 
      if (lastLEDState == true) {
        Blynk.logEvent("thong_bao_tat", "Có ánh sáng ngoài, tự động TẮT đèn!");
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

// Nút V6: Điều khiển Đèn thủ công bằng thanh trượt (0-100)
BLYNK_WRITE(V6) {
  // Ghi đè thông minh: Hễ kéo thanh trượt V6 là tự động giật quyền về Thủ công
  isAutoMode = false;
  Blynk.virtualWrite(V0, 0); 

  int sliderPercent = param.asInt(); // Nhận giá trị từ App là 0 đến 100
  
  // Ép tỷ lệ 0-100% thành số 0-255 cho phần cứng phát xung PWM
  int pwmValue = map(sliderPercent, 0, 100, 0, 255); 
  
  // Dùng analogWrite thay vì digitalWrite để làm mờ/sáng đèn
  analogWrite(LED_PIN, pwmValue); 

  // Logic chống Spam Log cho thanh trượt
  if (sliderPercent > 0) {
    if (lastLEDState == false) { // Chỉ báo đã BẬT khi kéo từ 0 lên một mức có sáng
      Blynk.logEvent("thong_bao_bat", "Đã BẬT đèn LED (Bằng tay)");
      lastLEDState = true;
    }
  } else {
    if (lastLEDState == true) {  // Chỉ báo đã TẮT khi kéo hẳn về 0
      Blynk.logEvent("thong_bao_tat", "Đã TẮT đèn LED (Bằng tay)");
      lastLEDState = false;
    }
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
  analogWrite(LED_PIN, 0);  

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
