#pragma once

#include <Geode/Geode.hpp>

#include <Geode/modify/GJBaseGameLayer.hpp>

#include <queue>
#include <vector>

struct PlayerButtonCommandCompare {
	constexpr bool operator()(const PlayerButtonCommand& a, const PlayerButtonCommand& b) {
		return a.m_step > b.m_step;
	}
};

using PlayerButtonCommandQueue = std::priority_queue<PlayerButtonCommand, std::vector<PlayerButtonCommand>, PlayerButtonCommandCompare>;

struct CustomGJBaseGameLayer : geode::Modify<CustomGJBaseGameLayer, GJBaseGameLayer> {
	struct Fields {
		// stores the time of the last frame
		// all input time calculations are based off this interval
		std::uint64_t m_timeBeginMs{0ull};

		double m_timeOffset{0.0};

		// store timed commands separately from the typical input queue
		// they will instead be added at the correct timestamp
		PlayerButtonCommandQueue m_timedCommands{};

		bool m_disableInputCutoff{};
		std::int32_t m_inputOffset{};
		std::int32_t m_inputOffsetRand{};

		// there's probably a variable for this, but i'm lazy
		bool m_inFrame{};
	};

	void update(float dt);

#ifdef GEODE_IS_WINDOWS
	// lovely inlined function error
	void queueButton_custom(int btnType, bool push, bool secondPlayer);
#else
	void queueButton(int btnType, bool push, bool secondPlayer);
#endif
	void resetLevelVariables();
	void processCommands(float timeStep);

	void processTimedInputs();

	void updateInputQueue();
	void dumpInputQueue();

	// windows workaround
	void fixUntimedInputs();
};


