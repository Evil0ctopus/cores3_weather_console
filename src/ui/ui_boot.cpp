#include "ui_boot.h"

#include <Arduino.h>

#include <math.h>
#include <stdlib.h>

namespace ui {
namespace {

const float kPi = 3.14159265f;
const float kTwoPi = kPi * 2.0f;

float Clamp01(float value) {
	if (value < 0.0f) {
		return 0.0f;
	}
	if (value > 1.0f) {
		return 1.0f;
	}
	return value;
}

float EaseInOut(float t) {
	t = Clamp01(t);
	return t * t * (3.0f - (2.0f * t));
}

float EaseOutCubic(float t) {
	t = Clamp01(t);
	const float inv = 1.0f - t;
	return 1.0f - (inv * inv * inv);
}

}  // namespace

BootAnimation::BootAnimation() = default;

BootAnimation::~BootAnimation() {
	if (canvasBuffer_ != nullptr) {
		free(canvasBuffer_);
		canvasBuffer_ = nullptr;
	}
}

lv_obj_t* BootAnimation::begin(ThemeId themeId) {
	ensureTheme(themeId);
	startedAtMs_ = 0;
	lastFrameMs_ = 0;
	ledPhase_ = 0.0f;
	ledIntensity_ = 0.0f;
	ledLockIn_ = false;
	ledFadingOut_ = false;
	finished_ = false;
	started_ = true;

	if (canvasBuffer_ == nullptr) {
		canvasBuffer_ = static_cast<lv_color_t*>(malloc(sizeof(lv_color_t) * kCanvasSize * kCanvasSize));
	}

	screen_ = lv_obj_create(nullptr);
	lv_obj_add_style(screen_, theme_.screenStyle(), LV_PART_MAIN);
	lv_obj_set_style_border_width(screen_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(screen_, 0, LV_PART_MAIN);
	lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);

	background_ = lv_obj_create(screen_);
	lv_obj_set_size(background_, lv_pct(100), lv_pct(100));
	lv_obj_center(background_);
	lv_obj_set_style_radius(background_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(background_, 0, LV_PART_MAIN);
	lv_obj_set_style_bg_color(background_, lv_color_black(), LV_PART_MAIN);
	lv_obj_set_style_bg_grad_color(background_, lv_color_black(), LV_PART_MAIN);
	lv_obj_set_style_bg_grad_dir(background_, LV_GRAD_DIR_VER, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(background_, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_clear_flag(background_, LV_OBJ_FLAG_SCROLLABLE);

	canvas_ = lv_canvas_create(screen_);
	lv_canvas_set_buffer(canvas_, canvasBuffer_, kCanvasSize, kCanvasSize, LV_IMG_CF_TRUE_COLOR);
	lv_obj_set_size(canvas_, kCanvasSize, kCanvasSize);
	lv_obj_align(canvas_, LV_ALIGN_CENTER, 0, -10);
	lv_obj_set_style_border_width(canvas_, 0, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(canvas_, LV_OPA_TRANSP, LV_PART_MAIN);

	title_ = lv_label_create(screen_);
	lv_obj_add_style(title_, theme_.titleStyle(), LV_PART_MAIN);
	lv_obj_set_style_text_font(title_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_transform_zoom(title_, 175, LV_PART_MAIN);
	lv_obj_set_style_text_opa(title_, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_label_set_text(title_, "Flic Weather");
	lv_obj_align(title_, LV_ALIGN_CENTER, 0, 108);

	subtitle_ = lv_label_create(screen_);
	lv_obj_add_style(subtitle_, theme_.captionStyle(), LV_PART_MAIN);
	lv_obj_set_style_text_font(subtitle_, &lv_font_montserrat_14, LV_PART_MAIN);
	lv_obj_set_style_text_opa(subtitle_, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_label_set_text(subtitle_, "Aligning atmosphere and motion");
	lv_obj_align(subtitle_, LV_ALIGN_CENTER, 0, 136);

	renderFrame(0);
	return screen_;
}

void BootAnimation::setTheme(ThemeId themeId) {
	ensureTheme(themeId);
	if (!started_) {
		return;
	}
	applyTheme();
	renderFrame(millis() - startedAtMs_);
}

ThemeId BootAnimation::themeId() const {
	return theme_.themeId();
}

bool BootAnimation::update() {
	if (!started_ || finished_) {
		return finished_;
	}

	const uint32_t nowMs = millis();
	if (startedAtMs_ == 0) {
		startedAtMs_ = nowMs;
		lastFrameMs_ = 0;
	}
	if (lastFrameMs_ > 0 && (nowMs - lastFrameMs_) < kFrameIntervalMs) {
		return false;
	}
	lastFrameMs_ = nowMs;

	const uint32_t elapsedMs = nowMs - startedAtMs_;
	renderFrame(elapsedMs);
	if (elapsedMs >= kDurationMs) {
		finished_ = true;
	}
	return finished_;
}

bool BootAnimation::isFinished() const {
	return finished_;
}

float BootAnimation::ledPhase() const {
	return ledPhase_;
}

float BootAnimation::ledIntensity() const {
	return ledIntensity_;
}

bool BootAnimation::ledLockIn() const {
	return ledLockIn_;
}

bool BootAnimation::ledFadingOut() const {
	return ledFadingOut_;
}

void BootAnimation::ensureTheme(ThemeId themeId) {
	if (screen_ == nullptr) {
		theme_.begin(themeId);
	} else {
		theme_.setTheme(themeId);
	}
}

void BootAnimation::applyTheme() {
	if (screen_ == nullptr) {
		return;
	}
	lv_obj_add_style(screen_, theme_.screenStyle(), LV_PART_MAIN);
	lv_obj_set_style_bg_color(background_, lv_color_black(), LV_PART_MAIN);
	lv_obj_set_style_bg_grad_color(background_, lv_color_black(), LV_PART_MAIN);
	lv_obj_add_style(title_, theme_.titleStyle(), LV_PART_MAIN);
	lv_obj_add_style(subtitle_, theme_.captionStyle(), LV_PART_MAIN);
}

void BootAnimation::renderFrame(uint32_t elapsedMs) {
	const float backgroundFade = EaseInOut(normalizedTime(elapsedMs, 0, kBackgroundFadeMs));
	const float globeFadeIn = EaseInOut(normalizedTime(elapsedMs, 220, kGlobeFadeInMs));
	const float fadeOut = 1.0f - EaseInOut(normalizedTime(elapsedMs, kFadeOutStartMs, kDurationMs - kFadeOutStartMs));
	const float titleFade = globeFadeIn * fadeOut;
	const float globeFade = globeFadeIn * fadeOut;
	const float angleRad = currentAngleRad(elapsedMs);
	const float angleTurns = angleRad / kTwoPi;
	const float wrappedTurns = angleTurns - floorf(angleTurns);
	const float lockWindow = normalizedTime(elapsedMs, kSlowdownStartMs, kFadeOutStartMs - kSlowdownStartMs);

	ledPhase_ = wrappedTurns;
	ledIntensity_ = globeFade;
	ledLockIn_ = (elapsedMs >= kSlowdownStartMs) && (elapsedMs < kFadeOutStartMs);
	ledFadingOut_ = elapsedMs >= kFadeOutStartMs;

	lv_obj_set_style_bg_opa(background_, scaledOpacity(backgroundFade * fadeOut, 255), LV_PART_MAIN);
	lv_obj_set_style_text_opa(title_, scaledOpacity(titleFade, 255), LV_PART_MAIN);
	lv_obj_set_style_text_opa(subtitle_, scaledOpacity(titleFade * 0.86f, 220), LV_PART_MAIN);

	const lv_opa_t frontOpa = scaledOpacity(globeFade, 240);
	const lv_opa_t backOpa = scaledOpacity(globeFade * (0.20f + (0.18f * (1.0f - lockWindow))), 96);
	drawGlobe(angleRad, frontOpa, backOpa);
}

void BootAnimation::drawGlobe(float angleRad, lv_opa_t frontOpa, lv_opa_t backOpa) {
	if (canvas_ == nullptr || canvasBuffer_ == nullptr) {
		return;
	}

	lv_canvas_fill_bg(canvas_, lv_color_black(), LV_OPA_COVER);

	for (int latitudeIndex = -3; latitudeIndex <= 3; ++latitudeIndex) {
		if (latitudeIndex == 0) {
			continue;
		}
		drawLatitude(static_cast<float>(latitudeIndex) * 0.33f, angleRad, frontOpa, backOpa);
	}
	drawLatitude(0.0f, angleRad, frontOpa, backOpa);

	for (int longitudeIndex = 0; longitudeIndex < 8; ++longitudeIndex) {
		const float longitude = (kTwoPi * static_cast<float>(longitudeIndex)) / 8.0f;
		drawLongitude(longitude, angleRad, frontOpa, backOpa);
	}
}

void BootAnimation::drawLatitude(float latitudeRad, float angleRad, lv_opa_t frontOpa, lv_opa_t backOpa) {
	ProjectedPoint previous;
	const float cosLatitude = cosf(latitudeRad);
	const float sinLatitude = sinf(latitudeRad);
	for (uint8_t step = 0; step <= 32; ++step) {
		const float theta = (kTwoPi * static_cast<float>(step)) / 32.0f;
		const float x = cosLatitude * cosf(theta);
		const float y = sinLatitude;
		const float z = cosLatitude * sinf(theta);
		const ProjectedPoint current = projectPoint(x, y, z, angleRad, 66.0f);
		if (step > 0) {
			drawSegment(previous, current, frontOpa, backOpa);
		}
		previous = current;
	}
}

void BootAnimation::drawLongitude(float longitudeRad, float angleRad, lv_opa_t frontOpa, lv_opa_t backOpa) {
	ProjectedPoint previous;
	for (uint8_t step = 0; step <= 24; ++step) {
		const float latitude = (-kPi * 0.5f) + (kPi * static_cast<float>(step) / 24.0f);
		const float radius = cosf(latitude);
		const float x = radius * cosf(longitudeRad);
		const float y = sinf(latitude);
		const float z = radius * sinf(longitudeRad);
		const ProjectedPoint current = projectPoint(x, y, z, angleRad, 66.0f);
		if (step > 0) {
			drawSegment(previous, current, frontOpa, backOpa);
		}
		previous = current;
	}
}

BootAnimation::ProjectedPoint BootAnimation::projectPoint(float x, float y, float z, float angleRad, float radius) const {
	ProjectedPoint point;
	const float sinAngle = sinf(angleRad);
	const float cosAngle = cosf(angleRad);
	const float rotatedX = (x * cosAngle) + (z * sinAngle);
	const float rotatedZ = (-x * sinAngle) + (z * cosAngle);
	const float perspective = 0.84f + (0.20f * ((rotatedZ + 1.0f) * 0.5f));
	point.x = static_cast<int16_t>((kCanvasSize / 2) + (rotatedX * radius * perspective));
	point.y = static_cast<int16_t>((kCanvasSize / 2) + (y * radius * perspective));
	point.z = rotatedZ;
	point.valid = true;
	return point;
}

void BootAnimation::drawSegment(const ProjectedPoint& a, const ProjectedPoint& b, lv_opa_t frontOpa, lv_opa_t backOpa) {
	if (!a.valid || !b.valid || canvas_ == nullptr) {
		return;
	}

	lv_draw_line_dsc_t dsc;
	lv_draw_line_dsc_init(&dsc);
	dsc.width = 1;
	dsc.round_start = 1;
	dsc.round_end = 1;
	const float averageZ = (a.z + b.z) * 0.5f;
	if (averageZ >= -0.04f) {
		dsc.color = theme_.palette().accent;
		dsc.opa = frontOpa;
	} else {
		dsc.color = theme_.palette().textSecondary;
		dsc.opa = backOpa;
	}

	lv_point_t points[2];
	points[0].x = a.x;
	points[0].y = a.y;
	points[1].x = b.x;
	points[1].y = b.y;
	lv_canvas_draw_line(canvas_, points, 2, &dsc);
}

float BootAnimation::currentAngleRad(uint32_t elapsedMs) const {
	const float slowdownProgress = normalizedTime(elapsedMs, kSlowdownStartMs, kFadeOutStartMs - kSlowdownStartMs);
	const float startTurns = 1.85f;
	if (elapsedMs < kSlowdownStartMs) {
		const float spinProgress = EaseInOut(normalizedTime(elapsedMs, kSpinStartMs, kSlowdownStartMs - kSpinStartMs));
		return startTurns * kTwoPi * spinProgress;
	}
	const float startAngle = startTurns * kTwoPi;
	const float finalAngle = 2.0f * kTwoPi;
	return startAngle + ((finalAngle - startAngle) * EaseOutCubic(slowdownProgress));
}

float BootAnimation::normalizedTime(uint32_t elapsedMs, uint32_t startMs, uint32_t durationMs) const {
	if (elapsedMs <= startMs) {
		return 0.0f;
	}
	if (durationMs == 0) {
		return 1.0f;
	}
	return Clamp01(static_cast<float>(elapsedMs - startMs) / static_cast<float>(durationMs));
}

lv_opa_t BootAnimation::scaledOpacity(float amount, uint8_t maxOpacity) const {
	const float clamped = Clamp01(amount);
	return static_cast<lv_opa_t>(clamped * static_cast<float>(maxOpacity));
}

}  // namespace ui