// Trạm nhận: nhận gói ESP-NOW từ các nút đo, in ra cổng USB cho MacBook
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_idf_version.h>

const uint8_t CHANNEL = 1;  // phải giống CHANNEL trong code nút đo

// Cấu trúc gói: phải giống hệt trong code nút đo
typedef struct __attribute__((packed)) {
  uint8_t magic;      // luôn là 0xA5
  uint8_t ver;        // phiên bản gói, hiện là 1
  uint8_t node;       // mã nút: 1 = N01, 2 = N02, 3 = dự phòng
  uint8_t flags;      // bit0 BOOT, bit1 lỗi đầu dò, bit2 lỗi giờ, bit3 lỗi flash
  uint8_t rst;        // lý do khởi động lại gần nhất (9 = sụt áp)
  uint32_t seq;       // số thứ tự gói, đếm lại từ 0 mỗi lần nút khởi động
  uint32_t utc;       // giờ UTC đọc từ DS3231, giây từ 1/1/1970
  uint16_t adc_mv;    // điện áp điểm giữa cầu phân áp, mV
  int16_t temp_c100;  // nhiệt độ danh định x100 (chưa hiệu chuẩn)
} Packet;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  int rssi = info->rx_ctrl->rssi;
#else
void onRecv(const uint8_t *mac, const uint8_t *data, int len) {
  int rssi = 0;
#endif
  if (len != (int)sizeof(Packet)) return;
  Packet p;
  memcpy(&p, data, sizeof(p));
  if (p.magic != 0xA5 || p.ver != 1) return;
  // Một gói = một dòng: DATA,nút,seq,utc,adc_mv,nhiệt độ,flags,rst,rssi
  Serial.printf("DATA,%u,%lu,%lu,%u,%.2f,%u,%u,%d\n", (unsigned)p.node, (unsigned long)p.seq,
                (unsigned long)p.utc, (unsigned)p.adc_mv, p.temp_c100 / 100.0, (unsigned)p.flags,
                (unsigned)p.rst, rssi);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  Serial.println();
  Serial.print("TRAM NHAN MAC ");
  Serial.println(WiFi.macAddress());
  if (esp_now_init() != ESP_OK) {
    Serial.println("LOI: khong khoi dong duoc ESP-NOW");
    while (true) delay(1000);
  }
  esp_now_register_recv_cb(onRecv);
  Serial.println("TRAM NHAN SAN SANG");
}

void loop() {
  delay(1000);
}
