#include "async.hpp"

#define DEBUG_PROCESSING false

AsyncUILayer::Fields::~Fields() {
	ExtendedCCKeyboardDispatcher::removeDelegate(this);
}

void AsyncUILayer::Fields::extendedKeyDown(cocos2d::enumKeyCodes key, cocos2d::CCEvent* eventInfo) {
	// this is a workaround for the game calling onExit/onEnter for pausing to disable events
	// as we can't replace random virtuals to remove our delegate... lol
	// just another side effect of having to make a new delegate to handle this :(
	if (!m_hasRecvInput) {
		return;
	}

	m_hasRecvInput = false;

	auto extendedInfo = static_cast<ExtendedCCEvent*>(eventInfo);
	m_lastTimestamp = extendedInfo->getTimestamp();

	m_self->keyDown(key);
	m_lastTimestamp = 0;
}

void AsyncUILayer::Fields::extendedKeyUp(cocos2d::enumKeyCodes key, cocos2d::CCEvent* eventInfo) {
	if (!m_hasRecvInput) {
		return;
	}

	m_hasRecvInput = false;

	auto extendedInfo = static_cast<ExtendedCCEvent*>(eventInfo);
	m_lastTimestamp = extendedInfo->getTimestamp();

	m_self->keyUp(key);
	m_lastTimestamp = 0;
}

bool AsyncUILayer::init(GJBaseGameLayer* gameLayer) {
	m_fields->m_self = this;
	ExtendedCCKeyboardDispatcher::addDelegate(m_fields.self());
	return UILayer::init(gameLayer);
}

void AsyncUILayer::handleKeypress(cocos2d::enumKeyCodes key, bool down) {
	// check if it came from the normal callback, which we're ignoring
	// there's probably a better way to do this but i'm lazy
	if (!m_fields->m_lastTimestamp) {
		m_fields->m_hasRecvInput = true;
		return;
	}

	UILayer::handleKeypress(key, down);
}

bool AsyncUILayer::ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) {
	auto extendedInfo = static_cast<ExtendedCCEvent*>(event);
	m_fields->m_lastTimestamp = extendedInfo->getTimestamp();

	if (!UILayer::ccTouchBegan(touch, event)) {
		m_fields->m_lastTimestamp = 0ull;
		return false;
	}

	m_fields->m_lastTimestamp = 0ull;
	return true;
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