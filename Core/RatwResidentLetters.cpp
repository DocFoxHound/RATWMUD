// Letters from residents (Docs/Design/55-letters-gifts-favours.md, 5; Phase 5). The rules decide who writes, why, what
// it says in facts and what goes with it; the words are templates (Data/Voice/letters.json), which the light model may
// only polish (Options::letterModelCallsPerHour, off by default), keeping every name and number.
// - Thanks: the day after a deed for the resident (a contract done for it, tending it when Downed, a gift worth 5p or
//   more), from one with liking 40 and trust 20 toward the player; one time in three with a few pennies from its own
//   purse (never a till's stock), at most 6p and a tenth of what it holds.
// - Requests: a resident's courier contract, newly posted, offered first by letter to a player it trusts (30): reserved
//   for them for 2 game days, then open to all. TAKE IT ON from the letter.
// - At most 3 residents' letters a game week to a player, one from any resident. Candidates come from events as they are
//   recorded (World::setEventWatcher), never from scans; they are sent in one pass a game hour.
#include "RatwGame.h"

#include "RatwCalendar.h"
#include "RatwInjury.h"
#include "RatwItems.h"
#include "RatwJsonDoc.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>

namespace ratw::game
{
using json::Value;

namespace
{
constexpr double WeekDays = 7, OfferDays = 2;
constexpr int PerPlayerWeek = 3;
constexpr std::size_t MostDue = 500;

const json::Value& templates()
{
    static const json::Value doc = [] {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::path path;
        if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
            path = fs::path(dir) / "Voice" / "letters.json";
        else
        {
            for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
            {
                if (fs::exists(at / "Data" / "Voice" / "letters.json", ec))
                {
                    path = at / "Data" / "Voice" / "letters.json";
                    break;
                }
                if (at == at.parent_path())
                    break;
            }
#ifdef RATW_SOURCE_DIR
            if (path.empty())
                path = fs::path(RATW_SOURCE_DIR) / "Data" / "Voice" / "letters.json";
#endif
        }
        std::ifstream in(path);
        std::stringstream text;
        text << in.rdbuf();
        json::Value v;
        std::string error;
        if (!in || !json::parse(text.str(), v, error) || !v.isObject() || v.string("format") != "ratw-letters")
        {
            std::cerr << "[warn] RATW_LETTERS no Data/Voice/letters.json: residents write no letters\n";
            return json::Value::object();
        }
        return v;
    }();
    return doc;
}

std::string fill(std::string text, const std::map<std::string, std::string>& blanks)
{
    for (const auto& [key, value] : blanks)
        for (std::size_t at = text.find("{" + key + "}"); at != std::string::npos; at = text.find("{" + key + "}", at + value.size()))
            text.replace(at, key.size() + 2, value);
    return text;
}

std::string deedWords(const WorldEvent& e)
{
    if (e.kind == "tended")
        return "tending me when I was down";
    if (e.kind == "gift")
        return "your gift";
    const auto& d = e.detail;
    if (d.rfind("courier", 0) == 0)
        return "carrying that letter";
    if (d.rfind("procure", 0) == 0 || d.rfind("supply", 0) == 0)
        return "bringing what I needed";
    if (d.rfind("escort", 0) == 0)
        return "seeing us safe on the road";
    if (d.rfind("bounty", 0) == 0)
        return "dealing with those bandits";
    return "the work you did for me";
}
} // namespace

void Game::watchEvent(const WorldEvent& e)
{
    // Occasions (doc 55, 6): a marriage's wedding, a death's funeral, hosted by residents; and their witnesses.
    if (e.kind == "marriage" || e.kind == "mourning")
    {
        const auto* a = world_.entity(e.actor);
        const auto* b = world_.entity(e.target);
        if (e.kind == "marriage" && a && b && a->npc && b->npc)
            world_.planOccasion("wedding", {e.actor, e.target}, e.actor < e.target ? e.actor + "+" + e.target : e.target + "+" + e.actor);
        else if (e.kind == "mourning" && a && a->npc && e.detail == "family")
            world_.planOccasion("funeral", {e.actor}, e.target);
        return;
    }
    if (e.kind == "witnessed")
    {
        if (auto* o = world_.occasion(e.detail))
            for (const auto& host : o->hosts)
            {
                auto& list = witnessedBy_[host];
                list.push_back({e.actor, o->kind, world_.calendarDays()});
                if (list.size() > 8)
                    list.erase(list.begin());
            }
        return;
    }
    // A deed a resident may write to thank a player for: a contract done for it, tending it when Downed, a gift worth
    // 5p or more. Queued for the day after.
    if (e.kind != "contract done" && e.kind != "tended" && e.kind != "gift")
        return;
    const auto* player = world_.entity(e.actor);
    const auto* resident = world_.entity(e.target);
    if (!player || player->npc || !resident || !resident->npc || resident->transient)
        return;
    if (e.kind == "gift")
    {
        const auto* good = e.item.empty() ? nullptr : items::good(e.item.substr(0, e.item.find('~')));
        if (e.coins + std::int64_t(good ? good->price : 0) * e.quantity < 5)
            return;
    }
    if (thanksDue_.size() >= MostDue || std::any_of(thanksDue_.begin(), thanksDue_.end(), [&](const ThanksDue& t) {
            return t.resident == e.target && t.player == e.actor;
        }))
        return;
    thanksDue_.push_back({e.target, e.actor, deedWords(e), world_.calendarDays()});
}

bool Game::residentLetterRoom(const std::string& resident, const std::string& player) const
{
    // At most 3 residents' letters a game week to a player, and one from any resident.
    int toPlayer = 0;
    for (const auto& s : residentLettersSent_)
        if (s.player == player && world_.calendarDays() - s.day < WeekDays)
        {
            ++toPlayer;
            if (s.resident == resident)
                return false;
        }
    return toPlayer < PerPlayerWeek;
}

std::string Game::residentLetterText(const std::string& kind, const std::string& resident, const std::string& player,
                                     const std::map<std::string, std::string>& blanks) const
{
    // The tone by how the resident feels: warm (liking 70), plain (40), brief (it trusts more than it likes).
    const auto* bond = world_.bonds().find(resident, player);
    const double liking = bond ? bond->affinity : 0, trust = bond ? bond->trust : 0;
    const std::string tone = liking >= 70 ? "warm" : trust > liking ? "brief" : "plain";
    const auto& set = templates().object(kind);
    auto list = set.array(tone);
    if (list.empty())
        list = set.array("plain");
    if (list.empty())
        return {};
    const auto pick = std::hash<std::string>{}(resident + player + std::to_string(std::int64_t(world_.calendarDays()))) % list.size();
    return fill(list[pick].asString(), blanks);
}

documents::Document* Game::residentLetter(const std::string& resident, const std::string& player, const std::string& kind, const std::string& text,
                                          const std::string& facts, std::int64_t coins, const std::string& contract)
{
    const auto* r = world_.entity(resident);
    if (!r || text.empty())
        return nullptr;
    // The signature: its name if it would give it and knows the player well (familiarity 50); else how the player knows
    // it ("the baker at The Amber Loaf"), which reads as no name at all.
    const auto* bond = world_.bonds().find(resident, player);
    const bool named = willName(resident, player) && bond && bond->familiarity >= 50;
    documents::Document d;
    d.kind = "resident_letter";
    d.author = resident;
    d.scent = resident;
    d.to = player;
    d.text = text;
    d.sign = named ? r->name : std::string();
    d.fromTown = townFor(r->cellId);
    d.postTown = postTownOf(player);
    d.written = world_.calendarDays();
    const int cells = courierCells(d.fromTown, d.postTown);
    d.deliverAt = d.written + documents::courierHours(cells, cells == 0) / 24;
    d.facts = facts;
    d.contract = contract;
    auto& stored = documents_.add(d);
    if (coins > 0)
    {
        // From its own purse, in the letter.
        auto& society = world_.society();
        stored.escrow = "letter:" + stored.id + "@" + resident;
        society.openAccount(stored.escrow);
        if (society.shift(resident, stored.escrow, "", 0, coins, "a letter's enclosure"))
            stored.encCoins = coins;
        else
            stored.escrow.clear();
    }
    residentLettersSent_.push_back({resident, player, world_.calendarDays()});
    world_.recordEvent({"resident letter", resident, player, r->cellId, 0, 0, kind, 0, coins, stored.id});
    // The light model may put it in the resident's voice, keeping every fact (off by default).
    if (options_.letterModelCallsPerHour > 0)
    {
        const double hour = std::floor(world_.calendarDays() * 24);
        if (hour != letterCallsHour_)
            letterCallsHour_ = hour, letterCalls_ = 0;
        if (letterCalls_ < options_.letterModelCallsPerHour)
        {
            ++letterCalls_;
            const auto id = stored.id;
            mind_.polish(resident, "", "warm", text, [this, id](int status, const std::string& words) {
                if (auto* doc = documents_.find(id); doc && status == 200 && !words.empty() && doc->readAt < 0)
                    doc->text = words;
            });
        }
    }
    return &stored;
}

void Game::tendResidentLetters()
{
    // Once a game hour.
    const double hour = std::floor(world_.calendarDays() * 24);
    if (hour == residentLettersHour_)
        return;
    residentLettersHour_ = hour;
    const double now = world_.calendarDays();
    residentLettersSent_.erase(std::remove_if(residentLettersSent_.begin(), residentLettersSent_.end(),
                                              [&](const ResidentLetterSent& s) { return now - s.day >= WeekDays; }),
                               residentLettersSent_.end());
    auto& society = world_.society();
    inviteToOccasions();
    // Thanks, the day after.
    for (auto it = thanksDue_.begin(); it != thanksDue_.end();)
    {
        if (std::floor(now) <= std::floor(it->day))
        {
            ++it;
            continue;
        }
        const auto t = *it;
        it = thanksDue_.erase(it);
        const auto* bond = world_.bonds().find(t.resident, t.player);
        if (!bond || bond->affinity < 40 || bond->trust < 20 || !characters_.count(t.player) || !residentLetterRoom(t.resident, t.player))
            continue;
        // One time in three, a few pennies from its own purse.
        std::int64_t coins = 0;
        if (std::hash<std::string>{}(t.resident + t.player + std::to_string(std::int64_t(now))) % 3 == 0)
            if (const auto* purse = society.account(t.resident))
                coins = std::min<std::int64_t>(6, purse->cash / 10);
        const auto* r = world_.entity(t.resident);
        const auto from = r ? (willName(t.resident, t.player) && bond->familiarity >= 50 ? r->name : names::capitalised(labelFor(t.player, t.resident)))
                            : std::string();
        std::string gift;
        if (coins > 0)
        {
            const auto lines = templates().array("gift");
            if (!lines.empty())
                gift = fill(lines[std::size_t(coins) % lines.size()].asString(), {{"gift", std::to_string(coins) + " pennies"}});
        }
        const auto text = residentLetterText("thanks", t.resident, t.player,
                                             {{"to", names::capitalised(labelFor(t.resident, t.player))}, {"deed", t.deed}, {"gift", gift}, {"from", from}});
        residentLetter(t.resident, t.player, "thanks", text,
                       "to thank them for " + t.deed + (coins > 0 ? ", and sent " + std::to_string(coins) + " pennies with it" : std::string()), coins, {});
    }
    // Requests: a courier contract a resident posted in the last day, offered first to the player it trusts most.
    for (auto& c : world_.roads().contracts)
    {
        if (c.kind != "courier" || c.status != "open" || !c.offeredTo.empty() || now - c.created > 1)
            continue;
        const auto* poster = world_.entity(c.poster);
        if (!poster || !poster->npc)
            continue;
        std::string best;
        double most = 29.999;
        for (const auto& [id, ch] : characters_)
            if (const auto* bond = world_.bonds().find(c.poster, id); bond && bond->trust > most && residentLetterRoom(c.poster, id))
                best = id, most = bond->trust;
        if (best.empty())
        {
            c.offeredTo = "-";                       // (Nobody to offer it to: it is the board's.)
            continue;
        }
        c.offeredTo = best;
        c.offeredUntil = now + OfferDays;
        const auto* bond = world_.bonds().find(c.poster, best);
        const auto from = willName(c.poster, best) && bond && bond->familiarity >= 50 ? poster->name : names::capitalised(labelFor(best, c.poster));
        const auto* to = world_.entity(c.target);
        const auto place = to ? townWords(townFor(to->cellId)) : std::string("another town");
        const auto text = residentLetterText("request", c.poster, best,
                                             {{"to", names::capitalised(labelFor(c.poster, best))}, {"place", place}, {"reward", std::to_string(c.reward)},
                                              {"days", std::to_string(int(OfferDays))}, {"from", from}});
        residentLetter(c.poster, best, "request", text, "offering them a letter to carry to " + place, 0, c.id);
    }
}

void Game::inviteToOccasions()
{
    // A day ahead where it can: each occasion's hosts invite up to 3 players they like (liking 50), by letter.
    const double now = world_.calendarDays();
    for (const auto& planned : world_.occasions())
    {
        if (planned.invitesSent || planned.start - now > 1.2)
            continue;
        auto* o = world_.occasion(planned.id);
        o->invitesSent = true;
        std::vector<std::pair<double, std::string>> liked;
        for (const auto& [id, ch] : characters_)
        {
            double most = 0;
            for (const auto& host : o->hosts)
                if (const auto* bond = world_.bonds().find(host, id))
                    most = std::max(most, bond->affinity);
            if (most >= 50)
                liked.push_back({most, id});
        }
        std::sort(liked.rbegin(), liked.rend());
        for (std::size_t i = 0; i < liked.size() && i < 3; ++i)
        {
            const auto& player = liked[i].second;
            std::string host;
            double best = -1;
            for (const auto& h : o->hosts)
                if (const auto* bond = world_.bonds().find(h, player); bond && bond->affinity > best)
                    host = h, best = bond->affinity;
            const auto* r = world_.entity(host);
            if (!r || !residentLetterRoom(host, player))
                continue;
            std::string others;
            for (const auto& h : o->hosts)
                if (h != host)
                    if (const auto* e = world_.entity(h))
                        others = names::capitalised(labelFor(player, h));
            const auto* subject = world_.entity(o->id.substr(o->id.find(':') + 1));
            const int hour = int(std::lround((o->start - std::floor(o->start)) * 24));
            const auto when = calendar::weekdayName(calendar::weekdayOf(o->start)) + " at " + std::to_string(hour) + ":00";
            const auto place = townWords(o->community) + "'s church";
            const auto* bond = world_.bonds().find(host, player);
            const auto from = willName(host, player) && bond && bond->familiarity >= 50 ? r->name : names::capitalised(labelFor(player, host));
            const auto text = residentLetterText(o->kind == "wedding" ? "wedding" : "funeral", host, player,
                                                 {{"to", names::capitalised(labelFor(host, player))}, {"when", when}, {"place", place},
                                                  {"couple", others}, {"deceased", subject ? subject->name : std::string("the one we lost")},
                                                  {"from", from}});
            if (auto* d = residentLetter(host, player, "invitation", text,
                                         "to ask them to " + std::string(o->kind == "wedding" ? "your wedding" : "the funeral") + " on " + when, 0, {}))
            {
                d->occasion = o->id;
                o->invited.insert(player);
            }
        }
    }
}

std::string Game::residentLetterBriefing(const std::string& npc, const std::string& player) const
{
    // The resident remembers what it wrote them in the last 28 days: at most two lines.
    std::vector<const documents::Document*> mine;
    for (const auto* d : documents_.fromAuthor(npc))
        if (d->to == player && d->kind == "resident_letter" && world_.calendarDays() - d->written <= 28 && !d->facts.empty())
            mine.push_back(d);
    std::sort(mine.begin(), mine.end(), [](const auto* a, const auto* b) { return a->written > b->written; });
    std::string out;
    for (std::size_t i = 0; i < mine.size() && i < 2; ++i)
        out += " You wrote to this wolf " + injury::dateWords(mine[i]->written) + " " + mine[i]->facts + ".";
    // An occasion it stood witness at (doc 55, 6).
    if (const auto found = witnessedBy_.find(npc); found != witnessedBy_.end())
        for (auto it = found->second.rbegin(); it != found->second.rend(); ++it)
            if (it->player == player)
            {
                out += it->kind == "wedding" ? " This wolf stood witness at your wedding." : " This wolf stood with you at the funeral.";
                break;
            }
    return out;
}
} // namespace ratw::game
