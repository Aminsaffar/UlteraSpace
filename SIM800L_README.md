# SIM800L Integration Guide for ESP32 GPS Tracker

## Overview
This sketch now includes SIM800L GSM/GPRS module support to send GPS data to a remote webserver via cellular network.

## Hardware Connections

### ESP32 to SIM800L Wiring

| ESP32 Pin | SIM800L Pin | Description |
|-----------|-------------|-------------|
| GPIO 25   | TXD         | ESP32 RX (receives from SIM800L) |
| GPIO 26   | RXD         | ESP32 TX (sends to SIM800L) |
| GND       | GND         | Common ground |
| 5V        | VCC         | Power (use external 5V/2A power supply recommended) |

### ESP32 to NEO-6M GPS Wiring (unchanged)

| ESP32 Pin | GPS Pin | Description |
|-----------|---------|-------------|
| GPIO 16   | TXD     | GPS transmit to ESP32 receive |
| GPIO 17   | RXD     | GPS receive from ESP32 transmit |
| 3.3V      | VCC     | Power |
| GND       | GND     | Ground |

### Important Notes on Power

**⚠️ CRITICAL: SIM800L Power Requirements**
- The SIM800L requires **4.0V with current peaks up to 2A** during transmission
- **DO NOT** power SIM800L directly from ESP32's 5V pin - it cannot supply enough current
- Use a **dedicated 5V/2A power supply** or a **LiPo battery with voltage regulator**
- Connect ESP32 GND and SIM800L GND together (common ground)
- Poor power supply will cause:
  - Module resets
  - Failed AT commands
  - GPRS connection failures
  - "UNDER-VOLTAGE WARNING" errors

### Recommended Power Setup
```
Option 1: External Power Supply
- Use 5V/2A wall adapter
- Connect to SIM800L VCC
- Share common ground with ESP32

Option 2: LiPo Battery
- Use 3.7V LiPo battery (2000mAh+)
- Connect through appropriate voltage regulator
- Provides stable power during transmission peaks
```

## Software Configuration

### 1. Update SIM800L Settings
In the sketch, modify these lines according to your mobile carrier:

```cpp
// SIM800L Settings
const char* apn = "internet";  // Change to your carrier's APN
const char* gprsUser = "";     // GPRS username (usually empty)
const char* gprsPass = "";     // GPRS password (usually empty)
```

### Common APN Settings by Carrier

| Carrier (US) | APN |
|--------------|-----|
| T-Mobile | fast.t-mobile.com |
| AT&T | phone |
| Verizon | vzwinternet |

| Carrier (International) | APN |
|-------------------------|-----|
| Vodafone | internet |
| Orange | orange.world |
| MTN | internet |
| Airtel | airtelgprs.com |
| Hologram (IoT) | hologram |

### 2. Configure Server Endpoint

Update the server URL where GPS data will be sent:

```cpp
const char* serverUrl = "http://yourserver.com/api/gps";
const int serverPort = 80;
```

### 3. Adjust Sending Intervals

```cpp
const unsigned long DATA_SEND_INTERVAL_MS = 30000;  // Send data every 30 seconds
```

## Data Format

GPS data is sent as JSON via HTTP POST:

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

## Server-Side Setup

Your webserver needs an endpoint to receive GPS data. Here's a simple example:

### PHP Example
```php
<?php
header('Content-Type: application/json');

// Read JSON data
$json = file_get_contents('php://input');
$data = json_decode($json, true);

// Validate data
if (isset($data['latitude']) && isset($data['longitude'])) {
    // Store in database or file
    $logFile = 'gps_data.log';
    file_put_contents($logFile, date('Y-m-d H:i:s') . ' - ' . $json . "\n", FILE_APPEND);
    
    // Return success
    echo json_encode(['status' => 'success', 'message' => 'Data received']);
} else {
    http_response_code(400);
    echo json_encode(['status' => 'error', 'message' => 'Invalid data']);
}
?>
```

### Node.js/Express Example
```javascript
const express = require('express');
const app = express();
app.use(express.json());

app.post('/api/gps', (req, res) => {
    const gpsData = req.body;
    console.log('GPS Data received:', gpsData);
    
    // Store in database
    // ... your database logic here
    
    res.json({ status: 'success', message: 'Data received' });
});

app.listen(80, () => {
    console.log('Server running on port 80');
});
```

## Features

### Automatic GPRS Management
- Automatically connects to GPRS network on startup
- Monitors connection status every 60 seconds
- Reconnects automatically if connection is lost

### Smart Data Sending
- Sends GPS data every 30 seconds (configurable)
- Only sends when GPS has valid fix
- Only sends when GPRS is connected
- Displays send status on web interface

### Status Monitoring
The web interface shows:
- SIM800L module status (Network OK, SIM Error, etc.)
- GPRS connection status (Connected, Disconnected, etc.)
- Last successful data transmission time
- Last transmission status (Success, Failed, etc.)

## Troubleshooting

### SIM800L Not Responding
1. **Check power supply** - Most common issue!
2. Verify wiring connections
3. Ensure SIM card is inserted correctly
4. Check if SIM card has active data plan
5. Try different baud rate (try 115200 if 9600 doesn't work)

### Network Registration Failed
1. Check SIM card has network coverage
2. Verify PIN is disabled on SIM card
3. Wait longer (can take 30-60 seconds)
4. Check antenna is connected

### GPRS Connection Failed
1. Verify APN settings for your carrier
2. Check SIM card has data plan activated
3. Ensure sufficient power supply
4. Try manually: Send `AT+SAPBR=2,1` to check status

### HTTP POST Failed
1. Verify server URL is correct (HTTP, not HTTPS)
2. SIM800L doesn't support HTTPS easily - use HTTP
3. Check server is accessible from internet
4. Verify firewall allows incoming connections
5. Test server endpoint with curl or Postman first

### Serial Monitor Commands
You can send AT commands directly through Serial Monitor:
```
AT                  // Test communication
AT+CSQ              // Check signal quality (should be 10-31)
AT+CREG?            // Check network registration
AT+SAPBR=2,1        // Query GPRS connection status
AT+CIPSTATUS        // Check connection status
```

## Pin Conflicts

**Note**: This sketch uses:
- Hardware Serial 0 (USB) for debugging
- Hardware Serial 1 (GPIO25, GPIO26) for SIM800L
- Hardware Serial 2 (GPIO16, GPIO17) for GPS

Ensure these pins are not used for other purposes in your project.

## Memory Considerations

The SIM800L requires additional RAM for:
- AT command buffers
- HTTP response storage
- JSON payload construction

If you experience memory issues:
1. Reduce `DATA_SEND_INTERVAL_MS` to send less frequently
2. Reduce JSON payload size
3. Increase delay between operations

## Performance Optimization

### Reduce Data Usage
```cpp
// Send only essential data
String jsonData = "{";
jsonData += "\"lat\":" + String(gps.location.lat(), 6) + ",";
jsonData += "\"lng\":" + String(gps.location.lng(), 6) + ",";
jsonData += "\"ts\":\"" + lastTime + "\"";
jsonData += "}";
```

### Batch Multiple Readings
Store GPS readings in memory and send multiple points in one HTTP request to reduce data costs.

## Cost Estimation

Typical data usage:
- Each JSON payload: ~200 bytes
- Sending every 30 seconds: ~576 KB/day
- Monthly data (30 days): ~17 MB/month

Choose an appropriate IoT data plan based on your sending frequency.

## License

This code is provided as-is for educational and hobbyist purposes.

## Support

For issues specific to:
- **SIM800L AT commands**: Refer to SIMCom official AT command manual
- **ESP32**: Check ESP32 Arduino core documentation
- **This sketch**: Check serial monitor for debug messages
