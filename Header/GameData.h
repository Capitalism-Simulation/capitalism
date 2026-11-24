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
    int trueEndingMinAffection = -1;             // -1 = use Settings::trueEndingMinAffection
};

struct Difficulty {
    std::string name;
    std::string description;
    // Share of each event pool that can appear. Pools are ordered so that affection events
    // always stay and pure-money events go from harshest to most generous; harder levels
    // therefore lose the most generous money events first.
    double eventPoolRatio = 1.0;
    double lossMultiplier = 1.0;  // applied to negative money outcomes
    double gainMultiplier = 1.0;  // applied to positive money outcomes
    int minAffectionDelta = 0;    // added to the affection needed for the true ending
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

struct GenderOption {
    std::string id;  // e.g. "m", "f", "n"
    std::string name;
};

// A word that changes with the chosen genders, used in text as {word}.
struct GenderWord {
    std::string by;                             // "player", "lover" or "lover+player"
    std::map<std::string, std::string> values;  // key: player id, lover id, or "lover:player"
};

struct GameData {
    Settings settings;
    std::vector<GenderOption> playerGenders;
    std::vector<GenderOption> loverGenders;
    std::map<std::string, GenderWord> genderWords;
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

// Resolves gender tokens in game text:
//   {P:male|female}            by player gender (one option per player gender, in data order)
//   {L:male|female|unset}      by lover gender (one option per lover gender, in data order)
//   {word}                     a GameData::genderWords entry
// Any other {token} is left untouched.
std::string applyGender(const GameData& data, const std::string& text, size_t playerGender, size_t loverGender);

// Replaces {name} placeholders, e.g. format("{a} won", {{"a", "100"}}).
std::string format(const std::string& tmpl, const std::vector<std::pair<std::string, std::string>>& vars);
