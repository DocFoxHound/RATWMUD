// The library, the archive and exploration (Docs/Design/54-gathering-places.md, 7; Phase 7). Archive work is given
// where a resident holding a records post (Data/Lore/archives.json: the Hall of Records' copyists, the Concord Annex's
// keepers, the Warden crypt's librarian...) is there and awake: "ask for archive work".
// - Sorting: six records to put in order by their clues (a year of the old count, a roll's number in old numerals, or
//   the time of year). The server keeps the order and checks it; three tries, then the keeper takes them back.
// - Copying: five minutes sitting at a desk in the archive; a scholarship check decides the errors (none or one: full
//   pay; more: half). Nothing sellable is made.
// 2p a task from the town's treasury (`archive work`), 4 a game day. Each finished task grows scholarship
// (Entity::gameSkills["scholarship"]) and shows the next lore fragment of the archive's town not yet read (its `after`
// read first), else a shared one; an archive with nothing new still pays. A wolf who has read a town's records (10, or
// all there are) is known to that town's residents as a scholar. The journal (`journal`): lore, the bestiary (World's
// huntKill), the herbarium (World::forage) and places (each cell a wolf has been in).
#include "RatwGame.h"

#include "RatwInjury.h"
#include "RatwWild.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace ratw::game
{
using json::Value;

namespace
{
struct Fragment
{
    std::string id, region, topic, text, after;
};
struct Lore
{
    std::vector<std::string> posts;
    std::int64_t pay = 2;
    int perDay = 4, scholarAfter = 10;
    double copySeconds = 300;
    std::vector<Fragment> fragments;
};

std::filesystem::path loreFile(const char* name)
{
    // Data/Lore: RATW_DATA_DIR, else the working directory or one above it, else the source tree.
    namespace fs = std::filesystem;
    std::error_code ec;
    if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
        return fs::path(dir) / "Lore" / name;
    for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
    {
        if (fs::exists(at / "Data" / "Lore" / name, ec))
            return at / "Data" / "Lore" / name;
        if (at == at.parent_path())
            break;
    }
#ifdef RATW_SOURCE_DIR
    return fs::path(RATW_SOURCE_DIR) / "Data" / "Lore" / name;
#else
    return fs::path("Data") / "Lore" / name;
#endif
}

json::Value readJson(const char* name)
{
    std::ifstream in(loreFile(name));
    std::stringstream text;
    text << in.rdbuf();
    json::Value doc;
    std::string error;
    if (!in || !json::parse(text.str(), doc, error))
        std::cerr << "[warn] RATW_LORE no Data/Lore/" << name << ": no archive work there\n";
    return doc;
}

const Lore& lore()
{
    static const Lore l = [] {
        Lore out;
        const auto archives = readJson("archives.json");
        for (const auto& p : archives.array("posts"))
            if (p.isString())
                out.posts.push_back(p.asString());
        out.pay = std::int64_t(archives.number("pay", 2));
        out.perDay = int(archives.number("perDay", 4));
        out.copySeconds = archives.number("copySeconds", 300);
        out.scholarAfter = int(archives.number("scholarAfter", 10));
        const auto fragments = readJson("fragments.json");   // (Kept: the loop reads into it.)
        for (const auto& f : fragments.array("fragments"))
            if (!f.string("id").empty() && !f.string("text").empty())
                out.fragments.push_back({f.string("id"), f.string("region"), f.string("topic"), f.string("text").substr(0, 400), f.string("after")});
        return out;
    }();
    return l;
}

std::string lowered(std::string s)
{
    for (auto& ch : s)
        ch = char(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

std::string ordinal(int n)
{
    const int tens = n % 100, ones = n % 10;
    return std::to_string(n) + (tens >= 11 && tens <= 13 ? "th" : ones == 1 ? "st" : ones == 2 ? "nd" : ones == 3 ? "rd" : "th");
}

std::string roman(int n)
{
    static const std::pair<int, const char*> Numerals[] = {{40, "XL"}, {10, "X"}, {9, "IX"}, {5, "V"}, {4, "IV"}, {1, "I"}};
    std::string out;
    for (const auto& [value, glyphs] : Numerals)
        while (n >= value)
            out += glyphs, n -= value;
    return out;
}

const char* const Kinds[] = {"A deed", "A tally of grain", "A marriage roll", "A hearing's ruling", "A letter of passage", "A census page",
                             "A boundary survey", "A guild's dues book", "A bill of sale", "A watch report"};
const char* const Places[] = {"by the ford", "of the east ward", "for the mill", "of the high pastures", "for the river houses", "of the gate street",
                              "for the old orchard", "of the north farms", "by the well", "for the tannery yard"};
const char* const Times[] = {"early spring", "midspring", "late spring", "early summer", "midsummer", "late summer",
                             "early autumn", "midautumn", "late autumn", "early winter", "midwinter", "late winter"};
} // namespace

void Game::refreshArchives()
{
    // The records posts and where they are, once in five minutes.
    if (archivesAt_ >= 0 && world_.time() - archivesAt_ < 300)
        return;
    archivesAt_ = world_.time();
    archiveKeepers_.clear();
    const auto& posts = lore().posts;
    for (const auto& [id, life] : world_.society().state().residents)
        if (const auto* job = world_.society().jobOf(id))
        {
            const auto title = lowered(job->title);
            if (std::any_of(posts.begin(), posts.end(), [&](const std::string& p) { return title.find(p) != std::string::npos; }))
                archiveKeepers_[job->work.cell].push_back(id);
        }
}

bool Game::archiveKeeperHere(const std::string& cell)
{
    refreshArchives();
    const auto found = archiveKeepers_.find(cell);
    if (found == archiveKeepers_.end())
        return false;
    for (const auto& id : found->second)
        if (const auto* e = world_.entity(id); e && !e->dead && e->cellId == cell)
            if (const auto life = world_.society().state().residents.find(id);
                life != world_.society().state().residents.end() && life->second.task != "sleep")
                return true;
    return false;
}

void Game::sendArchiveTask(Connection* c, const ArchiveTask& t)
{
    auto e = Value::object();
    e.add("type", "archive");
    e.add("kind", t.kind);
    e.add("rule", t.rule);
    auto records = Value::array();
    for (const auto& [id, words] : t.records)
    {
        auto r = Value::object();
        r.add("id", id);
        r.add("text", words);
        records.push(r);
    }
    e.add("records", records);
    e.add("tries", 3 - t.tries);
    send(c, e);
}

void Game::finishArchiveTask(Connection* c, const ArchiveTask& t, std::int64_t pay, const std::string& how)
{
    const auto me = c->entityId;
    auto* e = world_.entity(me);
    if (!e)
        return;
    auto& society = world_.society();
    const auto community = world_.lawTown(t.cell);
    const auto treasury = society.treasuryOf(community);
    const auto paid = std::min(pay, society.spendable(treasury));
    if (paid > 0)
        society.shift(treasury, me, "", 0, paid, "archive work");
    ++archiveToday_[me + "|" + std::to_string(std::int64_t(std::floor(world_.calendarDays())))];
    auto& skill = e->gameSkills["scholarship"];
    skill = std::min(100., skill + 1);
    world_.recordEvent({"archive work", me, {}, t.cell, 0, 0, t.kind, 0, paid, community});
    record(Economy | Character, me);
    std::string line = how + " The keeper pays you " + std::to_string(paid) + "p.";
    // The next fragment: the archive's own town first, then the shared ones.
    const auto read = [&](const std::string& id) { return std::find(e->lore.begin(), e->lore.end(), id) != e->lore.end(); };
    const Fragment* next = nullptr;
    for (const auto* region : {&community, static_cast<const std::string*>(nullptr)})
    {
        for (const auto& f : lore().fragments)
            if ((region ? f.region == *region : f.region.empty()) && !read(f.id) && (f.after.empty() || read(f.after)))
            {
                next = &f;
                break;
            }
        if (next)
            break;
    }
    system(c, line);
    if (next && e->lore.size() < 500)
    {
        e->lore.push_back(next->id);
        auto ev = Value::object();
        ev.add("type", "lore");
        ev.add("topic", next->topic);
        ev.add("text", next->text);
        send(c, ev);
        system(c, "Among the records, something on " + next->topic + ": \"" + next->text + "\" (In your journal.)");
    }
    else
        system(c, "Nothing in these records you haven't read; the work still pays.");
}

bool Game::archiveCommand(Connection* c, const json::Value& j, Result& result)
{
    // {"verb": "ask", "kind": "sort" | "copy"} / "sort" {order: [ids]} / "leave".
    const auto me = c->entityId;
    auto* e = world_.entity(me);
    if (!e)
        return result = {false, "No such character.", {}}, true;
    const auto verb = j.string("verb", "ask");
    const auto task = archiveTasks_.find(me);
    if (verb == "leave")
    {
        if (task == archiveTasks_.end())
            return result = {false, "You have no archive work.", {}}, true;
        archiveTasks_.erase(task);
        return result = {true, "You hand the work back.", {}}, true;
    }
    if (verb == "sort")
    {
        if (task == archiveTasks_.end() || task->second.kind != "sort")
            return result = {false, "Ask the keeper for records to sort first.", {}}, true;
        auto& t = task->second;
        std::vector<std::string> order;
        for (const auto& id : j.array("order"))
            if (id.isString())
                order.push_back(id.asString());
        int right = 0;
        for (std::size_t i = 0; i < t.answer.size() && i < order.size(); ++i)
            right += order[i] == t.answer[i];
        if (right == int(t.answer.size()) && order.size() == t.answer.size())
        {
            const auto done = t;
            archiveTasks_.erase(task);
            finishArchiveTask(c, done, lore().pay, "The records are in order.");
            return result = {true, "", {}}, true;
        }
        if (++t.tries >= 3)
        {
            archiveTasks_.erase(task);
            return result = {false, std::to_string(right) + " of 6 in the right place. The keeper takes the records back with a sigh.", {}}, true;
        }
        sendArchiveTask(c, t);
        return result = {false, std::to_string(right) + " of 6 in the right place. Try again (" + std::to_string(3 - t.tries) + " left).", {}}, true;
    }
    if (verb != "ask")
        return result = {false, "That isn't archive work.", {}}, true;
    if (task != archiveTasks_.end())
    {
        if (task->second.kind == "sort")
            sendArchiveTask(c, task->second);
        return result = {false, "You have work in hand already.", {}}, true;
    }
    if (!archiveKeeperHere(e->cellId))
        return result = {false, "Ask a keeper of records, where they keep them.", {}}, true;
    const auto day = std::int64_t(std::floor(world_.calendarDays()));
    if (archiveToday_[me + "|" + std::to_string(day)] >= lore().perDay)
        return result = {false, "\"That's enough for one day. Come back tomorrow.\"", {}}, true;
    ArchiveTask t;
    t.cell = e->cellId;
    t.begun = world_.time();
    if (j.string("kind") == "copy")
    {
        t.kind = "copy";
        archiveTasks_[me] = t;
        return result = {true, "The keeper sets you a page to copy. Sit at a desk here and keep at it for five minutes.", {}}, true;
    }
    // Six records and a rule; the order kept here.
    t.kind = "sort";
    tavern::Rng rng{std::hash<std::string>{}(me + "|" + std::to_string(day) + "|" + std::to_string(world_.time())) | 1};
    const int rule = rng.below(3);
    std::vector<int> keys;
    while (keys.size() < 6)
    {
        const int k = rule == 0 ? 40 + rng.below(150) : rule == 1 ? 1 + rng.below(39) : rng.below(12);
        if (std::find(keys.begin(), keys.end(), k) == keys.end())
            keys.push_back(k);
    }
    std::vector<std::pair<int, std::string>> made;
    for (std::size_t i = 0; i < keys.size(); ++i)
    {
        const std::string kind = Kinds[rng.below(10)], place = Places[rng.below(10)];
        const auto k = keys[i];
        const std::string words = rule == 0 ? kind + " " + place + ", sealed in the " + ordinal(k) + " winter of the old count."
                                : rule == 1 ? "Roll " + roman(k) + ": " + lowered(kind.substr(0, 1)) + kind.substr(1) + " " + place + "."
                                            : kind + " " + place + ", taken in " + Times[k] + ".";
        made.push_back({k, words});
    }
    std::vector<std::size_t> shown(made.size());
    for (std::size_t i = 0; i < shown.size(); ++i)
        shown[i] = i;
    for (std::size_t i = shown.size(); i > 1; --i)
        std::swap(shown[i - 1], shown[std::size_t(rng.below(int(i)))]);
    for (std::size_t n = 0; n < shown.size(); ++n)
        t.records.push_back({"r" + std::to_string(n + 1), made[shown[n]].second});
    std::vector<std::size_t> byKey(made.size());
    for (std::size_t i = 0; i < byKey.size(); ++i)
        byKey[i] = i;
    std::sort(byKey.begin(), byKey.end(), [&](std::size_t a, std::size_t b) { return made[a].first < made[b].first; });
    for (const auto i : byKey)
        for (std::size_t n = 0; n < shown.size(); ++n)
            if (shown[n] == i)
                t.answer.push_back("r" + std::to_string(n + 1));
    t.rule = rule == 0 ? "Put them in order, oldest first." : rule == 1 ? "Put them in order by their roll numbers, lowest first."
                                                           : "Put them in order through the year, from early spring.";
    archiveTasks_[me] = t;
    sendArchiveTask(c, t);
    return result = {true, "The keeper hands you six records to sort.", {}}, true;
}

void Game::tendArchive()
{
    // Places: the cells wolves are in. Copying: at the desk, sitting, for the time it takes.
    for (auto* c : clients_)
        if (c)
            if (auto* e = world_.entity(c->entityId); e && !e->npc && e->places.size() < 5000)
                e->places.insert(e->cellId);
    for (auto it = archiveTasks_.begin(); it != archiveTasks_.end();)
    {
        auto& t = it->second;
        auto* c = clientOf(it->first);
        const auto* e = world_.entity(it->first);
        if (!c || !e || e->cellId != t.cell)
        {
            if (c)
                system(c, t.kind == "copy" ? "You leave the archive; the copy is unfinished." : "You leave the archive with the records unsorted; the keeper takes them back.");
            it = archiveTasks_.erase(it);
            continue;
        }
        if (t.kind == "copy")
        {
            if (e->posture != "sitting")
                t.begun = world_.time();            // (The clock runs only while sitting at it.)
            else if (world_.time() - t.begun >= lore().copySeconds)
            {
                const double skill = e->gameSkills.count("scholarship") ? e->gameSkills.at("scholarship") : 0.;
                const int errors = int(std::floor((1 - skill / 100) * 4 * (double(std::hash<std::string>{}(it->first + std::to_string(world_.time())) % 1000) / 1000)));
                const auto done = t;
                it = archiveTasks_.erase(it);
                finishArchiveTask(c, done, errors <= 1 ? lore().pay : lore().pay / 2,
                                  errors == 0 ? "A clean copy, not a blot on it." : "The copy is done, with " + std::to_string(errors) + (errors == 1 ? " slip." : " slips."));
                continue;
            }
        }
        ++it;
    }
}

void Game::sendJournal(Connection* c)
{
    const auto* e = world_.entity(c->entityId);
    if (!e)
        return;
    auto ev = Value::object();
    ev.add("type", "journal");
    auto lines = Value::array();
    for (const auto& id : e->lore)
        for (const auto& f : lore().fragments)
            if (f.id == id)
            {
                auto o = Value::object();
                o.add("topic", f.topic);
                o.add("town", f.region.empty() ? std::string("everywhere") : townWords(f.region));
                o.add("text", f.text);
                lines.push(o);
            }
    ev.add("lore", lines);
    auto beasts = Value::array();
    for (const auto& [species, seen] : e->bestiary)
    {
        const auto* s = wild::speciesById(species);
        auto o = Value::object();
        o.add("name", s ? names::capitalised(s->name) : species);
        o.add("first", injury::dateWords(seen.first));
        o.add("count", seen.second);
        beasts.push(o);
    }
    ev.add("bestiary", beasts);
    auto herbs = Value::array();
    for (const auto& [item, found] : e->herbarium)
    {
        auto o = Value::object();
        o.add("name", Society::itemName(item));
        o.add("first", injury::dateWords(found.first));
        o.add("where", found.second);
        herbs.push(o);
    }
    ev.add("herbarium", herbs);
    std::map<std::string, int> byTown;
    for (const auto& cell : e->places)
    {
        const auto town = world_.lawTown(cell);
        ++byTown[town.empty() ? std::string() : townWords(town)];
    }
    auto places = Value::array();
    for (const auto& [town, n] : byTown)
    {
        auto o = Value::object();
        o.add("town", town.empty() ? std::string("the wild country") : town);
        o.add("count", n);
        places.push(o);
    }
    ev.add("places", places);
    ev.add("scholarship", std::round(e->gameSkills.count("scholarship") ? e->gameSkills.at("scholarship") : 0.));
    send(c, ev);
}

std::string Game::scholarBriefing(const std::string& npc, const std::string& player)
{
    // A wolf who has read this town's old records (10 of them, or all there are) is known for it.
    const auto& residents = world_.society().state().residents;
    const auto life = residents.find(npc);
    const auto* p = world_.entity(player);
    if (life == residents.end() || !p || p->lore.empty())
        return {};
    const auto town = world_.lawTown(life->second.homeCell);
    int there = 0, read = 0;
    std::vector<std::string> topics;
    for (const auto& f : lore().fragments)
        if (f.region == town && !town.empty())
        {
            ++there;
            if (std::find(p->lore.begin(), p->lore.end(), f.id) != p->lore.end())
            {
                ++read;
                if (std::find(topics.begin(), topics.end(), f.topic) == topics.end() && topics.size() < 3)
                    topics.push_back(f.topic);
            }
        }
    if (there == 0 || read < std::min(lore().scholarAfter, there))
        return {};
    std::string list;
    for (std::size_t i = 0; i < topics.size(); ++i)
        list += (i ? (i + 1 == topics.size() ? " and " : ", ") : "") + topics[i];
    return " This wolf has read the old records of " + townWords(town) + " (" + list + "); you might ask them about the town's history.";
}
} // namespace ratw::game
