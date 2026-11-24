#include "GameData.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "Json.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {

[[noreturn]] void dataError(const std::string& where, const std::string& msg) {
    throw std::runtime_error("game data error in " + where + ": " + msg);
}

Slot parseSlot(const std::string& s, const std::string& where) {
    if (s == "morning") return Slot::Morning;
    if (s == "afternoon") return Slot::Afternoon;
    if (s == "night") return Slot::Night;
    dataError(where, "unknown slot \"" + s + "\" (use morning/afternoon/night)");
}

Phase parsePhase(const std::string& s, const std::string& where) {
    if (s == "early") return Phase::Early;
    if (s == "late") return Phase::Late;
    dataError(where, "unknown phase \"" + s + "\" (use early/late)");
}

Outcome parseOutcome(const json::Value& v, size_t loverCount, const std::string& where) {
    Outcome o;
    if (const json::Value* t = v.find("text")) o.text = t->asString();
    if (const json::Value* m = v.find("money")) o.money = m->asInt();
    if (const json::Value* list = v.find("affection")) {
        for (const json::Value& a : list->asArray()) {
            AffectionChange c;
            const json::Value& target = a.at("target");
            if (target.isString()) {
                if (target.asString() != "all") dataError(where, "affection target must be a lover index or \"all\"");
                c.target = kAllLovers;
            } else {
                c.target = target.asInt();
                if (c.target < 0 || static_cast<size_t>(c.target) >= loverCount)
                    dataError(where, "affection target " + std::to_string(c.target) + " is out of range");
            }
            c.amount = a.at("amount").asInt();
            o.affection.push_back(c);
        }
    }
    return o;
}

Event parseEvent(const json::Value& v, size_t loverCount, const std::string& jobId) {
    Event e;
    e.id = v.at("id").asString();
    std::string where = "event \"" + e.id + "\" (job " + jobId + ")";
    e.slot = parseSlot(v.at("slot").asString(), where);
    e.phase = parsePhase(v.at("phase").asString(), where);
    e.text = v.at("text").asString();
    if (const json::Value* choices = v.find("choices")) {
        e.hasChoice = true;
        e.yes = parseOutcome(choices->at("y"), loverCount, where + " choice y");
        e.no = parseOutcome(choices->at("n"), loverCount, where + " choice n");
    } else {
        e.outcome = parseOutcome(v.at("outcome"), loverCount, where + " outcome");
    }
    return e;
}

std::map<std::string, std::string> parseStringMap(const json::Value& v) {
    std::map<std::string, std::string> out;
    for (const auto& kv : v.asObject())
        if (kv.second.isString()) out[kv.first] = kv.second.asString();
    return out;
}

GameData parseGameData(const json::Value& root) {
    GameData d;

    const json::Value& s = root.at("settings");
    d.settings.typingDelayMs = s.at("typingDelayMs").asInt();
    d.settings.endingTurn = s.at("endingTurn").asInt();
    d.settings.lateGameStartTurn = s.at("lateGameStartTurn").asInt();
    d.settings.trueEndingMinBalance = s.at("trueEnding").at("minBalance").asInt();
    d.settings.trueEndingMinAffection = s.at("trueEnding").at("minAffection").asInt();
    if (d.settings.endingTurn < 1) dataError("settings", "endingTurn must be at least 1");

    d.ui = parseStringMap(root.at("ui"));
    d.slotNames = parseStringMap(root.at("ui").at("slotNames"));
    d.endings = parseStringMap(root.at("endings"));
    for (const char* key : {"true", "noMoney", "noLove", "broke"})
        if (!d.endings.count(key)) dataError("endings", std::string("missing \"") + key + "\"");

    for (const json::Value& v : root.at("difficulties").asArray()) {
        Difficulty diff;
        diff.name = v.at("name").asString();
        diff.eventPoolRatio = v.at("eventPoolRatio").asNumber();
        if (diff.eventPoolRatio <= 0.0 || diff.eventPoolRatio > 1.0)
            dataError("difficulty " + diff.name, "eventPoolRatio must be in (0, 1]");
        d.difficulties.push_back(diff);
    }
    if (d.difficulties.empty()) dataError("difficulties", "at least one difficulty is required");

    const json::Value& genders = root.at("genders");
    for (const json::Value& v : genders.at("player").asArray())
        d.playerGenders.push_back({v.at("id").asString(), v.at("name").asString()});
    for (const json::Value& v : genders.at("lover").asArray())
        d.loverGenders.push_back({v.at("id").asString(), v.at("name").asString()});
    if (d.playerGenders.empty() || d.loverGenders.empty())
        dataError("genders", "player and lover need at least one option");
    if (const json::Value* words = root.find("genderWords")) {
        for (const auto& kv : words->asObject()) {
            GenderWord w;
            w.by = kv.second.at("by").asString();
            if (w.by != "player" && w.by != "lover" && w.by != "lover+player")
                dataError("genderWords." + kv.first, "by must be player, lover or lover+player");
            w.values = parseStringMap(kv.second.at("values"));
            d.genderWords[kv.first] = w;
        }
    }

    for (const json::Value& v : root.at("lovers").asArray())
        d.lovers.push_back({v.at("color").asString(), v.at("confess").asString()});
    if (d.lovers.empty()) dataError("lovers", "at least one lover is required");

    for (const json::Value& v : root.at("jobs").asArray()) {
        Job job;
        job.id = v.at("id").asString();
        job.name = v.at("name").asString();
        job.startBalance = v.at("startBalance").asInt();
        job.endingIntro = v.at("endingIntro").asString();
        for (const auto& kv : v.at("stories").asObject()) {
            std::vector<std::string> pages;
            for (const json::Value& page : kv.second.asArray()) pages.push_back(page.asString());
            job.stories[std::stoi(kv.first)] = pages;
        }
        for (const json::Value& ev : v.at("events").asArray())
            job.events.push_back(parseEvent(ev, d.lovers.size(), job.id));
        if (const json::Value* confess = v.find("confess")) {
            for (const json::Value& line : confess->asArray()) job.confess.push_back(line.asString());
            if (job.confess.size() != d.lovers.size())
                dataError("job " + job.id, "confess must have one line per lover");
        }
        if (const json::Value* endings = v.find("endings")) job.endings = parseStringMap(*endings);
        if (const json::Value* te = v.find("trueEnding"))
            if (const json::Value* mb = te->find("minBalance")) job.trueEndingMinBalance = mb->asInt();
        d.jobs.push_back(std::move(job));
    }
    if (d.jobs.empty()) dataError("jobs", "at least one job is required");

    return d;
}

fs::path executableDir(const char* argv0) {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) return fs::path(buf).parent_path();
#endif
    if (argv0 && *argv0) {
        std::error_code ec;
        fs::path p = fs::absolute(argv0, ec);
        if (!ec) return p.parent_path();
    }
    return fs::current_path();
}

}  // namespace

const std::string& GameData::text(const std::string& key) const {
    auto it = ui.find(key);
    if (it == ui.end()) throw std::runtime_error("game data error: missing ui text \"" + key + "\"");
    return it->second;
}

GameData loadGameData(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot open game data file: " + path.u8string());
    std::stringstream buf;
    buf << file.rdbuf();
    try {
        return parseGameData(json::parse(buf.str()));
    } catch (const std::exception& e) {
        throw std::runtime_error(path.u8string() + ": " + e.what());
    }
}

fs::path findDataFile(const char* argv0) {
    const fs::path relative = fs::path("data") / "game.json";
    std::vector<fs::path> bases = {fs::current_path()};
    fs::path dir = executableDir(argv0);
    for (int i = 0; i < 4 && !dir.empty(); ++i) {
        bases.push_back(dir);
        if (dir == dir.parent_path()) break;
        dir = dir.parent_path();
    }
    for (const fs::path& base : bases) {
        std::error_code ec;
        if (fs::exists(base / relative, ec)) return base / relative;
    }
    return relative;  // loadGameData reports the failure
}

namespace {

std::vector<std::string> splitOptions(const std::string& s) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        size_t bar = s.find('|', start);
        out.push_back(s.substr(start, bar == std::string::npos ? std::string::npos : bar - start));
        if (bar == std::string::npos) break;
        start = bar + 1;
    }
    return out;
}

}  // namespace

std::string applyGender(const GameData& data, const std::string& text, size_t playerGender, size_t loverGender) {
    const std::string& p = data.playerGenders[playerGender].id;
    const std::string& l = data.loverGenders[loverGender].id;
    std::string out;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t open = text.find('{', pos);
        size_t close = open == std::string::npos ? std::string::npos : text.find('}', open);
        if (close == std::string::npos) {
            out.append(text, pos, std::string::npos);
            break;
        }
        out.append(text, pos, open - pos);
        std::string inner = text.substr(open + 1, close - open - 1);
        std::string replacement = "{" + inner + "}";
        if (inner.size() >= 2 && (inner[0] == 'P' || inner[0] == 'L') && inner[1] == ':') {
            std::vector<std::string> options = splitOptions(inner.substr(2));
            size_t index = inner[0] == 'P' ? playerGender : loverGender;
            if (index < options.size()) replacement = options[index];
        } else {
            auto it = data.genderWords.find(inner);
            if (it != data.genderWords.end()) {
                const GenderWord& w = it->second;
                std::string key = w.by == "player" ? p : w.by == "lover" ? l : l + ":" + p;
                auto v = w.values.find(key);
                if (v != w.values.end()) replacement = v->second;
            }
        }
        out += replacement;
        pos = close + 1;
    }
    return out;
}

std::string format(const std::string& tmpl, const std::vector<std::pair<std::string, std::string>>& vars) {
    std::string out = tmpl;
    for (const auto& kv : vars) {
        const std::string token = "{" + kv.first + "}";
        size_t pos = 0;
        while ((pos = out.find(token, pos)) != std::string::npos) {
            out.replace(pos, token.size(), kv.second);
            pos += kv.second.size();
        }
    }
    return out;
}
