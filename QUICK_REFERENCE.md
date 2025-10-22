# Quick Reference Card - ESP32 GPS Tracker with SIM800L

## Pin Assignments

```
ESP32          →    GPS NEO-6M
GPIO 16 (RX2)  ←    TXD
GPIO 17 (TX2)  →    RXD
3.3V           →    VCC
GND            →    GND

ESP32          →    SIM800L
GPIO 25 (RX1)  ←    TXD
GPIO 26 (TX1)  →    RXD
GND            →    GND
                    VCC → 5V/2A External Supply!

ESP32
GPIO 2         →    LED (Activity Indicator)
```

## Must Configure

```cpp
// Line ~35: Your carrier's APN
const char* apn = "internet";

// Line ~40: Your server endpoint  
const char* serverUrl = "http://yourserver.com/api/gps";

// Line ~48: How often to send data
const unsigned long DATA_SEND_INTERVAL_MS = 30000;  // 30 seconds
```

## Common APNs

| Carrier | APN |
|---------|-----|
| T-Mobile (US) | fast.t-mobile.com |
| AT&T (US) | phone |
| Hologram (IoT) | hologram |
| Generic | internet |

## Serial Monitor Commands

Open Serial Monitor at **115200 baud** to see:

```
✓ GPS Serial started
✓ Setting GPS Dynamic Model to: 4
✓ Sending UBX-CFG-NAV5 command to GPS...
✓ GPS Dynamic Model set successfully (ACK received)
✓ Configuration saved. GPS will retain Automotive mode after power cycle.
✓ SIM800L Serial started
✓ AT Command: AT → Response: OK
✓ Registered on network
✓ GPRS connected successfully
✓ GPS Logged: ...
✓ Sending GPS data to server...
✓ Data sent successfully!
```

## LED Behavior

- **Blinking**: GPS data being received
- **Solid OFF**: No GPS data (normal, wait for fix)

## Web Interface

```
Connect to WiFi:
SSID: UlTeRa_SpAcE
Password: 12345678

Open Browser: http://192.168.4.1
```

**Interface shows:**
- GPS coordinates & stats
- SIM800L status
- GPRS connection status
- Last data transmission time
- Download GPS log (CSV)

## JSON Data Format

```json
{
  "latitude": 37.774929,
  "longitude": -122.419416,
  "altitude": 45.20,
  "speed": 12.50,
  "course": 180.00,
  "satellites": 8,
  "hdop": 1.20,
  "timestamp": "2023/10/08 14:30:25"
}
```

## Status Indicators

### SIM800L Status
- **Initializing**: Module starting up
- **Network OK**: Registered on network ✓
- **SIM Error**: No SIM or SIM not ready
- **No Network**: Can't register on network

### GPRS Status
- **Connected**: Ready to send data ✓
- **Connecting**: Establishing connection
- **Disconnected**: Not connected
- **Failed**: Connection failed

## Troubleshooting Checklist

### ⚠️ Power Issues (Most Common!)
- [ ] SIM800L powered by external 5V/2A supply?
- [ ] Common ground between ESP32 and SIM800L?
- [ ] Power supply can deliver 2A peak current?

### 📡 Network Issues
- [ ] SIM card inserted correctly?
- [ ] SIM card has active data plan?
- [ ] Antenna connected to SIM800L?
- [ ] GSM coverage in your area?
- [ ] PIN disabled on SIM card?

### 🔧 Configuration Issues
- [ ] Correct APN for your carrier?
- [ ] Server URL is HTTP (not HTTPS)?
- [ ] Server accessible from internet?
- [ ] Wiring matches pin assignments?

### 🐛 Software Issues
- [ ] Serial Monitor at 115200 baud?
- [ ] Sketch uploaded successfully?
- [ ] ESP32 board selected in Arduino IDE?
- [ ] Required libraries installed?

## AT Commands (Manual Testing)

Send these via Serial Monitor to test SIM800L:

```
AT              → Should respond: OK
AT+CSQ          → Signal quality (10-31 = good)
AT+CREG?        → Network registration status
AT+SAPBR=2,1    → Check GPRS connection
AT+CIPSTATUS    → Connection status
```

## Data Costs

**Typical Usage:**
- Per transmission: ~200 bytes
- Every 30 seconds: ~576 KB/day
- Monthly: ~17 MB/month

**Recommended Plans:**
- IoT SIM cards: 10-50 MB/month
- Cost: $2-10/month typical

## Performance Tips

### Reduce Data Costs
```cpp
// Send less frequently
const unsigned long DATA_SEND_INTERVAL_MS = 60000;  // 60 seconds

// Or only when moving significantly
// (already implemented in sketch)
```

### Improve Reliability
```cpp
// Check GPRS more frequently
const unsigned long GPRS_CHECK_INTERVAL_MS = 30000;  // 30 seconds
```

## File Locations

```
sketch_oct6a/
├── sketch_oct6a.ino         → Main sketch (updated)
├── UPDATE_SUMMARY.md        → What changed
├── SIM800L_README.md        → Full integration guide
├── WIRING_DIAGRAM.md        → Hardware connections
├── SERVER_EXAMPLES.md       → Server code examples
├── GPS_DYNAMIC_MODEL.md     → GPS configuration guide
├── config_template.h        → Configuration template
└── QUICK_REFERENCE.md       → This file
```

## Emergency Reset

If system hangs or behaves strangely:

1. **Hardware Reset**: Press ESP32 RESET button
2. **SIM800L Reset**: Power cycle SIM800L
3. **Factory Reset**: Re-upload sketch
4. **Clear Storage**: Add this in `setup()`:
   ```cpp
   LittleFS.format();  // Clears all data!
   ```

## Version Info

- ESP32 Arduino Core: 3.0.5+
- TinyGPSPlus: Latest
- ESPAsyncWebServer: v1.2.3+
- SIM800L: Any variant (L, C, etc.)

## Safety Notes

⚠️ **Do NOT:**
- Power SIM800L from ESP32 pins
- Remove SIM while powered
- Power on without antenna
- Use without proper grounding

✅ **Always:**
- Use external power for SIM800L
- Connect antenna before power-on
- Share common ground
- Monitor current draw

## Quick Start (First Time)

1. **Hardware**: Wire as shown in WIRING_DIAGRAM.md
2. **Power**: External 5V/2A to SIM800L VCC
3. **SIM**: Insert SIM, antenna connected
4. **Configure**: Update APN and server URL
5. **Upload**: Flash sketch to ESP32
6. **Monitor**: Open Serial Monitor (115200)
7. **Wait**: GPS fix (1-5 min), network (30-60 sec)
8. **Verify**: Check web interface and server logs

## Support

**Issues?**
1. Check Serial Monitor for errors
2. Verify power supply (most common!)
3. Review troubleshooting section
4. Check all .md files for details

**Working?**
- LED blinks = GPS data
- Web shows "Network OK" = SIM800L good
- Web shows "Connected" = GPRS ready
- "Data sent successfully" = All working! 🎉

---

**Keep this card handy during setup and testing!**

Print it out or keep it on a second monitor. 📋✨
