/*
 * ESP32 OBD-II HV Battery Current Monitor (Arduino IDE version)
 *
 * Board: ESP32 Dev Module
 * Libraries required:
 *   - LiquidCrystal_I2C (by Frank de Brabander)
 *   - BluetoothSerial (built-in with ESP32 board package)
 */

#include <BluetoothSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------- LCD 1602 I2C ----------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------- BT / OBD ----------
BluetoothSerial SerialBT;
uint8_t OBD_ADDR[6] = { 0x98, 0xD3, 0x33, 0xF5, 0xC0, 0xE6 };

String line;

static float filtA = 0.0f;
static float lastA = 0.0f;
static uint32_t lastDataMs = 0;
static uint32_t lastElmInit = 0;

void sendCmd(const char* cmd, int waitMs = 120) {
  SerialBT.print(cmd);
  SerialBT.print("\r");
  delay(waitMs);
}

// helper for hex nibble parsing
static int hexNib(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

// ===== parse: "7DA2100FF7900000000" -> bytes[8]
static bool parseCAN8(const String& s, uint8_t bytes[8]) {
  if (s.length() < 3 + 16) return false;
  if (!s.startsWith("7DA")) return false;

  int p = 3;
  for (int i = 0; i < 8; i++) {
    int hi = hexNib(s[p++]);
    int lo = hexNib(s[p++]);
    if (hi < 0 || lo < 0) return false;
    bytes[i] = (uint8_t)((hi << 4) | lo);
  }
  return true;
}

// ===== ISO-TP state
static bool collecting = false;
static int expectedLen = 0;
static int haveLen = 0;
static uint8_t payload[64];

static void resetIsoTp() {
  collecting = false;
  expectedLen = 0;
  haveLen = 0;
}

static void feedIsoTp(const uint8_t b[8]) {
  uint8_t pciType = (b[0] >> 4) & 0x0F;

  if (pciType == 0x0) { // Single Frame
    int len = b[0] & 0x0F;
    if (len <= 0 || len > 7) return;
    expectedLen = len;
    haveLen = 0;
    for (int i = 0; i < len; i++) payload[haveLen++] = b[1 + i];
    collecting = false;
  }
  else if (pciType == 0x1) { // First Frame
    int len = ((b[0] & 0x0F) << 8) | b[1];
    if (len <= 0 || len > (int)sizeof(payload)) return;

    expectedLen = len;
    haveLen = 0;
    for (int i = 2; i < 8 && haveLen < expectedLen; i++) {
      payload[haveLen++] = b[i];
    }
    collecting = true;
  }
  else if (pciType == 0x2) { // Consecutive Frame
    if (!collecting) return;
    for (int i = 1; i < 8 && haveLen < expectedLen; i++) {
      payload[haveLen++] = b[i];
    }
    if (haveLen >= expectedLen) collecting = false;
  }
}

static bool isComplete() {
  return (expectedLen > 0 && haveLen >= expectedLen);
}

static bool isOurDid() {
  return (expectedLen >= 3 && payload[0] == 0x62 && payload[1] == 0x1F && payload[2] == 0x9A);
}

static bool extractCurrent(float &amps) {
  if (!isComplete() || !isOurDid()) return false;
  if (expectedLen < 9) return false;

  int16_t raw = (int16_t)((payload[7] << 8) | payload[8]);
  amps = (float)raw * 0.1f;
  return true;
}

void elmInitLikeApp() {
  resetIsoTp();

  sendCmd("ATZ", 900);
  sendCmd("ATD0", 250);
  sendCmd("ATE0", 120);
  sendCmd("ATH1", 120);
  sendCmd("ATSP0", 250);
  sendCmd("ATM0", 120);
  sendCmd("ATS0", 120);
  sendCmd("ATAT1", 120);
  sendCmd("ATAL", 120);
  sendCmd("ATST64", 120);

  sendCmd("ATSH7DF", 120);
  sendCmd("0100", 250);
  sendCmd("ATST64", 120);
  sendCmd("0100", 250);

  sendCmd("ATSH7D2", 120);
}

// ---------- LCD UI ----------
void lcdShow(bool connected) {
  bool hasData = (millis() - lastDataMs) < 1500;

  lcd.setCursor(0, 0);
  if (hasData) {
    char buf[17];
    snprintf(buf, sizeof(buf), "I:%7.1fA      ", -lastA);
    lcd.print(buf);
  } else {
    lcd.print("I:   --.-A      ");
  }

  lcd.setCursor(0, 1);
  if (!connected) {
    lcd.print("OBD:DISCONNECTED");
    return;
  }

  if (!hasData) {
    lcd.print("OBD:OK  NO DATA ");
    return;
  }

  if (lastA > 0.2f)      lcd.print("DISCHG    OBD:OK  ");
  else if (lastA < -0.2f) lcd.print("CHG       OBD:OK  ");
  else                  lcd.print("IDLE      OBD:OK  ");
}

bool connectOBD() {
  int attempt = 0;
  while (true) {
    attempt++;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("BT Connecting...");
    lcd.setCursor(0, 1);
    char buf[17];
    snprintf(buf, sizeof(buf), "Attempt: %d", attempt);
    lcd.print(buf);
    Serial.printf("BT connect attempt %d\n", attempt);

    bool ok = SerialBT.connect(OBD_ADDR);
    if (ok) {
      Serial.println("BT connected!");
      return true;
    }

    Serial.println("BT connect failed, retrying in 3s...");
    lcd.setCursor(0, 1);
    lcd.print("Failed, retry...");
    delay(3000);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ESP32 OBD...");

  SerialBT.begin("ESP32-OBD", true);
  SerialBT.setPin("3080");

  connectOBD();

  delay(400);
  elmInitLikeApp();
  lastElmInit = millis();

  lcd.clear();
  lcd.print("OBD OK");
  lcd.setCursor(0, 1);
  lcd.print("Polling 221F9A2");
  delay(600);

  lcd.clear();
}

void loop() {
  static uint32_t lastReq = 0;

  // אם נותק - מתחבר מחדש
  if (!SerialBT.connected()) {
    Serial.println("BT disconnected! Reconnecting...");
    lcdShow(false);
    filtA = 0.0f;
    lastA = 0.0f;
    lastDataMs = 0;
    resetIsoTp();
    line = "";

    delay(1000);
    connectOBD();
    delay(400);
    elmInitLikeApp();
    lastElmInit = millis();
    lastDataMs = 0;
    lcd.clear();
    return;
  }

  // אם מחובר אבל אין דאטה כבר 8 שניות - אתחל ELM מחדש
  if (lastDataMs > 0 && (millis() - lastDataMs > 8000) && (millis() - lastElmInit > 15000)) {
    Serial.println("No data for 8s, re-initializing ELM...");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No data...");
    lcd.setCursor(0, 1);
    lcd.print("Re-init ELM    ");
    resetIsoTp();
    line = "";
    elmInitLikeApp();
    lastElmInit = millis();
    lastDataMs = millis();
    lcd.clear();
    return;
  }

  // אם עדיין לא קיבלנו דאטה בכלל אחרי 10 שניות מאתחול
  if (lastDataMs == 0 && lastElmInit > 0 && (millis() - lastElmInit > 10000)) {
    Serial.println("Never got data after init, re-initializing ELM...");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No response...");
    lcd.setCursor(0, 1);
    lcd.print("Re-init ELM    ");
    resetIsoTp();
    line = "";
    elmInitLikeApp();
    lastElmInit = millis();
    lcd.clear();
    return;
  }

  // פולינג
  if (millis() - lastReq > 50) {
    lastReq = millis();
    SerialBT.print("221F9A2\r");
  }

  // קריאה
  while (SerialBT.available()) {
    char c = (char)SerialBT.read();

    if (c == '\r' || c == '\n') {
      line.trim();
      if (line.length()) {
        if (line.startsWith("7DA")) {
          uint8_t b[8];
          if (parseCAN8(line, b)) {
            feedIsoTp(b);
            float amps;
            if (extractCurrent(amps)) {
              filtA = 0.85f * filtA + 0.15f * amps;
              lastA = filtA;
              lastDataMs = millis();

              Serial.print("HV Current: ");
              Serial.printf("%+.1f A\n", lastA);

              resetIsoTp();
            }
          }
        }
      }
      line = "";
    } else {
      if (c != '>') {
        line += c;
        if (line.length() > 400) line = "";
      }
    }
  }

  // עדכון LCD
  static uint32_t lastUi = 0;
  if (millis() - lastUi > 50) {
    lastUi = millis();
    lcdShow(SerialBT.connected());
  }
}
