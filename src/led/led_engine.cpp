#include "led_engine.h"

#include <WiFi.h>
#include <math.h>
#include <time.h>

#include "led_patterns.h"

namespace led {

namespace {

const uint32_t kFrameIntervalMs = 20;

bool SummaryContains(const String& summary, const char* token) {
	String haystack = summary;
	haystack.toLowerCase();
	String needle = token;
	needle.toLowerCase();
	return haystack.indexOf(needle) >= 0;
}

}  // namespace

void LedEngine::begin(uint8_t brightness) {
	brightness_ = brightness;
	initialized_ = true;
	lastInitAttemptMs_ = millis();
	const bool beginOk = M5.Led.begin();
	const bool enabled = M5.Led.isEnabled();
	const size_t reportedCount = M5.Led.getCount();
	ready_ = beginOk || enabled || (reportedCount > 0);
	if (!ready_) {
		ready_ = true;
	}
	ledCount_ = reportedCount;
	if (ledCount_ == 0) {
		ledCount_ = kMaxLeds;
	}
	if (ledCount_ > kMaxLeds) {
		ledCount_ = kMaxLeds;
	}

	M5.Led.setAutoDisplay(false);
	if (ready_) {
		M5.Led.setBrightness(brightness_);
	}
	emitEvent("led-init", "ready=" + String(ready_ ? "true" : "false") +
		" begin=" + String(beginOk ? "true" : "false") +
		" enabled=" + String(enabled ? "true" : "false") +
		" count=" + String(static_cast<unsigned>(ledCount_)));

	mood_.primary = patterns::color(24, 48, 96);
	mood_.secondary = patterns::color(6, 14, 28);
	mood_.accent = patterns::color(110, 180, 255);
	mood_.daylight = true;

	transition_.active = false;
	transition_.forward = true;
	transition_.fromPage = 0;
	transition_.toPage = 0;
	transition_.startedAtMs = 0;
	transition_.durationMs = 520;

	touch_.active = false;
	touch_.kind = TouchKind::Tap;
	touch_.deltaX = 0;
	touch_.deltaY = 0;
	touch_.startedAtMs = 0;
	touch_.durationMs = 220;

	alert_.active = false;
	alert_.level = AlertLevel::None;
	alert_.startedAtMs = 0;
	alert_.durationMs = 0;

	progress_.active = false;
	progress_.percent = 0;
	progress_.updatedAtMs = 0;

	idle_.active = true;
	idle_.requestedMode = IdleMode::Auto;

	system_.booting = false;
	system_.wifiConnected = (WiFi.status() == WL_CONNECTED);
	system_.warningActive = false;
	system_.changedAtMs = millis();

	boot_.active = false;
	boot_.rotationPhase = 0.0f;
	boot_.intensity = 0.0f;
	boot_.lockIn = false;
	boot_.fadingOut = false;
	boot_.updatedAtMs = 0;

	clearFrame();
	for (size_t i = 0; i < kMaxLeds; ++i) {
		lastPushed_[i] = RGBColor();
	}
	pushToHardware();
}

void LedEngine::update() {
	if (!initialized_) {
		return;
	}

	if (!ready_ || ledCount_ == 0) {
		const uint32_t nowMs = millis();
		if ((nowMs - lastInitAttemptMs_) >= 1500U) {
			lastInitAttemptMs_ = nowMs;
			const bool beginOk = M5.Led.begin();
			const bool enabled = M5.Led.isEnabled();
			const size_t reportedCount = M5.Led.getCount();
			ready_ = beginOk || enabled || (reportedCount > 0);
			if (!ready_) {
				ready_ = true;
			}
			ledCount_ = reportedCount;
			if (ledCount_ == 0) {
				ledCount_ = kMaxLeds;
			}
			if (ledCount_ > kMaxLeds) {
				ledCount_ = kMaxLeds;
			}
			if (ready_ && ledCount_ > 0) {
				M5.Led.setAutoDisplay(false);
				M5.Led.setBrightness(brightness_);
				clearFrame();
				for (size_t i = 0; i < kMaxLeds; ++i) {
					lastPushed_[i] = RGBColor();
				}
				pushToHardware();
				emitEvent("led-ready", "reinitialized");
			} else {
				emitEvent("led-retry", "ready=false begin=" + String(beginOk ? "true" : "false") +
					" enabled=" + String(enabled ? "true" : "false") +
					" count=" + String(static_cast<unsigned>(reportedCount)));
			}
		}
	}

	if (!ready_ || ledCount_ == 0) {
		return;
	}

	const uint32_t nowMs = millis();
	if ((nowMs - lastRenderMs_) < kFrameIntervalMs) {
		return;
	}

	lastRenderMs_ = nowMs;
	expireTransientStates(nowMs);
	renderFrame(nowMs);
	pushToHardware();
}

void LedEngine::updateWeatherMood(const WeatherData& data) {
	mood_ = chooseMoodPalette(data);
}

void LedEngine::pageTransition(uint8_t fromPage, uint8_t toPage) {
	transition_.active = true;
	transition_.forward = toPage >= fromPage;
	transition_.fromPage = fromPage;
	transition_.toPage = toPage;
	transition_.startedAtMs = millis();
	transition_.durationMs = 520;
	emitEvent("page-transition", "from=" + String(fromPage) + " to=" + String(toPage));
}

void LedEngine::touchEvent(TouchKind kind, int16_t deltaX, int16_t deltaY) {
	touch_.active = true;
	touch_.kind = kind;
	touch_.deltaX = deltaX;
	touch_.deltaY = deltaY;
	touch_.startedAtMs = millis();
	touch_.durationMs = (kind == TouchKind::LongPress) ? 360 : 220;
	emitEvent("touch", "kind=" + String(static_cast<int>(kind)) + " dx=" + String(deltaX) + " dy=" + String(deltaY));
}

void LedEngine::alert(AlertLevel level, const String& message, uint32_t durationMs) {
	if (level == AlertLevel::None) {
		clearAlert();
		return;
	}

	alert_.active = true;
	alert_.level = level;
	alert_.message = message;
	alert_.startedAtMs = millis();
	if (durationMs == 0) {
		alert_.durationMs = (level == AlertLevel::Critical) ? 0 : 2200;
	} else {
		alert_.durationMs = durationMs;
	}
	emitEvent("alert", "level=" + String(static_cast<int>(level)) + " msg=" + message);
}

void LedEngine::progress(uint8_t percent, bool active) {
	if (progress_.active == active && progress_.percent == percent) {
		return;
	}
	progress_.active = active;
	progress_.percent = percent;
	progress_.updatedAtMs = millis();
	emitEvent("progress", "active=" + String(active ? "true" : "false") + " percent=" + String(percent));
}

void LedEngine::idle(IdleMode mode, bool active) {
	if (idle_.requestedMode == mode && idle_.active == active) {
		return;
	}
	idle_.requestedMode = mode;
	idle_.active = active;
	emitEvent("idle", "active=" + String(active ? "true" : "false") + " mode=" + String(static_cast<int>(mode)));
}

void LedEngine::bootAnimation(float rotationPhase, float intensity, bool active, bool lockIn, bool fadingOut) {
	boot_.active = active;
	boot_.rotationPhase = rotationPhase;
	boot_.intensity = patterns::clamp01(intensity);
	boot_.lockIn = lockIn;
	boot_.fadingOut = fadingOut;
	boot_.updatedAtMs = millis();
}

void LedEngine::setSystemStatus(bool booting, bool wifiConnected, bool warningActive) {
	if (system_.booting == booting && system_.wifiConnected == wifiConnected && system_.warningActive == warningActive) {
		return;
	}
	if (system_.booting != booting || system_.wifiConnected != wifiConnected || system_.warningActive != warningActive) {
		system_.changedAtMs = millis();
	}
	system_.booting = booting;
	system_.wifiConnected = wifiConnected;
	system_.warningActive = warningActive;
	emitEvent("system", "boot=" + String(booting ? "true" : "false") + " wifi=" + String(wifiConnected ? "true" : "false") + " warn=" + String(warningActive ? "true" : "false"));
}

void LedEngine::clearAlert() {
	if (!alert_.active) {
		return;
	}
	alert_.active = false;
	alert_.level = AlertLevel::None;
	alert_.message = String();
	alert_.startedAtMs = 0;
	alert_.durationMs = 0;
	emitEvent("alert-clear", "");
}

void LedEngine::setEventCallback(LedEventCallback callback, void* userContext) {
	eventCallback_ = callback;
	eventUserContext_ = userContext;
}

String LedEngine::statusLabel() const {
	if (boot_.active) {
		return "Boot";
	}
	if (alert_.active && alert_.level == AlertLevel::Critical) {
		return "Critical Alert";
	}
	if (alert_.active) {
		return "Alert";
	}
	if (progress_.active) {
		return "Progress";
	}
	if (transition_.active) {
		return "Page Sweep";
	}
	if (touch_.active) {
		return "Touch Response";
	}
	if (!idle_.active) {
		return "Off";
	}

	const IdleMode mode = resolveIdleMode(millis());
	switch (mode) {
		case IdleMode::CyanBreathing:
			return "Cyan Breathing";
		case IdleMode::Breathing:
			return "Breathing";
		case IdleMode::Sunrise:
			return "Sunrise";
		case IdleMode::Sunset:
			return "Sunset";
		case IdleMode::WeatherGlow:
			return "Weather Glow";
		case IdleMode::Off:
			return "Off";
		case IdleMode::Auto:
		default:
			return "Auto";
	}
}

bool LedEngine::isReady() const {
	return ready_ && ledCount_ > 0;
}

size_t LedEngine::ledCount() const {
	return ledCount_;
}

void LedEngine::renderFrame(uint32_t nowMs) {
	LayerEntry stack[8];
	const uint8_t count = buildLayerStack(nowMs, stack, static_cast<uint8_t>(sizeof(stack) / sizeof(stack[0])));
	if (count == 0) {
		clearFrame();
		return;
	}

	for (uint8_t i = 0; i < count; ++i) {
		renderLayer(stack[i].id, nowMs);
		if (stack[i].id == LayerId::CriticalAlert) {
			break;
		}
	}
}

uint8_t LedEngine::buildLayerStack(uint32_t nowMs, LayerEntry* outStack, uint8_t maxEntries) const {
	(void)nowMs;
	if (outStack == nullptr || maxEntries == 0) {
		return 0;
	}

	uint8_t count = 0;
	auto pushLayer = [&](LayerId id, uint8_t priority) {
		if (count >= maxEntries) {
			return;
		}
		outStack[count++] = LayerEntry{id, priority};
	};

	pushLayer(LayerId::WeatherBase, 10);
	if (idle_.active) {
		pushLayer(LayerId::Idle, 20);
	}
	if (progress_.active) {
		pushLayer(LayerId::Progress, 30);
	}
	if (transition_.active) {
		pushLayer(LayerId::Transition, 40);
	}
	if (touch_.active) {
		pushLayer(LayerId::Touch, 50);
	}
	pushLayer(LayerId::System, 60);
	if (boot_.active) {
		pushLayer(LayerId::Boot, 65);
	}
	if (alert_.active) {
		if (alert_.level == AlertLevel::Critical) {
			pushLayer(LayerId::CriticalAlert, 255);
		} else {
			pushLayer(LayerId::Alert, 70);
		}
	}

	for (uint8_t i = 1; i < count; ++i) {
		LayerEntry key = outStack[i];
		int8_t j = static_cast<int8_t>(i) - 1;
		while (j >= 0 && outStack[j].priority > key.priority) {
			outStack[j + 1] = outStack[j];
			--j;
		}
		outStack[j + 1] = key;
	}

	return count;
}

void LedEngine::renderLayer(LayerId id, uint32_t nowMs) {
	switch (id) {
		case LayerId::WeatherBase:
			renderWeatherBase(nowMs);
			return;
		case LayerId::Idle:
			renderIdleLayer(nowMs);
			return;
		case LayerId::Progress:
			renderProgressLayer(nowMs);
			return;
		case LayerId::Transition:
			renderTransitionLayer(nowMs);
			return;
		case LayerId::Touch:
			renderTouchLayer(nowMs);
			return;
		case LayerId::System:
			renderSystemLayer(nowMs);
			return;
		case LayerId::Boot:
			renderBootLayer(nowMs);
			return;
		case LayerId::Alert:
			renderAlertLayer(nowMs);
			return;
		case LayerId::CriticalAlert:
			renderCriticalAlert(nowMs);
			return;
	}
}

void LedEngine::emitEvent(const char* eventName, const String& details) const {
	if (eventCallback_ == nullptr || eventName == nullptr) {
		return;
	}
	eventCallback_(eventUserContext_, eventName, details);
}

void LedEngine::pushToHardware() {
	if (!ready_ || ledCount_ == 0) {
		return;
	}

	M5.Led.setBrightness(brightness_);
	for (size_t i = 0; i < ledCount_; ++i) {
		if (frame_[i].r == lastPushed_[i].r && frame_[i].g == lastPushed_[i].g && frame_[i].b == lastPushed_[i].b) {
			continue;
		}

		M5.Led.setColor(i, frame_[i]);
		lastPushed_[i] = frame_[i];
	}
	M5.Led.display();
}

LedEngine::MoodPalette LedEngine::chooseMoodPalette(const WeatherData& data) const {
	MoodPalette palette;
	palette.primary = patterns::color(24, 48, 96);
	palette.secondary = patterns::color(6, 14, 28);
	palette.accent = patterns::color(110, 180, 255);
	palette.daylight = data.current.isDaylight;

	const String summary = data.current.summary;
	const int icon = data.current.icon;

	if (SummaryContains(summary, "thunder") || icon == 15 || icon == 16 || icon == 17) {
		palette.primary = patterns::color(52, 20, 88);
		palette.secondary = patterns::color(9, 6, 24);
		palette.accent = patterns::color(255, 208, 72);
	} else if (SummaryContains(summary, "snow") || icon == 19 || icon == 20 || icon == 21 || icon == 22 || icon == 23 || icon == 24 || icon == 25 || icon == 29) {
		palette.primary = patterns::color(176, 220, 255);
		palette.secondary = patterns::color(28, 52, 82);
		palette.accent = patterns::color(255, 255, 255);
	} else if (SummaryContains(summary, "rain") || SummaryContains(summary, "shower") || icon == 12 || icon == 13 || icon == 14 || icon == 18 || icon == 26 || icon == 39 || icon == 40) {
		palette.primary = patterns::color(24, 84, 170);
		palette.secondary = patterns::color(8, 22, 48);
		palette.accent = patterns::color(92, 188, 255);
	} else if (SummaryContains(summary, "fog") || SummaryContains(summary, "mist") || icon == 11) {
		palette.primary = patterns::color(112, 126, 144);
		palette.secondary = patterns::color(30, 36, 46);
		palette.accent = patterns::color(190, 210, 230);
	} else if (SummaryContains(summary, "cloud") || icon == 6 || icon == 7 || icon == 8) {
		palette.primary = patterns::color(104, 136, 168);
		palette.secondary = patterns::color(18, 26, 40);
		palette.accent = patterns::color(184, 214, 244);
	} else if (SummaryContains(summary, "clear") || SummaryContains(summary, "sun") || icon == 1 || icon == 2 || icon == 3 || icon == 4 || icon == 5) {
		palette.primary = data.current.isDaylight ? patterns::color(255, 176, 62) : patterns::color(34, 72, 132);
		palette.secondary = data.current.isDaylight ? patterns::color(34, 20, 6) : patterns::color(5, 8, 20);
		palette.accent = data.current.isDaylight ? patterns::color(255, 232, 140) : patterns::color(148, 192, 255);
	}

	if (!data.current.isDaylight) {
		palette.primary = patterns::blend(palette.primary, patterns::color(18, 28, 84), 0.35f);
		palette.secondary = patterns::blend(palette.secondary, patterns::color(2, 4, 12), 0.45f);
	}

	return palette;
}

LedEngine::IdleMode LedEngine::resolveIdleMode(uint32_t nowMs) const {
	(void)nowMs;
	if (!idle_.active) {
		return IdleMode::Off;
	}

	if (idle_.requestedMode != IdleMode::Auto) {
		return idle_.requestedMode;
	}

	time_t now = time(nullptr);
	struct tm localTime;
	memset(&localTime, 0, sizeof(localTime));
	localtime_r(&now, &localTime);
	const int hour = localTime.tm_hour;
	if (hour >= 5 && hour <= 8) {
		return IdleMode::Sunrise;
	}
	if (hour >= 17 && hour <= 20) {
		return IdleMode::Sunset;
	}
	if (mood_.daylight) {
		return IdleMode::WeatherGlow;
	}
	return IdleMode::Breathing;
}

RGBColor LedEngine::pageColor(uint8_t page) const {
	switch (page) {
		case 0:
			return patterns::color(90, 200, 255);
		case 1:
			return patterns::color(60, 255, 146);
		case 2:
			return patterns::color(72, 146, 255);
		case 3:
			return patterns::color(255, 102, 74);
		case 4:
			return patterns::color(72, 232, 255);
		default:
			return patterns::color(255, 255, 255);
	}
}

void LedEngine::renderWeatherBase(uint32_t nowMs) {
	const float drift = patterns::triangleWave((static_cast<float>(nowMs % 6000U) / 6000.0f) + 0.12f);
	patterns::fillGradient(frame_, ledCount_, mood_.secondary, mood_.primary, drift * 0.25f);

	const float accentPos = (ledCount_ <= 1)
															? 0.0f
															: (static_cast<float>(ledCount_ - 1) * patterns::triangleWave(static_cast<float>(nowMs % 5000U) / 5000.0f));
	patterns::addSweep(frame_, ledCount_, accentPos, 2.3f, mood_.accent, 0.45f);
}

void LedEngine::renderIdleLayer(uint32_t nowMs) {
	const IdleMode mode = resolveIdleMode(nowMs);
	if (mode == IdleMode::Off) {
		return;
	}

	if (mode == IdleMode::Breathing || mode == IdleMode::CyanBreathing) {
		const float amount = patterns::pulseWave(nowMs, 0.09f, 0.18f, 0.42f);
		const RGBColor breathColor = (mode == IdleMode::CyanBreathing) ? patterns::color(72, 232, 255) : mood_.accent;
		for (size_t i = 0; i < ledCount_; ++i) {
			frame_[i] = patterns::blend(frame_[i], patterns::scale(breathColor, amount), 0.32f);
		}
		return;
	}

	if (mode == IdleMode::Sunrise || mode == IdleMode::Sunset) {
		const RGBColor warmA = (mode == IdleMode::Sunrise) ? patterns::color(255, 140, 54) : patterns::color(255, 94, 36);
		const RGBColor warmB = (mode == IdleMode::Sunrise) ? patterns::color(255, 214, 112) : patterns::color(224, 84, 54);
		const float orbit = static_cast<float>(nowMs % 7000U) / 7000.0f;
		const float center = orbit * static_cast<float>(ledCount_ == 0 ? 0 : (ledCount_ - 1));
		patterns::addSweep(frame_, ledCount_, center, 2.8f, warmA, 0.50f);
		patterns::addSweep(frame_, ledCount_, center - 1.4f, 3.5f, warmB, 0.30f);
		return;
	}

	const float shimmer = 0.16f + (0.16f * sinf(static_cast<float>(nowMs) * 0.0042f));
	for (size_t i = 0; i < ledCount_; ++i) {
		const float offset = static_cast<float>(i) / static_cast<float>(ledCount_ == 0 ? 1 : ledCount_);
		frame_[i] = patterns::add(frame_[i], patterns::scale(mood_.accent, shimmer + (0.08f * sinf((offset * 4.0f) + (static_cast<float>(nowMs) * 0.003f)))));
	}
}

void LedEngine::renderProgressLayer(uint32_t nowMs) {
	if (!progress_.active) {
		return;
	}

	const float litCount = (static_cast<float>(progress_.percent) / 100.0f) * static_cast<float>(ledCount_);
	const float head = litCount + (0.35f * sinf(static_cast<float>(nowMs) * 0.018f));
	for (size_t i = 0; i < ledCount_; ++i) {
		const float position = static_cast<float>(i) + 0.5f;
		if (position <= litCount) {
			frame_[i] = patterns::blend(frame_[i], patterns::color(16, 132, 255), 0.72f);
		}
	}
	patterns::addSweep(frame_, ledCount_, head, 1.25f, patterns::color(190, 245, 255), 1.0f);
}

void LedEngine::renderTransitionLayer(uint32_t nowMs) {
	if (!transition_.active || transition_.durationMs == 0) {
		return;
	}

	const float t = patterns::clamp01(static_cast<float>(nowMs - transition_.startedAtMs) / static_cast<float>(transition_.durationMs));
	const float center = transition_.forward
													 ? patterns::easeInOut(t) * static_cast<float>(ledCount_ == 0 ? 0 : ledCount_ - 1)
													 : (1.0f - patterns::easeInOut(t)) * static_cast<float>(ledCount_ == 0 ? 0 : ledCount_ - 1);
	const RGBColor from = pageColor(transition_.fromPage);
	const RGBColor to = pageColor(transition_.toPage);
	patterns::addSweep(frame_, ledCount_, center, 2.1f, from, 0.55f * (1.0f - t));
	patterns::addSweep(frame_, ledCount_, center, 1.2f, to, 0.9f);
}

void LedEngine::renderTouchLayer(uint32_t nowMs) {
	if (!touch_.active || touch_.durationMs == 0) {
		return;
	}

	const float t = patterns::clamp01(static_cast<float>(nowMs - touch_.startedAtMs) / static_cast<float>(touch_.durationMs));
	const float fade = 1.0f - patterns::easeInOut(t);

	if (touch_.kind == TouchKind::Tap) {
		const float center = static_cast<float>(ledCount_ == 0 ? 0 : ledCount_ - 1) * 0.5f;
		patterns::addSweep(frame_, ledCount_, center, 3.3f * (0.35f + t), patterns::color(255, 255, 255), fade);
		return;
	}

	if (touch_.kind == TouchKind::LongPress) {
		const float pulse = 0.45f + (0.4f * sinf(t * 12.566f));
		for (size_t i = 0; i < ledCount_; ++i) {
			frame_[i] = patterns::add(frame_[i], patterns::scale(patterns::color(255, 182, 52), pulse * fade));
		}
		return;
	}

	const bool leftToRight = (touch_.kind == TouchKind::SwipeRight || touch_.kind == TouchKind::SwipeDown);
	const float start = leftToRight ? 0.0f : static_cast<float>(ledCount_ == 0 ? 0 : ledCount_ - 1);
	const float end = leftToRight ? static_cast<float>(ledCount_ == 0 ? 0 : ledCount_ - 1) : 0.0f;
	const float center = start + ((end - start) * patterns::easeInOut(t));
	const RGBColor swipeColor = (touch_.kind == TouchKind::SwipeUp || touch_.kind == TouchKind::SwipeDown)
																			? patterns::color(180, 122, 255)
																			: patterns::color(92, 255, 208);
	patterns::addSweep(frame_, ledCount_, center, 1.7f, swipeColor, fade);
}

void LedEngine::renderSystemLayer(uint32_t nowMs) {
	if (system_.booting) {
		const float center = (static_cast<float>(nowMs % 1800U) / 1800.0f) * static_cast<float>(ledCount_ == 0 ? 0 : ledCount_ - 1);
		patterns::addSweep(frame_, ledCount_, center, 1.8f, patterns::color(255, 168, 56), 1.0f);
		patterns::addSweep(frame_, ledCount_, center - 1.4f, 2.4f, patterns::color(255, 84, 32), 0.45f);
		return;
	}

	if (!system_.wifiConnected) {
		const float blink = patterns::pulseWave(nowMs, 0.22f, 0.15f, 1.0f);
		if (ledCount_ > 0) {
			frame_[0] = patterns::add(frame_[0], patterns::scale(patterns::color(255, 54, 54), blink));
			frame_[ledCount_ - 1] = patterns::add(frame_[ledCount_ - 1], patterns::scale(patterns::color(255, 54, 54), blink));
		}
		if (ledCount_ > 2) {
			frame_[1] = patterns::add(frame_[1], patterns::scale(patterns::color(100, 20, 20), blink * 0.55f));
			frame_[ledCount_ - 2] = patterns::add(frame_[ledCount_ - 2], patterns::scale(patterns::color(100, 20, 20), blink * 0.55f));
		}
		return;
	}

	if ((nowMs - system_.changedAtMs) < 1400U) {
		const float confirm = 1.0f - patterns::clamp01(static_cast<float>(nowMs - system_.changedAtMs) / 1400.0f);
		for (size_t i = 0; i < ledCount_; ++i) {
			frame_[i] = patterns::add(frame_[i], patterns::scale(patterns::color(68, 255, 132), confirm * 0.45f));
		}
	}

	if (system_.warningActive && ledCount_ > 0) {
		const float blink = 0.3f + (0.45f * patterns::triangleWave(static_cast<float>(nowMs % 1100U) / 1100.0f));
		frame_[0] = patterns::add(frame_[0], patterns::scale(patterns::color(255, 180, 48), blink));
		frame_[ledCount_ - 1] = patterns::add(frame_[ledCount_ - 1], patterns::scale(patterns::color(255, 180, 48), blink));
	}
}

void LedEngine::renderAlertLayer(uint32_t nowMs) {
	if (!alert_.active || alert_.level == AlertLevel::Critical) {
		return;
	}

	const float blink = 0.35f + (0.55f * patterns::triangleWave(static_cast<float>(nowMs % 900U) / 900.0f));
	RGBColor color = patterns::color(120, 180, 255);
	if (alert_.level == AlertLevel::Warning) {
		color = patterns::color(255, 136, 42);
	}

	for (size_t i = 0; i < ledCount_; ++i) {
		if ((i % 2U) == 0U) {
			frame_[i] = patterns::add(frame_[i], patterns::scale(color, blink));
		}
	}
}

void LedEngine::renderBootLayer(uint32_t nowMs) {
	if (!boot_.active) {
		return;
	}

	const float phase = boot_.lockIn ? 0.5f : boot_.rotationPhase;
	const float center = phase * static_cast<float>(ledCount_ <= 1 ? 0 : ledCount_ - 1);
	const float baseIntensity = boot_.fadingOut ? (boot_.intensity * 0.55f) : boot_.intensity;
	const RGBColor orbitColor = boot_.lockIn ? patterns::color(196, 230, 255) : patterns::color(86, 182, 255);
	const RGBColor trailColor = boot_.lockIn ? patterns::color(86, 142, 255) : patterns::color(24, 84, 200);

	for (size_t i = 0; i < ledCount_; ++i) {
		frame_[i] = patterns::blend(frame_[i], patterns::color(2, 12, 26), 0.22f * baseIntensity);
	}

	patterns::addSweep(frame_, ledCount_, center, boot_.lockIn ? 2.8f : 1.6f, orbitColor, 0.95f * baseIntensity);
	patterns::addSweep(frame_, ledCount_, center - 1.2f, boot_.lockIn ? 3.8f : 2.6f, trailColor, 0.55f * baseIntensity);

	const float counterCenter = static_cast<float>(ledCount_ <= 1 ? 0 : ledCount_ - 1) - center;
	patterns::addSweep(frame_, ledCount_, counterCenter, 2.2f, patterns::color(24, 210, 255), 0.20f * baseIntensity);

	if (boot_.lockIn) {
		const float pulse = 0.32f + (0.22f * patterns::pulseWave(nowMs, 0.45f, 0.0f, 1.0f));
		for (size_t i = 0; i < ledCount_; ++i) {
			frame_[i] = patterns::add(frame_[i], patterns::scale(patterns::color(255, 255, 255), pulse * baseIntensity));
		}
	}
}

void LedEngine::renderCriticalAlert(uint32_t nowMs) {
	clearFrame(patterns::color(12, 0, 0));
	const float blink = patterns::triangleWave(static_cast<float>(nowMs % 700U) / 700.0f);
	const RGBColor warning = patterns::blend(patterns::color(96, 0, 0), patterns::color(255, 24, 0), blink);
	const RGBColor highlight = patterns::color(255, 180, 64);
	for (size_t i = 0; i < ledCount_; ++i) {
		frame_[i] = warning;
	}
	const float center = (ledCount_ <= 1) ? 0.0f : (static_cast<float>(ledCount_ - 1) * blink);
	patterns::addSweep(frame_, ledCount_, center, 1.4f, highlight, 1.0f);
}

void LedEngine::clearFrame(const RGBColor& value) {
	for (size_t i = 0; i < kMaxLeds; ++i) {
		frame_[i] = value;
	}
}

void LedEngine::expireTransientStates(uint32_t nowMs) {
	if (transition_.active && transition_.durationMs > 0 && (nowMs - transition_.startedAtMs) >= transition_.durationMs) {
		transition_.active = false;
	}

	if (touch_.active && touch_.durationMs > 0 && (nowMs - touch_.startedAtMs) >= touch_.durationMs) {
		touch_.active = false;
	}

	if (progress_.active && (nowMs - progress_.updatedAtMs) > 1200U && progress_.percent >= 100U) {
		progress_.active = false;
	}

	if (boot_.active && (nowMs - boot_.updatedAtMs) > 200U) {
		boot_.active = false;
	}

	if (alert_.active && alert_.durationMs > 0 && (nowMs - alert_.startedAtMs) >= alert_.durationMs) {
		clearAlert();
	}
}

}  // namespace led
