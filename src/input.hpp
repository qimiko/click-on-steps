#pragma once

#include <Geode/Geode.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/CCTouchDispatcher.hpp>

#include <cstdint>
#include <unordered_set>

// returns the current time in ms, relative to whatever the system uses for input events
// (usually system boot)
std::uint64_t platform_get_time();

class ExtendedCCEvent : public cocos2d::CCEvent {
	std::uint64_t m_timestamp;

public:
	ExtendedCCEvent(std::uint64_t timestamp) : m_timestamp(timestamp) {}

	std::uint64_t getTimestamp() const {
		return m_timestamp;
	}
};

struct ExtendedCCKeyboardDispatcher : geode::Modify<ExtendedCCKeyboardDispatcher, cocos2d::CCKeyboardDispatcher> {
	// there's only one keyboard dispatcher ever and we can't really add new fields
	static std::uint64_t g_lastTimestamp;
	static void setTimestamp(std::uint64_t);

	// this part is a little not ideal, but it's very hard to add a new parameter to the game
	// (unless you were robtop, then this custom delegate thing wouldn't be necessary)
	static cocos2d::CCEvent* g_currentEventInfo;
	static cocos2d::CCEvent* getCurrentEventInfo();

	static void onModify(auto& self) {
		// custom keybinds compat
		(void)self.setHookPriority("cocos2d::CCKeyboardDispatcher::dispatchKeyboardMSG", -1000);
	}

	bool dispatchKeyboardMSG(cocos2d::enumKeyCodes key, bool isKeyDown, bool isKeyRepeat);
};

// all this has to do is add the timestamp to the event
struct ExtendedCCTouchDispatcher : geode::Modify<ExtendedCCTouchDispatcher, cocos2d::CCTouchDispatcher> {
	static std::uint64_t g_lastTimestamp;
	static void setTimestamp(std::uint64_t);

	static void onModify(auto& self) {
		// future proofing, ig
		(void)self.setHookPriority("cocos2d::CCTouchDispatcher::touches", -1000);
	}

	void touches(cocos2d::CCSet* pTouches, cocos2d::CCEvent* pEvent, unsigned int uIndex);
};
