#include "location_lookup.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <new>

namespace app {
namespace {
QueueHandle_t requests = nullptr;
QueueHandle_t responses = nullptr;
TaskHandle_t worker = nullptr;
bool busy = false;

void runLookup(void*) {
	for (;;) {
		LocationResult* result = nullptr;
		if (xQueueReceive(requests, &result, portMAX_DELAY) != pdTRUE) continue;
		if (!resolveWeatherLocation(result->apiKey, result->query, result->key, result->name, result->error) &&
				result->error.length() == 0) result->error = "Location lookup failed.";
		xQueueSend(responses, &result, portMAX_DELAY);
	}
}
}

bool locationLookupStart(const String& apiKey, const String& query, String& error) {
	if (busy) {
		error = "A location lookup is still running.";
		return false;
	}
	if (worker == nullptr) {
		requests = xQueueCreate(1, sizeof(LocationResult*));
		responses = xQueueCreate(1, sizeof(LocationResult*));
		if (requests == nullptr || responses == nullptr ||
				xTaskCreatePinnedToCore(runLookup, "location-lookup", 8192, nullptr, 1, &worker, 0) != pdPASS) {
			if (requests != nullptr) vQueueDelete(requests);
			if (responses != nullptr) vQueueDelete(responses);
			requests = nullptr;
			responses = nullptr;
			worker = nullptr;
			error = "Could not start location worker.";
			return false;
		}
	}
	LocationResult* request = new (std::nothrow) LocationResult();
	if (request == nullptr) {
		error = "Not enough memory for lookup.";
		return false;
	}
	request->query = query;
	request->apiKey = apiKey;
	if (xQueueSend(requests, &request, 0) != pdTRUE) {
		delete request;
		error = "Location request queue is full.";
		return false;
	}
	busy = true;
	return true;
}

bool locationLookupTake(LocationResult& result) {
	if (responses == nullptr) return false;
	LocationResult* response = nullptr;
	if (xQueueReceive(responses, &response, 0) != pdTRUE) return false;
	result = *response;
	delete response;
	busy = false;
	return true;
}

}  // namespace app
