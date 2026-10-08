// People (Docs/Design/50-player-card-friends-safety.md, Phase 1): each account as a person (a public handle, experience,
// played time, settings) and each character's roleplay profile on the player card, shown to each viewer as far as it
// may see, and read by residents as far as they could perceive. The pure rules are in RatwPeople.*.
#include "RatwGame.h"
#include "RatwPeople.h"
#include "RatwWire.h"

#include <cmath>

namespace ratw::game
{
using json::Value;

std::string Game::accountKey(const std::string& characterId) const
{
    // An account's name, or "dev:<id>" for a development identity's wolf (as portraits already treat it).
    const auto account = accounts_.ownerOf(characterId);
    return !account.empty() ? account : characterId.empty() ? std::string() : "dev:" + characterId;
}

std::string Game::accountKey(const Connection* c) const
{
    return !c->accountUsername.empty() ? c->accountUsername : !c->entityId.empty() ? "dev:" + c->entityId : std::string();
}

people::AccountRecord& Game::personOf(const std::string& account)
{
    auto& a = people_[account];
    if (a.experience.empty())
        a.experience = people::rules().defaultExperience;
    return a;
}

json::Value Game::accountView(const std::string& account) const
{
    // The owner's own view of their account as a person: never shown to anyone else whole.
    auto o = Value::object();
    const auto it = people_.find(account);
    const people::AccountRecord a = it == people_.end() ? people::AccountRecord{} : it->second;
    o.add("handle", a.handle);
    o.add("experience", a.experience.empty() ? people::rules().defaultExperience : a.experience);
    o.add("playedHours", std::floor(a.playedSeconds / 360) / 10);
    o.add("playedMinutes", std::floor(a.playedSeconds / 60));
    auto settings = Value::object();
    settings.add("showMature", a.settings.showMature);
    settings.add("recaps", a.settings.recaps);
    settings.add("toasts", a.settings.toasts);
    settings.add("messages", a.settings.messages);
    settings.add("matchmaking", a.settings.matchmaking);
    o.add("settings", settings);
    o.add("mentor", mentorView(account));           // (Doc 52, 3.)
    return o;
}

void Game::sendProfile(Connection* c)
{
    if (!c || c->entityId.empty())
        return;
    auto e = Value::object();
    e.add("type", "profile");
    const auto it = profiles_.find(c->entityId);
    e.add("own", people::save(it == profiles_.end() ? people::Profile{} : it->second));
    e.add("account", accountView(accountKey(c)));
    e.add("rules", people::rules().catalog);
    send(c, e);
}

Result Game::setHandle(const std::string& account, const std::string& username, const std::string& wanted)
{
    // A handle (doc 50, 1): unique, never the sign-in name, changed at most once a month (friends see the old one a week).
    const auto handle = people::clean(wanted, std::size_t(people::rules().handleMax));
    std::string why;
    if (!people::validHandle(handle, username, why))
        return {false, why, {}};
    for (const auto& [other, a] : people_)
        if (other != account && (people::handleKey(a.handle) == people::handleKey(handle) ||
                                 (people::handleKey(a.formerHandle) == people::handleKey(handle) && now() - a.handleChangedAt < 7 * 86400)))
            return {false, "That handle is taken.", {}};
    auto& a = personOf(account);
    if (a.handle == handle)
        return {true, "That is your handle already.", {}};
    if (!a.handle.empty() && a.handleChangedAt >= 0 && now() - a.handleChangedAt < people::rules().handleChangeDays * 86400.0)
        return {false, "A handle can be changed once every " + std::to_string(people::rules().handleChangeDays) + " days.", {}};
    if (!a.handle.empty())
    {
        a.formerHandle = a.handle;
        a.handleChangedAt = now();
    }
    else
        a.handleChangedAt = now();
    a.handle = handle;
    saveSoon();
    return {true, "Your handle is now " + handle + ".", {}};
}

bool Game::profileCommand(Connection* c, const json::Value& j, Result& result)
{
    // {"type": "profile", "verb": "set" | "status" | "walkup" | "handle" | "experience" | "settings" | "get", ...}.
    const auto& id = c->entityId;
    const auto verb = j.string("verb");
    const auto account = accountKey(c);
    if (id.empty() || account.empty())
        return false;
    auto& profile = profiles_[id];
    std::string why;
    if (verb == "set")
    {
        result = people::applyFields(profile, j.object("fields"), why) ? Result{true, "Your profile is saved.", {}} : Result{false, why, {}};
    }
    else if (verb == "status")
    {
        // (Storyteller: only those a Dungeon Master has approved, doc 58; until doc 58, no one.)
        const auto status = j.string("value");
        if (people::validStatus(status, false, why))
        {
            profile.status = status;
            result = {true, "Status: " + status + ".", {}};
        }
        else
            result = {false, why, {}};
    }
    else if (verb == "walkup")
    {
        profile.walkup = j.boolean("on");
        result = {true, profile.walkup ? "Others may approach you unannounced." : "Walk-up friendly is off.", {}};
    }
    else if (verb == "handle")
        result = setHandle(account, c->accountUsername, j.string("handle"));
    else if (verb == "experience")
    {
        const auto x = j.string("value");
        if (people::validExperience(x))
        {
            personOf(account).experience = x;
            result = {true, "Experience: " + x + ".", {}};
        }
        else
            result = {false, "Not a kind of experience.", {}};
    }
    else if (verb == "settings")
    {
        auto& s = personOf(account).settings;
        const auto& given = j.object("settings");
        s.showMature = given.boolean("showMature", s.showMature);
        s.recaps = given.boolean("recaps", s.recaps);
        s.toasts = given.boolean("toasts", s.toasts);
        s.messages = given.boolean("messages", s.messages);
        s.matchmaking = given.boolean("matchmaking", s.matchmaking);
        result = {true, "Settings saved.", {}};
    }
    else if (verb == "get")
        result = {true, {}, {}};
    else
        return false;
    if (result.ok)
    {
        saveSoon();
        sendProfile(c);
    }
    return true;
}

people::Viewer Game::viewerFacts(const std::string& viewer, const std::string& target) const
{
    // What the viewer can perceive of the target now: sight, scent close by and not masked, sound in earshot; whether
    // it knows a name of theirs; and its own mature setting.
    people::Viewer v;
    const auto* a = world_.entity(viewer);
    const auto* b = world_.entity(target);
    v.self = viewer == target;
    v.player = a && !a->npc;
    v.knowsName = knowsName(viewer, target);
    if (a && b && !v.self)
    {
        const double apart = a->cellId == b->cellId ? std::hypot(a->position.x - b->position.x, a->position.y - b->position.y) : 1e9;
        const auto reach = [](const char* sense) {
            const auto it = people::rules().senseReach.find(sense);
            return it == people::rules().senseReach.end() ? 0.0 : it->second;
        };
        v.sees = world_.visionClarity(viewer, target) > 0;
        v.smells = apart <= reach("scent") && !world_.scentMasked(*b) && a->smell > 0;
        v.hears = apart <= reach("sound");
    }
    if (v.player)
        if (const auto it = people_.find(accountKey(viewer)); it != people_.end())
            v.showMature = it->second.settings.showMature;
    if (v.player && !people_.count(accountKey(viewer)))
        v.showMature = false;
    return v;
}

json::Value Game::cardFor(const std::string& viewer, const std::string& target) const
{
    // The profile part of a closer look (doc 50, 2), names veiled as everywhere (doc 32, 1.5), with the owner's
    // experience in words.
    const auto it = profiles_.find(target);
    people::Profile p = it == profiles_.end() ? people::Profile{} : it->second;
    p.pronouns = pronounsOf(target);
    auto card = people::cardFor(p, viewerFacts(viewer, target));
    for (const char* key : {"description", "currently", "title", "motto"})
        if (card.has(key))
            card.set(key, veilFor(viewer, card.string(key)));
    if (const auto* e = world_.entity(target); e && !e->npc)
    {
        if (const auto person = people_.find(accountKey(target)); person != people_.end())
            card.add("experience", person->second.experience);
        if (isNewcomer(accountKey(target)))
            card.add("newcomer", true);             // "New to these parts" (doc 52, 2).
        if (const auto person = people_.find(accountKey(target)); person != people_.end() && person->second.mentor.on)
        {
            card.add("mentor", person->second.mentor.available ? "available" : "busy");   // (Doc 52, 3.)
            card.add("guided", person->second.mentor.guided);
        }
    }
    return card;
}

std::string Game::profileContext(const std::string& npc, const std::string& player) const
{
    // What a resident is told of a player it is answering (doc 50, 2): only what it could perceive.
    const auto it = profiles_.find(player);
    if (it == profiles_.end())
        return {};
    auto p = it->second;
    p.pronouns = pronounsOf(player);
    return veilFor(npc, people::residentContext(p, viewerFacts(npc, player)));
}

std::string Game::pronounsOf(const std::string& id) const
{
    // A character is male or female (the user), and its pronouns follow; "" for one not in the world.
    const auto* e = world_.entity(id);
    return !e ? std::string() : e->appearance->sex == "female" ? "she/her" : "he/him";
}

void Game::tendPeople(double dt)
{
    // Played time (doc 50, 1): once a minute, each connected player at the keys in the last five minutes adds the
    // minute to their account.
    const auto& r = people::rules();
    if (reports_ && now() - reportsPurgedAt_ > 86400)
    {
        // Once a day: reports past what is kept go (doc 50, 7); the cache is read again.
        reportsPurgedAt_ = now();
        reports_->purge(now());
        reportCache_.clear();
        for (const auto& kept : reports_->all())
            reportCache_[kept.id] = kept;
    }
    tendFriends();                                  // Old friend requests and kept private messages go.
    peopleAccumulator_ += dt;
    if (peopleAccumulator_ < r.playedEvery)
        return;
    const double minute = peopleAccumulator_;
    peopleAccumulator_ = 0;
    for (auto* c : clients_)
    {
        if (!c || c->entityId.empty())
            continue;
        const auto acted = operatorActivity_.find(c->entityId);
        const auto* e = world_.entity(c->entityId);
        const bool active = (acted != operatorActivity_.end() && now() - acted->second < r.activeWithin) ||
                            (e && e->lastPoseAt >= 0 && world_.time() - e->lastPoseAt < r.activeWithin);
        if (active)
            personOf(accountKey(c)).playedSeconds += minute;
    }
}

json::Value Game::peopleSave() const
{
    // Two lists (game.account_profiles and game.profiles through game.sections): readable by the tools and the DM,
    // holding no verifiers.
    auto accounts = Value::array();
    for (const auto& [account, a] : people_)
    {
        auto e = people::saveAccount(a);
        e.add("account", account);
        auto ids = Value::array();                  // (Its characters, so the tools can find a character's account.)
        for (const auto& cid : account.rfind("dev:", 0) == 0 ? std::vector<std::string>{account.substr(4)} : accounts_.characters(account))
            ids.push(cid);
        e.add("characters", ids);
        accounts.push(e);
    }
    auto profiles = Value::array();
    for (const auto& [character, p] : profiles_)
    {
        auto e = people::save(p);
        e.add("character", character);
        profiles.push(e);
    }
    // Mutes and blocks (game.safety_marks): by holder, kind and target.
    auto safety = Value::array();
    for (const auto& [holder, marks] : safety_)
        for (const auto& m : marks)
        {
            auto e = Value::object();
            e.add("holder", holder);
            e.add("kind", m.kind);
            e.add("target", m.target);
            e.add("character", m.character);
            e.add("label", m.label);
            e.add("at", m.at);
            safety.push(e);
        }
    auto root = Value::object();
    root.add("accounts", accounts);
    root.add("profiles", profiles);
    root.add("safety", safety);
    friendsSave(root);                              // Friends, requests and kept private messages (doc 50, 4).
    knownSave(root);                                // Known wolves and recaps (doc 50, 5).
    circlesSave(root);                              // Circles (doc 50, 6).
    const auto book = starBook_.save();             // The star book (doc 51): tallies by account, the last 30 days' stars.
    root.add("starTallies", book.array("tallies"));
    root.add("stars", book.array("recent"));
    booksSave(root);                                // Story books (doc 51, Phase 7).
    tiesSave(root);                                 // Ties (doc 52, 4).
    vouchesSave(root);                              // Vouches and first evenings (doc 52, 6 and 7).
    lettersSave(root);                              // The document store: letters (doc 55).
    loansSave(root);                                // Loans (doc 55, 8).
    lodgingsSave(root);                             // Lodgings and holds (doc 54, 4).
    stallsSave(root);                               // Market stalls (doc 54, 3).
    fameSave(root);                                 // Deeds (doc 56).
    return root;
}

void Game::peopleLoad(const json::Value& saved)
{
    people_.clear();
    profiles_.clear();
    for (const auto& e : saved.array("accounts"))
        if (const auto account = e.string("account"); !account.empty() && (accounts_.exists(account) || account.rfind("dev:", 0) == 0))
            people_[account] = people::loadAccount(e);
    for (const auto& e : saved.array("profiles"))
        if (const auto character = e.string("character"); !character.empty() && characters_.count(character))
            profiles_[character] = people::load(e);
    safety_.clear();
    for (const auto& e : saved.array("safety"))
        if (const auto kind = e.string("kind"); (kind == "mute" || kind == "block") && !e.string("holder").empty() && !e.string("target").empty())
            safety_[e.string("holder")].push_back({kind, e.string("target").substr(0, 80), e.string("character").substr(0, 80),
                                                   e.string("label").substr(0, 80), e.number("at")});
    friendsLoad(saved);
    knownLoad(saved);
    circlesLoad(saved);
    auto book = Value::object();
    book.add("tallies", saved.array("starTallies"));
    book.add("recent", saved.array("stars"));
    starBook_.load(book);
    booksLoad(saved);
    tiesLoad(saved);
    vouchesLoad(saved);
    lettersLoad(saved);
    loansLoad(saved);
    lodgingsLoad(saved);
    stallsLoad(saved);
    fameLoad(saved);
}
} // namespace ratw::game
