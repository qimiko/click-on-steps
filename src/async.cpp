#include "async.hpp"

#define DEBUG_PROCESSING false

void AsyncUILayer::handleKeypress(cocos2d::enumKeyCodes key, bool down) {
	auto event = ExtendedCCKeyboardDispatcher::getCurrentEventInfo();
	auto extendedInfo = static_cast<ExtendedCCEvent*>(event);
	m_fields->m_lastTimestamp = extendedInfo->getTimestamp();

	UILayer::handleKeypress(key, down);
	m_fields->m_lastTimestamp = 0ull;
}

bool AsyncUILayer::ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
	auto extendedInfo = static_cast<ExtendedCCEvent*>(event);
	m_fields->m_lastTimestamp = extendedInfo->getTimestamp();

	auto r = UILayer::ccTouchBegan(touch, event);
	m_fields->m_lastTimestamp = 0ull;

	return r;
}

void AsyncUILayer::ccTouchMoved(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
	auto extendedInfo = static_cast<ExtendedCCEvent*>(event);
	m_fields->m_lastTimestamp = extendedInfo->getTimestamp();

	UILayer::ccTouchMoved(touch, event);
	m_fields->m_lastTimestamp = 0ull;
}

void AsyncUILayer::ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
	auto extendedInfo = static_cast<ExtendedCCEvent*>(event);
	m_fields->m_lastTimestamp = extendedInfo->getTimestamp();

	UILayer::ccTouchEnded(touch, event);
	m_fields->m_lastTimestamp = 0ull;
}

void AsyncUILayer::ccTouchCancelled(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
	auto extendedInfo = static_cast<ExtendedCCEvent*>(event);
	m_fields->m_lastTimestamp = extendedInfo->getTimestamp();

	UILayer::ccTouchCancelled(touch, event);
	m_fields->m_lastTimestamp = 0ull;
}

std::uint64_t AsyncUILayer::getLastTimestamp() {
	return m_fields->m_lastTimestamp;
}