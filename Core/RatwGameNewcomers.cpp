// Newcomers (Docs/Design/52-newcomers.md): an account is new until 15 hours played or social level 3, and then never
// again; a new character arrives in one of the three start towns, the busiest preselected (Upper Accord when none is
// busier), any of them chosen (Phase 1). Active wolves are counted in each start town once a minute, in memory only,
// and a town's figure is the mean of its last 30 counts. Mentors (Phase 2): an account at social level 5 with no
// upheld report in 30 days opts in, is available or busy, and is recognised; an upheld report or a Dungeon Master
// turns it off. The numbers are in Data/Social/newcomers.json.
#include "RatwGame.h"

#include "RatwNames.h"
#include "RatwPractice.h"
#include "RatwScenes.h"
#include "RatwVoice.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <set>

namespace ratw::game
{
using json::Value;

bool Game::isNewcomer(const std::string& account) const
{
    if (account.empty())
        return false;
    const auto it = people_.find(account);
    if (it == people_.end())
        return true;                                // (A new account: nothing recorded of it yet.)
    return !it->second.graduated && !newcomers::graduates(it->second.playedSeconds, accountSocialLevel(account));
}

std::vector<std::string> Game::startTowns() const
{
    // The data file's towns this world has, in its order; none in a world of one settlement (its spawn, as ever).
    std::vector<std::string> out;
    for (const auto& s : newcomers::rules().starts)
        for (const auto& t : world_.towns())
            if (t.id == s.id)
            {
                out.push_back(s.id);
                break;
            }
    return out;
}

std::string Game::suggestedStart() const
{
    return newcomers::busiest(startTowns(), townCounts_);
}

json::Value Game::startsView() const
{
    // For the creator: each start town's name, its line, about how many wolves are about lately, and the one preselected.
    auto list = Value::array();
    const auto suggested = suggestedStart();
    for (const auto& id : startTowns())
        for (const auto& s : newcomers::rules().starts)
            if (s.id == id)
            {
                auto o = Value::object();
                o.add("id", id);
                o.add("name", s.name);
                o.add("line", s.line);
                o.add("wolves", std::round(townCounts_.mean(id)));
                if (id == suggested)
                    o.add("suggested", true);
                list.push(o);
            }
    return list;
}

void Game::tendNewcomers(double dt)
{
    const auto& r = newcomers::rules();
    newcomersAccumulator_ += dt;
    if (newcomersAccumulator_ < r.sampleEvery)
        return;
    newcomersAccumulator_ = 0;
    const auto towns = startTowns();
    std::map<std::string, int> counts;
    std::map<std::string, std::vector<std::string>> roster;
    for (auto* c : clients_)
    {
        if (!c || c->entityId.empty())
            continue;
        const auto* e = world_.entity(c->entityId);
        if (!e)
            continue;
        // Counted where it is, if it has been at the keys lately (as played time is: doc 50).
        const auto acted = operatorActivity_.find(c->entityId);
        const bool active = (acted != operatorActivity_.end() && now() - acted->second < r.activeWithin) ||
                            (e->lastPoseAt >= 0 && world_.time() - e->lastPoseAt < r.activeWithin);
        if (active)
            if (const auto* town = world_.townOf(e->cellId))
            {
                ++counts[town->id];
                roster[town->id].push_back(c->entityId);   // (For matchmakers: doc 52, 5.)
            }
        // No longer new, for good, once past either threshold.
        auto& person = personOf(accountKey(c));
        if (!person.graduated && newcomers::graduates(person.playedSeconds, socialLevel(c->entityId), r))
        {
            person.graduated = true;
            saveSoon();
        }
    }
    if (!towns.empty())
        townCounts_.add(counts, towns, r.window);
    townRoster_ = std::move(roster);
    // Once a day: mentors who may no longer mentor (silenced, say) are turned off. (An upheld report does it at once.)
    if (now() - mentorsCheckedAt_ >= 86400)
    {
        mentorsCheckedAt_ = now();
        std::vector<std::pair<std::string, std::string>> off;
        for (const auto& [account, a] : people_)
            if (std::string why; a.mentor.on && !mayMentor(account, why))
                off.push_back({account, why});
        for (const auto& [account, why] : off)
            mentorOff(account, why);
    }
}

// ------------------------------------------------------------------ Mentors (Phase 2)

int Game::accountSocialLevel(const std::string& account) const
{
    // The account's social level: its characters' social XP together (doc 49).
    long long xp = 0;
    for (const auto& id : account.rfind("dev:", 0) == 0 ? std::vector<std::string>{account.substr(4)} : accounts_.characters(account))
        if (const auto it = social_.points.find(id); it != social_.points.end())
            xp += it->second;
    return practice::levelFor(xp);
}

bool Game::mayMentor(const std::string& account, std::string& why) const
{
    const auto& r = newcomers::rules();
    newcomers::MentorCheck m;
    m.socialLevel = accountSocialLevel(account);
    m.upheldReports = upheldReportsWithin(account, r.mentorReportDays);
    m.newcomer = isNewcomer(account);
    if (const auto it = people_.find(account); it != people_.end())
    {
        m.silenced = it->second.silencedUntil > now();
        m.revoked = it->second.mentor.revoked;
    }
    return newcomers::mayMentor(m, why, r);
}

bool Game::availableMentor(const std::string& account) const
{
    // Opted in, available, on no tie and not resting after one: free to take a newcomer.
    const auto it = people_.find(account);
    if (it == people_.end())
        return false;
    const auto& m = it->second.mentor;
    return m.on && !m.revoked && m.available && m.tie.empty() && m.restingUntil <= now();
}

json::Value Game::mentorView(const std::string& account) const
{
    // The owner's own view (doc 52, 3): on or off, available or busy, may they (and why not), guided so far.
    auto o = Value::object();
    const auto it = people_.find(account);
    const people::AccountRecord::Mentor m = it == people_.end() ? people::AccountRecord::Mentor{} : it->second.mentor;
    std::string why;
    o.add("on", m.on);
    o.add("available", m.available);
    o.add("revoked", m.revoked);
    o.add("may", mayMentor(account, why));
    if (!why.empty())
        o.add("why", why);
    o.add("guided", m.guided);
    if (m.restingUntil > now())
        o.add("restingFor", std::ceil((m.restingUntil - now()) / 3600));
    return o;
}

bool Game::mentorCommand(Connection* c, const json::Value& j, Result& result)
{
    // {"type": "mentor", "verb": "optin" | "optout" | "available" | "busy" | "accept" | "pass" | "release", "tie": id}.
    if (!c || c->entityId.empty())
        return false;
    const auto account = accountKey(c);
    const auto verb = j.string("verb");
    auto& person = personOf(account);
    if (verb == "optin")
    {
        std::string why;
        if (person.mentor.on)
            result = {true, "You are mentoring newcomers already.", {}};
        else if (!mayMentor(account, why))
            result = {false, why, {}};
        else
        {
            person.mentor.on = true;
            person.mentor.available = true;
            if (people::validExperience("guide"))
                person.experience = "guide";        // (Newcomer Guide, doc 50's experience.)
            result = {true, "You mentor newcomers now: they see you're available. Set yourself busy when you'd rather not be asked.", {}};
        }
    }
    else if (verb == "optout")
    {
        person.mentor.on = false;
        result = {true, "You no longer mentor newcomers.", {}};
    }
    else if (verb == "accept" || verb == "pass")
    {
        // An offered tie (doc 52, 4): taken, or passed to the next mentor.
        const auto it = ties_.find(j.string("tie"));
        if (it == ties_.end() || it->second.state != "offered" || it->second.offeredTo != c->entityId)
            result = {false, "That tie isn't offered to you any more.", {}};
        else if (verb == "pass")
        {
            it->second.asked.push_back(account);
            it->second.offeredTo.clear();
            it->second.state = "seeking";
            result = {true, "You let that tie pass to someone else.", {}};
        }
        else if (!availableMentor(account))
            result = {false, "You can take a tie only while available, holding no other and not resting.", {}};
        else
        {
            makeTie(it->second, c->entityId, account, false);
            result = {true, "You take the tie.", {}};
        }
    }
    else if (verb == "release")
    {
        // A mentor lets a tie go: the newcomer is given a resident instead, and the mentor rests the day.
        const auto it = heldTieOf_.find(c->entityId);
        if (it == heldTieOf_.end() || ties_[it->second].resident)
            result = {false, "You hold no tie to release.", {}};
        else
        {
            auto& t = ties_[it->second];
            if (auto* n = clientOf(t.newcomer))
                system(n, "Your tie's mentor has had to step away; someone else will be found for you.");
            const auto newcomer = t.newcomer, account0 = t.account, starter = t.starter, town = t.town, arrival = t.arrivalCell;
            closeTie(t, "ended", true);
            startTie(newcomer, account0, starter, town, arrival);
            for (auto& [id, other] : ties_)
                if (other.newcomer == newcomer && other.state == "seeking")
                    other.mentorsDone = true;
            result = {true, "You release the tie; they'll be tied to a resident instead. Rest a day before the next.", {}};
        }
    }
    else if (verb == "available" || verb == "busy")
    {
        if (!person.mentor.on)
            result = {false, "You aren't mentoring: opt in first.", {}};
        else
        {
            person.mentor.available = verb == "available";
            result = {true, verb == "available" ? "Available to newcomers." : "Busy: newcomers won't be sent your way.", {}};
        }
    }
    else
        return false;
    saveSoon();
    sendProfile(c);
    return true;
}

void Game::tellAccount(const std::string& account, const std::string& words)
{
    // Every wolf of the account that is in the world hears it, and its profile panel is brought up to date.
    for (const auto& cid : account.rfind("dev:", 0) == 0 ? std::vector<std::string>{account.substr(4)} : accounts_.characters(account))
        if (auto* c = clientOf(cid))
        {
            system(c, words);
            sendProfile(c);
        }
}

void Game::mentorOff(const std::string& account, const std::string& why)
{
    auto& person = personOf(account);
    if (!person.mentor.on)
        return;
    person.mentor.on = false;
    tellAccount(account, "Your mentoring is turned off: " + why);
    saveSoon();
}

Result Game::revokeMentor(const std::string& account, bool revoke, const std::string& by)
{
    // A Dungeon Master turns an account's mentoring off until restored, or lets it mentor again (doc 52, 3).
    if (account.empty() || (account.rfind("dev:", 0) != 0 && !accounts_.exists(account)))
        return {false, "No such account.", {}};
    auto& person = personOf(account);
    person.mentor.revoked = revoke;
    if (revoke)
        person.mentor.on = false;
    tellAccount(account, revoke ? "A Dungeon Master has turned your mentoring off for now."
                                : "A Dungeon Master has restored your mentoring: you may opt in again.");
    logEvent(revoke ? "mentor revoked" : "mentor restored", account, {}, "by " + by);
    saveSoon();
    return {true, std::string(revoke ? "Mentoring revoked" : "Mentoring restored") + " for " + account + ".", account};
}
} // namespace ratw::game

namespace ratw::game
{
// ------------------------------------------------------------------ Ties (Phase 3)
//
// A new wolf's tie starts seeking when it is made: mentors first, the one longest without a tie asked first (an
// offer for 3 minutes; a pass or silence asks the next), then a resident in the start town who fits the starter, else
// the innkeeper to show them around. Both are told, with where to find the other (a marker there for 20 minutes); the
// tie goes on both known-wolves lists. It lapses after 7 days or 3 shared scenes, and the mentor rests a day.

std::string Game::lookOfCharacter(const std::string& viewer, const std::string& id) const
{
    // As the viewer would call them: by name or look in the world; by their saved look when they're away (a newcomer
    // still in the creator).
    if (world_.entity(id))
        return labelFor(viewer, id);
    if (const auto it = characters_.find(id); it != characters_.end())
        return knowsName(viewer, id) ? it->second.name : names::describe(it->second.appearance, it->second.age);
    return "someone";
}

void Game::reindexTies()
{
    tieOfCharacter_.clear();
    heldTieOf_.clear();
    for (const auto& [id, t] : ties_)
        if (t.state == "seeking" || t.state == "offered" || t.state == "active")
        {
            tieOfCharacter_[t.newcomer] = id;
            if (!t.other.empty() && t.state == "active")
                heldTieOf_[t.other] = id;
        }
}

int Game::tieScenes(const newcomers::Tie& t) const
{
    // Scenes the two have shared since the tie began (doc 50's known-wolves count).
    if (const auto list = knownWolves_.find(t.newcomer); list != knownWolves_.end())
        if (const auto k = list->second.find(t.other); k != list->second.end())
            return std::max(0, k->second.scenes - t.scenesAtStart);
    return 0;
}

void Game::startTie(const std::string& newcomer, const std::string& account, const std::string& starter, const std::string& town,
                    const std::string& arrivalCell)
{
    newcomers::Tie t;
    t.id = "tie-" + guid().substr(0, 12);
    t.newcomer = newcomer;
    t.account = account;
    t.starter = starter;
    t.town = town;
    t.arrivalCell = arrivalCell;
    t.created = now();
    ties_[t.id] = t;
    reindexTies();
    saveSoon();
}

bool Game::offerTie(newcomers::Tie& t)
{
    // The next mentor to ask: online, available, eligible, in or near the start town, holding no tie, not resting, not
    // Out of character, not the newcomer's own account, not blocked either way, not asked already.
    const auto& r = newcomers::tieRules();
    const auto* s = newcomers::starter(t.starter);
    if (!s || !s->mentor)
        return false;
    std::vector<newcomers::MentorCandidate> candidates;
    std::set<std::string> seen;
    for (const auto* c : clients_)
    {
        if (!c || c->entityId.empty())
            continue;
        const auto account = accountKey(c);
        if (account == t.account || !seen.insert(account).second ||
            std::find(t.asked.begin(), t.asked.end(), account) != t.asked.end())
            continue;
        const auto person = people_.find(account);
        std::string why;
        if (person == people_.end() || !availableMentor(account) || !mayMentor(account, why))
            continue;
        if (const auto p = profiles_.find(c->entityId); p != profiles_.end() && p->second.status == "ooc")
            continue;
        if (blocked(c->entityId, t.newcomer))
            continue;
        const auto* e = world_.entity(c->entityId);
        if (!e)
            continue;
        const auto* town = world_.townOf(e->cellId);
        bool near = !t.town.empty() && town && town->id == t.town;
        if (!near && !t.arrivalCell.empty())
        {
            const auto route = world_.routeBetween(e->cellId, t.arrivalCell);
            near = !route.empty() && route.back() == t.arrivalCell && int(route.size()) <= r.nearCells + 1;
        }
        if (near)
            candidates.push_back({account, c->entityId, person->second.mentor.lastTieAt});
    }
    if (candidates.empty())
        return false;
    std::uint32_t seed = 0;
    for (const unsigned char ch : t.id)
        seed = seed * 31 + ch;
    const auto first = newcomers::mentorOrder(candidates, seed).front();
    const double seconds = options_.tieOfferSeconds >= 0 ? options_.tieOfferSeconds : r.offerSeconds;
    t.state = "offered";
    t.offeredTo = first.character;
    t.offerUntil = now() + seconds;
    if (auto* c = clientOf(first.character))
    {
        auto e = Value::object();
        e.add("type", "tieOffer");
        e.add("tie", t.id);
        e.add("look", lookOfCharacter(first.character, t.newcomer));
        e.add("starter", s->other);
        std::string town = t.town;
        for (const auto& st : newcomers::rules().starts)
            if (st.id == t.town)
                town = st.name;
        e.add("town", town);
        e.add("seconds", seconds);
        send(c, e);
    }
    saveSoon();
    return true;
}

void Game::tieToResident(newcomers::Tie& t)
{
    // A resident in the start town who fits the starter, awake and not following anyone (one asleep if none is
    // awake), chosen at random; else the starter becomes "show you around", with the innkeeper nearest the arrival;
    // else the nearest grown resident in town.
    const auto* chosen = newcomers::starter(t.starter);
    const auto* fallback = newcomers::fallbackStarter();
    struct Fit
    {
        const Entity* e;
        bool awake;
        double apart;
    };
    std::vector<Fit> fits, innkeepers, anyone;
    const auto* arrival = world_.cell(t.arrivalCell);
    const auto apartFrom = [&](const Entity& e) {
        const auto* cell = world_.cell(e.cellId);
        if (!arrival || !cell)
            return 1e9;
        return std::hypot(cell->worldX + e.position.x - arrival->worldX, cell->worldY + e.position.y - arrival->worldY);
    };
    std::set<std::string> masters;                  // Holders of a post with no apprentice.
    for (const auto& [pid, st] : world_.society().state().careers.positions)
        if (!st.holder.empty() && st.apprentice.empty())
            masters.insert(st.holder);
    for (const auto& [id, e] : world_.entities())
    {
        if (!e.npc || e.dead || e.transient || !e.leaderId.empty() || pendingNpc_.count(id))
            continue;
        const auto* town = world_.townOf(e.cellId);
        if (t.town.empty() ? (!arrival || world_.townOf(e.cellId) != world_.townOf(arrival->id)) : (!town || town->id != t.town))
            continue;
        const auto job = jobCategoryOf(id);
        const auto life = world_.society().state().residents.find(id);
        const bool awake = life == world_.society().state().residents.end() || life->second.task != "sleep";
        const Fit f{&e, awake, apartFrom(e)};
        if (chosen && newcomers::residentFits(*chosen, job, e.age, masters.count(id) > 0))
            fits.push_back(f);
        if (job == "innkeeper")
            innkeepers.push_back(f);
        if (e.age >= 18 && job != "child")
            anyone.push_back(f);
    }
    const auto preferAwake = [](std::vector<Fit>& list) {
        if (std::any_of(list.begin(), list.end(), [](const Fit& f) { return f.awake; }))
            list.erase(std::remove_if(list.begin(), list.end(), [](const Fit& f) { return !f.awake; }), list.end());
    };
    preferAwake(fits);
    if (!fits.empty())
    {
        std::uint32_t seed = 0;
        for (const unsigned char ch : t.id)
            seed = seed * 31 + ch;
        std::sort(fits.begin(), fits.end(), [](const Fit& a, const Fit& b) { return a.e->id < b.e->id; });
        makeTie(t, fits[seed % fits.size()].e->id, {}, true);
        return;
    }
    if (fallback)
        t.starter = fallback->id;
    preferAwake(innkeepers);
    auto& from = !innkeepers.empty() ? innkeepers : anyone;
    if (from.empty())
    {
        closeTie(t, "ended", false);                // (No one at all in town to tie them to.)
        return;
    }
    const auto nearest = std::min_element(from.begin(), from.end(), [](const Fit& a, const Fit& b) { return a.apart < b.apart; });
    makeTie(t, nearest->e->id, {}, true);
}

void Game::tieKnown(const std::string& owner, const std::string& other, const std::string& line)
{
    // The tie on the owner's known-wolves list, the starter as its note (doc 50, 5), even before the owner enters.
    auto& list = knownWolves_[owner];
    auto it = list.find(other);
    if (it == list.end())
    {
        it = list.emplace(other, people::KnownWolf{}).first;
        it->second.firstMet = now();
        it->second.resident = world_.entity(other) && world_.entity(other)->npc;
        it->second.label = lookOfCharacter(owner, other);
    }
    it->second.tie = line;
}

void Game::makeTie(newcomers::Tie& t, const std::string& other, const std::string& otherAccount, bool resident)
{
    const auto& r = newcomers::tieRules();
    const auto* s = newcomers::starter(t.starter);
    t.state = "active";
    t.other = other;
    t.otherAccount = otherAccount;
    t.resident = resident;
    t.offeredTo.clear();
    t.made = now();
    t.lapsesAt = now() + (options_.tieLapseSeconds >= 0 ? options_.tieLapseSeconds : r.lapseDays * 86400);
    t.scenesAtStart = 0;
    if (const auto list = knownWolves_.find(t.newcomer); list != knownWolves_.end())
        if (const auto k = list->second.find(other); k != list->second.end())
            t.scenesAtStart = k->second.scenes;
    if (const auto* e = world_.entity(other))
    {
        t.spotCell = e->cellId;
        t.spotX = e->position.x;
        t.spotY = e->position.y;
        const auto* cell = world_.cell(e->cellId);
        t.spotPlace = cell ? cell->name : std::string();
        t.markerUntil = now() + r.markerSeconds;
    }
    if (!resident)
    {
        auto& person = personOf(otherAccount);
        person.mentor.tie = t.id;
        person.mentor.lastTieAt = now();
    }
    else if (s)
    {
        // What the starter does with a resident: a bond to start from, a small debt, names for family.
        const double day = world_.calendarDays();
        world_.bonds().change(other, t.newcomer, {s->affinity, s->trust, s->familiarity, 0, s->respect}, day);
        if (s->owed > 0)
            world_.bonds().addOwed(other, t.newcomer, s->owed, day);
        if (s->names)
        {
            learnName(t.newcomer, other, nameOf(other), "tie");
            if (const auto it = characters_.find(t.newcomer); it != characters_.end())
                learnName(other, t.newcomer, it->second.name, "tie");
        }
    }
    if (s)
    {
        tieKnown(t.newcomer, other, s->newcomer);
        if (!resident)
            tieKnown(other, t.newcomer, s->other);
    }
    reindexTies();
    tellTie(t);
    saveSoon();
}

void Game::tellTie(newcomers::Tie& t)
{
    // Both told the starter and where to find each other; anyone away is told on entering.
    const auto* s = newcomers::starter(t.starter);
    if (!s || t.state != "active")
        return;
    if (auto* c = clientOf(t.newcomer); c && !t.told)
    {
        const auto look = lookOfCharacter(t.newcomer, t.other);
        system(c, "Your tie: " + s->newcomer + " Look for " + look + (t.spotPlace.empty() ? std::string(".") : " near " + t.spotPlace + ".") +
                      (t.markerUntil > now() ? " The spot is marked on your map for a while." : ""));
        t.told = true;
    }
    if (!t.resident)
        if (auto* c = clientOf(t.other); c && !t.otherTold)
        {
            std::string town = t.town;
            for (const auto& st : newcomers::rules().starts)
                if (st.id == t.town)
                    town = st.name;
            system(c, "Your tie: " + s->other + " Look for " + lookOfCharacter(t.other, t.newcomer) +
                          (town.empty() ? std::string(".") : ", new in " + town + "."));
            t.otherTold = true;
        }
}

void Game::closeTie(newcomers::Tie& t, const std::string& state, bool rest)
{
    // A tie lapses (it ran its course) or ends (by the newcomer, a mentor's release, a Dungeon Master): the mentor's
    // claim ends; it rests a day after a lapse or a release; a tie with a scene shared counts as one guided.
    const auto& r = newcomers::tieRules();
    const bool shared = t.state == "active" && tieScenes(t) > 0;
    const auto was = t.state;
    t.state = state;
    t.ended = now();
    t.offeredTo.clear();
    if (!t.resident && !t.otherAccount.empty())
    {
        auto& person = personOf(t.otherAccount);
        if (person.mentor.tie == t.id)
            person.mentor.tie.clear();
        person.mentor.lastTieAt = now();
        if (rest)
            person.mentor.restingUntil = now() + r.restSeconds;
        if (shared)
            ++person.mentor.guided;
    }
    if (was == "active")
    {
        const auto words = state == "lapsed" ? std::string(" has run its course: what comes next is yours.") : std::string(" is ended.");
        if (auto* c = clientOf(t.newcomer))
            system(c, "Your tie with " + lookOfCharacter(t.newcomer, t.other) + words);
        if (!t.resident)
            if (auto* c = clientOf(t.other))
                system(c, "Your tie with " + lookOfCharacter(t.other, t.newcomer) + words);
    }
    reindexTies();
    saveSoon();
}

void Game::tendTies(double dt)
{
    tiesAccumulator_ += dt;
    if (tiesAccumulator_ < 1)
        return;
    tiesAccumulator_ = 0;
    tendVouches();                                  // (Vouches and first evenings ride on the same second: Phase 5.)
    tendEvenings();
    std::vector<std::string> open, due;
    for (const auto& [id, t] : ties_)
        if (t.state == "seeking" || t.state == "offered")
            open.push_back(id);
        else if (t.state == "active" && now() >= t.lapsesAt)
            due.push_back(id);                      // (Its days are up.)
    for (const auto& id : due)
        closeTie(ties_[id], "lapsed", true);
    for (const auto& id : open)
    {
        auto& t = ties_[id];
        if (t.state == "offered")
        {
            if (now() <= t.offerUntil)
                continue;
            t.asked.push_back(accountKey(t.offeredTo));   // (Silence: the next is asked.)
            if (auto* c = clientOf(t.offeredTo))
                system(c, "The tie offered to you has gone to someone else.");
            t.offeredTo.clear();
            t.state = "seeking";
        }
        if (!t.mentorsDone && offerTie(t))
            continue;
        t.mentorsDone = true;
        tieToResident(t);
    }
    // Once a minute: ties whose scenes have run their course; old closed ones let go after 30 days.
    if (now() - tiesLapsedAt_ < 60)
        return;
    tiesLapsedAt_ = now();
    std::vector<std::string> gone;
    for (auto& [id, t] : ties_)
    {
        if (t.state == "active" && newcomers::lapsed(t, tieScenes(t), now()))
            closeTie(t, "lapsed", true);
        else if ((t.state == "lapsed" || t.state == "ended") && now() - t.ended > 30 * 86400.0)
            gone.push_back(id);
    }
    for (const auto& id : gone)
        ties_.erase(id);
}

bool Game::tieCommand(Connection* c, const json::Value& j, Result& result)
{
    // {"type": "tie", "verb": "end"}: the newcomer ends their tie early (the mentor doesn't rest then).
    if (!c || c->entityId.empty() || j.string("verb") != "end")
        return false;
    const auto it = tieOfCharacter_.find(c->entityId);
    if (it == tieOfCharacter_.end())
    {
        result = {false, "You have no tie to end.", {}};
        return true;
    }
    closeTie(ties_[it->second], "ended", false);
    result = {true, "Your tie is ended. Whatever you've begun with them goes on as you like.", {}};
    return true;
}

Result Game::endTieOf(const std::string& character, const std::string& by)
{
    std::string id;
    if (const auto it = tieOfCharacter_.find(character); it != tieOfCharacter_.end())
        id = it->second;
    else if (const auto held = heldTieOf_.find(character); held != heldTieOf_.end())
        id = held->second;
    if (id.empty())
        return {false, "They have no tie.", {}};
    closeTie(ties_[id], "ended", false);
    logEvent("tie ended", character, {}, "by " + by);
    return {true, "The tie is ended.", character};
}

void Game::tiesOnEnter(const std::string& character)
{
    // Told on entering what they missed: a tie made while they were in the creator or away.
    for (const auto* index : {&tieOfCharacter_, &heldTieOf_})
        if (const auto it = index->find(character); it != index->end())
            tellTie(ties_[it->second]);
}

json::Value Game::tieView(const std::string& character) const
{
    // The snapshot's self.tie: the starter from this side, the other as known, how it stands, and the marker while it
    // lasts (for the newcomer: where the other was when it was made).
    // (A mentor's own newcomer's tie shows before one it holds for someone else.)
    auto it = tieOfCharacter_.find(character);
    if (it == tieOfCharacter_.end())
    {
        it = heldTieOf_.find(character);
        if (it == heldTieOf_.end())
            return {};
    }
    const auto found = ties_.find(it->second);
    if (found == ties_.end())
        return {};
    const auto& t = found->second;
    const auto* s = newcomers::starter(t.starter);
    auto o = Value::object();
    const bool newcomer = t.newcomer == character;
    o.add("id", t.id);
    o.add("state", t.state == "offered" ? std::string("seeking") : t.state);
    o.add("line", s ? (newcomer ? s->newcomer : s->other) : std::string());
    o.add("newcomer", newcomer);
    if (t.state == "active")
    {
        o.add("other", lookOfCharacter(character, newcomer ? t.other : t.newcomer));
        o.add("resident", t.resident);
        o.add("lapsesInDays", std::max(0.0, std::ceil((t.lapsesAt - now()) / 86400)));
        o.add("scenes", tieScenes(t));
        if (newcomer && t.markerUntil > now() && !t.spotCell.empty())
        {
            auto m = Value::object();
            m.add("cell", t.spotCell);
            m.add("x", t.spotX);
            m.add("y", t.spotY);
            m.add("place", t.spotPlace);
            m.add("seconds", std::round(t.markerUntil - now()));
            o.add("marker", m);
        }
    }
    return o;
}

std::string Game::tieBriefing(const std::string& npc, const std::string& player) const
{
    // A resident's briefing: the tie it shares with this wolf (doc 52, 4).
    const auto it = tieOfCharacter_.find(player);
    if (it == tieOfCharacter_.end())
        return {};
    const auto found = ties_.find(it->second);
    if (found == ties_.end() || found->second.state != "active" || found->second.other != npc || found->second.newcomer != player)
        return {};
    const auto* s = newcomers::starter(found->second.starter);
    if (!s)
        return {};
    std::string line = s->other;
    if (!line.empty())
        line[0] = char(std::tolower(static_cast<unsigned char>(line[0])));
    return " You and this wolf share a tie: " + line;
}

void Game::tiesSave(json::Value& root) const
{
    auto list = Value::array();
    for (const auto& [id, t] : ties_)
        list.push(newcomers::saveTie(t));
    root.add("ties", list);
}

void Game::tiesLoad(const json::Value& saved)
{
    ties_.clear();
    for (const auto& e : saved.array("ties"))
    {
        auto t = newcomers::loadTie(e);
        if (t.id.empty() || !characters_.count(t.newcomer) || !newcomers::starter(t.starter))
            continue;
        ties_[t.id] = std::move(t);
    }
    reindexTies();
}
} // namespace ratw::game

namespace ratw::game
{
// ------------------------------------------------------------------ Residents as matchmakers (Phase 4)
//
// While a player talks to an innkeeper, a priest or a town's market merchant, the server may choose one wolf the
// matchmaker knows, in the same town, for a reason (a newcomer and a helper, a tie not yet met, both looking for a
// scene, something asked for and who meets it, the same start town this week), and add it to the briefing; the Mind
// says it, by the look the player knows (never a name the player hasn't been given: the reply is checked); without a
// model, a written line says it. At most once a game hour a player, six an hour a matchmaker, three times an hour
// pointed at. Never at a wolf Out of character, opted out, blocked, in the player's party, or already known well.

std::string Game::jobCategoryOf(const std::string& npc) const
{
    const auto* e = world_.entity(npc);
    if (!e)
        return {};
    const auto* spec = world_.society().spec(npc);
    const auto* post = world_.society().jobOf(npc);
    const auto role = spec ? spec->role : std::string("civilian");
    return scenes::jobCategory((post ? post->title : std::string()) + " " + e->description, spec ? spec->workLabel : std::string(),
                               role == "merchant" || role == "guard" ? role : "civilian", e->age);
}

bool Game::isMatchmaker(const std::string& npc) const
{
    const auto& r = newcomers::matchRules();
    const auto* e = world_.entity(npc);
    if (!e || !e->npc || e->dead || r.matchmakers.empty())
        return false;
    const auto job = jobCategoryOf(npc);
    if (std::find(r.matchmakers.begin(), r.matchmakers.end(), job) != r.matchmakers.end())
        return true;
    // The town's market merchant: the one who keeps the market's stall (where caravans load).
    if (!r.marketMerchant)
        return false;
    const auto* post = world_.society().jobOf(npc);
    const auto* town = post ? world_.townOf(post->work.cell) : nullptr;
    return post && town && post->role == "merchant" && post->work.cell == town->market &&
           std::abs(post->work.x - town->marketX) < 0.5 && std::abs(post->work.y - town->marketY) < 0.5;
}

std::string Game::matchmake(const std::string& npc, const std::string& player, const std::string& heard)
{
    const auto& r = newcomers::matchRules();
    const auto* a = world_.entity(player);
    const auto* m = world_.entity(npc);
    if (!a || a->npc || !m || !isMatchmaker(npc))
        return {};
    const auto settingOn = [&](const std::string& id) {
        const auto person = people_.find(accountKey(id));
        return person == people_.end() || person->second.settings.matchmaking;
    };
    const auto status = [&](const std::string& id) {
        const auto p = profiles_.find(id);
        return p == profiles_.end() ? std::string("ic") : p->second.status;
    };
    if (!settingOn(player) || status(player) == "ooc")
        return {};
    const auto hour = std::int64_t(std::floor(world_.calendarDays() * 24));
    if (const auto last = matchedHour_.find(player); last != matchedHour_.end() && hour - last->second < r.perPlayerHours)
        return {};
    auto& mine = matchmakerHour_[npc];
    if (mine.first == hour && mine.second >= r.perMatchmakerHour)
        return {};
    const auto* town = world_.townOf(m->cellId);
    if (!town)
        return {};
    const auto side = [&](const std::string& id) {
        newcomers::MatchSide s;
        const auto account = accountKey(id);
        s.newcomer = isNewcomer(account);
        const auto person = people_.find(account);
        // An available mentor, or a self-declared Newcomer Guide who isn't a mentor (a busy mentor isn't sent anyone).
        s.helper = availableMentor(account) ||
                   (person != people_.end() && person->second.experience == "guide" && !person->second.mentor.on && !s.newcomer);
        s.looking = status(id) == "lfs";
        return s;
    };
    const auto knows = [&](const std::string& id) {
        const auto* b = world_.bonds().find(npc, id);
        return b && b->familiarity >= r.knowsFamiliarity;
    };
    const auto pointedEnough = [&](const std::string& id) {
        const auto it = pointedHour_.find(id);
        return it != pointedHour_.end() && it->second.first == hour && it->second.second >= r.pointedPerHour;
    };
    const auto knownWell = [&](const std::string& id) {
        if (const auto list = knownWolves_.find(player); list != knownWolves_.end())
            if (const auto k = list->second.find(id); k != list->second.end())
                return k->second.scenes >= r.wellKnownScenes;
        return false;
    };
    // Where each arrived (their tie's town and time), for the same start town this week.
    const auto arrival = [&](const std::string& id) -> std::pair<std::string, double> {
        for (const auto& [tid, t] : ties_)
            if (t.newcomer == id)
                return {t.town, t.created};
        return {};
    };
    const auto mineArrival = arrival(player);
    const auto* ask = newcomers::askIn(voice::Rules::normalise(heard, m->name), r);
    std::vector<newcomers::MatchCandidate> candidates;
    if (const auto roster = townRoster_.find(town->id); roster != townRoster_.end())
        for (const auto& id : roster->second)
        {
            const auto* b = world_.entity(id);
            if (id == player || !b || b->npc || world_.townOf(b->cellId) != town)
                continue;                           // (Still in town, at the minute's count and now.)
            newcomers::MatchCandidate c;
            c.id = id;
            c.side = side(id);
            if (const auto t = tieOfCharacter_.find(player); t != tieOfCharacter_.end())
                c.tiedUnmet = ties_[t->second].state == "active" && ties_[t->second].other == id && tieScenes(ties_[t->second]) == 0;
            if (const auto t = tieOfCharacter_.find(id); t != tieOfCharacter_.end() && !c.tiedUnmet)
                c.tiedUnmet = ties_[t->second].state == "active" && ties_[t->second].other == player && tieScenes(ties_[t->second]) == 0;
            c.meetsAsk = ask && ask->player == "looking" && c.side.looking;
            const auto theirs = arrival(id);
            c.sameRoots = !mineArrival.first.empty() && theirs.first == mineArrival.first &&
                          now() - mineArrival.second < r.sharedRootsDays * 86400 && now() - theirs.second < r.sharedRootsDays * 86400;
            c.excluded = !knows(id) || status(id) == "ooc" || !settingOn(id) || blocked(player, id) || parties_.together(player, id) ||
                         knownWell(id) || pointedEnough(id);
            candidates.push_back(c);
        }
    // A resident the matchmaker knows who meets what was asked for: a master with a place to fill, or a trade.
    if (ask && (!ask->resident.empty() || !ask->jobs.empty()))
        if (const auto* bonds = world_.bonds().of(npc))
        {
            std::set<std::string> masters;
            for (const auto& [pid, st] : world_.society().state().careers.positions)
                if (!st.holder.empty() && st.apprentice.empty())
                    masters.insert(st.holder);
            for (const auto& [id, bond] : *bonds)
            {
                const auto* c = world_.entity(id);
                if (!c || !c->npc || c->dead || id == npc || bond.familiarity < r.knowsFamiliarity || world_.townOf(c->cellId) != town)
                    continue;
                newcomers::MatchCandidate cand;
                cand.id = id;
                cand.player = false;
                const auto job = jobCategoryOf(id);
                cand.meetsAsk = ask->resident == "apprentice" ? masters.count(id) > 0
                                                               : std::find(ask->jobs.begin(), ask->jobs.end(), job) != ask->jobs.end();
                cand.excluded = knownWell(id) || pointedEnough(id);
                candidates.push_back(cand);
            }
        }
    std::uint32_t seed = 2166136261u;
    for (const unsigned char ch : npc + player + std::to_string(hour))
        seed = (seed ^ ch) * 16777619u;
    const auto pick = newcomers::pickPairing(side(player), candidates, seed, r);
    if (pick.id.empty())
        return {};
    // In words: as the player knows them, where they are, and why.
    const auto* other = world_.entity(pick.id);
    // As the player knows them: lookalikes told apart by number, as In Sight shows them ("a brown wolf (2)").
    const auto called = strangerNames(player);
    const auto named = called.find(pick.id);
    const auto who = named != called.end() ? named->second : labelFor(player, pick.id);
    std::string where = "near by";
    if (other && other->cellId == m->cellId)
        where = "just over there";
    else if (const auto* cell = other ? world_.cell(other->cellId) : nullptr)
        where = "over at " + cell->name;
    std::string townName = town->id;
    for (const auto& s : newcomers::rules().starts)
        if (s.id == town->id)
            townName = s.name;
    const auto reasonWords = newcomers::fill(r.reasons.count(pick.reason) ? r.reasons.at(pick.reason) : std::string(),
                                             {{"who", who}, {"meets", ask ? ask->meets : std::string()}, {"town", townName}});
    matchPending_[npc] = {player, pick.id, who, where, reasonWords};
    matchedHour_[player] = hour;
    if (mine.first != hour)
        mine = {hour, 0};
    ++mine.second;
    auto& pointed = pointedHour_[pick.id];
    if (pointed.first != hour)
        pointed = {hour, 0};
    ++pointed.second;
    logEvent("matchmaking", npc, player, pick.reason + " " + pick.id);
    return " " + newcomers::fill(r.briefing, {{"who", who}, {"where", where}, {"reason", reasonWords}});
}

std::string Game::sayMatch(const std::string& npc, const std::string& text, bool modelSaid)
{
    // A pointer said: the other wolf's names the player doesn't know become how they look, whatever the model wrote;
    // without a model, the written line after the reply; and the other, if it can see the matchmaker, a quiet nod.
    const auto it = matchPending_.find(npc);
    if (it == matchPending_.end())
        return text;
    const auto m = it->second;
    matchPending_.erase(it);
    std::string said = text;
    if (!knowsName(m.player, m.other))
        for (const auto& name : namesOf(m.other))
        {
            if (name.empty())
                continue;
            const auto letter = [](char ch) { return std::isalnum(static_cast<unsigned char>(ch)) || ch == '\'' || ch == '_'; };
            std::size_t from = 0;
            while (true)
            {
                const auto at = said.find(name, from);
                if (at == std::string::npos)
                    break;
                const auto end = at + name.size();
                if ((at > 0 && letter(said[at - 1])) || (end < said.size() && letter(said[end])))
                {
                    from = at + 1;                  // (Inside another word: left alone.)
                    continue;
                }
                said.replace(at, name.size(), m.who);
                from = at + m.who.size();
            }
        }
    if (!modelSaid)
    {
        // The other named when in the player's place, so each listener hears them as it knows them (the reply is
        // veiled per listener); elsewhere, as the player knows them.
        const auto* p = world_.entity(m.player);
        const auto* o = world_.entity(m.other);
        const auto who = p && o && p->cellId == o->cellId && !o->name.empty() ? o->name : m.who;
        auto line = newcomers::fill(newcomers::matchRules().line, {{"who", who}, {"where", m.where}, {"reason", m.reason}});
        if (who != m.who && !m.who.empty())
            for (std::size_t at = line.find(m.who); at != std::string::npos; at = line.find(m.who, at + who.size()))
                line.replace(at, m.who.size(), who);
        said += (said.empty() ? "" : " ") + line;
    }
    if (auto* c = clientOf(m.other); c && m.other != m.player && world_.visionClarity(m.other, npc) > 0)
    {
        auto quiet = newcomers::fill(newcomers::matchRules().quiet, {{"matchmaker", labelFor(m.other, npc)}, {"other", labelFor(m.other, m.player)}});
        if (!quiet.empty())
            quiet[0] = char(std::toupper(static_cast<unsigned char>(quiet[0])));
        system(c, quiet);
    }
    return said;
}
} // namespace ratw::game

namespace ratw::game
{
// ------------------------------------------------------------------ First evenings and vouching (Phase 5)
//
// A newcomer's first evening at an inn: with an innkeeper at work and awake, and others there (not Out of character,
// not blocked, matchmaking on), the innkeeper welcomes them aloud and points out up to four wolves it knows, by the
// newcomer's look for them and what it knows of them; each gets an Introduce prompt, and so does the newcomer. Once a
// character; an evening with no one else there counts, three at most. Vouching: a wolf the resident trusts and likes
// says "They're with me"; a share of that trust and liking carries to the one vouched for, half of it to the
// resident's household; if, within 30 game days, the one vouched for wrongs the resident or is caught at a crime in
// its town, the voucher's standing takes twice the hit and the share is taken back.

void Game::tendEvenings()
{
    const auto& r = newcomers::rules();
    if (now() - eveningsAt_ < 5)
        return;
    eveningsAt_ = now();
    const double days = world_.calendarDays();
    const double hour = (days - std::floor(days)) * 24, day = std::floor(days);
    if (hour < r.eveningFrom || hour >= r.eveningTo)
        return;
    const auto settingOn = [&](const std::string& id) {
        const auto person = people_.find(accountKey(id));
        return person == people_.end() || person->second.settings.matchmaking;
    };
    const auto status = [&](const std::string& id) {
        const auto p = profiles_.find(id);
        return p == profiles_.end() ? std::string("ic") : p->second.status;
    };
    // Newcomers due an evening, by the innkeeper at work and awake where they are: welcomed together, so a room with
    // several newcomers hears one welcome; an innkeeper welcomes no one again for ten minutes.
    std::map<std::string, std::vector<std::string>> due;
    for (const auto* c : clients_)
    {
        if (!c || c->entityId.empty())
            continue;
        const auto& id = c->entityId;
        const auto* e = world_.entity(id);
        if (!e || e->npc || !isNewcomer(accountKey(c)) || !settingOn(id))
            continue;
        const auto& ev = evenings_[id];
        if (ev.done || ev.tries >= r.eveningTries || ev.lastDay == day)
            continue;
        for (const auto* o : world_.entitiesIn(e->cellId))
        {
            if (!o->npc || o->dead)
                continue;
            const auto* post = world_.society().jobOf(o->id);
            const auto life = world_.society().state().residents.find(o->id);
            const bool awake = life == world_.society().state().residents.end() || life->second.task != "sleep";
            if (awake && post && post->work.cell == e->cellId && jobCategoryOf(o->id) == "innkeeper")
            {
                due[o->id].push_back(id);
                break;
            }
        }
    }
    for (const auto& [innkeeper, newcomers] : due)
    {
        if (const auto at = welcomedAt_.find(innkeeper); at != welcomedAt_.end() && now() - at->second < 600)
            continue;                               // (Not counted as an evening tried: it is tried again soon.)
        const auto* inn = world_.entity(innkeeper);
        std::vector<std::string> others;
        for (const auto* o : world_.entitiesIn(inn->cellId))
            if (!o->npc && !o->dead && std::find(newcomers.begin(), newcomers.end(), o->id) == newcomers.end() &&
                status(o->id) != "ooc" && settingOn(o->id) &&
                std::none_of(newcomers.begin(), newcomers.end(), [&](const std::string& n) { return blocked(n, o->id); }))
                others.push_back(o->id);
        for (const auto& id : newcomers)
        {
            auto& ev = evenings_[id];
            ev.lastDay = day;
            ++ev.tries;
            ev.done = !others.empty();
        }
        if (!others.empty())
        {
            welcomedAt_[innkeeper] = now();
            welcomeAtInn(newcomers, innkeeper, others);
        }
        saveSoon();
    }
}

void Game::welcomeAtInn(const std::vector<std::string>& newcomers, const std::string& innkeeper, const std::vector<std::string>& others)
{
    const auto& m = newcomers::matchRules();
    const auto& r = newcomers::rules();
    // Said aloud to the newcomers, the room hearing; the wolves are named, and each listener hears them as it knows
    // them: by name, or by look (so a name is never passed on as hearsay: doc 32, 1.5).
    const auto say = [&](const std::string& text) {
        ParsedPost post;
        post.ok = true;
        post.speech = true;
        post.veilNames = true;
        post.segments.push_back({"speech", text});
        publish(innkeeper, post, Voice::Speak, newcomers);
        npcLastSpeech_[innkeeper] = world_.time();
    };
    say(m.welcome);
    std::vector<std::string> pointed;
    for (const auto& id : others)
    {
        if (int(pointed.size()) >= r.eveningLines)
            break;
        const auto* bond = world_.bonds().find(innkeeper, id);
        if (!bond || bond->familiarity < m.knowsFamiliarity)
            continue;
        std::vector<std::string> facts;
        const auto fact = [&](const char* key) {
            if (const auto f = m.facts.find(key); f != m.facts.end())
                facts.push_back(f->second);
        };
        fact(bond->familiarity >= r.regular ? "regular" : "sometimes");
        if (const auto p = profiles_.find(id); p != profiles_.end() && p->second.status == "lfs")
            fact("looking");
        const auto account = accountKey(id);
        const auto person = people_.find(account);
        if (availableMentor(account) || (person != people_.end() && person->second.experience == "guide" && !person->second.mentor.on))
            fact("guide");
        std::string what;
        for (std::size_t i = 0; i < facts.size(); ++i)
            what += (i == 0 ? "" : i + 1 == facts.size() ? " and " : ", ") + facts[i];
        say(newcomers::fill(m.pointOut, {{"who", nameOf(id)}, {"what", what}}));
        pointed.push_back(id);
    }
    // Names stay with their owners: each pointed out, and the newcomers, are asked to introduce themselves.
    const auto prompt = [&](const std::string& to, const std::string& target, const std::string& words) {
        if (auto* c = clientOf(to))
        {
            auto e = Value::object();
            e.add("type", "introducePrompt");
            e.add("target", target);
            auto inn = labelFor(to, innkeeper);
            if (!inn.empty())
                inn[0] = char(std::toupper(static_cast<unsigned char>(inn[0])));
            e.add("text", newcomers::fill(words, {{"innkeeper", inn}}));
            e.add("seconds", 120);
            send(c, e);
        }
    };
    for (const auto& id : pointed)
        prompt(id, newcomers.size() == 1 ? newcomers.front() : std::string(), m.promptPointed);
    if (!pointed.empty())
        for (const auto& id : newcomers)
            prompt(id, {}, m.promptNewcomer);
    for (const auto& id : newcomers)
        logEvent("welcome", innkeeper, id, std::to_string(pointed.size()) + " pointed out");
}

bool Game::vouchCommand(Connection* c, const json::Value& j, Result& result)
{
    // {"type": "vouch", "resident": id, "for": id}: "They're with me. I'll vouch for them."
    if (!c || c->entityId.empty())
        return false;
    const auto& r = newcomers::rules();
    const auto& w = r.vouching;
    const auto voucher = c->entityId;
    const auto residentId = j.string("resident"), vouched = j.string("for");
    const auto* resident = world_.entity(residentId);
    const auto* them = world_.entity(vouched);
    const auto* me = world_.entity(voucher);
    if (!resident || !resident->npc || resident->dead || !them || them->npc || vouched == voucher || !me)
    {
        result = {false, "Vouch for a wolf here, to a resident here.", {}};
        return true;
    }
    if (resident->cellId != me->cellId || them->cellId != me->cellId ||
        world_.perceive(residentId, voucher, Voice::Speak).hearing <= 0 || world_.perceive(residentId, vouched, Voice::Speak).hearing <= 0)
    {
        result = {false, "They must both be in earshot of the resident.", {}};
        return true;
    }
    if (blocked(voucher, vouched))
    {
        result = {false, "You can't vouch for that wolf.", {}};
        return true;
    }
    const auto* inVoucher = world_.bonds().find(residentId, voucher);
    const auto* inVouched = world_.bonds().find(residentId, vouched);
    int active = 0;
    bool already = false;
    for (const auto& [id, v] : vouches_)
        if (v.state == "active")
        {
            active += v.voucher == voucher;
            already = already || (v.resident == residentId && v.vouched == vouched);
        }
    std::string why;
    if (!newcomers::mayVouch(inVoucher ? inVoucher->trust : 0, inVoucher ? inVoucher->affinity : 0, inVouched ? inVouched->trust : 0,
                             active, already, why, r))
    {
        result = {false, why, {}};
        return true;
    }
    // Said aloud, so it happens in the world and the resident hears it.
    ParsedPost post;
    post.ok = true;
    post.speech = true;
    post.segments.push_back({"speech", "They're with me. I'll vouch for them."});
    publish(voucher, post, Voice::Speak, {residentId});
    const double day = world_.calendarDays();
    newcomers::Vouch v;
    v.id = "vouch-" + guid().substr(0, 12);
    v.resident = residentId;
    v.voucher = voucher;
    v.vouched = vouched;
    v.at = now();
    v.day = day;
    v.until = day + w.days;
    v.given = newcomers::vouchShare(inVoucher->trust, inVoucher->affinity, r);
    world_.bonds().change(residentId, vouched, {v.given.liking, v.given.trust, v.given.familiarity, 0, 0}, day);
    // The household gets half (the user, 2026-10-07).
    for (const auto& [id, life] : world_.society().state().residents)
    {
        if (int(v.household.size()) >= w.householdMost)
            break;
        if (!world_.society().household(residentId, id))
            continue;
        const newcomers::VouchGift gift{id, v.given.trust * w.householdShare, v.given.liking * w.householdShare,
                                        v.given.familiarity * w.householdShare};
        world_.bonds().change(id, vouched, {gift.liking, gift.trust, gift.familiarity, 0, 0}, day);
        v.household.push_back(gift);
    }
    vouches_[v.id] = v;
    logEvent("vouch", voucher, vouched, residentId);
    saveSoon();
    result = {true, "You vouch for " + labelFor(voucher, vouched) + " to " + labelFor(voucher, residentId) + ". Your word goes with them for " +
                        std::to_string(int(w.days)) + " days.", {}};
    return true;
}

void Game::tendVouches()
{
    // New crime incidents: one vouched for who wrongs the resident, is seen at it by the resident, or is caught at a
    // crime in the resident's town breaks the vouch. Old vouches end quietly, their share kept.
    for (const auto& inc : world_.crime().incidents)
    {
        const auto n = inc.id.rfind("inc-", 0) == 0 ? std::atoll(inc.id.c_str() + 4) : 0;
        if (n <= vouchCursor_)
            continue;
        vouchCursor_ = n;
        for (auto& [id, v] : vouches_)
        {
            if (v.state != "active" || v.vouched != inc.offender || inc.day < v.day)
                continue;
            const auto* resident = world_.entity(v.resident);
            const bool seen = std::any_of(inc.witnesses.begin(), inc.witnesses.end(),
                                          [&](const Witness& w) { return w.id == v.resident && w.identified; });
            const bool here = resident && !inc.town.empty() && world_.lawTown(resident->cellId) == inc.town;
            if (inc.victim == v.resident || seen || here)
                breakVouch(v);
        }
    }
    const double day = world_.calendarDays();
    for (auto& [id, v] : vouches_)
        if (v.state == "active" && day > v.until)
            v.state = "ended";
}

void Game::breakVouch(newcomers::Vouch& v)
{
    const auto& w = newcomers::rules().vouching;
    const double day = world_.calendarDays();
    world_.bonds().change(v.resident, v.voucher, {-w.hitLiking, -w.hitTrust * v.given.trust, 0, 0, 0}, day);
    world_.bonds().change(v.resident, v.vouched, {-v.given.liking, -v.given.trust, 0, 0, 0}, day);
    for (const auto& g : v.household)
        world_.bonds().change(g.id, v.vouched, {-g.liking, -g.trust, 0, 0, 0}, day);
    v.state = "broken";
    v.raised = false;
    if (auto* c = clientOf(v.voucher))
        system(c, "Word reaches you: " + lookOfCharacter(v.voucher, v.vouched) + ", whom you vouched for to " + labelFor(v.voucher, v.resident) +
                      ", has wronged them. Your word counts for less with them now.");
    logEvent("vouch broken", v.voucher, v.vouched, v.resident);
    saveSoon();
}

std::string Game::vouchBriefing(const std::string& npc, const std::string& player)
{
    // A resident's briefing: who vouched for this wolf; or that the one this wolf vouched for wronged it (said once).
    std::string out;
    for (auto& [id, v] : vouches_)
    {
        if (v.resident != npc)
            continue;
        if (v.state == "active" && v.vouched == player)
            out += " " + names::capitalised(knowsName(npc, v.voucher) ? nameOf(v.voucher) : labelFor(npc, v.voucher)) +
                   " vouched for this wolf: you think a little better of them for it.";
        else if (v.state == "broken" && v.voucher == player && !v.raised)
        {
            out += " This wolf vouched for " + (knowsName(npc, v.vouched) ? nameOf(v.vouched) : labelFor(npc, v.vouched)) +
                   ", who has since wronged you; bring it up.";
            v.raised = true;
            saveSoon();
        }
    }
    return out;
}

void Game::vouchesSave(json::Value& root) const
{
    auto list = Value::array();
    for (const auto& [id, v] : vouches_)
        list.push(newcomers::saveVouch(v));
    root.add("vouches", list);
    auto evenings = Value::array();
    for (const auto& [id, ev] : evenings_)
    {
        auto o = Value::object();
        o.add("character", id);
        o.add("tries", ev.tries);
        o.add("lastDay", ev.lastDay);
        o.add("done", ev.done);
        evenings.push(o);
    }
    root.add("evenings", evenings);
}

void Game::vouchesLoad(const json::Value& saved)
{
    vouches_.clear();
    for (const auto& e : saved.array("vouches"))
    {
        auto v = newcomers::loadVouch(e);
        if (!v.id.empty() && characters_.count(v.voucher) && characters_.count(v.vouched))
            vouches_[v.id] = std::move(v);
    }
    evenings_.clear();
    for (const auto& e : saved.array("evenings"))
        if (const auto id = e.string("character"); characters_.count(id))
            evenings_[id] = {int(e.number("tries", 0)), e.number("lastDay", -1), e.boolean("done", false)};
}
} // namespace ratw::game
