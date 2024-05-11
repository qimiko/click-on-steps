#pragma once

#include <Geode/Geode.hpp>
#include <Geode/modify/UILayer.hpp>

#include <cstdint>

#include "input.hpp"

// this mostly focuses on passing the inputs (with timestamps) to the gamelayer underneath
// (we have to pull the timestamp from whatever method used to add timestamps to events)

struct AsyncUILayer : geode::Modify<AsyncUILayer, UILayer> {
	struct Fields : ExtendedKeyboardDelegate {
		AsyncUILayer* m_self{nullptr};
		std::uint64_t m_lastTimestamp{0ull};
		bool m_hasRecvInput{false};

		~Fields();
		virtual void extendedKeyDown(cocos2d::enumKeyCodes key, cocos2d::CCEvent* eventInfo) override;
		virtual void extendedKeyUp(cocos2d::enumKeyCodes key, cocos2d::CCEvent* eventInfo) override;
	};

	bool init(GJBaseGameLayer* gameLayer);
	void handleKeypress(cocos2d::enumKeyCodes, bool);
	bool ccTouchBegan(cocos2d::CCTouch*, cocos2d::CCEvent*);
	void ccTouchMoved(cocos2d::CCTouch*, cocos2d::CCEvent*);
	void ccTouchEnded(cocos2d::CCTouch*, cocos2d::CCEvent*);
	void ccTouchCancelled(cocos2d::CCTouch*, cocos2d::CCEvent*);

	std::uint64_t getLastTimestamp();
};
