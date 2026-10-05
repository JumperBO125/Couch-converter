#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <windows.h>

#include "interception.h"
#include "ViGEm/Client.h"

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "vigemclient.lib")
#pragma comment(lib, "interception.lib")

namespace fs = std::filesystem;

enum ControllerAction {
    BTN_A, BTN_B, BTN_X, BTN_Y,
    BTN_LB, BTN_RB, BTN_START, BTN_BACK,
    BTN_THUMBL, BTN_THUMBR,
    DPAD_UP, DPAD_DOWN, DPAD_LEFT, DPAD_RIGHT,
    AXIS_LX_POSITIVE, AXIS_LX_NEGATIVE,
    AXIS_LY_POSITIVE, AXIS_LY_NEGATIVE,
    AXIS_RX_POSITIVE, AXIS_RX_NEGATIVE,
    AXIS_RY_POSITIVE, AXIS_RY_NEGATIVE,
    TRIGGER_L, TRIGGER_R,
    ACTION_COUNT
};

const std::string ActionNames[ACTION_COUNT] = {
    "Button A", "Button B", "Button X", "Button Y",
    "Left Shoulder (LB)", "Right Shoulder (RB)", "Start", "Back",
    "Left Thumb Click", "Right Thumb Click",
    "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right",
    "Left Stick X+", "Left Stick X-", "Left Stick Y+", "Left Stick Y-",
    "Right Stick X+", "Right Stick X-", "Right Stick Y+", "Right Stick Y-",
    "Left Trigger", "Right Trigger"
};

bool g_PhysicalKeysPressed[512] = { false };
std::vector<USHORT> g_ActionBindings[ACTION_COUNT];
std::string g_SelectedConfigFile = "";

InterceptionDevice g_LockedKeyboardDevice = 0;
bool g_IsDeviceLocked = false;

bool LoadConfigFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return false;

    for (int i = 0; i < ACTION_COUNT; ++i) {
        g_ActionBindings[i].clear();
    }

    std::string line;
    while (std::getline(file, line)) {
        size_t delim = line.find('=');
        if (delim == std::string::npos) continue;

        int actionIdx = std::stoi(line.substr(0, delim));
        if (actionIdx < 0 || actionIdx >= ACTION_COUNT) continue;

        std::string keysStr = line.substr(delim + 1);
        size_t pos = 0;
        while ((pos = keysStr.find(',')) != std::string::npos) {
            g_ActionBindings[actionIdx].push_back(std::stoi(keysStr.substr(0, pos)));
            keysStr.erase(0, pos + 1);
        }
        if (!keysStr.empty()) {
            g_ActionBindings[actionIdx].push_back(std::stoi(keysStr));
        }
    }
    file.close();
    return true;
}

void HandleConfigInitialization() {
    std::vector<std::string> configFiles;
    try {
        for (const auto& entry : fs::directory_iterator(fs::current_path())) {
            if (entry.is_regular_file() && entry.path().extension() == ".cfg") {
                configFiles.push_back(entry.path().filename().string());
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Directory scan failed: " << e.what() << "\n";
        system("pause");
        exit(1);
    }

    if (configFiles.empty()) {
        std::cerr << "\n==================================================\n";
        std::cerr << "No profile map files found, run setup and configure one\n";
        std::cerr << "==================================================\n";
        system("pause");
        exit(1);
    }

    if (configFiles.size() == 1) {
        g_SelectedConfigFile = configFiles[0];
    }
    else {
        std::cout << "\n==================================================\n";
        std::cout << " MULTIPLE PROFILE MAPS DETECTED:\n";
        std::cout << "==================================================\n";
        for (size_t i = 0; i < configFiles.size(); ++i) {
            std::cout << "  [" << (i + 1) << "] " << configFiles[i] << "\n";
        }
        int choice = 0;
        while (true) {
            std::cout << "Select a profile map index (1-" << configFiles.size() << "): ";
            if (std::cin >> choice && choice >= 1 && choice <= static_cast<int>(configFiles.size())) break;
            std::cin.clear();
            std::cin.ignore(256, '\n');
        }
        g_SelectedConfigFile = configFiles[choice - 1];
    }

    if (!LoadConfigFromFile(g_SelectedConfigFile)) {
        std::cerr << "[ERROR] Corrupt file" << g_SelectedConfigFile << "\n";
        system("pause");
        exit(1);
    }
}

void SetupConsoleTerminal() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
    std::cout << "\x1b[?25l";
}

bool IsActionPressed(ControllerAction action) {
    for (USHORT scanCode : g_ActionBindings[action]) {
        if (scanCode < 512 && g_PhysicalKeysPressed[scanCode]) return true;
    }
    return false;
}

void DrawInterfaceDashboard() {
    std::cout << "\x1b[H";
    std::cout << "==================================================\n";
    std::cout << "    COUCH-PLAY KEYBOARD HARDWARE REMAPPER\n";
    std::cout << "==================================================\n";
    std::cout << " Active Profile: " << g_SelectedConfigFile << "\n";
    std::cout << " Device Lock:    ";
    if (g_IsDeviceLocked) {
        std::cout << "\x1b[36m[ LOCKED TO EXTERNAL KEYBOARD ID: " << g_LockedKeyboardDevice << " ]\x1b[0m\n";
    }
    else {
        std::cout << "\x1b[33m[ AWAITING FIRST MAPPED KEY INPUT... ]\x1b[0m\n";
    }
    std::cout << "--------------------------------------------------\n";
    std::cout << " [DEBUG] BUTTON TEST:\n\n";

    for (int i = 0; i < ACTION_COUNT; ++i) {
        if (g_ActionBindings[i].empty()) continue;
        std::cout << "  " << ActionNames[i] << ": ";
        if (IsActionPressed((ControllerAction)i)) {
            std::cout << "\x1b[32m[ ACTIVE ]\x1b[0m  \n";
        }
        else {
            std::cout << "\x1b[90m[  idle  ]\x1b[0m  \n";
        }
    }
    std::cout << "--------------------------------------------------\n";
    std::cout << "Press Ctrl+C inside this terminal window to close.\n";
    std::cout << "DON'T PRESS 'x' AND DON'T UNPLUG THE KEYBOARD UNTIL THE PROGRAM IS CLOSED.\n";
}


void ProcessAndSendInput(PVIGEM_CLIENT client, PVIGEM_TARGET pad) {
    XUSB_REPORT report = { 0 };

    if (IsActionPressed(BTN_A))      report.wButtons |= XUSB_GAMEPAD_A;
    if (IsActionPressed(BTN_B))      report.wButtons |= XUSB_GAMEPAD_B;
    if (IsActionPressed(BTN_X))      report.wButtons |= XUSB_GAMEPAD_X;
    if (IsActionPressed(BTN_Y))      report.wButtons |= XUSB_GAMEPAD_Y;
    if (IsActionPressed(BTN_LB))     report.wButtons |= XUSB_GAMEPAD_LEFT_SHOULDER;
    if (IsActionPressed(BTN_RB))     report.wButtons |= XUSB_GAMEPAD_RIGHT_SHOULDER;
    if (IsActionPressed(BTN_START))  report.wButtons |= XUSB_GAMEPAD_START;
    if (IsActionPressed(BTN_BACK))   report.wButtons |= XUSB_GAMEPAD_BACK;
    if (IsActionPressed(BTN_THUMBL)) report.wButtons |= XUSB_GAMEPAD_LEFT_THUMB;
    if (IsActionPressed(BTN_THUMBR)) report.wButtons |= XUSB_GAMEPAD_RIGHT_THUMB;

    bool dpadUp = IsActionPressed(DPAD_UP), dpadDown = IsActionPressed(DPAD_DOWN);
    bool dpadLeft = IsActionPressed(DPAD_LEFT), dpadRight = IsActionPressed(DPAD_RIGHT);
    if (dpadUp && dpadDown) { dpadUp = false; dpadDown = false; }
    if (dpadLeft && dpadRight) { dpadLeft = false; dpadRight = false; }
    if (dpadUp)    report.wButtons |= XUSB_GAMEPAD_DPAD_UP;
    if (dpadDown)  report.wButtons |= XUSB_GAMEPAD_DPAD_DOWN;
    if (dpadLeft)  report.wButtons |= XUSB_GAMEPAD_DPAD_LEFT;
    if (dpadRight) report.wButtons |= XUSB_GAMEPAD_DPAD_RIGHT;

    bool laUp = IsActionPressed(AXIS_LY_POSITIVE), laDown = IsActionPressed(AXIS_LY_NEGATIVE);
    bool laLeft = IsActionPressed(AXIS_LX_NEGATIVE), laRight = IsActionPressed(AXIS_LX_POSITIVE);
    if (laLeft && laRight) report.sThumbLX = 0;
    else report.sThumbLX = laLeft ? -32768 : (laRight ? 32767 : 0);
    if (laDown && laUp) report.sThumbLY = 0;
    else report.sThumbLY = laDown ? -32768 : (laUp ? 32767 : 0);

    bool raUp = IsActionPressed(AXIS_RY_POSITIVE), raDown = IsActionPressed(AXIS_RY_NEGATIVE);
    bool raLeft = IsActionPressed(AXIS_RX_NEGATIVE), raRight = IsActionPressed(AXIS_RX_POSITIVE);
    if (raLeft && raRight) report.sThumbRX = 0;
    else report.sThumbRX = raLeft ? -32768 : (raRight ? 32767 : 0);
    if (raDown && raUp) report.sThumbRY = 0;
    else report.sThumbRY = raDown ? -32768 : (raUp ? 32767 : 0);

    report.bLeftTrigger = IsActionPressed(TRIGGER_L) ? 255 : 0;
    report.bRightTrigger = IsActionPressed(TRIGGER_R) ? 255 : 0;

    vigem_target_x360_update(client, pad, report);
}

int main() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    HandleConfigInitialization();
    SetupConsoleTerminal();
    system("cls");

    PVIGEM_CLIENT client = vigem_alloc();
    if (!client || !SUCCEEDED(vigem_connect(client))) {
        std::cerr << "[CRASH] ViGEm allocation or communication link failed.\n";
        system("pause");
        return 1;
    }

    PVIGEM_TARGET pad = vigem_target_x360_alloc();
    if (!SUCCEEDED(vigem_target_add(client, pad))) {
        std::cerr << "[CRASH] Failed to initialize virtual X360 pad.\n";
        system("pause");
        return 1;
    }

    InterceptionContext context = interception_create_context();
    if (context == NULL) {
        std::cerr << "[CRASH] Interception Driver context returned NULL. Make sure to run as Admin.\n";
        system("pause");
        return 1;
    }

    interception_set_filter(context, interception_is_keyboard, INTERCEPTION_FILTER_KEY_ALL);

    InterceptionDevice device;
    InterceptionStroke stroke;

    DrawInterfaceDashboard();

    int receiveResult = 0;
    while ((receiveResult = interception_receive(context, device = interception_wait(context), &stroke, 1)) > 0) {
        if (interception_is_keyboard(device)) {
            InterceptionKeyStroke& keyStroke = (InterceptionKeyStroke&)stroke;

            USHORT rawScanCode = keyStroke.code;
            USHORT shiftedScanCode = rawScanCode + 256;
            USHORT windowsExtendedCode = rawScanCode | 0xE000;

            bool isExtended = (keyStroke.state & INTERCEPTION_KEY_E0);

            USHORT activeCodeToTrack = isExtended ? shiftedScanCode : rawScanCode;
            bool isKeyDown = !(keyStroke.state & INTERCEPTION_KEY_UP);
            bool isBoundKey = false;
            for (int i = 0; i < ACTION_COUNT; ++i) {
                for (USHORT boundCode : g_ActionBindings[i]) {
                    if (boundCode == rawScanCode ||
                        boundCode == shiftedScanCode ||
                        boundCode == windowsExtendedCode) {
                        isBoundKey = true;
                        break;
                    }
                }
                if (isBoundKey) break;
            }

            if (isBoundKey) {
                if (!g_IsDeviceLocked) {
                    g_LockedKeyboardDevice = device;
                    g_IsDeviceLocked = true;
                }

                if (device == g_LockedKeyboardDevice) {
                    if (activeCodeToTrack < 512) {
                        g_PhysicalKeysPressed[activeCodeToTrack] = isKeyDown;
                    }
                    ProcessAndSendInput(client, pad);
                    DrawInterfaceDashboard();
                    continue;
                }
            }
        }

        interception_send(context, device, &stroke, 1);
    }

    if (receiveResult <= 0) {
        std::cerr << "\nIf you see this message, it means the interception stream was closed unexpectedly.\n";
        std::cerr << "\nI don't know why this happened but running as an administrator might help.\n";
    }

    interception_destroy_context(context);
    vigem_target_remove(client, pad);
    vigem_target_free(pad);
    vigem_disconnect(client);
    vigem_free(client);

    std::cout << "\x1b[?25h";
    system("pause");
    return 0;
}
// most of this code was written by JumperBO125. The button mapping website was gemini (was too lazy to make my own(might get around to it if this repo gets popular))
// You have permission to use this code for your own projects, but please credit me if you do. I don't care if you make money off of it, just give me credit.
// Donations are appreciated but not required. I'm sure you know of my situation regarding my laptop screen (Just need 150 dollars for a replacement)
// Hello phoenix SC, Hope you find a use for this code even though it's not minecraft related
// If you want to contact me, you can reach me at my email: Jumper.12m@gmail.com Youtube:https://youtube.com/@jumperbo125?si=exADv-oxeEcyvKvx fiverr: I'm nigerian, I'm discriminated against (We deserve it actually)
// I'm a 2nd year computer science student at godfrey okoye university, Enugu Nigeria
// I love programming, video editing and creating music (Also love gaming but that's neither here nor there)
// I hope you enjoy this code and find it useful. If you have any questions or need help, feel free to reach out to me.

// I know this might seem like I'm begging for money, but that's cause I AM!!!
// Got school on thursday and my shitty laptop pulled this on me
// I'd link a go fundme but I'm Nigerian (same for kickstarter, paypal) and patrion is a monthly type payment and I don't have anything to offer in return
// And so I'd kindly appritiate an amazon gift card of 150$ peas and zanku
//Sorry for the long rant hope you're having a better year than me and enjoy minecraft dungeons without shitty controls