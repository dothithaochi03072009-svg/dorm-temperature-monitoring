// Nút đo: đo mỗi 10 giây, gửi ESP-NOW về trạm nhận, lưu trung bình mỗi phút vào flash
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_idf_version.h>
#include <esp_system.h>
#include <Wire.h>
#include <RTClib.h>
#include <LittleFS.h>
#include <Preferences.h>

// ===== CHỈ SỬA DÒNG NÀY: địa chỉ MAC của trạm nhận (trạm in ra khi khởi động) =====
uint8_t GATEWAY_MAC[6] = {0x04, 0xB2, 0x47, 0x54, 0xA1, 0x88};
// ==================================================================================

const uint8_t CHANNEL = 1;          // phải giống CHANNEL trong code trạm nhận
const uint32_t PERIOD_MS = 10000;   // đo mỗi 10 giây
const int PIN_POWER = 25;           // cấp điện cầu phân áp
const int PIN_ADC = 34;             // điểm giữa cầu phân áp
const int LED_PIN = 2;              // đèn trên board
const char *LOG_FILE = "/log.csv";  // trung bình mỗi phút: epoch,adc_mv,flags

enum { F_BOOT = 1, F_PROBE = 2, F_RTC = 4, F_FS = 8 };

// Cấu trúc gói: phải giống hệt trong code trạm nhận
typedef struct __attribute__((packed)) {
  uint8_t magic;
  uint8_t ver;
  uint8_t node;
  uint8_t flags;
  uint8_t rst;
  uint32_t seq;
  uint32_t utc;
  uint16_t adc_mv;
  int16_t temp_c100;
} Packet;

RTC_DS3231 rtc;
Preferences prefs;
uint8_t nodeId = 0;
bool rtcOk = false, fsOk = false, nowOk = false, fsFull = false, firstPacket = true;
uint32_t seq = 0;
volatile bool sendDone = false, sendOk = false;
uint32_t minuteKey = 0, sumMv = 0, nMv = 0;
uint8_t minuteFlags = 0;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
void onSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
#else
void onSent(const uint8_t *mac, esp_now_send_status_t status) {
#endif
  sendOk = (status == ESP_NOW_SEND_SUCCESS);
  sendDone = true;
}

uint32_t readMilliVolts() {
  digitalWrite(PIN_POWER, HIGH);
  delay(10);
  uint32_t sum = 0;
  for (int i = 0; i < 64; i++) sum += analogReadMilliVolts(PIN_ADC);
  digitalWrite(PIN_POWER, LOW);
  return sum / 64;
}

// Nhiệt độ danh định (điện trở 10 kΩ, 3,3 V); số chính xác tính lại khi phân tích
int16_t nominalTempC100(uint32_t mv) {
  if (mv < 300 || mv > 3000) return INT16_MIN;
  float r = 10000.0f * (3300.0f / mv - 1.0f);
  float tC = 1.0f / (1.0f / 298.15f + logf(r / 10000.0f) / 3950.0f) - 273.15f;
  return (int16_t)lroundf(tC * 100.0f);
}

void blink(int times, int ms) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(ms);
    digitalWrite(LED_PIN, LOW);
    delay(ms);
  }
}

bool gatewayMacSet() {
  for (int i = 0; i < 6; i++)
    if (GATEWAY_MAC[i] != 0) return true;
  return false;
}

// Ghi trung bình của phút vừa xong vào flash
void logMinute() {
  if (nMv > 0 && fsOk) {
    File f = LittleFS.open(LOG_FILE, FILE_APPEND);
    if (f) {
      f.printf("%lu,%lu,%u\n", (unsigned long)(minuteKey * 60), (unsigned long)(sumMv / nMv),
               (unsigned)minuteFlags);
      f.close();
    } else {
      fsOk = false;
    }
    fsFull = LittleFS.usedBytes() * 10 > LittleFS.totalBytes() * 9;  // đầy quá 90%
  }
  sumMv = 0;
  nMv = 0;
  minuteFlags = 0;
}

void printInfo() {
  Serial.printf("Mã nút: %s\n", nodeId ? ("N0" + String(nodeId)).c_str() : "CHƯA ĐẶT (gõ N1, N2 hoặc N3)");
  Serial.print("MAC của nút: ");
  Serial.println(WiFi.macAddress());
  Serial.printf("MAC trạm nhận trong code: %02X:%02X:%02X:%02X:%02X:%02X%s\n", GATEWAY_MAC[0],
                GATEWAY_MAC[1], GATEWAY_MAC[2], GATEWAY_MAC[3], GATEWAY_MAC[4], GATEWAY_MAC[5],
                gatewayMacSet() ? "" : "   <- CHƯA ĐIỀN");
  Serial.printf("DS3231: %s", rtcOk ? "OK" : "LỖI");
  if (rtcOk) Serial.printf(", giờ UTC %s", rtc.now().timestamp().c_str());
  Serial.println();
  Serial.printf("Flash: %s, đã dùng %lu / %lu byte\n", fsOk ? "OK" : "LỖI",
                (unsigned long)(fsOk ? LittleFS.usedBytes() : 0),
                (unsigned long)(fsOk ? LittleFS.totalBytes() : 0));
  Serial.printf("ESP-NOW: %s\n", nowOk ? "OK" : "LỖI");
  Serial.println("Lệnh: N1/N2/N3 đặt mã nút, T<epoch> đặt giờ, I xem thông tin, D tải file flash, XOA xoá file flash");
}

void handleSerial() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0) return;
  char c = toupper(cmd[0]);
  if (cmd == "XOA") {  // chỉ xoá sau khi đã tải file về máy
    LittleFS.remove(LOG_FILE);
    Serial.println("Đã xoá file dữ liệu trong flash.");
  } else if (c == 'N') {
    int id = cmd.substring(1).toInt();
    if (id >= 1 && id <= 9) {
      prefs.putUChar("id", (uint8_t)id);
      nodeId = (uint8_t)id;
      Serial.printf("Đã đặt mã nút N0%d (lưu trong board, không mất khi rút điện).\n", id);
    } else {
      Serial.println("Mã nút phải từ 1 đến 9, ví dụ N1");
    }
  } else if (c == 'T') {
    uint32_t epoch = strtoul(cmd.c_str() + 1, nullptr, 10);
    if (rtcOk && epoch > 1767225600UL) {
      rtc.adjust(DateTime(epoch));
      Serial.println("Đã đặt giờ UTC.");
    } else {
      Serial.println("Không đặt được giờ. Trên Mac chạy: date +%s rồi gõ T và số đó.");
    }
  } else if (c == 'D') {
    File f = LittleFS.open(LOG_FILE, FILE_READ);
    if (!f) {
      Serial.println("Chưa có file dữ liệu.");
      return;
    }
    Serial.println("---BAT DAU log.csv---");
    uint8_t buf[256];
    size_t n;
    while ((n = f.read(buf, sizeof(buf))) > 0) Serial.write(buf, n);
    f.close();
    Serial.println("---KET THUC---");
  } else if (c == 'I') {
    printInfo();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(LED_PIN, OUTPUT);
  pinMode(PIN_POWER, OUTPUT);
  digitalWrite(PIN_POWER, LOW);

  prefs.begin("cfg", false);
  nodeId = prefs.getUChar("id", 0);

  Wire.begin(21, 22);
  rtcOk = rtc.begin(&Wire);
  fsOk = LittleFS.begin(true);  // lần đầu tự định dạng vùng flash

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (gatewayMacSet() && esp_now_init() == ESP_OK) {
    esp_now_register_send_cb(onSent);
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, GATEWAY_MAC, 6);
    peer.channel = CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    nowOk = (esp_now_add_peer(&peer) == ESP_OK);
  }

  Serial.println();
  Serial.println("=== NÚT ĐO ===");
  printInfo();
}

void loop() {
  handleSerial();
  static uint32_t nextMs = 0;
  if ((int32_t)(millis() - nextMs) < 0) return;
  nextMs = millis() + PERIOD_MS;

  uint8_t flags = firstPacket ? F_BOOT : 0;
  uint32_t utc = 0;
  if (rtcOk) {
    DateTime now = rtc.now();
    utc = now.unixtime();
    if (now.year() < 2026 || rtc.lostPower()) flags |= F_RTC;
  } else {
    flags |= F_RTC;
  }
  uint32_t mv = readMilliVolts();
  if (mv < 300 || mv > 3000) flags |= F_PROBE;
  if (!fsOk || fsFull) flags |= F_FS;

  uint32_t key = utc / 60;  // sang phút mới thì ghi trung bình phút cũ
  if (key != minuteKey) {
    logMinute();
    minuteKey = key;
  }
  sumMv += mv;
  nMv++;
  minuteFlags |= flags;

  if (nodeId == 0 || !nowOk) {
    blink(5, 60);
    Serial.println("Chưa gửi được: thiếu mã nút (gõ N1/N2/N3) hoặc chưa điền MAC trạm nhận.");
    return;
  }

  Packet p = {0xA5, 1, nodeId, flags, (uint8_t)esp_reset_reason(), seq, utc, (uint16_t)mv, nominalTempC100(mv)};
  sendDone = false;
  sendOk = false;
  esp_err_t err = esp_now_send(GATEWAY_MAC, (const uint8_t *)&p, sizeof(p));
  uint32_t t0 = millis();
  while (err == ESP_OK && !sendDone && millis() - t0 < 200) delay(1);
  bool ok = (err == ESP_OK) && sendDone && sendOk;

  if (flags & (F_PROBE | F_RTC)) {
    blink(3, 80);  // nháy nhanh: lỗi đầu dò hoặc lỗi giờ
  } else if (ok) {
    blink(1, 40);  // nháy 1 lần: trạm đã nhận
  }
  if (ok) firstPacket = false;

  Serial.printf("N0%u seq %lu utc %lu adc %lu mV  %.2f °C  flags %u -> %s\n", (unsigned)nodeId,
                (unsigned long)seq, (unsigned long)utc, (unsigned long)mv, p.temp_c100 / 100.0,
                (unsigned)flags, ok ? "OK" : "KHÔNG TỚI TRẠM");
  seq++;
}
