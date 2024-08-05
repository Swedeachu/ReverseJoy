#include <windows.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <vjoyinterface.h>
#include <ViGEmClient.h>

// Link the 64-bit vJoy and ViGEm libraries
#pragma comment(lib, "vJoyInterface.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "ViGEmClient.lib")

// Constants for vJoy
constexpr int VJOY_DEVICE_ID = 1;
constexpr int AXIS_MAX = 32767;
constexpr int AXIS_MIN = -32767;

// Sensitivity factor for mouse movement to right stick conversion
constexpr float MOUSE_SENSITIVITY = 10.0f;

// Variables to track the state of the keys
bool wPressed = false;
bool aPressed = false;
bool sPressed = false;
bool dPressed = false;
bool spacePressed = false;
bool gToggled = true; // Variable to toggle sending input on/off

// Variables to track the state of the mouse
LONG rightStickX = 0;
LONG rightStickY = 0;

// ViGEm client and target
PVIGEM_CLIENT client;
PVIGEM_TARGET target;

void UpdateController()
{
  if (!gToggled) return;

  XUSB_REPORT report;
  ZeroMemory(&report, sizeof(XUSB_REPORT));

  // Handle left stick (movement)
  LONG leftX = 0;
  LONG leftY = 0;

  if (wPressed) leftY += AXIS_MAX;
  if (sPressed) leftY += AXIS_MIN;
  if (aPressed) leftX += AXIS_MIN;
  if (dPressed) leftX += AXIS_MAX;

  // Ensure values are within the range
  leftX = max(AXIS_MIN, min(leftX, AXIS_MAX));
  leftY = max(AXIS_MIN, min(leftY, AXIS_MAX));

  report.sThumbLX = static_cast<SHORT>(leftX);
  report.sThumbLY = static_cast<SHORT>(leftY);

  // Handle right stick (aiming)
  report.sThumbRX = static_cast<SHORT>(rightStickX);
  report.sThumbRY = static_cast<SHORT>(rightStickY);

  // Handle buttons
  if (wPressed) report.wButtons |= XUSB_GAMEPAD_DPAD_UP;
  if (sPressed) report.wButtons |= XUSB_GAMEPAD_DPAD_DOWN;
  if (aPressed) report.wButtons |= XUSB_GAMEPAD_DPAD_LEFT;
  if (dPressed) report.wButtons |= XUSB_GAMEPAD_DPAD_RIGHT;
  if (spacePressed) report.wButtons |= XUSB_GAMEPAD_A;

  // Send the report
  if (VIGEM_ERROR_NONE != vigem_target_x360_update(client, target, report))
  {
    std::cerr << "Failed to update virtual Xbox controller" << std::endl;
  }

  // Reset the right stick values for the next update
  rightStickX = 0;
  rightStickY = 0;
}

void UpdateThread()
{
  while (true)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 updates per second
    UpdateController();
  }
}

// Keyboard hook callback function
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
  if (nCode == HC_ACTION)
  {
    KBDLLHOOKSTRUCT* pKbdLLHookStruct = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
    switch (wParam)
    {
      case WM_KEYDOWN:
      case WM_SYSKEYDOWN:
        switch (pKbdLLHookStruct->vkCode)
        {
          case 'W': wPressed = true; break;
          case 'A': aPressed = true; break;
          case 'S': sPressed = true; break;
          case 'D': dPressed = true; break;
          case VK_SPACE: spacePressed = true; break;
          case 'G': gToggled = !gToggled; std::cout << "Input toggled " << (gToggled ? "on" : "off") << std::endl; break; // Toggle input
          case '1': // Left bumper
            if (gToggled)
            {
              XUSB_REPORT report;
              ZeroMemory(&report, sizeof(XUSB_REPORT));
              report.wButtons |= XUSB_GAMEPAD_LEFT_SHOULDER;
              vigem_target_x360_update(client, target, report);
            }
            break;
          case '2': // Y button
            if (gToggled)
            {
              XUSB_REPORT report;
              ZeroMemory(&report, sizeof(XUSB_REPORT));
              report.wButtons |= XUSB_GAMEPAD_Y;
              vigem_target_x360_update(client, target, report);
            }
            break;
          case '3': // Right bumper
            if (gToggled)
            {
              XUSB_REPORT report;
              ZeroMemory(&report, sizeof(XUSB_REPORT));
              report.wButtons |= XUSB_GAMEPAD_RIGHT_SHOULDER;
              vigem_target_x360_update(client, target, report);
            }
            break;
        }
        break;

      case WM_KEYUP:
      case WM_SYSKEYUP:
        switch (pKbdLLHookStruct->vkCode)
        {
          case 'W': wPressed = false; break;
          case 'A': aPressed = false; break;
          case 'S': sPressed = false; break;
          case 'D': dPressed = false; break;
          case VK_SPACE: spacePressed = false; break;
          case '1':
          case '2':
          case '3':
            // Release buttons when keys are released
            if (gToggled)
            {
              XUSB_REPORT report;
              ZeroMemory(&report, sizeof(XUSB_REPORT));
              vigem_target_x360_update(client, target, report);
            }
            break;
        }
        break;
    }
    // cancel output as we are completely overriding it
    if (gToggled && (wPressed || aPressed || sPressed || dPressed || spacePressed)) return 1;
  }

  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// Mouse hook callback function
LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
  if (nCode == HC_ACTION && gToggled)
  {
    static LONG lastX = 0, lastY = 0; // Store last mouse position

    switch (wParam)
    {
      case WM_LBUTTONDOWN:
      {
        XUSB_REPORT report;
        ZeroMemory(&report, sizeof(XUSB_REPORT));
        report.bLeftTrigger = 255; // Left trigger full press
        vigem_target_x360_update(client, target, report);
        return 1;
      }
      case WM_LBUTTONUP:
      {
        XUSB_REPORT report;
        ZeroMemory(&report, sizeof(XUSB_REPORT));
        report.bLeftTrigger = 0; // Release left trigger
        vigem_target_x360_update(client, target, report);
        return 1;
      }
      case WM_RBUTTONDOWN:
      {
        XUSB_REPORT report;
        ZeroMemory(&report, sizeof(XUSB_REPORT));
        report.bRightTrigger = 255; // Right trigger full press
        vigem_target_x360_update(client, target, report);
        return 1;
      }
      case WM_RBUTTONUP:
      {
        XUSB_REPORT report;
        ZeroMemory(&report, sizeof(XUSB_REPORT));
        report.bRightTrigger = 0; // Release right trigger
        vigem_target_x360_update(client, target, report);
        return 1;
      }
      case WM_MOUSEMOVE:
      {
        MSLLHOOKSTRUCT* pMouseStruct = (MSLLHOOKSTRUCT*)lParam;
        if (pMouseStruct != nullptr)
        {
          LONG deltaX = pMouseStruct->pt.x - lastX;
          LONG deltaY = pMouseStruct->pt.y - lastY;

          lastX = pMouseStruct->pt.x;
          lastY = pMouseStruct->pt.y;

          rightStickX += static_cast<LONG>(deltaX * MOUSE_SENSITIVITY);
          rightStickY += static_cast<LONG>(-deltaY * MOUSE_SENSITIVITY); // Invert Y axis for typical FPS controls

          // Ensure values are within the range
          rightStickX = max(AXIS_MIN, min(rightStickX, AXIS_MAX));
          rightStickY = max(AXIS_MIN, min(rightStickY, AXIS_MAX));
        }
        return 1;
      }
    }
  }
  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int main()
{
  // Initialize vJoy
  if (!vJoyEnabled())
  {
    std::cerr << "vJoy driver not enabled: Failed Getting vJoy attributes." << std::endl;
    return -1;
  }

  VjdStat status = GetVJDStatus(VJOY_DEVICE_ID);
  std::cout << "vJoy device status: " << status << std::endl;
  if (status != VJD_STAT_FREE)
  {
    std::cerr << "vJoy device " << VJOY_DEVICE_ID << " is not free." << std::endl;
    return -1;
  }

  if (!AcquireVJD(VJOY_DEVICE_ID))
  {
    std::cerr << "Failed to acquire vJoy device " << VJOY_DEVICE_ID << "." << std::endl;
    return -1;
  }

  // Initialize ViGEm Client
  client = vigem_alloc();
  if (client == nullptr)
  {
    std::cerr << "Failed to allocate ViGEm client" << std::endl;
    return -1;
  }

  auto ret = vigem_connect(client);
  if (!VIGEM_SUCCESS(ret))
  {
    std::cerr << "Failed to connect to ViGEmBus" << std::endl;
    vigem_free(client);
    return -1;
  }

  // Allocate and add a virtual Xbox 360 controller
  target = vigem_target_x360_alloc();
  ret = vigem_target_add(client, target);
  if (!VIGEM_SUCCESS(ret))
  {
    std::cerr << "Failed to add virtual Xbox 360 controller" << std::endl;
    vigem_target_free(target);
    vigem_free(client);
    return -1;
  }

  // Set up the keyboard hook
  HHOOK hhkLowLevelKybd = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, 0, 0);
  if (hhkLowLevelKybd == NULL)
  {
    std::cerr << "Failed to install keyboard hook." << std::endl;
    return -1;
  }

  // Set up the mouse hook
  HHOOK hhkLowLevelMouse = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, 0, 0);
  if (hhkLowLevelMouse == NULL)
  {
    std::cerr << "Failed to install mouse hook." << std::endl;
    UnhookWindowsHookEx(hhkLowLevelKybd);
    return -1;
  }

  std::cout << "Keyboard and mouse hooks installed successfully." << std::endl;

  // Start the update thread
  std::thread updateThread(UpdateThread);
  updateThread.detach();

  // Message loop to keep the hooks alive
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  // Clean up
  vigem_target_remove(client, target);
  vigem_target_free(target);
  vigem_disconnect(client);
  vigem_free(client);
  RelinquishVJD(VJOY_DEVICE_ID);
  UnhookWindowsHookEx(hhkLowLevelKybd);
  UnhookWindowsHookEx(hhkLowLevelMouse);

  return 0;
}
