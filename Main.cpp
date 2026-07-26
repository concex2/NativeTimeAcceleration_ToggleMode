#include "PCH.h"
#include <RE/G/GameSettingCollection.h>

#include <thread>
#include <atomic>
#include <chrono>

// Global state for interception control
static std::atomic<bool> g_isTurboActive(true);
static int32_t g_turboValue = 10800; // Default: 3 in-game hours per tick
static const int32_t g_vanillaValue = 1800; // Original Starfield value (Half an in-game hour per tick)
static int g_modifierKey = 16; // 'Shift' by default
static int g_mainKey = 84;     // 'T' by default

static void InitializeLogging() {
    char profilePath[MAX_PATH];
    ExpandEnvironmentStringsA("%USERPROFILE%", profilePath, MAX_PATH);
    std::filesystem::path logPath = std::filesystem::path(profilePath) / "Documents" / "My Games" / "Starfield" / "SFSE" / "Logs" / "NativeTimeAcceleration_ToggleMode.log";

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
    auto logger = std::make_shared<spdlog::logger>("global", file_sink);

    logger->set_level(spdlog::level::info);
    logger->flush_on(spdlog::level::info);
    spdlog::set_default_logger(logger);

    spdlog::info("NativeTimeAcceleration_ToggleMode v2.0.0 initialized. Asynchronous architecture with key combination ready.");
}

static void ReadConfiguration() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(GetModuleHandleA("NativeTimeAcceleration_ToggleMode.dll"), buffer, MAX_PATH);
    std::string iniPath(buffer);
    size_t pos = iniPath.find_last_of('.');
    if (pos != std::string::npos) {
        iniPath = iniPath.substr(0, pos) + ".ini";
    }
    else {
        iniPath += ".ini";
    }

    // Process base speed (Now based on in-game Hours per visual jump)
    int userHours = GetPrivateProfileIntA("Settings", "HoursPerTick", 3, iniPath.c_str());
    
    switch (userHours) {
    case 2:
        g_turboValue = 7200;  // 2 hours
        break;
    case 3:
        g_turboValue = 10800; // 3 hours
        break;
    case 4:
        g_turboValue = 14400; // 4 hours
        break;
    default:
        g_turboValue = 10800;
        spdlog::warn("Invalid hours value in INI. Conservative fallback (3 Hours / 10800).");
        break;
    }

    // Process keyboard hotkeys
    g_modifierKey = GetPrivateProfileIntA("Hotkeys", "ModifierKey", 16, iniPath.c_str());
    g_mainKey = GetPrivateProfileIntA("Hotkeys", "MainKey", 84, iniPath.c_str());

    spdlog::info("Configuration loaded. Turbo Value: {} ({} Hours). Hotkey: Modifier [{}] + Main [{}]", g_turboValue, userHours, g_modifierKey, g_mainKey);
}

static void HotkeyMonitorLoop() {
    bool wasTogglePressed = false;
    bool wasSpeedKeyPressed = false;
    
    while (true) {
        // Execution filter: Only process if the Starfield window is the active window (prevents triggers on Alt-Tab)
        HWND foregroundWindow = GetForegroundWindow();
        HWND starfieldWindow = FindWindowA("Starfield", NULL);
        
        if (foregroundWindow != NULL && foregroundWindow == starfieldWindow) {
            
            bool modifierSatisfied = (g_modifierKey == 0) || (GetAsyncKeyState(g_modifierKey) & 0x8000);
            
            // ---------------------------------------------------------
            // 1. MAIN TOGGLE LOGIC (TURBO/VANILLA)
            // ---------------------------------------------------------
            bool mainKeySatisfied = (GetAsyncKeyState(g_mainKey) & 0x8000);

            if (modifierSatisfied && mainKeySatisfied) {
                if (!wasTogglePressed) {
                    wasTogglePressed = true;
                    
                    // Invert the current mode (will toggle between Vanilla and the last registered g_turboValue)
                    g_isTurboActive = !g_isTurboActive;
                    
                    auto gmstCollection = RE::GameSettingCollection::GetSingleton();
                    if (gmstCollection) {
                        int32_t newValue = g_isTurboActive ? g_turboValue : g_vanillaValue;
                        gmstCollection->SetSetting("iSecondsToSleepPerUpdate", newValue);
                        
                        spdlog::info("TOGGLE TRIGGERED: Turbo Mode {}. iSecondsToSleepPerUpdate changed to {}", 
                            g_isTurboActive ? "ENABLED" : "DISABLED", newValue);
                    }
                }
            }
            else {
                wasTogglePressed = false;
            }

            // ---------------------------------------------------------
            // 2. VISUAL HOURS SPEED CHANGE LOGIC (1, 2, 3, 4)
            // ---------------------------------------------------------
            if (modifierSatisfied && !mainKeySatisfied) {
                // Check state of numbers 1 to 4 (Main keyboard and NumPad)
                bool key1 = (GetAsyncKeyState(0x31) & 0x8000) || (GetAsyncKeyState(0x61) & 0x8000);
                bool key2 = (GetAsyncKeyState(0x32) & 0x8000) || (GetAsyncKeyState(0x62) & 0x8000);
                bool key3 = (GetAsyncKeyState(0x33) & 0x8000) || (GetAsyncKeyState(0x63) & 0x8000);
                bool key4 = (GetAsyncKeyState(0x34) & 0x8000) || (GetAsyncKeyState(0x64) & 0x8000);

                if (key1 || key2 || key3 || key4) {
                    if (!wasSpeedKeyPressed) {
                        wasSpeedKeyPressed = true;
                        
                        auto gmstCollection = RE::GameSettingCollection::GetSingleton();
                        
                        if (key1) {
                            // Force off (Vanilla Mode - 1 Hour visual jumps)
                            g_isTurboActive = false;
                            spdlog::info("AUTO-OFF: Vanilla Mode FORCED (Key 1).");
                            
                            if (gmstCollection) {
                                gmstCollection->SetSetting("iSecondsToSleepPerUpdate", g_vanillaValue);
                            }
                        } 
                        else {
                            // Force on and apply the new hour jump
                            int32_t newTurboValue = g_turboValue;
                            if (key2) newTurboValue = 7200; 
                            else if (key3) newTurboValue = 10800;  
                            else if (key4) newTurboValue = 14400;  

                            g_turboValue = newTurboValue;
                            g_isTurboActive = true;

                            spdlog::info("AUTO-ON / UPDATE: Turbo Speed set to {} and Turbo Mode ENABLED", g_turboValue);

                            if (gmstCollection) {
                                gmstCollection->SetSetting("iSecondsToSleepPerUpdate", g_turboValue);
                            }
                        }
                    }
                }
                else {
                    wasSpeedKeyPressed = false;
                }
            }
            else if (!modifierSatisfied) {
                wasSpeedKeyPressed = false;
            }
        }
        else {
            // If the window is not active, reset states
            wasTogglePressed = false;
            wasSpeedKeyPressed = false;
        }
        
        // 50 milliseconds pause to prevent CPU cycle saturation
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

static void MessageCallback(SFSE::MessagingInterface::Message* a_msg) {
    if (a_msg->type == SFSE::MessagingInterface::kPostDataLoad) {
        spdlog::info("kPostDataLoad signal received. Setting initial configuration.");

        auto gmstCollection = RE::GameSettingCollection::GetSingleton();
        if (gmstCollection) {
            // Inject the default Turbo value on startup
            bool result = gmstCollection->SetSetting("iSecondsToSleepPerUpdate", g_turboValue);
            
            if (result) {
                spdlog::info("SUCCESS: Base parameter set to {} (Turbo Mode Active).", g_turboValue);
                
                // Deploy the secondary thread to avoid blocking the main engine thread
                std::thread(HotkeyMonitorLoop).detach();
                spdlog::info("Secondary keyboard interception thread deployed successfully.");
            }
            else {
                spdlog::error("CRITICAL FAILURE: SetSetting function returned false during initial injection.");
            }
        }
        else {
            spdlog::error("CRITICAL FAILURE: GameSettingCollection singleton is a null pointer.");
        }
    }
}

SFSE_PLUGIN_PRELOAD(const SFSE::PreLoadInterface* a_sfse)
{
    SFSE::Init(a_sfse);
    return true;
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* a_sfse)
{
    InitializeLogging();
    ReadConfiguration();
    SFSE::Init(a_sfse);

    auto messaging = SFSE::GetMessagingInterface();
    if (!messaging->RegisterListener(MessageCallback)) {
        spdlog::error("Infrastructure failure: Could not register the Listener.");
        return false;
    }

    spdlog::info("SFSE event listener registered successfully.");
    return true;
}