// Test 3: đọc đầu dò NTC qua cầu phân áp
// Mạch: GPIO25 -> đầu dò -> GPIO34 -> điện trở 10 kΩ -> GND
const int PIN_POWER = 25;  // cấp điện cho cầu phân áp, chỉ bật lúc đo
const int PIN_ADC = 34;    // đọc điện áp điểm giữa

// Điền số đo thật bằng đồng hồ vạn năng
const float VS_MV = 3300.0;     // điện áp chân 3V3 của board này, mV
const float R_FIXED = 10000.0;  // điện trở 10 kΩ đo được, ohm

const float R0 = 10000.0, T0_K = 298.15, BETA = 3950.0;

uint32_t readMilliVolts() {
  digitalWrite(PIN_POWER, HIGH);
  delay(10);  // chờ điện áp ổn định
  uint32_t sum = 0;
  for (int i = 0; i < 64; i++) sum += analogReadMilliVolts(PIN_ADC);
  digitalWrite(PIN_POWER, LOW);
  return sum / 64;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(PIN_POWER, OUTPUT);
  digitalWrite(PIN_POWER, LOW);
  Serial.println();
  Serial.println("=== TEST 3: ĐẦU DÒ NHIỆT ===");
}

void loop() {
  uint32_t mv = readMilliVolts();
  if (mv < 300) {
    Serial.printf("adc %4lu mV   LỖI: đầu dò hở mạch (lỏng vít hoặc cắm nhầm lỗ)\n", (unsigned long)mv);
  } else if (mv > 3000) {
    Serial.printf("adc %4lu mV   LỖI: đầu dò bị chập\n", (unsigned long)mv);
  } else {
    float r = R_FIXED * (VS_MV / mv - 1.0f);
    float tC = 1.0f / (1.0f / T0_K + logf(r / R0) / BETA) - 273.15f;
    Serial.printf("adc %4lu mV   R = %6.0f ohm   nhiệt độ = %5.2f °C\n", (unsigned long)mv, r, tC);
  }
  delay(2000);
}