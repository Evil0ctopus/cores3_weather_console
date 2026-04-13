#pragma once

#include <lvgl.h>

#include "ui_theme.h"

namespace ui {

class BootAnimation {
 public:
	BootAnimation();
	~BootAnimation();

	lv_obj_t* begin(ThemeId themeId);
	void setTheme(ThemeId themeId);
	ThemeId themeId() const;
	bool update();
	bool isFinished() const;

	float ledPhase() const;
	float ledIntensity() const;
	bool ledLockIn() const;
	bool ledFadingOut() const;

 private:
	static constexpr uint16_t kCanvasSize = 180;
	static constexpr uint32_t kFrameIntervalMs = 20;
	static constexpr uint32_t kBackgroundFadeMs = 650;
	static constexpr uint32_t kGlobeFadeInMs = 900;
	static constexpr uint32_t kSpinStartMs = 320;
	static constexpr uint32_t kSlowdownStartMs = 2700;
	static constexpr uint32_t kFadeOutStartMs = 3600;
	static constexpr uint32_t kDurationMs = 4300;

	struct ProjectedPoint {
		int16_t x = 0;
		int16_t y = 0;
		float z = 0.0f;
		bool valid = false;
	};

	void ensureTheme(ThemeId themeId);
	void applyTheme();
	void renderFrame(uint32_t elapsedMs);
	void drawGlobe(float angleRad, lv_opa_t frontOpa, lv_opa_t backOpa);
	void drawLatitude(float latitudeRad, float angleRad, lv_opa_t frontOpa, lv_opa_t backOpa);
	void drawLongitude(float longitudeRad, float angleRad, lv_opa_t frontOpa, lv_opa_t backOpa);
	ProjectedPoint projectPoint(float x, float y, float z, float angleRad, float radius) const;
	void drawSegment(const ProjectedPoint& a, const ProjectedPoint& b, lv_opa_t frontOpa, lv_opa_t backOpa);
	float currentAngleRad(uint32_t elapsedMs) const;
	float normalizedTime(uint32_t elapsedMs, uint32_t startMs, uint32_t durationMs) const;
	lv_opa_t scaledOpacity(float amount, uint8_t maxOpacity) const;

	ThemeManager theme_;
	lv_obj_t* screen_ = nullptr;
	lv_obj_t* background_ = nullptr;
	lv_obj_t* canvas_ = nullptr;
	lv_obj_t* title_ = nullptr;
	lv_obj_t* subtitle_ = nullptr;
	lv_color_t* canvasBuffer_ = nullptr;
	uint32_t startedAtMs_ = 0;
	uint32_t lastFrameMs_ = 0;
	float ledPhase_ = 0.0f;
	float ledIntensity_ = 0.0f;
	bool ledLockIn_ = false;
	bool ledFadingOut_ = false;
	bool finished_ = false;
	bool started_ = false;
};

}  // namespace ui