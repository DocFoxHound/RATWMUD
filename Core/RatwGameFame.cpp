// Fame in the game (Docs/Design/56-fame-and-memory.md; Phase 1, the deed ledger). Deeds come from the world's events
// as they are recorded (Game::watchEvent), and from the DM (deed.award). The world doesn't know names, so the game fills
// them in: for each witness, the name it knew each doer by (or none: it knows them only by look); for each doer, its
// look then. Witnesses, the one it was done for and that one's household believe it (claim "deed:<id>"), and daily
// gossip carries it on. A notable deed or greater warms each resident who ties it to the doer, once (the user,
// 2026-10-08: +5 liking).
#include "RatwGame.h"

#include "RatwCalendar.h"
#include "RatwInjury.h"
#include "RatwItems.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace ratw::game
{
using json::Value;

namespace
{
constexpr double WitnessReach = 12, SeasonDays = 92;

std::string contestWords(const std::string& kind)
{
    return kind == "race" ? "the race" : kind == "tug" ? "the tug-of-war" : kind == "howl" ? "the howling" : kind == "tourney" ? "the sparring tourney"
         : kind == "hunt" ? "the hunting contest" : kind == "story" ? "the storytelling" : "a contest";
}
// "… (inc-12)" -> "inc-12".
std::string inBrackets(const std::string& text)
{
    const auto open = text.rfind('('), close = text.rfind(')');
    return open != std::string::npos && close != std::string::npos && close > open ? text.substr(open + 1, close - open - 1) : std::string();
}
} // namespace

std::string Game::deedPhrase(const std::string& viewer, const fame::Deed& d) const
{
    std::string beneficiary;
    if (d.beneficiary.rfind("town:", 0) == 0)
        beneficiary = townWords(d.beneficiary.substr(5));
    else if (!d.beneficiary.empty())
        beneficiary = d.beneficiary == viewer ? std::string("you") : viewer.empty() ? strangerLabel(d.beneficiary) : labelFor(viewer, d.beneficiary);
    return fame::phrase(d, beneficiary);
}

std::string Game::recordDeed(const std::string& kind, const std::vector<std::string>& doers, const std::string& beneficiary,
                             const std::string& cell, const std::string& source, const std::string& detail, int weight)
{
    const auto* k = fame::kind(kind);
    if (!k)
        return {};
    fame::Deed d;
    for (const auto& id : doers)
        if (const auto* e = world_.entity(id); e && !e->npc && !e->dead && d.doers.size() < 6 &&
                                                std::find(d.doers.begin(), d.doers.end(), id) == d.doers.end())
            d.doers.push_back(id);
    if (d.doers.empty())
        return {};
    const double today = world_.calendarDays();
    // A kind capped for one beneficiary in a season: the third courier letter to one resident makes no deed.
    if (k->perSeason > 0)
    {
        int made = 0;
        for (const auto* old : fame_.byDoer(d.doers[0]))
            made += old->kind == kind && old->beneficiary == beneficiary && today - old->day < SeasonDays;
        if (made >= k->perSeason)
            return {};
    }
    const auto* first = world_.entity(d.doers[0]);
    d.kind = kind;
    d.weight = weight >= 0 ? std::clamp(weight, 0, 3) : k->weight;
    d.beneficiary = beneficiary;
    d.cell = cell.empty() ? first->cellId : cell;
    d.town = world_.lawTown(d.cell);
    if (const auto* c = world_.cell(d.cell))
        d.place = c->name;
    d.detail = detail;
    d.source = source;
    d.day = today;
    d.x = first->position.x;
    d.y = first->position.y;
    // Witnesses: the one it was done for, then whoever was near enough to see, nearest first.
    std::vector<std::pair<double, std::string>> near;
    for (const auto* o : world_.entitiesIn(d.cell))
        if (o && o->cellId == d.cell && !o->dead && !o->transient && o->id != beneficiary &&
            std::find(d.doers.begin(), d.doers.end(), o->id) == d.doers.end())
            if (const double far = std::hypot(o->position.x - d.x, o->position.y - d.y); far <= WitnessReach)
                near.push_back({far, o->id});
    std::sort(near.begin(), near.end());
    std::vector<std::string> seen;
    if (const auto* b = world_.entity(beneficiary); b && !b->dead)
        seen.push_back(beneficiary);
    for (const auto& [far, id] : near)
        if (int(seen.size()) < fame::rules().witnessesMost)
            seen.push_back(id);
    for (const auto& w : seen)
    {
        fame::Witness witness{w, {}};
        for (const auto& doer : d.doers)
        {
            const auto name = knowsName(w, doer) ? labelFor(w, doer) : std::string();
            witness.as[doer] = name;
            auto& names = d.names[doer];
            if (!name.empty() && names.size() < 4 && std::find(names.begin(), names.end(), name) == names.end())
                names.push_back(name);
        }
        d.witnesses.push_back(std::move(witness));
    }
    for (const auto& doer : d.doers)
    {
        d.looks[doer] = strangerLabel(doer);
        // A Dungeon Master's award credits the wolf by its own name (a world story's credits).
        if (source == "dm" && d.names[doer].empty())
            d.names[doer].push_back(nameOf(doer));
    }
    auto& deed = fame_.record(std::move(d));
    spreadDeed(deed);
    const auto claim = "deed:" + deed.id;
    // Who believes it: the witnesses (it was done for one of them), and the household of the one it was done for.
    std::vector<std::pair<std::string, double>> believers;
    for (const auto& w : deed.witnesses)
        believers.push_back({w.id, .9});
    if (const auto& residents = world_.society().state().residents; residents.count(beneficiary))
        for (const auto& [id, life] : residents)
            if (id != beneficiary && life.homeCell == residents.at(beneficiary).homeCell && world_.society().household(id, beneficiary))
                believers.push_back({id, .8});
    for (const auto& [id, sure] : believers)
    {
        const auto* holder = world_.entity(id);
        if (!holder || !holder->npc)
            continue;
        bool tied = false;
        for (const auto& doer : deed.doers)
        {
            const auto as = knowsName(id, doer) ? labelFor(id, doer) : std::string();
            world_.believe(id, doer, claim, id == beneficiary ? "it was done for them" : sure >= .9 ? "saw it" : "the family", sure, {}, as);
            tied = true;
            // A notable deed or greater warms a resident to the doer, once (the user, 2026-10-08).
            if (deed.weight >= fame::Notable)
                world_.bonds().change(id, doer, {fame::rules().warmth, 0, 0, 0, 0}, today);
        }
        if (tied && deed.weight >= fame::Notable)
            deed.warmed.push_back(id);
    }
    world_.recordEvent({"deed", deed.doers[0], beneficiary, deed.cell, 0, 0, kind, deed.weight, 0, deedPhrase({}, deed) + " (" + deed.id + ")"});
    for (const auto& doer : deed.doers)
        record(Character, doer);
    // A nickname: at a notable deed or greater, or at the third small deed of one family in a town within a season.
    const auto id = deed.id;
    bool coin = deed.weight >= fame::Notable;
    if (!coin)
    {
        int alike = 0;
        for (const auto* old : fame_.byDoer(deed.doers[0]))
            if (const auto* ok = fame::kind(old->kind); ok && ok->family == k->family && old->town == deed.town &&
                                                            today - old->day <= fame::nicknameRules().withinDays)
                ++alike;
        coin = alike >= fame::nicknameRules().smallOfAFamily;
    }
    if (coin)
        tryNickname(id);
    saveSoon();
    return id;
}

bool Game::revokeDeed(const std::string& id)
{
    if (!fame_.revoke(id))
        return false;
    world_.forgetClaim("deed:" + id);
    world_.recordEvent({"deed revoked", {}, {}, {}, 0, 0, {}, 0, 0, id});
    saveSoon();
    return true;
}

void Game::fameFromEvent(const WorldEvent& e)
{
    const auto* actor = world_.entity(e.actor);
    const bool player = actor && !actor->npc;
    if (e.kind == "camp cleared" && player)
        recordDeed("broke_camp", {e.actor}, "town:" + world_.lawTown(e.cell), e.cell, "camp " + e.target);
    else if (e.kind == "tended" && player)
        recordDeed("tended", {e.actor}, e.target, e.cell, "tended");
    else if (e.kind == "promise kept" && player)
        recordDeed("kept_promise", {e.actor}, e.target, e.cell, "promise");
    else if (e.kind == "contract done" && player)
    {
        if (e.detail.rfind("courier:", 0) == 0)
            recordDeed("carried_letter", {e.actor}, e.target, e.cell, "contract");
        else if (e.detail.rfind("escort:", 0) == 0)
        {
            recordDeed("escorted_caravan", {e.actor}, "town:" + world_.lawTown(e.cell), e.cell, "contract");
            escortedTo_[world_.lawTown(e.cell)] = world_.time();   // (A guarded caravan carries notable deeds too: doc 56, 2.)
        }
    }
    else if (e.kind == "caravan arrives" && e.detail.rfind("from ", 0) == 0)
    {
        // The carters bring the talk of the town they came from: great deeds always; notable ones only with a caravan a
        // player guarded (doc 48 §1.5).
        const auto from = e.detail.substr(5), to = e.target;
        const auto guarded = escortedTo_.find(to);
        const bool escorted = guarded != escortedTo_.end() && world_.time() - guarded->second < 5;
        const double today = world_.calendarDays();
        for (const auto& [id, cd] : fame_.all())
        {
            if (cd.revoked || cd.weight < fame::Notable || (cd.weight == fame::Notable && !escorted))
                continue;
            const auto has = [&](const std::string& town) {
                return std::any_of(cd.towns.begin(), cd.towns.end(), [&](const fame::TownWord& w) { return w.town == town; });
            };
            if (has(from) && !has(to))
                if (auto* d = fame_.find(id))
                    d->towns.push_back({to, escorted ? "caravan, guarded" : "caravan", today, 0});
        }
    }
    else if (e.kind == "reported" && player)
    {
        // A wolf told the watch; if a warrant follows for that crime, the report got a thief caught.
        auto& who = reportedBy_[inBrackets(e.detail)];
        if (std::find(who.begin(), who.end(), e.actor) == who.end())
            who.push_back(e.actor);
        if (reportedBy_.size() > 512)
            reportedBy_.erase(reportedBy_.begin());
    }
    else if (e.kind == "warrant")
    {
        if (const auto found = reportedBy_.find(inBrackets(e.detail)); found != reportedBy_.end())
        {
            const auto reporters = found->second;
            reportedBy_.erase(found);
            recordDeed("report_thief", reporters, e.target, e.cell, "report");
        }
    }
    else if (e.kind == "festival won" && player)
        recordDeed("festival_won", {e.actor}, "town:" + world_.lawTown(e.cell), e.cell, "festival", contestWords(e.item) + " at " + e.detail);
}

void Game::tendFame()
{
    // Once a game day: deeds past their life go (they live on in game.events).
    const double today = std::floor(world_.calendarDays());
    if (today == fameDay_)
        return;
    fameDay_ = today;
    const double now = world_.calendarDays();
    bool changed = fame_.prune(now);
    // The town's word: what has faded to nothing goes; past the most a town holds, the lightest and oldest go.
    std::map<std::string, std::vector<std::pair<std::string, double>>> byTown;   // town -> (deed, weight*1e6 + since)
    for (const auto& [id, cd] : fame_.all())
        for (const auto& w : cd.towns)
            byTown[w.town].push_back({id, cd.weight * 1e6 + w.since});
    for (auto& [town, list] : byTown)
    {
        std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        for (std::size_t i = 0; i < list.size(); ++i)
            if (auto* d = fame_.find(list[i].first))
            {
                const bool over = int(i) >= fame::rules().townMost;
                const auto before = d->towns.size();
                d->towns.erase(std::remove_if(d->towns.begin(), d->towns.end(),
                                              [&](const fame::TownWord& w) {
                                                  return w.town == town && (over || (now - w.since > 1 && fame::reach(w, d->weight, now) <= 0));
                                              }),
                               d->towns.end());
                changed |= d->towns.size() != before;
            }
    }
    // A notable deed or greater with no nickname yet: once word is half round its town, its innkeeper coins one.
    for (const auto& [id, cd] : fame_.all())
    {
        if (cd.revoked || cd.weight < fame::Notable || !cd.nickname.empty() || cd.noNickname)
            continue;
        const auto word = std::find_if(cd.towns.begin(), cd.towns.end(), [&](const fame::TownWord& w) { return w.town == cd.town; });
        if (word == cd.towns.end() || fame::reach(*word, cd.weight, now) < fame::nicknameRules().innkeeperAtReach)
            continue;
        for (const auto& [resident, life] : world_.society().state().residents)
            if (const auto* job = world_.society().jobOf(resident); job && world_.lawTown(job->work.cell) == cd.town)
                if (const auto* b = items::businessFor(job->title); b && b->id == "inn")
                {
                    if (auto* d = fame_.find(id); d && coinNickname(*d, resident))
                        changed = true;
                    break;
                }
    }
    if (changed)
        saveSoon();
}

// ------------------------------------------------------------------ Phase 3: nicknames

void Game::tryNickname(const std::string& deedId)
{
    // Of those who saw it (and the one it was done for), the one who likes the doer best coins it: liking 20 or more,
    // familiarity 15 or more. Failing that, the innkeeper, later (tendFame).
    auto* d = fame_.find(deedId);
    if (!d || !d->nickname.empty() || d->noNickname || d->doers.empty())
        return;
    const auto& wolf = d->doers[0];
    std::string best;
    double liking = -1e9;
    for (const auto& w : d->witnesses)
        if (const auto* e = world_.entity(w.id); e && e->npc)
            if (const auto* bond = world_.bonds().find(w.id, wolf);
                bond && bond->affinity >= fame::nicknameRules().affinity && bond->familiarity >= fame::nicknameRules().familiarity && bond->affinity > liking)
                liking = bond->affinity, best = w.id;
    if (!best.empty())
        coinNickname(*d, best);
}

bool Game::coinNickname(fame::Deed& d, const std::string& coiner)
{
    const auto& wolf = d.doers[0];
    int worn = 0;
    for (const auto* n : fame_.nicknamesOf(wolf))
        worn += !n->dropped;
    const auto* k = fame::kind(d.kind);
    if (!k || worn >= fame::nicknameRules().most)
        return false;
    const auto name = knowsName(coiner, wolf) ? labelFor(coiner, wolf) : std::string();
    const auto forms = fame::nicknameForms(k->family, name, deedPhrase(coiner, d));
    if (forms.empty())
        return false;
    // One by hash; never one another wolf of the town already wears.
    const auto start = std::hash<std::string>{}(d.id + "|" + wolf) % forms.size();
    for (std::size_t i = 0; i < forms.size(); ++i)
    {
        const auto& text = forms[(start + i) % forms.size()];
        bool taken = false;
        for (const auto& [id, n] : fame_.nicknames())
            taken |= !n.dropped && n.wolf != wolf && n.town == d.town && n.text == text;
        if (taken)
            continue;
        auto& coined = fame_.coin({{}, wolf, text, k->family, d.id, coiner, d.town, world_.calendarDays(), false});
        d.nickname = coined.id;
        world_.recordEvent({"nickname", wolf, coiner, d.cell, 0, 0, {}, 0, 0, text + " (" + coined.id + ")"});
        record(Character, wolf);
        saveSoon();
        return true;
    }
    return false;
}

std::string Game::nicknameFor(const std::string& npc, const std::string& wolf, bool* coined)
{
    // Whoever can tie a deed to the wolf knows its nickname: residents use the one from the heaviest, newest deed.
    for (const auto& r : recognise(npc, wolf, false))
        if (!r.deed->nickname.empty())
            if (const auto* n = fame_.nickname(r.deed->nickname); n && !n->dropped)
            {
                if (coined)
                    *coined = n->coinedBy == npc;
                return n->text;
            }
    return {};
}

bool Game::dropNickname(const std::string& wolf, const std::string& id)
{
    auto* n = fame_.nickname(id);
    if (!n || n->wolf != wolf || n->dropped)
        return false;
    n->dropped = true;
    if (auto* d = fame_.find(n->deed))
        d->noNickname = true;                       // (Never coined again from that deed.)
    world_.recordEvent({"nickname dropped", wolf, {}, {}, 0, 0, {}, 0, 0, n->text + " (" + id + ")"});
    record(Character, wolf);
    saveSoon();
    return true;
}

json::Value Game::nicknamesView(const std::string& wolf)
{
    auto list = Value::array();
    for (const auto* n : fame_.nicknamesOf(wolf))
    {
        auto o = Value::object();
        o.add("id", n->id);
        o.add("text", n->text);
        o.add("coinedBy", names::capitalised(labelFor(wolf, n->coinedBy)));
        o.add("town", townWords(n->town));
        o.add("dropped", n->dropped);
        list.push(o);
    }
    return list;
}

// ------------------------------------------------------------------ Phase 4: festival criers

namespace
{
const json::Value& crierLines()
{
    static const json::Value lines = fame::dataFile("criers.json");
    return lines;
}
std::string pickLine(const std::string& list, const std::string& seed, const std::map<std::string, std::string>& blanks)
{
    const auto& options = crierLines().array(list);
    if (options.empty())
        return {};
    std::string line = options[std::hash<std::string>{}(seed) % options.size()].asString(std::string());
    for (const auto& [k, v] : blanks)
        for (auto at = line.find("{" + k + "}"); at != std::string::npos; at = line.find("{" + k + "}", at + v.size()))
            line.replace(at, k.size() + 2, v);
    return line;
}
} // namespace

std::string Game::crierOf(const std::string& community)
{
    // The town's official, else the innkeeper nearest the square, else its priest.
    const auto* square = world_.marketSpot(community);
    std::string official, inn, priest;
    double nearest = 1e18;
    for (const auto& [id, life] : world_.society().state().residents)
    {
        const auto* job = world_.society().jobOf(id);
        const auto* e = world_.entity(id);
        if (!job || !e || e->dead || world_.lawTown(job->work.cell) != community)
            continue;
        if (official.empty() && jobCategoryOf(id) == "official")
            official = id;
        if (const auto* b = items::businessFor(job->title); b && b->id == "inn")
        {
            const double far = square && job->work.cell == square->cell ? std::hypot(job->work.x - square->x, job->work.y - square->y) : 1e9;
            if (far < nearest)
                nearest = far, inn = id;
        }
        std::string title = job->title;
        for (auto& ch : title)
            ch = char(std::tolower(static_cast<unsigned char>(ch)));
        if (priest.empty() && (title.find("priest") != std::string::npos || title.find("preach") != std::string::npos || title.find("chaplain") != std::string::npos))
            priest = id;
    }
    return !official.empty() ? official : !inn.empty() ? inn : priest;
}

void Game::festivalCrier(const std::string& community, const std::string& festival)
{
    // The season's deeds in the town's word that travel under a name (none called by look: the user, 2026-10-08),
    // heaviest first, then the nicknames coined in town this season. Called deeds are all over town at once, and those at
    // the square believe them under the name they heard.
    const auto* square = world_.marketSpot(community);
    const auto crier = crierOf(community);
    if (!square || crier.empty() || criers_.count(community))
        return;
    const double now = world_.calendarDays(), season = crierLines().number("seasonDays", 92);
    const std::map<std::string, std::string> base{{"town", townWords(community)}, {"festival", festival}};
    std::vector<fame::Deed*> called;
    for (const auto& [id, cd] : fame_.all())
    {
        if (cd.revoked || cd.weight < fame::Notable || now - cd.day > season || cd.doers.empty() || !cd.names.count(cd.doers[0]) ||
            cd.names.at(cd.doers[0]).empty())
            continue;
        if (std::any_of(cd.towns.begin(), cd.towns.end(), [&](const fame::TownWord& w) { return w.town == community; }))
            called.push_back(fame_.find(id));
    }
    std::sort(called.begin(), called.end(), [](const fame::Deed* a, const fame::Deed* b) {
        return a->weight != b->weight ? a->weight > b->weight : a->day > b->day;
    });
    if (int(called.size()) > int(crierLines().number("deedsMost", 6)))
        called.resize(std::size_t(crierLines().number("deedsMost", 6)));
    CrierCall call{community, crier, square->cell, {}, 0, world_.time()};
    call.lines.push_back(pickLine("opening", community + festival, base));
    for (auto* d : called)
    {
        auto blanks = base;
        blanks["name"] = d->names.at(d->doers[0]).front();
        blanks["deed"] = deedPhrase({}, *d);
        call.lines.push_back(pickLine(fame::weightName(d->weight), d->id, blanks));
        for (auto& w : d->towns)                    // (All over town at once.)
            if (w.town == community)
                w.since = std::min(w.since, now - fame::rules().wordDays * 2);
        for (const auto* o : world_.entitiesIn(square->cell))
            if (o && o->npc && !o->dead && o->cellId == square->cell && std::hypot(o->position.x - square->x, o->position.y - square->y) <= 12)
                world_.believe(o->id, d->doers[0], "deed:" + d->id, crier, .7, {}, blanks["name"]);
    }
    for (const auto& [id, n] : fame_.nicknames())
        if (!n.dropped && n.town == community && now - n.day <= season)
            if (const auto* d = fame_.find(n.deed); d && d->names.count(n.wolf) && !d->names.at(n.wolf).empty())
            {
                auto blanks = base;
                blanks["name"] = d->names.at(n.wolf).front();
                blanks["nickname"] = n.text;
                call.lines.push_back(pickLine("nickname", n.id, blanks));
            }
    call.lines.push_back(pickLine("closing", community + festival + "|end", base));
    call.lines.erase(std::remove(call.lines.begin(), call.lines.end(), std::string()), call.lines.end());
    world_.recordEvent({"crier", crier, {}, square->cell, 0, 0, {}, int(called.size()), 0, festival});
    world_.seatResident(crier, {square->cell, square->x, square->y + 1, "Crying the season's deeds at the square."});
    criers_[community] = std::move(call);
    saveSoon();
}

void Game::cryLegends()
{
    // A legendary deed: every town's crier calls it at noon the day after.
    const double now = world_.calendarDays();
    if ((now - std::floor(now)) * 24 < 12)
        return;
    for (const auto& [id, cd] : fame_.all())
    {
        if (cd.revoked || cd.cried || cd.weight < fame::Legendary || std::floor(cd.day) >= std::floor(now) || cd.doers.empty())
            continue;
        auto* d = fame_.find(id);
        d->cried = true;
        const auto name = d->names.count(d->doers[0]) && !d->names.at(d->doers[0]).empty() ? d->names.at(d->doers[0]).front() : std::string();
        if (name.empty())
            continue;                               // (Nobody can say who: nothing to call.)
        for (const auto& w : d->towns)
        {
            const auto crier = crierOf(w.town);
            const auto* square = world_.marketSpot(w.town);
            if (crier.empty() || !square || criers_.count(w.town))
                continue;
            const std::map<std::string, std::string> blanks{{"town", townWords(w.town)}, {"festival", "this day"}, {"name", name},
                                                            {"deed", deedPhrase({}, *d)}};
            CrierCall call{w.town, crier, square->cell, {pickLine("legendary", d->id + w.town, blanks)}, 0, world_.time()};
            call.lines.erase(std::remove(call.lines.begin(), call.lines.end(), std::string()), call.lines.end());
            world_.seatResident(crier, {square->cell, square->x, square->y + 1, "Crying news at the square."});
            criers_[w.town] = std::move(call);
        }
        saveSoon();
    }
}

void Game::tendCriers()
{
    cryLegends();
    // A line a minute once the crier is at the square, spoken only where a player is in the square's cell (the reach was
    // set either way); no one there, the lines pass unheard.
    for (auto it = criers_.begin(); it != criers_.end();)
    {
        auto& call = it->second;
        const auto* crier = world_.entity(call.crier);
        if (!crier || crier->dead || call.next >= call.lines.size())
        {
            world_.unseatResident(call.crier);
            it = criers_.erase(it);
            continue;
        }
        if (world_.time() < call.nextAt)
        {
            ++it;
            continue;
        }
        bool heard = false;
        for (const auto* c : clients_)
            if (const auto* p = c ? world_.entity(c->entityId) : nullptr; p && p->cellId == call.cell)
                heard = true;
        const auto* square = world_.marketSpot(call.community);
        const bool there = square && crier->cellId == call.cell && std::hypot(crier->position.x - square->x, crier->position.y - square->y - 1) <= 3;
        if (heard && !there && world_.time() - call.nextAt < 300)
        {
            ++it;                                   // (Still on the way to the square.)
            continue;
        }
        if (heard)
        {
            ParsedPost post;
            post.ok = true;
            post.speech = true;
            post.segments.push_back({"speech", call.lines[call.next]});
            publish(call.crier, post, Voice::Yell);
            npcLastSpeech_[call.crier] = world_.time();
        }
        ++call.next;
        call.nextAt = world_.time() + 60;
        ++it;
    }
}

// ------------------------------------------------------------------ Phase 5: the chronicle

bool Game::chronicleCommand(Connection* c, Result& result)
{
    const auto me = c->entityId;
    if (const auto asked = chronicleAsked_.find(me); asked != chronicleAsked_.end() && now() - asked->second < 30)
        return result = {false, "Your chronicle is being written; ask again in a little while.", {}}, true;
    chronicleAsked_[me] = now();
    // The database's log where there is one (read on its own thread); else what is in memory.
    if (store_ && !options_.conninfo.empty() && !liveWorldId_.empty())
    {
        std::string problem;
        if (chronicleReader_.start(options_.conninfo, liveWorldId_, problem))
        {
            chronicleReader_.ask(me);
            return result = {true, "", {}}, true;
        }
        note("warn", "RATW_CHRONICLE no reader (" + problem + "): from memory");
    }
    std::vector<chronicle::Row> rows;
    for (const auto& e : world_.recentEvents())
        if (e.actor == me || e.target == me)
            rows.push_back({e.day, e.kind, e.actor, e.target, e.cell, e.item, e.detail, e.quantity, e.coins});
    sendChronicle(me, std::move(rows), true);
    return result = {true, "", {}}, true;
}

void Game::tendChronicle()
{
    if (!chronicleReader_.running())
        return;
    for (auto& done : chronicleReader_.take())
    {
        if (!done.error.empty())
            note("warn", "RATW_CHRONICLE " + done.error);
        if (done.mode == "welcome")
            sendWelcome(done.owner, done.rows);
        else
            sendChronicle(done.owner, std::move(done.rows), !done.error.empty());
    }
}

void Game::sendChronicle(const std::string& owner, std::vector<chronicle::Row> rows, bool partial)
{
    auto* c = clientOf(owner);
    if (!c)
        return;
    // Deeds and nicknames from the ledger too, should the log lack them (a deed kept in memory, a partial read).
    std::set<std::string> logged;
    for (const auto& r : rows)
        if (r.kind == "deed" || r.kind == "nickname")
            logged.insert(r.detail);
    for (const auto* d : fame_.byDoer(owner))
        if (const auto detail = deedPhrase({}, *d) + " (" + d->id + ")"; !logged.count(detail))
            rows.push_back({d->day, "deed", owner, d->beneficiary, d->cell, d->kind, detail, d->weight, 0});
    for (const auto* n : fame_.nicknamesOf(owner))
        if (const auto detail = n->text + " (" + n->id + ")"; !logged.count(detail))
            rows.push_back({n->day, "nickname", owner, n->coinedBy, {}, {}, detail, 0, 0});
    chronicle::Lens lens;
    lens.name = [&](const std::string& id) {
        if (id.rfind("town:", 0) == 0)
            return townWords(id.substr(5));
        return world_.entity(id) ? labelFor(owner, id) : std::string("someone");
    };
    lens.place = [&](const std::string& cell) {
        const auto* found = world_.cell(cell);
        return found ? found->name : std::string("somewhere");
    };
    auto e = Value::object();
    e.add("type", "chronicle");
    auto entries = Value::array();
    for (const auto& entry : chronicle::compile(owner, std::move(rows), lens))
    {
        auto o = Value::object();
        o.add("day", entry.day);
        o.add("text", entry.text);
        entries.push(o);
    }
    e.add("entries", entries);
    e.add("partial", partial);
    send(c, e);
}

// ------------------------------------------------------------------ Phase 6: unfinished business, and welcome back

json::Value Game::unfinishedView(const std::string& id)
{
    // Open threads, soonest due first, at most 8; rebuilt at most every 2 s. Nothing nags: it is only shown.
    auto& cached = unfinished_[id];
    if (cached.first > 0 && world_.time() - cached.first < 2)
        return cached.second;
    struct Thread
    {
        std::string text;
        double due = 1e9;
    };
    std::vector<Thread> threads;
    const double today = world_.calendarDays();
    const auto label = [&](const std::string& other) {
        return other.rfind("town:", 0) == 0 ? townWords(other.substr(5)) : world_.entity(other) ? labelFor(id, other) : std::string("someone");
    };
    for (const auto& p : world_.promises())
    {
        if (p.status != "open")
            continue;
        if (p.by == id)
            threads.push_back({"You promised " + label(p.to) + ": \"" + p.what + "\"", p.due - today});
        else if (p.to == id)
            threads.push_back({names::capitalised(label(p.by)) + " promised you: \"" + p.what + "\"", p.due - today});
    }
    for (const auto& k : world_.roads().contracts)
        if (k.taker == id && k.status == "taken")
            threads.push_back({"Work for " + (world_.entity(k.poster) ? label(k.poster) : townWords(k.town)) + ": " + k.detail +
                                   (k.quantity > 0 ? " (" + std::to_string(k.delivered) + " of " + std::to_string(k.quantity) + " handed in)" : ""),
                               k.due > 0 ? k.due - today : 1e8});
    for (const auto& [bookId, b] : books_)
        if (b.state == "finishing" && std::find(b.wolves.begin(), b.wolves.end(), id) != b.wolves.end() && !b.agreed.count(id) && !b.objected.count(id))
            threads.push_back({"\"" + b.title + "\" waits for your word on finishing it.", 1e7});
    if (const int unread = documents_.unread(id); unread > 0)
        threads.push_back({std::to_string(unread) + (unread == 1 ? " letter" : " letters") + " unread in your case.", 1e6});
    std::stable_sort(threads.begin(), threads.end(), [](const Thread& a, const Thread& b) { return a.due < b.due; });
    auto list = Value::array();
    for (std::size_t i = 0; i < threads.size() && i < 8; ++i)
    {
        auto o = Value::object();
        o.add("text", threads[i].text);
        if (threads[i].due < 1e5)
            o.add("days", std::round(threads[i].due * 10) / 10);
        list.push(o);
    }
    cached = {world_.time(), list};
    return list;
}

std::string Game::awayBriefing(const std::string& npc, const std::string& wolf, bool note)
{
    // Back after a break, and this resident hasn't seen them since: how long, in the resident's own (game) time. Once:
    // after this reply they have spoken.
    const auto absence = absences_.find(wolf);
    if (absence == absences_.end())
        return {};
    const auto seen = absence->second.unseen.find(npc);
    if (seen == absence->second.unseen.end())
        return {};
    const double gap = world_.calendarDays() - seen->second;
    const auto when = gap < 14 ? std::to_string(int(std::round(gap))) + " days" : gap < 92 ? std::to_string(int(std::round(gap / 7))) + " weeks"
                                                                                           : std::to_string(int(std::round(gap / 92))) + " seasons";
    const auto season = calendar::seasonName(calendar::calendarAt(seen->second).season);
    if (note)
        absence->second.unseen.erase(seen);
    return "You last saw this wolf about " + when + " ago, in " + season + ". Greet them as someone back after a long while.";
}

void Game::sendWelcome(const std::string& id, const std::vector<chronicle::Row>& events)
{
    auto* c = clientOf(id);
    const auto absence = absences_.find(id);
    if (!c || absence == absences_.end())
        return;
    std::vector<std::string> lines;
    const int days = int(std::round(absence->second.realDays));
    if (const int unread = documents_.unread(id); unread > 0)
        lines.push_back(std::to_string(unread) + (unread == 1 ? " letter waits" : " letters wait") + " in your case.");
    // The residents it knows best (familiarity), and the towns it knows (a deed of its there, or three who know it).
    std::vector<std::pair<double, std::string>> known;
    std::map<std::string, int> knowers;
    for (const auto& [resident, life] : world_.society().state().residents)
        if (const auto* bond = world_.bonds().find(resident, id); bond && bond->familiarity >= 10)
        {
            known.push_back({bond->familiarity, resident});
            ++knowers[world_.lawTown(life.homeCell)];
        }
    std::sort(known.rbegin(), known.rend());
    if (known.size() > 12)
        known.resize(12);
    std::set<std::string> best, towns;
    for (const auto& [f, resident] : known)
        best.insert(resident);
    for (const auto& [town, n] : knowers)
        if (n >= 3)
            towns.insert(town);
    for (const auto* d : fame_.byDoer(id))
        towns.insert(d->town);
    // What became of those it knows.
    for (const auto& e : events)
    {
        if (lines.size() >= 10)
            break;
        const bool a = best.count(e.actor), b = best.count(e.target);
        if (!a && !b)
            continue;
        const auto who = names::capitalised(labelFor(id, a ? e.actor : e.target));
        const auto other = a && !e.target.empty() && world_.entity(e.target) ? labelFor(id, e.target) : std::string();
        if (e.kind == "marriage")
            lines.push_back(who + " married" + (other.empty() ? "." : " " + other + "."));
        else if (e.kind == "death" && a)
            lines.push_back(who + " died.");
        else if (e.kind == "apprenticeship")
            lines.push_back(who + " took up an apprenticeship.");
        else if (e.kind == "apprenticeship completed")
            lines.push_back(who + " finished their apprenticeship.");
        else if (e.kind == "succession")
            lines.push_back(who + " took up a new post.");
        else if (e.kind == "relocation" && a)
            lines.push_back(who + " moved to a new home.");
    }
    // The talk of the towns it knows, since it left.
    int talk = 0;
    for (const auto& [deedId, d] : fame_.all())
    {
        if (talk >= 4 || d.revoked || d.day < absence->second.leftDay || d.doers.empty() || d.doers[0] == id)
            continue;
        for (const auto& w : d.towns)
            if (towns.count(w.town))
            {
                const auto& doer = d.doers[0];
                const auto name = d.names.count(doer) && !d.names.at(doer).empty() ? d.names.at(doer).front()
                                : d.looks.count(doer) ? d.looks.at(doer) : std::string("a wolf");
                lines.push_back("In " + townWords(w.town) + ", folk talk of " + name + ", who " + deedPhrase(id, d) + ".");
                ++talk;
                break;
            }
    }
    if (const auto* e = world_.entity(id); e && e->practice->rested > 0)
        lines.push_back("You come back rested: practice comes twice as fast for a while.");
    auto ev = Value::object();
    ev.add("type", "welcome");
    ev.add("days", days);
    auto list = Value::array();
    for (const auto& l : lines)
        list.push(l);
    ev.add("lines", list);
    send(c, ev);
    logEvent("welcomed back", id, {}, std::to_string(days) + " days away");
}

// ------------------------------------------------------------------ Phase 2: deeds travel and are recognised

void Game::spreadDeed(fame::Deed& d)
{
    // A notable deed or greater: its own town's word from now. A legendary one: every town's, half-way at once.
    const double today = world_.calendarDays();
    if (d.weight >= fame::Legendary)
    {
        std::set<std::string> towns;
        for (const auto& [id, life] : world_.society().state().residents)
            if (const auto town = world_.lawTown(life.homeCell); !town.empty())
                towns.insert(town);
        for (const auto& town : towns)
            d.towns.push_back({town, "legend", today, 0});
    }
    else if (d.weight >= fame::Notable && !d.town.empty())
        d.towns.push_back({d.town, "here", today, 0});
}

bool Game::townHeard(const std::string& npc, const fame::Deed& d) const
{
    // A resident of a town whose word holds it has heard it when its draw is under the reach times its ear: always the
    // same answer, and more of them as days pass.
    const auto& residents = world_.society().state().residents;
    const auto life = residents.find(npc);
    if (life == residents.end())
        return false;
    const auto town = world_.lawTown(life->second.homeCell);
    const auto& r = fame::rules();
    for (const auto& w : d.towns)
        if (w.town == town)
        {
            double ear = 1;
            const auto* e = world_.entity(npc);
            if (e && e->age < 16)
                ear = r.earChild;
            else if (const auto* job = world_.society().jobOf(npc))
            {
                std::string title = job->title;
                for (auto& ch : title)
                    ch = char(std::tolower(static_cast<unsigned char>(ch)));
                ear = job->role == "guard" ? r.earGuard : job->role == "merchant" ? r.earMerchant
                    : title.find("priest") != std::string::npos || title.find("preach") != std::string::npos || title.find("chaplain") != std::string::npos
                        ? r.earClergy : 1;
            }
            const double draw = double(std::hash<std::string>{}(npc + "|" + d.id) % 1000);
            return draw < 1000 * fame::reach(w, d.weight, world_.calendarDays()) * ear;
        }
    return false;
}

std::vector<Game::Recognition> Game::recognise(const std::string& npc, const std::string& wolf, bool warm)
{
    // The deeds `npc` has heard of that it can tie to `wolf`: by a name the deed travels under that it knows the wolf by,
    // or by a distinctive look that still matches. Heaviest, then newest, first.
    std::vector<Recognition> out;
    const auto* holder = world_.entity(npc);
    if (!holder || !holder->npc)
        return out;
    const auto known = knowsName(npc, wolf) ? labelFor(npc, wolf) : std::string();
    const auto lookNow = strangerLabel(wolf);
    for (const auto* d : fame_.byDoer(wolf))
    {
        const Belief* belief = nullptr;
        if (const auto* mine = world_.beliefsOf(npc))
            for (const auto& b : *mine)
                if (b.subject == wolf && b.claim == "deed:" + d->id)
                    belief = &b;
        const bool town = !belief && townHeard(npc, *d);
        if (!belief && !town)
            continue;
        Recognition r;
        r.deed = d;
        const auto names = d->names.count(wolf) ? d->names.at(wolf) : std::vector<std::string>{};
        r.byName = !known.empty() && ((belief && belief->as == known) || std::find(names.begin(), names.end(), known) != names.end());
        const auto look = d->looks.count(wolf) ? d->looks.at(wolf) : std::string();
        // By look: a distinctive one (it names a marking), or any, for one who saw it done or had it done for them.
        const bool saw = belief && (belief->source == "saw it" || belief->source == "it was done for them");
        const bool byLook = !r.byName && !look.empty() && look == lookNow && (saw || look.find(" with ") != std::string::npos);
        if (!r.byName && !byLook)
            continue;
        if (belief)
        {
            const auto* from = world_.entity(belief->source);
            r.how = belief->source == "saw it" ? "you saw it" : belief->source == "it was done for them" ? "it was done for you"
                  : belief->source == "the family" ? "your family told you" : belief->source == "the town's talk" ? "the town's talk"
                  : from ? from->name + " told you" : belief->source + " told you";
            r.sure = belief->confidence;
        }
        else
        {
            r.how = "the town's talk";
            r.sure = .6;
        }
        // A notable deed or greater warms the resident the first time it ties it to them (the user, 2026-10-08).
        if (warm && d->weight >= fame::Notable)
            if (auto* deed = fame_.find(d->id); deed && std::find(deed->warmed.begin(), deed->warmed.end(), npc) == deed->warmed.end())
            {
                deed->warmed.push_back(npc);
                world_.bonds().change(npc, wolf, {fame::rules().warmth, 0, 0, 0, 0}, world_.calendarDays());
            }
        out.push_back(r);
    }
    std::stable_sort(out.begin(), out.end(), [](const Recognition& a, const Recognition& b) {
        return a.deed->weight != b.deed->weight ? a.deed->weight > b.deed->weight : a.deed->day > b.deed->day;
    });
    return out;
}

std::string Game::fameBriefing(const std::string& npc, const std::string& wolf, bool note)
{
    const auto known = recognise(npc, wolf, true);
    if (known.empty())
        return {};
    std::string out;
    const auto realised = fameRealised_.find(npc + "|" + wolf);
    for (std::size_t i = 0; i < known.size() && i < 2; ++i)
    {
        const auto& r = known[i];
        const auto what = deedPhrase(npc, *r.deed);
        std::string line;
        if (realised != fameRealised_.end() && realised->second == r.deed->id)
            line = "You have just realised this is the wolf who " + what + ".";
        else if (r.byName)
            line = "You have heard (" + r.how + "; " + (r.sure >= .8 ? "sure" : r.sure >= .5 ? "fairly sure" : "not sure") + ") that this wolf " + what + ".";
        else
            line = "You have heard (" + r.how + ") that a wolf who " + what + " looked like this one: you think it might be them, but you aren't sure.";
        if (const auto said = fameMentioned_.find(npc + "|" + r.deed->id); said != fameMentioned_.end())
            line += " (You have spoken of it to them before: " + injury::dateWords(said->second) + ".)";
        if (out.size() + line.size() + 1 > 400)
            break;
        out += (out.empty() ? "" : " ") + line;
        if (note)
            fameMentioned_.emplace(npc + "|" + r.deed->id, world_.calendarDays());
    }
    bool coined = false;
    if (const auto nick = nicknameFor(npc, wolf, &coined); !nick.empty())
    {
        const auto line = coined ? "You were the first to call them '" + nick + "'." : "Folk call them '" + nick + "'.";
        if (out.size() + line.size() + 1 <= 400)
            out += " " + line;
    }
    if (note && realised != fameRealised_.end())
        fameRealised_.erase(realised);
    if (fameMentioned_.size() > 20000)
        fameMentioned_.clear();
    return out;
}

std::string Game::fameLine(const std::string& knower, const std::string& wolf)
{
    const auto known = recognise(knower, wolf, false);
    if (known.empty())
        return {};
    return std::string(known[0].byName ? "the one who " : "maybe the one who ") + deedPhrase(knower, *known[0].deed);
}

void Game::fameJoin(const std::string& listener, const std::string& wolf)
{
    // A resident who knew of a deed by look learns the wolf's name: its belief takes the name, the deed travels under it
    // from now (at most 4), and its next reply is told it has just realised who this is.
    const auto* l = world_.entity(listener);
    if (!l || !l->npc)
        return;
    const auto name = labelFor(listener, wolf);
    const auto lookNow = strangerLabel(wolf);
    for (const auto* cd : fame_.byDoer(wolf))
    {
        auto* d = fame_.find(cd->id);
        const auto look = d->looks.count(wolf) ? d->looks.at(wolf) : std::string();
        bool heldByLook = false, saw = false;
        if (const auto* mine = world_.beliefsOf(listener))
            for (const auto& b : *mine)
                if (b.subject == wolf && b.claim == "deed:" + d->id && b.as.empty())
                    heldByLook = true, saw = b.source == "saw it" || b.source == "it was done for them";
        const bool heard = heldByLook || townHeard(listener, *d);
        if (!heard || look.empty() || look != lookNow || (!saw && look.find(" with ") == std::string::npos))
            continue;                               // (Only one who saw it, or a distinctive look, could tell it was this wolf.)
        auto& names = d->names[wolf];
        if (std::find(names.begin(), names.end(), name) != names.end())
            continue;
        if (names.size() < 4)
            names.push_back(name);
        world_.believe(listener, wolf, "deed:" + d->id, heldByLook ? "saw it" : "the town's talk", heldByLook ? .9 : .6, {}, name);
        fameRealised_[listener + "|" + wolf] = d->id;
        saveSoon();
    }
}

World::DeedWords Game::fameWords(const std::string& teller, const std::string& claim, const std::string& subject)
{
    // For ambient talk: the doer as the teller knows them (a name the deed travels under, else its look then), the
    // deed's phrase as the teller would put it.
    World::DeedWords out;
    const auto* d = fame_.find(claim.substr(5));
    if (!d || d->revoked)
        return out;
    std::string as;
    if (const auto* mine = world_.beliefsOf(teller))
        for (const auto& b : *mine)
            if (b.subject == subject && b.claim == claim)
                as = b.as;
    out.byName = !as.empty();
    out.subject = !as.empty() ? as : d->looks.count(subject) ? d->looks.at(subject) : std::string("a wolf");
    out.phrase = deedPhrase(teller, *d);
    if (!d->nickname.empty())
        if (const auto* n = fame_.nickname(d->nickname); n && !n->dropped)
            out.nickname = n->text;
    return out;
}

void Game::fameSave(json::Value& root) const
{
    root.add("deeds", fame_.save());
    root.add("nicknames", fame_.saveNicknames());
}

void Game::fameLoad(const json::Value& saved)
{
    if (const auto* list = saved.find("deeds"))
        fame_.load(*list);
    else
        fame_.load(Value::array());
    if (const auto* list = saved.find("nicknames"))
        fame_.loadNicknames(*list);
    else
        fame_.loadNicknames(Value::array());
}
} // namespace ratw::game
