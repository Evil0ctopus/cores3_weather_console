#include "radar_engine.h"

#include <PNGdec.h>
#include <SPIFFS.h>
#include <WiFi.h>

namespace weather {
namespace {

const uint32_t kAnimationMinFps = 6;
const uint32_t kAnimationMaxFps = 12;
const uint32_t kDownloadPumpSliceBytes = 4096;
const uint32_t kDownloadPumpIntervalMs = 2;
const uint8_t kStormDetectFloor = 148;
const uint8_t kStormDetectPeakProminence = 22;
const uint8_t kStormDetectSampleStep = 3;
const uint8_t kStormDetectMinCellSpacing = 12;

struct PngDecodeContext {
	PNG* decoder = nullptr;
	uint8_t* buffer = nullptr;
	size_t strideBytes = 0;
};

int decodePngLine(PNGDRAW* draw) {
	PngDecodeContext* context = static_cast<PngDecodeContext*>(draw->pUser);
	if (context == nullptr || context->decoder == nullptr || context->buffer == nullptr) {
		return 0;
	}
	uint16_t* row = reinterpret_cast<uint16_t*>(context->buffer + (static_cast<size_t>(draw->y) * context->strideBytes));
	context->decoder->getLineAsRGB565(draw, row, PNG_RGB565_LITTLE_ENDIAN, 0x00000000);
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
	lastAnimationStepMs_ = millis();
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
		transitionToError(RadarEngineError::InvalidInput, "frameCount must be 6..12 with valid URL array");
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

	downloadStarted_ = true;
	state_ = DownloadState::Connect;
	stateEnteredMs_ = millis();
	lastReceiveMs_ = stateEnteredMs_;
	emitProgress("start");
	return true;
}

void RadarEngine::tick() {
	advanceAnimation();
	if (!downloadStarted_) {
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
	clearFrames();
	frameCount_ = 0;
	completedFrameCount_ = 0;
	currentDownloadIndex_ = 0;
	currentAnimationIndex_ = 0;
	activeContentLength_ = -1;
	activeHttpCode_ = 0;
	activeReceived_ = 0;
	activeHeaderDone_ = false;
	activeContentType_ = "";
	headerBuffer_ = "";
	requestBuffer_ = "";
	interpolationStep_ = 0;

	if (activeTempBuffer_ != nullptr) {
		free(activeTempBuffer_);
		activeTempBuffer_ = nullptr;
	}
	activeTempCapacity_ = 0;
	activeTempLength_ = 0;
	activeTransportClient_ = nullptr;
	activeSecureClient_.stop();
	activeClient_.stop();
	stateEnteredMs_ = 0;
	lastReceiveMs_ = 0;

	lastError_ = RadarEngineError::None;
	lastErrorMessage_ = "";
	lastHttpStatus_ = 0;
	state_ = DownloadState::Idle;
	downloadStarted_ = false;
}

void RadarEngine::clearFrame(FrameSlot& slot) {
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
	activeClient_.stop();
	activeSecureClient_.stop();
	activeTransportClient_ = nullptr;
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

bool RadarEngine::parseUrl(const String& url, UrlParts& out) const {
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

bool RadarEngine::beginFrameDownload(size_t index) {
	if (index >= frameCount_) {
		return false;
	}
	if (!parseUrl(frames_[index].url, activeUrl_)) {
		transitionToError(RadarEngineError::ParseError, "invalid radar URL");
		return false;
	}

	bool connected = false;
	if (activeUrl_.secure) {
		activeSecureClient_.setInsecure();
		activeSecureClient_.setTimeout(config_.readTimeoutMs / 1000);
		connected = activeSecureClient_.connect(activeUrl_.host.c_str(), activeUrl_.port);
		activeTransportClient_ = &activeSecureClient_;
	} else {
		activeClient_.setTimeout(config_.readTimeoutMs / 1000);
		connected = activeClient_.connect(activeUrl_.host.c_str(), activeUrl_.port, config_.connectTimeoutMs);
		activeTransportClient_ = &activeClient_;
	}

	if (!connected || activeTransportClient_ == nullptr) {
		transitionToError(RadarEngineError::ConnectFailed, "radar host connect failed");
		return false;
	}

	requestBuffer_ = "GET " + activeUrl_.path + " HTTP/1.1\r\nHost: " + activeUrl_.host +
									 "\r\nUser-Agent: Flic-Radar/1.0\r\nConnection: close\r\n\r\n";
	activeTransportClient_->print(requestBuffer_);

	headerBuffer_ = "";
	activeContentType_ = "";
	activeContentLength_ = -1;
	activeHttpCode_ = 0;
	activeReceived_ = 0;
	activeHeaderDone_ = false;
	activeTempLength_ = 0;
	stateEnteredMs_ = millis();
	lastReceiveMs_ = stateEnteredMs_;
	emitProgress("frame_connect");
	return true;
}

bool RadarEngine::readHeaderLine(String& line) {
	if (activeTransportClient_ == nullptr) {
		return false;
	}
	while (activeTransportClient_->available() > 0) {
		char c = static_cast<char>(activeTransportClient_->read());
		lastReceiveMs_ = millis();
		if (c == '\r') {
			continue;
		}
		if (c == '\n') {
			line = headerBuffer_;
			headerBuffer_ = "";
			return true;
		}
		headerBuffer_ += c;
		if (headerBuffer_.length() > 1024) {
			transitionToError(RadarEngineError::ParseError, "header line too long");
			return false;
		}
	}
	return false;
}

void RadarEngine::pumpDownload() {
	const uint32_t now = millis();
	if (state_ == DownloadState::ReadHeaders || state_ == DownloadState::ReadBody) {
		if (lastReceiveMs_ != 0 && (now - lastReceiveMs_) > config_.readTimeoutMs) {
			transitionToError(RadarEngineError::Timeout, "radar download read timeout");
			return;
		}
	}

	if (state_ == DownloadState::Connect) {
		if (stateEnteredMs_ != 0 && (now - stateEnteredMs_) > config_.connectTimeoutMs) {
			transitionToError(RadarEngineError::Timeout, "radar connect timeout");
			return;
		}
		if (!beginFrameDownload(currentDownloadIndex_)) {
			return;
		}
		state_ = DownloadState::ReadHeaders;
		stateEnteredMs_ = now;
		return;
	}

	if (state_ == DownloadState::ReadHeaders) {
		String line;
		while (readHeaderLine(line)) {
			if (line.length() == 0) {
				activeHeaderDone_ = true;
				if (activeHttpCode_ < 200 || activeHttpCode_ >= 300) {
					transitionToError(RadarEngineError::HttpError, "radar HTTP error", activeHttpCode_);
					return;
				}
				state_ = DownloadState::ReadBody;
				stateEnteredMs_ = now;
				emitProgress("frame_download");
				return;
			}

			if (line.startsWith("HTTP/")) {
				int firstSpace = line.indexOf(' ');
				if (firstSpace >= 0) {
					int secondSpace = line.indexOf(' ', firstSpace + 1);
					String codeText = secondSpace > 0 ? line.substring(firstSpace + 1, secondSpace)
																						: line.substring(firstSpace + 1);
					activeHttpCode_ = codeText.toInt();
				}
			} else if (line.startsWith("Content-Length:") || line.startsWith("content-length:")) {
				int colon = line.indexOf(':');
				activeContentLength_ = line.substring(colon + 1).toInt();
			} else if (line.startsWith("Content-Type:") || line.startsWith("content-type:")) {
				int colon = line.indexOf(':');
				activeContentType_ = line.substring(colon + 1);
				activeContentType_.trim();
			}
		}

		if (activeTransportClient_ != nullptr && !activeTransportClient_->connected() && activeTransportClient_->available() == 0) {
			transitionToError(RadarEngineError::ParseError, "connection closed before headers complete");
			return;
		}
		return;
	}

	if (state_ == DownloadState::ReadBody) {
		if (activeTempBuffer_ == nullptr) {
			size_t hint = activeContentLength_ > 0 ? static_cast<size_t>(activeContentLength_) : 64 * 1024;
			if (!reserveRamForActiveFrame(hint)) {
				return;
			}
		}

		uint8_t scratch[512];
		size_t consumedInThisTick = 0;
		while (activeTransportClient_ != nullptr && activeTransportClient_->available() > 0 && consumedInThisTick < kDownloadPumpSliceBytes) {
			size_t canRead = sizeof(scratch);
			int readLen = activeTransportClient_->read(scratch, canRead);
			if (readLen <= 0) {
				break;
			}
			lastReceiveMs_ = now;
			consumedInThisTick += static_cast<size_t>(readLen);
			if (!appendActiveData(scratch, static_cast<size_t>(readLen))) {
				return;
			}
		}

		activeReceived_ = activeTempLength_;
		if (activeTransportClient_ != nullptr && !activeTransportClient_->connected() && activeTransportClient_->available() == 0) {
			state_ = DownloadState::FinalizeFrame;
			stateEnteredMs_ = now;
			finalizeFrame();
			return;
		}
		return;
	}
}

void RadarEngine::finalizeFrame() {
	RadarFrameFormat finalizedFormat = inferFormat(activeContentType_, config_.expectedFormat);
	uint16_t finalizedWidth = config_.expectedWidth;
	uint16_t finalizedHeight = config_.expectedHeight;
	FrameSlot& slot = frames_[currentDownloadIndex_];
	slot.autoStormCellCount = 0;

	if (finalizedFormat == RadarFrameFormat::EncodedPng) {
		uint8_t* decodedData = nullptr;
		size_t decodedLength = 0;
		if (!decodePngFrameToRgb565(decodedData, decodedLength, finalizedWidth, finalizedHeight)) {
			return;
		}
		free(activeTempBuffer_);
		activeTempBuffer_ = decodedData;
		activeTempLength_ = decodedLength;
		activeTempCapacity_ = decodedLength;
		finalizedFormat = RadarFrameFormat::RawRgb565;
	}

	if (finalizedFormat == RadarFrameFormat::RawRgb565 || finalizedFormat == RadarFrameFormat::RawArgb8888) {
		detectStormCells(activeTempBuffer_, activeTempLength_, finalizedFormat, finalizedWidth, finalizedHeight, slot.autoStormCells, slot.autoStormCellCount);
	}
	if (!commitActiveDataToStorage()) {
		return;
	}

	slot.info.valid = true;
	slot.info.byteCount = activeTempLength_;
	slot.info.format = finalizedFormat;
	slot.info.width = finalizedWidth;
	slot.info.height = finalizedHeight;

	++completedFrameCount_;
	emitProgress("frame_done");
	activeClient_.stop();
	activeSecureClient_.stop();
	activeTransportClient_ = nullptr;

	if (currentDownloadIndex_ + 1 >= frameCount_) {
		state_ = DownloadState::Complete;
		downloadStarted_ = false;
		emitProgress("complete");
		return;
	}

	++currentDownloadIndex_;
	state_ = DownloadState::Connect;
	stateEnteredMs_ = millis();
}

void RadarEngine::advanceAnimation() {
	if (completedFrameCount_ < 2) {
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

bool RadarEngine::decodePngFrameToRgb565(uint8_t*& decodedData, size_t& decodedLength, uint16_t& width, uint16_t& height) {
	decodedData = nullptr;
	decodedLength = 0;
	if (activeTempBuffer_ == nullptr || activeTempLength_ == 0) {
		transitionToError(RadarEngineError::ParseError, "empty PNG frame");
		return false;
	}

	PNG* decoder = static_cast<PNG*>(malloc(sizeof(PNG)));
	if (decoder == nullptr) {
		transitionToError(RadarEngineError::OutOfMemory, "failed to allocate PNG decoder");
		return false;
	}
	const int openResult = decoder->openRAM(activeTempBuffer_, static_cast<int>(activeTempLength_), decodePngLine);
	if (openResult != PNG_SUCCESS) {
		free(decoder);
		transitionToError(RadarEngineError::ParseError, "PNG open failed");
		return false;
	}

	width = static_cast<uint16_t>(decoder->getWidth());
	height = static_cast<uint16_t>(decoder->getHeight());
	decodedLength = static_cast<size_t>(width) * static_cast<size_t>(height) * 2U;
	decodedData = static_cast<uint8_t*>(malloc(decodedLength));
	if (decodedData == nullptr) {
		decoder->close();
		free(decoder);
		transitionToError(RadarEngineError::OutOfMemory, "failed to allocate decoded PNG buffer");
		return false;
	}

	PngDecodeContext context;
	context.decoder = decoder;
	context.buffer = decodedData;
	context.strideBytes = static_cast<size_t>(width) * 2U;
	const int decodeResult = decoder->decode(&context, PNG_FAST_PALETTE);
	decoder->close();
	free(decoder);
	if (decodeResult != PNG_SUCCESS) {
		free(decodedData);
		decodedData = nullptr;
		decodedLength = 0;
		transitionToError(RadarEngineError::ParseError, "PNG decode failed");
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
														 size_t& outCount) const {
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
	}
}

void RadarEngine::invalidateDescriptors() {
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

bool RadarEngine::reserveRamForActiveFrame(size_t bytesHint) {
	if (activeTempBuffer_ != nullptr && activeTempCapacity_ >= bytesHint) {
		return true;
	}
	size_t nextCapacity = bytesHint;
	uint8_t* next = static_cast<uint8_t*>(realloc(activeTempBuffer_, nextCapacity));
	if (next == nullptr) {
		transitionToError(RadarEngineError::OutOfMemory, "failed to allocate download scratch");
		return false;
	}
	activeTempBuffer_ = next;
	activeTempCapacity_ = nextCapacity;
	return true;
}

bool RadarEngine::appendActiveData(const uint8_t* data, size_t len) {
	size_t needed = activeTempLength_ + len;
	if (needed > activeTempCapacity_) {
		size_t grow = activeTempCapacity_ == 0 ? needed : activeTempCapacity_;
		while (grow < needed) {
			grow *= 2;
			if (grow < 1024) {
				grow = 1024;
			}
		}
		if (!reserveRamForActiveFrame(grow)) {
			return false;
		}
	}
	memcpy(activeTempBuffer_ + activeTempLength_, data, len);
	activeTempLength_ += len;
	return true;
}

bool RadarEngine::commitActiveDataToStorage() {
	FrameSlot& slot = frames_[currentDownloadIndex_];
	const bool overRamBudget = (activeTempLength_ > config_.ramBudgetBytes);
	const bool useSpiffs = (config_.storageMode == RadarStorageMode::SpiffsOnly) ||
												 (config_.storageMode == RadarStorageMode::Auto && overRamBudget);

	if (useSpiffs) {
		if (!ensureSpiffs()) {
			transitionToError(RadarEngineError::IoError, "SPIFFS not available");
			return false;
		}

		String path = "/radar_" + String(static_cast<unsigned>(currentDownloadIndex_)) + ".bin";
		File f = SPIFFS.open(path, FILE_WRITE);
		if (!f) {
			transitionToError(RadarEngineError::IoError, "failed to open SPIFFS frame file");
			return false;
		}
		size_t written = f.write(activeTempBuffer_, activeTempLength_);
		f.close();
		if (written != activeTempLength_) {
			transitionToError(RadarEngineError::IoError, "failed to write full SPIFFS frame");
			return false;
		}
		slot.info.storedInSpiffs = true;
		slot.info.spiffsPath = path;
		slot.ramData = nullptr;
		slot.ramLength = 0;
	} else {
		uint8_t* exact = static_cast<uint8_t*>(malloc(activeTempLength_));
		if (exact == nullptr) {
			transitionToError(RadarEngineError::OutOfMemory, "failed to allocate frame storage");
			return false;
		}
		memcpy(exact, activeTempBuffer_, activeTempLength_);
		slot.info.storedInSpiffs = false;
		slot.info.spiffsPath = "";
		slot.ramData = exact;
		slot.ramLength = activeTempLength_;
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
