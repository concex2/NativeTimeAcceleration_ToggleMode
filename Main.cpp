#include "PCH.h"
#include <RE/G/GameSettingCollection.h>

#include <thread>
#include <atomic>
#include <chrono>

// Estado global para el control de la intercepcion
static std::atomic<bool> g_isTurboActive(true);
static int32_t g_turboValue = 10800;
static const int32_t g_vanillaValue = 1800; // Valor original de Starfield (Media hora in-game)
static int g_modifierKey = 16; // 'Shift' por defecto
static int g_mainKey = 84;     // 'T' por defecto

static void InitializeLogging() {
    char profilePath[MAX_PATH];
    ExpandEnvironmentStringsA("%USERPROFILE%", profilePath, MAX_PATH);
    std::filesystem::path logPath = std::filesystem::path(profilePath) / "Documents" / "My Games" / "Starfield" / "SFSE" / "Logs" / "NativeTimeAcceleration_ToggleMode.log";

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
    auto logger = std::make_shared<spdlog::logger>("global", file_sink);

    logger->set_level(spdlog::level::info);
    logger->flush_on(spdlog::level::info);
    spdlog::set_default_logger(logger);

    spdlog::info("NativeTimeAcceleration_ToggleMode v1.0.0 inicializado. Arquitectura asincrona con combinacion de teclas preparada.");
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

    // Procesar la velocidad base
    int userSeconds = GetPrivateProfileIntA("Settings", "WaitSeconds", 4, iniPath.c_str());
    
    switch (userSeconds) {
    case 3:
        g_turboValue = 14400; 
        break;
    case 4:
        g_turboValue = 10800; 
        break;
    case 6:
        g_turboValue = 7200;  
        break;
    default:
        g_turboValue = 10800;
        spdlog::warn("Valor de velocidad invalido en INI. Fallback conservador (10800).");
        break;
    }

    // Procesar los atajos de teclado
    g_modifierKey = GetPrivateProfileIntA("Hotkeys", "ModifierKey", 16, iniPath.c_str());
    g_mainKey = GetPrivateProfileIntA("Hotkeys", "MainKey", 84, iniPath.c_str());

    spdlog::info("Configuracion cargada. Valor Turbo: {}. Atajo de teclado: Modificador [{}] + Principal [{}]", g_turboValue, g_modifierKey, g_mainKey);
}

static void HotkeyMonitorLoop() {
    bool wasKeyPressed = false;
    
    while (true) {
        // Filtrar ejecucion: Solo procesar si la ventana de Starfield es la ventana activa (previene disparos en Alt-Tab)
        HWND foregroundWindow = GetForegroundWindow();
        HWND starfieldWindow = FindWindowA("Starfield", NULL);
        
        if (foregroundWindow != NULL && foregroundWindow == starfieldWindow) {
            
            // Comprobar el estado de ambas teclas
            bool modifierSatisfied = (g_modifierKey == 0) || (GetAsyncKeyState(g_modifierKey) & 0x8000);
            bool mainKeySatisfied = (GetAsyncKeyState(g_mainKey) & 0x8000);

            if (modifierSatisfied && mainKeySatisfied) {
                if (!wasKeyPressed) {
                    wasKeyPressed = true;
                    
                    // Invertir el modo actual
                    g_isTurboActive = !g_isTurboActive;
                    
                    auto gmstCollection = RE::GameSettingCollection::GetSingleton();
                    if (gmstCollection) {
                        int32_t newValue = g_isTurboActive ? g_turboValue : g_vanillaValue;
                        gmstCollection->SetSetting("iSecondsToSleepPerUpdate", newValue);
                        
                        spdlog::info("INTERRUPTOR ACCIONADO: Modo Turbo {}. iSecondsToSleepPerUpdate cambiado a {}", 
                            g_isTurboActive ? "ACTIVADO" : "DESACTIVADO", newValue);
                    }
                }
            }
            else {
                // Si cualquiera de las dos teclas requeridas se suelta, reiniciamos el flag
                wasKeyPressed = false;
            }
        }
        else {
            // Si la ventana no esta activa, reiniciamos el estado para evitar bloqueos
            wasKeyPressed = false;
        }
        
        // Pausa de 50 milisegundos para prevenir saturacion en los ciclos del CPU (polling ligero)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

static void MessageCallback(SFSE::MessagingInterface::Message* a_msg) {
    if (a_msg->type == SFSE::MessagingInterface::kPostDataLoad) {
        spdlog::info("Senal kPostDataLoad recibida. Estableciendo configuracion inicial.");

        auto gmstCollection = RE::GameSettingCollection::GetSingleton();
        if (gmstCollection) {
            // Se inyecta el valor Turbo por defecto al iniciar
            bool result = gmstCollection->SetSetting("iSecondsToSleepPerUpdate", g_turboValue);
            
            if (result) {
                spdlog::info("EXITO: Parametro base establecido a {} (Modo Turbo Activo).", g_turboValue);
                
                // Desplegar el hilo secundario para evitar bloquear el hilo principal del motor
                std::thread(HotkeyMonitorLoop).detach();
                spdlog::info("Hilo secundario de intercepcion de teclado desplegado correctamente.");
            }
            else {
                spdlog::error("FALLO CRITICO: La funcion SetSetting retorno falso en la inyeccion inicial.");
            }
        }
        else {
            spdlog::error("FALLO CRITICO: El singleton GameSettingCollection es un puntero nulo.");
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
        spdlog::error("Fallo de infraestructura: No se pudo registrar el Listener.");
        return false;
    }

    spdlog::info("Listener de eventos SFSE registrado con exito.");
    return true;
}