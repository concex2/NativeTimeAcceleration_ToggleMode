#include "PCH.h"

// Estado global para el control de la intercepción
static std::atomic<bool> g_isTurboActive(true);
static int32_t g_turboValue = 10800; // Por defecto: 3 horas in-game por tic
static const int32_t g_vanillaValue = 3600; // Valor original de Starfield (1 hora in-game por tic)
static int g_modifierKey = 16; // 'Shift' por defecto
static int g_mainKey = 84;     // 'T' por defecto

// Función auxiliar para el Micro-Parche Epsilon (-1s)
// Evita que el motor aborte por imprecisión flotante sin perder bloques de 30 minutos (1800s)
static inline int32_t GetEpsilonValue(int32_t baseSeconds) {
    return (baseSeconds > 1) ? (baseSeconds - 1) : baseSeconds;
}

static void InitializeLogging() {
    char profilePath[MAX_PATH];
    ExpandEnvironmentStringsA("%USERPROFILE%", profilePath, MAX_PATH);
    std::filesystem::path logPath = std::filesystem::path(profilePath) / "Documents" / "My Games" / "Starfield" / "SFSE" / "Logs" / "NativeTimeAcceleration.log";

    auto file_sink = std::make_shared<logger::sinks::basic_file_sink_mt>(logPath.string(), true);
    auto log = std::make_shared<logger::logger>("global", file_sink);

    log->set_level(logger::level::info);
    log->flush_on(logger::level::info);
    logger::set_default_logger(log);

    logger::info("NativeTimeAcceleration v2.0.1 inicializado. Arquitectura asíncrona con combinación de teclas preparada.");
}

static void ReadConfiguration() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(GetModuleHandleA("NativeTimeAcceleration.dll"), buffer, MAX_PATH);
    std::string iniPath(buffer);
    size_t pos = iniPath.find_last_of('.');
    if (pos != std::string::npos) {
        iniPath = iniPath.substr(0, pos) + ".ini";
    }
    else {
        iniPath += ".ini";
    }

    // Procesar la velocidad base (Ahora basada en Horas in-game por salto visual)
    int userHours = GetPrivateProfileIntA("Settings", "HoursPerTick", 3, iniPath.c_str());
    
    switch (userHours) {
    case 2:
        g_turboValue = 7200;  // 2 horas
        break;
    case 3:
        g_turboValue = 10800; // 3 horas
        break;
    case 4:
        g_turboValue = 14400; // 4 horas
        break;
    default:
        g_turboValue = 10800;
        logger::warn("Valor de horas inválido en INI. Fallback conservador (3 Horas / 10800).");
        break;
    }

    // Procesar los atajos de teclado
    g_modifierKey = GetPrivateProfileIntA("Hotkeys", "ModifierKey", 16, iniPath.c_str());
    g_mainKey = GetPrivateProfileIntA("Hotkeys", "MainKey", 84, iniPath.c_str());

    logger::info("Configuración cargada. Valor Turbo: {} ({} Horas). Atajo: Modificador [{}] + Principal [{}]", g_turboValue, userHours, g_modifierKey, g_mainKey);
}

static void HotkeyMonitorLoop() {
    bool wasTogglePressed = false;
    bool wasSpeedKeyPressed = false;
    
    while (true) {
        // Filtrar ejecución: Solo procesar si la ventana de Starfield es la ventana activa (previene disparos en Alt-Tab)
        HWND foregroundWindow = GetForegroundWindow();
        HWND starfieldWindow = FindWindowA("Starfield", NULL);
        
        if (foregroundWindow != NULL && foregroundWindow == starfieldWindow) {
            
            bool modifierSatisfied = (g_modifierKey == 0) || (GetAsyncKeyState(g_modifierKey) & 0x8000);
            
            // ---------------------------------------------------------
            // 1. LÓGICA DE INTERRUPTOR PRINCIPAL (TOGGLE TURBO/VANILLA)
            // ---------------------------------------------------------
            bool mainKeySatisfied = (GetAsyncKeyState(g_mainKey) & 0x8000);

            if (modifierSatisfied && mainKeySatisfied) {
                if (!wasTogglePressed) {
                    wasTogglePressed = true;
                    
                    // Invertir el modo actual (alternará entre Vanilla y el último g_turboValue registrado)
                    g_isTurboActive = !g_isTurboActive;
                    
                    auto gmstCollection = RE::GameSettingCollection::GetSingleton();
                    if (gmstCollection) {
                        int32_t baseValue = g_isTurboActive ? g_turboValue : g_vanillaValue;
                        int32_t newValue = GetEpsilonValue(baseValue);
                        gmstCollection->SetSetting("iSecondsToSleepPerUpdate", newValue);
                        
                        logger::info("INTERRUPTOR ACCIONADO: Modo Turbo {}. iSecondsToSleepPerUpdate cambiado a {}s (Micro-Epsilon -1s)", 
                            g_isTurboActive ? "ACTIVADO" : "DESACTIVADO", newValue);
                    }
                }
            }
            else {
                wasTogglePressed = false;
            }

            // ---------------------------------------------------------
            // 2. LÓGICA DE CAMBIO DE VELOCIDAD POR HORAS VISUALES (1, 2, 3, 4)
            // ---------------------------------------------------------
            if (modifierSatisfied && !mainKeySatisfied) {
                // Comprobar estado de los números 1 al 4 (Teclado principal y NumPad)
                bool key1 = (GetAsyncKeyState(0x31) & 0x8000) || (GetAsyncKeyState(0x61) & 0x8000);
                bool key2 = (GetAsyncKeyState(0x32) & 0x8000) || (GetAsyncKeyState(0x62) & 0x8000);
                bool key3 = (GetAsyncKeyState(0x33) & 0x8000) || (GetAsyncKeyState(0x63) & 0x8000);
                bool key4 = (GetAsyncKeyState(0x34) & 0x8000) || (GetAsyncKeyState(0x64) & 0x8000);

                if (key1 || key2 || key3 || key4) {
                    if (!wasSpeedKeyPressed) {
                        wasSpeedKeyPressed = true;
                        
                        auto gmstCollection = RE::GameSettingCollection::GetSingleton();
                        
                        if (key1) {
                            // Forzar apagado (Modo Vanilla - Saltos visuales de 1 Hora)
                            g_isTurboActive = false;
                            int32_t vanillaEpsilon = GetEpsilonValue(g_vanillaValue);
                            logger::info("AUTO-APAGADO: Modo Vanilla FORZADO (Tecla 1). GMST cambiado a {}s.", vanillaEpsilon);
                            
                            if (gmstCollection) {
                                gmstCollection->SetSetting("iSecondsToSleepPerUpdate", vanillaEpsilon);
                            }
                        } 
                        else {
                            // Forzar encendido y aplicar el nuevo salto de horas
                            int32_t newTurboValue = g_turboValue;
                            if (key2) newTurboValue = 7200; 
                            else if (key3) newTurboValue = 10800;  
                            else if (key4) newTurboValue = 14400;  

                            g_turboValue = newTurboValue;
                            g_isTurboActive = true;

                            int32_t turboEpsilon = GetEpsilonValue(g_turboValue);
                            logger::info("AUTO-ENCENDIDO / ACTUALIZACIÓN: Velocidad Turbo fijada en {}s (Micro-Epsilon: {}s) y ACTIVADA.", g_turboValue, turboEpsilon);

                            if (gmstCollection) {
                                gmstCollection->SetSetting("iSecondsToSleepPerUpdate", turboEpsilon);
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
            // Si la ventana no está activa, reiniciamos estados
            wasTogglePressed = false;
            wasSpeedKeyPressed = false;
        }
        
        // Pausa de 50 milisegundos para prevenir saturación en los ciclos del CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

static void MessageCallback(SFSE::MessagingInterface::Message* a_msg) {
    if (a_msg->type == SFSE::MessagingInterface::kPostDataLoad) {
        logger::info("Señal kPostDataLoad recibida. Estableciendo configuración inicial.");

        auto gmstCollection = RE::GameSettingCollection::GetSingleton();
        if (gmstCollection) {
            // Se inyecta el valor Turbo por defecto al iniciar
            int32_t initialEpsilon = GetEpsilonValue(g_turboValue);
            bool result = gmstCollection->SetSetting("iSecondsToSleepPerUpdate", initialEpsilon);
            
            if (result) {
                logger::info("ÉXITO: Parámetro base establecido a {}s (Modo Turbo Activo).", initialEpsilon);
                
                // Desplegar el hilo secundario para evitar bloquear el hilo principal del motor
                std::thread(HotkeyMonitorLoop).detach();
                logger::info("Hilo secundario de intercepción de teclado desplegado correctamente.");
            }
            else {
                logger::error("FALLO CRÍTICO: La función SetSetting retornó falso en la inyección inicial.");
            }
        }
        else {
            logger::error("FALLO CRÍTICO: El singleton GameSettingCollection es un puntero nulo.");
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
        logger::error("Fallo de infraestructura: No se pudo registrar el Listener.");
        return false;
    }

    logger::info("Listener de eventos SFSE registrado con éxito.");
    return true;
}