#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include <memory>
#include <queue>
#include <vector>

namespace audio {

enum class SoundType : uint8_t {
	BootStartupChime = 0,
	BootWhoosh,
	BootReady,
	TouchTap,
	TouchLongPressRise,
	SwipeWhoosh,
	PageTransitionWhoosh,
	SystemInfoPageTone,
	WeatherSevereAlert,
	WeatherTornadoWarning,
	WeatherFloodWarning,
	RadarUpdatePing,
	WiFiConnected,
	WiFiLost,
	SettingsSaved,
	ErrorBuzz,
	LedAlert,
};

enum class BootSound : uint8_t {
	StartupChime = 0,
	Whoosh,
	Ready,
};

enum class TouchSound : uint8_t {
	TapClick = 0,
	SwipeWhoosh,
	LongPressRise,
};

enum class SwipeSound : uint8_t {
	PageTransitionWhoosh = 0,
	SystemInfoEnterTone,
};

enum class AlertSound : uint8_t {
	SevereWeather = 0,
	TornadoWarning,
	FloodWarning,
	RadarUpdate,
};

enum class SystemSound : uint8_t {
	WiFiConnectedChime = 0,
	WiFiLostDescending,
	SettingsSavedTone,
	ErrorBuzz,
};

enum class ThemeId : uint8_t {
	Default = 0,
	Modern,
	Retro,
	Minimal,
	Custom1,
	Custom2,
};

// Audio file metadata
struct AudioFile {
	SoundType type;
	String filename;  // Path in SPIFFS: /audio/theme_name/sound.wav
	uint16_t durationMs;   // Approximate duration
	uint8_t volume;        // 0-255, 0 = use default
	bool loop;             // For continuous playback
};

// Audio playback callback for integration with UI/LED systems
typedef void (*AudioEventCallback)(void* userContext, const char* eventName,
                                    const String& details);

class AudioEngine {
 public:
	AudioEngine();
	~AudioEngine();

	// Initialization
	bool begin();
	void setTheme(ThemeId themeId, const String& themeName = String());

	// Playback control
	void play(SoundType type, uint8_t volumeOverride = 0);
	void stop();
	void stopAll();
	void pause();
	void resume();

	// Volume control
	void setMasterVolume(uint8_t volume);  // 0-255
	uint8_t getMasterVolume() const;
	void setMuted(bool muted);
	bool isMuted() const;

	// Status queries
	bool isPlaying() const;
	bool isInitialized() const;

	// Update loop (call from main loop)
	void update();

	// Event callback for system integration
	void setEventCallback(AudioEventCallback callback, void* userContext);

	// Sound effect triggers for UI integration
	void onTouchSound(uint8_t touchKind, int16_t deltaX = 0, int16_t deltaY = 0);
	void onPageTransition(uint8_t fromPage, uint8_t toPage);
	void onWeatherAlert(uint8_t alertLevel, const String& alertType = String());
	void onWiFiEvent(const String& eventType);  // "connected", "disconnected"
	void onSettingsChanged();
	void onRadarUpdate();
	void onLedAlert(uint8_t alertLevel);
	void onBootAnimationStart();
	void onBootAnimationEnd();

	// Category helpers requested by integration points
	void playBootSound(BootSound sound);
	void playTouchSound(TouchSound sound);
	void playSwipeSound(SwipeSound sound);
	void playAlertSound(AlertSound sound);
	void playSystemSound(SystemSound sound);

	// Audio configuration and registration
	void registerSound(SoundType type, const String& filename, uint16_t durationMs,
	                   uint8_t defaultVolume = 200, bool loop = false);
	void loadThemeSoundPack(const String& themeName);

	// Debug/diagnostic
	void dumpAudioStatus();

 private:
	struct PlaybackItem {
		SoundType type;
		String filepath;
		uint8_t volume;
		uint32_t startedAtMs;
		uint16_t durationMs;
		bool isPlaying;
	};

	// Internal helper methods
	bool loadAudioFile(const String& filepath, uint8_t* buffer, size_t maxSize,
	                   size_t& outSize);
	void processPlaybackQueue();
	SoundType touchKindToSound(uint8_t touchKind) const;
	bool shouldLoopSound(SoundType type) const;
	uint8_t effectiveVolume(uint8_t soundVolume) const;
	String buildSoundPath(const String& filename) const;
	bool loadWavOrPcmFile(const String& filepath, uint8_t* buffer, size_t maxSize,
													 size_t& outSize);
	void emitEvent(const char* eventName, const String& details);

	// State
	bool initialized_;
	bool playing_;
	bool paused_;
	bool muted_;
	uint8_t masterVolume_;
	ThemeId currentTheme_;
	String currentThemeName_;

	// Audio file registry
	std::vector<AudioFile> soundRegistry_;
	std::queue<PlaybackItem> playbackQueue_;

	// Current playback
	PlaybackItem* currentPlayback_;
	uint8_t* audioBuffer_;

	// Event callback
	AudioEventCallback eventCallback_;
	void* eventUserContext_;

	// Timing
	uint32_t lastUpdateMs_;

	// Configuration
	static constexpr size_t kMaxAudioBufferSize = 32768;  // 32KB for audio samples
	static constexpr size_t kMaxQueueSize = 10;
	static constexpr uint32_t kUpdateIntervalMs = 50;
};

}  // namespace audio
