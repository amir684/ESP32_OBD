#include <BluetoothSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------- LCD 1602 I2C ----------
LiquidCrystal_I2C lcd(0x27, 16, 2);   // הכתובת שלך

// ---------- BT / OBD ----------
BluetoothSerial SerialBT;
uint8_t OBD_ADDR[6] = { 0x98, 0xD3, 0x33, 0xF5, 0xC0, 0xE6 };

String line;

// פילטר עדין
static float filtA = 0.0f;
static float lastA = 0.0f;
static uint32_t lastDataMs = 0;

// שולח פקודה עם CR
void sendCmd(const char* cmd, int waitMs = 120) {
  SerialBT.print(cmd);
  SerialBT.print("\r");
  delay(waitMs);
}

// ===== parse: "7DA2100FF7900000000" -> bytes[8]
static bool parseCAN8(const String& s, uint8_t bytes[8]) {
  if (s.length() < 3 + 16) return false;
  if (!s.startsWith("7DA")) return false;

  int p = 3;
  auto hexNib = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
  };

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
  // מצפה: 62 1F 9A ...
  return (expectedLen >= 3 && payload[0] == 0x62 && payload[1] == 0x1F && payload[2] == 0x9A);
}

// הזרם ב payload[7..8]
static bool extractCurrent(float &amps) {
  if (!isComplete() || !isOurDid()) return false;
  if (expectedLen < 9) return false;

  int16_t raw = (int16_t)((payload[7] << 8) | payload[8]); // XXYY
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

  // שורה 1: זרם
  lcd.setCursor(0, 0);
  if (hasData) {
    // "I: +12.3A"
    char buf[17];
    snprintf(buf, sizeof(buf), "I:%+7.1fA      ", lastA);
    lcd.print(buf);
  } else {
    lcd.print("I:   --.-A      ");
  }

  // שורה 2: מצב + BT
  lcd.setCursor(0, 1);
  if (!connected) {
    lcd.print("OBD:DISCONNECTED");
    return;
  }

  if (!hasData) {
    lcd.print("OBD:OK  NO DATA ");
    return;
  }

  if (lastA > 0.2f)      lcd.print("DISCHG  OBD:OK  ");
  else if (lastA < -0.2f) lcd.print("CHG/REG OBD:OK  ");
  else                  lcd.print("IDLE    OBD:OK  ");
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // LCD init
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ESP32 OBD...");
  lcd.setCursor(0, 1);
  lcd.print("Connecting...");

  // BT init (בווינדוס זה יעבוד לך כ-Master)
  SerialBT.begin("ESP32-OBD", true);
  SerialBT.setPin("3080");

  bool ok = SerialBT.connect(OBD_ADDR);
  Serial.printf("connect=%d\n", ok);

  if (!ok) {
    lcd.clear();
    lcd.print("BT CONNECT FAIL ");
    lcd.setCursor(0, 1);
    lcd.print("check MAC/PIN   ");
    return;
  }

  delay(400);
  elmInitLikeApp();

  lcd.clear();
  lcd.print("OBD OK");
  lcd.setCursor(0, 1);
  lcd.print("Polling 221F9A2");
  delay(600);

  lcd.clear();
}

void loop() {
  static uint32_t lastReq = 0;

  // פולינג
  if (SerialBT.connected() && millis() - lastReq > 200) {
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
  if (millis() - lastUi > 200) {
    lastUi = millis();
    lcdShow(SerialBT.connected());
  }

  // אם נותק
  if (!SerialBT.connected()) {
    lcdShow(false);
    delay(300);
  }
}
