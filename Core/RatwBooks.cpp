#include "RatwBooks.h"
#include "RatwPeople.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace ratw::books
{
namespace
{
std::filesystem::path socialFile()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
        return fs::path(dir) / "Social" / "social.json";
    for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
    {
        if (fs::exists(at / "Data" / "Social" / "social.json", ec))
            return at / "Data" / "Social" / "social.json";
        if (at == at.parent_path())
            break;
    }
#ifdef RATW_SOURCE_DIR
    return fs::path(RATW_SOURCE_DIR) / "Data" / "Social" / "social.json";
#else
    return fs::path("Data") / "Social" / "social.json";
#endif
}

Rules build()
{
    Rules r;
    std::ifstream in(socialFile(), std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    json::Value doc;
    std::string error;
    if (!in || !json::parse(text.str(), doc, error))
        return r;
    const auto& b = doc.object("books");
    for (auto [key, field] : std::initializer_list<std::pair<const char*, int*>>{
             {"title", &r.title}, {"chapterTitle", &r.chapterTitle}, {"summary", &r.summary}, {"bookSummary", &r.bookSummary},
             {"flavour", &r.flavour}, {"linkDays", &r.linkDays}, {"activeDays", &r.activeDays}, {"quietDays", &r.quietDays},
             {"modelADay", &r.modelADay}, {"openBooks", &r.openBooks}, {"chapters", &r.chapters}, {"wolves", &r.wolves}})
        *field = int(b.number(key, *field));
    return r;
}

json::Value strings(const std::vector<std::string>& list)
{
    auto a = json::Value::array();
    for (const auto& s : list)
        a.push(s);
    return a;
}
} // namespace

const Rules& rules()
{
    static const Rules r = build();
    return r;
}

bool hasWolf(const Book& b, const std::string& character)
{
    return std::find(b.wolves.begin(), b.wolves.end(), character) != b.wolves.end();
}

bool hasPrivate(const Book& b)
{
    return std::any_of(b.chapters.begin(), b.chapters.end(), [](const Chapter& c) { return c.privateScene; });
}

std::vector<std::string> recentlyActive(const Book& b, double now)
{
    std::set<std::string> active;
    for (const auto& c : b.chapters)
        if (now - std::max(c.ended, c.at) <= rules().activeDays * 86400.0)
            for (const auto& w : c.wolves)
                if (hasWolf(b, w) && !b.hidden.count(w))
                    active.insert(w);
    if (active.empty())
        active.insert(b.keeper);                    // (No one recent: the keeper's word is the book's.)
    return {active.begin(), active.end()};
}

bool finishes(const Book& b, double now)
{
    if (b.state != "finishing")
        return false;
    const auto active = recentlyActive(b, now);
    int yes = 0;
    for (const auto& w : active)
        yes += b.agreed.count(w);
    if (yes * 2 > int(active.size()))
        return true;
    return b.objected.empty() && now - b.finishProposed >= rules().quietDays * 86400.0;
}

std::string inverse(const std::string& kind)
{
    return kind == "sequel" ? "prequel" : kind == "prequel" ? "sequel" : kind;
}

bool validLink(const std::string& kind)
{
    return kind == "related" || kind == "sequel" || kind == "prequel";
}

bool validSharing(const std::string& sharing)
{
    return sharing == "members" || sharing == "friends" || sharing == "circle" || sharing == "chapter" || sharing == "everyone";
}

json::Value save(const Book& b)
{
    auto o = json::Value::object();
    o.add("id", b.id);
    o.add("title", b.title);
    o.add("keeper", b.keeper);
    o.add("summary", b.summary);
    o.add("summaryBy", b.summaryBy);
    o.add("summaryModel", b.summaryModel);
    o.add("flavour", b.flavour);
    o.add("sharing", b.sharing);
    o.add("circle", b.circle);
    o.add("chapter", b.chapter);
    o.add("storyline", b.storyline);
    o.add("wolves", strings(b.wolves));
    o.add("hidden", strings({b.hidden.begin(), b.hidden.end()}));
    auto chapters = json::Value::array();
    for (const auto& c : b.chapters)
    {
        auto j = json::Value::object();
        j.add("id", c.id);
        j.add("session", c.session);
        j.add("title", c.title);
        j.add("summary", c.summary);
        j.add("summaryBy", c.summaryBy);
        j.add("model", c.model);
        j.add("private", c.privateScene);
        j.add("place", c.place);
        j.add("linkedBy", c.linkedBy);
        j.add("at", c.at);
        j.add("ended", c.ended);
        j.add("wolves", strings(c.wolves));
        chapters.push(j);
    }
    o.add("chapters", chapters);
    o.add("state", b.state);
    o.add("created", b.created);
    o.add("last", b.last);
    o.add("finishProposed", b.finishProposed);
    o.add("finished", b.finished);
    o.add("agreed", strings({b.agreed.begin(), b.agreed.end()}));
    o.add("objected", strings({b.objected.begin(), b.objected.end()}));
    o.add("story", b.story);
    auto links = json::Value::array();
    for (const auto& l : b.links)
    {
        auto j = json::Value::object();
        j.add("book", l.book);
        j.add("kind", l.kind);
        links.push(j);
    }
    o.add("links", links);
    o.add("nextScene", strings({b.nextScene.begin(), b.nextScene.end()}));
    return o;
}

Book load(const json::Value& o)
{
    const auto& r = rules();
    const auto text = [](const std::string& t, int most, bool lines = false) { return people::clean(t, std::size_t(most), lines); };
    const auto list = [](const json::Value& a, std::size_t most) {
        std::vector<std::string> out;
        for (const auto& v : a.items())
            if (v.isString() && out.size() < most)
                out.push_back(v.asString().substr(0, 80));
        return out;
    };
    Book b;
    b.id = o.string("id").substr(0, 80);
    b.title = text(o.string("title"), r.title);
    b.keeper = o.string("keeper").substr(0, 80);
    b.summary = text(o.string("summary"), r.bookSummary, true);
    b.summaryBy = o.string("summaryBy").substr(0, 80);
    b.summaryModel = o.boolean("summaryModel");
    b.flavour = text(o.string("flavour"), r.flavour, true);
    b.sharing = validSharing(o.string("sharing")) ? o.string("sharing") : "members";
    b.circle = o.string("circle").substr(0, 80);
    b.chapter = o.string("chapter").substr(0, 80);
    b.storyline = text(o.string("storyline"), 120);
    b.wolves = list(o.array("wolves"), std::size_t(r.wolves));
    for (const auto& h : list(o.array("hidden"), std::size_t(r.wolves)))
        b.hidden.insert(h);
    for (const auto& j : o.array("chapters"))
    {
        if (b.chapters.size() >= std::size_t(r.chapters))
            break;
        Chapter c;
        c.id = j.string("id").substr(0, 80);
        c.session = j.string("session").substr(0, 120);
        c.title = text(j.string("title"), r.chapterTitle);
        c.summary = text(j.string("summary"), r.summary, true);
        c.summaryBy = j.string("summaryBy").substr(0, 80);
        c.model = j.boolean("model");
        c.privateScene = j.boolean("private");
        c.place = text(j.string("place"), 120);
        c.linkedBy = j.string("linkedBy").substr(0, 80);
        c.at = j.number("at");
        c.ended = j.number("ended");
        c.wolves = list(j.array("wolves"), 64);
        b.chapters.push_back(c);
    }
    const auto state = o.string("state");
    b.state = state == "finishing" || state == "finished" ? state : "open";
    b.created = o.number("created");
    b.last = o.number("last");
    b.finishProposed = o.number("finishProposed");
    b.finished = o.number("finished");
    for (const auto& a : list(o.array("agreed"), std::size_t(r.wolves)))
        b.agreed.insert(a);
    for (const auto& a : list(o.array("objected"), std::size_t(r.wolves)))
        b.objected.insert(a);
    b.story = o.string("story").substr(0, 80);
    for (const auto& j : o.array("links"))
        if (validLink(j.string("kind")) && b.links.size() < 32)
            b.links.push_back({j.string("book").substr(0, 80), j.string("kind")});
    for (const auto& n : list(o.array("nextScene"), std::size_t(r.wolves)))
        b.nextScene.insert(n);
    return b;
}
} // namespace ratw::books
