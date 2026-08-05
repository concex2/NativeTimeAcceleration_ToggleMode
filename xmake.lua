-- Reglas comunes de compilación
add_rules("mode.debug", "mode.releasedbg", "mode.release")
add_rules("plugin.vsxmake.autoupdate")

-- Incluimos el subproyecto de CommonLibSF (esto provee spdlog automáticamente)
includes("lib/commonlibsf")

-- Constantes del proyecto
set_project("NativeTimeAcceleration")
set_version("2.0.1")
set_languages("c++23")

-- Definición del Target
target("NativeTimeAcceleration")
    -- La macro mágica de XMake que expone la versión y maneja las dependencias
    add_rules("commonlibsf.plugin", {
        name = "NativeTimeAcceleration",
        author = "concex1",
        description = "Acelera los tiempos de espera in-game alternando dinamicamente entre modo Turbo y Vanilla"
    })

    -- Agregamos nuestros archivos fuente
    add_files("Main.cpp")
    add_headerfiles("PCH.h")
    add_includedirs(".")
    
    -- Cabecera precompilada
    set_pcxxheader("PCH.h")