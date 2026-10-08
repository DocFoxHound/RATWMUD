// The chronicle (Docs/Design/56-fame-and-memory.md, 8): see RatwChronicle.h.
#include "RatwChronicle.h"

#include "RatwCalendar.h"
#include "RatwFame.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace ratw::chronicle
{
using json::Value;

namespace
{
std::string replaced(std::string text, const std::string& blank, const std::string& with)
{
    for (auto at = text.find(blank); at != std::string::npos; at = text.find(blank, at + with.size()))
        text.replace(at, blank.size(), with);
    return text;
}
std::string capitalised(std::string s)
{
    if (!s.empty())
        s[0] = char(std::toupper(static_cast<unsigned char>(s[0])));
    return s;
}
std::string beforeId(const std::string& detail)
{
    const auto at = detail.rfind(" (");
    return at == std::string::npos ? detail : detail.substr(0, at);
}
std::string contestWords(const std::string& kind)
{
    return kind == "race" ? "the race" : kind == "tug" ? "the tug-of-war" : kind == "howl" ? "the howling" : kind == "tourney" ? "the sparring tourney"
         : kind == "hunt" ? "the hunting contest" : kind == "story" ? "the storytelling" : "a contest";
}
const std::map<std::string, std::pair<std::string, std::string>> Plurals{
    {"wolves", {"wolf", "wolves"}}, {"beasts", {"beast", "beasts"}}, {"times", {"time", "times"}},
    {"games", {"game", "games"}},   {"things", {"thing", "things"}}, {"places", {"place", "places"}}};
} // namespace

std::string dateWords(double day)
{
    const auto c = calendar::calendarAt(day);
    return capitalised(calendar::seasonName(c.season)) + " " + std::to_string(c.dayOfSeason) + ", Year " + std::to_string(c.year);
}

std::vector<Entry> compile(const std::string& owner, std::vector<Row> rows, const Lens& lens, const Value& given)
{
    static const Value data = fame::dataFile("chronicle.json");
    const Value& t = given.isObject() ? given : data;
    const auto& told = t.object("told");
    const auto& folded = t.object("folded");
    std::set<std::string> firstKinds;
    for (const auto& k : t.array("first"))
        if (k.isString())
            firstKinds.insert(k.asString());
    std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.day < b.day; });
    std::vector<Entry> out;
    std::set<std::string> seen, places;
    // The season's round: counts by fold kind, and the different wolves talked with.
    struct Round
    {
        double day = 0;
        std::map<std::string, int> counts;
        std::set<std::string> talked;
    };
    std::map<std::pair<int, int>, Round> rounds;
    for (const auto& r : rows)
    {
        const bool mine = r.actor == owner, theirs = r.target == owner;
        if (!mine && !theirs)
            continue;
        const auto other = mine ? r.target : r.actor;
        // First times only (per other wolf where there is one).
        if (firstKinds.count(r.kind))
        {
            const auto key = r.kind == "introduced" ? r.kind + "|" + other : r.kind;
            if (r.kind == "arrival")
            {
                const bool fresh = places.insert(r.cell).second;
                if (!seen.insert(key).second)
                {
                    if (fresh && folded.find("arrival"))
                    {
                        const auto c = calendar::calendarAt(r.day);
                        auto& round = rounds[{int(c.year), int(c.season)}];
                        round.day = round.day ? round.day : r.day;
                        ++round.counts["arrival"];
                    }
                    continue;
                }
            }
            else if (!seen.insert(key).second)
                continue;
        }
        else if (folded.find(r.kind))
        {
            const auto c = calendar::calendarAt(r.day);
            auto& round = rounds[{int(c.year), int(c.season)}];
            if (round.counts.empty() && round.talked.empty())
                round.day = r.day;
            if (r.kind == "conversation")
                round.talked.insert(other);
            else
                ++round.counts[r.kind];
            continue;
        }
        const auto& lines = told.object(r.kind);
        const auto line = lines.string(mine ? "actor" : "target");
        if (line.empty())
            continue;
        const auto name = other.empty() ? std::string("someone") : lens.name ? lens.name(other) : other;
        std::string text = replaced(line, "{other}", name);
        text = replaced(text, "{Other}", capitalised(name));
        text = replaced(text, "{place}", lens.place ? lens.place(r.cell) : r.cell);
        text = replaced(text, "{contest}", contestWords(r.item));
        text = replaced(text, "{detail}", r.kind == "deed" || r.kind == "nickname" || r.kind == "nickname dropped" ? beforeId(r.detail) : r.detail);
        out.push_back({r.day, dateWords(r.day) + ": " + text});
    }
    for (const auto& [when, round] : rounds)
    {
        std::vector<std::string> parts;
        auto counts = round.counts;
        if (!round.talked.empty())
            counts["conversation"] = int(round.talked.size());
        for (const auto& [kind, n] : counts)
        {
            auto part = folded.string(kind);
            if (part.empty() || n <= 0)
                continue;
            part = replaced(part, "{n}", std::to_string(n));
            for (const auto& [blank, forms] : Plurals)
                part = replaced(part, "{" + blank + "}", n == 1 ? forms.first : forms.second);
            parts.push_back(part);
        }
        if (parts.empty())
            continue;
        std::string text;
        for (std::size_t i = 0; i < parts.size(); ++i)
            text += (i ? (i + 1 == parts.size() ? " and " : ", ") : "") + parts[i];
        const auto c = calendar::calendarAt(round.day);
        out.push_back({round.day, capitalised(calendar::seasonName(c.season)) + ", Year " + std::to_string(c.year) + ": you " + text + "."});
    }
    std::stable_sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) { return a.day < b.day; });
    return out;
}

bool Reader::start(const std::string& conninfo, const std::string& worldId, std::string& problem)
{
    if (worker_.joinable())
        return true;
    if (!pg_.connect(conninfo, problem))
        return false;
    world_ = worldId;
    stopping_ = false;
    worker_ = std::thread([this] { run(); });
    return true;
}

void Reader::stop()
{
    {
        std::lock_guard<std::mutex> guard(lock_);
        stopping_ = true;
    }
    wake_.notify_all();
    if (worker_.joinable())
        worker_.join();
}

void Reader::ask(const std::string& owner)
{
    {
        std::lock_guard<std::mutex> guard(lock_);
        if (std::none_of(asked_.begin(), asked_.end(), [&](const Asked& a) { return a.owner == owner && a.mode == "chronicle"; }))
            asked_.push_back({owner, "chronicle", 0});
    }
    wake_.notify_all();
}

void Reader::askSince(const std::string& owner, double day)
{
    {
        std::lock_guard<std::mutex> guard(lock_);
        asked_.push_back({owner, "welcome", day});
    }
    wake_.notify_all();
}

std::vector<Reader::Done> Reader::take()
{
    std::lock_guard<std::mutex> guard(lock_);
    auto out = std::move(done_);
    done_.clear();
    return out;
}

void Reader::run()
{
    for (;;)
    {
        Asked asked;
        {
            std::unique_lock<std::mutex> guard(lock_);
            wake_.wait(guard, [this] { return stopping_ || !asked_.empty(); });
            if (stopping_)
                return;
            asked = asked_.front();
            asked_.pop_front();
        }
        Done d;
        d.owner = asked.owner;
        d.mode = asked.mode;
        const auto result = asked.mode == "welcome" ? pg_.exec(SinceQuery, {world_, std::to_string(asked.since)})
                                                    : pg_.exec(Query, {world_, asked.owner});
        if (!result.ok)
            d.error = result.error;
        for (const auto& row : result.rows)
        {
            if (row.size() < 9)
                continue;
            const auto text = [&](std::size_t i) { return row[i] ? *row[i] : std::string(); };
            Row r;
            r.day = std::atof(text(0).c_str());
            r.kind = text(1), r.actor = text(2), r.target = text(3), r.cell = text(4), r.item = text(5);
            r.quantity = std::atoi(text(6).c_str());
            r.coins = std::atoll(text(7).c_str());
            r.detail = text(8);
            d.rows.push_back(std::move(r));
        }
        std::lock_guard<std::mutex> guard(lock_);
        done_.push_back(std::move(d));
    }
}
} // namespace ratw::chronicle
