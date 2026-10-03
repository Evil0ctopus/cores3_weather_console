#pragma once

#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <lvgl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <stddef.h>
#include <stdint.h>

namespace weather {

enum class RadarStorageMode : uint8_t {
	Auto = 0,
	RamOnly,
	SpiffsOnly,
};

enum class RadarFrameFormat : uint8_t {
	Unknown = 0,
	RawRgb565,
	RawArgb8888,
	EncodedPng,
	EncodedJpeg,
};

enum class ReflectivityMode : uint8_t {
	BaseReflectivity = 0,
	CompositeReflectivity,
	EchoTop,
	Velocity,
	DifferentialPhase,
	Custom,
};

enum class RadarEngineError : uint8_t {
	None = 0,
	InvalidInput,
	WifiDisconnected,
	Busy,
	ConnectFailed,
	HttpError,
	IoError,
	OutOfMemory,
	ParseError,
	Timeout,
};

struct RadarDownloadConfig {
	RadarStorageMode storageMode = RadarStorageMode::Auto;
	ReflectivityMode reflectivityMode = ReflectivityMode::BaseReflectivity;
	uint32_t connectTimeoutMs = 2500;
	uint32_t readTimeoutMs = 2500;
	size_t ramBudgetBytes = 350 * 1024;
	String baseMapUrl;
	RadarFrameFormat expectedFormat = RadarFrameFormat::EncodedPng;
	uint16_t expectedWidth = 0;
	uint16_t expectedHeight = 0;
};

struct RadarFrameInfo {
	bool valid = false;
	bool storedInSpiffs = false;
	RadarFrameFormat format = RadarFrameFormat::Unknown;
	ReflectivityMode reflectivityMode = ReflectivityMode::BaseReflectivity;
	uint32_t epochTime = 0;
	uint16_t width = 0;
	uint16_t height = 0;
	size_t byteCount = 0;
	String spiffsPath;
};

struct RadarProgress {
	size_t totalFrames = 0;
	size_t completedFrames = 0;
	size_t activeFrameIndex = 0;
	uint8_t percent = 0;
	const char* stage = "idle";
};

struct RadarVisualConfig {
	bool enableFrameInterpolation = true;
	bool enableAutoContrast = true;
	bool enableStormCellOverlays = true;
	uint8_t interpolationSteps = 1;
	uint8_t smoothingPasses = 1;
};

struct RadarStormCell {
	bool valid = false;
	int16_t x = 0;
	int16_t y = 0;
	uint8_t radius = 10;
	uint8_t intensity = 220;
	int8_t velocityX = 0;
	int8_t velocityY = 0;
};

typedef void (*RadarProgressCallback)(void* userContext, const RadarProgress& progress);

class RadarEngine {
 public:
	static constexpr size_t kMinFrameCount = 1;
	static constexpr size_t kMaxFrameCount = 12;
	static constexpr size_t kMaxStormCells = 8;

	RadarEngine();
	~RadarEngine();

	void begin();
	void reset();
	void setProgressCallback(RadarProgressCallback callback, void* userContext);
	void setAnimationFps(float fps);
	float animationFps() const;
	void setVisualConfig(const RadarVisualConfig& config);
	void setReflectivityMode(ReflectivityMode mode);
	const RadarVisualConfig& visualConfig() const;
	void setStormCells(const RadarStormCell* cells, size_t count);
	size_t stormCellCount() const;
	ReflectivityMode reflectivityMode() const;
	const char* reflectivityModeLabel() const;
	bool isInterpolating() const;
	bool hasDisplayEffects() const;

	bool startDownload(const String* tileUrls,
										 const uint32_t* frameEpochs,
										 size_t frameCount,
										 const RadarDownloadConfig& config);

	void tick();

	bool isDownloading() const;
	bool isAnimationReady() const;

	RadarEngineError lastError() const;
	int lastHttpStatus() const;
	String lastErrorMessage() const;

	size_t frameCount() const;
	size_t completedFrameCount() const;
	size_t currentAnimationIndex() const;
	uint32_t displayRevision() const;
	const RadarFrameInfo* frameInfo(size_t index) const;

	const lv_img_dsc_t* currentFrameAsLvglImage();
	bool getCurrentFrameRaw(const uint8_t*& data, size_t& length, RadarFrameFormat& format);

 private:
	struct FrameSlot {
		RadarFrameInfo info;
		String url;
		uint8_t* ramData = nullptr;
		size_t ramLength = 0;
		RadarStormCell autoStormCells[kMaxStormCells]{};
		size_t autoStormCellCount = 0;
		lv_img_dsc_t dsc;
		bool dscValid = false;
	};

	enum class DownloadState : uint8_t {
		Idle = 0,
		Connect,
		Waiting,
		FinalizeFrame,
		Complete,
		Error,
	};

	struct UrlParts {
		bool secure = false;
		String host;
		uint16_t port = 80;
		String path;
	};

	struct HttpJob {
		~HttpJob();
		uint32_t generation = 0;
		String url;
		RadarDownloadConfig config;
		uint8_t* data = nullptr;
		size_t length = 0;
		String contentType;
		RadarFrameFormat format = RadarFrameFormat::Unknown;
		uint16_t width = 0;
		uint16_t height = 0;
		RadarStormCell autoStormCells[kMaxStormCells]{};
		size_t autoStormCellCount = 0;
		uint8_t* mapData = nullptr;
		size_t mapLength = 0;
		RadarEngineError error = RadarEngineError::None;
		String message;
		int httpStatus = 0;
	};

	void resetAll();
	void clearFrame(FrameSlot& slot);
	void clearFrames();
	void transitionToError(RadarEngineError code, const String& message, int httpStatus = 0);
	void emitProgress(const char* stage);

	static bool parseUrl(const String& url, UrlParts& out);
	bool initializeHttpWorker();
	static void httpWorkerTask(void* context);
	void runHttpWorker();
	static void prepareDownloadedFrame(HttpJob& job);
	void pumpDownload();
	void finalizeFrame();
	void advanceAnimation();
	bool ensureFrameResident(FrameSlot& slot);
	bool ensureDisplayBuffer(size_t length);
	bool buildDisplayFrame(FrameSlot& current, FrameSlot* next, uint8_t blendStep, uint8_t blendSteps);
	static bool fetchUrlToBuffer(const String& url, const RadarDownloadConfig& config,
		uint8_t*& outData, size_t& outLength, String& outContentType,
		RadarEngineError& error, String& message, int& httpStatus);
	static bool decodePngFrameToRgb565(const uint8_t* sourceData,
									 size_t sourceLength,
									 uint32_t backgroundColorRgb888,
									 uint8_t*& decodedData,
									 size_t& decodedLength,
									 uint16_t& width,
									 uint16_t& height,
									 RadarEngineError& error, String& message);
	bool applyStaticPostProcess(uint8_t* data, size_t length, RadarFrameFormat format, uint16_t width, uint16_t height);
	void applyStormCellOverlay(uint8_t* data, size_t length, RadarFrameFormat format, uint16_t width, uint16_t height, const RadarStormCell* cells, size_t count);
	static void detectStormCells(const uint8_t* data, size_t length, RadarFrameFormat format, uint16_t width, uint16_t height, RadarStormCell* outCells, size_t& outCount);
	void invalidateDescriptors();

	bool ensureSpiffs();
	bool commitActiveDataToStorage();
	bool prepareLvglDescriptor(FrameSlot& slot);

	RadarProgressCallback progressCallback_ = nullptr;
	void* progressUserContext_ = nullptr;

	RadarDownloadConfig config_;
	FrameSlot frames_[kMaxFrameCount];

	size_t frameCount_ = 0;
	size_t completedFrameCount_ = 0;
	size_t currentDownloadIndex_ = 0;
	size_t currentAnimationIndex_ = 0;

	DownloadState state_ = DownloadState::Idle;
	RadarEngineError lastError_ = RadarEngineError::None;
	int lastHttpStatus_ = 0;
	String lastErrorMessage_;

	bool spiffsReady_ = false;
	bool downloadStarted_ = false;

	RadarFrameFormat activeFormat_ = RadarFrameFormat::Unknown;
	uint16_t activeWidth_ = 0;
	uint16_t activeHeight_ = 0;

	uint8_t* activeTempBuffer_ = nullptr;
	size_t activeTempLength_ = 0;

	uint32_t lastAnimationStepMs_ = 0;
	uint32_t lastDownloadPumpMs_ = 0;
	float animationFps_ = 8.0f;
	RadarVisualConfig visualConfig_{};
	RadarStormCell stormCells_[kMaxStormCells]{};
	size_t stormCellCount_ = 0;
	uint8_t* displayBuffer_ = nullptr;
	size_t displayBufferLength_ = 0;
	lv_img_dsc_t displayDsc;
	uint8_t interpolationStep_ = 0;
	uint32_t displayRevision_ = 0;

	QueueHandle_t requestQueue_ = nullptr;
	QueueHandle_t responseQueue_ = nullptr;
	SemaphoreHandle_t workerStopped_ = nullptr;
	TaskHandle_t workerTask_ = nullptr;
	bool requestInFlight_ = false;
	uint32_t downloadGeneration_ = 0;
};

}  // namespace weather
