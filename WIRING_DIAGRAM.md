# Wiring Diagram - ESP32 GPS Tracker with SIM800L

## Complete Wiring Schematic

```
┌─────────────────────────────────────────────────────────────────┐
│                    ESP32 GPS TRACKER SYSTEM                      │
└─────────────────────────────────────────────────────────────────┘

┌──────────────┐
│   ESP32      │
│   DevKit     │
├──────────────┤
│              │
│ 3.3V ────────┼──────────────┐
│              │              │
│ GND  ────────┼──────┬───────┼───────┬──────────────┐
│              │      │       │       │              │
│ GPIO16 ──────┼──────┼───────┼───────┼──────┐       │
│              │      │       │       │      │       │
│ GPIO17 ──────┼──────┼───────┼───────┼───┐  │       │
│              │      │       │       │   │  │       │
│ GPIO25 ──────┼──────┼───────┼───────┼───┼──┼───┐   │
│              │      │       │       │   │  │   │   │
│ GPIO26 ──────┼──────┼───────┼───────┼───┼──┼───┼───┼──┐
│              │      │       │       │   │  │   │   │  │
│ GPIO2  ──────┼──────┼───────┼───────┼───┼──┼───┼───┼──┼─── LED (Activity)
│              │      │       │       │   │  │   │   │  │
│ 5V   ────────┼──────┼───────┼───────┼───┼──┼───┼───┼──┼─── (NOT for SIM800L!)
│              │      │       │       │   │  │   │   │  │
└──────────────┘      │       │       │   │  │   │   │  │
                      │       │       │   │  │   │   │  │
                ┌─────▼────┐  │       │   │  │   │   │  │
                │ NEO-6M   │  │       │   │  │   │   │  │
                │   GPS    │  │       │   │  │   │   │  │
                ├──────────┤  │       │   │  │   │   │  │
                │ VCC  ◄───┼──┘       │   │  │   │   │  │
                │ GND  ◄───┼──────────┘   │  │   │   │  │
                │ TXD  ─────────────────► │  │   │   │  │
                │ RXD  ◄───────────────────┘  │   │   │  │
                └──────────┘                  │   │   │  │
                                              │   │   │  │
     ┌────────────────────────────────────────┘   │   │  │
     │                                            │   │  │
     │  ┌─────────────────────────────────────────┘   │  │
     │  │                                             │  │
     │  │  ┌──────────────────────────────────────────┘  │
     │  │  │                                             │
     │  │  │  ┌──────────────────────────────────────────┘
     │  │  │  │
     ▼  ▼  ▼  ▼
  ┌──────────────┐
  │  SIM800L     │  ⚠️  REQUIRES EXTERNAL POWER!
  │   Module     │
  ├──────────────┤
  │ VCC  ◄───────┼──── 5V @ 2A External Power Supply
  │ GND  ◄───────┼──── Common Ground with ESP32
  │ TXD  ─────►  │     (GPIO25 on ESP32)
  │ RXD  ◄────   │     (GPIO26 on ESP32)
  │ RST          │     (Optional)
  └──────────────┘

┌───────────────────────────────────────────────────┐
│   EXTERNAL POWER SUPPLY FOR SIM800L               │
│                                                   │
│   ┌─────────────┐                                │
│   │ 5V/2A Power │                                │
│   │   Adapter   │                                │
│   └──────┬──────┘                                │
│          │                                        │
│          ├─── (+) ──► SIM800L VCC                │
│          │                                        │
│          └─── (-) ──► Common GND (with ESP32)    │
│                                                   │
│   Alternative:                                    │
│   ┌──────────────┐                               │
│   │ 3.7V LiPo    │                               │
│   │ 2000mAh+     │ ─► Voltage Regulator ─► VCC  │
│   │ Battery      │                               │
│   └──────────────┘                               │
└───────────────────────────────────────────────────┘
```

## Pin Assignment Summary

### ESP32 Pin Usage

| GPIO | Function | Connected To | Direction |
|------|----------|--------------|-----------|
| 16 | UART2 RX | GPS TXD | Input |
| 17 | UART2 TX | GPS RXD | Output |
| 25 | UART1 RX | SIM800L TXD | Input |
| 26 | UART1 TX | SIM800L RXD | Output |
| 2 | LED | Activity Indicator | Output |
| 3.3V | Power | GPS VCC | Power |
| GND | Ground | GPS & SIM800L GND | Ground |

### NEO-6M GPS Module

| GPS Pin | ESP32 Pin | Description |
|---------|-----------|-------------|
| VCC | 3.3V | Power (3.3V, ~50mA) |
| GND | GND | Ground |
| TXD | GPIO16 | GPS sends data to ESP32 |
| RXD | GPIO17 | GPS receives data from ESP32 |

### SIM800L Module

| SIM800L Pin | Connection | Description |
|-------------|------------|-------------|
| VCC | External 5V/2A | Power supply ⚠️ |
| GND | Common GND | Ground (shared with ESP32) |
| TXD | GPIO25 | SIM800L sends to ESP32 |
| RXD | GPIO26 | SIM800L receives from ESP32 |
| RST | (Optional) | Reset pin |
| NET | Antenna | GSM antenna |

## Important Notes

### 🔴 Power Supply Critical!

**SIM800L Power Requirements:**
- Voltage: 3.7V - 4.2V (typical 5V with regulator on module)
- Current: 2A peak during transmission!
- **DO NOT** power from ESP32's pins
- **MUST** use external power supply or LiPo battery

**What happens with insufficient power:**
- Module resets during transmission
- AT commands fail randomly
- GPRS connection drops
- "UNDER-VOLTAGE" warnings

### ✅ Correct Power Setup Options

**Option 1: External Power Adapter**
```
5V/2A Wall Adapter ──┬─► SIM800L VCC
                     └─► ESP32 VIN (optional, can use separate USB)
Common GND between all modules
```

**Option 2: LiPo Battery**
```
3.7V LiPo (2000mAh+) ──► SIM800L VCC
Common GND between all modules
USB Power for ESP32
```

**Option 3: Shared Power with Buck Converter**
```
12V Power Supply ──► Buck Converter (5V/3A) ──┬─► SIM800L VCC
                                               └─► ESP32 VIN
Common GND between all modules
```

### 📡 Antenna

**Critical**: SIM800L requires antenna connected before powering on!
- Use proper GSM antenna (900/1800 MHz)
- Poor antenna = poor signal = connection failures

### 🔌 Serial Communication

**UART Pins:**
- ESP32 has 3 hardware serial ports (UART0, UART1, UART2)
- UART0: USB (Serial Monitor)
- UART1: SIM800L (GPIO25, GPIO26)
- UART2: GPS (GPIO16, GPIO17)

**Voltage Levels:**
- ESP32: 3.3V logic
- SIM800L: 2.8V logic (usually tolerates 3.3V)
- GPS: 3.3V logic
- ✅ Direct connection works for most modules
- ⚠️ If experiencing issues, use level shifters

## Testing Connections

### 1. Test GPS
Upload sketch and check Serial Monitor for:
```
GPS Serial started
GPS Logged: 2023/10/08...
```

### 2. Test SIM800L
Check Serial Monitor for:
```
SIM800L Serial started
AT Command: AT
Response: OK
SIM card not ready! OR Registered on network
```

### 3. Test Power
- Measure voltage at SIM800L VCC: Should be 4.0-4.5V
- During transmission, voltage should not drop below 3.8V
- If voltage drops significantly, power supply is insufficient

## Breadboard Layout (Simplified)

```
       ESP32           GPS Module      SIM800L
    ┌─────────┐       ┌────────┐     ┌────────┐
    │         │       │        │     │        │
    │  GPIO16├───────┤TXD     │     │        │
    │  GPIO17├───────┤RXD     │     │        │
    │         │       │        │     │        │
    │  GPIO25├───────┼────────┼─────┤TXD     │
    │  GPIO26├───────┼────────┼─────┤RXD     │
    │         │       │        │     │        │
    │    3.3V├───────┤VCC     │     │        │
    │     GND├───────┤GND     ├─────┤GND     │
    │         │       └────────┘     │        │
    └─────────┘                      │  VCC   ├──── 5V/2A Supply
                                     └────────┘
```

## Troubleshooting Connection Issues

| Problem | Check |
|---------|-------|
| GPS not responding | Verify RX/TX not swapped, check 3.3V power |
| SIM800L not responding | Check power supply (most common!), verify baud rate |
| Both modules interfere | Ensure separate serial ports (no pin conflicts) |
| System resets randomly | SIM800L power issue - use external supply |
| LED not blinking | Normal if no GPS data, check GPS has sky view |

## Safety Checklist

- ✅ SIM800L powered from external 5V/2A supply
- ✅ Common ground between all modules
- ✅ Antenna connected to SIM800L
- ✅ SIM card inserted in SIM800L (contacts facing down)
- ✅ GPS module has clear view of sky
- ✅ No short circuits on breadboard
- ✅ Voltage levels verified with multimeter
- ✅ USB cable provides stable power to ESP32

## Next Steps

1. Wire everything according to diagram
2. Upload the sketch
3. Open Serial Monitor (115200 baud)
4. Wait for GPS fix (may take 1-5 minutes)
5. Wait for SIM800L network registration (30-60 seconds)
6. Monitor GPRS connection and data transmission
7. Check web interface for status

Good luck with your build! 🚀
