// Test 2: tìm DS3231 trên I2C, đọc giờ và đặt giờ UTC
#include <Wire.h>
#include <RTClib.h>

RTC_DS3231 rtc;
bool rtcOk = false;

void scanI2C() {
  Serial.println("Quét I2C...");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Thấy thiết bị ở 0x%02X", addr);
      if (addr == 0x68) Serial.print("  <- DS3231 (đồng hồ)");
      if (addr == 0x57) Serial.print("  <- EEPROM trên module");
      Serial.println();
      found++;
    }
  }
  if (found == 0) {
    Serial.println("  Không thấy gì. Kiểm tra 4 dây: 3V3-VCC, GND-GND, 21-SDA, 22-SCL.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== TEST 2: DS3231 ===");
  Wire.begin(21, 22);  // SDA = GPIO21, SCL = GPIO22
  scanI2C();
  rtcOk = rtc.begin(&Wire);
  if (!rtcOk) {
    Serial.println("LỖI: không đọc được DS3231.");
    return;
  }
  if (rtc.lostPower()) {
    Serial.println("Đồng hồ chưa có giờ (hoặc từng mất cả điện lẫn pin): cần đặt giờ.");
  }
  Serial.println("Đặt giờ: trên Mac mở Terminal, chạy  date +%s");
  Serial.println("rồi gõ vào ô gửi của Serial Monitor: T và số vừa có, ví dụ T1792800000");
}

void handleSerial() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() > 1 && (cmd[0] == 'T' || cmd[0] == 't')) {
    uint32_t epoch = strtoul(cmd.c_str() + 1, nullptr, 10);
    if (epoch > 1767225600UL) {  // sau 1/1/2026
      rtc.adjust(DateTime(epoch));
      Serial.println("Đã đặt giờ UTC.");
    } else {
      Serial.println("Số không hợp lệ. Trên Mac chạy: date +%s");
    }
  }
}

void loop() {
  if (!rtcOk) {
    delay(1000);
    return;
  }
  handleSerial();
  static uint32_t last = 0;
  if (millis() - last >= 1000) {
    last = millis();
    DateTime now = rtc.now();
    Serial.printf("UTC %s   epoch %lu%s\n", now.timestamp().c_str(), (unsigned long)now.unixtime(),
                  now.year() < 2026 ? "   <- chưa đặt giờ" : "");
  }
}