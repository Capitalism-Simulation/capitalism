#pragma once

#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "GameData.h"

struct Player {
    std::string name;
    size_t job = 0;         // index into GameData::jobs
    size_t difficulty = 0;  // index into GameData::difficulties
    size_t gender = 0;      // index into GameData::playerGenders
    size_t loverGender = 0; // index into GameData::loverGenders
};

class Game {
public:
    Game(const GameData& data, unsigned seed);
    void run();

private:
    const GameData& data_;
    std::mt19937 rng_;
    Player player_;
    int balance_ = 0;
    std::vector<int> affection_;    // one value per lover
    std::set<std::string> usedEvents_;
    std::map<std::string, int> inventory_;  // item id -> count
    int turn_ = 0;                  // 0 = prologue, 1..endingTurn-1 = event turns

    const Job& job() const { return data_.jobs[player_.job]; }

    void setupPlayer();
    size_t chooseFromList(const std::string& title, const std::vector<std::string>& options);
    void showStatus();
    void showStories();
    void playEvent();
    const Event* pickEvent(Slot slot, Phase phase);
    void applyOutcome(const Outcome& outcome);
    void runEnding();
    const std::string& endingText(const std::string& key) const;  // job override, else shared text
    std::string localize(const std::string& text) const;          // resolves gender tokens
    bool showAffection() const;
    bool hasItem(const std::string& id) const;
    std::string inventoryText() const;
};
