#include "radar_engine.h"

#include <PNGdec.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <new>

namespace weather {
namespace {

const uint32_t kAnimationMinFps = 3;
const uint32_t kAnimationMaxFps = 10;
const uint32_t kDownloadPumpIntervalMs = 2;
const uint8_t kStormDetectFloor = 148;
const uint8_t kStormDetectPeakProminence = 22;
const uint8_t kStormDetectSampleStep = 3;
const uint8_t kStormDetectMinCellSpacing = 12;

struct PngDecodeContext {
	PNG* decoder = nullptr;
	uint8_t* buffer = nullptr;
	size_t strideBytes = 0;
	uint32_t backgroundColor = 0x00000000;
};

int decodePngLine(PNGDRAW* draw) {
	PngDecodeContext* context = static_cast<PngDecodeContext*>(draw->pUser);
	if (context == nullptr || context->decoder == nullptr || context->buffer == nullptr) {
		return 0;
	}
	uint16_t* row = reinterpret_cast<uint16_t*>(context->buffer + (static_cast<size_t>(draw->y) * context->strideBytes));
	context->decoder->getLineAsRGB565(draw, row, PNG_RGB565_LITTLE_ENDIAN, context->backgroundColor);
	return 1;
}

uint8_t clampByte(int value) {
	if (value < 0) {
		return 0;
	}
	if (value > 255) {
		return 255;
	}
	return static_cast<uint8_t>(value);
}

size_t bytesPerPixel(RadarFrameFormat format) {
	if (format == RadarFrameFormat::RawRgb565) {
		return 2;
	}
	if (format == RadarFrameFormat::RawArgb8888) {
		return 4;
	}
	return 0;
}

void readPixel(const uint8_t* data, size_t pixelIndex, RadarFrameFormat format, uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha) {
	if (format == RadarFrameFormat::RawRgb565) {
		const size_t offset = pixelIndex * 2;
		const uint16_t packed = static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
		red = static_cast<uint8_t>(((packed >> 11) & 0x1F) * 255 / 31);
		green = static_cast<uint8_t>(((packed >> 5) & 0x3F) * 255 / 63);
		blue = static_cast<uint8_t>((packed & 0x1F) * 255 / 31);
		alpha = 255;
		return;
	}
	const size_t offset = pixelIndex * 4;
	blue = data[offset + 0];
	green = data[offset + 1];
	red = data[offset + 2];
	alpha = data[offset + 3];
}

void writePixel(uint8_t* data, size_t pixelIndex, RadarFrameFormat format, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) {
	if (format == RadarFrameFormat::RawRgb565) {
		const uint16_t packed = static_cast<uint16_t>(((red * 31) / 255) << 11) |
								  static_cast<uint16_t>(((green * 63) / 255) << 5) |
								  static_cast<uint16_t>((blue * 31) / 255);
		const size_t offset = pixelIndex * 2;
		data[offset] = static_cast<uint8_t>(packed & 0xFF);
		data[offset + 1] = static_cast<uint8_t>((packed >> 8) & 0xFF);
		return;
	}
	const size_t offset = pixelIndex * 4;
	data[offset + 0] = blue;
	data[offset + 1] = green;
	data[offset + 2] = red;
	data[offset + 3] = alpha;
}

uint8_t luminanceOf(uint8_t red, uint8_t green, uint8_t blue) {
	return static_cast<uint8_t>((77U * red + 150U * green + 29U * blue) >> 8);
}

void applyReflectivityColor(ReflectivityMode mode, uint8_t& red, uint8_t& green, uint8_t& blue) {
	switch (mode) {
		case ReflectivityMode::CompositeReflectivity:
			red = clampByte((red * 112) / 100 + 10);
			green = clampByte((green * 105) / 100 + 6);
			blue = clampByte((blue * 92) / 100);
			break;
		case ReflectivityMode::EchoTop:
			red = clampByte((red * 115) / 100 + 8);
			green = clampByte((green * 92) / 100);
			blue = clampByte((blue * 118) / 100 + 12);
			break;
		case ReflectivityMode::Velocity:
			red = clampByte((red * 85) / 100);
			green = clampByte((green * 108) / 100 + 8);
			blue = clampByte((blue * 122) / 100 + 12);
			break;
		case ReflectivityMode::DifferentialPhase:
			red = clampByte((red * 110) / 100 + 6);
			green = clampByte((green * 118) / 100 + 10);
			blue = clampByte((blue * 95) / 100);
			break;
		case ReflectivityMode::Custom:
			red = clampByte((red * 108) / 100 + 4);
			green = clampByte((green * 102) / 100 + 2);
			blue = clampByte((blue * 112) / 100 + 10);
			break;
		case ReflectivityMode::BaseReflectivity:
		default:
			break;
	}
}

void blendToward(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t targetRed, uint8_t targetGreen, uint8_t targetBlue, uint8_t alpha) {
	red = static_cast<uint8_t>((static_cast<uint16_t>(red) * (255U - alpha) + static_cast<uint16_t>(targetRed) * alpha) / 255U);
	green = static_cast<uint8_t>((static_cast<uint16_t>(green) * (255U - alpha) + static_cast<uint16_t>(targetGreen) * alpha) / 255U);
	blue = static_cast<uint8_t>((static_cast<uint16_t>(blue) * (255U - alpha) + static_cast<uint16_t>(targetBlue) * alpha) / 255U);
}

const char* reflectivityModeName(ReflectivityMode mode) {
	switch (mode) {
		case ReflectivityMode::CompositeReflectivity:
			return "composite";
		case ReflectivityMode::EchoTop:
			return "echo-top";
		case ReflectivityMode::Velocity:
			return "velocity";
		case ReflectivityMode::DifferentialPhase:
			return "diff-phase";
		case ReflectivityMode::Custom:
			return "custom";
		case ReflectivityMode::BaseReflectivity:
		default:
			return "base";
	}
}

uint32_t clampFps(float fps) {
	if (fps < static_cast<float>(kAnimationMinFps)) {
		return kAnimationMinFps;
	}
	if (fps > static_cast<float>(kAnimationMaxFps)) {
		return kAnimationMaxFps;
	}
	return static_cast<uint32_t>(fps + 0.5f);
}

RadarFrameFormat inferFormat(const String& contentType, RadarFrameFormat expected) {
	String ct = contentType;
	ct.toLowerCase();
	if (ct.indexOf("image/png") >= 0) {
		return RadarFrameFormat::EncodedPng;
	}
	if (ct.indexOf("image/jpeg") >= 0 || ct.indexOf("image/jpg") >= 0) {
		return RadarFrameFormat::EncodedJpeg;
	}
	return expected;
}

}  // namespace

RadarEngine::RadarEngine() {
	resetAll();
}

RadarEngine::~RadarEngine() {
	if (workerTask_ != nullptr) {
		HttpJob* stop = nullptr;
		xQueueSend(requestQueue_, &stop, portMAX_DELAY);
		xSemaphoreTake(workerStopped_, portMAX_DELAY);
	}
	HttpJob* pending = nullptr;
	if (responseQueue_ != nullptr) {
		while (xQueueReceive(responseQueue_, &pending, 0) == pdTRUE) {
			delete pending;
		}
		vQueueDelete(responseQueue_);
	}
	if (requestQueue_ != nullptr) {
		vQueueDelete(requestQueue_);
	}
	if (workerStopped_ != nullptr) {
		vSemaphoreDelete(workerStopped_);
	}
	clearFrames();
	if (activeTempBuffer_ != nullptr) {
		free(activeTempBuffer_);
		activeTempBuffer_ = nullptr;
	}
	if (displayBuffer_ != nullptr) {
		free(displayBuffer_);
		displayBuffer_ = nullptr;
	}
}

void RadarEngine::begin() {
	ensureSpiffs();
	if (!initializeHttpWorker()) {
		transitionToError(RadarEngineError::OutOfMemory, "radar HTTP worker unavailable");
	}
	lastAnimationStepMs_ = millis();
}

RadarEngine::HttpJob::~HttpJob() {
	free(data);
	free(mapData);
}

bool RadarEngine::initializeHttpWorker() {
	if (workerTask_ != nullptr) {
		return true;
	}
	requestQueue_ = xQueueCreate(1, sizeof(HttpJob*));
	responseQueue_ = xQueueCreate(1, sizeof(HttpJob*));
	workerStopped_ = xSemaphoreCreateBinary();
	if (requestQueue_ != nullptr && responseQueue_ != nullptr && workerStopped_ != nullptr &&
			xTaskCreatePinnedToCore(httpWorkerTask, "radar-http", 8192, this, 1, &workerTask_, 0) == pdPASS) {
		return true;
	}
	if (requestQueue_ != nullptr) vQueueDelete(requestQueue_);
	if (responseQueue_ != nullptr) vQueueDelete(responseQueue_);
	if (workerStopped_ != nullptr) vSemaphoreDelete(workerStopped_);
	requestQueue_ = nullptr;
	responseQueue_ = nullptr;
	workerStopped_ = nullptr;
	workerTask_ = nullptr;
	return false;
}

void RadarEngine::httpWorkerTask(void* context) {
	static_cast<RadarEngine*>(context)->runHttpWorker();
}

void RadarEngine::runHttpWorker() {
	for (;;) {
		HttpJob* job = nullptr;
		if (xQueueReceive(requestQueue_, &job, portMAX_DELAY) != pdTRUE) {
			continue;
		}
		if (job == nullptr) {
			xSemaphoreGive(workerStopped_);
			vTaskDelete(nullptr);
			return;
		}
		if (fetchUrlToBuffer(job->url, job->config, job->data, job->length,
				job->contentType, job->error, job->message, job->httpStatus) &&
				job->config.baseMapUrl.length() > 0) {
			String mapContentType;
			RadarEngineError mapError = RadarEngineError::None;
			String mapMessage;
			int mapStatus = 0;
			if (!fetchUrlToBuffer(job->config.baseMapUrl, job->config, job->mapData,
					job->mapLength, mapContentType, mapError, mapMessage, mapStatus)) {
				Serial.printf("[RADAR] basemap unavailable: %s (http=%d); using overlay only\n",
					mapMessage.c_str(), mapStatus);
			}
		}
		if (job->error == RadarEngineError::None) {
			prepareDownloadedFrame(*job);
		}
		xQueueSend(responseQueue_, &job, portMAX_DELAY);
	}
}

void RadarEngine::reset() {
	resetAll();
	emitProgress("idle");
}

void RadarEngine::setProgressCallback(RadarProgressCallback callback, void* userContext) {
	progressCallback_ = callback;
	progressUserContext_ = userContext;
}

void RadarEngine::setAnimationFps(float fps) {
	animationFps_ = static_cast<float>(clampFps(fps));
}

float RadarEngine::animationFps() const {
	return animationFps_;
}

void RadarEngine::setVisualConfig(const RadarVisualConfig& config) {
	visualConfig_ = config;
	if (visualConfig_.interpolationSteps > 3) {
		visualConfig_.interpolationSteps = 3;
	}
	if (visualConfig_.smoothingPasses > 3) {
		visualConfig_.smoothingPasses = 3;
	}
	if (visualConfig_.enableFrameInterpolation && visualConfig_.interpolationSteps > 0 && visualConfig_.smoothingPasses > 1) {
		visualConfig_.smoothingPasses = 1;
	}
	invalidateDescriptors();
}

void RadarEngine::setReflectivityMode(ReflectivityMode mode) {
	config_.reflectivityMode = mode;
	invalidateDescriptors();
}

const RadarVisualConfig& RadarEngine::visualConfig() const {
	return visualConfig_;
}

void RadarEngine::setStormCells(const RadarStormCell* cells, size_t count) {
	stormCellCount_ = count > kMaxStormCells ? kMaxStormCells : count;
	for (size_t index = 0; index < kMaxStormCells; ++index) {
		stormCells_[index] = RadarStormCell();
	}
	for (size_t index = 0; index < stormCellCount_; ++index) {
		stormCells_[index] = cells[index];
	}
	invalidateDescriptors();
}

size_t RadarEngine::stormCellCount() const {
	return stormCellCount_;
}

ReflectivityMode RadarEngine::reflectivityMode() const {
	return config_.reflectivityMode;
}

const char* RadarEngine::reflectivityModeLabel() const {
	return reflectivityModeName(config_.reflectivityMode);
}

bool RadarEngine::isInterpolating() const {
	return interpolationStep_ > 0;
}

bool RadarEngine::hasDisplayEffects() const {
	return visualConfig_.enableAutoContrast || visualConfig_.smoothingPasses > 0 ||
			   visualConfig_.enableFrameInterpolation || visualConfig_.enableStormCellOverlays ||
			   config_.reflectivityMode != ReflectivityMode::BaseReflectivity;
}

bool RadarEngine::startDownload(const String* tileUrls,
																const uint32_t* frameEpochs,
																size_t frameCount,
																const RadarDownloadConfig& config) {
	if (tileUrls == nullptr || frameCount < kMinFrameCount || frameCount > kMaxFrameCount) {
		transitionToError(RadarEngineError::InvalidInput, "frameCount must be 1..12 with valid URL array");
		return false;
	}
	if (state_ != DownloadState::Idle && state_ != DownloadState::Complete && state_ != DownloadState::Error) {
		transitionToError(RadarEngineError::Busy, "download already in progress");
		return false;
	}
	if (WiFi.status() != WL_CONNECTED) {
		transitionToError(RadarEngineError::WifiDisconnected, "wifi not connected");
		return false;
	}

	resetAll();
	config_ = config;
	if (!initializeHttpWorker()) {
		transitionToError(RadarEngineError::OutOfMemory, "radar HTTP worker unavailable");
		return false;
	}
	frameCount_ = frameCount;
	animationFps_ = static_cast<float>(clampFps(animationFps_));

	for (size_t i = 0; i < frameCount_; ++i) {
		frames_[i].url = tileUrls[i];
		frames_[i].info.reflectivityMode = config_.reflectivityMode;
		frames_[i].info.epochTime = frameEpochs != nullptr ? frameEpochs[i] : 0;
		frames_[i].info.width = config_.expectedWidth;
		frames_[i].info.height = config_.expectedHeight;
		frames_[i].info.format = config_.expectedFormat;
	}

	if (config_.expectedWidth > 0 && config_.expectedHeight > 0) {
		const size_t displayBytesHint = static_cast<size_t>(config_.expectedWidth) * static_cast<size_t>(config_.expectedHeight) * 2U;
		if (!ensureDisplayBuffer(displayBytesHint)) {
			return false;
		}
	}

	downloadStarted_ = true;
	state_ = DownloadState::Connect;
	emitProgress("start");
	return true;
}

void RadarEngine::tick() {
	advanceAnimation();
	if (!downloadStarted_ && !requestInFlight_) {
		return;
	}

	const uint32_t now = millis();
	if (now - lastDownloadPumpMs_ < kDownloadPumpIntervalMs) {
		return;
	}
	lastDownloadPumpMs_ = now;
	pumpDownload();
}

bool RadarEngine::isDownloading() const {
	return state_ != DownloadState::Idle && state_ != DownloadState::Complete && state_ != DownloadState::Error;
}

bool RadarEngine::isAnimationReady() const {
	return completedFrameCount_ > 0;
}

RadarEngineError RadarEngine::lastError() const {
	return lastError_;
}

int RadarEngine::lastHttpStatus() const {
	return lastHttpStatus_;
}

String RadarEngine::lastErrorMessage() const {
	return lastErrorMessage_;
}

size_t RadarEngine::frameCount() const {
	return frameCount_;
}

size_t RadarEngine::completedFrameCount() const {
	return completedFrameCount_;
}

size_t RadarEngine::currentAnimationIndex() const {
	return currentAnimationIndex_;
}

uint32_t RadarEngine::displayRevision() const {
	return displayRevision_;
}

const RadarFrameInfo* RadarEngine::frameInfo(size_t index) const {
	if (index >= frameCount_) {
		return nullptr;
	}
	return &frames_[index].info;
}

const lv_img_dsc_t* RadarEngine::currentFrameAsLvglImage() {
	if (frameCount_ == 0) {
		return nullptr;
	}
	FrameSlot& slot = frames_[currentAnimationIndex_];
	if (!slot.info.valid) {
		return nullptr;
	}

	if ((slot.info.format == RadarFrameFormat::RawRgb565 || slot.info.format == RadarFrameFormat::RawArgb8888) && hasDisplayEffects()) {
		FrameSlot* next = nullptr;
		uint8_t blendStep = 0;
		uint8_t blendSteps = 0;
		if (visualConfig_.enableFrameInterpolation && completedFrameCount_ > 1 && interpolationStep_ > 0) {
			size_t nextIndex = currentAnimationIndex_ + 1;
			if (nextIndex >= completedFrameCount_) {
				nextIndex = 0;
			}
			next = &frames_[nextIndex];
			blendStep = interpolationStep_;
			blendSteps = visualConfig_.interpolationSteps;
		}
		if (!buildDisplayFrame(slot, next, blendStep, blendSteps)) {
			return nullptr;
		}
		return &displayDsc;
	}

	if (!slot.dscValid && !prepareLvglDescriptor(slot)) {
		return nullptr;
	}
	return &slot.dsc;
}

bool RadarEngine::getCurrentFrameRaw(const uint8_t*& data, size_t& length, RadarFrameFormat& format) {
	data = nullptr;
	length = 0;
	format = RadarFrameFormat::Unknown;
	if (frameCount_ == 0) {
		return false;
	}

	FrameSlot& slot = frames_[currentAnimationIndex_];
	if (!slot.info.valid) {
		return false;
	}

	if (slot.ramData == nullptr && slot.info.storedInSpiffs) {
		File f = SPIFFS.open(slot.info.spiffsPath, FILE_READ);
		if (!f) {
			transitionToError(RadarEngineError::IoError, "failed to reopen SPIFFS frame");
			return false;
		}
		size_t sz = static_cast<size_t>(f.size());
		uint8_t* buf = static_cast<uint8_t*>(malloc(sz));
		if (buf == nullptr) {
			f.close();
			transitionToError(RadarEngineError::OutOfMemory, "failed to allocate frame buffer");
			return false;
		}
		size_t readLen = f.read(buf, sz);
		f.close();
		if (readLen != sz) {
			free(buf);
			transitionToError(RadarEngineError::IoError, "failed to read full SPIFFS frame");
			return false;
		}
		slot.ramData = buf;
		slot.ramLength = sz;
	}

	if (slot.ramData == nullptr) {
		return false;
	}

	data = slot.ramData;
	length = slot.ramLength;
	format = slot.info.format;
	return true;
}

void RadarEngine::resetAll() {
	++downloadGeneration_;
	++displayRevision_;
	clearFrames();
	frameCount_ = 0;
	completedFrameCount_ = 0;
	currentDownloadIndex_ = 0;
	currentAnimationIndex_ = 0;
	activeFormat_ = RadarFrameFormat::Unknown;
	activeWidth_ = 0;
	activeHeight_ = 0;
	interpolationStep_ = 0;

	if (activeTempBuffer_ != nullptr) {
		free(activeTempBuffer_);
		activeTempBuffer_ = nullptr;
	}
	activeTempLength_ = 0;

	lastError_ = RadarEngineError::None;
	lastErrorMessage_ = "";
	lastHttpStatus_ = 0;
	state_ = DownloadState::Idle;
	downloadStarted_ = false;
}

void RadarEngine::clearFrame(FrameSlot& slot) {
	if (slot.info.valid) {
		lv_img_cache_invalidate_src(&slot.dsc);
	}
	if (slot.ramData != nullptr) {
		free(slot.ramData);
		slot.ramData = nullptr;
	}
	slot.ramLength = 0;
	slot.autoStormCellCount = 0;
	if (slot.info.storedInSpiffs && slot.info.spiffsPath.length() > 0 && spiffsReady_) {
		SPIFFS.remove(slot.info.spiffsPath);
	}
	slot.info = RadarFrameInfo();
	slot.url = "";
	memset(&slot.dsc, 0, sizeof(slot.dsc));
	slot.dscValid = false;
}

void RadarEngine::clearFrames() {
	for (size_t i = 0; i < kMaxFrameCount; ++i) {
		clearFrame(frames_[i]);
	}
}

void RadarEngine::transitionToError(RadarEngineError code, const String& message, int httpStatus) {
	lastError_ = code;
	lastErrorMessage_ = message;
	lastHttpStatus_ = httpStatus;
	state_ = DownloadState::Error;
	downloadStarted_ = false;
	Serial.printf("[RADAR] error=%d http=%d msg=%s\n", static_cast<int>(code), httpStatus, message.c_str());
	emitProgress("error");
}

void RadarEngine::emitProgress(const char* stage) {
	if (progressCallback_ == nullptr) {
		return;
	}
	RadarProgress p;
	p.totalFrames = frameCount_;
	p.completedFrames = completedFrameCount_;
	p.activeFrameIndex = currentDownloadIndex_;
	if (frameCount_ == 0) {
		p.percent = 0;
	} else {
		p.percent = static_cast<uint8_t>((completedFrameCount_ * 100U) / frameCount_);
	}
	p.stage = stage;
	progressCallback_(progressUserContext_, p);
}

bool RadarEngine::parseUrl(const String& url, UrlParts& out) {
	const int schemeEnd = url.indexOf("://");
	if (schemeEnd <= 0) {
		return false;
	}

	String scheme = url.substring(0, schemeEnd);
	scheme.toLowerCase();
	out.secure = (scheme == "https");
	out.port = out.secure ? 443 : 80;

	int hostStart = schemeEnd + 3;
	int pathStart = url.indexOf('/', hostStart);
	if (pathStart < 0) {
		pathStart = url.length();
		out.path = "/";
	} else {
		out.path = url.substring(pathStart);
	}

	String hostPort = url.substring(hostStart, pathStart);
	int colonPos = hostPort.indexOf(':');
	if (colonPos >= 0) {
		out.host = hostPort.substring(0, colonPos);
		out.port = static_cast<uint16_t>(hostPort.substring(colonPos + 1).toInt());
	} else {
		out.host = hostPort;
	}
	return out.host.length() > 0;
}

bool RadarEngine::fetchUrlToBuffer(const String& url, const RadarDownloadConfig& config,
		uint8_t*& outData, size_t& outLength, String& outContentType,
		RadarEngineError& error, String& message, int& httpStatus) {
	outData = nullptr;
	outLength = 0;
	outContentType = "";
	auto fail = [&](RadarEngineError code, const char* details) {
		free(outData);
		outData = nullptr;
		outLength = 0;
		error = code;
		message = details;
		return false;
	};

	UrlParts parts;
	if (!parseUrl(url, parts)) {
		return fail(RadarEngineError::ParseError, "invalid radar URL");
	}

	WiFiClient client;
	WiFiClientSecure secureClient;
	Client* transport = nullptr;
	const uint32_t timeoutSeconds = (config.readTimeoutMs + 999U) / 1000U;
	bool connected = false;
	if (parts.secure) {
		secureClient.setInsecure();
		secureClient.setTimeout(timeoutSeconds == 0 ? 1 : timeoutSeconds);
		secureClient.setHandshakeTimeout(timeoutSeconds == 0 ? 1 : timeoutSeconds);
		connected = secureClient.connect(parts.host.c_str(), parts.port, config.connectTimeoutMs);
		transport = &secureClient;
	} else {
		client.setTimeout(config.readTimeoutMs);
		connected = client.connect(parts.host.c_str(), parts.port, config.connectTimeoutMs);
		transport = &client;
	}
	if (!connected || transport == nullptr) {
		return fail(RadarEngineError::ConnectFailed, "radar host connect failed");
	}

	const String request = "GET " + parts.path + " HTTP/1.1\r\nHost: " + parts.host +
								 "\r\nUser-Agent: Flic-Radar/1.0\r\nConnection: close\r\n\r\n";
	if (transport->print(request) != request.length()) {
		transport->stop();
		return fail(RadarEngineError::IoError, "radar request write failed");
	}

	String lineBuffer;
	bool headerDone = false;
	int contentLength = -1;
	bool chunked = false;
	uint32_t startedAt = millis();
	uint32_t lastReadAt = startedAt;
	while (!headerDone && (transport->connected() || transport->available() > 0)) {
		while (transport->available() > 0) {
			char c = static_cast<char>(transport->read());
			lastReadAt = millis();
			if (c == '\r') {
				continue;
			}
			if (c == '\n') {
				if (lineBuffer.length() == 0) {
					headerDone = true;
					break;
				}
				if (lineBuffer.startsWith("HTTP/")) {
					int firstSpace = lineBuffer.indexOf(' ');
					int secondSpace = lineBuffer.indexOf(' ', firstSpace + 1);
					String codeText = secondSpace > 0 ? lineBuffer.substring(firstSpace + 1, secondSpace) : lineBuffer.substring(firstSpace + 1);
					httpStatus = codeText.toInt();
				} else if (lineBuffer.startsWith("Content-Length:") || lineBuffer.startsWith("content-length:")) {
					int colon = lineBuffer.indexOf(':');
					contentLength = lineBuffer.substring(colon + 1).toInt();
				} else if (lineBuffer.startsWith("Content-Type:") || lineBuffer.startsWith("content-type:")) {
					int colon = lineBuffer.indexOf(':');
					outContentType = lineBuffer.substring(colon + 1);
					outContentType.trim();
				} else {
					String header = lineBuffer;
					header.toLowerCase();
					if (header.startsWith("transfer-encoding:") && header.indexOf("chunked") >= 0) {
						chunked = true;
					}
				}
				lineBuffer = "";
			} else {
				lineBuffer += c;
				if (lineBuffer.length() > 1024) {
					transport->stop();
					return fail(RadarEngineError::ParseError, "radar header line too long");
				}
			}
		}
		if (headerDone) {
			break;
		}
		if ((millis() - lastReadAt) > config.readTimeoutMs || (millis() - startedAt) > config.readTimeoutMs) {
			transport->stop();
			return fail(RadarEngineError::Timeout, "radar header timeout");
		}
		delay(1);
	}

	if (!headerDone || httpStatus < 200 || httpStatus >= 300 || chunked) {
		transport->stop();
		return fail(headerDone && !chunked ? RadarEngineError::HttpError : RadarEngineError::ParseError,
			chunked ? "unsupported chunked radar response" : "invalid radar HTTP response");
	}

	const size_t maxBodyBytes = 512 * 1024;
	if (contentLength > static_cast<int>(maxBodyBytes)) {
		transport->stop();
		return fail(RadarEngineError::ParseError, "radar response exceeds 512 KB");
	}
	size_t capacity = contentLength > 0 ? static_cast<size_t>(contentLength) : 8192U;
	outData = static_cast<uint8_t*>(heap_caps_malloc(capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
	if (outData == nullptr) {
		transport->stop();
		return fail(RadarEngineError::OutOfMemory, "failed to allocate radar response");
	}

	uint8_t scratch[512];
	while (transport->connected() || transport->available() > 0) {
		while (transport->available() > 0) {
			size_t readSize = sizeof(scratch);
			if (contentLength >= 0) {
				const size_t remaining = static_cast<size_t>(contentLength) - outLength;
				if (remaining == 0) break;
				if (remaining < readSize) readSize = remaining;
			}
			int readLen = transport->read(scratch, readSize);
			if (readLen <= 0) {
				break;
			}
			lastReadAt = millis();
			if (outLength + static_cast<size_t>(readLen) > maxBodyBytes) {
				transport->stop();
				return fail(RadarEngineError::ParseError, "radar response exceeds 512 KB");
			}
			if (outLength + static_cast<size_t>(readLen) > capacity) {
				size_t nextCapacity = capacity;
				while (nextCapacity < outLength + static_cast<size_t>(readLen)) {
					nextCapacity *= 2U;
				}
				uint8_t* next = static_cast<uint8_t*>(heap_caps_realloc(
					outData, nextCapacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
				if (next == nullptr) {
					transport->stop();
					return fail(RadarEngineError::OutOfMemory, "failed to grow radar response");
				}
				outData = next;
				capacity = nextCapacity;
			}
			memcpy(outData + outLength, scratch, static_cast<size_t>(readLen));
			outLength += static_cast<size_t>(readLen);
		}
		if (contentLength >= 0 && outLength == static_cast<size_t>(contentLength)) {
			break;
		}
		if ((millis() - lastReadAt) > config.readTimeoutMs ||
				(millis() - startedAt) > config.readTimeoutMs + 15000UL) {
			transport->stop();
			return fail(RadarEngineError::Timeout, "radar body timeout");
		}
		delay(1);
	}
	transport->stop();

	if (outLength == 0 || (contentLength >= 0 && outLength != static_cast<size_t>(contentLength))) {
		return fail(RadarEngineError::ParseError, "empty or truncated radar response");
	}
	return true;
}

void RadarEngine::pumpDownload() {
	HttpJob* job = nullptr;
	if (responseQueue_ != nullptr && xQueueReceive(responseQueue_, &job, 0) == pdTRUE) {
		requestInFlight_ = false;
		if (job->generation == downloadGeneration_ && state_ == DownloadState::Waiting) {
			if (job->error != RadarEngineError::None) {
				transitionToError(job->error, job->message, job->httpStatus);
			} else {
				free(activeTempBuffer_);
				activeTempBuffer_ = job->data;
				activeTempLength_ = job->length;
				activeFormat_ = job->format;
				activeWidth_ = job->width;
				activeHeight_ = job->height;
				FrameSlot& slot = frames_[currentDownloadIndex_];
				slot.autoStormCellCount = job->autoStormCellCount;
				for (size_t i = 0; i < kMaxStormCells; ++i) {
					slot.autoStormCells[i] = job->autoStormCells[i];
				}
				job->data = nullptr;
				state_ = DownloadState::FinalizeFrame;
			}
		}
		delete job;
	}
	if (state_ == DownloadState::FinalizeFrame) {
		finalizeFrame();
		return;
	}
	if (state_ != DownloadState::Connect || requestInFlight_) {
		return;
	}
	job = new (std::nothrow) HttpJob();
	if (job == nullptr) {
		transitionToError(RadarEngineError::OutOfMemory, "radar request allocation failed");
		return;
	}
	job->generation = downloadGeneration_;
	job->url = frames_[currentDownloadIndex_].url;
	job->config = config_;
	if (xQueueSend(requestQueue_, &job, 0) != pdTRUE) {
		delete job;
		transitionToError(RadarEngineError::Busy, "radar request queue unavailable");
		return;
	}
	requestInFlight_ = true;
	state_ = DownloadState::Waiting;
	emitProgress("frame_download");
}

void RadarEngine::prepareDownloadedFrame(HttpJob& job) {
	job.format = inferFormat(job.contentType, job.config.expectedFormat);
	job.width = job.config.expectedWidth;
	job.height = job.config.expectedHeight;

	if (job.format == RadarFrameFormat::EncodedPng) {
		Serial.printf("[RADAR] decoding png bytes=%u\n", static_cast<unsigned>(job.length));
		uint8_t* decodedMapData = nullptr;
		size_t decodedMapLength = 0;
		uint16_t mapWidth = 0;
		uint16_t mapHeight = 0;
		if (job.mapData != nullptr) {
			RadarEngineError mapError = RadarEngineError::None;
			String mapMessage;
			if (!decodePngFrameToRgb565(job.mapData, job.mapLength, 0x00FFFFFF,
					decodedMapData, decodedMapLength, mapWidth, mapHeight, mapError, mapMessage)) {
				Serial.printf("[RADAR] map decode failed: %s; using radar overlay only\n", mapMessage.c_str());
			}
			free(job.mapData);
			job.mapData = nullptr;
			job.mapLength = 0;
		}

		uint8_t* decodedData = nullptr;
		size_t decodedLength = 0;
		const uint32_t backgroundColor = decodedMapData != nullptr ? 0x00FF00FF : 0x00000000;
		if (!decodePngFrameToRgb565(job.data, job.length, backgroundColor, decodedData,
				decodedLength, job.width, job.height, job.error, job.message)) {
			if (decodedMapData != nullptr) {
				free(decodedMapData);
			}
			return;
		}
		if (decodedMapData != nullptr && decodedMapLength == decodedLength && mapWidth == job.width && mapHeight == job.height) {
			const uint16_t chromaKey = 0xF81F;
			const size_t pixelCount = decodedLength / 2U;
			for (size_t pixel = 0; pixel < pixelCount; ++pixel) {
				const size_t offset = pixel * 2U;
				const uint16_t overlay = static_cast<uint16_t>(decodedData[offset]) |
					(static_cast<uint16_t>(decodedData[offset + 1]) << 8);
				if (overlay == chromaKey) {
					decodedData[offset] = decodedMapData[offset];
					decodedData[offset + 1] = decodedMapData[offset + 1];
				}
			}
		}
		if (decodedMapData != nullptr) {
			free(decodedMapData);
		}
		free(job.data);
		job.data = decodedData;
		job.length = decodedLength;
		job.format = RadarFrameFormat::RawRgb565;
	}

	if (job.format == RadarFrameFormat::RawRgb565 || job.format == RadarFrameFormat::RawArgb8888) {
		detectStormCells(job.data, job.length, job.format, job.width, job.height,
			job.autoStormCells, job.autoStormCellCount);
	}
}

void RadarEngine::finalizeFrame() {
	FrameSlot& slot = frames_[currentDownloadIndex_];
	if (!commitActiveDataToStorage()) {
		return;
	}

	slot.info.valid = true;
	slot.info.byteCount = activeTempLength_;
	slot.info.format = activeFormat_;
	slot.info.width = activeWidth_;
	slot.info.height = activeHeight_;

	++completedFrameCount_;
	++displayRevision_;
	emitProgress("frame_done");
	free(activeTempBuffer_);
	activeTempBuffer_ = nullptr;
	activeTempLength_ = 0;

	if (currentDownloadIndex_ + 1 >= frameCount_) {
		state_ = DownloadState::Complete;
		downloadStarted_ = false;
		Serial.printf("[RADAR] complete frames=%u bytes=%u\n", static_cast<unsigned>(completedFrameCount_), static_cast<unsigned>(slot.info.byteCount));
		emitProgress("complete");
		return;
	}

	++currentDownloadIndex_;
	state_ = DownloadState::Connect;
}

void RadarEngine::advanceAnimation() {
	if (completedFrameCount_ < 2) {
		interpolationStep_ = 0;
		currentAnimationIndex_ = 0;
		return;
	}
	const uint32_t fps = clampFps(animationFps_);
	const uint32_t subframes = (visualConfig_.enableFrameInterpolation ? static_cast<uint32_t>(visualConfig_.interpolationSteps) + 1U : 1U);
	const uint32_t frameInterval = 1000U / (fps * subframes);
	const uint32_t now = millis();
	if (now - lastAnimationStepMs_ < frameInterval) {
		return;
	}
	lastAnimationStepMs_ = now;
	++displayRevision_;

	if (visualConfig_.enableFrameInterpolation && visualConfig_.interpolationSteps > 0 && interpolationStep_ < visualConfig_.interpolationSteps) {
		++interpolationStep_;
		return;
	}
	interpolationStep_ = 0;

	size_t next = currentAnimationIndex_ + 1;
	if (next >= completedFrameCount_) {
		next = 0;
	}
	currentAnimationIndex_ = next;
}

bool RadarEngine::ensureFrameResident(FrameSlot& slot) {
	if (slot.ramData != nullptr) {
		return true;
	}
	if (!slot.info.storedInSpiffs || slot.info.spiffsPath.length() == 0) {
		return false;
	}

	for (size_t i = 0; i < frameCount_; ++i) {
		FrameSlot& other = frames_[i];
		if (&other == &slot) {
			continue;
		}
		if (other.info.storedInSpiffs && other.ramData != nullptr) {
			free(other.ramData);
			other.ramData = nullptr;
			other.ramLength = 0;
			other.dscValid = false;
		}
	}

	if (!ensureSpiffs()) {
		return false;
	}
	File file = SPIFFS.open(slot.info.spiffsPath, FILE_READ);
	if (!file) {
		transitionToError(RadarEngineError::IoError, "failed to reopen SPIFFS frame");
		return false;
	}
	const size_t size = static_cast<size_t>(file.size());
	uint8_t* data = static_cast<uint8_t*>(malloc(size));
	if (data == nullptr) {
		file.close();
		transitionToError(RadarEngineError::OutOfMemory, "failed to allocate frame buffer");
		return false;
	}
	if (file.read(data, size) != size) {
		file.close();
		free(data);
		transitionToError(RadarEngineError::IoError, "failed to read full SPIFFS frame");
		return false;
	}
	file.close();
	slot.ramData = data;
	slot.ramLength = size;
	return true;
}

bool RadarEngine::ensureDisplayBuffer(size_t length) {
	if (displayBuffer_ != nullptr && displayBufferLength_ >= length) {
		return true;
	}
	uint8_t* next = static_cast<uint8_t*>(realloc(displayBuffer_, length));
	if (next == nullptr) {
		transitionToError(RadarEngineError::OutOfMemory, "failed to allocate display frame");
		return false;
	}
	displayBuffer_ = next;
	displayBufferLength_ = length;
	return true;
}

bool RadarEngine::buildDisplayFrame(FrameSlot& current, FrameSlot* next, uint8_t blendStep, uint8_t blendSteps) {
	if (!ensureFrameResident(current)) {
		return false;
	}
	if (!ensureDisplayBuffer(current.ramLength)) {
		return false;
	}
	memcpy(displayBuffer_, current.ramData, current.ramLength);

	if (next != nullptr && blendSteps > 0 && ensureFrameResident(*next) && next->ramLength == current.ramLength && next->info.format == current.info.format) {
		const size_t pixelCount = static_cast<size_t>(current.info.width) * static_cast<size_t>(current.info.height);
		const uint8_t weight = static_cast<uint8_t>((255U * blendStep) / (blendSteps + 1U));
		for (size_t pixel = 0; pixel < pixelCount; ++pixel) {
			uint8_t r0 = 0;
			uint8_t g0 = 0;
			uint8_t b0 = 0;
			uint8_t a0 = 255;
			uint8_t r1 = 0;
			uint8_t g1 = 0;
			uint8_t b1 = 0;
			uint8_t a1 = 255;
			readPixel(current.ramData, pixel, current.info.format, r0, g0, b0, a0);
			readPixel(next->ramData, pixel, next->info.format, r1, g1, b1, a1);
			const uint8_t red = static_cast<uint8_t>((static_cast<uint16_t>(r0) * (255U - weight) + static_cast<uint16_t>(r1) * weight) / 255U);
			const uint8_t green = static_cast<uint8_t>((static_cast<uint16_t>(g0) * (255U - weight) + static_cast<uint16_t>(g1) * weight) / 255U);
			const uint8_t blue = static_cast<uint8_t>((static_cast<uint16_t>(b0) * (255U - weight) + static_cast<uint16_t>(b1) * weight) / 255U);
			const uint8_t alpha = static_cast<uint8_t>((static_cast<uint16_t>(a0) * (255U - weight) + static_cast<uint16_t>(a1) * weight) / 255U);
			writePixel(displayBuffer_, pixel, current.info.format, red, green, blue, alpha);
			if ((pixel & 0x03FFU) == 0U) {
				yield();
			}
		}
	}

	if (!applyStaticPostProcess(displayBuffer_, current.ramLength, current.info.format, current.info.width, current.info.height)) {
		return false;
	}

	if (visualConfig_.enableStormCellOverlays) {
		if (current.autoStormCellCount > 0) {
			applyStormCellOverlay(displayBuffer_, current.ramLength, current.info.format, current.info.width, current.info.height, current.autoStormCells, current.autoStormCellCount);
		}
		if (stormCellCount_ > 0) {
			applyStormCellOverlay(displayBuffer_, current.ramLength, current.info.format, current.info.width, current.info.height, stormCells_, stormCellCount_);
		}
	}

	memset(&displayDsc, 0, sizeof(displayDsc));
	displayDsc.header.always_zero = 0;
	displayDsc.header.w = current.info.width;
	displayDsc.header.h = current.info.height;
	displayDsc.header.cf = current.info.format == RadarFrameFormat::RawArgb8888 ? LV_IMG_CF_TRUE_COLOR_ALPHA : LV_IMG_CF_TRUE_COLOR;
	displayDsc.data_size = current.ramLength;
	displayDsc.data = displayBuffer_;
	return true;
}

bool RadarEngine::decodePngFrameToRgb565(const uint8_t* sourceData,
													 size_t sourceLength,
													 uint32_t backgroundColorRgb888,
													 uint8_t*& decodedData,
													 size_t& decodedLength,
													 uint16_t& width,
													 uint16_t& height,
													 RadarEngineError& error, String& errorMessage) {
	decodedData = nullptr;
	decodedLength = 0;
	auto fail = [&](RadarEngineError code, const char* message) {
		error = code;
		errorMessage = message;
	};
	if (sourceData == nullptr || sourceLength == 0) {
		fail(RadarEngineError::ParseError, "empty PNG frame");
		return false;
	}

	PNG* decoder = static_cast<PNG*>(malloc(sizeof(PNG)));
	if (decoder == nullptr) {
		fail(RadarEngineError::OutOfMemory, "failed to allocate PNG decoder");
		return false;
	}
	const int openResult = decoder->openRAM(const_cast<uint8_t*>(sourceData), static_cast<int>(sourceLength), decodePngLine);
	if (openResult != PNG_SUCCESS) {
		free(decoder);
		fail(RadarEngineError::ParseError, "PNG open failed");
		return false;
	}

	const int pngWidth = decoder->getWidth();
	const int pngHeight = decoder->getHeight();
	if (pngWidth <= 0 || pngHeight <= 0 || pngWidth > 512 || pngHeight > 512) {
		decoder->close();
		free(decoder);
		fail(RadarEngineError::ParseError, "radar PNG dimensions must be 1..512");
		return false;
	}
	width = static_cast<uint16_t>(pngWidth);
	height = static_cast<uint16_t>(pngHeight);
	decodedLength = static_cast<size_t>(width) * static_cast<size_t>(height) * 2U;
	decodedData = static_cast<uint8_t*>(heap_caps_malloc(
		decodedLength, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
	if (decodedData == nullptr) {
		decoder->close();
		free(decoder);
		fail(RadarEngineError::OutOfMemory, "failed to allocate decoded PNG buffer");
		return false;
	}

	PngDecodeContext context;
	context.decoder = decoder;
	context.buffer = decodedData;
	context.strideBytes = static_cast<size_t>(width) * 2U;
	context.backgroundColor = backgroundColorRgb888;
	const int decodeResult = decoder->decode(&context, PNG_FAST_PALETTE);
	decoder->close();
	free(decoder);
	if (decodeResult != PNG_SUCCESS) {
		free(decodedData);
		decodedData = nullptr;
		decodedLength = 0;
		fail(RadarEngineError::ParseError, "PNG decode failed");
		return false;
	}
	return true;
}

bool RadarEngine::applyStaticPostProcess(uint8_t* data, size_t length, RadarFrameFormat format, uint16_t width, uint16_t height) {
	const size_t pixelBytes = bytesPerPixel(format);
	if (data == nullptr || pixelBytes == 0 || width == 0 || height == 0) {
		return true;
	}
	const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
	if (length < pixelCount * pixelBytes) {
		return true;
	}

	uint8_t minLuma = 255;
	uint8_t maxLuma = 0;
	for (size_t pixel = 0; pixel < pixelCount; ++pixel) {
		uint8_t red = 0;
		uint8_t green = 0;
		uint8_t blue = 0;
		uint8_t alpha = 255;
		readPixel(data, pixel, format, red, green, blue, alpha);
		const uint8_t luma = luminanceOf(red, green, blue);
		if (luma < minLuma) {
			minLuma = luma;
		}
		if (luma > maxLuma) {
			maxLuma = luma;
		}
		if ((pixel & 0x03FFU) == 0U) {
			yield();
		}
	}

	const uint16_t lumaRange = static_cast<uint16_t>(maxLuma) - static_cast<uint16_t>(minLuma);
	for (size_t pixel = 0; pixel < pixelCount; ++pixel) {
		uint8_t red = 0;
		uint8_t green = 0;
		uint8_t blue = 0;
		uint8_t alpha = 255;
		readPixel(data, pixel, format, red, green, blue, alpha);
		if (visualConfig_.enableAutoContrast && lumaRange >= 20U) {
			red = clampByte(((static_cast<int>(red) - minLuma) * 255) / lumaRange);
			green = clampByte(((static_cast<int>(green) - minLuma) * 255) / lumaRange);
			blue = clampByte(((static_cast<int>(blue) - minLuma) * 255) / lumaRange);
		}
		applyReflectivityColor(config_.reflectivityMode, red, green, blue);
		writePixel(data, pixel, format, red, green, blue, alpha);
		if ((pixel & 0x03FFU) == 0U) {
			yield();
		}
	}

	if (visualConfig_.smoothingPasses == 0) {
		return true;
	}
	uint8_t* scratch = static_cast<uint8_t*>(malloc(length));
	if (scratch == nullptr) {
		return true;
	}
	for (uint8_t pass = 0; pass < visualConfig_.smoothingPasses; ++pass) {
		memcpy(scratch, data, length);
		for (uint16_t y = 0; y < height; ++y) {
			for (uint16_t x = 0; x < width; ++x) {
				const size_t pixel = static_cast<size_t>(y) * width + x;
				uint16_t redSum = 0;
				uint16_t greenSum = 0;
				uint16_t blueSum = 0;
				uint16_t alphaSum = 0;
				uint8_t sampleCount = 0;
				for (int8_t oy = -1; oy <= 1; ++oy) {
					for (int8_t ox = -1; ox <= 1; ++ox) {
						const int nx = static_cast<int>(x) + ox;
						const int ny = static_cast<int>(y) + oy;
						if (nx < 0 || ny < 0 || nx >= width || ny >= height) {
							continue;
						}
						uint8_t red = 0;
						uint8_t green = 0;
						uint8_t blue = 0;
						uint8_t alpha = 255;
						readPixel(scratch, static_cast<size_t>(ny) * width + static_cast<size_t>(nx), format, red, green, blue, alpha);
						redSum += red;
						greenSum += green;
						blueSum += blue;
						alphaSum += alpha;
						++sampleCount;
					}
				}
				writePixel(data,
						  pixel,
						  format,
						  static_cast<uint8_t>(redSum / sampleCount),
						  static_cast<uint8_t>(greenSum / sampleCount),
						  static_cast<uint8_t>(blueSum / sampleCount),
						  static_cast<uint8_t>(alphaSum / sampleCount));
			}
			if ((y % 16U) == 0U) {
				yield();
			}
		}
	}
	free(scratch);
	return true;
}

void RadarEngine::applyStormCellOverlay(uint8_t* data, size_t length, RadarFrameFormat format, uint16_t width, uint16_t height, const RadarStormCell* cells, size_t count) {
	if (data == nullptr || bytesPerPixel(format) == 0 || width == 0 || height == 0 || length == 0) {
		return;
	}
	const uint8_t overlayRed = config_.reflectivityMode == ReflectivityMode::Velocity ? 96 : 255;
	const uint8_t overlayGreen = config_.reflectivityMode == ReflectivityMode::EchoTop ? 120 : 176;
	const uint8_t overlayBlue = config_.reflectivityMode == ReflectivityMode::CompositeReflectivity ? 88 : 64;

	for (size_t index = 0; index < count; ++index) {
		const RadarStormCell& cell = cells[index];
		if (!cell.valid) {
			continue;
		}
		const int outerRadius = cell.radius;
		const int innerRadius = outerRadius > 2 ? outerRadius - 2 : outerRadius;
		for (int y = cell.y - outerRadius; y <= cell.y + outerRadius; ++y) {
			if (y < 0 || y >= height) {
				continue;
			}
			for (int x = cell.x - outerRadius; x <= cell.x + outerRadius; ++x) {
				if (x < 0 || x >= width) {
					continue;
				}
				const int dx = x - cell.x;
				const int dy = y - cell.y;
				const int dist2 = dx * dx + dy * dy;
				if (dist2 > outerRadius * outerRadius || dist2 < innerRadius * innerRadius) {
					continue;
				}
				const size_t pixel = static_cast<size_t>(y) * width + static_cast<size_t>(x);
				uint8_t red = 0;
				uint8_t green = 0;
				uint8_t blue = 0;
				uint8_t alpha = 255;
				readPixel(data, pixel, format, red, green, blue, alpha);
				blendToward(red, green, blue, overlayRed, overlayGreen, overlayBlue, cell.intensity);
				writePixel(data, pixel, format, red, green, blue, alpha);
			}
		}

		int tailX = cell.x;
		int tailY = cell.y;
		for (uint8_t step = 0; step < 5; ++step) {
			tailX += cell.velocityX;
			tailY += cell.velocityY;
			if (tailX < 0 || tailY < 0 || tailX >= width || tailY >= height) {
				break;
			}
			const size_t pixel = static_cast<size_t>(tailY) * width + static_cast<size_t>(tailX);
			uint8_t red = 0;
			uint8_t green = 0;
			uint8_t blue = 0;
			uint8_t alpha = 255;
			readPixel(data, pixel, format, red, green, blue, alpha);
			blendToward(red, green, blue, overlayRed, overlayGreen, overlayBlue, static_cast<uint8_t>(cell.intensity / 2));
			writePixel(data, pixel, format, red, green, blue, alpha);
		}
	}
}

void RadarEngine::detectStormCells(const uint8_t* data,
														 size_t length,
														 RadarFrameFormat format,
														 uint16_t width,
														 uint16_t height,
														 RadarStormCell* outCells,
														 size_t& outCount) {
	outCount = 0;
	if (outCells == nullptr || data == nullptr || bytesPerPixel(format) == 0 || width < 8 || height < 8) {
		return;
	}
	const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
	if (length < pixelCount * bytesPerPixel(format)) {
		return;
	}

	// Build a lightweight luminance distribution so the threshold adapts to each frame.
	uint32_t histogram[256] = {0};
	for (uint16_t y = 0; y < height; y += kStormDetectSampleStep) {
		for (uint16_t x = 0; x < width; x += kStormDetectSampleStep) {
			const size_t pixel = static_cast<size_t>(y) * width + x;
			uint8_t red = 0;
			uint8_t green = 0;
			uint8_t blue = 0;
			uint8_t alpha = 255;
			readPixel(data, pixel, format, red, green, blue, alpha);
			++histogram[luminanceOf(red, green, blue)];
		}
		if ((y % 24U) == 0U) {
			yield();
		}
	}

	uint32_t sampleCount = 0;
	for (uint16_t luma = 0; luma < 256; ++luma) {
		sampleCount += histogram[luma];
	}
	uint8_t dynamicThreshold = kStormDetectFloor;
	if (sampleCount > 0) {
		const uint32_t target = (sampleCount * 9U) / 10U;
		uint32_t cumulative = 0;
		for (int luma = 0; luma < 256; ++luma) {
			cumulative += histogram[luma];
			if (cumulative >= target) {
				dynamicThreshold = static_cast<uint8_t>(luma);
				break;
			}
		}
		if (dynamicThreshold < kStormDetectFloor) {
			dynamicThreshold = kStormDetectFloor;
		}
	}

	for (uint16_t y = 2; y + 2 < height && outCount < kMaxStormCells; y += kStormDetectSampleStep) {
		for (uint16_t x = 2; x + 2 < width && outCount < kMaxStormCells; x += kStormDetectSampleStep) {
			const size_t pixel = static_cast<size_t>(y) * width + x;
			uint8_t red = 0;
			uint8_t green = 0;
			uint8_t blue = 0;
			uint8_t alpha = 255;
			readPixel(data, pixel, format, red, green, blue, alpha);
			const uint8_t centerLuma = luminanceOf(red, green, blue);
			if (centerLuma < dynamicThreshold) {
				continue;
			}

			bool localPeak = true;
			int horizontalGradient = 0;
			int verticalGradient = 0;
			uint32_t neighborhoodSum = 0;
			uint8_t neighborhoodCount = 0;
			uint8_t peakDrop = 255;
			int weightedOffsetX = 0;
			int weightedOffsetY = 0;
			int weightedMass = 0;
			for (int8_t oy = -2; oy <= 2 && localPeak; ++oy) {
				for (int8_t ox = -2; ox <= 2; ++ox) {
					if (ox == 0 && oy == 0) {
						continue;
					}
					uint8_t neighborR = 0;
					uint8_t neighborG = 0;
					uint8_t neighborB = 0;
					uint8_t neighborA = 255;
					const size_t neighborPixel = static_cast<size_t>(static_cast<int>(y) + oy) * width + static_cast<size_t>(static_cast<int>(x) + ox);
					readPixel(data, neighborPixel, format, neighborR, neighborG, neighborB, neighborA);
					const uint8_t neighborLuma = luminanceOf(neighborR, neighborG, neighborB);
					if (neighborLuma > centerLuma) {
						localPeak = false;
						break;
					}
					if (neighborLuma < peakDrop) {
						peakDrop = neighborLuma;
					}
					neighborhoodSum += neighborLuma;
					++neighborhoodCount;
					const int weight = static_cast<int>(neighborLuma) - static_cast<int>(dynamicThreshold);
					if (weight > 0) {
						weightedOffsetX += ox * weight;
						weightedOffsetY += oy * weight;
						weightedMass += weight;
					}
					horizontalGradient += ox * static_cast<int>(neighborLuma);
					verticalGradient += oy * static_cast<int>(neighborLuma);
				}
			}
			if (!localPeak) {
				continue;
			}

			const uint8_t neighborhoodAvg = neighborhoodCount == 0 ? 0 : static_cast<uint8_t>(neighborhoodSum / neighborhoodCount);
			const int prominence = static_cast<int>(centerLuma) - static_cast<int>(neighborhoodAvg);
			if (prominence < kStormDetectPeakProminence || (centerLuma - peakDrop) < (kStormDetectPeakProminence / 2)) {
				continue;
			}

			bool tooClose = false;
			for (size_t existing = 0; existing < outCount; ++existing) {
				const int dx = static_cast<int>(outCells[existing].x) - static_cast<int>(x);
				const int dy = static_cast<int>(outCells[existing].y) - static_cast<int>(y);
				if ((dx * dx + dy * dy) < (kStormDetectMinCellSpacing * kStormDetectMinCellSpacing)) {
					tooClose = true;
					break;
				}
			}
			if (tooClose) {
				continue;
			}

			RadarStormCell& cell = outCells[outCount++];
			cell = RadarStormCell();
			cell.valid = true;
			cell.x = static_cast<int16_t>(x);
			cell.y = static_cast<int16_t>(y);
			const int lumaDelta = static_cast<int>(centerLuma) - static_cast<int>(dynamicThreshold);
			cell.radius = static_cast<uint8_t>(6 + (lumaDelta / 14) + (prominence / 18));
			if (cell.radius > 16) {
				cell.radius = 16;
			}
			cell.intensity = clampByte(172 + lumaDelta + prominence / 2);

			if (weightedMass > 0) {
				const int driftX = weightedOffsetX / weightedMass;
				const int driftY = weightedOffsetY / weightedMass;
				cell.velocityX = static_cast<int8_t>(driftX > 0 ? 1 : (driftX < 0 ? -1 : 0));
				cell.velocityY = static_cast<int8_t>(driftY > 0 ? 1 : (driftY < 0 ? -1 : 0));
			} else {
				cell.velocityX = static_cast<int8_t>(horizontalGradient > 280 ? 1 : (horizontalGradient < -280 ? -1 : 0));
				cell.velocityY = static_cast<int8_t>(verticalGradient > 280 ? 1 : (verticalGradient < -280 ? -1 : 0));
			}
		}
		if ((y % 24U) == 2U) {
			yield();
		}
	}
}

void RadarEngine::invalidateDescriptors() {
	++displayRevision_;
	for (size_t index = 0; index < frameCount_; ++index) {
		frames_[index].dscValid = false;
	}
}

bool RadarEngine::ensureSpiffs() {
	if (spiffsReady_) {
		return true;
	}
	spiffsReady_ = SPIFFS.begin(true);
	return spiffsReady_;
}

bool RadarEngine::commitActiveDataToStorage() {
	FrameSlot& slot = frames_[currentDownloadIndex_];
	const bool overRamBudget = (activeTempLength_ > config_.ramBudgetBytes);
	bool useSpiffs = (config_.storageMode == RadarStorageMode::SpiffsOnly) ||
										 (config_.storageMode == RadarStorageMode::Auto && overRamBudget);

	if (useSpiffs) {
		if (ensureSpiffs()) {
			String path = "/radar_" + String(static_cast<unsigned>(currentDownloadIndex_)) + ".bin";
			File f = SPIFFS.open(path, FILE_WRITE);
			if (f) {
				size_t written = f.write(activeTempBuffer_, activeTempLength_);
				f.close();
				if (written == activeTempLength_) {
					slot.info.storedInSpiffs = true;
					slot.info.spiffsPath = path;
					slot.ramData = nullptr;
					slot.ramLength = 0;
				} else {
					SPIFFS.remove(path);
					Serial.println("[RADAR] SPIFFS write incomplete, falling back to RAM");
					useSpiffs = false;
				}
			} else {
				Serial.println("[RADAR] SPIFFS open failed, falling back to RAM");
				useSpiffs = false;
			}
		} else {
			Serial.println("[RADAR] SPIFFS unavailable, falling back to RAM");
			useSpiffs = false;
		}
	}
	if (!useSpiffs) {
		slot.info.storedInSpiffs = false;
		slot.info.spiffsPath = "";
		slot.ramData = activeTempBuffer_;
		slot.ramLength = activeTempLength_;
		activeTempBuffer_ = nullptr;
	}

	slot.info.byteCount = activeTempLength_;
	slot.dscValid = false;
	return true;
}

bool RadarEngine::prepareLvglDescriptor(FrameSlot& slot) {
	if (!ensureFrameResident(slot)) {
		return false;
	}

	if (slot.info.format == RadarFrameFormat::RawRgb565) {
		memset(&slot.dsc, 0, sizeof(slot.dsc));
		slot.dsc.header.always_zero = 0;
		slot.dsc.header.w = slot.info.width;
		slot.dsc.header.h = slot.info.height;
		slot.dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
		slot.dsc.data_size = slot.ramLength;
		slot.dsc.data = slot.ramData;
		slot.dscValid = true;
		return true;
	}

	if (slot.info.format == RadarFrameFormat::RawArgb8888) {
		memset(&slot.dsc, 0, sizeof(slot.dsc));
		slot.dsc.header.always_zero = 0;
		slot.dsc.header.w = slot.info.width;
		slot.dsc.header.h = slot.info.height;
		slot.dsc.header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
		slot.dsc.data_size = slot.ramLength;
		slot.dsc.data = slot.ramData;
		slot.dscValid = true;
		return true;
	}

	slot.dscValid = false;
	return false;
}

}  // namespace weather
