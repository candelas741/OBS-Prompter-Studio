#pragma once

#include <functional>

#include <QElapsedTimer>
#include <QTimer>

#ifdef _WIN32
#include <windows.h>
#include <Xinput.h>
#endif

// Sondea XInput y entrega los flancos de los gatillos al controlador compartido.
class XboxController {
public:
	explicit XboxController(std::function<void(bool)> connectionChanged);
	~XboxController();

	void setEnabled(bool enabled);
	void setTriggerThreshold(double threshold);
	void setDebounceMs(int debounceMs);

private:
	void poll();

	QTimer timer;
	QElapsedTimer clock;
	std::function<void(bool)> connectionChanged;
	bool enabled = false;
	bool connected = false;
	bool rightTriggerPressed = false;
	bool leftTriggerPressed = false;
	double triggerThreshold = 0.55;
	int debounceMs = 250;
	qint64 lastRightTriggerMs = -1000000;
	qint64 lastLeftTriggerMs = -1000000;

#ifdef _WIN32
	using XInputGetStateFn = DWORD(WINAPI *)(DWORD, XINPUT_STATE *);
	HMODULE xinputModule = nullptr;
	XInputGetStateFn getState = nullptr;
#endif
};
