#pragma once

#include <SFSE/SFSE.h>
#include <RE/G/GameSettingCollection.h>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <Windows.h>

// Encabezados de la biblioteca estándar de C++ para concurrencia, tiempo y cadenas
#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <string_view>
#include <filesystem>

using namespace std::literals;

namespace logger = spdlog;