#include "async.hpp"

#define DEBUG_PROCESSING false

void AsyncUILayer::handleKeypress(cocos2d::enumKeyCodes key, bool down) {
	auto event = ExtendedCCKeyboardDispatcher::getCurrentEventInfo();
	if (!event) {
		return UILayer::handleKeypress(key, down);
	}

	auto extendedInfo = static_cast<ExtendedCCEvent*>(event);
	m_fields->m_lastTimestamp = extendedInfo->getTimestamp();

	UILayer::handleKeypress(key, down);
	m_fields->m_lastTimestamp = 0ull;
}

bool AsyncUILayer::ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
	if (!event) {
		return UILayer::ccTouchBegan(touch, event);
	}

	auto extendedInfo = static_cast<ExtendedCCEvent*>(event);
	m_fields->m_lastTimestamp = extendedInfo->getTimestamp();

	auto r = UILayer::ccTouchBegan(touch, event);
	m_fields->m_lastTimestamp = 0ull;

	return r;
}

void AsyncUILayer::ccTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
	if (!event) {
		return UILayer::ccTouchMoved(touch, event);
	}

	auto extendedInfo = static_cast<ExtendedCCEvent*>(event);
	m_fields->m_lastTimestamp = extendedInfo->getTimestamp();

	UILayer::ccTouchMoved(touch, event);
	m_fields->m_lastTimestamp = 0ull;
}

void AsyncUILayer::ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
	if (!event) {
		return UILayer::ccTouchEnded(touch, event);
	}

	auto extendedInfo = static_cast<ExtendedCCEvent*>(event);
	m_fields->m_lastTimestamp = extendedInfo->getTimestamp();

	UILayer::ccTouchEnded(touch, event);
	m_fields->m_lastTimestamp = 0ull;
}

#ifndef GEODE_IS_WINDOWS
void AsyncUILayer::ccTouchCancelled(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
	if (!event) {
		return UILayer::ccTouchCancelled(touch, event);
	}

	auto extendedInfo = static_cast<ExtendedCCEvent*>(event);
	m_fields->m_lastTimestamp = extendedInfo->getTimestamp();

	UILayer::ccTouchCancelled(touch, event);
	m_fields->m_lastTimestamp = 0ull;
}
#endif

std::uint64_t AsyncUILayer::getLastTimestamp() {
	return m_fields->m_lastTimestamp;
}
