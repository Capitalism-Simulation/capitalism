#include "Game.h"

#include <algorithm>
#include <cmath>

#include "Console.h"

Game::Game(const GameData& data, unsigned seed)
    : data_(data), rng_(seed), affection_(data.lovers.size(), 0) {}

void Game::run() {
    setupPlayer();
    balance_ = job().startBalance;

    showStatus();
    while (true) {
        if (turn_ >= data_.settings.endingTurn) {
            console::clear();
            runEnding();
            break;
        }
        if (turn_ >= 1) playEvent();
        showStories();
        if (balance_ <= 0) {
            console::clear();
            console::typewrite(localize(endingText("broke")));
            break;
        }
        ++turn_;
        showStatus();
    }
    console::print(data_.text("gameOver") + "\n");
}

size_t Game::chooseFromList(const std::string& title, const std::vector<std::string>& options) {
    std::string prompt = title + "\n";
    for (size_t i = 0; i < options.size(); ++i) prompt += std::to_string(i + 1) + "." + options[i] + "\n";
    prompt += ": ";
    int n = console::askNumber(prompt, 1, static_cast<int>(options.size()), data_.text("invalidNumber"),
                               data_.text("invalidChoice"));
    return static_cast<size_t>(n - 1);
}

void Game::setupPlayer() {
    std::vector<std::string> jobNames, difficultyNames, playerGenderNames, loverGenderNames;
    for (const Job& j : data_.jobs) jobNames.push_back(j.name);
    for (const Difficulty& d : data_.difficulties)
        difficultyNames.push_back(d.description.empty() ? d.name : d.name + " (" + d.description + ")");
    for (const GenderOption& g : data_.playerGenders) playerGenderNames.push_back(g.name);
    for (const GenderOption& g : data_.loverGenders) loverGenderNames.push_back(g.name);

    while (true) {
        do {
            console::print(data_.text("enterName"));
            player_.name = console::readLine();
        } while (player_.name.empty());

        console::clear();
        player_.job = chooseFromList(data_.text("selectJob"), jobNames);
        console::clear();
        player_.difficulty = chooseFromList(data_.text("selectDifficulty"), difficultyNames);
        console::clear();
        player_.gender = chooseFromList(data_.text("selectPlayerGender"), playerGenderNames);
        console::clear();
        player_.loverGender = chooseFromList(data_.text("selectLoverGender"), loverGenderNames);
        console::clear();

        console::print(data_.text("playerInfo") + "\n");
        console::print(format(data_.text("infoName"), {{"name", player_.name}}) + "\n");
        console::print(format(data_.text("infoJob"), {{"job", job().name}}) + "\n");
        console::print(format(data_.text("infoDifficulty"),
                              {{"difficulty", data_.difficulties[player_.difficulty].name}}) + "\n");
        console::print(format(data_.text("infoGender"), {{"player", data_.playerGenders[player_.gender].name},
                                                         {"lover", data_.loverGenders[player_.loverGender].name}}) +
                       "\n\n");
        if (console::askYesNo(data_.text("confirmInfo"), data_.text("confirmInvalid"))) break;
        console::clear();
    }
}

void Game::showStatus() {
    console::clear();
    console::print(format(data_.text("statusBar"), {{"name", player_.name},
                                                     {"job", job().name},
                                                     {"difficulty", data_.difficulties[player_.difficulty].name},
                                                     {"balance", std::to_string(balance_)}}) + "\n");
    if (turn_ >= 1 && turn_ < data_.settings.endingTurn) {
        static const char* slotKeys[] = {"morning", "afternoon", "night"};
        int day = (turn_ - 1) / 3 + 1;
        auto it = data_.slotNames.find(slotKeys[(turn_ - 1) % 3]);
        std::string slotName = it != data_.slotNames.end() ? it->second : slotKeys[(turn_ - 1) % 3];
        console::print(format(data_.text("timeOfDay"), {{"day", std::to_string(day)}, {"slot", slotName}}) + "\n");
    }
    if (turn_ >= 1) {
        if (showAffection()) {
            std::string list;
            for (size_t i = 0; i < data_.lovers.size(); ++i)
                list += (i ? "  " : "") + data_.lovers[i].color + " " + std::to_string(affection_[i]);
            console::print(format(data_.text("affectionLine"), {{"list", list}}) + "\n");
        }
        console::print(inventoryText() + "\n");
    }
    console::print("\n");
}

bool Game::showAffection() const {
    return data_.difficulties[player_.difficulty].showAffection;
}

bool Game::hasItem(const std::string& id) const {
    auto it = inventory_.find(id);
    return it != inventory_.end() && it->second > 0;
}

std::string Game::inventoryText() const {
    std::string list;
    for (const auto& kv : inventory_) {
        if (kv.second <= 0) continue;
        if (!list.empty()) list += ", ";
        list += data_.items.at(kv.first).name;
        if (kv.second > 1) list += " x" + std::to_string(kv.second);
    }
    return list.empty() ? data_.text("inventoryEmpty") : format(data_.text("inventoryLine"), {{"list", list}});
}

void Game::showStories() {
    auto it = job().stories.find(turn_);
    if (it == job().stories.end()) return;
    bool first = true;
    for (const std::string& page : it->second) {
        // The prologue's first page appears under the status bar; every other page gets a fresh screen.
        if (!(first && turn_ == 0)) showStatus();
        first = false;
        console::typewrite(localize(page));
        console::print("\n");
        console::waitEnter(data_.text("pressEnter"));
    }
}

void Game::playEvent() {
    Slot slot = static_cast<Slot>((turn_ - 1) % 3);
    Phase phase = turn_ >= data_.settings.lateGameStartTurn ? Phase::Late : Phase::Early;
    const Event* ev = pickEvent(slot, phase);
    if (!ev) return;  // no event defined for this slot: quiet turn

    console::typewrite(localize(ev->text));
    console::print("\n");
    if (ev->hasChoice) {
        if (ev->hasNeeds) {
            const Item& item = data_.items.at(ev->needsItem);
            console::print(format(data_.text("itemNeeded"),
                                  {{"item", item.name},
                                   {"status", data_.text(hasItem(ev->needsItem) ? "itemHave" : "itemMissing")}}) +
                           "\n");
        }
        bool yes = console::askYesNo(data_.text("yesNoPrompt"), data_.text("yesNoInvalid"));
        console::print("\n");
        if (ev->hasNeeds && yes == ev->needsYes && !hasItem(ev->needsItem))
            applyOutcome(ev->missing);
        else
            applyOutcome(yes ? ev->yes : ev->no);
    } else {
        applyOutcome(ev->outcome);
    }
}

const Event* Game::pickEvent(Slot slot, Phase phase) {
    std::vector<const Event*> pool;
    for (const Event& e : job().events)
        if (e.slot == slot && e.phase == phase) pool.push_back(&e);
    if (pool.empty()) return nullptr;

    // Order the pool so harder difficulties drop the most generous money events first:
    // affection events always come first (kept on every difficulty), then pure-money events
    // from harshest to most generous by average money. Ties keep the file order.
    auto hasAffection = [](const Event* e) { return e->isRelationship(); };
    auto averageMoney = [](const Event* e) {
        return e->hasChoice ? (e->yes.money + e->no.money) / 2.0 : static_cast<double>(e->outcome.money);
    };
    std::stable_sort(pool.begin(), pool.end(), [&](const Event* a, const Event* b) {
        bool affA = hasAffection(a), affB = hasAffection(b);
        if (affA != affB) return affA;
        if (affA) return false;
        return averageMoney(a) < averageMoney(b);
    });

    // Harder difficulties only use the first part of each pool (same rule as the original game).
    double ratio = data_.difficulties[player_.difficulty].eventPoolRatio;
    size_t limit = static_cast<size_t>(std::ceil(static_cast<double>(pool.size()) * ratio - 1e-9));
    pool.resize(std::max<size_t>(1, std::min(limit, pool.size())));

    std::vector<const Event*> fresh;
    for (const Event* e : pool)
        if (!usedEvents_.count(e->id)) fresh.push_back(e);
    if (fresh.empty()) {
        // Every event in this pool was already seen: allow repeats instead of looping forever.
        for (const Event* e : pool) usedEvents_.erase(e->id);
        fresh = pool;
    }

    std::uniform_int_distribution<size_t> dist(0, fresh.size() - 1);
    const Event* chosen = fresh[dist(rng_)];
    usedEvents_.insert(chosen->id);
    return chosen;
}

void Game::applyOutcome(const Outcome& outcome) {
    if (!outcome.text.empty()) {
        console::typewrite(localize(outcome.text));
        if (outcome.text.back() != '\n') console::print("\n");
    }

    for (const AffectionChange& c : outcome.affection) {
        for (size_t i = 0; i < affection_.size(); ++i) {
            if (c.target != kAllLovers && static_cast<size_t>(c.target) != i) continue;
            affection_[i] += c.amount;
            if (showAffection())
                console::print(format(data_.text("affectionChange"),
                                      {{"name", data_.lovers[i].color},
                                       {"amount", (c.amount > 0 ? "+" : "") + std::to_string(c.amount)}}) +
                               "\n");
        }
    }
    for (const std::string& id : outcome.loseItems) {
        if (!hasItem(id)) continue;
        --inventory_[id];
        console::print(format(data_.text("itemUsed"), {{"item", data_.items.at(id).name}}) + "\n");
    }
    for (const std::string& id : outcome.gainItems) {
        ++inventory_[id];
        console::print(format(data_.text("itemGained"), {{"item", data_.items.at(id).name}}) + "\n");
    }

    // Difficulty scales money, rounded to 100 won.
    const Difficulty& diff = data_.difficulties[player_.difficulty];
    double scaled = outcome.money * (outcome.money < 0 ? diff.lossMultiplier : diff.gainMultiplier);
    int money = static_cast<int>(std::lround(scaled / 100.0)) * 100;

    if (money > 0)
        console::print(format(data_.text("gain"), {{"amount", std::to_string(money)}}) + "\n");
    else if (money < 0)
        console::print(format(data_.text("loss"), {{"amount", std::to_string(money)}}) + "\n");
    else
        console::print(format(data_.text("balance"), {{"balance", std::to_string(balance_)}}) + "\n");
    balance_ += money;

    console::waitEnter(data_.text("pressEnter"));
}

void Game::runEnding() {
    console::typewrite(localize(job().endingIntro + data_.text("loverMenu")));
    console::print("\n");

    const std::string count = std::to_string(data_.lovers.size());
    int n = console::askNumber(format(data_.text("loverPrompt"), {{"count", count}}), 1,
                               static_cast<int>(data_.lovers.size()),
                               format(data_.text("loverInvalid"), {{"count", count}}),
                               format(data_.text("loverInvalid"), {{"count", count}}));
    size_t chosen = static_cast<size_t>(n - 1);
    const Lover& lover = data_.lovers[chosen];

    console::typewrite(localize(job().confess.empty() ? lover.confess : job().confess[chosen]));
    console::print("\n");
    if (!lover.favoriteItem.empty() && hasItem(lover.favoriteItem)) {
        // Carrying the lover's favorite item makes the confession more convincing.
        --inventory_[lover.favoriteItem];
        console::typewrite(format(data_.text("giftLine"),
                                  {{"item", data_.items.at(lover.favoriteItem).object}, {"color", lover.color}}));
        affection_[chosen] += data_.settings.giftBonus;
        if (showAffection())
            console::print(format(data_.text("affectionChange"),
                                  {{"name", lover.color}, {"amount", "+" + std::to_string(data_.settings.giftBonus)}}) +
                           "\n");
    }
    console::waitEnter(data_.text("pressEnter"));

    int minBalance = job().trueEndingMinBalance >= 0 ? job().trueEndingMinBalance
                                                      : data_.settings.trueEndingMinBalance;
    int minAffection = (job().trueEndingMinAffection >= 0 ? job().trueEndingMinAffection
                                                          : data_.settings.trueEndingMinAffection) +
                       data_.difficulties[player_.difficulty].minAffectionDelta;
    const char* key = "noMoney";
    if (balance_ >= minBalance) key = affection_[chosen] >= minAffection ? "true" : "noLove";
    console::print("\n");
    console::typewrite(format(localize(endingText(key)), {{"color", lover.color}}));
}

const std::string& Game::endingText(const std::string& key) const {
    auto it = job().endings.find(key);
    return it != job().endings.end() ? it->second : data_.endings.at(key);
}

std::string Game::localize(const std::string& text) const {
    std::string gendered = applyGender(data_, text, player_.gender, player_.loverGender);

    // Dialogue lines "@speaker line" become "<name> > line" (format: ui.speakerLine).
    std::string out;
    size_t pos = 0;
    while (pos <= gendered.size()) {
        size_t end = gendered.find('\n', pos);
        if (end == std::string::npos) end = gendered.size();
        std::string line = gendered.substr(pos, end - pos);
        if (line.size() > 1 && line[0] == '@') {
            size_t space = line.find(' ');
            if (space != std::string::npos) {
                std::string id = line.substr(1, space - 1);
                std::string name = id;
                auto it = data_.speakers.find(id);
                if (it != data_.speakers.end()) {
                    if (it->second.lover >= 0 && static_cast<size_t>(it->second.lover) < data_.lovers.size())
                        name = format(data_.text("loverName"),
                                      {{"color", data_.lovers[static_cast<size_t>(it->second.lover)].color}});
                    else if (!it->second.name.empty())
                        name = format(it->second.name, {{"player", player_.name}});
                }
                line = format(data_.text("speakerLine"), {{"name", name}, {"line", line.substr(space + 1)}});
            }
        }
        out += line;
        if (end < gendered.size()) out += '\n';
        pos = end + 1;
    }
    return out;
}
