#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

namespace app {

class DebugLog {
 public:
	struct Entry {
		uint32_t ms = 0;
		String category;
		String message;
	};

	static constexpr size_t kMaxEntries = 64;

	void begin(bool enabled = false);
	void setEnabled(bool enabled);
	bool enabled() const;

	void log(const char* category, const String& message);
	void clear();

	uint32_t revision() const;
	size_t count() const;

	void writeRecentJson(JsonDocument& doc, size_t maxEntries = 40) const;
	String overlayText(size_t maxLines = 6) const;

 private:
	void appendEntry(const char* category, const String& message, uint32_t nowMs);

	Entry entries_[kMaxEntries];
	size_t entryCount_ = 0;
	size_t nextIndex_ = 0;
	uint32_t revision_ = 0;
	bool enabled_ = false;
};

}  // namespace app
