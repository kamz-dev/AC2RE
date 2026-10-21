#include "port/game_dir.h"

#include <cstdlib>
#include <string>

namespace port {

std::optional<std::filesystem::path> findGameDir(const std::filesystem::path& start) {
    namespace fs = std::filesystem;
    std::string env;
#ifdef _WIN32
    char* buf = nullptr;
    size_t len = 0;
    if (_dupenv_s(&buf, &len, "AC2_GAME_DIR") == 0 && buf) {
        env = buf;
        std::free(buf);
    }
#else
    if (const char* v = std::getenv("AC2_GAME_DIR")) env = v;
#endif
    std::error_code ec;
    if (!env.empty()) {
        fs::path p(env);
        if (fs::is_directory(p, ec)) return p;
        return std::nullopt;
    }
    for (fs::path dir = fs::absolute(start, ec); !dir.empty(); dir = dir.parent_path()) {
        fs::path candidate = dir.parent_path() / "Assassin's Creed II";
        if (fs::is_directory(candidate, ec)) return candidate;
        if (dir == dir.parent_path()) break;
    }
    return std::nullopt;
}

}  // namespace port
