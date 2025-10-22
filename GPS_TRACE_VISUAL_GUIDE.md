# GPS Trace Map - Visual Reference

## Page Layout

```
┌─────────────────────────────────────────────────────────────┐
│  🗺️ GPS Trace Map          [🔄 Refresh] [← Back Home]      │ ← Header
├─────────────────────────────────────────────────────────────┤
│                                                              │
│                        ┌──────────────┐                     │
│                        │  Route Info  │                     │
│    🗺 MAP AREA         │  Points: 234 │  ← Info Panel      │
│                        │  Distance:   │                     │
│    • Zoomable          │  45.67 km    │                     │
│    • Pannable          │  Max Speed:  │                     │
│    • Interactive       │  87.3 km/h   │                     │
│                        │              │                     │
│    Route with          │  Legend:     │                     │
│    colored segments    │  🟢 < 20     │                     │
│    based on speed      │  🟡 20-60    │                     │
│                        │  🔴 > 60     │                     │
│                        └──────────────┘                     │
│                                                              │
│    📍 START                                                 │
│         🟢━━🟡━━━🔴━━━🔴━━🟡━━━🟢                         │
│                                   📍 END                    │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

## Visual Examples

### Route Color Coding
```
Example Route Visualization:

Start (Home)
   ↓
🟢━━━━━  (Residential - 15 km/h)
   ↓
🟢━━━━━  (Stopped at light - 0 km/h)
   ↓
🟡━━━━━  (City street - 35 km/h)
   ↓
🟡━━━━━  (Main road - 50 km/h)
   ↓
🔴━━━━━  (Highway entrance - 75 km/h)
   ↓
🔴━━━━━  (Highway - 95 km/h)
   ↓
🔴━━━━━  (Highway - 105 km/h)
   ↓
🟡━━━━━  (Exit ramp - 45 km/h)
   ↓
🟢━━━━━  (Destination - 10 km/h)
   ↓
End (Destination)
```

### Popup Window (Click on Segment)
```
╔════════════════════╗
║  Segment 15        ║
║                    ║
║  Speed: 52.3 km/h  ║
║  Time: 19:45:30    ║
║  Altitude: 1205 m  ║
║  Satellites: 9     ║
╚════════════════════╝
```

### Info Panel (Top Right Corner)
```
┌──────────────────────┐
│   Route Info         │
│                      │
│   Points: 234        │
│   Distance: 45.67 km │
│   Max Speed: 87 km/h │
│   ──────────────     │
│   🟢  < 20 km/h      │
│   🟡  20-60 km/h     │
│   🔴  > 60 km/h      │
└──────────────────────┘
```

## Color Scheme

### Speed Colors
| Speed Range | Color Code | Visual | Use Case |
|-------------|-----------|--------|----------|
| 0-20 km/h | `#22c55e` | 🟢 Green | Parking, traffic, residential |
| 20-60 km/h | `#eab308` | 🟡 Yellow | City roads, urban driving |
| 60+ km/h | `#ef4444` | 🔴 Red | Highways, fast roads |

### UI Colors
| Element | Color | Hex Code | Usage |
|---------|-------|----------|-------|
| Background | Dark blue | `#0b0d17` | Page background |
| Header | Dark slate | `rgba(15,23,42,0.95)` | Top bar |
| Info panel | Dark slate | `rgba(15,23,42,0.95)` | Statistics box |
| Primary button | Blue | `#5c6cff` | Action buttons |
| Secondary button | Gray | `rgba(148,163,184,0.22)` | Back button |
| Text | Light | `#f5f7ff` | Main text |
| Subtitle | Blue-gray | `#94a3b8` | Labels |

## Example Screenshots Description

### Loading State
```
┌─────────────────────────────────────┐
│                                      │
│         ┌──────────────────┐        │
│         │                   │        │
│         │  Loading GPS      │        │
│         │  data...          │        │
│         │                   │        │
│         └──────────────────┘        │
│                                      │
└─────────────────────────────────────┘
```

### Empty State (No Data)
```
┌─────────────────────────────────────┐
│                                      │
│         ┌──────────────────┐        │
│         │                   │        │
│         │  No GPS data      │        │
│         │  available yet.   │        │
│         │                   │        │
│         │  [← Back Home]    │        │
│         │                   │        │
│         └──────────────────┘        │
│                                      │
└─────────────────────────────────────┘
```

### Loaded Route
```
┌──────────────────────────────────────────────┐
│ 🗺️ GPS Trace    [Refresh] [Back]            │
├──────────────────────────────────────────────┤
│                           ┌──────────────┐   │
│   ╔═══════════╗          │ Route Info   │   │
│   ║ OSM Map   ║          │ Points: 156  │   │
│   ║           ║          │ Dist: 23.4km │   │
│   ║  📍       ║          │ Max: 72 km/h │   │
│   ║   ╲       ║          └──────────────┘   │
│   ║    ╲      ║                              │
│   ║     ╲     ║                              │
│   ║      ╲    ║     Colored route segments   │
│   ║       ╲   ║     show speed variation     │
│   ║        ╲  ║                              │
│   ║         📍 ║                              │
│   ╚═══════════╝                              │
│                                               │
└──────────────────────────────────────────────┘
```

## Interactive Elements

### Clickable Areas
```
Map Controls:
┌────────┐
│   +    │  ← Zoom in
├────────┤
│   -    │  ← Zoom out
└────────┘

Route Segments:
━━━━━  ← Click for details

Markers:
📍  ← Click for timestamp
```

### Hover Effects
```css
Buttons:
  Normal:   [Refresh]
  Hover:    [Refresh] (slightly elevated, brighter)

Route Segments:
  Normal:   ━━━━━ (opacity 0.8)
  Hover:    ━━━━━ (thicker, highlighted)
```

## Mobile View

### Portrait Mode
```
┌──────────────┐
│ GPS Trace    │
│ [≡]  [↶]     │ ← Compact header
├──────────────┤
│              │
│   🗺 MAP     │
│              │
│   Full       │
│   Height     │
│              │
│              │
│              │
├──────────────┤
│ Info Panel   │ ← Slides up on tap
└──────────────┘
```

### Landscape Mode
```
┌───────────────────────────────┐
│ GPS Trace    [Refresh] [Back] │
├──────────────┬────────────────┤
│              │  Route Info    │
│   🗺 MAP     │  Points: 234   │
│   Area       │  Dist: 45.6km  │
│              │  Max: 87 km/h  │
│              │  Legend:       │
│              │  🟢 🟡 🔴      │
└──────────────┴────────────────┘
```

## Example Scenarios

### Scenario 1: City Commute
```
Route Pattern:
🟢 Home (parking)
🟢 Residential street (slow)
🟡 Main road (moderate)
🟢 Traffic light (stopped)
🟡 City center (moderate)
🟢 Office (parking)

Color Distribution:
🟢 40% - Green segments
🟡 50% - Yellow segments  
🔴 10% - Red segments

Max Speed: 58 km/h
Distance: 8.5 km
```

### Scenario 2: Highway Trip
```
Route Pattern:
🟢 Home (parking)
🟡 City exit
🔴 Highway (fast)
🔴 Highway (cruise)
🔴 Highway (fast)
🟡 Exit ramp
🟢 Destination

Color Distribution:
🟢 15% - Green segments
🟡 20% - Yellow segments
🔴 65% - Red segments

Max Speed: 115 km/h
Distance: 75.2 km
```

### Scenario 3: Urban Delivery
```
Route Pattern:
Multiple stops with varied speeds

🟢 Warehouse
🟡 To stop 1
🟢 Stop 1 (parking)
🟡 To stop 2
🟢 Stop 2 (parking)
🟡 To stop 3
... (repeat)

Color Distribution:
🟢 60% - Green (many stops)
🟡 35% - Yellow (city roads)
🔴  5% - Red (occasional fast)

Max Speed: 65 km/h
Distance: 42.3 km
Points: 856 (high frequency)
```

## Map Tile Examples

### OpenStreetMap (Default)
```
Standard street map with:
- Roads in white/yellow
- Parks in green
- Water in blue
- Buildings outlined
- Street names labeled
```

### Satellite (Optional)
```
Aerial imagery showing:
- Real terrain
- Buildings from above
- Natural features
- Less cluttered view
```

### Terrain (Optional)
```
Topographic view with:
- Elevation contours
- Hill shading
- Natural features
- Outdoor recreation focus
```

## Responsive Breakpoints

### Desktop (> 768px)
```
- Full header with all buttons
- Info panel top-right
- Wide map view
- All text at full size
```

### Tablet (480-768px)
```
- Compact header
- Info panel movable
- Medium map view
- Readable text
```

### Mobile (< 480px)
```
- Minimal header
- Collapsible info panel
- Full-screen map
- Touch-optimized controls
```

## Performance Indicators

### Fast Load (< 3s)
```
✅ Good
- Smooth map interaction
- Instant zoom/pan
- Quick popup response
- No lag
```

### Moderate Load (3-8s)
```
⚠️ Acceptable
- Slight delay in rendering
- Smooth once loaded
- Usable performance
```

### Slow Load (> 8s)
```
❌ Consider optimization
- Long initial wait
- Possible lag on interaction
- May need route filtering
```

## User Journey

```
1. User clicks "GPS Trace" button
   ↓
2. Page loads with "Loading GPS data..."
   ↓
3. CSV fetched from ESP32
   ↓
4. Map initializes with tiles
   ↓
5. Route segments drawn (colored)
   ↓
6. Map auto-zooms to fit route
   ↓
7. Info panel shows statistics
   ↓
8. User can interact:
   - Zoom in/out
   - Pan around
   - Click segments
   - View popups
   ↓
9. User clicks "Refresh" to update
   OR
   User clicks "Back Home" to return
```

## Tips for Best Visual Results

### GPS Logging
- ✅ Log frequently (every 5-10 seconds)
- ✅ Ensure good satellite visibility
- ✅ Drive varied speeds for color variety
- ❌ Avoid logging indoors (poor accuracy)

### Route Display
- ✅ Zoom appropriate to route length
- ✅ Click segments for details
- ✅ Use legend to interpret colors
- ✅ Check statistics for overview

### Performance
- ✅ Clear old logs periodically
- ✅ Use on modern browser
- ✅ Ensure good internet connection
- ✅ Refresh page if sluggish

---

**This visual reference helps you understand what to expect when using the GPS Trace Map feature!**
