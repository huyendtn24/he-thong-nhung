# 🌱 Hệ thống trồng nấm trong nhà

## 📌 Giới thiệu
Dự án xây dựng một hệ thống IoT nhằm giám sát và điều khiển môi trường trong nhà trồng nấm theo thời gian thực.  
Hệ thống giúp duy trì các điều kiện tối ưu như nhiệt độ, độ ẩm, ánh sáng và độ ẩm đất nhằm nâng cao năng suất và chất lượng nấm.

---

## 🎯 Mục tiêu
- Thu thập dữ liệu môi trường tự động
- Điều khiển thiết bị thông minh (bơm nước, đèn)
- Giám sát từ xa qua Internet
- Hỗ trợ hai chế độ: **Tự động (Auto)** và **Thủ công (Manual)**

---



### 🔹 Phân tầng hệ thống
- **Tầng cảm biến và chấp hàng:** Cảm biến, relay, mosfet  
- **Tầng xử lý và kết nối:** ESP32
- **Tầng ứng dụng:** LCD, Blynk 

---

## 🧰 Phần cứng sử dụng

| Thiết bị | Mô tả |
|--------|------|
| ESP32 | Vi điều khiển chính, tích hợp WiFi |
| DHT11 | Đo nhiệt độ và độ ẩm không khí |
| Cảm biến độ ẩm đất | Đo độ ẩm đất |
| Cảm biến ánh sáng | Đo cường độ ánh sáng |
| Relay | Điều khiển máy bơm |
| LED | Hệ thống chiếu sáng |
| LCD 16x2 (I2C) | Hiển thị thông tin tại chỗ |

---

## 💻 Phần mềm & thư viện

- WiFi.h  
- BlynkSimpleEsp32.h  
- DHT.h  
- LiquidCrystal_I2C.h  
- Wire.h  

---

## ⚙️ Chức năng chính

### 📡 1. Giám sát môi trường
- Nhiệt độ 🌡️  
- Độ ẩm không khí 💧  
- Độ ẩm đất 🌱  
- Ánh sáng ☀️  

---

### 🤖 2. Chế độ tự động (Auto Mode)
- Bật bơm khi độ ẩm đất < 40%
- Tắt bơm khi độ ẩm đất > 70% (có hysteresis)
- Điều chỉnh độ sáng LED theo ánh sáng môi trường

---

### 🎮 3. Chế độ thủ công (Manual Mode)
- Điều khiển bật/tắt bơm từ app
- Điều chỉnh độ sáng LED theo ý muốn

---

### 📱 4. Giám sát từ xa
- Sử dụng ứng dụng Blynk
- Hiển thị dữ liệu thời gian thực
- Gửi thông báo khi thiết bị thay đổi trạng thái

---


## 🧠 Logic điều khiển

### 💧 Điều khiển bơm (Hysteresis)
- Bật khi: Độ ẩm đất < 40%
- Tắt khi: Độ ẩm đất > 70%

### 💡 Điều khiển đèn
- Ánh sáng thấp → tăng độ sáng LED
- Ánh sáng cao → tắt LED

---

## ⚠️ Xử lý lỗi

- ❌ DHT trả về NaN → bỏ qua dữ liệu
- 📶 Mất WiFi → tự động reconnect
- 🔌 Mất kết nối Blynk → thử kết nối lại
- ⏱️ Timeout khi kết nối mạng

---

