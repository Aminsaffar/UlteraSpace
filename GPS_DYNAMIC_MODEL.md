# GPS Dynamic Platform Model Configuration

## Overview

The NEO-6M GPS module has been configured to use **Automotive Dynamic Model (mode 4)** for optimal performance in vehicle tracking applications.

## What is Dynamic Platform Model?

The dynamic platform model tells the GPS receiver what kind of motion to expect, allowing it to optimize its Kalman filter for better accuracy and performance in specific scenarios.

## Available Dynamic Models

| Value | Model | Use Case | Max Altitude | Max Velocity | Max Vert Velocity |
|-------|-------|----------|--------------|--------------|-------------------|
| 0 | Portable | General purpose | 12 km | 310 m/s | 50 m/s |
| 2 | Stationary | Fixed installations | 9 km | 10 m/s | 6 m/s |
| 3 | Pedestrian | Walking/hiking | 9 km | 30 m/s | 20 m/s |
| **4** | **Automotive** | **Cars/vehicles** | **6 km** | **84 m/s (302 km/h)** | **15 m/s** |
| 5 | Sea | Boats/marine | 6 km | 25 m/s | 15 m/s |
| 6 | Airborne <1g | Light aircraft | 50 km | 100 m/s | 100 m/s |
| 7 | Airborne <2g | Aerobatic aircraft | 50 km | 250 m/s | 100 m/s |
| 8 | Airborne <4g | High-performance aircraft | 50 km | 500 m/s | 100 m/s |

## Why Automotive Mode?

**Selected: Mode 4 (Automotive)** is optimized for:

✅ **Vehicle Tracking**
- Best for cars, trucks, motorcycles
- Optimized for road travel patterns
- Handles acceleration/deceleration well

✅ **Performance Benefits**
- Better position accuracy during turns
- Improved tracking during acceleration
- Reduced position jumps
- Better velocity estimation

✅ **Specifications**
- Max velocity: 84 m/s (302 km/h / 188 mph)
- Max vertical velocity: 15 m/s
- Max altitude: 6 km (sufficient for ground vehicles)

## How It Works

### UBX Protocol Configuration

The sketch sends a **UBX-CFG-NAV5** message to configure the GPS:

```cpp
setGPSDynamicModel(4);  // Called in setup()
```

### Message Structure

```
Header: 0xB5 0x62 (UBX sync characters)
Class:  0x06 (CFG - Configuration)
ID:     0x24 (NAV5 - Navigation Engine Settings)
Length: 36 bytes
Payload: Configuration data including dynamic model = 4
Checksum: CRC-8
```

### Configuration Parameters

The function sets:
- **Dynamic Model**: 4 (Automotive)
- **Fix Mode**: Auto 2D/3D
- **Min Elevation**: 5 degrees
- **Position DOP**: 25.0
- **Time DOP**: 25.0
- **Position Accuracy**: 100m
- **Time Accuracy**: 300m

### Persistence

The configuration is **saved to GPS flash memory** using UBX-CFG-CFG command, meaning:
- ✅ Settings persist after power cycle
- ✅ No need to reconfigure on each boot
- ✅ Survives ESP32 reset

## Implementation Details

### When Configuration Happens

```cpp
void setup() {
    // GPS Serial initialized
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
    
    // Wait for GPS to boot
    delay(1000);
    
    // Set Automotive mode
    setGPSDynamicModel(4);
    
    // Continue with rest of setup...
}
```

### Verification

The sketch waits for ACK (acknowledgment) from GPS:
- ✅ **ACK received**: Configuration successful
- ⚠️ **No ACK**: Timeout (may still be applied)

Check Serial Monitor (115200 baud) for:
```
Setting GPS Dynamic Model to: 4
Sending UBX-CFG-NAV5 command to GPS...
GPS Dynamic Model set successfully (ACK received)
Saving configuration to GPS flash memory...
Configuration saved. GPS will retain Automotive mode after power cycle.
```

## Troubleshooting

### Configuration Failed / No ACK

**Possible Causes:**
1. GPS not fully booted when command sent
2. Wiring issues (RX/TX swapped)
3. Baud rate mismatch
4. GPS module doesn't support UBX protocol

**Solutions:**
```cpp
// Increase boot delay
delay(2000);  // Instead of 1000ms

// Try sending twice
setGPSDynamicModel(4);
delay(1000);
setGPSDynamicModel(4);
```

### Verify Current Setting

Uncomment this line in setup():
```cpp
queryGPSDynamicModel();
```

This polls the GPS and prints the current configuration to Serial Monitor.

### Manual Verification with u-center

1. Download u-blox u-center software
2. Connect GPS to PC via USB-UART adapter
3. Open u-center
4. View → Configuration View
5. CFG (Configuration) → NAV5
6. Check "Dynamic Platform Model" = Automotive

## Performance Comparison

### Before (Default: Portable/Pedestrian)
- Position "jumps" during acceleration
- Less smooth velocity tracking
- More susceptible to multipath errors in urban areas

### After (Automotive Mode)
- ✅ Smoother position updates
- ✅ More accurate velocity during acceleration/braking
- ✅ Better handling of consistent directional motion
- ✅ Reduced position drift in urban canyons

## Technical Notes

### UBX Protocol

The NEO-6M supports two protocols:
- **NMEA** (default): Text-based, human-readable
- **UBX**: Binary, allows configuration

This implementation:
- Sends NMEA data for TinyGPS++ parsing
- Accepts UBX commands for configuration
- Best of both worlds!

### Checksum Calculation

```cpp
void calcChecksum(unsigned char* CK, unsigned char* payload, uint8_t length) {
    CK[0] = 0;
    CK[1] = 0;
    for (uint8_t i = 0; i < length; i++) {
        CK[0] += payload[i];
        CK[1] += CK[0];
    }
}
```

Uses 8-bit Fletcher algorithm (standard UBX checksum).

### Configuration Persistence

The UBX-CFG-CFG command saves settings to:
- Battery-backed RAM (BBR)
- Flash memory
- EEPROM (if available)

Settings survive:
- ✅ Power cycle
- ✅ ESP32 reset
- ✅ GPS module reset
- ❌ GPS factory reset (obviously)

## Advanced Usage

### Change Dynamic Model On-The-Fly

You can change modes during runtime:

```cpp
void loop() {
    // Detect if vehicle stopped for extended period
    if (speed < 1.0 && stationaryTime > 300000) {  // 5 minutes
        setGPSDynamicModel(2);  // Switch to Stationary
    }
    
    // Detect if moving again
    if (speed > 5.0) {
        setGPSDynamicModel(4);  // Switch back to Automotive
    }
}
```

### Query Current Settings

```cpp
queryGPSDynamicModel();
```

Prints current NAV5 configuration to Serial Monitor.

### Different Models for Different Vehicles

```cpp
// Motorcycle (high speed, agile)
setGPSDynamicModel(4);  // Automotive

// Delivery drone
setGPSDynamicModel(6);  // Airborne <1g

// Marine vessel
setGPSDynamicModel(5);  // Sea

// Hiking tracker
setGPSDynamicModel(3);  // Pedestrian
```

## References

- **u-blox NEO-6 Receiver Description**: Official documentation
- **UBX Protocol Specification**: Message format details
- **u-blox u-center**: Configuration software (Windows)

## FAQs

**Q: Do I need to reconfigure after each power cycle?**  
A: No! Configuration is saved to GPS flash memory and persists.

**Q: Can I switch models during runtime?**  
A: Yes! Call `setGPSDynamicModel(model)` anytime.

**Q: What if my GPS is different (NEO-7, NEO-8, etc.)?**  
A: This code works with most u-blox GPS modules (NEO-6/7/8/M8).

**Q: Will this work with other GPS brands?**  
A: No. This is u-blox specific (UBX protocol). Other brands use different protocols.

**Q: How do I know it's working?**  
A: Check Serial Monitor for ACK message, or use u-center software to verify.

**Q: Does this affect GPS accuracy?**  
A: It optimizes the Kalman filter for vehicle motion, generally improving accuracy for automotive use.

**Q: What if I get no ACK?**  
A: Configuration may still be applied. Verify with `queryGPSDynamicModel()` or u-center.

## Summary

Your GPS is now configured for **Automotive mode**, optimized for:
- 🚗 Vehicle tracking
- 🏍️ Motorcycle GPS
- 🚐 Fleet management
- 🚕 Taxi/rideshare tracking
- 🚚 Delivery tracking

The configuration:
- ✅ Automatically applied on boot
- ✅ Saved to GPS flash memory
- ✅ Persists after power cycle
- ✅ Can be changed anytime

**Enjoy better GPS performance for your vehicle tracking application! 🚀**
