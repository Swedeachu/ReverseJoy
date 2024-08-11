#include <windows.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <vjoyinterface.h>
#include <ViGEmClient.h>
#include <interception.h>

// Link the 64-bit vJoy and ViGEm and interception libraries
#pragma comment(lib, "vJoyInterface.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "ViGEmClient.lib")
#pragma comment(lib, "interception.lib")

// Constants for vJoy
constexpr int VJOY_DEVICE_ID = 1;
constexpr int AXIS_MAX = 32767;
constexpr int AXIS_MIN = -32767;

// Sensitivity factor for mouse movement to right stick conversion
constexpr float MOUSE_SENSITIVITY = 16000.0f; // 16k DPI (we need it high for a controller stick)

// Variables to track the state of the keys
bool wPressed = false;
bool aPressed = false;
bool sPressed = false;
bool dPressed = false;
bool spacePressed = false;
bool qPressed = false;
bool ePressed = false;
bool gToggled = true; // Variable to toggle sending input on/off

// ViGEm client and target
PVIGEM_CLIENT client;
PVIGEM_TARGET target;

// Interception context and device
InterceptionContext context;

// Keyboard hook
HHOOK hhkLowLevelKybd = NULL;

// Function to initialize vJoy
bool InitVJoy()
{
	if (!vJoyEnabled())
	{
		std::cerr << "vJoy driver not enabled: Failed Getting vJoy attributes." << std::endl;
		return false;
	}

	VjdStat status = GetVJDStatus(VJOY_DEVICE_ID);
	std::cout << "vJoy device status: " << status << std::endl;
	if (status != VJD_STAT_FREE)
	{
		std::cerr << "vJoy device " << VJOY_DEVICE_ID << " is not free." << std::endl;
		return false;
	}

	if (!AcquireVJD(VJOY_DEVICE_ID))
	{
		std::cerr << "Failed to acquire vJoy device " << VJOY_DEVICE_ID << "." << std::endl;
		return false;
	}

	return true;
}

// Function to initialize ViGEm
bool InitViGEm()
{
	client = vigem_alloc();
	if (client == nullptr)
	{
		std::cerr << "Failed to allocate ViGEm client" << std::endl;
		return false;
	}

	auto ret = vigem_connect(client);
	if (!VIGEM_SUCCESS(ret))
	{
		std::cerr << "Failed to connect to ViGEmBus" << std::endl;
		vigem_free(client);
		return false;
	}

	// Allocate and add a virtual Xbox 360 controller
	target = vigem_target_x360_alloc();
	ret = vigem_target_add(client, target);
	if (!VIGEM_SUCCESS(ret))
	{
		std::cerr << "Failed to add virtual Xbox 360 controller" << std::endl;
		vigem_target_free(target);
		vigem_free(client);
		return false;
	}

	return true;
}

// Function to initialize Interception
void InitInterception()
{
	context = interception_create_context();
	interception_set_filter(context, interception_is_mouse, INTERCEPTION_FILTER_MOUSE_ALL);
}

void CleanUpAndExit()
{
	interception_destroy_context(context);
	vigem_target_remove(client, target);
	vigem_target_free(target);
	vigem_disconnect(client);
	vigem_free(client);
	RelinquishVJD(VJOY_DEVICE_ID);
	UnhookWindowsHookEx(hhkLowLevelKybd);

	std::cout << "Clean exit completed." << std::endl;

	// Exit the program
	exit(0);
}

void UpdateController(XUSB_REPORT& report)
{
	if (!gToggled) { return; }

	// Send the report
	if (VIGEM_ERROR_NONE != vigem_target_x360_update(client, target, report))
	{
		std::cerr << "Failed to update virtual Xbox controller" << std::endl;
	}
}

static auto lastDPadUpdateTimestamp = std::chrono::steady_clock::now();

void UpdateButtonsAndDPadFromKeyBoard(XUSB_REPORT& report)
{
	auto now = std::chrono::steady_clock::now();
	lastDPadUpdateTimestamp = std::chrono::steady_clock::now();

	// Handle left stick (movement)
	LONG leftX = 0;
	LONG leftY = 0;

	if (wPressed) { leftY += AXIS_MAX; }
	if (sPressed) { leftY += AXIS_MIN; }
	if (aPressed) { leftX += AXIS_MIN; }
	if (dPressed) { leftX += AXIS_MAX; }

	// Ensure values are within the range
	leftX = max(AXIS_MIN, min(leftX, AXIS_MAX));
	leftY = max(AXIS_MIN, min(leftY, AXIS_MAX));

	report.sThumbLX = static_cast<SHORT>(leftX);
	report.sThumbLY = static_cast<SHORT>(leftY);

	// Handle buttons
	if (wPressed) { report.wButtons |= XUSB_GAMEPAD_DPAD_UP; }
	if (sPressed) { report.wButtons |= XUSB_GAMEPAD_DPAD_DOWN; }
	if (aPressed) { report.wButtons |= XUSB_GAMEPAD_DPAD_LEFT; }
	if (dPressed) { report.wButtons |= XUSB_GAMEPAD_DPAD_RIGHT; }
	if (spacePressed) { report.wButtons |= XUSB_GAMEPAD_A; }
	if (qPressed) { report.wButtons |= XUSB_GAMEPAD_X; }
	if (ePressed) { report.wButtons |= XUSB_GAMEPAD_B; }
}

LONG rightStickX = 0;
LONG rightStickY = 0;

// Creates a report with the mouse data we just got but this also calls UpdateButtonsAndDPadFromKeyBoard to make sure the whole control device is in sync
void UpdateControllerFromMouse(const InterceptionMouseStroke& stroke)
{
	XUSB_REPORT report;
	ZeroMemory(&report, sizeof(XUSB_REPORT));

	// Convert mouse movements to right stick if we have any movement
	if (stroke.x != 0 || stroke.y != 0)
	{
		// Accumulate mouse movement for the right stick
		rightStickX += static_cast<LONG>(stroke.x * MOUSE_SENSITIVITY);
		rightStickY += static_cast<LONG>(-stroke.y * MOUSE_SENSITIVITY); // Invert Y axis for typical FPS controls

		// Ensure values are within the range
		rightStickX = max(AXIS_MIN, min(rightStickX, AXIS_MAX));
		rightStickY = max(AXIS_MIN, min(rightStickY, AXIS_MAX));

		// Assign values to the report
		report.sThumbRX = static_cast<SHORT>(rightStickX);
		report.sThumbRY = static_cast<SHORT>(rightStickY);
	}

	// Handle mouse buttons (left and right click -> triggers)
	if (stroke.state & INTERCEPTION_MOUSE_LEFT_BUTTON_DOWN)
	{
		report.bLeftTrigger = 255; // Full press for left trigger
	}
	else if (stroke.state & INTERCEPTION_MOUSE_LEFT_BUTTON_UP)
	{
		report.bLeftTrigger = 0; // Release left trigger
	}

	if (stroke.state & INTERCEPTION_MOUSE_RIGHT_BUTTON_DOWN)
	{
		report.bRightTrigger = 255; // Full press for right trigger
	}
	else if (stroke.state & INTERCEPTION_MOUSE_RIGHT_BUTTON_UP)
	{
		report.bRightTrigger = 0; // Release right trigger
	}

	// Now update the controller
	UpdateButtonsAndDPadFromKeyBoard(report);
	UpdateController(report);

	// Reset the right stick values after sending the report
	rightStickX = 0;
	rightStickY = 0;
}

// Function to handle Interception loop
void InterceptionLoop()
{
	InterceptionDevice device;
	InterceptionStroke stroke;

	while (interception_receive(context, device = interception_wait(context), &stroke, 1) > 0)
	{
		if (interception_is_mouse(device))
		{
			if (gToggled)
			{
				// Translate from mouse into controller
				InterceptionMouseStroke& mouseStroke = *(InterceptionMouseStroke*)&stroke;
				UpdateControllerFromMouse(mouseStroke);
				// Block the mouse input if gToggled is true
				continue; // Do not send the stroke further
			}
			else
			{
				// If gToggled is false, pass the input to the next application
				interception_send(context, device, &stroke, 1);
			}
		}
	}
}

// Keyboard hook callback function
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION)
	{
		KBDLLHOOKSTRUCT* pKbdLLHookStruct = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
		bool keyPressed = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
		bool keyReleased = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);
		if (keyPressed || keyReleased)
		{
			switch (pKbdLLHookStruct->vkCode)
			{
				case 'W': wPressed = keyPressed; break;
				case 'A': aPressed = keyPressed; break;
				case 'S': sPressed = keyPressed; break;
				case 'D': dPressed = keyPressed; break;
				case 'Q': qPressed = keyPressed; break;
				case 'E': ePressed = keyPressed; break;
				case VK_SPACE: spacePressed = keyPressed; break;
				case 'G': if (keyPressed) { gToggled = !gToggled; std::cout << "Input toggled " << (gToggled ? "on" : "off") << std::endl; } break;
				case 'J': if (keyPressed) { CleanUpAndExit(); } break;
			}
			// Cancel output as we are completely overriding it
			if (gToggled && (wPressed || aPressed || sPressed || dPressed || qPressed || ePressed || spacePressed)) { return 1; }
		}
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void UpdateThread()
{
	while (true)
	{
		// Calculate the elapsed time since the last timestamp that the Dpad was updated
		auto now = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDPadUpdateTimestamp);

		// Check if 16 milliseconds or more have passed
		if (elapsed.count() >= 16)
		{
			XUSB_REPORT report;
			ZeroMemory(&report, sizeof(XUSB_REPORT));
			UpdateButtonsAndDPadFromKeyBoard(report);
			UpdateController(report);
		}
	}
}

int main()
{
	// Initialize vJoy
	if (!InitVJoy())
	{
		return -1;
	}

	// Initialize ViGEm
	if (!InitViGEm())
	{
		return -1;
	}

	// Initialize Interception
	InitInterception();

	// Set up the keyboard hook
	hhkLowLevelKybd = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, 0, 0);
	if (hhkLowLevelKybd == NULL)
	{
		std::cerr << "Failed to install keyboard hook." << std::endl;
		return -1;
	}

	std::cout << "Keyboard and driver level mouse hooks installed successfully." << std::endl;

	// Start the update thread for the keyboard to controller update polling to the virtual controller
	std::thread updateThread(UpdateThread);
	updateThread.detach();

	// Start the interception loop thread for the driver level mouse hook for polling to the virtual controller
	std::thread interceptionThread(InterceptionLoop);
	interceptionThread.detach();

	// Message loop to keep the windows keyboard hook alive
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	CleanUpAndExit();

	return 0;
}
