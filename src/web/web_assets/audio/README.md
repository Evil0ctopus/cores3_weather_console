# SPIFFS Audio Directory Structure

This directory contains theme-based sound packs for the M5Stack CoreS3 audio engine.

## Directory Organization

```
src/web/web_assets/audio/
├── README.md (this file)
├── default/
│   ├── boot_start.wav           (startup animation)
│   ├── touch_tap.wav            (~80ms, UI button tap)
│   ├── touch_hold.wav           (~200ms, long press/hold)
│   ├── swipe.wav                (~120ms, swipe gesture)
│   ├── page_whoosh.wav          (~180ms, UI page changed)
│   ├── sysinfo.wav              (~260ms, system info page entry)
│   ├── wx_severe.wav            (~400ms, severe weather alert)
│   ├── wx_tornado.wav           (~600ms, tornado warning)
│   ├── wx_flood.wav             (~500ms, flood warning)
│   ├── wifi_on.wav              (~300ms, network connected)
│   ├── wifi_off.wav             (~300ms, network disconnected)
│   ├── settings_ok.wav          (~250ms, settings saved)
│   ├── radar_ping.wav           (~150ms, radar frame received)
│   └── led_alert.wav            (~350ms, LED alert triggered)
│
├── modern/
│   └── (same files as default, with modern sound design)
│
├── retro/
│   └── (same files as default, with retro sound design)
│
└── minimal/
    └── (same files as default, with minimal sound design)
```

## Audio File Specifications

**Essential Requirements:**
- **Format**: PCM WAV (uncompressed)
- **Bit Depth**: 16-bit (16-bit PCM)
- **Sample Rate**: 44.1 kHz (standard) or 22.05 kHz (lower quality)
- **Channels**: Mono (1 channel)
- **Maximum Size**: 32 KB per file
- **Typical Duration**: 50ms - 2000ms

## Creating Theme Sound Packs

### Step 1: Prepare Audio Files

Create source audio files in your preferred format (MP3, OGG, FLAC, etc.)

### Step 2: Convert to WAV

Using FFmpeg:
```bash
# Basic conversion to 44.1 kHz, 16-bit, mono
ffmpeg -i input.mp3 -acodec pcm_s16le -ar 44100 -ac 1 output.wav

# With normalization
ffmpeg -i input.mp3 -acodec pcm_s16le -ar 44100 -ac 1 -filter:a "loudnorm" output.wav

# Trim to specific duration (e.g., 300ms)
ffmpeg -i input.wav -ss 0.0 -t 0.3 -acodec pcm_s16le -ar 44100 -ac 1 output.wav
```

### Step 3: Verify File Properties

```bash
# Check audio properties
ffprobe -hide_banner -loglevel quiet output.wav

# Get file size
ls -lh output.wav
```

### Step 4: Organize in Theme Directory

1. Create a new theme directory: `src/web/web_assets/audio/my_theme/`
2. Copy all 16 WAV files to the directory
3. Ensure filenames match exactly (case-sensitive on ESP32):
   - `boot_start.wav`, `touch_tap.wav`, `touch_hold.wav`, `swipe.wav`, `page_whoosh.wav`, `sysinfo.wav`, `wx_severe.wav`, `wx_tornado.wav`, `wx_flood.wav`, `radar_ping.wav`, `wifi_on.wav`, `wifi_off.wav`, `settings_ok.wav`, `error_buzz.wav`, `led_alert.wav`, `boot_whoosh.wav`, `boot_ready.wav`

### SPIFFS Path Limit

SPIFFS object names are short. Keep full paths (for example `/audio/default/filename.wav`) at or below 31 characters.

### Step 5: Register Theme in Code

In `audio_engine.cpp`, add to `ThemeId` enum:
```cpp
enum class ThemeId : uint8_t {
    Default = 0,
    Modern,
    Retro,
    Minimal,
    MyCustomTheme,     // Add your theme here
};
```

### Step 6: Build and Upload SPIFFS

```bash
# Build SPIFFS filesystem
pio run -t buildfs

# Upload SPIFFS to device
pio run -t uploadfs
```

## Sound Design Guidelines

### Touch/Gesture Sounds (80-200ms)
- Short, immediate feedback
- High frequency tones (1-4 kHz) work best
- Examples: click, beep, chirp, pop
- Use consistent volume (~180-200)

### Alert Sounds (300-800ms)
- Graduated intensity:
  - Info: 1-2 tones, 400ms (~210 volume)
  - Warning: 2-3 tones, 600ms (~220 volume)
  - Critical: 3-4 tones, 800ms (~230 volume)
- Use ascending/descending pitch to convey urgency

### Status Sounds (150-400ms)
- WiFi connected: Rising tone, bright character (200 volume)
- WiFi disconnected: Falling tone, softer (190 volume)
- Settings confirm: Ascending arpeggio (210 volume)
- Radar update: Subtle blip (160 volume)

### Major Events (180-2000ms)
- Boot animation: Distinctive, memorable (220 volume)
- Page transition: Brief, musical (190 volume)
- LED alert: Attention-grabbing (210 volume)

## Volume Reference

Master volume is 0-255 scale. Recommended defaults:
- Quiet background: 140-160
- Standard/default: 180-210
- Loud/alert: 220-230
- Maximum: 240-255 (may distort)

Per-sound volume override: applies on top of master volume
- Override 0 = use sound's default
- Override 100 = 39% volume
- Override 255 = 100% volume

## Recommended Sound Sources

### Free Resources
- [Freesound.org](https://freesound.org/): CC-licensed samples
- [Zapsplat](https://www.zapsplat.com/): Free sound effects
- [OpenGameArt.org](https://opengameart.org/): Game audio assets
- [Epidemic Sound](https://www.epidemicsound.com/): Royalty-free (paid)

### Tools
- **Audacity** (free): Audio editing and export
- **FFmpeg**: Command-line audio conversion
- **REAPER**: Professional DAW (30-day trial)
- **GarageBand**: macOS built-in
- **Windows 10 Sound Recorder**: Built-in basic recording

## Optimization Tips

### Reduce File Size
1. Lower sample rate to 22.05 kHz (acceptable for UI sounds)
2. Trim silence from beginning/end
3. Reduce duration where possible
4. Use simpler waveforms (less harmonic content)

### Improve Playback Quality
1. Use 44.1 kHz when file size allows
2. Normalize audio to -1dB peak
3. Apply gentle EQ to reduce pops/clicks
4. Avoid extreme compression

### Example: Optimize to 22.05 kHz
```bash
ffmpeg -i input.wav -acodec pcm_s16le -ar 22050 -ac 1 output.wav
# Reduces file size by ~50%, acceptable for short UI sounds
```

## Installing Sound Packs

### From Provided Packs (Default/Modern/Retro/Minimal)

Audio files are already provided in the default SPIFFS structure. Simply:

```bash
pio run -t buildfs
pio run -t uploadfs
```

### From Custom Pack

1. Create `src/web/web_assets/audio/custom_pack/` directory
2. Add all 16 WAV files
3. Build and upload SPIFFS
4. Switch theme in code:
   ```cpp
   gAudioEngine.setTheme(audio::ThemeId::Custom1, "custom_pack");
   ```

## Troubleshooting

### File Not Found Error
- Verify SPIFFS was built: Check output of `pio run -t buildfs`
- Verify SPIFFS was uploaded: Check device serial output
- Check path matches exactly: `/audio/theme_name/filename.wav`
- Verify filename spelling (case-sensitive)

### Audio Quality Issues
- Use 16-bit PCM format, not compressed
- Check sample rate: 44.1 kHz or 22.05 kHz
- Verify mono (1 channel), not stereo
- Trim silence to reduce clicks
- Normalize before export

### File Size Too Large
- Switch to 22.05 kHz sample rate
- Trim duration to minimum needed
- Reduce audio content (fewer voices)
- Use simpler waveforms

### Playback Won't Start
- Confirm M5 speaker is powered
- Check master volume: `gAudioEngine.getMasterVolume() > 0`
- Verify engine initialized: `gAudioEngine.isInitialized() == true`
- Check debug logs for load errors
- Verify SPIFFS partition has space

## Audio Pack Submission

To contribute custom sound packs:

1. Create a subdirectory with your pack name
2. Include all 16 WAV files
3. Add a `PACK_INFO.txt` with:
   - Pack name
   - Author
   - License/Attribution
   - Theme description
4. Test on M5Stack CoreS3 device
5. Submit pull request with `/audio/your_pack/` directory

## Performance Notes

- Audio loading is non-blocking (buffered)
- Playback queue handles up to 10 pending sounds
- Update cycle runs every 50ms
- Maximum buffer size: 32 KB per sound
- SPIFFS read speed: ~100-200 KB/s (typical)

## Related Files

- `audio_engine.h/cpp`: Core audio engine implementation
- `audio_integration_helpers.h`: Integration helper functions
- `AUDIO_INTEGRATION.md`: Detailed integration guide
- `main.cpp`: System initialization and main loop
