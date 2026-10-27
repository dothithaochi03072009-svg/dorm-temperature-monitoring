// Test 1: board chạy, nạp code được, in địa chỉ MAC
#include <WiFi.h>

const int LED_PIN = 2;  // đèn trên board (nếu board có)

void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(LED_PIN, OUTPUT);
  WiFi.mode(WIFI_STA);  // bật WiFi để đọc đúng địa chỉ MAC
  delay(100);
  Serial.println();
  Serial.println("=== TEST 1: BOARD ===");
  Serial.printf("Chip: %s, %d nhân, %lu MHz\n", ESP.getChipModel(), ESP.getChipCores(),
                (unsigned long)ESP.getCpuFreqMHz());
  Serial.printf("Flash: %lu MB\n", (unsigned long)(ESP.getFlashChipSize() / (1024 * 1024)));
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.println("Ghi địa chỉ MAC này lên nhãn của board.");
}

void loop() {
  static int count = 0;
  digitalWrite(LED_PIN, HIGH);
  delay(500);
  digitalWrite(LED_PIN, LOW);
  delay(500);
  if (++count % 5 == 0) {
    Serial.print("Đang chạy, MAC ");
    Serial.println(WiFi.macAddress());
  }
}