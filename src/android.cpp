#include <Geode/loader/Dispatch.hpp>
#include <time.h>

#include "input.hpp"

std::uint64_t platform_get_time() {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (now.tv_sec * 1000) + (now.tv_nsec / 1'000'000);
}

$execute {
	using namespace geode;

	// TODO: stabilze some sort of input timestamp api
	new EventListener(+[](std::uint64_t timestamp) {
		ExtendedCCKeyboardDispatcher::setTimestamp(timestamp);
		ExtendedCCTouchDispatcher::setTimestamp(timestamp);

		return ListenerResult::Propagate;
	}, DispatchFilter<std::uint64_t>("geode.loader/android-next-input-timestamp"));
}
