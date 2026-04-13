#include "audio_engine.h"

#include <SPIFFS.h>

#include "../system/debug_log.h"

extern app::DebugLog gDebugLog;

namespace audio {

namespace {

const char* kDefaultAudioPath = "/audio";
const char* kDefaultThemeName = "default";
constexpr uint32_t kDefaultPcmSampleRate = 22050;

const AudioFile kDefaultSoundPack[] = {
  {SoundType::BootStartupChime, "boot_start.wav", 900, 242, false},
    {SoundType::BootWhoosh, "boot_whoosh.wav", 260, 232, false},
    {SoundType::BootReady, "boot_ready.wav", 560, 244, false},
    {SoundType::TouchTap, "touch_tap.wav", 90, 170, false},
  {SoundType::TouchLongPressRise, "touch_hold.wav", 280, 190, false},
  {SoundType::SwipeWhoosh, "swipe.wav", 120, 185, false},
  {SoundType::PageTransitionWhoosh, "page_whoosh.wav", 180, 190, false},
  {SoundType::SystemInfoPageTone, "sysinfo.wav", 260, 200, false},
  {SoundType::WeatherSevereAlert, "wx_severe.wav", 550, 215, false},
  {SoundType::WeatherTornadoWarning, "wx_tornado.wav", 720, 230, true},
  {SoundType::WeatherFloodWarning, "wx_flood.wav", 620, 210, false},
    {SoundType::RadarUpdatePing, "radar_ping.wav", 140, 180, false},
  {SoundType::WiFiConnected, "wifi_on.wav", 350, 205, false},
  {SoundType::WiFiLost, "wifi_off.wav", 420, 205, false},
  {SoundType::SettingsSaved, "settings_ok.wav", 240, 200, false},
    {SoundType::ErrorBuzz, "error_buzz.wav", 260, 220, false},
    {SoundType::LedAlert, "led_alert.wav", 300, 210, false},
};

const size_t kDefaultSoundPackSize = sizeof(kDefaultSoundPack) / sizeof(kDefaultSoundPack[0]);

void WriteU16LE(uint8_t* out, uint16_t value) {
  out[0] = static_cast<uint8_t>(value & 0xFF);
  out[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

void WriteU32LE(uint8_t* out, uint32_t value) {
  out[0] = static_cast<uint8_t>(value & 0xFF);
  out[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
  out[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
  out[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

bool EndsWithIgnoreCase(const String& value, const char* suffix) {
  if (suffix == nullptr) {
    return false;
  }
  String v = value;
  String s = suffix;
  v.toLowerCase();
  s.toLowerCase();
  return v.endsWith(s);
}

void ApplyBoundaryEnvelopeToWavPcm16(uint8_t* wavData, size_t wavSize) {
  if (wavData == nullptr || wavSize < 64) {
    return;
  }
  if (memcmp(wavData, "RIFF", 4) != 0 || memcmp(wavData + 8, "WAVE", 4) != 0) {
    return;
  }

  // Minimal parser for common PCM16 mono/stereo WAV files.
  uint16_t audioFormat = 0;
  uint16_t bitsPerSample = 0;
  size_t dataOffset = 0;
  uint32_t dataSize = 0;

  size_t offset = 12;
  while (offset + 8 <= wavSize) {
    const char* chunkId = reinterpret_cast<const char*>(wavData + offset);
    const uint32_t chunkSize =
        static_cast<uint32_t>(wavData[offset + 4]) |
        (static_cast<uint32_t>(wavData[offset + 5]) << 8) |
        (static_cast<uint32_t>(wavData[offset + 6]) << 16) |
        (static_cast<uint32_t>(wavData[offset + 7]) << 24);
    const size_t chunkData = offset + 8;
    if (chunkData + chunkSize > wavSize) {
      break;
    }

    if (memcmp(chunkId, "fmt ", 4) == 0 && chunkSize >= 16) {
      audioFormat = static_cast<uint16_t>(wavData[chunkData]) |
                    (static_cast<uint16_t>(wavData[chunkData + 1]) << 8);
      bitsPerSample = static_cast<uint16_t>(wavData[chunkData + 14]) |
                      (static_cast<uint16_t>(wavData[chunkData + 15]) << 8);
    } else if (memcmp(chunkId, "data", 4) == 0) {
      dataOffset = chunkData;
      dataSize = chunkSize;
      break;
    }

    offset = chunkData + chunkSize + (chunkSize & 1U);
  }

  if (audioFormat != 1 || bitsPerSample != 16 || dataOffset == 0 || dataSize < 8) {
    return;
  }

  int16_t* samples = reinterpret_cast<int16_t*>(wavData + dataOffset);
  const size_t sampleCount = dataSize / sizeof(int16_t);
  const size_t fadeSamples = (sampleCount < 512) ? (sampleCount / 8) : 256;
  if (fadeSamples < 4 || fadeSamples * 2 >= sampleCount) {
    return;
  }

  for (size_t i = 0; i < fadeSamples; ++i) {
    const int32_t inGain = static_cast<int32_t>((i * 32767U) / fadeSamples);
    const size_t outIndex = sampleCount - 1 - i;
    const int32_t outGain = static_cast<int32_t>((i * 32767U) / fadeSamples);

    samples[i] = static_cast<int16_t>((static_cast<int32_t>(samples[i]) * inGain) / 32767);
    samples[outIndex] = static_cast<int16_t>((static_cast<int32_t>(samples[outIndex]) * outGain) / 32767);
  }
}

}  // namespace

AudioEngine::AudioEngine()
    : initialized_(false),
      playing_(false),
      paused_(false),
      muted_(false),
      masterVolume_(200),
      currentTheme_(ThemeId::Default),
      currentThemeName_(kDefaultThemeName),
      currentPlayback_(nullptr),
      audioBuffer_(nullptr),
      eventCallback_(nullptr),
      eventUserContext_(nullptr),
      lastUpdateMs_(0) {}

AudioEngine::~AudioEngine() {
  if (audioBuffer_ != nullptr) {
    free(audioBuffer_);
    audioBuffer_ = nullptr;
  }
  if (currentPlayback_ != nullptr) {
    delete currentPlayback_;
    currentPlayback_ = nullptr;
  }
}

bool AudioEngine::begin() {
  if (initialized_) {
    return true;
  }

  auto spkCfg = M5.Speaker.config();
  // 44.1 kHz is typically cleaner on CoreS3's internal speaker path than 64 kHz.
  spkCfg.sample_rate = 44100;
  M5.Speaker.config(spkCfg);
  M5.Speaker.begin();
  M5.Speaker.setVolume(masterVolume_);

  audioBuffer_ = static_cast<uint8_t*>(malloc(kMaxAudioBufferSize));
  if (audioBuffer_ == nullptr) {
    ::gDebugLog.log("AudioEngine", "Failed to allocate audio buffer");
    return false;
  }

  soundRegistry_.clear();
  for (size_t i = 0; i < kDefaultSoundPackSize; ++i) {
    soundRegistry_.push_back(kDefaultSoundPack[i]);
  }

  loadThemeSoundPack(kDefaultThemeName);
  initialized_ = true;
  emitEvent("initialized", "audio engine ready");
  return true;
}

void AudioEngine::setTheme(ThemeId themeId, const String& themeName) {
  currentTheme_ = themeId;
  if (themeName.length() > 0) {
    currentThemeName_ = themeName;
  } else {
    switch (themeId) {
      case ThemeId::Default:
        currentThemeName_ = "default";
        break;
      case ThemeId::Modern:
        currentThemeName_ = "modern";
        break;
      case ThemeId::Retro:
        currentThemeName_ = "retro";
        break;
      case ThemeId::Minimal:
        currentThemeName_ = "minimal";
        break;
      default:
        currentThemeName_ = "default";
        break;
    }
  }

  loadThemeSoundPack(currentThemeName_);
  emitEvent("theme_changed", currentThemeName_);
}

void AudioEngine::play(SoundType type, uint8_t volumeOverride) {
  if (!initialized_ || muted_) {
    return;
  }

  if (playbackQueue_.size() >= kMaxQueueSize) {
    ::gDebugLog.log("AudioEngine", "Playback queue full, dropping sound");
    return;
  }

  AudioFile* foundSound = nullptr;
  for (auto& sound : soundRegistry_) {
    if (sound.type == type) {
      foundSound = &sound;
      break;
    }
  }

  if (foundSound == nullptr) {
    ::gDebugLog.log("AudioEngine", String("Sound type not registered: ") + static_cast<int>(type));
    return;
  }

  PlaybackItem item;
  item.type = type;
  item.filepath = buildSoundPath(foundSound->filename);
  item.volume = (volumeOverride > 0) ? volumeOverride : foundSound->volume;
  item.startedAtMs = 0;
  item.durationMs = foundSound->durationMs;
  item.isPlaying = false;
  playbackQueue_.push(item);
}

void AudioEngine::stop() {
  if (M5.Speaker.isPlaying()) {
    M5.Speaker.stop();
  }
  if (currentPlayback_ != nullptr) {
    delete currentPlayback_;
    currentPlayback_ = nullptr;
  }
  playing_ = false;
}

void AudioEngine::stopAll() {
  stop();
  while (!playbackQueue_.empty()) {
    playbackQueue_.pop();
  }
  emitEvent("stopped_all", "all sounds cleared");
}

void AudioEngine::pause() {
  if (!playing_) {
    return;
  }
  if (M5.Speaker.isPlaying()) {
    M5.Speaker.stop();
  }
  paused_ = true;
  playing_ = false;
}

void AudioEngine::resume() {
  if (!paused_) {
    return;
  }
  paused_ = false;
}

void AudioEngine::setMasterVolume(uint8_t volume) {
  masterVolume_ = volume;
  M5.Speaker.setVolume(masterVolume_);
}

uint8_t AudioEngine::getMasterVolume() const {
  return masterVolume_;
}

void AudioEngine::setMuted(bool muted) {
  muted_ = muted;
  if (muted_) {
    stopAll();
  }
}

bool AudioEngine::isMuted() const {
  return muted_;
}

bool AudioEngine::isPlaying() const {
  return playing_;
}

bool AudioEngine::isInitialized() const {
  return initialized_;
}

void AudioEngine::update() {
  if (!initialized_) {
    return;
  }

  const uint32_t now = millis();
  if ((now - lastUpdateMs_) < kUpdateIntervalMs) {
    return;
  }
  lastUpdateMs_ = now;

  processPlaybackQueue();

  if (currentPlayback_ != nullptr && currentPlayback_->isPlaying) {
    if (!M5.Speaker.isPlaying()) {
      const SoundType finishedType = currentPlayback_->type;
      delete currentPlayback_;
      currentPlayback_ = nullptr;
      playing_ = false;
      emitEvent("sound_finished", String(static_cast<int>(finishedType)));

      if (shouldLoopSound(finishedType)) {
        play(finishedType);
      }
      return;
    }
  }
}

void AudioEngine::processPlaybackQueue() {
  if (paused_ || (currentPlayback_ != nullptr && currentPlayback_->isPlaying)) {
    return;
  }

  if (playbackQueue_.empty()) {
    return;
  }

  PlaybackItem nextItem = playbackQueue_.front();
  playbackQueue_.pop();

  if (currentPlayback_ != nullptr) {
    delete currentPlayback_;
    currentPlayback_ = nullptr;
  }

  size_t audioSize = 0;
  if (!loadWavOrPcmFile(nextItem.filepath, audioBuffer_, kMaxAudioBufferSize, audioSize) || audioSize == 0) {
    ::gDebugLog.log("AudioEngine", String("Failed to load: ") + nextItem.filepath);
    return;
  }

  ApplyBoundaryEnvelopeToWavPcm16(audioBuffer_, audioSize);

  currentPlayback_ = new PlaybackItem(nextItem);
  currentPlayback_->startedAtMs = millis();
  currentPlayback_->isPlaying = true;

  M5.Speaker.setVolume(effectiveVolume(nextItem.volume));
  M5.Speaker.playWav(audioBuffer_, audioSize);
  playing_ = true;

  emitEvent("sound_started", String("file=") + nextItem.filepath);
}

bool AudioEngine::loadAudioFile(const String& filepath,
                                uint8_t* buffer,
                                size_t maxSize,
                                size_t& outSize) {
  if (!SPIFFS.exists(filepath)) {
    ::gDebugLog.log("AudioEngine", String("File not found: ") + filepath);
    return false;
  }

  File file = SPIFFS.open(filepath, "r");
  if (!file) {
    ::gDebugLog.log("AudioEngine", String("Cannot open: ") + filepath);
    return false;
  }

  const size_t fileSize = file.size();
  if (fileSize > maxSize) {
    file.close();
    ::gDebugLog.log("AudioEngine", String("File too large: ") + filepath);
    return false;
  }

  const size_t bytesRead = file.read(buffer, fileSize);
  file.close();
  if (bytesRead != fileSize) {
    ::gDebugLog.log("AudioEngine", String("Incomplete read: ") + filepath);
    return false;
  }

  outSize = bytesRead;
  return true;
}

bool AudioEngine::loadWavOrPcmFile(const String& filepath,
                                   uint8_t* buffer,
                                   size_t maxSize,
                                   size_t& outSize) {
  if (!EndsWithIgnoreCase(filepath, ".pcm")) {
    return loadAudioFile(filepath, buffer, maxSize, outSize);
  }

  if (!SPIFFS.exists(filepath)) {
    ::gDebugLog.log("AudioEngine", String("PCM not found: ") + filepath);
    return false;
  }

  File file = SPIFFS.open(filepath, "r");
  if (!file) {
    ::gDebugLog.log("AudioEngine", String("Cannot open PCM: ") + filepath);
    return false;
  }

  const size_t pcmSize = file.size();
  const size_t wavHeaderSize = 44;
  if ((pcmSize + wavHeaderSize) > maxSize) {
    file.close();
    ::gDebugLog.log("AudioEngine", String("PCM too large: ") + filepath);
    return false;
  }

  uint8_t* wav = buffer;
  memcpy(wav + 0, "RIFF", 4);
  WriteU32LE(wav + 4, static_cast<uint32_t>(pcmSize + 36));
  memcpy(wav + 8, "WAVE", 4);
  memcpy(wav + 12, "fmt ", 4);
  WriteU32LE(wav + 16, 16);
  WriteU16LE(wav + 20, 1);
  WriteU16LE(wav + 22, 1);
  WriteU32LE(wav + 24, kDefaultPcmSampleRate);
  WriteU32LE(wav + 28, kDefaultPcmSampleRate * 2);
  WriteU16LE(wav + 32, 2);
  WriteU16LE(wav + 34, 16);
  memcpy(wav + 36, "data", 4);
  WriteU32LE(wav + 40, static_cast<uint32_t>(pcmSize));

  const size_t readBytes = file.read(wav + wavHeaderSize, pcmSize);
  file.close();
  if (readBytes != pcmSize) {
    ::gDebugLog.log("AudioEngine", String("PCM read failed: ") + filepath);
    return false;
  }

  outSize = readBytes + wavHeaderSize;
  return true;
}

void AudioEngine::setEventCallback(AudioEventCallback callback, void* userContext) {
  eventCallback_ = callback;
  eventUserContext_ = userContext;
}

void AudioEngine::emitEvent(const char* eventName, const String& details) {
  if (eventCallback_ != nullptr) {
    eventCallback_(eventUserContext_, eventName, details);
  }
}

SoundType AudioEngine::touchKindToSound(uint8_t touchKind) const {
  switch (touchKind) {
    case 0:
      return SoundType::TouchTap;
    case 5:
      return SoundType::TouchLongPressRise;
    default:
      return SoundType::SwipeWhoosh;
  }
}

bool AudioEngine::shouldLoopSound(SoundType type) const {
  return type == SoundType::WeatherTornadoWarning;
}

uint8_t AudioEngine::effectiveVolume(uint8_t soundVolume) const {
  const uint16_t scaled = static_cast<uint16_t>(masterVolume_) * static_cast<uint16_t>(soundVolume);
  return static_cast<uint8_t>((scaled / 255U) & 0xFF);
}

String AudioEngine::buildSoundPath(const String& filename) const {
  return String(kDefaultAudioPath) + "/" + currentThemeName_ + "/" + filename;
}

void AudioEngine::onTouchSound(uint8_t touchKind, int16_t deltaX, int16_t deltaY) {
  (void)deltaX;
  (void)deltaY;
  play(touchKindToSound(touchKind));
}

void AudioEngine::onPageTransition(uint8_t fromPage, uint8_t toPage) {
  (void)fromPage;
  playSwipeSound(SwipeSound::PageTransitionWhoosh);
  if (toPage == 4) {
    playSwipeSound(SwipeSound::SystemInfoEnterTone);
  }
}

void AudioEngine::onWeatherAlert(uint8_t alertLevel, const String& alertType) {
  String lower = alertType;
  lower.toLowerCase();
  if (lower.indexOf("tornado") >= 0 || alertLevel >= 3) {
    playAlertSound(AlertSound::TornadoWarning);
    return;
  }
  if (lower.indexOf("flood") >= 0) {
    playAlertSound(AlertSound::FloodWarning);
    return;
  }
  if (alertLevel > 0) {
    playAlertSound(AlertSound::SevereWeather);
  }
}

void AudioEngine::onWiFiEvent(const String& eventType) {
  if (eventType == "connected") {
    playSystemSound(SystemSound::WiFiConnectedChime);
  } else if (eventType == "disconnected") {
    playSystemSound(SystemSound::WiFiLostDescending);
  }
}

void AudioEngine::onSettingsChanged() {
  playSystemSound(SystemSound::SettingsSavedTone);
}

void AudioEngine::onRadarUpdate() {
  playAlertSound(AlertSound::RadarUpdate);
}

void AudioEngine::onLedAlert(uint8_t alertLevel) {
  if (alertLevel > 0) {
    play(SoundType::LedAlert);
  }
}

void AudioEngine::onBootAnimationStart() {
  playBootSound(BootSound::StartupChime);
}

void AudioEngine::onBootAnimationEnd() {
  playBootSound(BootSound::Ready);
}

void AudioEngine::playBootSound(BootSound sound) {
  switch (sound) {
    case BootSound::StartupChime:
      play(SoundType::BootStartupChime);
      break;
    case BootSound::Whoosh:
      play(SoundType::BootWhoosh);
      break;
    case BootSound::Ready:
      play(SoundType::BootReady);
      break;
  }
}

void AudioEngine::playTouchSound(TouchSound sound) {
  switch (sound) {
    case TouchSound::TapClick:
      play(SoundType::TouchTap);
      break;
    case TouchSound::SwipeWhoosh:
      play(SoundType::SwipeWhoosh);
      break;
    case TouchSound::LongPressRise:
      play(SoundType::TouchLongPressRise);
      break;
  }
}

void AudioEngine::playSwipeSound(SwipeSound sound) {
  switch (sound) {
    case SwipeSound::PageTransitionWhoosh:
      play(SoundType::PageTransitionWhoosh);
      break;
    case SwipeSound::SystemInfoEnterTone:
      play(SoundType::SystemInfoPageTone);
      break;
  }
}

void AudioEngine::playAlertSound(AlertSound sound) {
  switch (sound) {
    case AlertSound::SevereWeather:
      play(SoundType::WeatherSevereAlert);
      break;
    case AlertSound::TornadoWarning:
      play(SoundType::WeatherTornadoWarning);
      break;
    case AlertSound::FloodWarning:
      play(SoundType::WeatherFloodWarning);
      break;
    case AlertSound::RadarUpdate:
      play(SoundType::RadarUpdatePing);
      break;
  }
}

void AudioEngine::playSystemSound(SystemSound sound) {
  switch (sound) {
    case SystemSound::WiFiConnectedChime:
      play(SoundType::WiFiConnected);
      break;
    case SystemSound::WiFiLostDescending:
      play(SoundType::WiFiLost);
      break;
    case SystemSound::SettingsSavedTone:
      play(SoundType::SettingsSaved);
      break;
    case SystemSound::ErrorBuzz:
      play(SoundType::ErrorBuzz);
      break;
  }
}

void AudioEngine::registerSound(SoundType type,
                                const String& filename,
                                uint16_t durationMs,
                                uint8_t defaultVolume,
                                bool loop) {
  for (auto& sound : soundRegistry_) {
    if (sound.type == type) {
      sound.filename = filename;
      sound.durationMs = durationMs;
      sound.volume = defaultVolume;
      sound.loop = loop;
      return;
    }
  }

  AudioFile newSound;
  newSound.type = type;
  newSound.filename = filename;
  newSound.durationMs = durationMs;
  newSound.volume = defaultVolume;
  newSound.loop = loop;
  soundRegistry_.push_back(newSound);
}

void AudioEngine::loadThemeSoundPack(const String& themeName) {
  const String packPath = String(kDefaultAudioPath) + "/" + themeName;
  if (!SPIFFS.exists(packPath)) {
    ::gDebugLog.log("AudioEngine", String("Theme pack not found: ") + themeName);
    if (themeName != kDefaultThemeName) {
      loadThemeSoundPack(kDefaultThemeName);
    }
    return;
  }

  emitEvent("theme_pack_loaded", themeName);
}

void AudioEngine::dumpAudioStatus() {
  String status = String("initialized=") + (initialized_ ? "1" : "0");
  status += String(",playing=") + (playing_ ? "1" : "0");
  status += String(",muted=") + (muted_ ? "1" : "0");
  status += String(",volume=") + String(masterVolume_);
  status += String(",queue=") + String(playbackQueue_.size());
  status += String(",theme=") + currentThemeName_;
  ::gDebugLog.log("AudioEngine", status);
}

}  // namespace audio
