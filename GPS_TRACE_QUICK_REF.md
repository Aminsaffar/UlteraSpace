# GPS Trace Map - Quick Reference

## Quick Access
- **URL**: `http://[ESP32_IP]/trace`
- **Button**: Available on both home pages (Direct Mode & Connected Dashboard)

## Color Legend
| Color | Speed Range | Typical Use Case |
|-------|-------------|------------------|
| 🟢 Green | < 20 km/h | City traffic, parking, stopped |
| 🟡 Yellow | 20-60 km/h | Residential areas, city driving |
| 🔴 Red | > 60 km/h | Highway, open roads |

## Map Controls
- **Zoom**: Mouse wheel, +/- buttons, or pinch on mobile
- **Pan**: Click and drag, or swipe on mobile
- **Popup**: Click any route segment for details
- **Refresh**: Button in header to reload latest data
- **Back**: Return to home page

## What You'll See

### Info Panel (Top Right)
```
Route Info
Points: 234
Distance: 45.67 km
Max Speed: 87.3 km/h
[Color Legend]
```

### Segment Popup (Click on route)
```
Segment 123
Speed: 52.3 km/h
Time: 2025/10/14 19:45:30
Altitude: 1205.5 m
Satellites: 9
```

## Common Issues

| Problem | Solution |
|---------|----------|
| Map doesn't load | Check internet connection (needed for tiles) |
| No route displayed | Ensure GPS has logged valid coordinates |
| Wrong route location | GPS accuracy issue - check satellite count |
| Can't click segments | Route too dense - try zooming in |

## Integration Code Added

### Server Routes (in `setup()`)
```cpp
server.on("/trace", HTTP_GET, handleTrace);
server.serveStatic("/gpslog.csv", LittleFS, "/gpslog.csv");
```

### Home Page Buttons
**Direct Mode (AP):**
```html
<div class="actions">
  <a class="btn" href="/download">Download Latest CSV</a>
  <a class="btn" href="/trace">GPS Trace</a>
</div>
```

**Connected Dashboard:**
```html
<section class="actions">
  <a class="btn" href="/download">Download CSV</a>
  <a class="btn" href="/trace">GPS Trace</a>
  <a class="btn secondary" href="/data.json">Open Real-time JSON</a>
</section>
```

## Technical Specs

| Feature | Details |
|---------|---------|
| Map Library | Leaflet 1.9.4 |
| Tile Provider | OpenStreetMap |
| Data Source | `/gpslog.csv` from LittleFS |
| Page Size | ~7KB (HTML/CSS/JS) |
| External Assets | ~140KB (Leaflet library) |
| Max Points | 1000+ (browser-dependent) |

## Distance Calculation
Uses **Haversine formula** for accurate Earth-surface distances:
```javascript
R = 6371 km (Earth radius)
Distance = 2R × arctan2(√a, √(1-a))
```

## Speed Color Logic
```javascript
function getSpeedColor(speed) {
  if (speed < 20) return '#22c55e';  // Green
  if (speed < 60) return '#eab308';  // Yellow
  return '#ef4444';                   // Red
}
```

## CSV Format Expected
```csv
UTC Time,Latitude,Longitude,Altitude(m),Speed(km/h),Satellites
2025/10/14 19:30:45,35.689487,51.388974,1200.5,45.2,8
2025/10/14 19:31:15,35.690123,51.389456,1205.3,52.8,9
```

## Browser Requirements
- Modern browser (Chrome, Firefox, Safari, Edge)
- JavaScript enabled
- Internet connection for map tiles
- SVG support (all modern browsers)

## Memory Usage
- **ESP32**: ~7KB PROGMEM, no RAM impact
- **Browser**: ~5-10MB (Leaflet + tiles)
- **Network**: ~200KB initial load

## Files Modified
1. `sketch_oct6a.ino` - Main sketch
   - Added `handleTrace()` function
   - Added `/trace` route
   - Added CSV static serving
   - Updated both home pages with buttons

## Testing Steps
1. Upload sketch to ESP32
2. Wait for GPS fix (check satellite count)
3. Drive/walk around for a few minutes
4. Connect to ESP32 (AP or WiFi)
5. Click "GPS Trace" button
6. Verify map loads and route displays

## Customization Quick Tips

### Change speed thresholds
Edit values in `getSpeedColor()`:
```javascript
if (speed < 30) return green;   // Was 20
if (speed < 80) return yellow;  // Was 60
```

### Change route appearance
Edit polyline options:
```javascript
weight: 4,      // Line thickness (1-10)
opacity: 0.8    // Transparency (0.0-1.0)
```

### Use different map
Replace tile URL in:
```javascript
L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', ...)
```

## Support
See full documentation in `GPS_TRACE_MAP.md`
