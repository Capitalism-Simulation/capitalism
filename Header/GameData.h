#pragma once

// Game content loaded from data/game.json: settings, UI text, jobs, events and endings.

#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

enum class Slot { Morning, Afternoon, Night };
enum class Phase { Early, Late };  // Early: Day1-3 (money only), Late: Day4-7 (affection)

constexpr int kAllLovers = -1;  // AffectionChange target meaning "every lover"

struct AffectionChange {
    int target = 0;  // lover index, or kAllLovers
    int amount = 0;
};

struct Outcome {
    std::string text;
    int money = 0;
    std::vector<AffectionChange> affection;
};

struct Event {
    std::string id;
    Slot slot = Slot::Morning;
    Phase phase = Phase::Early;
    std::string text;
    bool hasChoice = false;
    Outcome yes;      // used when hasChoice
    Outcome no;       // used when hasChoice
    Outcome outcome;  // used when !hasChoice
};

struct Job {
    std::string id;
    std::string name;
    int startBalance = 0;
    std::map<int, std::vector<std::string>> stories;  // turn -> pages shown after that turn's event
    std::string endingIntro;
    std::vector<Event> events;

    // Optional per-job overrides of the shared ending content.
    std::vector<std::string> confess;            // one line per lover; empty = use Lover::confess
    std::map<std::string, std::string> endings;  // same keys as GameData::endings
    int trueEndingMinBalance = -1;               // -1 = use Settings::trueEndingMinBalance
};

struct Difficulty {
    std::string name;
    double eventPoolRatio = 1.0;  // share of each event pool (in file order) that can appear
};

struct Lover {
    std::string color;
    std::string confess;
};

struct Settings {
    int typingDelayMs = 20;
    int endingTurn = 21;
    int lateGameStartTurn = 10;
    int trueEndingMinBalance = 50000;
    int trueEndingMinAffection = 4;
};

struct GameData {
    Settings settings;
    std::map<std::string, std::string> ui;
    std::map<std::string, std::string> slotNames;
    std::vector<Difficulty> difficulties;
    std::vector<Lover> lovers;
    std::map<std::string, std::string> endings;
    std::vector<Job> jobs;

    const std::string& text(const std::string& key) const;  // UI string, throws when missing
};

// Throws std::runtime_error with a readable message on missing files or invalid data.
GameData loadGameData(const std::filesystem::path& path);

// Looks for data/game.json next to the working directory or the executable (and its parents).
std::filesystem::path findDataFile(const char* argv0);

// Replaces {name} placeholders, e.g. format("{a} won", {{"a", "100"}}).
std::string format(const std::string& tmpl, const std::vector<std::pair<std::string, std::string>>& vars);
