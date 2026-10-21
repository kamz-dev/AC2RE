#pragma once
// Where the owner's Assassin's Creed II install is. The port reads game data from there at run time and never
// ships any of it.

#include <filesystem>
#include <optional>

namespace port {

// AC2_GAME_DIR if set, else a folder "Assassin's Creed II" next to the repository (searched upwards from `start`).
std::optional<std::filesystem::path> findGameDir(const std::filesystem::path& start = std::filesystem::current_path());

}  // namespace port
