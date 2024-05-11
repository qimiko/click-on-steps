#pragma once

#include <Geode/Geode.hpp>
#include <Geode/modify/UILayer.hpp>

#include <cstdint>

#include "input.hpp"

// this mostly focuses on passing the inputs (with timestamps) to the gamelayer underneath
// (we have to pull the timestamp from whatever method used to add timestamps to events)

struct AsyncUILayer : geode::Modify<AsyncUILayer, UILayer> {
	struct Fields {
		std::uint64_t m_lastTimestamp{0ull};
	};

	void handleKeypress(cocos2d::enumKeyCodes, bool);
	bool ccTouchBegan(cocos2d::CCTouch*, cocos2d::CCEvent*);
	void ccTouchMoved(cocos2d::CCTouch*, cocos2d::CCEvent*);
	void ccTouchEnded(cocos2d::CCTouch*, cocos2d::CCEvent*);
	void ccTouchCancelled(cocos2d::CCTouch*, cocos2d::CCEvent*);

	std::uint64_t getLastTimestamp();
};
