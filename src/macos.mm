#define CommentType CommentTypeDummy
#import <Cocoa/Cocoa.h>
#include <objc/runtime.h>
#undef CommentType

#include <Geode/Geode.hpp>

#include <cstdint>
#include <time.h>

#include "input.hpp"

std::uint64_t platform_get_time() {
	// convert ns to ms
	return clock_gettime_nsec_np(CLOCK_UPTIME_RAW) / 1'000'000;
}

@interface EAGLView : NSOpenGLView
@end

static IMP keyDownExecOIMP;
void keyDownExec(EAGLView* self, SEL sel, NSEvent* event) {
	auto timestamp = static_cast<std::uint64_t>([event timestamp] * 1000.0);
	ExtendedCCKeyboardDispatcher::setTimestamp(timestamp);

	reinterpret_cast<decltype(&keyDownExec)>(keyDownExecOIMP)(self, sel, event);
}

static IMP keyUpExecOIMP;
void keyUpExec(EAGLView* self, SEL sel, NSEvent* event) {
	auto timestamp = static_cast<std::uint64_t>([event timestamp] * 1000.0);
	ExtendedCCKeyboardDispatcher::setTimestamp(timestamp);

	reinterpret_cast<decltype(&keyUpExec)>(keyUpExecOIMP)(self, sel, event);
}

static IMP mouseDownExecOIMP;
void mouseDownExec(EAGLView* self, SEL sel, NSEvent* event) {
	auto timestamp = static_cast<std::uint64_t>([event timestamp] * 1000.0);
	ExtendedCCTouchDispatcher::setTimestamp(timestamp);

	reinterpret_cast<decltype(&mouseDownExec)>(mouseDownExecOIMP)(self, sel, event);
}

static IMP mouseDraggedExecOIMP;
void mouseDraggedExec(EAGLView* self, SEL sel, NSEvent* event) {
	auto timestamp = static_cast<std::uint64_t>([event timestamp] * 1000.0);
	ExtendedCCTouchDispatcher::setTimestamp(timestamp);

	reinterpret_cast<decltype(&mouseDraggedExec)>(mouseDraggedExecOIMP)(self, sel, event);
}

static IMP mouseUpExecOIMP;
void mouseUpExec(EAGLView* self, SEL sel, NSEvent* event) {
	auto timestamp = static_cast<std::uint64_t>([event timestamp] * 1000.0);
	ExtendedCCTouchDispatcher::setTimestamp(timestamp);

	reinterpret_cast<decltype(&mouseUpExec)>(mouseUpExecOIMP)(self, sel, event);
}

$execute {
	auto eaglView = objc_getClass("EAGLView");

	auto keyDownExecMethod = class_getInstanceMethod(eaglView, @selector(keyDownExec:));
	keyDownExecOIMP = method_getImplementation(keyDownExecMethod);
	method_setImplementation(keyDownExecMethod, (IMP)&keyDownExec);

	auto keyUpExecMethod = class_getInstanceMethod(eaglView, @selector(keyUpExec:));
	keyUpExecOIMP = method_getImplementation(keyUpExecMethod);
	method_setImplementation(keyUpExecMethod, (IMP)&keyUpExec);

	auto mouseDownExecMethod = class_getInstanceMethod(eaglView, @selector(mouseDownExec:));
	mouseDownExecOIMP = method_getImplementation(mouseDownExecMethod);
	method_setImplementation(mouseDownExecMethod, (IMP)&mouseDownExec);

	auto mouseDraggedExecMethod = class_getInstanceMethod(eaglView, @selector(mouseDraggedExec:));
	mouseDraggedExecOIMP = method_getImplementation(mouseDraggedExecMethod);
	method_setImplementation(mouseDraggedExecMethod, (IMP)&mouseDraggedExec);

	auto mouseUpExecMethod = class_getInstanceMethod(eaglView, @selector(mouseUpExec:));
	mouseUpExecOIMP = method_getImplementation(mouseUpExecMethod);
	method_setImplementation(mouseUpExecMethod, (IMP)&mouseUpExec);
}