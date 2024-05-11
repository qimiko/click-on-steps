#include "input.hpp"

#define DEBUG_DISPATCH false

std::unordered_set<ExtendedKeyboardDelegate*> ExtendedCCKeyboardDispatcher::g_extendedInputDelegates{};
std::uint64_t ExtendedCCKeyboardDispatcher::g_lastTimestamp = 0ull;

void ExtendedCCKeyboardDispatcher::addDelegate(ExtendedKeyboardDelegate* delegate) {
	g_extendedInputDelegates.insert(delegate);
}

void ExtendedCCKeyboardDispatcher::removeDelegate(ExtendedKeyboardDelegate* delegate) {
	g_extendedInputDelegates.erase(delegate);
}

void ExtendedCCKeyboardDispatcher::setTimestamp(std::uint64_t timestamp) {
	g_lastTimestamp = timestamp;
}

bool ExtendedCCKeyboardDispatcher::dispatchKeyboardMSG(cocos2d::enumKeyCodes key, bool isKeyDown, bool isKeyRepeat) {
	if (!CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat)) {
		return false;
	}

	// the message was dispatched, so continue to pass it along

	// convertKeyCode (inlined on macOS, so rewritten here)
	switch (key) {
		case cocos2d::KEY_ArrowUp:
			key = cocos2d::KEY_Up;
			break;
		case cocos2d::KEY_ArrowDown:
			key = cocos2d::KEY_Down;
			break;
		case cocos2d::KEY_ArrowLeft:
			key = cocos2d::KEY_Left;
			break;
		case cocos2d::KEY_ArrowRight:
			key = cocos2d::KEY_Right;
			break;
		default: break;
	}

	auto event = new ExtendedCCEvent(g_lastTimestamp);

	for (const auto& delegate : g_extendedInputDelegates) {
		if (isKeyDown) {
			delegate->extendedKeyDown(key, event);
		} else {
			delegate->extendedKeyUp(key, event);
		}
	}

	g_lastTimestamp = 0ull;
	return true;
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
