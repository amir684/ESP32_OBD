# ESP32 OBD-II Toyota HV Current Monitor

![Platform](https://img.shields.io/badge/platform-ESP32-blue.svg)
![Framework](https://img.shields.io/badge/framework-Arduino-00979D.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

מד זרם HV (High Voltage) בזמן אמת לרכבי טויוטה היברידיים, עם חיבור Bluetooth לאבחון OBD-II והצגה על מסך LCD.

## 📋 תיאור הפרויקט

פרויקט זה קורא את זרם המתח הגבוה (HV) מסוללת ההיברידית של רכבי טויוטה באמצעות פרוטוקול OBD-II. הנתונים נקראים דרך חיבור Bluetooth לממשק ELM327 ומוצגים על מסך LCD 1602 I2C.

### מה זה עושה?
- 📊 קריאת זרם HV בזמן אמת (עדכון כל 200ms)
- 🔋 הצגת מצב הסוללה: טעינה, פריקה או מנוחה
- 📱 חיבור אלחוטי דרך Bluetooth
- 🖥️ תצוגה ברורה על LCD 1602
- 🔧 פילטר דיגיטלי לקריאות יציבות

## 🛠️ חומרה נדרשת

### רכיבים
| רכיב | תיאור | כמות |
|------|--------|------|
| ESP32 DevKit V1 | מיקרו-בקר עם Bluetooth | 1 |
| LCD 1602 I2C | מסך 16x2 עם ממשק I2C | 1 |
| ELM327 Bluetooth | מתאם OBD-II עם Bluetooth | 1 |
| כבלים | חיבור Dupont | לפי צורך |
| ספק כח | 5V USB או מצת הרכב | 1 |

### פינים (Pinout)

#### ESP32 ↔️ LCD 1602 I2C
```
ESP32          LCD I2C
─────────────────────────
GPIO 21   →    SDA
GPIO 22   →    SCL
3.3V      →    VCC
GND       →    GND
```

#### הערות חשובות
- ⚠️ **כתובת I2C של ה-LCD**: `0x27` (ניתן לשנות בשורה 6 בקוד)
- 📍 אם ה-LCD לא עובד, נסה לסרוק כתובות I2C: `i2cdetect`
- 🔌 ESP32 מחובר למחשב/ספק כח דרך USB
- 📡 ELM327 מחובר לרכב דרך שקע OBD-II (בדרך כלל מתחת להגה)

### מפרט טכני ELM327
```
כתובת MAC:  98:D3:33:F5:C0:E6  (ניתן לשנות בשורה 10)
PIN Code:    3080               (ניתן לשנות בשורה 180)
פרוטוקול:   ISO 15765-4 (CAN)
```

## 🚗 רכבים נתמכים

הקוד מותאם **לרכבי טויוטה/לקסוס היברידיים**:
- Toyota Prius (Gen 2, 3, 4)
- Toyota Aqua
- Toyota C-HR Hybrid
- Lexus CT200h
- Lexus ES Hybrid
- ועוד רכבים עם מערכת THS (Toyota Hybrid System)

### פרמטרים טכניים
- **DID (Data Identifier)**: `221F9A2` (זרם HV)
- **CAN Header**: `7D2` (ECU של מערכת ההיברידית)
- **תגובה**: `7DA` (תגובת ECU)
- **פורמט זרם**: 2 bytes signed, Big Endian, יחידות: 0.1A

## 📦 התקנה

### 1. הכנת הסביבה
```bash
# התקן PlatformIO
pip install platformio

# שכפל את הפרויקט
git clone https://github.com/amir684/ESP32_OBD.git
cd ESP32_OBD
```

### 2. קומפילציה והעלאה
```bash
# קומפילציה
pio run

# העלאה ל-ESP32 (חבר את ה-ESP32 ב-USB)
pio run --target upload

# צפייה בפלט Serial (אופציונלי)
pio device monitor
```

### 3. התאמה אישית

#### שינוי כתובת MAC של ELM327
```cpp
// שורה 10 ב-main.cpp
uint8_t OBD_ADDR[6] = { 0x98, 0xD3, 0x33, 0xF5, 0xC0, 0xE6 };
```

איך למצוא את הכתובת? חפש את ה-ELM327 דרך הטלפון שלך בהגדרות Bluetooth.

#### שינוי כתובת I2C של LCD
```cpp
// שורה 6 ב-main.cpp
LiquidCrystal_I2C lcd(0x27, 16, 2);  // שנה את 0x27 לכתובת שלך
```

#### שינוי פינים של I2C
```cpp
// שורה 169 ב-main.cpp
Wire.begin(21, 22);  // SDA, SCL
```

## 📊 פרטי הקוד

### מבנה הקוד
```
src/
└── main.cpp         # קוד ראשי
    ├── ISO-TP       # מימוש פרוטוקול ISO-TP
    ├── ELM327       # אתחול ELM327
    ├── LCD UI       # ממשק תצוגה
    └── Loop         # לולאה ראשית
```

### עקרונות עבודה

#### 1. חיבור Bluetooth
```cpp
SerialBT.begin("ESP32-OBD", true);  // Master mode
SerialBT.setPin("3080");
SerialBT.connect(OBD_ADDR);
```

#### 2. פרוטוקול ISO-TP
הקוד מממש פרוטוקול ISO-TP (ISO 15765-2) לתקשורת עם ה-ECU:
- **Single Frame**: הודעה בודדת (עד 7 bytes)
- **First Frame**: תחילת הודעה מרובה frames
- **Consecutive Frame**: המשך הודעה

#### 3. פילטר דיגיטלי
```cpp
filtA = 0.85f * filtA + 0.15f * amps;  // Low-pass filter
```
מסנן IIR פשוט להחלקת הנתונים.

#### 4. תצוגה על LCD
```
שורה 1: I: +12.3A        (זרם HV)
שורה 2: DISCHG  OBD:OK   (מצב)
```

מצבים:
- `DISCHG` - פריקה (מנוע חשמלי פועל)
- `CHG/REG` - טעינה/רגנרציה (בלימה)
- `IDLE` - מנוחה

## 🔧 פתרון בעיות

### ה-ESP32 לא מתחבר ל-ELM327
1. ✅ בדוק שה-ELM327 דלוק (LED מהבהב)
2. ✅ ודא שכתובת ה-MAC נכונה
3. ✅ נסה לאפס את ה-ELM327 (נתק והחזר חיבור)
4. ✅ בדוק ש-PIN Code תואם (3080 או 1234)

### LCD לא מציג כלום
1. ✅ בדוק חיבורי הפינים (SDA/SCL)
2. ✅ ודא שכתובת I2C נכונה (0x27)
3. ✅ בדוק שה-backlight דלוק
4. ✅ התאם את הקונטרסט (פוטנציומטר מאחורי ה-LCD)

### אין נתונים מהרכב
1. ✅ ודא שהרכב דלוק (מפתח ב-ON)
2. ✅ בדוק שה-ELM327 מחובר לשקע OBD-II
3. ✅ נסה פקודות אבחון דרך Serial Monitor:
   ```
   ATZ      - Reset
   0100     - Test connectivity
   ```

### הנתונים קופצים/לא יציבים
הקוד כבר כולל פילטר, אבל אפשר להגדיל אותו:
```cpp
filtA = 0.90f * filtA + 0.10f * amps;  // פילטר חזק יותר
```

## 📈 שיפורים עתידיים

- [ ] הוספת קריאת SOC (State of Charge) סוללה
- [ ] שמירת נתונים ל-SD Card
- [ ] גרפים בזמן אמת
- [ ] WiFi + Web Server
- [ ] תמיכה ברכבי Hyundai/Kia
- [ ] לוג שגיאות DTC

## 📄 רישיון

MIT License - ראה קובץ LICENSE

## 🤝 תרומות

Pull Requests מתקבלים בברכה! אנא פתח Issue קודם לשינויים גדולים.

## 📞 יצירת קשר

- **GitHub**: [@amir684](https://github.com/amir684)
- **Email**: amir684684@gmail.com

## 🙏 תודות

- **ELM327** - Elm Electronics
- **Arduino-ESP32** - Espressif Systems
- **LiquidCrystal_I2C** - F. Malpartida

---

**⚠️ אזהרה**: שימוש בכלי זה הוא באחריותך בלבד. אל תנהג בזמן תצפית בנתונים. שינויים לא נכונים במערכת ה-OBD עלולים לפגוע ברכב.

**Made with ❤️ for Toyota Hybrid Owners**
