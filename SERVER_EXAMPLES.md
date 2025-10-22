# Server-Side Examples for Receiving GPS Data

This document provides server-side code examples for receiving GPS data from your ESP32 tracker.

## PHP Example (Simple)

Create a file named `gps_receiver.php` on your webserver:

```php
<?php
/**
 * GPS Data Receiver - PHP
 * Simple endpoint to receive and store GPS data from ESP32+SIM800L
 */

// Set response headers
header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: POST');

// Read incoming JSON data
$json_input = file_get_contents('php://input');
$data = json_decode($json_input, true);

// Log file path
$log_file = __DIR__ . '/gps_data.log';
$json_file = __DIR__ . '/latest_position.json';

// Validate required fields
if (!isset($data['latitude']) || !isset($data['longitude'])) {
    http_response_code(400);
    echo json_encode([
        'status' => 'error',
        'message' => 'Missing required fields: latitude, longitude'
    ]);
    exit;
}

// Extract GPS data
$latitude = floatval($data['latitude']);
$longitude = floatval($data['longitude']);
$altitude = isset($data['altitude']) ? floatval($data['altitude']) : 0;
$speed = isset($data['speed']) ? floatval($data['speed']) : 0;
$course = isset($data['course']) ? floatval($data['course']) : 0;
$satellites = isset($data['satellites']) ? intval($data['satellites']) : 0;
$hdop = isset($data['hdop']) ? floatval($data['hdop']) : 0;
$timestamp = isset($data['timestamp']) ? $data['timestamp'] : date('Y-m-d H:i:s');

// Create log entry
$log_entry = sprintf(
    "[%s] GPS Data - Lat: %.6f, Lng: %.6f, Alt: %.2fm, Speed: %.2fkm/h, Course: %.2f°, Sats: %d, HDOP: %.2f\n",
    date('Y-m-d H:i:s'),
    $latitude,
    $longitude,
    $altitude,
    $speed,
    $course,
    $satellites,
    $hdop
);

// Append to log file
if (file_put_contents($log_file, $log_entry, FILE_APPEND | LOCK_EX) !== false) {
    // Save latest position as JSON
    $position_data = [
        'latitude' => $latitude,
        'longitude' => $longitude,
        'altitude' => $altitude,
        'speed' => $speed,
        'course' => $course,
        'satellites' => $satellites,
        'hdop' => $hdop,
        'timestamp' => $timestamp,
        'received_at' => date('Y-m-d H:i:s')
    ];
    
    file_put_contents($json_file, json_encode($position_data, JSON_PRETTY_PRINT));
    
    // Success response
    echo json_encode([
        'status' => 'success',
        'message' => 'GPS data received and logged',
        'received_at' => date('Y-m-d H:i:s')
    ]);
} else {
    http_response_code(500);
    echo json_encode([
        'status' => 'error',
        'message' => 'Failed to write log file'
    ]);
}
?>
```

## PHP Example (Database Storage)

```php
<?php
/**
 * GPS Data Receiver - PHP with MySQL
 */

header('Content-Type: application/json');

// Database configuration
$db_host = 'localhost';
$db_name = 'gps_tracker';
$db_user = 'your_username';
$db_pass = 'your_password';

try {
    $pdo = new PDO(
        "mysql:host=$db_host;dbname=$db_name;charset=utf8mb4",
        $db_user,
        $db_pass,
        [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]
    );
} catch (PDOException $e) {
    http_response_code(500);
    echo json_encode(['status' => 'error', 'message' => 'Database connection failed']);
    exit;
}

// Read JSON input
$json_input = file_get_contents('php://input');
$data = json_decode($json_input, true);

// Validate
if (!isset($data['latitude']) || !isset($data['longitude'])) {
    http_response_code(400);
    echo json_encode(['status' => 'error', 'message' => 'Invalid data']);
    exit;
}

// Insert into database
$sql = "INSERT INTO gps_positions (latitude, longitude, altitude, speed, course, satellites, hdop, timestamp, created_at) 
        VALUES (:lat, :lng, :alt, :speed, :course, :sats, :hdop, :timestamp, NOW())";

try {
    $stmt = $pdo->prepare($sql);
    $stmt->execute([
        ':lat' => $data['latitude'],
        ':lng' => $data['longitude'],
        ':alt' => $data['altitude'] ?? 0,
        ':speed' => $data['speed'] ?? 0,
        ':course' => $data['course'] ?? 0,
        ':sats' => $data['satellites'] ?? 0,
        ':hdop' => $data['hdop'] ?? 0,
        ':timestamp' => $data['timestamp'] ?? date('Y-m-d H:i:s')
    ]);
    
    echo json_encode([
        'status' => 'success',
        'message' => 'Data saved',
        'id' => $pdo->lastInsertId()
    ]);
} catch (PDOException $e) {
    http_response_code(500);
    echo json_encode(['status' => 'error', 'message' => 'Database error']);
}
?>
```

### MySQL Table Schema

```sql
CREATE TABLE gps_positions (
    id INT AUTO_INCREMENT PRIMARY KEY,
    latitude DECIMAL(10, 6) NOT NULL,
    longitude DECIMAL(10, 6) NOT NULL,
    altitude DECIMAL(8, 2) DEFAULT 0,
    speed DECIMAL(6, 2) DEFAULT 0,
    course DECIMAL(5, 2) DEFAULT 0,
    satellites INT DEFAULT 0,
    hdop DECIMAL(4, 2) DEFAULT 0,
    timestamp VARCHAR(50),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_timestamp (timestamp),
    INDEX idx_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

## Node.js/Express Example

```javascript
/**
 * GPS Data Receiver - Node.js/Express
 */

const express = require('express');
const fs = require('fs');
const path = require('path');

const app = express();
app.use(express.json());

// Log file path
const LOG_FILE = path.join(__dirname, 'gps_data.log');
const JSON_FILE = path.join(__dirname, 'latest_position.json');

// GPS data endpoint
app.post('/api/gps', (req, res) => {
    const data = req.body;
    
    // Validate required fields
    if (!data.latitude || !data.longitude) {
        return res.status(400).json({
            status: 'error',
            message: 'Missing required fields: latitude, longitude'
        });
    }
    
    // Extract GPS data
    const gpsData = {
        latitude: parseFloat(data.latitude),
        longitude: parseFloat(data.longitude),
        altitude: parseFloat(data.altitude) || 0,
        speed: parseFloat(data.speed) || 0,
        course: parseFloat(data.course) || 0,
        satellites: parseInt(data.satellites) || 0,
        hdop: parseFloat(data.hdop) || 0,
        timestamp: data.timestamp || new Date().toISOString(),
        receivedAt: new Date().toISOString()
    };
    
    // Create log entry
    const logEntry = `[${new Date().toISOString()}] GPS Data - ` +
        `Lat: ${gpsData.latitude.toFixed(6)}, ` +
        `Lng: ${gpsData.longitude.toFixed(6)}, ` +
        `Alt: ${gpsData.altitude.toFixed(2)}m, ` +
        `Speed: ${gpsData.speed.toFixed(2)}km/h, ` +
        `Sats: ${gpsData.satellites}\n`;
    
    // Append to log file
    fs.appendFile(LOG_FILE, logEntry, (err) => {
        if (err) {
            console.error('Error writing log:', err);
            return res.status(500).json({
                status: 'error',
                message: 'Failed to write log'
            });
        }
        
        // Save latest position
        fs.writeFile(JSON_FILE, JSON.stringify(gpsData, null, 2), (err) => {
            if (err) console.error('Error saving JSON:', err);
        });
        
        console.log('GPS data received:', gpsData);
        res.json({
            status: 'success',
            message: 'GPS data received and logged',
            receivedAt: gpsData.receivedAt
        });
    });
});

// Get latest position
app.get('/api/gps/latest', (req, res) => {
    fs.readFile(JSON_FILE, 'utf8', (err, data) => {
        if (err) {
            return res.status(404).json({
                status: 'error',
                message: 'No position data available'
            });
        }
        res.json(JSON.parse(data));
    });
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
    console.log(`GPS receiver server running on port ${PORT}`);
});
```

### MongoDB Version

```javascript
/**
 * GPS Data Receiver - Node.js/Express with MongoDB
 */

const express = require('express');
const mongoose = require('mongoose');

const app = express();
app.use(express.json());

// MongoDB connection
mongoose.connect('mongodb://localhost:27017/gps_tracker', {
    useNewUrlParser: true,
    useUnifiedTopology: true
});

// GPS Position Schema
const positionSchema = new mongoose.Schema({
    latitude: { type: Number, required: true },
    longitude: { type: Number, required: true },
    altitude: { type: Number, default: 0 },
    speed: { type: Number, default: 0 },
    course: { type: Number, default: 0 },
    satellites: { type: Number, default: 0 },
    hdop: { type: Number, default: 0 },
    timestamp: { type: String },
    receivedAt: { type: Date, default: Date.now }
});

const Position = mongoose.model('Position', positionSchema);

// Receive GPS data
app.post('/api/gps', async (req, res) => {
    try {
        const position = new Position({
            latitude: req.body.latitude,
            longitude: req.body.longitude,
            altitude: req.body.altitude,
            speed: req.body.speed,
            course: req.body.course,
            satellites: req.body.satellites,
            hdop: req.body.hdop,
            timestamp: req.body.timestamp
        });
        
        await position.save();
        
        console.log('GPS data saved:', position);
        res.json({
            status: 'success',
            message: 'GPS data saved',
            id: position._id
        });
    } catch (error) {
        console.error('Error saving GPS data:', error);
        res.status(500).json({
            status: 'error',
            message: 'Failed to save GPS data'
        });
    }
});

// Get latest position
app.get('/api/gps/latest', async (req, res) => {
    try {
        const latest = await Position.findOne().sort({ receivedAt: -1 });
        res.json(latest || { status: 'error', message: 'No data available' });
    } catch (error) {
        res.status(500).json({ status: 'error', message: 'Database error' });
    }
});

app.listen(3000, () => console.log('Server running on port 3000'));
```

## Python/Flask Example

```python
"""
GPS Data Receiver - Python/Flask
"""

from flask import Flask, request, jsonify
from datetime import datetime
import json
import os

app = Flask(__name__)

LOG_FILE = 'gps_data.log'
JSON_FILE = 'latest_position.json'

@app.route('/api/gps', methods=['POST'])
def receive_gps():
    data = request.get_json()
    
    # Validate required fields
    if not data or 'latitude' not in data or 'longitude' not in data:
        return jsonify({
            'status': 'error',
            'message': 'Missing required fields: latitude, longitude'
        }), 400
    
    # Extract GPS data
    gps_data = {
        'latitude': float(data['latitude']),
        'longitude': float(data['longitude']),
        'altitude': float(data.get('altitude', 0)),
        'speed': float(data.get('speed', 0)),
        'course': float(data.get('course', 0)),
        'satellites': int(data.get('satellites', 0)),
        'hdop': float(data.get('hdop', 0)),
        'timestamp': data.get('timestamp', datetime.now().isoformat()),
        'received_at': datetime.now().isoformat()
    }
    
    # Create log entry
    log_entry = f"[{datetime.now().isoformat()}] GPS Data - " \
                f"Lat: {gps_data['latitude']:.6f}, " \
                f"Lng: {gps_data['longitude']:.6f}, " \
                f"Alt: {gps_data['altitude']:.2f}m, " \
                f"Speed: {gps_data['speed']:.2f}km/h, " \
                f"Sats: {gps_data['satellites']}\n"
    
    # Append to log file
    try:
        with open(LOG_FILE, 'a') as f:
            f.write(log_entry)
        
        # Save latest position
        with open(JSON_FILE, 'w') as f:
            json.dump(gps_data, f, indent=2)
        
        print(f"GPS data received: {gps_data}")
        return jsonify({
            'status': 'success',
            'message': 'GPS data received and logged',
            'received_at': gps_data['received_at']
        })
    except Exception as e:
        print(f"Error: {e}")
        return jsonify({
            'status': 'error',
            'message': 'Failed to write log'
        }), 500

@app.route('/api/gps/latest', methods=['GET'])
def get_latest():
    try:
        with open(JSON_FILE, 'r') as f:
            data = json.load(f)
        return jsonify(data)
    except FileNotFoundError:
        return jsonify({
            'status': 'error',
            'message': 'No position data available'
        }), 404

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
```

## Testing Your Server

### Using curl

```bash
# Test POST endpoint
curl -X POST http://yourserver.com/api/gps \
  -H "Content-Type: application/json" \
  -d '{
    "latitude": 37.774929,
    "longitude": -122.419416,
    "altitude": 45.20,
    "speed": 12.50,
    "course": 180.00,
    "satellites": 8,
    "hdop": 1.20,
    "timestamp": "2023/10/08 14:30:25"
  }'

# Get latest position
curl http://yourserver.com/api/gps/latest
```

### Using Postman

1. Create new POST request
2. URL: `http://yourserver.com/api/gps`
3. Headers: `Content-Type: application/json`
4. Body (raw JSON):
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

## Deployment Tips

### Nginx Configuration

```nginx
server {
    listen 80;
    server_name yourserver.com;
    
    location /api/gps {
        # For PHP
        fastcgi_pass unix:/var/run/php/php7.4-fpm.sock;
        include fastcgi_params;
        fastcgi_param SCRIPT_FILENAME /var/www/html/gps_receiver.php;
        
        # For Node.js (using PM2)
        # proxy_pass http://localhost:3000;
        # proxy_http_version 1.1;
        # proxy_set_header Host $host;
    }
}
```

### Security Considerations

1. **Authentication**: Add API key validation
2. **Rate Limiting**: Prevent abuse
3. **HTTPS**: Use SSL/TLS (but SIM800L doesn't support HTTPS easily)
4. **Input Validation**: Sanitize all inputs
5. **Database Security**: Use prepared statements

### Example with API Key

```php
// Add to top of PHP script
$api_key = $_SERVER['HTTP_X_API_KEY'] ?? '';
$valid_key = 'your-secret-api-key-here';

if ($api_key !== $valid_key) {
    http_response_code(401);
    echo json_encode(['status' => 'error', 'message' => 'Unauthorized']);
    exit;
}
```

Then update ESP32 code:
```cpp
sendATCommand("AT+HTTPPARA=\"USERDATA\",\"X-API-Key: your-secret-api-key-here\"", 2000);
```

## Monitoring and Logging

### Log Rotation

```bash
# Linux logrotate configuration
# /etc/logrotate.d/gps-tracker

/var/www/html/gps_data.log {
    daily
    rotate 30
    compress
    delaycompress
    missingok
    notifempty
    create 0644 www-data www-data
}
```

### Real-time Monitoring

```bash
# Watch incoming GPS data
tail -f gps_data.log

# Count today's entries
grep "$(date +%Y-%m-%d)" gps_data.log | wc -l
```

## Next Steps

1. Choose your preferred server technology
2. Deploy server code to your web hosting
3. Test endpoint with curl or Postman
4. Update ESP32 `serverUrl` with your endpoint
5. Monitor server logs for incoming data
6. Build frontend to visualize GPS tracks

Happy tracking! 🗺️
