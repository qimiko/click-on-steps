#include "main.hpp"
#include "async.hpp"
#include "input.hpp"

#define DEBUG_STEPS false

#if DEBUG_STEPS
// this one's me being lazy
template <typename T>
inline T* ptr_to_offset(void* base, unsigned int offset) {
	return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(base) + offset);
};

template <typename T>
inline T get_from_offset(void* base, unsigned int offset) {
	return *ptr_to_offset<T>(base, offset);
};
#endif

// get timestamp, but with custom keybinds (or similar mods) compatibility
std::uint64_t getTimestampCompat() {
	auto event = ExtendedCCKeyboardDispatcher::getCurrentEventInfo();
	if (!event) {
		return 0;
	}

	auto extendedInfo = static_cast<ExtendedCCEvent*>(event);
	return extendedInfo->getTimestamp();
}

#ifdef GEODE_IS_WINDOWS
void CustomGJBaseGameLayer::queueButton_custom(int btnType, bool push, bool secondPlayer) {
#else
void CustomGJBaseGameLayer::queueButton(int btnType, bool push, bool secondPlayer) {
#endif
	// this is another workaround for it not being very easy to pass arguments to things
	// oh well, ig

	auto& fields = this->m_fields;

#ifdef GEODE_IS_ANDROID
	// if the player is currently in a frame, just insert the input directly
	// we do this to support android, which has an unstable gd::vector::push_back
	// also, this breaks windows bc we hook vector enqueue which tries to return back to here... yay
	if (fields->m_inFrame) {
		return GJBaseGameLayer::queueButton(btnType, push, secondPlayer);
	}
#endif

	auto inputTimestamp = static_cast<AsyncUILayer*>(this->m_uiLayer)->getLastTimestamp();
	auto timeRelativeBegin = fields->m_timeBeginMs;

	if (!inputTimestamp) {
		inputTimestamp = getTimestampCompat();
	}

	auto currentTime = inputTimestamp - timeRelativeBegin;
	if (inputTimestamp < timeRelativeBegin || !inputTimestamp || !timeRelativeBegin) {
		// holding at the start can queue a button before time is initialized
		currentTime = 0;
	}

	// if you felt like it, you could calculate the step too
	// i personally don't. this maintains compatibility with physics bypass

	auto inputOffset = fields->m_inputOffset;
	if (fields->m_inputOffsetRand != 0) {
		auto offsetMax = fields->m_inputOffsetRand;
		auto randValue = static_cast<double>(rand()) / static_cast<double>(RAND_MAX);
		auto randOffset = static_cast<std::int32_t>((offsetMax * 2) * randValue - offsetMax);
		inputOffset += randOffset;
	}

	// if addition would cause a negative time, just reset it to zero
	if (inputOffset < 0 && -inputOffset > currentTime) {
		currentTime = 0;
	} else {
		currentTime += inputOffset;
	}

#if DEBUG_STEPS
	geode::log::debug("queueing input type={} down={} p2={} at time {} (ts {} -> {})", btnType, push, secondPlayer, currentTime, timeRelativeBegin, inputTimestamp);
#endif

	fields->m_timedCommands.push({
		static_cast<PlayerButton>(btnType),
		push,
		secondPlayer,
		// currentTime shouldn't overflow unless you have one frame every month
		static_cast<std::int32_t>(currentTime)
	});
}

void CustomGJBaseGameLayer::resetLevelVariables() {
	GJBaseGameLayer::resetLevelVariables();

	auto& fields = m_fields;
	fields->m_timeBeginMs = 0;
	fields->m_timeOffset = 0.0;
	fields->m_timedCommands = {};
	fields->m_disableInputCutoff = geode::Mod::get()->getSettingValue<bool>("late-input-cutoff");
	fields->m_inputOffset = static_cast<std::int32_t>(geode::Mod::get()->getSettingValue<std::int64_t>("input-offset"));
	fields->m_inputOffsetRand = static_cast<std::int32_t>(geode::Mod::get()->getSettingValue<std::int64_t>("input-offset-rand"));
}

void CustomGJBaseGameLayer::processTimedInputs() {
	// if you calculated steps upfront, you could also just rewrite processQueuedButtons to stop after it handles the current step
	// not done here as processQueuedButtons is inlined on macos :(

	// calculate the current time offset (in ms) that we stop handling inputs at
	auto& fields = m_fields;

	auto timeMs = static_cast<std::uint64_t>(fields->m_timeOffset * 1000.0);

	auto& commands = fields->m_timedCommands;
	if (!commands.empty()) {
		auto nextTime = commands.top().m_step;

#if DEBUG_STEPS
		geode::log::debug("step info: time={}, waiting for {}", timeMs, nextTime);
#endif

		while (!commands.empty() && nextTime <= timeMs) {
			auto btn = commands.top();
			commands.pop();

#if DEBUG_STEPS
			geode::log::debug("queuedInput: btn={} push={} p2={} timeMs={}",
				static_cast<int>(btn.m_button), btn.m_isPush, btn.m_isPlayer2, btn.m_step
			);
#endif

			// in this case, we push our handled inputs into the queue for the game to handle afterwards
			// again, unnecessary if you could rewrite processQueuedButtons
#ifdef GEODE_IS_ANDROID
			queueButton(static_cast<int>(btn.m_button), btn.m_isPush, btn.m_isPlayer2);
#else
			this->m_queuedButtons.push_back(btn);
#endif

			if (!commands.empty()) {
				nextTime = commands.top().m_step;
			}
		}
	}
}

void CustomGJBaseGameLayer::updateInputQueue() {
	// failsafe, but with late input cutoff disabled, so we take each command and reset its time
	auto& fields = this->m_fields;
	auto& commands = fields->m_timedCommands;
	auto timeMs = static_cast<std::int32_t>(fields->m_timeOffset * 1000.0);

	PlayerButtonCommandQueue newCommands{};

	while (!commands.empty()) {
		auto btn = commands.top();
		commands.pop();

		auto currentStep = btn.m_step;
		btn.m_step = std::max(btn.m_step - timeMs, 0);

		newCommands.push(btn);
	}

	commands.swap(newCommands);
}

void CustomGJBaseGameLayer::dumpInputQueue() {
	// failsafe, if an input hasn't been processed then we'll force it to be processed by the next frame
	auto& fields = this->m_fields;
	auto& commands = fields->m_timedCommands;

	while (!commands.empty()) {
		auto btn = commands.top();
		commands.pop();

#if DEBUG_STEPS
		geode::log::debug("failsafe queuedInput: btn={} push={} p2={} timeMs={}",
			static_cast<int>(btn.m_button), btn.m_isPush, btn.m_isPlayer2, btn.m_step
		);
#endif

#ifdef GEODE_IS_ANDROID
		queueButton(static_cast<int>(btn.m_button), btn.m_isPush, btn.m_isPlayer2);
#else
		this->m_queuedButtons.push_back(btn);
#endif
	}
}

void CustomGJBaseGameLayer::update(float dt) {
	auto& fields = m_fields;
	fields->m_timeBeginMs = platform_get_time();
	fields->m_timeOffset = 0.0;

	fields->m_inFrame = true;

	GJBaseGameLayer::update(dt);

	if (fields->m_disableInputCutoff) {
		updateInputQueue();
	} else {
		dumpInputQueue();
	}

	fields->m_inFrame = false;
}

void CustomGJBaseGameLayer::processCommands(float timeStep) {
	auto timeWarp = m_gameState.m_timeWarp;
	m_fields->m_timeOffset += timeStep / timeWarp;

	processTimedInputs();

	GJBaseGameLayer::processCommands(timeStep);
}

void CustomGJBaseGameLayer::fixUntimedInputs() {
#ifdef GEODE_IS_WINDOWS
	// windows workaround as queueButton and its vector insert is supposedly inlined everywhere!!
	// as long as this is called before the timestamp is cleared, it should work
	for (const auto& btn : this->m_queuedButtons) {
		this->queueButton_custom(static_cast<int>(btn.m_button), btn.m_isPush, btn.m_isPlayer2);
	}

	this->m_queuedButtons.clear();
#endif
}

