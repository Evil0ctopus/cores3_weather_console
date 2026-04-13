#include "debug_log.h"

namespace app {

void DebugLog::begin(bool enabled) {
	setEnabled(enabled);
}

void DebugLog::setEnabled(bool enabled) {
	if (enabled_ == enabled) {
		return;
	}
	enabled_ = enabled;
	if (enabled_) {
		log("system", "debug mode enabled");
	} else {
		log("system", "debug mode disabled");
	}
}

bool DebugLog::enabled() const {
	return enabled_;
}

void DebugLog::log(const char* category, const String& message) {
	if (category == nullptr || message.length() == 0) {
		return;
	}
	const uint32_t nowMs = millis();
	appendEntry(category, message, nowMs);
	if (!enabled_) {
		return;
	}
	Serial.printf("[DBG][%10lu][%s] %s\n", static_cast<unsigned long>(nowMs), category, message.c_str());
}

void DebugLog::clear() {
	entryCount_ = 0;
	nextIndex_ = 0;
	++revision_;
}

uint32_t DebugLog::revision() const {
	return revision_;
}

size_t DebugLog::count() const {
	return entryCount_;
}

void DebugLog::writeRecentJson(JsonDocument& doc, size_t maxEntries) const {
	JsonArray entries = doc["entries"].to<JsonArray>();
	const size_t capped = maxEntries > kMaxEntries ? kMaxEntries : maxEntries;
	const size_t take = entryCount_ < capped ? entryCount_ : capped;
	const size_t first = entryCount_ > take ? (entryCount_ - take) : 0;

	for (size_t order = first; order < entryCount_; ++order) {
		size_t physical = order;
		if (entryCount_ == kMaxEntries) {
			physical = (nextIndex_ + order) % kMaxEntries;
		}
		const Entry& entry = entries_[physical];
		JsonObject obj = entries.add<JsonObject>();
		obj["ms"] = entry.ms;
		obj["category"] = entry.category;
		obj["message"] = entry.message;
	}
	doc["enabled"] = enabled_;
	doc["revision"] = revision_;
	doc["count"] = entryCount_;
}

String DebugLog::overlayText(size_t maxLines) const {
	if (!enabled_ || entryCount_ == 0 || maxLines == 0) {
		return String();
	}
	const size_t take = entryCount_ < maxLines ? entryCount_ : maxLines;
	const size_t first = entryCount_ - take;
	String out;
	for (size_t order = first; order < entryCount_; ++order) {
		size_t physical = order;
		if (entryCount_ == kMaxEntries) {
			physical = (nextIndex_ + order) % kMaxEntries;
		}
		const Entry& entry = entries_[physical];
		if (out.length() > 0) {
			out += "\n";
		}
		out += "[";
		out += entry.category;
		out += "] ";
		out += entry.message;
	}
	return out;
}

void DebugLog::appendEntry(const char* category, const String& message, uint32_t nowMs) {
	Entry next;
	next.ms = nowMs;
	next.category = category;
	next.message = message;

	if (entryCount_ < kMaxEntries) {
		entries_[entryCount_] = next;
		++entryCount_;
	} else {
		entries_[nextIndex_] = next;
		nextIndex_ = (nextIndex_ + 1) % kMaxEntries;
	}
	++revision_;
}

}  // namespace app
