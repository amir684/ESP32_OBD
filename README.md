# ESP32 OBD-II Toyota HV Current Monitor

![Platform](https://img.shields.io/badge/platform-ESP32-blue.svg)
![Framework](https://img.shields.io/badge/framework-Arduino-00979D.svg)
![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)

Real-time High Voltage (HV) current monitor for Toyota Hybrid vehicles, featuring Bluetooth OBD-II connectivity and LCD display.

## 📋 Project Overview

This project reads the High Voltage (HV) battery current from Toyota hybrid vehicles using the OBD-II protocol. Data is retrieved via Bluetooth connection to an ELM327 adapter and displayed on a 1602 I2C LCD screen.

### Features
- 📊 Real-time HV current reading (200ms update rate)
- 🔋 Battery state display: Charging, Discharging, or Idle
- 📱 Wireless connection via Bluetooth
- 🖥️ Clear LCD 1602 display
- 🔧 Digital filter for stable readings

## 🛠️ Hardware Requirements

### Components
| Component | Description | Quantity |
|-----------|-------------|----------|
| ESP32 DevKit V1 | Microcontroller with Bluetooth | 1 |
| LCD 1602 I2C | 16x2 LCD with I2C interface | 1 |
| ELM327 Bluetooth | OBD-II adapter with Bluetooth | 1 |
| Jumper Wires | Dupont connection wires | As needed |
| Power Supply | 5V USB or car cigarette lighter | 1 |

### Pinout

#### ESP32 ↔️ LCD 1602 I2C
```
ESP32          LCD I2C
─────────────────────────
GPIO 21   →    SDA
GPIO 22   →    SCL
3.3V      →    VCC
GND       →    GND
```

#### Important Notes
- ⚠️ **LCD I2C Address**: `0x27` (configurable in line 6 of code)
- 📍 If LCD doesn't work, try scanning I2C addresses: `i2cdetect`
- 🔌 ESP32 connects to computer/power supply via USB
- 📡 ELM327 connects to vehicle via OBD-II port (usually below steering wheel)

### ELM327 Specifications
```
MAC Address: 98:D3:33:F5:C0:E6  (configurable in line 10)
PIN Code:    3080                (configurable in line 180)
Protocol:    ISO 15765-4 (CAN)
```

## 🚗 Supported Vehicles

Code is optimized for **Toyota/Lexus Hybrid vehicles**:
- Toyota Prius (Gen 2, 3, 4)
- Toyota Aqua
- Toyota C-HR Hybrid
- Lexus CT200h
- Lexus ES Hybrid
- Other vehicles with THS (Toyota Hybrid System)

### Technical Parameters
- **DID (Data Identifier)**: `221F9A2` (HV current)
- **CAN Header**: `7D2` (Hybrid system ECU)
- **Response**: `7DA` (ECU response)
- **Current Format**: 2 bytes signed, Big Endian, units: 0.1A

## 📦 Installation

### 1. Environment Setup
```bash
# Install PlatformIO
pip install platformio

# Clone the project
git clone https://github.com/amir684/ESP32_OBD.git
cd ESP32_OBD
```

### 2. Compile and Upload
```bash
# Compile
pio run

# Upload to ESP32 (connect ESP32 via USB)
pio run --target upload

# View Serial output (optional)
pio device monitor
```

### 3. Customization

#### Change ELM327 MAC Address
```cpp
// Line 10 in main.cpp
uint8_t OBD_ADDR[6] = { 0x98, 0xD3, 0x33, 0xF5, 0xC0, 0xE6 };
```

How to find the address? Search for the ELM327 via your phone's Bluetooth settings.

#### Change LCD I2C Address
```cpp
// Line 6 in main.cpp
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Change 0x27 to your address
```

#### Change I2C Pins
```cpp
// Line 169 in main.cpp
Wire.begin(21, 22);  // SDA, SCL
```

## 📊 Code Details

### Code Structure
```
src/
└── main.cpp         # Main code
    ├── ISO-TP       # ISO-TP protocol implementation
    ├── ELM327       # ELM327 initialization
    ├── LCD UI       # Display interface
    └── Loop         # Main loop
```

### Working Principles

#### 1. Bluetooth Connection
```cpp
SerialBT.begin("ESP32-OBD", true);  // Master mode
SerialBT.setPin("3080");
SerialBT.connect(OBD_ADDR);
```

#### 2. ISO-TP Protocol
The code implements ISO-TP protocol (ISO 15765-2) for ECU communication:
- **Single Frame**: Single message (up to 7 bytes)
- **First Frame**: Start of multi-frame message
- **Consecutive Frame**: Continuation of message

#### 3. Digital Filter
```cpp
filtA = 0.85f * filtA + 0.15f * amps;  // Low-pass filter
```
Simple IIR filter for data smoothing.

#### 4. LCD Display
```
Line 1: I: +12.3A        (HV Current)
Line 2: DISCHG  OBD:OK   (Status)
```

States:
- `DISCHG` - Discharging (electric motor active)
- `CHG/REG` - Charging/Regeneration (braking)
- `IDLE` - Idle

## 🔧 Troubleshooting

### ESP32 Won't Connect to ELM327
1. ✅ Check that ELM327 is powered (LED blinking)
2. ✅ Verify MAC address is correct
3. ✅ Try resetting ELM327 (disconnect and reconnect)
4. ✅ Check PIN code matches (3080 or 1234)

### LCD Shows Nothing
1. ✅ Check pin connections (SDA/SCL)
2. ✅ Verify I2C address is correct (0x27)
3. ✅ Check backlight is on
4. ✅ Adjust contrast (potentiometer on LCD back)

### No Data from Vehicle
1. ✅ Ensure vehicle is on (key in ON position)
2. ✅ Check ELM327 is connected to OBD-II port
3. ✅ Try diagnostic commands via Serial Monitor:
   ```
   ATZ      - Reset
   0100     - Test connectivity
   ```

### Data is Jumpy/Unstable
Code already includes filtering, but you can increase it:
```cpp
filtA = 0.90f * filtA + 0.10f * amps;  // Stronger filter
```

## 📈 Future Improvements

- [ ] Add SOC (State of Charge) reading
- [ ] Data logging to SD Card
- [ ] Real-time graphs
- [ ] WiFi + Web Server
- [ ] Support for Hyundai/Kia vehicles
- [ ] DTC error logging

## 📄 License

This project is licensed under the Apache License 2.0 - see the [LICENSE](LICENSE) file for details.

## 🤝 Contributing

Pull Requests are welcome! Please open an Issue first for major changes.

## 📞 Contact

- **GitHub**: [@amir684](https://github.com/amir684)
- **Email**: amir684684@gmail.com

## 🙏 Acknowledgments

- **ELM327** - Elm Electronics
- **Arduino-ESP32** - Espressif Systems
- **LiquidCrystal_I2C** - F. Malpartida

---

**⚠️ Warning**: Use this tool at your own risk. Do not drive while viewing data. Incorrect modifications to the OBD system may damage your vehicle.

**Made with ❤️ for Toyota Hybrid Owners**
