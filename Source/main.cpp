// Capitalism - a money vs. romance text adventure.
// All game text and balance values live in data/game.json.
//
// Options:
//   --data <path>   use a specific game data file
//   --fast          disable the typewriter effect
//   --seed <n>      fixed random seed (reproducible runs)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <random>
#include <string>

#include "Console.h"
#include "Game.h"
#include "GameData.h"

int main(int argc, char** argv) {
    console::init();

    std::filesystem::path dataPath;
    bool fast = false;
    bool hasSeed = false;
    unsigned seed = 0;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
            dataPath = std::filesystem::u8path(argv[++i]);
        } else if (std::strcmp(argv[i], "--fast") == 0) {
            fast = true;
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = static_cast<unsigned>(std::strtoul(argv[++i], nullptr, 10));
            hasSeed = true;
        }
    }
    if (dataPath.empty()) dataPath = findDataFile(argc > 0 ? argv[0] : nullptr);
    if (!hasSeed) seed = std::random_device{}();

    try {
        GameData data = loadGameData(dataPath);
        console::setTypingDelay(fast ? 0 : data.settings.typingDelayMs);
        Game game(data, seed);
        game.run();
    } catch (const console::InputClosed&) {
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
    return 0;
}
