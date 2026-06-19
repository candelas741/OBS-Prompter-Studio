#include "prompter-xbox.hpp"

#include "prompter-dock.hpp"

#include <obs-module.h>

#include <algorithm>

XboxController::XboxController(std::function<void(bool)> onConnectionChanged)
	: connectionChanged(std::move(onConnectionChanged))
{
	clock.start();
	timer.setInterval(33);
	QObject::connect(&timer, &QTimer::timeout, [this]() { poll(); });

#ifdef _WIN32
	for (const wchar_t *library : {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"}) {
		xinputModule = LoadLibraryW(library);
		if (xinputModule) {
			getState = reinterpret_cast<XInputGetStateFn>(GetProcAddress(xinputModule, "XInputGetState"));
			if (getState)
				break;
			FreeLibrary(xinputModule);
			xinputModule = nullptr;
		}
	}
	if (!getState)
		blog(LOG_WARNING, "[Prompter Studio] XInput no esta disponible en este sistema");
#endif
}

XboxController::~XboxController()
{
	timer.stop();
#ifdef _WIN32
	if (xinputModule)
		FreeLibrary(xinputModule);
#endif
}

void XboxController::setEnabled(bool value)
{
	if (enabled == value)
		return;
	enabled = value;
	rightTriggerPressed = false;
	leftTriggerPressed = false;
	if (enabled)
		timer.start();
	else {
		timer.stop();
		if (connected) {
			connected = false;
			blog(LOG_INFO, "[Prompter Studio] Xbox controller disconnected");
			if (connectionChanged)
				connectionChanged(false);
		}
	}
}

void XboxController::setTriggerThreshold(double threshold)
{
	triggerThreshold = std::clamp(threshold, 0.1, 1.0);
}

void XboxController::setDebounceMs(int value)
{
	debounceMs = std::clamp(value, 50, 2000);
}

void XboxController::poll()
{
	if (!enabled)
		return;

#ifdef _WIN32
	XINPUT_STATE state = {};
	bool found = false;
	for (DWORD index = 0; index < XUSER_MAX_COUNT; ++index) {
		if (getState && getState(index, &state) == ERROR_SUCCESS) {
			found = true;
			break;
		}
	}

	if (found != connected) {
		connected = found;
		blog(LOG_INFO, "[Prompter Studio] Xbox controller %s", connected ? "connected" : "disconnected");
		if (connectionChanged)
			connectionChanged(connected);
	}
	if (!found) {
		rightTriggerPressed = false;
		leftTriggerPressed = false;
		return;
	}

	const bool rightPressed = state.Gamepad.bRightTrigger / 255.0 >= triggerThreshold;
	const bool leftPressed = state.Gamepad.bLeftTrigger / 255.0 >= triggerThreshold;
	const qint64 now = clock.elapsed();
	if (rightPressed && !rightTriggerPressed && now - lastRightTriggerMs >= debounceMs) {
		lastRightTriggerMs = now;
		blog(LOG_INFO, "[Prompter Studio] Xbox RT pressed: next paragraph");
		prompter_controller_step_forward();
	}
	if (leftPressed && !leftTriggerPressed && now - lastLeftTriggerMs >= debounceMs) {
		lastLeftTriggerMs = now;
		blog(LOG_INFO, "[Prompter Studio] Xbox LT pressed: previous paragraph");
		prompter_controller_step_backward();
	}
	rightTriggerPressed = rightPressed;
	leftTriggerPressed = leftPressed;
#else
	if (connected) {
		connected = false;
		if (connectionChanged)
			connectionChanged(false);
	}
#endif
}
