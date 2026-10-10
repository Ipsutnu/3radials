#include "pch.h"

#include "Logger.h"

#include <shlobj.h>
#include <filesystem>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

bool ParseBool(const std::string& value)
{
    return
        value == "true" ||
        value == "TRUE" ||
        value == "True" ||
        value == "1" ||
        value == "yes" ||
        value == "YES";
}

std::filesystem::path GetConfigPath()
{
    // Obtém a pasta 'Data/SKSE/Plugins' diretamente via CommonLibSSE
    // Caso a pasta não exista no disco, o filesystem cria a estrutura.
    auto path = SKSE::log::log_directory(); 
    if (path) {
        // SKSE::log::log_directory() geralmente aponta para "My Games/Skyrim Special Edition/SKSE"
        // Para salvar diretamente na pasta do jogo Data/SKSE/Plugins:
        return std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" /
            "p-radials" / "p-radials.ini";
    }

    // Fallback relativo seguro
    return std::filesystem::path("Data") / "SKSE" / "Plugins" /
        "p-radials" / "p-radials.ini";
}


std::string KeyToString(int key)
{
    // F1 - F24
    if (key >= VK_F1 && key <= VK_F24)
    {
        return "F" + std::to_string(
            key - VK_F1 + 1);
    }

    // Letras A-Z
    if (key >= 'A' && key <= 'Z')
    {
        return std::string(
            1,
            static_cast<char>(key));
    }

    // Números 0-9
    if (key >= '0' && key <= '9')
    {
        return std::string(
            1,
            static_cast<char>(key));
    }

    // Teclas especiais
    if (key == VK_INSERT)
        return "INSERT";

    if (key == VK_DELETE)
        return "DELETE";

    if (key == VK_HOME)
        return "HOME";

    if (key == VK_END)
        return "END";

    if (key == VK_PRIOR)
        return "PAGEUP";

    if (key == VK_NEXT)
        return "PAGEDOWN";

    if (key == VK_UP)
        return "UP";

    if (key == VK_DOWN)
        return "DOWN";

    if (key == VK_LEFT)
        return "LEFT";

    if (key == VK_RIGHT)
        return "RIGHT";

    if (key == VK_SPACE)
        return "SPACE";

    if (key == VK_TAB)
        return "TAB";

    if (key == VK_RETURN)
        return "ENTER";

    if (key == VK_ESCAPE)
        return "ESC";

    return "UNKNOWN";
}

void ToUpperBuffer(char* buffer)
{
    if (!buffer)
        return;

    for (char* p = buffer; *p; ++p)
    {
        *p = static_cast<char>(
            std::toupper(
                static_cast<unsigned char>(*p)
            )
        );
    }
}


int ParseKey(const std::string& key)
{
    std::string k = key;

    // Uppercase
    for (char& c : k)
    {
        c = static_cast<char>(
            std::toupper(
                static_cast<unsigned char>(c)));
    }

    // Letras A-Z
    if (k.size() == 1 &&
        k[0] >= 'A' &&
        k[0] <= 'Z')
    {
        return k[0];
    }

    // Números 0-9
    if (k.size() == 1 &&
        k[0] >= '0' &&
        k[0] <= '9')
    {
        return k[0];
    }

    // F1 - F24
    if (k[0] == 'F' &&
        k.size() <= 3)
    {
        try
        {
            const int number =
                std::stoi(k.substr(1));

            if (number >= 1 &&
                number <= 24)
            {
                return VK_F1 + (number - 1);
            }
        }
        catch (...)
        {
        }
    }

    // Teclas especiais
    if (k == "INSERT")
        return VK_INSERT;

    if (k == "DELETE" || k == "DEL")
        return VK_DELETE;

    if (k == "HOME")
        return VK_HOME;

    if (k == "END")
        return VK_END;

    if (k == "PAGEUP")
        return VK_PRIOR;

    if (k == "PAGEDOWN")
        return VK_NEXT;

    if (k == "UP")
        return VK_UP;

    if (k == "DOWN")
        return VK_DOWN;

    if (k == "LEFT")
        return VK_LEFT;

    if (k == "RIGHT")
        return VK_RIGHT;

    if (k == "SPACE")
        return VK_SPACE;

    if (k == "TAB")
        return VK_TAB;

    if (k == "ENTER")
        return VK_RETURN;

    if (k == "ESC" || k == "ESCAPE")
        return VK_ESCAPE;

    // Se inválida, volta para G
    return VK_F3;
}

static std::filesystem::path GetLogDirectory()
{
    wchar_t* buffer = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &buffer))) {
        std::filesystem::path path(buffer);
        CoTaskMemFree(buffer);

        path /= "My Games";
        path /= "Skyrim Special Edition";
        path /= "SKSE";
        return path;
    }
    return {};
}

void Logger::Initialize()
{
    auto directory = GetLogDirectory();
    if (directory.empty()) {
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        return;
    }

    auto logPath = directory / "p-radials.log";

    // Reescreve o log a cada nova inicialização do jogo
    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
    auto msvcSink = std::make_shared<spdlog::sinks::msvc_sink_mt>();

    auto logger = std::make_shared<spdlog::logger>(
        "global", 
        spdlog::sinks_init_list{ fileSink, msvcSink }
    );

    // Registra como padrão (NÃO chame spdlog::register_logger aqui!)
    spdlog::set_default_logger(logger);

    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::info); // Força flush automático para INFO, WARN e ERROR

    // Garante que o spdlog descarregue o buffer para o disco a cada 1 segundo (sem travar as threads do jogo)
    spdlog::flush_every(std::chrono::seconds(1));

    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

    spdlog::info("========================================");
    spdlog::info("p-radials by Preguissoso - START");
    spdlog::info("========================================");
}
