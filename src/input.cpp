#include "input.hpp"

#define DEBUG_DISPATCH false

std::uint64_t ExtendedCCKeyboardDispatcher::g_lastTimestamp = 0ull;
cocos2d::CCEvent* ExtendedCCKeyboardDispatcher::g_currentEventInfo = nullptr;

void ExtendedCCKeyboardDispatcher::setTimestamp(std::uint64_t timestamp) {
	g_lastTimestamp = timestamp;
}

cocos2d::CCEvent* ExtendedCCKeyboardDispatcher::getCurrentEventInfo() {
	return g_currentEventInfo;
};

bool ExtendedCCKeyboardDispatcher::dispatchKeyboardMSG(cocos2d::enumKeyCodes key, bool isKeyDown, bool isKeyRepeat) {
#if DEBUG_DISPATCH
	geode::log::debug("dispatch keyboard event with time {}", g_lastTimestamp);
#endif

	g_currentEventInfo = new ExtendedCCEvent(g_lastTimestamp);

	auto r = CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat);

	delete g_currentEventInfo;
	g_currentEventInfo = nullptr;
	g_lastTimestamp = 0ull;

	return r;
}

std::uint64_t ExtendedCCTouchDispatcher::g_lastTimestamp = 0ull;

void ExtendedCCTouchDispatcher::setTimestamp(std::uint64_t timestamp) {
	g_lastTimestamp = timestamp;
}

void ExtendedCCTouchDispatcher::touches(cocos2d::CCSet* touches, cocos2d::CCEvent* event, unsigned int index) {
	auto extEvent = new ExtendedCCEvent(g_lastTimestamp);
	CCTouchDispatcher::touches(touches, extEvent, index);
	g_lastTimestamp = 0ull;
}
