#include <Geode/Geode.hpp>
#include <Geode/modify/CCApplication.hpp>
#include <Geode/modify/CCEGLView.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/loader/SettingEvent.hpp>

#include <Windows.h>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <cmath>

#include "input.hpp"

// time code by mat. thank you mat
std::int64_t query_performance_counter() {
	LARGE_INTEGER result;
	QueryPerformanceCounter(&result);
	return result.QuadPart;
}

std::int64_t query_performance_frequency() {
	static auto freq = [] {
		LARGE_INTEGER result;
		QueryPerformanceFrequency(&result);
		return result.QuadPart;
	}();
	return freq;
}

std::uint64_t platform_get_time() {
	return query_performance_counter() * 1000 / query_performance_frequency();
}

// windows is unfortunately a very outdated platform and requires us to track inputs ourselves
// so um, enjoy this code to track inputs ourselves. i suppose

void (__thiscall *updateControllerState)(CXBOXController*);
void (*ptr_glfwMakeContextCurrent)(GLFWwindow*);
void (*ptr_glfwCustomAdjustWindowSize)(GLFWwindow*, int width, int height);
void (*ptr_glfwGetWindowSize)(GLFWwindow*, int* width, int* height);
HWND (*ptr_glfwGetWin32Window)(GLFWwindow*);

// my reimplementation of glfwPostEmptyEvent();
void postEmptyEvent(GLFWwindow* window) {
	auto hwnd = ptr_glfwGetWin32Window(window);
	PostMessage(hwnd, WM_NULL, 0, 0);
}

LARGE_INTEGER* s_nTimeElapsed = nullptr;
int* s_iFrameCounter = nullptr;
float* s_fFrameTime = nullptr;
std::atomic_uint8_t g_inputLoopType = 0;
bool g_runSingleThreaded = false;
bool g_singleThreadedInInputPoll = false;

class GameEvent {
protected:
	std::uint64_t timestamp;

public:
	GameEvent() {
		timestamp = platform_get_time();
	}

	virtual ~GameEvent() = default;
	virtual void dispatch() = 0;
};

struct CustomCCEGLView : geode::Modify<CustomCCEGLView, cocos2d::CCEGLView> {
	static void onModify(auto& self) {
		// run togglefullscreen last so mods that use it to set context can do that
		(void)self.setHookPriority("cocos2d::CCEGLView::toggleFullScreen", 1000);
	}

	static std::queue<GameEvent*> g_events;
	static std::mutex g_eventMutex;
	static CustomCCEGLView* g_self;
	static DWORD g_mainThreadId;
	static DWORD g_renderingThreadId;
	static std::condition_variable g_fullscreenVariable;
	static std::mutex g_fullscreenMutex;
	static bool g_needsSwitchFullscreen;
	static bool g_switchToFullscreen;
	static bool g_switchToBorderless;

	static GLFWkeyfun g_keyCallback;
	static GLFWmousebuttonfun g_mouseCallback;
	static GLFWcursorposfun g_cursorPosCallback;
	static GLFWscrollfun g_scrollCallback;
	static GLFWcharfun g_charCallback;
	static GLFWwindowposfun g_windowPosCallback;
	static GLFWwindowsizefun g_windowSizeCallback;
	static GLFWwindowiconifyfun g_windowIconifyCallback;

	void queueEvent(GameEvent* event) {
		std::scoped_lock lock(g_eventMutex);
		g_events.push(event);
	}

	void dumpEventQueue() {
		// do as little as possible with the mutex locked
		std::queue<GameEvent*> currentEvents{};
		{
			std::scoped_lock lock(g_eventMutex);
			g_events.swap(currentEvents);
		}

		while (!currentEvents.empty()) {
			auto nextEvent = currentEvents.front();
			nextEvent->dispatch();

			currentEvents.pop();
			delete nextEvent;
		}
	}

	static void onCustomGLFWKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void onCustomGLFWMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	static void onCustomGLFWCursorPosCallback(GLFWwindow* window, double xPos, double yPos);
	static void onCustomGLFWScrollCallback(GLFWwindow* window, double xOffset, double yOffset);
	static void onCustomGLFWCharCallback(GLFWwindow* window, unsigned int codepoint);
	static void onCustomGLFWWindowPosCallback(GLFWwindow* window, int xPos, int yPos);
	static void onCustomGLFWWindowSizeCallback(GLFWwindow* window, int width, int height);
	static void onCustomGLFWWindowIconifyCallback(GLFWwindow* window, int status);

	void setupWindow(cocos2d::CCRect size) {
		CCEGLView::setupWindow(size);

		// override some of rob's event callbacks here
		auto window = this->m_pMainWindow;
		g_self = this;
		g_mouseCallback = reinterpret_cast<decltype(&glfwSetMouseButtonCallback)>(geode::base::getCocos() + 0xd3210)(window, &onCustomGLFWMouseButtonCallback);
		g_cursorPosCallback = reinterpret_cast<decltype(&glfwSetCursorPosCallback)>(geode::base::getCocos() + 0xd2f40)(window, &onCustomGLFWCursorPosCallback);
		g_scrollCallback = reinterpret_cast<decltype(&glfwSetScrollCallback)>(geode::base::getCocos() + 0xd3250)(window, &onCustomGLFWScrollCallback);
		g_charCallback = reinterpret_cast<decltype(&glfwSetCharCallback)>(geode::base::getCocos() + 0xd2ec0)(window, &onCustomGLFWCharCallback);
		g_keyCallback = reinterpret_cast<decltype(&glfwSetKeyCallback)>(geode::base::getCocos() + 0xd31d0)(window, &onCustomGLFWKeyCallback);
		g_windowPosCallback = reinterpret_cast<decltype(&glfwSetWindowPosCallback)>(geode::base::getCocos() + 0xd22f0)(window, &onCustomGLFWWindowPosCallback);
		// -- framebuffer size callback (should be safe)
		// this one calls some glfw methods and opengl methods so it needs a full rewrite 🥲
		g_windowSizeCallback = reinterpret_cast<decltype(&glfwSetWindowSizeCallback)>(geode::base::getCocos() + 0xd2380)(window, &onCustomGLFWWindowSizeCallback);
		// -- unknown callback (stub)
		// -- device change callback (should be safe)
		g_windowIconifyCallback = reinterpret_cast<decltype(&glfwSetWindowIconifyCallback)>(geode::base::getCocos() + 0xd22b0)(window, &onCustomGLFWWindowIconifyCallback);
		// -- cursor enter callback (should be safe)
	}

	void toggleFullScreen(bool fullscreen, bool borderless) {
		// this one's pretty good
		// what we must do:
		// - run setupwindow in the main thread
		// - unset the main thread's context (again)
		// - set the rendering thread's context back

		if (g_runSingleThreaded) {
			return CCEGLView::toggleFullScreen(fullscreen, borderless);
		}

		if (GetCurrentThreadId() != g_mainThreadId) {
			std::unique_lock fsLock(g_fullscreenMutex);
			g_needsSwitchFullscreen = true;
			g_switchToFullscreen = fullscreen;
			g_switchToBorderless = borderless;

			postEmptyEvent(this->m_pMainWindow);

			ptr_glfwMakeContextCurrent(nullptr);
			g_fullscreenVariable.wait(fsLock, []{ return !g_needsSwitchFullscreen; });
			ptr_glfwMakeContextCurrent(this->m_pMainWindow);

			fsLock.unlock();

			return;
		}

		ptr_glfwMakeContextCurrent(this->m_pMainWindow);
		CCEGLView::toggleFullScreen(fullscreen, borderless);
		ptr_glfwMakeContextCurrent(nullptr);
	}

	// remake of CCEGLViewProtocol::setFrameSize, for the resize callback. because CCEGLView::setFrameSize is scary and virtual
	void protocolSetFrameSize(float width, float height) {
		auto frameSize = cocos2d::CCSize(width, height);

		m_obDesignResolutionSize = frameSize;
		m_obScreenSize = frameSize;
	}

	void end() {
		// post empty just before the mainwindow gets set to null
		postEmptyEvent(this->m_pMainWindow);

		CCEGLView::end();
	}
};

std::queue<GameEvent*> CustomCCEGLView::g_events{};
std::mutex CustomCCEGLView::g_eventMutex{};
CustomCCEGLView* CustomCCEGLView::g_self = nullptr;
DWORD CustomCCEGLView::g_mainThreadId = 0;
DWORD CustomCCEGLView::g_renderingThreadId = 0;
std::condition_variable CustomCCEGLView::g_fullscreenVariable{};
std::mutex CustomCCEGLView::g_fullscreenMutex{};
bool CustomCCEGLView::g_needsSwitchFullscreen = false;
bool CustomCCEGLView::g_switchToFullscreen = false;
bool CustomCCEGLView::g_switchToBorderless = false;

GLFWkeyfun CustomCCEGLView::g_keyCallback = nullptr;
GLFWmousebuttonfun CustomCCEGLView::g_mouseCallback = nullptr;
GLFWcursorposfun CustomCCEGLView::g_cursorPosCallback = nullptr;
GLFWscrollfun CustomCCEGLView::g_scrollCallback = nullptr;
GLFWcharfun CustomCCEGLView::g_charCallback = nullptr;
GLFWwindowposfun CustomCCEGLView::g_windowPosCallback = nullptr;
GLFWwindowsizefun CustomCCEGLView::g_windowSizeCallback = nullptr;
GLFWwindowiconifyfun CustomCCEGLView::g_windowIconifyCallback = nullptr;

class KeyEvent : public GameEvent {
	GLFWwindow* window;
	int key;
	int scancode;
	int action;
	int mods;

public:
	virtual void dispatch() override {
		ExtendedCCKeyboardDispatcher::setTimestamp(timestamp);
		CustomCCEGLView::g_keyCallback(window, key, scancode, action, mods);
	}

	KeyEvent(GLFWwindow* window, int key, int scancode, int action, int mods)
		: window(window), key(key), scancode(scancode), action(action), mods(mods) {}
};

void CustomCCEGLView::onCustomGLFWKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	auto event = new KeyEvent(window, key, scancode, action, mods);
	g_self->queueEvent(event);
}

class MouseEvent : public GameEvent {
	GLFWwindow* window;
	int button;
	int action;
	int mods;

public:
	virtual void dispatch() override {
		ExtendedCCTouchDispatcher::setTimestamp(timestamp);
		CustomCCEGLView::g_mouseCallback(window, button, action, mods);
	}

	MouseEvent(GLFWwindow* window, int button, int action, int mods)
		: window(window), button(button), action(action), mods(mods) {}
};

void CustomCCEGLView::onCustomGLFWMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	auto event = new MouseEvent(window, button, action, mods);
	g_self->queueEvent(event);
}

class CursorPosEvent : public GameEvent {
	GLFWwindow* window;
	double xPos;
	double yPos;
public:
	virtual void dispatch() override {
		ExtendedCCTouchDispatcher::setTimestamp(timestamp);
		CustomCCEGLView::g_cursorPosCallback(window, xPos, yPos);
	}

	CursorPosEvent(GLFWwindow* window, double xPos, double yPos)
		: window(window), xPos(xPos), yPos(yPos) {}
};

void CustomCCEGLView::onCustomGLFWCursorPosCallback(GLFWwindow* window, double xPos, double yPos) {
	auto event = new CursorPosEvent(window, xPos, yPos);
	g_self->queueEvent(event);
}

class ScrollEvent : public GameEvent {
	GLFWwindow* window;
	double xOffset;
	double yOffset;
public:
	virtual void dispatch() override {
		CustomCCEGLView::g_scrollCallback(window, xOffset, yOffset);
	}

	ScrollEvent(GLFWwindow* window, double xOffset, double yOffset)
		: window(window), xOffset(xOffset), yOffset(yOffset) {}
};

void CustomCCEGLView::onCustomGLFWScrollCallback(GLFWwindow* window, double xOffset, double yOffset) {
	auto event = new ScrollEvent(window, xOffset, yOffset);
	g_self->queueEvent(event);
}

class CharEvent : public GameEvent {
	GLFWwindow* window;
	unsigned int codepoint;
public:
	virtual void dispatch() override {
		// this primarily goes into the imedispatcher, which doesn't have timestamps
		CustomCCEGLView::g_charCallback(window, codepoint);
	}

	CharEvent(GLFWwindow* window, unsigned int codepoint)
		: window(window), codepoint(codepoint) {}
};

void CustomCCEGLView::onCustomGLFWCharCallback(GLFWwindow* window, unsigned int codepoint) {
	auto event = new CharEvent(window, codepoint);
	g_self->queueEvent(event);
}

class WindowPosEvent : public GameEvent {
	GLFWwindow* window;
	int xPos;
	int yPos;
public:
	virtual void dispatch() override {
		CustomCCEGLView::g_windowPosCallback(window, xPos, yPos);
	}

	WindowPosEvent(GLFWwindow* window, int xPos, int yPos)
		: window(window), xPos(xPos), yPos(yPos) {}
};

void CustomCCEGLView::onCustomGLFWWindowPosCallback(GLFWwindow* window, int xPos, int yPos) {
	auto event = new WindowPosEvent(window, xPos, yPos);
	g_self->queueEvent(event);
}

class WindowSizeEvent : public GameEvent {
	GLFWwindow* window;
	int width;
	int height;
public:
	virtual void dispatch() override {
		CustomCCEGLView::g_self->protocolSetFrameSize(static_cast<float>(width), static_cast<float>(height));
		cocos2d::CCDirector::sharedDirector()->updateScreenScale({
			static_cast<float>(width),
			static_cast<float>(height)
		});
		cocos2d::CCDirector::sharedDirector()->setViewport();
		cocos2d::CCDirector::sharedDirector()->setProjection(cocos2d::kCCDirectorProjection2D);
	}

	WindowSizeEvent(GLFWwindow* window, int width, int height)
		: window(window), width(width), height(height) {}
};

void CustomCCEGLView::onCustomGLFWWindowSizeCallback(GLFWwindow* window, int width, int height) {
	ptr_glfwCustomAdjustWindowSize(window, width, height);
	ptr_glfwGetWindowSize(window, &width, &height);

	auto event = new WindowSizeEvent(window, width, height);
	g_self->queueEvent(event);
}

class IconifyEvent : public GameEvent {
	GLFWwindow* window;
	int status;
public:
	virtual void dispatch() override {
		CustomCCEGLView::g_windowIconifyCallback(window, status);
	}

	IconifyEvent(GLFWwindow* window, int status)
		: window(window), status(status) {}
};

void CustomCCEGLView::onCustomGLFWWindowIconifyCallback(GLFWwindow* window, int status) {
	auto event = new IconifyEvent(window, status);
	g_self->queueEvent(event);
}

// this event is primarily for controllers, if dispatchKeyboardMSG is called from the main thread
class ManualKeyboardEvent : public GameEvent {
	cocos2d::enumKeyCodes key;
	bool isDown;
	bool isRepeat;

public:
	virtual void dispatch() override {
		ExtendedCCKeyboardDispatcher::setTimestamp(timestamp);
		cocos2d::CCDirector::sharedDirector()->getKeyboardDispatcher()->dispatchKeyboardMSG(key, isDown, isRepeat);
	}

	ManualKeyboardEvent(cocos2d::enumKeyCodes key, bool isDown, bool isRepeat)
		: key(key), isDown(isDown), isRepeat(isRepeat) {}
};

struct AsyncCCKeyboardDispatcher : geode::Modify<AsyncCCKeyboardDispatcher, cocos2d::CCKeyboardDispatcher> {
	static void onModify(auto& self) {
		// this is the most likely to be called before by another mod and it would be really bad if that other mod got to it first
		(void)self.setHookPriority("cocos2d::CCKeyboardDispatcher::dispatchKeyboardMSG", -100000);
	}

	bool dispatchKeyboardMSG(cocos2d::enumKeyCodes key, bool isDown, bool isRepeat) {
		if (g_singleThreadedInInputPoll || GetCurrentThreadId() != CustomCCEGLView::g_renderingThreadId) {
			// queue our event
			auto event = new ManualKeyboardEvent(key, isDown, isRepeat);
			CustomCCEGLView::g_self->queueEvent(event);
			return true;
		}

		return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isDown, isRepeat);
	}
};

std::atomic_uint32_t g_inputTps{};
std::atomic_int64_t g_pollingRate = 1000;

struct CustomCCApplication : geode::Modify<CustomCCApplication, cocos2d::CCApplication> {
	static void onModify(auto& self) {
		(void)self.setHookPriority("cocos2d::CCApplication::run", 5000);
	}

	int runSingleThreaded() {
		this->setupGLView();

		auto director = cocos2d::CCDirector::sharedDirector();
		auto glView = director->m_pobOpenGLView;
		glView->retain();

		this->setupVerticalSync();
		this->updateVerticalSync();

		auto customGlView = static_cast<CustomCCEGLView*>(glView);
		CustomCCEGLView::g_mainThreadId = GetCurrentThreadId();
		CustomCCEGLView::g_renderingThreadId = GetCurrentThreadId();

		if (!this->applicationDidFinishLaunching()) {
			return 0;
		}

		auto isFullscreen = glView->m_bIsFullscreen;
		this->m_bFullscreen = isFullscreen;

		auto freq = query_performance_frequency();
		double dFreq = freq;

		std::int64_t currentInputPollingRate = g_pollingRate;
		double pollUpdateRate = 1.0 / static_cast<double>(currentInputPollingRate);

		auto pollInterval = static_cast<std::uint64_t>(freq * pollUpdateRate);

		auto inputFrames = 0u;
		auto inputTime = 0.0f;

		LARGE_INTEGER lastFrameTime;
		LARGE_INTEGER lastInputTime;
		QueryPerformanceCounter(&lastFrameTime);
		lastInputTime.QuadPart = lastFrameTime.QuadPart;
		*s_nTimeElapsed = lastFrameTime;

		while (true) {
			if (glView->windowShouldClose()) {
				if (!this->m_bShutdownCalled) {
					cocos2d::CCApplication::sharedApplication()->trySaveGame(false);
					// sm_pSharedApplication->trySaveGame(false);
				}

				if (glView->isOpenGLReady()) {
					director->end();

					// technically, this works, but i don't know why mainloop was protected in the first place
					reinterpret_cast<cocos2d::CCDisplayLinkDirector*>(director)->mainLoop();
				}

				glView->release();

				// unsure why, but calling these with sm_pSharedApplication crashes
				cocos2d::CCApplication::sharedApplication()->platformShutdown();
				// sm_pSharedApplication->platformShutdown();

				return 1;
			}

			if (this->m_bFullscreen != isFullscreen) {
				this->updateVerticalSync();
				this->m_bForceTimer = false;
			}
			isFullscreen = this->m_bFullscreen;

			LARGE_INTEGER currentTime;
			QueryPerformanceCounter(&currentTime);
			*s_nTimeElapsed = currentTime;

			if (currentTime.QuadPart - lastInputTime.QuadPart >= pollInterval) {
				g_singleThreadedInInputPoll = true;

				if (this->m_bUpdateController) {
					updateControllerState(this->m_pControllerHandler);
					updateControllerState(this->m_pController2Handler);
					this->m_bUpdateController = false;
				}

				auto player1Controller = this->m_pControllerHandler;
				auto player2Controller = this->m_pController2Handler;

				this->m_bControllerConnected = player1Controller->m_controllerConnected || player2Controller->m_controllerConnected;

				if (player1Controller->m_controllerConnected) {
					this->updateControllerKeys(player1Controller, 1);
				}

				if (player2Controller->m_controllerConnected) {
					this->updateControllerKeys(player2Controller, 2);
				}

				auto dt = static_cast<double>(currentTime.QuadPart - lastInputTime.QuadPart) / dFreq;

				inputFrames++;
				inputTime += dt;

				if (inputTime > 0.5f) {
					g_inputTps = std::round(inputFrames / inputTime);
					inputFrames = 0;
					inputTime = 0.0f;

					// recalculate interval here if necessary
					if (currentInputPollingRate != g_pollingRate) {
						currentInputPollingRate = g_pollingRate;
						pollUpdateRate = 1.0 / currentInputPollingRate;
						pollInterval = static_cast<std::uint64_t>(freq * pollUpdateRate);
					}
				}

				glView->pollEvents();

				g_singleThreadedInInputPoll = false;
				lastInputTime.QuadPart = currentTime.QuadPart;
			}

			auto updateRate = m_bVerticalSyncEnabled ? m_fVsyncInterval : m_fAnimationInterval;
			auto interval = m_bVerticalSyncEnabled ? m_nVsyncInterval : m_nAnimationInterval;

			auto useFrameCount = !isFullscreen && !this->m_bForceTimer;

			if (useFrameCount || currentTime.QuadPart - lastFrameTime.QuadPart >= interval.QuadPart) {
				// run update
				auto dCurrentTime = static_cast<double>(currentTime.QuadPart);
				auto dLastTime = static_cast<double>(lastFrameTime.QuadPart);
				auto dInterval = static_cast<double>(interval.QuadPart);
				auto dUpdateRate = static_cast<double>(updateRate);

				auto dt = ((dCurrentTime - dLastTime) / dInterval) * dUpdateRate;
				lastFrameTime.QuadPart = currentTime.QuadPart;

				if (useFrameCount) {
					auto currentFrameTime = *s_fFrameTime + dt;
					(*s_iFrameCounter)++;
					*s_fFrameTime = currentFrameTime;
					if (0.5 < currentFrameTime) {
						auto frameInterval = static_cast<float>(*s_iFrameCounter);
						*s_iFrameCounter = 0;
						*s_fFrameTime = 0.0f;

						auto targetFps = 1.0 / updateRate;
						auto currentFps = frameInterval / currentFrameTime;
						if ((targetFps * 1.2) < currentFps) {
							this->m_bForceTimer = true;
						}
					}
				}

				// i haven't actually figured out what happens here
				auto adjustedDeltaTime = dt;
				if (m_bSmoothFix && director->getSmoothFixCheck() && std::abs(dt - updateRate) <= updateRate * 0.1f) {
					adjustedDeltaTime = updateRate;
				}

				customGlView->dumpEventQueue();

				director->setDeltaTime(adjustedDeltaTime);
				director->setActualDeltaTime(dt);
				reinterpret_cast<cocos2d::CCDisplayLinkDirector*>(director)->mainLoop();
			}
		}
	}

	int run() {
		updateControllerState = reinterpret_cast<decltype(updateControllerState)>(geode::base::getCocos() + 0x7bb40);
		ptr_glfwMakeContextCurrent = reinterpret_cast<decltype(ptr_glfwMakeContextCurrent)>(geode::base::getCocos() + 0xd0b40);
		ptr_glfwCustomAdjustWindowSize = reinterpret_cast<decltype(ptr_glfwCustomAdjustWindowSize)>(geode::base::getCocos() + 0xd2220);
		ptr_glfwGetWindowSize = reinterpret_cast<decltype(ptr_glfwGetWindowSize)>(geode::base::getCocos() + 0xd2170);
		ptr_glfwGetWin32Window = reinterpret_cast<decltype(ptr_glfwGetWin32Window)>(geode::base::getCocos() + 0xd5a70);

		s_nTimeElapsed = reinterpret_cast<LARGE_INTEGER*>(geode::base::getCocos() + 0x1a5498);
		s_iFrameCounter = reinterpret_cast<int*>(geode::base::getCocos() + 0x1a5494);
		s_fFrameTime = reinterpret_cast<float*>(geode::base::getCocos() + 0x1a5490);

		if (g_runSingleThreaded) {
			return runSingleThreaded();
		}

		// this is inlined on 2.206 and i don't care enough to remake it (what does it even do??)
		// PVRFrameEnableControlWindow(false);
		this->setupGLView();

		auto director = cocos2d::CCDirector::sharedDirector();
		auto glView = director->m_pobOpenGLView;
		glView->retain();

		this->setupVerticalSync();
		this->updateVerticalSync();

		this->m_bFullscreen = glView->m_bIsFullscreen;

		LARGE_INTEGER lastTime;
		QueryPerformanceCounter(&lastTime);

		std::int64_t currentPollingRate = g_pollingRate;
		double updateRate = 1.0 / static_cast<double>(currentPollingRate);

		auto freq = query_performance_frequency();
		double dFreq = freq;
		auto interval = static_cast<std::uint64_t>(freq * updateRate);

		CustomCCEGLView::g_mainThreadId = GetCurrentThreadId();
		ptr_glfwMakeContextCurrent(nullptr);

		auto inputFrames = 0u;
		auto inputTime = 0.0f;

		std::thread render_loop(&CustomCCApplication::glLoop, this);

		while (true) {
			auto loopType = g_inputLoopType.load();

			auto skipControllerPoll = loopType == 2;
			auto disableMessageWait = loopType == 1;
			auto canWaitForMessages = skipControllerPoll || !this->m_bControllerConnected;
			auto waitForMessages = canWaitForMessages && !disableMessageWait;

			if (glView->windowShouldClose()) {
				render_loop.join();
				glView->release();
				return 1;
			}

			LARGE_INTEGER currentTime;
			QueryPerformanceCounter(&currentTime);

			if (!waitForMessages && currentTime.QuadPart - lastTime.QuadPart < interval) {
				continue;
			}

			{
				// determine if we need to switch the fullscreen status
				// there's probably a better way to do this, but i'm lazy
				std::lock_guard fsLock(CustomCCEGLView::g_fullscreenMutex);
				if (CustomCCEGLView::g_needsSwitchFullscreen) {
					glView->toggleFullScreen(CustomCCEGLView::g_switchToFullscreen, CustomCCEGLView::g_switchToBorderless);
					CustomCCEGLView::g_fullscreenVariable.notify_all();
					CustomCCEGLView::g_needsSwitchFullscreen = false;
				}
			}

			// did you think i was going to make a "disable controller support" option without disabling controller support
			if (!skipControllerPoll) {
				// perform the input things
				if (this->m_bUpdateController) {
					updateControllerState(this->m_pControllerHandler);
					updateControllerState(this->m_pController2Handler);
					this->m_bUpdateController = false;
				}

				auto player1Controller = this->m_pControllerHandler;
				auto player2Controller = this->m_pController2Handler;

				this->m_bControllerConnected = player1Controller->m_controllerConnected || player2Controller->m_controllerConnected;

				if (player1Controller->m_controllerConnected) {
					this->updateControllerKeys(player1Controller, 1);
				}

				if (player2Controller->m_controllerConnected) {
					this->updateControllerKeys(player2Controller, 2);
				}
			}

			// fps is kinda meaningless with the other loop option, but whatever
			auto dt = static_cast<double>(currentTime.QuadPart - lastTime.QuadPart) / dFreq;

			inputFrames++;
			inputTime += dt;

			if (inputTime > 0.5f) {
				g_inputTps = std::round(inputFrames / inputTime);
				inputFrames = 0;
				inputTime = 0.0f;

				// recalculate interval here if necessary
				if (currentPollingRate != g_pollingRate) {
					currentPollingRate = g_pollingRate;
					updateRate = 1.0 / currentPollingRate;
					interval = static_cast<std::uint64_t>(freq * updateRate);
				}
			}

			// my reimplementation of glfwWaitEventsTimeout
			if (waitForMessages) {
				// timeout here is just in case waitmessage gets confused and forgets to process an input
				// in practice, moving the mouse would be enough to get waitmessage unstuck, but easier to just avoid it
				MsgWaitForMultipleObjects(0, nullptr, false, 2'500, QS_ALLEVENTS);
			}

			glView->pollEvents();

			lastTime.QuadPart = currentTime.QuadPart;
			std::this_thread::yield();
		}
	}

	void glLoop() {
		geode::utils::thread::setName("Render");
		CustomCCEGLView::g_renderingThreadId = GetCurrentThreadId();

		auto director = cocos2d::CCDirector::sharedDirector();
		auto glView = director->m_pobOpenGLView;
		auto customGlView = static_cast<CustomCCEGLView*>(glView);

		ptr_glfwMakeContextCurrent(glView->getWindow());

		// this call can never return false anyways
		if (!this->applicationDidFinishLaunching()) {
			glView->end();
			return;
		}

		auto isFullscreen = glView->m_bIsFullscreen;

		LARGE_INTEGER lastTime;
		QueryPerformanceCounter(&lastTime);
		*s_nTimeElapsed = lastTime;

		while (true) {
			if (glView->windowShouldClose()) {
				if (!this->m_bShutdownCalled) {
					cocos2d::CCApplication::sharedApplication()->trySaveGame(false);
				}

				if (glView->isOpenGLReady()) {
					director->end();

					reinterpret_cast<cocos2d::CCDisplayLinkDirector*>(director)->mainLoop();
				}

				cocos2d::CCApplication::sharedApplication()->platformShutdown();

				return;
			}

			if (this->m_bFullscreen != isFullscreen) {
				this->updateVerticalSync();
				this->m_bForceTimer = false;
			}
			isFullscreen = this->m_bFullscreen;

			auto updateRate = m_bVerticalSyncEnabled ? m_fVsyncInterval : m_fAnimationInterval;
			auto interval = m_bVerticalSyncEnabled ? m_nVsyncInterval : m_nAnimationInterval;

			auto useFrameCount = !isFullscreen && !this->m_bForceTimer;

			LARGE_INTEGER currentTime;
			QueryPerformanceCounter(&currentTime);
			*s_nTimeElapsed = currentTime;

			if (!useFrameCount && currentTime.QuadPart - lastTime.QuadPart < interval.QuadPart) {
				continue;
			}

			auto dCurrentTime = static_cast<double>(currentTime.QuadPart);
			auto dLastTime = static_cast<double>(lastTime.QuadPart);
			auto dInterval = static_cast<double>(interval.QuadPart);
			auto dUpdateRate = static_cast<double>(updateRate);

			auto dt = ((dCurrentTime - dLastTime) / dInterval) * dUpdateRate;
			lastTime.QuadPart = currentTime.QuadPart;

			if (useFrameCount) {
				auto currentFrameTime = *s_fFrameTime + dt;
				(*s_iFrameCounter)++;
				*s_fFrameTime = currentFrameTime;
				if (0.5 < currentFrameTime) {
					auto frameInterval = static_cast<float>(*s_iFrameCounter);
					*s_iFrameCounter = 0;
					*s_fFrameTime = 0.0f;

					auto targetFps = 1.0 / updateRate;
					auto currentFps = frameInterval / currentFrameTime;
					if ((targetFps * 1.2) < currentFps) {
						this->m_bForceTimer = true;
					}
				}
			}

			auto adjustedDeltaTime = dt;
			if (m_bSmoothFix && director->getSmoothFixCheck() && std::abs(dt - updateRate) <= updateRate * 0.1f) {
				adjustedDeltaTime = updateRate;
			}

			customGlView->dumpEventQueue();

			director->setDeltaTime(adjustedDeltaTime);
			director->setActualDeltaTime(dt);
			reinterpret_cast<cocos2d::CCDisplayLinkDirector*>(director)->mainLoop();
		}
	}
};

struct CustomCCDirector : geode::Modify<CustomCCDirector, cocos2d::CCDirector> {
	void showFPSLabel() {
		auto willUpdate = m_pFPSNode && (m_fFpsAccumDt + m_fDeltaTime > 0.1f);
		CCDirector::showFPSLabel();

		if (willUpdate) {
			// the node is already visited and drawing fps twice creates some visual weirdness
			// so only draw the next line and then set it to render the full thing on the next iteration
			auto inputSection = fmt::format("\nInput: {}", g_inputTps.load());
			this->m_pFPSNode->setString(inputSection.c_str());
			this->m_pFPSNode->visit();

			auto fpsLabel = fmt::format("FPS: {:.0f}{}", this->m_fFrameRate, inputSection);
			this->m_pFPSNode->setString(fpsLabel.c_str());
		}
	}
};

$execute {
	using namespace geode;

	g_inputLoopType = Mod::get()->getSettingValue<std::int64_t>("loop-type");
	listenForSettingChanges("loop-type", +[](std::int64_t value) {
		// value is in between 0 - 2
		g_inputLoopType = static_cast<std::uint8_t>(value);
	});

	g_pollingRate = Mod::get()->getSettingValue<std::int64_t>("input-rate");
	listenForSettingChanges("input-rate", +[](std::int64_t value) {
		g_pollingRate = value;
	});

	g_runSingleThreaded = Mod::get()->getSettingValue<bool>("single-threaded");
}
