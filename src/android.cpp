#include <Geode/cocos/platform/android/jni/JniHelper.h>
#include <time.h>

#include "input.hpp"

std::uint64_t platform_get_time() {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (now.tv_sec * 1000) + (now.tv_nsec / 1'000'000);
}

void clearJNIExceptions() {
	auto vm = cocos2d::JniHelper::getJavaVM();

	JNIEnv* env;
	if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) {
			env->ExceptionClear();
	}
}

bool reportPlatformCapability(std::string id) {
	cocos2d::JniMethodInfo t;
	if (cocos2d::JniHelper::getStaticMethodInfo(t, "com/geode/launcher/utils/GeodeUtils", "reportPlatformCapability", "(Ljava/lang/String;)Z")) {
		jstring stringArg1 = t.env->NewStringUTF(id.c_str());

		auto r = t.env->CallStaticBooleanMethod(t.classID, t.methodID, stringArg1);

		t.env->DeleteLocalRef(stringArg1);
		t.env->DeleteLocalRef(t.classID);

		return r;
	} else {
		clearJNIExceptions();
	}

	return false;
}

void JNICALL JNI_setNextInputTimestamp(JNIEnv* env, jobject, jlong timestamp) {
	auto timestampMs = timestamp / 1'000'000;
	ExtendedCCKeyboardDispatcher::setTimestamp(timestampMs);
	ExtendedCCTouchDispatcher::setTimestamp(timestampMs);
}

static JNINativeMethod methods[] = {
  {
		"setNextInputTimestamp",
		"(J)V",
		reinterpret_cast<void*>(&JNI_setNextInputTimestamp)
	},
};

$execute {
	auto vm = cocos2d::JniHelper::getJavaVM();

	JNIEnv* env;
	if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) {
			auto clazz = env->FindClass("com/geode/launcher/utils/GeodeUtils");
			if (env->RegisterNatives(clazz, methods, 1) != 0) {
				// method was not found
				clearJNIExceptions();
			} else {
				reportPlatformCapability("timestamp_inputs");
			}
	}
}
