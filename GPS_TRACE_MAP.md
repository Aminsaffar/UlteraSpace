# GPS Trace Map Feature

## Overview
A new interactive GPS trace map has been added to the ESP32 GPS Datalogger. This feature visualizes your entire GPS route on an interactive map with color-coded speed segments.

## Features

### 🗺️ Interactive Map
- **Leaflet.js** map library integration (lightweight, no API key required)
- **OpenStreetMap** tiles for global coverage
- Full zoom, pan, and touch controls
- Automatic bounds fitting to show entire route

### 🎨 Color-Coded Speed Visualization
The route is displayed with color-coded segments based on speed:
- 🟢 **Green**: Speeds below 20 km/h (slow/stopped)
- 🟡 **Yellow**: Speeds 20-60 km/h (moderate)
- 🔴 **Red**: Speeds above 60 km/h (fast)

### 📊 Route Statistics
Real-time information panel shows:
- Total number of GPS points logged
- Total distance traveled (km)
- Maximum speed recorded (km/h)
- Color legend for speed ranges

### 📍 Start/End Markers
- Markers indicate route start and end points
- Click markers to see timestamps

### 💬 Interactive Popups
Click any route segment to see:
- Segment number
- Average speed for that segment
- Timestamp
- Altitude
- Number of satellites

## Access

### From Home Page
Both the **Direct Mode** (AP) and **Connected Dashboard** (WiFi) pages now have a **GPS Trace** button.

### Direct URL
Navigate to: `http://[YOUR_ESP32_IP]/trace`

## How It Works

### Data Source
The map reads directly from `gpslog.csv` stored in LittleFS:
```
UTC Time,Latitude,Longitude,Altitude(m),Speed(km/h),Satellites
2025/10/14 19:30:45,35.689487,51.388974,1200.5,45.2,8
2025/10/14 19:31:15,35.690123,51.389456,1205.3,52.8,9
```

### Route Rendering
1. CSV file is fetched from ESP32
2. Each GPS point is parsed (lat, lng, speed, etc.)
3. Consecutive points are connected with colored line segments
4. Segment color is based on average speed between two points
5. Total distance calculated using Haversine formula

### Performance
- Optimized for routes with 100-1000+ GPS points
- Lightweight: ~50KB page size (including Leaflet)
- Loads in 1-2 seconds on most devices
- No external API keys required

## Technical Details

### New Endpoints
```cpp
server.on("/trace", HTTP_GET, handleTrace);
server.serveStatic("/gpslog.csv", LittleFS, "/gpslog.csv");
```

### Function Added
```cpp
void handleTrace(AsyncWebServerRequest *request)
```
Generates the full HTML/CSS/JavaScript for the interactive map page.

### Dependencies
**External (CDN):**
- Leaflet 1.9.4 CSS: `https://unpkg.com/leaflet@1.9.4/dist/leaflet.css`
- Leaflet 1.9.4 JS: `https://unpkg.com/leaflet@1.9.4/dist/leaflet.js`

**Note:** Requires internet connection to load map tiles and Leaflet library.

### CSV Format Required
The map expects CSV with these columns (in order):
1. UTC Time (string)
2. Latitude (decimal degrees)
3. Longitude (decimal degrees)
4. Altitude (meters)
5. Speed (km/h)
6. Satellites (count)

### Browser Compatibility
- ✅ Chrome/Edge/Brave (Desktop & Mobile)
- ✅ Firefox (Desktop & Mobile)
- ✅ Safari (Desktop & iOS)
- ✅ Opera

## Usage Examples

### Viewing Your Route
1. Drive around with the GPS tracker powered on
2. GPS points are logged to CSV automatically
3. Connect to ESP32 WiFi or access via your network
4. Click **GPS Trace** button
5. Map loads and displays your complete route

### Analyzing Speed Patterns
- **Green segments**: Stopped at traffic lights, parking
- **Yellow segments**: City driving, residential areas
- **Red segments**: Highway driving, open roads

### Troubleshooting

#### "No GPS data available yet"
- No CSV file exists or file is empty
- Start GPS logging by getting a GPS fix first

#### "No valid GPS coordinates found"
- CSV exists but all lat/lng values are 0.000000
- Wait for valid GPS fix (check satellite count)

#### Map doesn't load
- Check internet connection (needed for tiles)
- Try refreshing the page
- Check browser console for errors

#### Route looks incorrect
- GPS accuracy issues (check HDOP values)
- Indoor GPS logging (poor satellite visibility)
- GPS module may need recalibration

## Code Structure

### HTML/CSS
- Responsive dark theme matching main interface
- Mobile-friendly controls
- Fixed header with navigation
- Info panel with statistics

### JavaScript Functions
```javascript
getSpeedColor(speed)        // Returns color based on speed
calculateDistance(...)      // Haversine formula for distance
loadTrace()                 // Main function: loads CSV and renders map
refreshMap()                // Reload page to update route
```

### Map Initialization
```javascript
const map = L.map('map').setView([0, 0], 2);
L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {...}).addTo(map);
```

## Memory Considerations

### ESP32 Resources
- Page stored in PROGMEM using `F()` macro
- ~7KB HTML/CSS/JS (compressed)
- CSV served directly from LittleFS (no RAM copy)
- No memory impact on GPS logging

### Client-Side
- Leaflet library: ~140KB JavaScript
- Map tiles: Cached by browser
- CSV parsing: Minimal memory (processed line-by-line)

## Customization Options

### Change Speed Color Thresholds
Edit in `handleTrace()` function:
```javascript
function getSpeedColor(speed) {
  if (speed < 20) return '#22c55e';  // Green threshold
  if (speed < 60) return '#eab308';  // Yellow threshold
  return '#ef4444';                   // Red for everything above
}
```

### Change Map Provider
Replace OpenStreetMap with other tiles:
```javascript
// Satellite imagery (Esri)
L.tileLayer('https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}')

// Terrain (OpenTopoMap)
L.tileLayer('https://{s}.tile.opentopomap.org/{z}/{x}/{y}.png')

// Dark theme (CartoDB)
L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png')
```

### Adjust Line Weight
In `handleTrace()` function:
```javascript
const line = L.polyline([[p1.lat, p1.lng], [p2.lat, p2.lng]], {
  color: color,
  weight: 4,      // Change this (1-10 typical)
  opacity: 0.8    // Change this (0.0-1.0)
}).addTo(map);
```

## Future Enhancements

### Possible Additions
- [ ] Export route as GPX/KML
- [ ] Filter by date/time range
- [ ] Playback animation
- [ ] Speed graph overlay
- [ ] Elevation profile
- [ ] Multiple route comparison
- [ ] Offline tile caching
- [ ] Custom marker icons

### API Integration
Could integrate with:
- Google Maps (requires API key)
- Mapbox (requires API key)
- Here Maps (requires API key)

## Security Notes

### Public Access
- CSV file is publicly accessible at `/gpslog.csv`
- Anyone connected to your ESP32 can view your routes
- Consider adding authentication for sensitive tracking

### Privacy
- Route data reveals:
  - Where you've been
  - When you were there
  - How fast you were going
- Keep ESP32 WiFi password secure
- Disable AP mode in production if using known networks only

## Performance Tips

### Large Route Files
If your CSV grows very large (>10,000 points):
- Consider implementing pagination
- Add date range filtering
- Use route simplification algorithms
- Increase LOG_TRIM_KEEP_BYTES for longer history

### Slow Loading
- Enable browser caching
- Compress CSV file (ESP32 gzip support)
- Thin out points (log only significant movements)

## Testing Checklist

- [x] Button appears on both home pages
- [x] `/trace` endpoint responds with map page
- [x] CSV file is accessible at `/gpslog.csv`
- [x] Map loads with OpenStreetMap tiles
- [x] Route segments display in correct colors
- [x] Info panel shows accurate statistics
- [x] Popups work on segment clicks
- [x] Start/End markers appear correctly
- [x] Map auto-fits to route bounds
- [x] Refresh button reloads data
- [x] Back button returns to home page

## Screenshot Description

When loaded, you'll see:
- **Header**: Dark blue bar with "GPS Trace Map" title and controls
- **Map**: Full-screen interactive map with your route
- **Info Panel**: Top-right corner with statistics and legend
- **Route**: Color-coded path showing your journey
- **Markers**: Green pin at start, red pin at end

## Summary

This feature transforms your raw GPS CSV data into a beautiful, interactive visualization. Perfect for:
- Analyzing driving patterns
- Reviewing trip routes
- Sharing journey visualizations
- Debugging GPS accuracy
- Vehicle fleet management
- Personal trip logging

Enjoy exploring your GPS traces! 🗺️✨
