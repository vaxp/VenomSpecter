# Venom Miasma

A lightweight X11 compositor with **Dual Kawase blur**, **window shadows**, **rounded corners**, **animations**, and **D-Bus hot reload**.


## Features

- 🌫️ **Dual Kawase Blur** - High-quality, GPU-accelerated blur effect
- 🌑 **Window Shadows** - Customizable drop shadows with Gaussian kernel
- 📐 **Rounded Corners** - SDF-based anti-aliased corner radius
- 🎬 **Animations** - Smooth open/close and geometry transitions
- ⚡ **GLX Backend** - Hardware-accelerated rendering
- 🎛️ **D-Bus Hot Reload** - Change settings without restart

## Requirements

- X11 Window System
- OpenGL 3.3+ compatible GPU
- GLX extension support


## Audio Visualizer

- 🎵 **Audio Visualizer** - Real-time audio visualization with vibrant colors
- 🎵 **Music Effects** - Real-time audio visualization with vibrant colors

🎵 Venom Miasma: Audio Visualizer Effects
This document lists the real-time audio-reactive effects implemented in the Venom Compositor. These effects transform the desktop environment into a living, breathing visualization of your music.

1. Dynamic Beat Pulse 💓
Trigger: Bass Frequencies (Kick Drums) Description: Windows and their shadows "breathe" with the music. On every beat, the shadow opacity spikes instantly and decays slowly, while the shadow radius expands, giving a physical sense of impact to every drum kick.

2. Vibrant Color Flashes 🌈
Trigger: Peak Intensity (Beats) Description: Instead of a static color, every major beat triggers a flash of a random vibrant color. This ensures the visualizer never looks repetitive, cycling through a rich palette of neon hues (Red, Cyan, Purple, Gold, etc.) dynamically.

3. Rotating Neon Aura 🎨
Trigger: Constant / Background Description: Even during quiet moments, the desktop is alive. A base neon glow continuously rotates through the entire color spectrum (Hue Cycle) every 5 seconds. This rotating base blends with the reactive beat colors to create high-contrast, beautiful gradients.

4. Chromatic Aberration (Glitch) ⚡
Trigger: High Frequencies (Hi-Hats, Snares, Claps) Description: A digital "glitch" effect that simulates a broken lens. When high-pitched sounds hit, the Red and Blue color channels of the windows split apart horizontally. This creates a jittery, holographic look that syncs perfectly with fast percussion.

5. Bass Shockwaves 🌊
Trigger: Deep Bass Drop Description: A physics-based distortion shader. When a heavy bass note hits, a transparent ripple (shockwave) erupts from the center of the windows and expands outwards, distorting the pixels it passes through like a stone thrown into a pond.

6. Pixelation (Mosaic) 👾
Trigger: High Energy Drops (Climax) Description: Triggered only during the most intense moments of a track (The Drop). The entire window content momentarily lowers in resolution, turning into large "8-bit" blocks (Mosaic effect) before snapping back to crystal clear HD. This visually represents the "destruction" of the beat.

7🚀 تم تفعيل Radial Zoom Blur!
أصبح النظام الآن أسرع وأكثر حيوية:

التأثير: عند ضربات الـ Snare القوية، يحدث تمويه شعاعي (Radial Blur) من المركز للخارج.
الشعور: يعطيك إحساساً بالاندفاع للأمام (Warp Speed) مع كل إيقاع.
## Building

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install meson ninja-build libconfig-dev uthash-dev libev-dev \
    libpcre2-dev libepoxy-dev libdbus-1-dev libpixman-1-dev \
    libx11-dev libx11-xcb-dev libxcb1-dev libxcb-composite0-dev \
    libxcb-damage0-dev libxcb-glx0-dev libxcb-present-dev \
    libxcb-randr0-dev libxcb-render0-dev libxcb-render-util0-dev \
    libxcb-shape0-dev libxcb-sync-dev libxcb-xfixes0-dev \
    libxcb-image0-dev libxcb-util-dev

# Build
meson setup build
ninja -C build
```

## Installation

```bash
./install.sh
```

## Usage

```bash
# Start with D-Bus support (required for hot reload)
venom-miasma --config ~/.config/venom-miasma/venom.conf --dbus -b

# Show version
venom-miasma --version
```

## D-Bus API

Service: `org.venomlinux.miasma._0`
Object: `/org/venomlinux/miasma`
Interface: `org.venomlinux.miasma`

### Hot Reload Options (opts_set)

Change these settings **live without restart**:

| Option | Type | Description | Example |
|--------|------|-------------|---------|
| `shadow_radius` | int32 | Shadow blur size | 12 |
| `shadow_opacity` | double | Shadow opacity (0.0-1.0) | 0.75 |
| `shadow_red` | double | Shadow red color (0.0-1.0) | 0.0 |
| `shadow_green` | double | Shadow green color (0.0-1.0) | 0.0 |
| `shadow_blue` | double | Shadow blue color (0.0-1.0) | 0.0 |
| `shadow_offset_x` | int32 | Shadow X offset | -15 |
| `shadow_offset_y` | int32 | Shadow Y offset | -15 |
| `blur_strength` | int32 | Blur intensity (0-20) | 5 |
| `fade_in_step` | double | Fade-in speed (0.01-1.0) | 0.07 |
| `fade_out_step` | double | Fade-out speed (0.01-1.0) | 0.07 |
| `fade_delta` | int32 | Fade step time (ms) | 10 |
| `no_fading_openclose` | boolean | Disable open/close fade | false |
| `unredir_if_possible` | boolean | Allow unredirection | false |

### Audio Visualizer Effects (effect_set)

Control real-time effects using `effect_set`.
Method: `org.venomlinux.miasma.effect_set`

| Effect Name | Description |
|-------------|-------------|
| `pulse` | Beat-reactive shadow pulsing |
| `flash` | Random vibrant color flashes on beats |
| `neon` | Rotating neon background aura |
| `chromatic` | RGB split glitch on high frequencies |
| `shockwave` | Radial distortion wave on bass drops |
| `pixelation` | Screen pixelation on high energy drops |
| `radial_zoom` | Radial blur zoom on snare hits |

# Path: /org/venomlinux/miasma
dbus-send --session --print-reply \
  --dest=org.venomlinux.miasma._0 \
  /org/venomlinux/miasma \
  org.venomlinux.miasma.effect_set \
  string:"pulse" boolean:true


**Usage Syntax:**
```bash
dbus-send --print-reply --dest=org.venomlinux.miasma._0 \
  /org/venomlinux/miasma org.venomlinux.miasma.effect_set \
  string:"<effect_name>" boolean:<true|false>
```

**Examples:**
```bash
# Enable Pulse effect
dbus-send --print-reply --dest=org.venomlinux.miasma._0 \
  /org/venomlinux/miasma org.venomlinux.miasma.effect_set \
  string:"pulse" boolean:true

# Disable Flash effect
dbus-send --print-reply --dest=org.venomlinux.miasma._0 \
  /org/venomlinux/miasma org.venomlinux.miasma.effect_set \
  string:"flash" boolean:false

# Enable Radial Zoom (Warp Speed)
dbus-send --print-reply --dest=org.venomlinux.miasma._0 \
  /org/venomlinux/miasma org.venomlinux.miasma.effect_set \
  string:"radial_zoom" boolean:true

# Enable all other effects
dbus-send --print-reply --dest=org.venomlinux.miasma._0 /org/venomlinux/miasma org.venomlinux.miasma.effect_set string:"neon" boolean:true
dbus-send --print-reply --dest=org.venomlinux.miasma._0 /org/venomlinux/miasma org.venomlinux.miasma.effect_set string:"chromatic" boolean:true
dbus-send --print-reply --dest=org.venomlinux.miasma._0 /org/venomlinux/miasma org.venomlinux.miasma.effect_set string:"shockwave" boolean:true
dbus-send --print-reply --dest=org.venomlinux.miasma._0 /org/venomlinux/miasma org.venomlinux.miasma.effect_set string:"pixelation" boolean:true
```

### Example Commands

```bash
# Change shadow radius (hot reload)
dbus-send --print-reply --dest=org.venomlinux.miasma._0 \
  /org/venomlinux/miasma org.venomlinux.miasma.opts_set \
  string:"shadow_radius" int32:50

# Change blur strength (hot reload)
dbus-send --print-reply --dest=org.venomlinux.miasma._0 \
  /org/venomlinux/miasma org.venomlinux.miasma.opts_set \
  string:"blur_strength" int32:10

# Change shadow opacity
dbus-send --print-reply --dest=org.venomlinux.miasma._0 \
  /org/venomlinux/miasma org.venomlinux.miasma.opts_set \
  string:"shadow_opacity" double:1.0

# Change shadow color (white shadow)
dbus-send --print-reply --dest=org.venomlinux.miasma._0 \
  /org/venomlinux/miasma org.venomlinux.miasma.opts_set \
  string:"shadow_red" double:1.0

dbus-send --print-reply --dest=org.venomlinux.miasma._0 \
  /org/venomlinux/miasma org.venomlinux.miasma.opts_set \
  string:"shadow_green" double:1.0

dbus-send --print-reply --dest=org.venomlinux.miasma._0 \
  /org/venomlinux/miasma org.venomlinux.miasma.opts_set \
  string:"shadow_blue" double:1.0

# Get current shadow radius
dbus-send --print-reply --dest=org.venomlinux.miasma._0 \
  /org/venomlinux/miasma org.venomlinux.miasma.opts_set \
  string:"shadow_radius" int32:50

# List all windows
dbus-send --print-reply --dest=org.venomlinux.miasma._0 \
  /org/venomlinux/miasma org.venomlinux.miasma.list_win

# Force repaint
dbus-send --print-reply --dest=org.venomlinux.miasma._0 \
  /org/venomlinux/miasma org.venomlinux.miasma.repaint

# Reset compositor
dbus-send --print-reply --dest=org.venomlinux.miasma._0 \
  /org/venomlinux/miasma org.venomlinux.miasma.reset
```

### Read-Only Options (opts_get)

| Option | Type | Description |
|--------|------|-------------|
| `version` | string | Compositor version |
| `pid` | int32 | Process ID |
| `backend` | string | Current backend |
| `shadow_red/green/blue` | double | Shadow color (0.0-1.0) |
| `vsync` | boolean | VSync status |
| `use_damage` | boolean | Damage tracking status |

## Configuration

Config file: `~/.config/venom-miasma/venom.conf`

```conf
# Blur
blur-method = "dual_kawase";
blur-strength = 5;

# Shadows
shadow = true;
shadow-radius = 12;
shadow-opacity = 0.75;

# Corners
corner-radius = 12;

# Backend
backend = "glx";
vsync = true;

# D-Bus (optional, but required for hot reload)
dbus = true;
```

