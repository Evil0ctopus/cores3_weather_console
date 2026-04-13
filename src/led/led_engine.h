#pragma once

#include <Arduino.h>
#include <M5Unified.h>

#include "../weather/weather_models.h"

namespace led {

typedef void (*LedEventCallback)(void* userContext, const char* eventName, const String& details);

class LedEngine {
 public:
	enum class TouchKind : uint8_t {
		Tap = 0,
		SwipeLeft,
		SwipeRight,
		SwipeUp,
		SwipeDown,
		LongPress,
	};

	enum class AlertLevel : uint8_t {
		None = 0,
		Info,
		Warning,
		Critical,
	};

	enum class IdleMode : uint8_t {
		Auto = 0,
		Off,
		Breathing,
		CyanBreathing,
		Sunrise,
		Sunset,
		WeatherGlow,
	};

	void begin(uint8_t brightness = 96);
	void update();

	void updateWeatherMood(const WeatherData& data);
	void pageTransition(uint8_t fromPage, uint8_t toPage);
	void touchEvent(TouchKind kind, int16_t deltaX = 0, int16_t deltaY = 0);
	void alert(AlertLevel level, const String& message = String(), uint32_t durationMs = 0);
	void progress(uint8_t percent, bool active = true);
	void idle(IdleMode mode = IdleMode::Auto, bool active = true);
	void bootAnimation(float rotationPhase, float intensity, bool active = true, bool lockIn = false, bool fadingOut = false);
	void setSystemStatus(bool booting, bool wifiConnected, bool warningActive);
	void clearAlert();
	void setEventCallback(LedEventCallback callback, void* userContext);
	String statusLabel() const;

	bool isReady() const;
	size_t ledCount() const;

 private:
	static const size_t kMaxLeds = 8;

	struct MoodPalette {
		RGBColor primary;
		RGBColor secondary;
		RGBColor accent;
		bool daylight;
	};

	struct TransitionState {
		bool active;
		bool forward;
		uint8_t fromPage;
		uint8_t toPage;
		uint32_t startedAtMs;
		uint32_t durationMs;
	};

	struct TouchState {
		bool active;
		TouchKind kind;
		int16_t deltaX;
		int16_t deltaY;
		uint32_t startedAtMs;
		uint32_t durationMs;
	};

	struct AlertState {
		bool active;
		AlertLevel level;
		String message;
		uint32_t startedAtMs;
		uint32_t durationMs;
	};

	struct ProgressState {
		bool active;
		uint8_t percent;
		uint32_t updatedAtMs;
	};

	struct IdleState {
		bool active;
		IdleMode requestedMode;
	};

	struct SystemState {
		bool booting;
		bool wifiConnected;
		bool warningActive;
		uint32_t changedAtMs;
	};

	struct BootState {
		bool active;
		float rotationPhase;
		float intensity;
		bool lockIn;
		bool fadingOut;
		uint32_t updatedAtMs;
	};

	enum class LayerId : uint8_t {
		WeatherBase = 0,
		Idle,
		Progress,
		Transition,
		Touch,
		System,
		Boot,
		Alert,
		CriticalAlert,
	};

	struct LayerEntry {
		LayerId id;
		uint8_t priority;
	};

	void renderFrame(uint32_t nowMs);
	uint8_t buildLayerStack(uint32_t nowMs, LayerEntry* outStack, uint8_t maxEntries) const;
	void renderLayer(LayerId id, uint32_t nowMs);
	void pushToHardware();

	MoodPalette chooseMoodPalette(const WeatherData& data) const;
	IdleMode resolveIdleMode(uint32_t nowMs) const;
	RGBColor pageColor(uint8_t page) const;

	void renderWeatherBase(uint32_t nowMs);
	void renderIdleLayer(uint32_t nowMs);
	void renderProgressLayer(uint32_t nowMs);
	void renderTransitionLayer(uint32_t nowMs);
	void renderTouchLayer(uint32_t nowMs);
	void renderSystemLayer(uint32_t nowMs);
	void renderBootLayer(uint32_t nowMs);
	void renderAlertLayer(uint32_t nowMs);
	void renderCriticalAlert(uint32_t nowMs);
	void emitEvent(const char* eventName, const String& details) const;

	void clearFrame(const RGBColor& value = RGBColor());
	void expireTransientStates(uint32_t nowMs);

	bool initialized_ = false;
	bool ready_ = false;
	uint8_t brightness_ = 96;
	size_t ledCount_ = 0;
	uint32_t lastRenderMs_ = 0;
	uint32_t lastInitAttemptMs_ = 0;

	MoodPalette mood_;
	TransitionState transition_;
	TouchState touch_;
	AlertState alert_;
	ProgressState progress_;
	IdleState idle_;
	SystemState system_;
	BootState boot_;
	LedEventCallback eventCallback_ = nullptr;
	void* eventUserContext_ = nullptr;

	RGBColor frame_[kMaxLeds];
	RGBColor lastPushed_[kMaxLeds];
};

}  // namespace led
