// The Market Board (Docs/Design/79-a-staged-economy.md, sections 5 and 7; Phase 1): its data, the pure arithmetic of
// pressure, the ladder and the clamp, the links through recipes, and Society's staged price path (quote, trade, the
// dawn's turn and the morning restock) and the world's markets found at load. The only prices there are (doc 79,
// Phase 3: the conserving economy's shopFactor went with the city's loop).
#include "RatwMarket.h"

#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwSociety.h"
#include "RatwWild.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>

namespace ratw::market
{
namespace
{
constexpr const char* DefaultGroups[][2] = {{"staple", "2"}, {"food", "1"}, {"meat", "1"}, {"herbs", ".8"}, {"materials", "1"},
                                            {"fuel", "1"}, {"metal", "1"}, {"cloth", ".5"}, {"tools", ".4"}, {"luxury", ".3"},
                                            {"services", "1"}};

bool readFile(const std::filesystem::path& path, json::Value& out, std::string& error)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        error = "cannot read " + path.string();
        return false;
    }
    std::stringstream text;
    text << in.rdbuf();
    return json::parse(text.str(), out, error);
}

double clampNumber(double x, double low, double high, double fallback)
{
    return finite(x) ? std::clamp(x, low, high) : fallback;
}

// The staged events and causes a world has without market.json's (doc 79, 8.4, 8.5): a few any market with food can
// see, and a cause each way.
void defaultEvents(Data& d)
{
    Data::EventKind mill;
    mill.id = "mill_stopped";
    mill.words = "the mill wheel stopped";
    mill.notice = "The mill wheel is stopped for mending: {goods} dear in {market} till it turns again.";
    mill.goods = {{"flour", .15}, {"bread", .1}, {"fine_bread", .1}};
    mill.days = 2;
    mill.times = -3;
    Data::EventKind drovers;
    drovers.id = "drovers_in";
    drovers.words = "the drovers in with their herds";
    drovers.notice = "The drovers are in at {market} with their herds: {goods} cheap a day or two.";
    drovers.goods = {{"raw_meat", -.15}, {"mutton", -.15}, {"hide", -.1}};
    drovers.days = 2;
    drovers.times = 3;
    drovers.crowd = true;
    d.events = {mill, drovers};
    d.causesDown = {{"war_levy", "the Crown's war levy", "By order of the Crown: a levy for the war, on every household and trade.",
                     "Hear ye! The Crown levies for the war!"}};
    d.causesUp = {{"silver_strike", "the silver strike in the hills", "Silver struck in the hills: coin and work for all who want it.",
                   "Hear ye! Silver in the hills!"}};
}

Data load()
{
    Data d;
    for (const auto& [id, base] : DefaultGroups)
        d.groups[id] = std::stod(base);
    d.times["staple"] = {1.08, 1.04, 1, 1, 1};
    d.times["luxury"] = {1, 1, 1, 1.04, 1.08};
    d.district["heights"]["luxury"] = 1.08;
    d.district["sump"]["all"] = .95;
    d.buys = {{"smithy", {"iron_ore"}}, {"ironworks", {"iron_ore"}}, {"foundry", {"iron_ore"}}};
    defaultEvents(d);
    const auto file = items::dataDirectory().parent_path() / "Economy" / "market.json";
    json::Value v;
    std::string error;
    if (!std::filesystem::exists(file) || !readFile(file, v, error) || !v.isObject())
    {
        std::cerr << "RATW_MARKET market.json not read (" << (error.empty() ? file.string() : error) << "): the defaults\n";
        return d;
    }
    d.loaded = true;
    // Every value lenient: a wrong one keeps its default (doc 79, 14: never refused over economy data).
    if (const auto& groups = v.object("groups"); groups.isObject())
        for (const auto& [id, base] : groups.fields())
            if (base.isNumber() && finite(base.asNumber()) && !id.empty() && id.size() <= 32)
                d.groups[id] = std::clamp(base.asNumber(), .01, 100.);
    if (const auto& goods = v.object("goods"); goods.isObject())
        for (const auto& [group, list] : goods.fields())
            if (d.groups.count(group))
                for (const auto& g : list.items())
                    if (g.isString() && !g.asString().empty())
                        d.goods[g.asString()] = group;
    if (const auto& categories = v.object("categories"); categories.isObject())
        for (const auto& [category, group] : categories.fields())
            if (group.isString() && d.groups.count(group.asString()))
                d.categories[category] = group.asString();
    if (const auto fallback = v.string("fallback"); d.groups.count(fallback))
        d.fallback = fallback;
    if (const auto& district = v.object("district"); district.isObject())
    {
        d.district.clear();
        for (const auto& [quarter, by] : district.fields())
            for (const auto& [group, f] : by.fields())
                if (f.isNumber() && (group == "all" || d.groups.count(group)))
                    d.district[quarter][group] = clampNumber(f.asNumber(), FactorLow, FactorHigh, 1);
    }
    if (const auto& times = v.object("times"); times.isObject())
        for (const auto& [group, list] : times.fields())
            if (d.groups.count(group) && list.size() == 5)
            {
                std::array<double, 5> row{};
                for (std::size_t i = 0; i < 5; ++i)
                    row[i] = clampNumber(list.items()[i].asNumber(1), .9, 1.1, 1);
                d.times[group] = row;
            }
    if (const auto& cuts = v.array("timesCuts"); cuts.size() == 4)
    {
        std::array<int, 4> c{};
        for (std::size_t i = 0; i < 4; ++i)
            c[i] = int(clampNumber(cuts[i].asNumber(0), -100, 100, 0));
        if (std::is_sorted(c.begin(), c.end()))
            d.timesCuts = c;
    }
    d.seasonSpanMost = clampNumber(v.number("seasonSpanMost", 1.06), 1, 1.55, 1.06);
    const auto& region = v.object("region");
    d.regionMade = clampNumber(region.number("made", .95), .85, 1, .95);
    d.regionNear = clampNumber(region.number("near", 1), .9, 1.1, 1);
    d.regionFar = clampNumber(region.number("far", 1.08), 1, 1.15, 1.08);
    d.budget = clampNumber(v.number("budget", 40), 0, 100000, 40);
    d.routes = int(clampNumber(v.number("routes", 3), 0, 12, 3));
    if (const auto& buys = v.object("buys"); buys.isObject())
    {
        d.buys.clear();
        for (const auto& [business, list] : buys.fields())
            for (const auto& g : list.items())
                if (g.isString() && items::good(g.asString()))
                    d.buys[business].push_back(g.asString());
    }
    if (const auto& shelves = v.object("shelves"); shelves.isObject())
        for (const auto& [good, n] : shelves.fields())
            if (n.isNumber())
                d.shelves[good] = int(clampNumber(n.asNumber(), 0, 1000, 4));
    // The rules' defaults (doc 79, 12: the rules of items::Rules, read over its own; a name it doesn't know is skipped).
    for (const auto& [name, n] : v.object("rules").fields())
        if (const auto* info = items::ruleInfo(name); info && n.isNumber())
            d.rules[name] = std::clamp(int(std::lround(n.asNumber())), info->low, info->high);
    // The port's goods and the ships' names (8.3; Phase 5): the `wharf` block's `imports` less its `never`, its `takes`,
    // and `ships` (names of letters and spaces only, each once; none good keeps the defaults).
    {
        const auto& wharf = v.object("wharf");
        std::set<std::string> never;
        for (const auto& g : wharf.array("never"))
            if (g.isString())
                never.insert(g.asString());
        d.shipNever = never;
        const auto goods = [&](const char* key, std::vector<std::string>& into, bool imports) {
            for (const auto& g : wharf.array(key))
                if (g.isString() && items::good(g.asString()) && !(imports && never.count(g.asString())) &&
                    std::find(into.begin(), into.end(), g.asString()) == into.end() && into.size() < 64)
                    into.push_back(g.asString());
        };
        goods("imports", d.shipBrings, true);
        goods("takes", d.shipTakes, false);
        if (auto names = shipNamesOf(wharf.array("ships")); !names.empty())
            d.shipNames = std::move(names);
        // The words that make a market a port, and the cargo's own words (the Phase 5 repair): plain words only.
        const auto words = [](const std::string& text) {
            return !text.empty() && text.size() <= 40 && !forbiddenWord(text) && std::all_of(text.begin(), text.end(), [](unsigned char c) {
                return std::isalpha(c) || c == ' ' || c == '\'' || c == '-';
            });
        };
        std::vector<std::string> posts;
        for (const auto& p : wharf.array("posts"))
        {
            auto word = p.asString({});
            for (auto& c : word)
                c = char(std::tolower(static_cast<unsigned char>(c)));
            if (words(word) && word.size() >= 4 && std::find(posts.begin(), posts.end(), word) == posts.end() && posts.size() < 16)
                posts.push_back(word);
        }
        if (!posts.empty())
            d.wharfPosts = std::move(posts);
        if (const auto& cargo = wharf.object("cargoWords"); cargo.isObject())
            for (const auto& [good, text] : cargo.fields())
                if (text.isString() && items::good(good) && words(text.asString()) && d.cargoWords.size() < 128)
                    d.cargoWords[good] = text.asString();
    }
    // Street life (Phase 6): the luxuries (known goods, each once) and the services (a catalogue item, a business, every
    // 1 to 3650 days; a bad one skipped).
    for (const auto& g : v.object("luxuries").array("goods"))
        if (const auto id = g.asString({}); items::good(id) && std::find(d.luxuries.begin(), d.luxuries.end(), id) == d.luxuries.end() &&
                                            d.luxuries.size() < 64)
            d.luxuries.push_back(id);
    for (const auto& s : v.object("services").array("list"))
    {
        Data::Service service;
        service.id = s.string("id");
        service.business = s.string("business");
        service.every = int(clampNumber(s["every"].asNumber(30), 1, 3650, 30));
        service.patrons = s.string("for") == "patrons";
        service.poor = s.boolean("poor", false) && !service.patrons;
        if (!service.id.empty() && service.id.size() <= 80 && !service.business.empty() && service.business.size() <= 80 &&
            d.services.size() < 128)
            d.services.push_back(std::move(service));
    }
    // Staged events and the slider's causes (8.4, 8.5; Phase 7): the host's lists as the loader takes them; an empty or
    // broken list keeps the defaults.
    if (const auto& events = v.object("events"); events.isObject())
    {
        if (auto kinds = eventKindsOf(events.array("list")); !kinds.empty())
            d.events = std::move(kinds);
        d.eventsPerWeek = clampNumber(events["perWeek"].asNumber(d.eventsPerWeek), 0, 6, 2);
        const auto& causes = events.object("causes");
        if (auto down = causesOf(causes.array("down")); !down.empty())
            d.causesDown = std::move(down);
        if (auto up = causesOf(causes.array("up")); !up.empty())
            d.causesUp = std::move(up);
        const auto words = [&](const char* key, std::string& into) {
            if (const auto text = causes.string(key);
                plainWords(text, 200) && (text.find("{cause}") != std::string::npos || text.find("{Cause}") != std::string::npos))
                into = text;
        };
        words("noticeDown", d.causeNoticeDown);
        words("noticeUp", d.causeNoticeUp);
        words("cryDown", d.causeCryDown);
        words("cryUp", d.causeCryUp);
    }
    // The season groups (7.1, moved from city_loop.json): a good in the first group naming it; a
    // group without an id or goods dropped, its numbers clamped (never a refusal).
    for (const auto& g : v.array("seasons"))
    {
        Data::Season season;
        season.id = g.string("id");
        season.name = g.string("name", season.id);
        if (season.id.empty() || season.id.size() > 40 ||
            std::any_of(d.seasons.begin(), d.seasons.end(), [&](const Data::Season& x) { return x.id == season.id; }))
            continue;
        for (const auto& item : g.array("goods"))
            if (const auto id = item.asString({}); !id.empty() && items::good(id) && !d.seasonOf.count(id))
                season.goods.push_back(id);
        const auto four = [&](const char* key, std::array<int, 4>& into, int low, int high) {
            if (const auto& values = g.array(key); values.size() == 4)
                for (std::size_t i = 0; i < 4; ++i)
                    into[i] = int(std::lround(clampNumber(values[i].asNumber(100), low, high, 100)));
        };
        four("price", season.price, 10, 1000);
        four("shelf", season.shelf, 0, 1000);
        if (season.goods.empty())
            continue;
        for (const auto& id : season.goods)
            d.seasonOf.emplace(id, d.seasons.size());
        d.seasons.push_back(std::move(season));
    }
    return d;
}
} // namespace

bool finite(double x)
{
    return std::isfinite(x);
}

const Data& data()
{
    static const Data d = load();
    return d;
}

std::vector<std::string> shipNamesOf(const std::vector<json::Value>& list)
{
    std::vector<std::string> names;
    for (const auto& n : list)
    {
        const auto name = n.asString({});
        // (And never the forbidden word: a host's list is checked as the game's own is.)
        if (shipNameFits(name) && std::find(names.begin(), names.end(), name) == names.end() && names.size() < 64)
            names.push_back(name);
    }
    return names;
}

bool shipNameFits(const std::string& name)
{
    return !name.empty() && name.size() <= 40 && !forbiddenInName(name) && std::all_of(name.begin(), name.end(), [](unsigned char c) {
        return std::isalpha(c) || c == ' ' || c == '\'' || c == '-';
    });
}

bool plainWords(const std::string& text, std::size_t most)
{
    return text.size() <= most && !forbiddenWord(text) &&
           std::none_of(text.begin(), text.end(), [](unsigned char c) { return c < 32 || c == 127; });
}

const Data::EventKind* Data::eventKind(const std::string& id) const
{
    for (const auto& k : events)
        if (k.id == id)
            return &k;
    return nullptr;
}

std::vector<Data::EventKind> eventKindsOf(const std::vector<json::Value>& list)
{
    std::vector<Data::EventKind> kinds;
    const auto idFits = [](const std::string& id) {
        return !id.empty() && id.size() <= 32 && !forbiddenInName(id) &&
               std::all_of(id.begin(), id.end(), [](unsigned char c) { return std::islower(c) || std::isdigit(c) || c == '_'; });
    };
    for (const auto& j : list)
    {
        if (!j.isObject() || kinds.size() >= 64)
            continue;
        Data::EventKind k;
        k.id = j.string("id");
        k.words = j.string("words");
        k.notice = j.string("notice");
        k.cry = j.string("cry");
        k.sign = j.string("sign", "the crier");
        if (!idFits(k.id) || std::any_of(kinds.begin(), kinds.end(), [&](const Data::EventKind& o) { return o.id == k.id; }) ||
            k.words.empty() || !plainWords(k.words, 80) || !plainWords(k.notice, 280) || !plainWords(k.cry, 200) ||
            k.sign.empty() || !plainWords(k.sign, 40))
            continue;
        if (const auto& goods = j.object("goods"); goods.isObject())
            for (const auto& [g, P] : goods.fields())
                if (items::good(g) && P.isNumber() && finite(P.asNumber()) && std::abs(P.asNumber()) >= .01 && k.goods.size() < 6)
                    k.goods.push_back({g, std::clamp(P.asNumber(), -EventTargetMost, EventTargetMost)});
        k.yields = clampNumber(j["yields"].asNumber(0), -EventTargetMost, EventTargetMost, 0);
        k.brings = clampNumber(j["brings"].asNumber(0), -EventTargetMost, EventTargetMost, 0);
        k.days = int(clampNumber(j["days"].asNumber(2), 1, 7, 2));
        k.times = int(clampNumber(j["times"].asNumber(0), -EventTimesMost, EventTimesMost, 0));
        k.ahead = int(clampNumber(j["ahead"].asNumber(0), 0, 6, 0));
        k.hour = clampNumber(j["hour"].asNumber(9), 0, 23.5, 9);
        k.weight = clampNumber(j["weight"].asNumber(1), 0, 10, 1);
        k.needs = j.string("needs");
        k.ship = j.string("ship");
        k.refill = j.boolean("refill", false);
        k.crowd = j.boolean("crowd", false);
        k.holdsCart = j.boolean("holdsCart", false);
        if (k.needs != "" && k.needs != "city" && k.needs != "port" && k.needs != "cart" && k.needs != "festival")
            continue;
        if (k.ship != "" && k.ship != "docks" && k.ship != "lost")
            continue;
        if (!k.ship.empty())
            k.needs = "port";
        if (k.holdsCart || std::abs(k.yields) > 0)
            k.needs = "cart";
        if (k.goods.empty() && std::abs(k.yields) < .01 && std::abs(k.brings) < .01)
            continue;                                // (Nothing it would press: no event.)
        kinds.push_back(std::move(k));
    }
    return kinds;
}

std::vector<Data::Cause> causesOf(const std::vector<json::Value>& list)
{
    std::vector<Data::Cause> causes;
    for (const auto& j : list)
    {
        if (!j.isObject() || causes.size() >= 32)
            continue;
        Data::Cause c{j.string("id"), j.string("words"), j.string("notice"), j.string("cry")};
        if (c.id.empty() || c.id.size() > 32 || forbiddenInName(c.id) || c.words.empty() || !plainWords(c.words, 80) ||
            !plainWords(c.notice, 280) || !plainWords(c.cry, 200) ||
            std::any_of(causes.begin(), causes.end(), [&](const Data::Cause& o) { return o.id == c.id; }))
            continue;
        causes.push_back(std::move(c));
    }
    return causes;
}

bool forbiddenInName(const std::string& text)
{
    const auto is = [&](std::size_t i, char c) { return std::tolower(static_cast<unsigned char>(text[i])) == c; };
    for (std::size_t i = 0; i + 2 < text.size(); ++i)
        if (is(i, 'd') && is(i + 1, 'o') && is(i + 2, 'g'))
            return true;
    return false;
}

bool forbiddenWord(const std::string& text)
{
    const auto letter = [&](std::size_t i) { return i < text.size() && std::isalpha(static_cast<unsigned char>(text[i])) != 0; };
    const auto is = [&](std::size_t i, char c) { return i < text.size() && std::tolower(static_cast<unsigned char>(text[i])) == c; };
    for (std::size_t i = 0; i + 2 < text.size(); ++i)
        if (is(i, 'd') && is(i + 1, 'o') && is(i + 2, 'g') && (i == 0 || !letter(i - 1)))
            if (const auto end = is(i + 3, 's') ? i + 4 : i + 3; !letter(end))
                return true;
    return false;
}

std::string groupOf(const std::string& base, std::string* how)
{
    const auto& d = data();
    if (const auto g = d.goods.find(base); g != d.goods.end())
    {
        if (how)
            *how = "good";
        return g->second;
    }
    if (const auto* item = items::good(base))
        if (const auto c = d.categories.find(item->category); c != d.categories.end())
        {
            if (how)
                *how = "category";
            return c->second;
        }
    // (The economy's own three: a meal is a staple, herbs are herbs, the sword a tool.)
    if (base == "meal" || base == "herbs" || base == "sword")
    {
        if (how)
            *how = "good";
        return base == "meal" ? "staple" : base == "herbs" ? "herbs" : "tools";
    }
    if (how)
        *how = "fallback";
    return d.fallback;
}

double depthBase(const std::string& base)
{
    const auto& groups = data().groups;
    const auto g = groups.find(groupOf(base));
    return g == groups.end() ? 1 : g->second;
}

int timesOf(double score)
{
    const auto& cuts = data().timesCuts;
    int at = 0;
    while (at < 4 && score >= cuts[std::size_t(at)])
        ++at;
    return at;
}

const char* timesName(int times)
{
    static const char* const Names[] = {"hard", "lean", "ordinary", "good", "boom"};
    return Names[std::clamp(times, 0, 4)];
}

double seasonal(const std::array<int, 4>& by, double yearDay)
{
    // (Each season's value at its middle day, a straight line between: the old city loop's blend, kept.)
    static constexpr double Middle[4] = {46, 92 + 46, 184 + 45.5, 275 + 45};
    if (!finite(yearDay) || yearDay < 0)
        return 1;
    const double d = std::fmod(yearDay, double(calendar::DaysPerYear));
    int a = 3;
    for (int i = 0; i < 4; ++i)
        if (d >= Middle[i])
            a = i;
    const int b = (a + 1) % 4;
    const double from = Middle[a], to = b == 0 ? Middle[0] + calendar::DaysPerYear : Middle[b];
    const double at = d < from ? d + calendar::DaysPerYear : d;
    const double t = std::clamp((at - from) / (to - from), 0., 1.);
    return (by[std::size_t(a)] + (by[std::size_t(b)] - by[std::size_t(a)]) * t) / 100.;
}

double seasonFactor(const std::array<int, 4>& by, double yearDay)
{
    const double s = seasonal(by, yearDay);
    const auto [lo, hi] = std::minmax_element(by.begin(), by.end());
    if (*lo <= 0 || !finite(s) || s <= 0)
        return 1;
    const double span = double(*hi) / double(*lo), most = data().seasonSpanMost;
    if (span <= most)
        return s;
    return std::exp(std::log(s) * std::log(most) / std::log(span));
}

const Data::Season* seasonOf(const std::string& base)
{
    const auto& d = data();
    const auto found = d.seasonOf.find(base);
    return found == d.seasonOf.end() ? nullptr : &d.seasons[found->second];
}

double halfLife(const std::string& base, const Rules& rules)
{
    const auto* item = items::good(base);
    const bool food = base == "meal" || (item && item->nourish > 0);
    const double keeps = base == "meal" ? 3 : item ? item->keeps : 0;
    const double days = food && keeps > 0 && keeps <= 7 ? rules.halfLifePerishable : food ? rules.halfLifeKeeping : rules.halfLifeDurable;
    return std::clamp(finite(days) ? days : 8., .5, 30.);
}

double decayed(double value, double since, double now, double halfLife)
{
    if (!finite(value) || value == 0)
        return 0;
    if (!finite(since) || !finite(now) || now <= since || !finite(halfLife) || halfLife <= 0)
        return value;
    return value * std::pow(.5, (now - since) / halfLife);
}

double factor(double base, double P)
{
    const double f = (finite(base) ? base : 1) * std::exp(finite(P) ? std::clamp(P, -PressureMost, PressureMost) : 0);
    return finite(f) ? std::clamp(f, FactorLow, FactorHigh) : 1;
}

void madeBy(const std::string& business, std::set<std::string>& made)
{
    // What a shop's being in a market makes the market's own (7.1's region): its crafts' outputs and what it supplies
    // (crafts.json `supplies`). A country workshop (workshops.json) is a business of businesses.json, so its are its
    // crafts'.
    for (const auto* craft : items::craftsFor(business))
        for (const auto& [good, _] : craft->out)
            made.insert(items::baseOf(good));
    for (const auto& good : items::suppliesFor(business))
        made.insert(items::baseOf(good));
}

void craftedBy(const std::string& business, std::set<std::string>& crafted)
{
    for (const auto* craft : items::craftsFor(business))
        for (const auto& [good, _] : craft->out)
            crafted.insert(items::baseOf(good));
}

std::int64_t boughtLot(double sum, double each, int quantity, bool stall)
{
    // A bought lot (7.1): the ladder's sum rounded, at least 1p; at a stall × 0.9 and rounded down. Never under the
    // clamp floor's stall price, ceil(catalogue × 0.80 × 0.9 × q) (I3, 7.2): rounding small lots of cheap goods could
    // price them under the clamp (a 2p good singly at a stall at 1p, three 1p goods at a shop at 2p), and 7.2's
    // arithmetic (0.55 × 1.28 < 0.9 × 0.80) then no longer held: pieces bought in small lots could be sold on as one lot,
    // there or elsewhere, now or later, for more. With the floor, any q pieces bought in any lots cost at least
    // 0.72 × catalogue × q and any lot of q sells for at most floor(0.704 × catalogue × q): at least a penny lost. A
    // stall's lot is never dearer than a shop's at the same sum.
    const double q = double(std::max(1, quantity));
    const auto low = std::int64_t(std::ceil(FactorLow * StallShare * each * q - 1e-9));
    const auto lot = stall ? std::int64_t(std::floor(StallShare * sum + 1e-9)) : std::int64_t(std::llround(sum));
    return std::max<std::int64_t>({1, lot, low});
}

std::array<std::int64_t, 100> leastSplits(double each, double factor, int upTo)
{
    std::array<std::int64_t, 100> least{}, lot{};
    const int q = std::clamp(upTo, 1, 99);
    for (int k = 1; k <= q; ++k)
        lot[std::size_t(k)] = boughtLot(each * factor * double(k), each, k, true);
    for (int n = 1; n <= q; ++n)
    {
        auto best = lot[std::size_t(n)];
        for (int k = 1; k + k <= n; ++k)            // (A split and its mirror cost the same: half the pairs.)
            best = std::min(best, least[std::size_t(k)] + least[std::size_t(n - k)]);
        least[std::size_t(n)] = best;
    }
    return least;
}

// ------------------------------------------------------------------------------------------------- The Board saved

json::Value encode(const Board& b)
{
    using json::Value;
    auto o = Value::object();
    o.add("slider", b.slider);
    o.add("sliderTarget", b.sliderTarget);
    if (b.sliderDay >= 0)
        o.add("sliderDay", b.sliderDay);
    o.add("day", b.day);
    auto cells = Value::object();
    for (const auto& [m, goods] : b.cell)
    {
        auto g = Value::object();
        for (const auto& [good, c] : goods)
        {
            auto j = Value::object();
            j.add("P", c.P);
            j.add("touched", c.touched);
            j.add("cause", c.cause);
            g.add(good, j);
        }
        cells.add(m, g);
    }
    o.add("cell", cells);
    auto budgets = Value::object();
    for (const auto& [till, n] : b.budgetLeft)
        budgets.add(till, n);
    o.add("budgetLeft", budgets);
    auto taken = Value::object();
    for (const auto& [till, goods] : b.takenToday)
    {
        auto g = Value::object();
        for (const auto& [good, n] : goods)
            g.add(good, n);
        taken.add(till, g);
    }
    o.add("takenToday", taken);
    auto pushed = Value::object();
    for (const auto& [m, holders] : b.pushed)
    {
        auto hs = Value::object();
        for (const auto& [holder, goods] : holders)
        {
            auto g = Value::object();
            for (const auto& [good, p] : goods)
            {
                auto j = Value::object();
                j.add("amount", p.amount);
                j.add("touched", p.touched);
                g.add(good, j);
            }
            hs.add(holder, g);
        }
        pushed.add(m, hs);
    }
    o.add("pushed", pushed);
    auto net = Value::object();
    for (const auto& [m, goods] : b.netToday)
    {
        auto g = Value::object();
        for (const auto& [good, holders] : goods)
        {
            auto hs = Value::object();
            for (const auto& [holder, n] : holders)
                hs.add(holder, n);
            g.add(good, hs);
        }
        net.add(m, g);
    }
    o.add("netToday", net);
    auto sold = Value::object();
    for (const auto& [holder, n] : b.soldToday)
        sold.add(holder, n);
    o.add("soldToday", sold);
    o.add("week", b.week);
    auto discount = Value::object();
    for (const auto& [chapter, n] : b.discountWeek)
        discount.add(chapter, n);
    o.add("discountWeek", discount);
    auto restocked = Value::object();
    for (const auto& [till, day] : b.restocked)
        restocked.add(till, day);
    o.add("restocked", restocked);
    auto settingOut = Value::object();
    for (const auto& [till, until] : b.settingOut)
        settingOut.add(till, until);
    o.add("settingOut", settingOut);
    auto recent = Value::array();
    for (const auto& r : b.recent)
    {
        auto j = Value::object();
        j.add("day", r.day);
        j.add("delta", r.delta);
        j.add("market", r.market);
        j.add("good", r.good);
        j.add("cause", r.cause);
        recent.push(j);
    }
    o.add("recent", recent);
    // Staged trade (Phase 4): written only when there is any, so a Board from before it reads back the same.
    if (!b.committed.empty())
    {
        auto list = Value::array();
        for (const auto& d : b.committed)
        {
            auto j = Value::object();
            j.add("id", d.id);
            j.add("kind", d.kind);
            j.add("from", d.from);
            j.add("to", d.to);
            j.add("driver", d.driver);
            j.add("day", d.day);
            j.add("hour", d.hour);
            j.add("size", d.size);
            auto goods = Value::object();
            for (const auto& [good, n] : d.manifest)
                goods.add(good, n);
            j.add("manifest", goods);
            auto order = Value::array();
            for (const auto& [good, n] : d.manifest)
                order.push(Value(good));
            j.add("order", order);
            if (!d.cause.empty())
                j.add("cause", d.cause);
            j.add("posted", d.posted);
            list.push(j);
        }
        o.add("committed", list);
    }
    // The ports' timetables (Phase 5): written only where there are ships.
    if (!b.ships.empty())
    {
        auto list = Value::array();
        for (const auto& s : b.ships)
        {
            auto j = Value::object();
            j.add("id", s.id);
            j.add("port", s.port);
            j.add("name", s.name);
            j.add("day", s.day);
            j.add("hour", s.hour);
            j.add("sails", s.sails);
            const auto goods = [&](const char* key, const std::vector<std::pair<std::string, int>>& of) {
                // (A list of pairs: the order is the notice's, and Postgres's jsonb keeps no key order.)
                auto pairs = Value::array();
                for (const auto& [good, n] : of)
                {
                    auto pair = Value::array();
                    pair.push(Value(good));
                    pair.push(Value(n));
                    pairs.push(pair);
                }
                j.add(key, pairs);
            };
            goods("brings", s.brings);
            goods("takes", s.takes);
            j.add("status", s.status);
            j.add("posted", s.posted);
            if (s.lost)
                j.add("lost", true);
            list.push(j);
        }
        o.add("ships", list);
    }
    // Staged events and causes (Phase 7): written only where there are any.
    if (!b.events.empty())
    {
        auto list = Value::array();
        for (const auto& e : b.events)
        {
            auto j = Value::object();
            j.add("id", e.id);
            j.add("market", e.market);
            j.add("kind", e.kind);
            j.add("day", e.day);
            j.add("hour", e.hour);
            j.add("days", e.days);
            j.add("times", e.times);
            if (e.ahead)
                j.add("ahead", e.ahead);
            if (e.crowd)
                j.add("crowd", true);
            auto pairs = Value::array();            // (A list of pairs, as a ship's cargo: jsonb keeps no key order.)
            for (const auto& [good, P] : e.goods)
            {
                auto pair = Value::array();
                pair.push(Value(good));
                pair.push(Value(P));
                pairs.push(pair);
            }
            j.add("goods", pairs);
            j.add("words", e.words);
            if (!e.notice.empty())
                j.add("notice", e.notice);
            if (!e.cry.empty())
                j.add("cry", e.cry);
            if (!e.subject.empty())
                j.add("subject", e.subject);
            j.add("status", e.status);
            j.add("by", e.by);
            if (e.announced)
                j.add("announced", true);
            j.add("posted", e.posted);
            list.push(j);
        }
        o.add("events", list);
    }
    if (!b.eventsThrough.empty())
    {
        auto through = Value::object();
        for (const auto& [m, week] : b.eventsThrough)
            through.add(m, week);
        o.add("eventsThrough", through);
    }
    if (b.nextEvent != 1)
        o.add("nextEvent", b.nextEvent);
    if (b.sliderCause.n != 0 || !b.sliderCause.words.empty())
    {
        auto j = Value::object();
        j.add("words", b.sliderCause.words);
        j.add("notice", b.sliderCause.notice);
        j.add("cry", b.sliderCause.cry);
        j.add("dir", b.sliderCause.dir);
        j.add("day", b.sliderCause.day);
        j.add("n", b.sliderCause.n);
        j.add("told", b.sliderCause.told);
        if (b.sliderCause.dm)
            j.add("dm", true);
        o.add("sliderCause", j);
    }
    if (!b.nudges.empty())
    {
        auto n = Value::object();
        for (const auto& [m, nudge] : b.nudges)
        {
            auto j = Value::object();
            j.add("points", nudge.points);
            j.add("until", nudge.until);
            j.add("cause", nudge.cause);
            j.add("told", nudge.told);
            n.add(m, j);
        }
        o.add("nudges", n);
    }
    if (!b.harvestShift.empty())
    {
        auto t = Value::object();
        for (const auto& [m, n] : b.harvestShift)
            t.add(m, n);
        o.add("harvestShift", t);
    }
    if (!b.shipsThrough.empty())
    {
        auto through = Value::object();
        for (const auto& [port, week] : b.shipsThrough)
            through.add(port, week);
        o.add("shipsThrough", through);
    }
    if (b.nextShip != 1)
        o.add("nextShip", b.nextShip);
    if (b.rolledThrough >= 0)
        o.add("rolledThrough", b.rolledThrough);
    if (b.nextDeparture != 1)
        o.add("nextDeparture", b.nextDeparture);
    if (!b.backRoom.empty())
    {
        auto rooms = Value::object();
        for (const auto& [till, goods] : b.backRoom)
        {
            auto g = Value::object();
            for (const auto& [good, n] : goods)
                g.add(good, n);
            rooms.add(till, g);
        }
        o.add("backRoom", rooms);
    }
    if (!b.harvestOverride.empty())
    {
        auto h = Value::object();
        for (const auto& [area, ov] : b.harvestOverride)
        {
            auto j = Value::object();
            j.add("word", ov.word);
            j.add("until", ov.until);
            if (!ov.cause.empty())
                j.add("cause", ov.cause);
            if (!ov.told)
                j.add("told", false);
            h.add(area, j);
        }
        o.add("harvestOverride", h);
    }
    if (!b.troubles.empty())
    {
        auto list = Value::array();
        for (const auto& t : b.troubles)
        {
            auto j = Value::object();
            j.add("day", t.day);
            j.add("market", t.market);
            j.add("kind", t.kind);
            list.push(j);
        }
        o.add("troubles", list);
    }
    if (!b.timesShift.empty())
    {
        auto t = Value::object();
        for (const auto& [m, n] : b.timesShift)
            t.add(m, n);
        o.add("timesShift", t);
    }
    if (!b.news.empty())
    {
        auto n = Value::object();
        for (const auto& [m, news] : b.news)
        {
            auto j = Value::object();
            j.add("from", news.from);
            j.add("words", news.words);
            j.add("day", news.day);
            n.add(m, j);
        }
        o.add("news", n);
    }
    if (!b.broughtIn.empty())
    {
        auto in = Value::object();
        for (const auto& [m, goods] : b.broughtIn)
        {
            auto g = Value::object();
            for (const auto& [good, day] : goods)
                g.add(good, day);
            in.add(m, g);
        }
        o.add("broughtIn", in);
    }
    return o;
}

void mendShips(Board& b)
{
    const auto& d = data();
    std::vector<Ship> kept;
    for (auto& s : b.ships)
    {
        if (s.id.empty() || s.port.empty() ||
            std::any_of(kept.begin(), kept.end(), [&](const Ship& o) { return o.id == s.id || (o.port == s.port && o.day == s.day); }))
            continue;
        if (s.status != "in" && s.status != "sailed" && s.status != "storm")
            s.status = "due";
        s.lost = s.lost && s.status == "storm";
        s.hour = finite(s.hour) ? std::clamp(s.hour, 0., 23.99) : 8;
        s.sails = finite(s.sails) ? std::clamp(s.sails, s.hour, 23.99) : std::max(s.hour, 18.);
        s.posted = finite(s.posted) ? std::max(0., s.posted) : 0;
        const auto cargo = [&](std::vector<std::pair<std::string, int>>& goods, bool brought) {
            std::vector<std::pair<std::string, int>> fit;
            for (const auto& [g, n] : goods)
                if (n > 0 && fit.size() < 5 && items::good(g) && !(brought && d.shipNever.count(g)) &&
                    std::none_of(fit.begin(), fit.end(), [&](const auto& have) { return have.first == g; }))
                    fit.push_back({g, std::min(n, 999)});
            goods = std::move(fit);
        };
        cargo(s.brings, true);
        cargo(s.takes, false);
        kept.push_back(std::move(s));
    }
    for (auto& s : kept)
    {
        if (shipNameFits(s.name))
            continue;
        const auto taken = [&](const std::string& name) {
            return std::any_of(kept.begin(), kept.end(), [&](const Ship& o) { return &o != &s && o.port == s.port && o.name == name; });
        };
        s.name = "a ship";
        const auto& names = d.shipNames;
        for (std::size_t i = 0; i < names.size(); ++i)
            if (const auto& name = names[(std::size_t(std::max<std::int64_t>(0, s.day)) + i) % names.size()]; !taken(name))
            {
                s.name = name;
                break;
            }
    }
    std::map<std::string, std::size_t> ofPort;
    for (const auto& s : kept)
        ++ofPort[s.port];
    for (auto& [port, count] : ofPort)
        for (; count > ShipsAPort; --count)
        {
            auto out = kept.end();
            for (auto it = kept.begin(); it != kept.end(); ++it)
                if (it->port == port && (it->status == "sailed" || it->status == "storm") && (out == kept.end() || it->day < out->day))
                    out = it;
            if (out == kept.end())
                for (auto it = kept.begin(); it != kept.end(); ++it)
                    if (it->port == port && it->status == "due" && (out == kept.end() || it->day > out->day))
                        out = it;
            if (out == kept.end())
                for (auto it = kept.begin(); it != kept.end(); ++it)
                    if (it->port == port)
                        out = it;
            kept.erase(out);
        }
    if (kept.size() > ShipsMost)
        kept.resize(ShipsMost);
    b.ships = std::move(kept);
    b.nextShip = std::max<std::int64_t>(1, b.nextShip);
    for (const auto& s : b.ships)
        if (s.id.size() > 1 && s.id.size() <= 16 && s.id[0] == 's' &&
            std::all_of(s.id.begin() + 1, s.id.end(), [](unsigned char c) { return std::isdigit(c) != 0; }))
            b.nextShip = std::max<std::int64_t>(b.nextShip, std::stoll(s.id.substr(1)) + 1);
}

Board decode(const json::Value& v)
{
    Board b;
    if (!v.isObject())
        return b;
    const auto whole = [](const json::Value& j, double low, double high, double fallback) {
        const double n = j.asNumber(fallback);
        return finite(n) ? std::clamp(std::floor(n), low, high) : fallback;
    };
    const auto name = [](const std::string& s) { return !s.empty() && s.size() <= 120; };
    const auto good = [&](const std::string& s) { return name(s) && (itemValid(s) || items::good(s)); };
    b.slider = int(whole(v["slider"], -100, 100, 0));
    b.sliderTarget = int(whole(v["sliderTarget"], -100, 100, 0));
    b.sliderDay = std::int64_t(whole(v["sliderDay"], -1, 365000000, -1));
    b.day = std::int64_t(whole(v["day"], -1, 365000000, -1));
    for (const auto& [m, goods] : v.object("cell").fields())
        if (name(m))
            for (const auto& [g, c] : goods.fields())
            {
                const double P = c.number("P", 0), touched = c.number("touched", 0);
                if (!good(g) || !finite(P) || !finite(touched) || std::abs(P) < 1e-6)
                    continue;               // (An unknown good, a broken number: dropped, never refused.)
                b.cell[m][g] = {std::clamp(P, -PressureMost, PressureMost), std::max(0., touched), c.string("cause").substr(0, 16)};
            }
    for (const auto& [till, n] : v.object("budgetLeft").fields())
        if (name(till) && n.isNumber())
            b.budgetLeft[till] = std::int64_t(whole(n, 0, 1e9, 0));
    for (const auto& [till, goods] : v.object("takenToday").fields())
        if (name(till))
            for (const auto& [g, n] : goods.fields())
                if (good(g) && n.isNumber())
                    b.takenToday[till][g] = int(whole(n, 0, 1e6, 0));
    for (const auto& [m, holders] : v.object("pushed").fields())
        if (name(m))
            for (const auto& [holder, goods] : holders.fields())
                if (name(holder))
                    for (const auto& [g, p] : goods.fields())
                    {
                        const double amount = p.number("amount", 0), touched = p.number("touched", 0);
                        if (good(g) && finite(amount) && finite(touched) && std::abs(amount) >= 1e-6)
                            b.pushed[m][holder][g] = {std::clamp(amount, -PressureMost, PressureMost), std::max(0., touched)};
                    }
    for (const auto& [m, goods] : v.object("netToday").fields())
        if (name(m))
            for (const auto& [g, holders] : goods.fields())
                if (good(g))
                    for (const auto& [holder, n] : holders.fields())
                        if (name(holder) && n.isNumber())
                            b.netToday[m][g][holder] = int(whole(n, -1e6, 1e6, 0));
    for (const auto& [holder, n] : v.object("soldToday").fields())
        if (name(holder) && n.isNumber())
            b.soldToday[holder] = std::int64_t(whole(n, 0, 1e9, 0));
    b.week = std::int64_t(whole(v["week"], -1, 365000000, -1));
    for (const auto& [chapter, n] : v.object("discountWeek").fields())
        if (name(chapter) && n.isNumber())
            b.discountWeek[chapter] = std::int64_t(whole(n, 0, 1e9, 0));
    for (const auto& [till, day] : v.object("restocked").fields())
        if (name(till) && day.isNumber())
            b.restocked[till] = std::int64_t(whole(day, -1, 365000000, -1));
    for (const auto& [till, until] : v.object("settingOut").fields())
        if (const double t = until.asNumber(-1); name(till) && until.isNumber() && finite(t) && t >= 0)
            b.settingOut[till] = t;
    for (const auto& r : v.array("recent"))
        if (r.isObject() && b.recent.size() < RecentKept)
        {
            Ripple one{r.number("day", 0), r.number("delta", 0), r.string("market"), r.string("good"), r.string("cause").substr(0, 16)};
            if (finite(one.day) && finite(one.delta) && name(one.market) && good(one.good))
                b.recent.push_back(std::move(one));
        }
    // Staged trade (Phase 4), leniently: a departure with no ends or a broken day is dropped, its unknown goods dropped,
    // its numbers clamped (the world drops one whose markets are gone: World::ensureRoadMarkets).
    for (const auto& j : v.array("committed"))
    {
        if (!j.isObject() || b.committed.size() >= CommittedMost)
            continue;
        Departure d;
        d.id = j.string("id").substr(0, 40);
        d.kind = j.string("kind") == "cart" ? "cart" : "wagon";
        d.from = j.string("from").substr(0, 120);
        d.to = j.string("to").substr(0, 120);
        d.driver = j.string("driver").substr(0, 120);
        d.day = std::int64_t(whole(j["day"], 0, 365000000, -1));
        const double hour = j.number("hour", 8);
        d.hour = finite(hour) ? std::clamp(hour, 0., 23.99) : 8;
        d.size = int(whole(j["size"], 1, 3, 1));
        d.cause = j.string("cause").substr(0, 60);
        const double posted = j.number("posted", 0);
        d.posted = finite(posted) ? std::max(0., posted) : 0;
        const auto& goods = j.object("manifest");
        std::vector<std::string> order;
        for (const auto& g : j.array("order"))
            if (g.isString())
                order.push_back(g.asString());
        for (const auto& [g, n] : goods.fields())
            if (std::find(order.begin(), order.end(), g) == order.end())
                order.push_back(g);
        for (const auto& g : order)
            if (const auto* n = goods.find(g); n && n->isNumber() && good(g) && d.manifest.size() < 5)
                if (const int count = int(whole(*n, 0, 999, 0)); count > 0)
                    d.manifest.push_back({g, count});
        if (d.id.empty() || !name(d.from) || !name(d.to) || !j["day"].isNumber())
            continue;
        b.committed.push_back(std::move(d));
    }
    // The ports' timetables (Phase 5), leniently: a ship with no port, name or day is dropped, its unknown goods dropped,
    // its numbers clamped (the world drops one whose port is gone: World::dropOrphanedShips).
    for (const auto& j : v.array("ships"))
    {
        if (!j.isObject() || b.ships.size() >= ShipsMost)
            continue;
        Ship s;
        s.id = j.string("id").substr(0, 40);
        s.port = j.string("port").substr(0, 120);
        s.name = json::utf8Prefix(j.string("name"), 40);
        s.day = std::int64_t(whole(j["day"], 0, 365000000, -1));
        const double hour = j.number("hour", 8), sails = j.number("sails", 18);
        s.hour = finite(hour) ? std::clamp(hour, 0., 23.99) : 8;
        s.sails = finite(sails) ? std::clamp(sails, s.hour, 23.99) : std::max(s.hour, 18.);
        const auto goods = [&](const char* key, std::vector<std::pair<std::string, int>>& into) {
            for (const auto& pair : j.array(key))
                if (const auto& of = pair.items(); of.size() == 2 && of[0].isString() && of[1].isNumber() && good(of[0].asString()) &&
                                                   into.size() < 5)
                    if (const int count = int(whole(of[1], 0, 999, 0)); count > 0)
                        into.push_back({of[0].asString(), count});
        };
        goods("brings", s.brings);
        goods("takes", s.takes);
        const auto status = j.string("status");
        s.status = status == "in" || status == "sailed" || status == "storm" ? status : "due";
        const double posted = j.number("posted", 0);
        s.posted = finite(posted) ? std::max(0., posted) : 0;
        s.lost = s.status == "storm" && j.boolean("lost", false);
        if (s.id.empty() || !name(s.port) || s.name.empty() || !j["day"].isNumber())
            continue;
        // (One ship of an id, and one a port a day, as they are rolled: a save edited to say otherwise keeps the first.)
        if (std::any_of(b.ships.begin(), b.ships.end(), [&](const Ship& o) { return o.id == s.id || (o.port == s.port && o.day == s.day); }))
            continue;
        b.ships.push_back(std::move(s));
    }
    for (const auto& [port, week] : v.object("shipsThrough").fields())
        if (name(port) && week.isNumber() && b.shipsThrough.size() < 64)
            b.shipsThrough[port] = std::int64_t(whole(week, -1, 365000000, -1));
    b.nextShip = std::int64_t(whole(v["nextShip"], 1, 1e15, 1));
    mendShips(b);                                   // (Its bounds a port, its names and cargoes, its next id.)
    b.rolledThrough = std::int64_t(whole(v["rolledThrough"], -1, 365000000, -1));
    b.nextDeparture = std::int64_t(whole(v["nextDeparture"], 1, 1e15, 1));
    for (const auto& [till, goods] : v.object("backRoom").fields())
        if (name(till))
            for (const auto& [g, n] : goods.fields())
                if (good(g) && n.isNumber())
                    b.backRoom[till][g] = int(whole(n, 0, 999, 0));
    for (const auto& [area, o] : v.object("harvestOverride").fields())
        if (const double until = o.number("until", -1); name(area) && o.isObject() && finite(until) && until >= 0)
        {
            auto cause = json::utf8Prefix(o.string("cause"), 80);
            if (!plainWords(cause, 80))
                cause.clear();
            b.harvestOverride[area] = {int(whole(o["word"], 0, 2, 1)), until, cause, o.boolean("told", true)};
        }
    for (const auto& t : v.array("troubles"))
        if (t.isObject() && b.troubles.size() < TroublesKept)
        {
            Trouble one{t.number("day", -1), t.string("market"), t.string("kind")};
            if (finite(one.day) && one.day >= 0 && name(one.market) && (one.kind == "raid" || one.kind == "late"))
                b.troubles.push_back(std::move(one));
        }
    for (const auto& [m, n] : v.object("timesShift").fields())
        if (name(m) && n.isNumber())
            if (const int shift = int(whole(n, -100, 100, 0)); shift != 0)
                b.timesShift[m] = shift;
    for (const auto& [m, n] : v.object("news").fields())
        if (const double day = n.number("day", -1); name(m) && n.isObject() && finite(day) && day >= 0)
            b.news[m] = {n.string("from").substr(0, 120), json::utf8Prefix(n.string("words"), 120), day};
    for (const auto& [m, goods] : v.object("broughtIn").fields())
        if (name(m))
            for (const auto& [g, day] : goods.fields())
                if (good(g) && day.isNumber() && b.broughtIn[m].size() < 512)
                    b.broughtIn[m][g] = std::int64_t(whole(day, 0, 365000000, 0));
    // Staged events and causes (Phase 7), leniently: an event with no market, kind or day is dropped, its unknown goods
    // dropped, its numbers clamped, its words held to the loader's rule (mendEvents); the world drops one whose market is
    // gone (World::dropOrphanedEvents).
    for (const auto& j : v.array("events"))
    {
        if (!j.isObject() || b.events.size() >= EventsMost)
            continue;
        StagedEvent e;
        e.id = j.string("id").substr(0, 40);
        e.market = j.string("market").substr(0, 120);
        e.kind = j.string("kind").substr(0, 32);
        e.day = std::int64_t(whole(j["day"], 0, 365000000, -1));
        e.hour = j.number("hour", 9);
        e.days = int(whole(j["days"], 1, 7, 2));
        e.times = int(whole(j["times"], -EventTimesMost, EventTimesMost, 0));
        e.ahead = int(whole(j["ahead"], 0, 6, 0));
        e.crowd = j.boolean("crowd", false);
        for (const auto& pair : j.array("goods"))
            if (const auto& of = pair.items(); of.size() == 2 && of[0].isString() && of[1].isNumber() && good(of[0].asString()))
                e.goods.push_back({of[0].asString(), of[1].asNumber()});
        e.words = json::utf8Prefix(j.string("words"), 80);
        e.notice = json::utf8Prefix(j.string("notice"), 280);
        e.cry = json::utf8Prefix(j.string("cry"), 200);
        e.subject = j.string("subject").substr(0, 120);
        e.status = j.string("status");
        e.by = j.string("by") == "dm" ? "dm" : "seeded";
        e.announced = j.boolean("announced", false);
        e.posted = j.number("posted", 0);
        if (e.id.empty() || !name(e.market) || e.kind.empty() || !j["day"].isNumber())
            continue;
        b.events.push_back(std::move(e));
    }
    for (const auto& [m, week] : v.object("eventsThrough").fields())
        if (name(m) && week.isNumber() && b.eventsThrough.size() < 256)
            b.eventsThrough[m] = std::int64_t(whole(week, -1, 365000000, -1));
    b.nextEvent = std::int64_t(whole(v["nextEvent"], 1, 1e15, 1));
    mendEvents(b);
    if (const auto& c = v.object("sliderCause"); c.isObject())
    {
        auto& sc = b.sliderCause;
        sc.words = json::utf8Prefix(c.string("words"), 80);
        sc.notice = json::utf8Prefix(c.string("notice"), 280);
        sc.cry = json::utf8Prefix(c.string("cry"), 200);
        if (!plainWords(sc.words, 80) || !plainWords(sc.notice, 280) || !plainWords(sc.cry, 200))
            sc.words.clear(), sc.notice.clear(), sc.cry.clear();
        sc.dir = int(whole(c["dir"], -1, 1, 0));
        const double day = c.number("day", -1);
        sc.day = finite(day) ? std::max(-1., day) : -1;
        sc.n = std::int64_t(whole(c["n"], 0, 1e15, 0));
        sc.told = c.boolean("told", true) || sc.words.empty();
        sc.dm = c.boolean("dm", false);
    }
    for (const auto& [m, n] : v.object("nudges").fields())
        if (const double until = n.number("until", -1); name(m) && n.isObject() && finite(until) && until >= 0 && b.nudges.size() < 64)
        {
            auto cause = json::utf8Prefix(n.string("cause"), 80);
            if (!plainWords(cause, 80))
                cause.clear();
            if (const int points = int(whole(n["points"], -100, 100, 0)); points != 0)
                b.nudges[m] = {points, until, cause, n.boolean("told", true)};
        }
    for (const auto& [m, n] : v.object("harvestShift").fields())
        if (name(m) && n.isNumber())
            if (const int shift = int(whole(n, -100, 100, 0)); shift != 0)
                b.harvestShift[m] = shift;
    return b;
}

void mendEvents(Board& b)
{
    std::vector<StagedEvent> kept;
    for (auto& e : b.events)
    {
        if (e.id.empty() || e.market.empty() || e.kind.empty() || e.day < 0 ||
            std::any_of(kept.begin(), kept.end(), [&](const StagedEvent& o) { return o.id == e.id; }))
            continue;
        if (e.words.empty() || !plainWords(e.words, 80) || !plainWords(e.notice, 280) || !plainWords(e.cry, 200))
            continue;                               // (Words the loader would not take: never pinned on a board.)
        if (e.status != "on" && e.status != "over" && e.status != "cancelled")
            e.status = "due";
        e.hour = finite(e.hour) ? std::clamp(e.hour, 0., 23.99) : 9;
        e.days = std::clamp(e.days, 1, 7);
        e.times = std::clamp(e.times, -EventTimesMost, EventTimesMost);
        e.posted = finite(e.posted) ? std::max(0., e.posted) : 0;
        std::vector<std::pair<std::string, double>> fit;
        for (const auto& [g, P] : e.goods)
            if (finite(P) && fit.size() < 6 && (items::good(g) || itemValid(g)) &&
                std::none_of(fit.begin(), fit.end(), [&](const auto& have) { return have.first == g; }))
                fit.push_back({g, std::clamp(P, -EventTargetMost, EventTargetMost)});
        e.goods = std::move(fit);
        kept.push_back(std::move(e));
    }
    std::map<std::string, std::size_t> ofMarket;
    for (const auto& e : kept)
        ++ofMarket[e.market];
    for (auto& [m, count] : ofMarket)
        for (; count > EventsAMarket; --count)
        {
            auto out = kept.end();
            for (auto it = kept.begin(); it != kept.end(); ++it)
                if (it->market == m && (it->status == "over" || it->status == "cancelled") && (out == kept.end() || it->day < out->day))
                    out = it;
            if (out == kept.end())
                for (auto it = kept.begin(); it != kept.end(); ++it)
                    if (it->market == m && it->status == "due" && (out == kept.end() || it->day > out->day))
                        out = it;
            if (out == kept.end())
                for (auto it = kept.begin(); it != kept.end(); ++it)
                    if (it->market == m)
                        out = it;
            kept.erase(out);
        }
    if (kept.size() > EventsMost)
        kept.resize(EventsMost);
    b.events = std::move(kept);
    b.nextEvent = std::max<std::int64_t>(1, b.nextEvent);
    for (const auto& e : b.events)
        if (e.id.size() > 1 && e.id.size() <= 16 && e.id[0] == 'e' &&
            std::all_of(e.id.begin() + 1, e.id.end(), [](unsigned char c) { return std::isdigit(c) != 0; }))
            b.nextEvent = std::max<std::int64_t>(b.nextEvent, std::stoll(e.id.substr(1)) + 1);
}

int eventTimes(const Board& b, const std::string& market)
{
    int sum = 0;
    for (const auto& e : b.events)
        if (e.status == "on" && e.market == market)
            sum += e.times;
    return std::clamp(sum, -EventTimesMost, EventTimesMost);
}

// ---------------------------------------------------------------------------------------------------- The markets

const TradePoint* Topology::find(const std::string& id) const
{
    if (ofId.size() == markets.size())
    {
        const auto at = ofId.find(id);
        return at == ofId.end() ? nullptr : &markets[at->second];
    }
    for (const auto& m : markets)                   // (Not indexed yet: a test's or a builder's topology.)
        if (m.id == id)
            return &m;
    return nullptr;
}

const TradePoint* Topology::ofTillId(const std::string& till) const
{
    const auto at = ofTill.find(till);
    return at == ofTill.end() || at->second >= markets.size() ? nullptr : &markets[at->second];
}

const TradePoint* Topology::ofCellId(const std::string& cell) const
{
    const auto at = ofCell.find(cell);
    return at == ofCell.end() || at->second >= markets.size() ? nullptr : &markets[at->second];
}

void Topology::index()
{
    ofTill.clear();
    ofId.clear();
    ofCell.clear();
    for (std::size_t i = 0; i < markets.size(); ++i)
    {
        ofId.emplace(markets[i].id, i);
        for (const auto& till : markets[i].tills)
            ofTill.emplace(till, i);
        for (const auto& cell : markets[i].cells)
            ofCell.emplace(cell, i);
    }
    if (ofId.size() != markets.size())
        ofId.clear();                               // (Two markets of one id: find() looks along the list, the first.)
}

// ------------------------------------------------------------------------------------------------------- The links

const Links& links()
{
    static const Links built = [] {
        Links l;
        const auto dir = items::dataDirectory();
        for (const char* file : {"crafts.json", "recipes.json"})
        {
            json::Value v;
            std::string error;
            if (!readFile(dir / file, v, error))
                continue;
            for (const auto& r : v.array(std::string(file) == "crafts.json" ? "crafts" : "recipes"))
            {
                for (const auto& [in, _] : r.object("in").fields())
                    for (const auto& [out, __] : r.object("out").fields())
                        if (in != out && items::good(in) && items::good(out))
                        {
                            ++l.outputs[in][out];
                            ++l.inputs[out][in];
                        }
                // (A recipe with an input the catalogue doesn't know, "@fuel" and the like, is no bound on I5.)
                Links::Recipe recipe;
                bool known = !r.object("in").fields().empty();
                for (const auto& [in, n] : r.object("in").fields())
                {
                    known = known && items::good(in) && n.asNumber(0) >= 1;
                    recipe.in.push_back({in, int(std::clamp(n.asNumber(1), 1., 999.))});
                }
                if (known)
                    for (const auto& [out, n] : r.object("out").fields())
                        if (items::good(out) && n.asNumber(0) >= 1 &&
                            std::none_of(recipe.in.begin(), recipe.in.end(), [&](const auto& i) { return i.first == out; }))
                        {
                            auto one = recipe;
                            one.out = int(std::clamp(n.asNumber(1), 1., 999.));
                            l.makes[out].push_back(std::move(one));
                        }
            }
        }
        return l;
    }();
    return built;
}

std::vector<std::pair<std::string, int>> rippleTargets(const std::string& good, bool downstream)
{
    const auto& table = downstream ? links().outputs : links().inputs;
    std::vector<std::pair<std::string, int>> out;
    std::set<std::string> seen{good};
    std::vector<std::string> frontier{good};
    for (int step = 1; step <= 2 && !frontier.empty(); ++step)
    {
        std::map<std::string, int> uses;
        for (const auto& from : frontier)
            if (const auto row = table.find(from); row != table.end())
                for (const auto& [to, n] : row->second)
                    if (!seen.count(to))
                        uses[to] += n;
        std::vector<std::pair<std::string, int>> ranked(uses.begin(), uses.end());
        std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
            if (a.second != b.second)
                return a.second > b.second;
            const auto pa = Society::catalogPrice(a.first), pb = Society::catalogPrice(b.first);
            return pa != pb ? pa < pb : a.first < b.first;
        });
        if (ranked.size() > RippleKeep)
            ranked.resize(RippleKeep);
        frontier.clear();
        for (const auto& [to, _] : ranked)
        {
            seen.insert(to);
            frontier.push_back(to);
            out.push_back({to, step});
        }
    }
    return out;
}
} // namespace ratw::market

// =================================================================================================== Society's part

namespace ratw
{
namespace
{
// A price's cause where a ship moved it (doc 79, 8.6 and Section 11: "the Gull came in"; the Phase 5 repair): the Board's
// word for it ("ship": its cargo, cheaper; "ship takes": what it buys, dearer; "ship late": a stormbound ship's cargo,
// dearer) said of the ship it was, by name, while the timetable still has it; and never a cause the price belies.
std::string shipCause(const market::Board& b, const std::string& market, const std::string& base, const std::string& cause, int trend)
{
    const bool takes = cause == "ship takes", late = cause == "ship late";
    const market::Ship* ship = nullptr;
    for (const auto& s : b.ships)
    {
        if (s.port != market || (late ? s.status != "storm" : s.status != "in" && s.status != "sailed"))
            continue;
        const auto& goods = takes ? s.takes : s.brings;
        if (std::any_of(goods.begin(), goods.end(), [&](const auto& g) { return g.first == base; }) && (!ship || s.day > ship->day))
            ship = &s;
    }
    const std::string name = ship ? ship->name : "a ship";
    if (late && ship && ship->lost)
        return trend > 0 ? name + " was lost at sea" : "cheap still, though " + name + " was lost at sea";
    if (late)
        return trend > 0 ? name + " is stormbound" : "cheap still, though " + name + " is stormbound";
    if (takes)
        return trend > 0 ? name + (ship && ship->status == "in" ? " is in, buying" : " bought a load")
                         : "cheap still, though " + name + (ship && ship->status == "in" ? " is buying" : " bought a load");
    return trend < 0 ? name + " came in" : "dear still, though " + name + " came in";
}
// A price's cause where a staged event moved it (doc 79, 8.4; Phase 7): the words of the latest event at that market
// pressing the good (on, else over), never a cause the price belies; and the roads' and the farms' causes in words.
std::string eventCause(const market::Board& b, const std::string& market, const std::string& base, int trend)
{
    const market::StagedEvent* event = nullptr;
    for (const auto& e : b.events)
        if (e.market == market && (e.status == "on" || e.status == "over"))
            for (const auto& [g, target] : e.goods)
                if (g == base && (!event || (e.status == "on") > (event->status == "on") ||
                                  ((e.status == "on") == (event->status == "on") && e.day >= event->day)))
                    event = &e;
    if (!event)
        return trend > 0 ? "talk of trouble at the market" : "talk of plenty at the market";
    double target = 0;
    for (const auto& [g, t] : event->goods)
        if (g == base)
            target = t;
    return (target > 0) == (trend > 0) ? event->words : (trend > 0 ? "dear still, though " : "cheap still, though ") + event->words;
}
std::string roadCause(const std::string& cause, int trend)
{
    if (cause == "caravan")
        return trend < 0 ? "the wagon came in" : "dear still, though the wagon came in";
    if (cause == "raid")
        return trend > 0 ? "the wagon was robbed on the road" : "cheap still, though the wagon was robbed";
    if (cause == "late")
        return trend > 0 ? "the wagon's late" : "cheap still, though the wagon's late";
    if (cause == "harvest")
        return trend < 0 ? "a good harvest" : "a short harvest, or the cart didn't come";
    return cause;
}
constexpr std::int64_t MarketMoneyLimit = 1000000000;   // (RatwSociety.cpp's MoneyLimit and StockLimit.)
constexpr int MarketStockLimit = 10000;

std::string lotWords(int n)
{
    return n == 2 ? "the pair" : n == 6 ? "the half-dozen" : n == 12 ? "the dozen" : "the " + std::to_string(n);
}
} // namespace

// The rules as this world has them (doc 79, 12): items::Rules' defaults, market.json's `rules` over them, the world's own
// over those, each within its range; read once, with the Board's own (readMarketRules).
void Society::buildRules()
{
    rules_.clear();
    for (const auto& info : items::Rules)
    {
        int value = info.fallback;
        if (const auto d = market::data().rules.find(info.name); d != market::data().rules.end())
            value = d->second;
        if (const auto own = authored_.economy.rules.find(info.name); own != authored_.economy.rules.end())
            value = own->second;
        rules_[info.name] = std::clamp(value, info.low, info.high);
    }
    marketRules_ = readMarketRules();
}

int Society::rule(const std::string& name) const
{
    if (const auto found = rules_.find(name); found != rules_.end())
        return found->second;
    const auto* info = items::ruleInfo(name);
    return info ? info->fallback : -1;
}

void Society::adoptEconomy(const EconomySpec& economy)
{
    auto next = economy;
    const auto& now = authored_.economy;
    next.treasury = now.treasury;                   // (Starting amounts: spent long ago.)
    next.storeHerbs = now.storeHerbs;
    next.storeMeals = now.storeMeals;
    authored_.economy = std::move(next);
    buildRules();
}

void Society::setYearDay(double absoluteDay)
{
    if (std::isfinite(absoluteDay) && absoluteDay >= 0)
        yearDay_ = std::fmod(absoluteDay, double(calendar::DaysPerYear));
}

market::Rules Society::readMarketRules() const
{
    // (Read once from the world's rules as buildRules sets them: marketRules() is on every price's path.)
    market::Rules r;
    const auto read = [&](const char* name, double scale, double fallback) {
        const int v = ownRule(name);
        return v < 0 ? fallback : v * scale;
    };
    r.halfLifePerishable = read("halfLifePerishable", .1, 2);
    r.halfLifeKeeping = read("halfLifeKeeping", .1, 5);
    r.halfLifeDurable = read("halfLifeDurable", .1, 8);
    r.ripple = std::clamp(read("ripple", .01, .5), 0., .7);
    r.depthScale = std::clamp(read("depthScale", .01, 1), .1, 10.);
    r.pushMost = std::clamp(read("playerPushMost", .01, .15), 0., market::PressureMost);
    r.sellMost = std::clamp<std::int64_t>(std::int64_t(read("playerSellMost", 1, 400)), 0, 5000);
    r.takeAllowance = std::clamp(read("takeAllowance", .01, .5), .05, 1.);
    r.shelfScale = std::clamp(read("shelfScale", .01, 1), .1, 10.);
    r.discountMost = std::clamp<std::int64_t>(std::int64_t(read("factionDiscountMost", 1, 30)), 0, 1000);
    return r;
}

void Society::resumeAt(double absoluteDay)
{
    if (!std::isfinite(absoluteDay) || absoluteDay < 0)
        return;
    decidingAt_ = absoluteDay;
    if (roster_ != Roster::Authored)
        return;                                     // (Only an authored roster's decide sets the Board's and the season's.)
    marketAt_ = absoluteDay;
    setYearDay(absoluteDay);
}

double Society::marketClock() const
{
    return marketAt_ >= 0 ? marketAt_ : std::max(0., decidingAt_);
}

std::int64_t Society::marketDay() const
{
    return std::int64_t(std::floor(marketClock() - market::DawnHour / 24));
}

void Society::setTradePoints(market::Topology topology)
{
    topology.fromWorld = true;
    topology.index();
    // (Each market's tills before, so a household's food shop is chosen again when a market's shops move.)
    std::map<std::string, std::vector<std::string>> tillsWere;
    const bool first = !worldTopology_.fromWorld;   // (At a restore the bands were worked out on Society's own market.)
    for (const auto& m : worldTopology_.markets)
        tillsWere[m.id] = m.tills;
    worldTopology_ = std::move(topology);
    setOutCache_.clear();
    districtLeast_.clear();
    districtWarm_ = false;
    // (On load, unknown markets are dropped from the Board: doc 79, 5.2. A host's deleted community leaves nothing.)
    auto& b = state_.board;
    const auto known = [&](const std::string& m) { return worldTopology_.find(m) != nullptr; };
    for (auto it = b.cell.begin(); it != b.cell.end();)
        it = known(it->first) ? std::next(it) : b.cell.erase(it);
    for (auto it = b.pushed.begin(); it != b.pushed.end();)
        it = known(it->first) ? std::next(it) : b.pushed.erase(it);
    for (auto it = b.netToday.begin(); it != b.netToday.end();)
        it = known(it->first) ? std::next(it) : b.netToday.erase(it);
    // Today's households against the world's markets (doc 79, 5.1 and 6.5): after a restore their markets, food shops and
    // provisions were worked out before the world had found its markets (Society's own one market, "realm"), and the
    // day's pass would not come again until midnight. Again now, each keeping its band (as at a restore), when the
    // markets are new or a household's market or a market's shops have moved.
    if (stagedDay_ >= 0 && !stagedHomes_.empty())
    {
        bool again = first;
        for (const auto& m : worldTopology_.markets)
            if (const auto w = tillsWere.find(m.id); w == tillsWere.end() || w->second != m.tills)
                again = true;
        for (auto h = stagedHomes_.begin(); !again && h != stagedHomes_.end(); ++h)
        {
            std::string homeCell;
            if (!h->second.members.empty())
                if (const auto r = state_.residents.find(h->second.members.front()); r != state_.residents.end())
                    homeCell = r->second.homeCell;
            again = marketOfHome(homeCell, h->second.members) != h->second.market;
        }
        if (again)
        {
            const bool was = restoring_;
            restoring_ = true;
            stagedBands(stagedDay_);
            restoring_ = was;
        }
    }
}

const market::Topology& Society::tradePoints() const
{
    if (worldTopology_.fromWorld)
        return worldTopology_;
    // Society's own (no world, or a world that hasn't found its markets): one market of every till, its id the capital's
    // or "realm"; its catchment every household and passing trade. Rebuilt when the roster moves.
    if (ownTopologyKey_ != rosterRevision_ || ownTopology_.markets.empty())
    {
        ownTopologyKey_ = rosterRevision_;
        market::Topology t;
        market::TradePoint m;
        m.id = capital_.empty() ? "realm" : capital_;
        for (const auto& p : positions_)
            if (p.role == "merchant")
                if (const auto till = "till:" + p.id; account(till))
                {
                    m.tills.push_back(till);
                    for (const auto& ware : stagedShelfGoods(p.founder))
                        m.sold.insert(items::baseOf(ware));
                    if (const auto* r = spec(p.founder))
                        if (const auto* business = items::businessFor(r->workLabel))
                            market::madeBy(business->id, m.made);
                }
        m.catchment = double(households(true).size()) + market::PassingTrade;
        t.markets.push_back(std::move(m));
        t.index();
        ownTopology_ = std::move(t);
        districtLeast_.clear();
        districtWarm_ = false;
        ownTopologyKey_ = rosterRevision_;          // (As it stands after the households were asked for.)
    }
    return ownTopology_;
}

std::string Society::marketOf(const std::string& till) const
{
    const auto& t = tradePoints();
    if (const auto* m = t.ofTillId(till))
        return m->id;
    // (A shop the markets don't know yet, a keeper with no till: the first market's, else "outside".)
    return t.markets.empty() ? std::string("outside") : t.markets.front().id;
}

std::string Society::marketOfHome(const std::string& homeCell, const std::vector<std::string>& members) const
{
    const auto& t = tradePoints();
    if (const auto* m = homeCell.empty() ? nullptr : t.ofCellId(homeCell))
        return m->id;
    for (const auto& id : members)
        if (const auto* job = jobOf(id))
        {
            if (const auto* m = job->role == "merchant" ? t.ofTillId("till:" + job->id) : nullptr)
                return m->id;
            if (const auto* m = t.ofCellId(job->work.cell))
                return m->id;
        }
    return t.markets.empty() ? std::string("outside") : t.markets.front().id;
}

void Society::setQuarters(std::map<std::string, std::string> quarters)
{
    quarters_ = std::move(quarters);
    districtLeast_.clear();
    districtWarm_ = false;
}

bool Society::setProsperity(int target, bool now, const std::string& cause)
{
    auto& b = state_.board;
    target = std::clamp(target, -100, 100);
    const int from = b.slider;
    const bool moved = target != b.sliderTarget;
    b.sliderTarget = target;
    if (now)
        b.slider = b.sliderTarget;                 // (The DM's "now": no smoothing, doc 79, 6.3.)
    // Its cause (8.5): the way it moves now, else (a cause written for a slider already there) the way it stands.
    int dir = target > from ? 1 : target < from ? -1 : 0;
    if (dir == 0 && !cause.empty())
        dir = target > 0 ? 1 : target < 0 ? -1 : b.sliderCause.dir;
    const auto& d = market::data();
    const auto fill = [](std::string text, const std::string& words) {
        auto capital = words;
        if (!capital.empty())
            capital[0] = char(std::toupper(static_cast<unsigned char>(capital[0])));
        for (const auto& [key, with] : {std::pair<std::string, std::string>{"{cause}", words}, {"{Cause}", capital}})
            for (auto at = text.find(key); at != std::string::npos; at = text.find(key, at + with.size()))
                text.replace(at, key.size(), with);
        return text;
    };
    auto words = json::utf8Prefix(cause, 80);
    while (!words.empty() && (words.back() == ' ' || words.back() == '.'))
        words.pop_back();
    while (!words.empty() && words.front() == ' ')
        words.erase(words.begin());
    const bool taken = !words.empty() && market::plainWords(words, 80);
    if (dir == 0 || (!taken && (!moved || (b.sliderCause.dir == dir && !b.sliderCause.words.empty()))))
        return taken || cause.empty();
    if (taken && b.sliderCause.dm && b.sliderCause.words == words && b.sliderCause.dir == dir)
        return true;                               // (The same words again, the same way: a sync says what it said.)
    market::SliderCause next;
    next.dir = dir;
    next.day = marketClock();
    next.n = b.sliderCause.n + 1;
    next.told = false;
    if (taken)
    {
        next.words = words;
        next.dm = true;
        next.notice = json::utf8Prefix(fill(dir < 0 ? d.causeNoticeDown : d.causeNoticeUp, words), 280);
        next.cry = json::utf8Prefix(fill(dir < 0 ? d.causeCryDown : d.causeCryUp, words), 200);
    }
    else
    {
        const auto& list = dir < 0 ? d.causesDown : d.causesUp;
        if (list.empty())
            return cause.empty();
        // (Seeded by the day and the cause's number: the same world, the same cause; never the one it had just now.)
        auto at = std::size_t(std::hash<std::string>{}("cause:" + std::to_string(marketDay()) + ":" + std::to_string(next.n)) % list.size());
        if (list.size() > 1 && list[at].words == b.sliderCause.words)
            at = (at + 1) % list.size();
        const auto& c = list[at];
        next.words = c.words;
        next.notice = c.notice.empty() ? json::utf8Prefix(fill(dir < 0 ? d.causeNoticeDown : d.causeNoticeUp, c.words), 280) : c.notice;
        next.cry = c.cry.empty() ? json::utf8Prefix(fill(dir < 0 ? d.causeCryDown : d.causeCryUp, c.words), 200) : c.cry;
    }
    b.sliderCause = std::move(next);
    return taken || cause.empty();
}

int Society::timesScore(const std::string& market) const
{
    // 5.3: the slider, plus the market's own shift from its areas' harvests and the roads' troubles of the week (raids and
    // late wagons to it), which the world sets each morning (Phase 4), its staged events on (Phase 7) and the DM's nudge
    // while it holds (Phase 7's, set by the DM's side in Phase 9). Never any player-caused pressure.
    const auto& b = state_.board;
    int score = b.slider;
    if (const auto shift = b.timesShift.find(market); shift != b.timesShift.end())
        score += shift->second;
    if (!b.events.empty())
        score += market::eventTimes(b, market);
    if (const auto nudge = b.nudges.find(market); nudge != b.nudges.end() && marketClock() < nudge->second.until)
        score += nudge->second.points;
    return std::clamp(score, -300, 300);
}

int Society::marketTimes(const std::string& market) const
{
    return std::clamp(market::timesOf(timesScore(market)), 0, 4) - 2;
}

std::string Society::timesCause(const std::string& market) const
{
    // The largest part of the score on its side (T16, 8.5: what people say the times are down to).
    const auto& b = state_.board;
    const int score = timesScore(market);
    if (score == 0)
        return {};
    std::vector<std::pair<int, std::string>> parts;
    if (b.slider != 0)
    {
        const int dir = b.slider > 0 ? 1 : -1;
        parts.push_back({b.slider, !b.sliderCause.words.empty() && b.sliderCause.dir == dir ? b.sliderCause.words
                                   : dir < 0 ? std::string("hard times across the realm") : std::string("the realm's good fortune")});
    }
    if (const auto h = b.harvestShift.find(market); h != b.harvestShift.end() && h->second != 0)
        parts.push_back({h->second, h->second > 0 ? "a good harvest" : "a poor harvest"});
    int raids = 0, lates = 0;
    for (const auto& t : b.troubles)
        if (t.market == market)
            ++(t.kind == "raid" ? raids : lates);
    if (raids > 0)
        parts.push_back({-std::min(30, 10 * raids), "the wagons robbed on the road"});
    if (lates > 0)
        parts.push_back({-std::min(15, 5 * lates), "the wagons late on the road"});
    for (const auto& e : b.events)
        if (e.status == "on" && e.market == market && e.times != 0)
            parts.push_back({e.times, e.words});
    if (const auto nudge = b.nudges.find(market); nudge != b.nudges.end() && marketClock() < nudge->second.until)
        parts.push_back({nudge->second.points, !nudge->second.cause.empty() ? nudge->second.cause
                                               : nudge->second.points > 0 ? std::string("trade picking up") : std::string("trade falling off")});
    const std::pair<int, std::string>* best = nullptr;
    for (const auto& p : parts)
        if ((p.first > 0) == (score > 0) && (!best || std::abs(p.first) > std::abs(best->first)))
            best = &p;
    return best ? best->second : std::string();
}

void Society::setHarvestShift(std::map<std::string, int> shift)
{
    for (auto it = shift.begin(); it != shift.end();)
        it = it->second == 0 ? shift.erase(it) : std::next(it);
    for (auto& [m, n] : shift)
        n = std::clamp(n, -100, 100);
    state_.board.harvestShift = std::move(shift);
}

void Society::setTimesShift(std::map<std::string, int> shift)
{
    for (auto it = shift.begin(); it != shift.end();)
        it = it->second == 0 ? shift.erase(it) : std::next(it);
    for (auto& [m, n] : shift)
        n = std::clamp(n, -100, 100);
    state_.board.timesShift = std::move(shift);
}

bool Society::imported(const std::string& market, const std::string& base) const
{
    const auto found = imports_.find(market);
    return found != imports_.end() && found->second.count(base) > 0;
}

int Society::marketShelf(const std::string& market, const std::string& base) const
{
    int total = 0;
    if (const auto* m = tradePoints().find(market))
        for (const auto& till : m->tills)
            if (const auto* p = till.rfind("till:", 0) == 0 ? position(till.substr(5)) : nullptr)
                for (const auto& ware : stagedShelfGoods(p->founder))
                    if (items::baseOf(ware) == base)
                        total += shelfTarget(till, ware);
    return total;
}

double Society::marketFullness(const std::string& market, const std::string& base) const
{
    // How full a market keeps a good it waits for wagons for (a manifest's want, 8.1 and T14): its shelves and back rooms
    // (one never stocked yet counted as full, as its first restock would stock it) against a shelf and a back room's worth.
    // 1 full, 0 bare; 1 for a good no shop there sets out.
    double have = 0, full = 0;
    if (const auto* m = tradePoints().find(market))
        for (const auto& till : m->tills)
            if (const auto* p = till.rfind("till:", 0) == 0 ? position(till.substr(5)) : nullptr)
            {
                const auto* a = account(till);
                for (const auto& ware : stagedShelfGoods(p->founder))
                    if (items::baseOf(ware) == base)
                    {
                        const int target = shelfTarget(till, ware);
                        int room = target;
                        if (const auto r = state_.board.backRoom.find(till); r != state_.board.backRoom.end())
                            if (const auto g = r->second.find(base); g != r->second.end())
                                room = g->second;
                        have += std::min(target, a ? stock(*a, ware) : 0) + std::min(target, room);
                        full += 2.0 * target;
                    }
            }
    return full > 0 ? std::clamp(have / full, 0., 1.) : 1.;
}

int Society::arrivalRefill(const std::string& market, const std::string& base, double scale, double rooms)
{
    // An arrival (8.6): the tills of the market's shops that set the good out jump to their targets (a Marketday cart's
    // stalls a little over: `scale`), each back room filled to a shelf more, and the keeper seen "unloading" a while.
    rollMarket();
    int tills = 0;
    const auto* m = tradePoints().find(market);
    if (!m || !market::finite(scale) || !market::finite(rooms))
        return 0;
    scale = std::clamp(scale, 1., 2.);
    rooms = std::clamp(rooms, 1., 3.);              // (A ship's back rooms hold more than a wagon's: Phase 5, 8.3.)
    if (imported(market, base) && state_.board.day >= 0)
        state_.board.broughtIn[market][base] = state_.board.day;   // (When it came: the next wagons' rotation, T14.)
    for (const auto& till : m->tills)
    {
        const auto* p = till.rfind("till:", 0) == 0 ? position(till.substr(5)) : nullptr;
        auto found = state_.accounts.find(till);
        if (!p || found == state_.accounts.end())
            continue;
        auto& a = found->second;
        bool any = false;
        for (const auto& ware : stagedShelfGoods(p->founder))
        {
            if (items::baseOf(ware) != base || !itemValid(ware))
                continue;
            const int target = shelfTarget(till, ware);
            const int want = std::min(MarketStockLimit, int(std::ceil(target * scale - 1e-9)));
            if (imported(market, base))             // (Only a good it waits for wagons for keeps a back room.)
            {
                auto& room = state_.board.backRoom[till][base];
                room = std::max(room, int(std::ceil(target * rooms - 1e-9)));
            }
            any = true;
            if (stock(a, ware) >= want || (!a.stock.count(ware) && a.stock.size() >= MaxGoodsKinds))
                continue;
            a.stock[ware] = want;
            tillsMoved_.insert(till);               // (Journalled with the Board's side of the arrival.)
        }
        if (any)
        {
            ++tills;
            state_.board.settingOut[till] = marketClock() + market::SettingOutDays;
        }
    }
    return tills;
}

int Society::sendOn(const std::string& market, const std::string& base)
{
    // Goods move on (7.5): what a market's tills hold of a good over its target leaves on the wagon or cart going out.
    int pieces = 0;
    const auto* m = tradePoints().find(market);
    if (!m)
        return 0;
    for (const auto& till : m->tills)
    {
        const auto* p = till.rfind("till:", 0) == 0 ? position(till.substr(5)) : nullptr;
        auto found = state_.accounts.find(till);
        if (!p || found == state_.accounts.end())
            continue;
        // (Whatever it holds of the good: what it sets out, and what it only buys from players and so never sets out.)
        for (auto& [ware, have] : found->second.stock)
            if (items::baseOf(ware) == base)
                if (const int over = have - shelfTarget(till, ware); over > 0)
                {
                    have -= over;
                    pieces += over;
                    tillsMoved_.insert(till);
                }
    }
    return pieces;
}

Society::StagedHooks Society::boardHooks()
{
    StagedHooks h;
    h.times = [](const Society& s, const std::string& market) { return s.marketTimes(market); };
    h.pressure = [](const Society& s, const std::string& market, const std::string& good) {
        return market.empty() ? 0. : s.pressure(market, items::baseOf(good));
    };
    h.shelf = &Society::tillShelf;
    return h;
}

void Society::stepSlider(std::int64_t day, bool now)
{
    // The one slider (doc 79, 5.2; D17): the Board's dawn and the bands' day's pass both step it, once a day whichever
    // comes first, at most the status table's smoothing (10, SliderStepADay) a day toward the DM's; "now" goes at once.
    // Its first step (no day yet) counts as one day's.
    auto& b = state_.board;
    if (now)
        b.slider = b.sliderTarget;
    else if (day > b.sliderDay)
    {
        const auto days = b.sliderDay < 0 ? 1 : std::min<std::int64_t>(day - b.sliderDay, 365);
        const int step = int(std::min<std::int64_t>(200, std::llround(statusTable().smoothing * double(days))));
        b.slider = b.slider < b.sliderTarget ? std::min(b.sliderTarget, b.slider + step) : std::max(b.sliderTarget, b.slider - step);
    }
    b.sliderDay = std::max(b.sliderDay, day);
}

void Society::setPressure(const std::string& market, const std::string& base, double P, const std::string& cause)
{
    if (!market::finite(P) || market.empty() || base.empty())
        return;
    P = std::clamp(P, -market::PressureMost, market::PressureMost);
    if (std::abs(P) < 1e-9)
    {
        if (const auto m = state_.board.cell.find(market); m != state_.board.cell.end())
            m->second.erase(base);
        return;
    }
    state_.board.cell[market][base] = {P, marketClock(), cause.substr(0, 16)};
}

double Society::depthOf(const std::string& market, const std::string& base) const
{
    const auto* m = tradePoints().find(market);
    const double catchment = m ? std::max(1., m->catchment) : market::PassingTrade;
    const double depth = std::max(market::DepthLeast, market::depthBase(base) * std::pow(catchment, .6));
    return depth * marketRules().depthScale;
}

double Society::pressure(const std::string& market, const std::string& base) const
{
    const auto m = state_.board.cell.find(market);
    if (m == state_.board.cell.end())
        return 0;
    const auto c = m->second.find(base);
    if (c == m->second.end())
        return 0;
    return market::decayed(c->second.P, c->second.touched, marketClock(), market::halfLife(base, marketRules()));
}

double Society::pushedBy(const std::string& market, const std::string& holder, const std::string& base) const
{
    const auto m = state_.board.pushed.find(market);
    if (m == state_.board.pushed.end())
        return 0;
    const auto h = m->second.find(holder);
    if (h == m->second.end())
        return 0;
    const auto p = h->second.find(base);
    if (p == h->second.end())
        return 0;
    return market::decayed(p->second.amount, p->second.touched, marketClock(), market::halfLife(base, marketRules()));
}

const market::Data::Season* Society::stagedSeasonGroup(const std::string& base, std::array<int, 4>* price,
                                                      std::array<int, 4>* shelf) const
{
    // market.json's group (7.1), with this world's own "season" record for that group over it (the DM's Economy tab,
    // until Phase 9's market.json overrides).
    const auto* g = market::seasonOf(base);
    if (!g)
        return nullptr;
    const auto own = authored_.economy.seasons.find(g->id);
    if (price)
        *price = own != authored_.economy.seasons.end() ? own->second.price : g->price;
    if (shelf)
        *shelf = own != authored_.economy.seasons.end() ? own->second.shelf : g->shelf;
    return g;
}

double Society::stagedSeason(const std::string& item) const
{
    // The season's price (market.json's groups, as this world has them), its swing rescaled to fit the clamp with room
    // for pressure (doc 79, 7.1: fruit's 80-140 is wider than the clamp; market::seasonFactor).
    std::array<int, 4> price{};
    if (yearDay_ < 0 || !stagedSeasonGroup(items::baseOf(item), &price, nullptr))
        return 1;
    return market::seasonFactor(price, yearDay_);
}

double Society::districtFactor(const std::string& till, const std::string& base) const
{
    return districtGroupFactor(till, market::groupOf(base));
}

const std::map<std::string, double>* Society::districtRow(const std::string& till) const
{
    // The till's keeper's quarter's row of market.json's `district` (none: no factor).
    if (quarters_.empty())
        return nullptr;
    std::string keeper;
    if (till.rfind("till:", 0) == 0)
    {
        const auto post = state_.careers.positions.find(till.substr(5));
        keeper = post != state_.careers.positions.end() && !post->second.holder.empty() ? post->second.holder : std::string();
        if (keeper.empty())
            for (const auto& p : positions_)
                if (p.id == till.substr(5))
                    keeper = p.founder;
    }
    else
        keeper = till;
    const auto q = quarters_.find(keeper);
    if (q == quarters_.end())
        return nullptr;
    const auto& d = market::data();
    const auto row = d.district.find(q->second);
    return row == d.district.end() ? nullptr : &row->second;
}

double Society::districtGroupFactor(const std::string& till, const std::string& group) const
{
    const auto* row = districtRow(till);
    if (!row)
        return 1;
    if (const auto f = row->find(group); f != row->end())
        return f->second;
    if (const auto f = row->find("all"); f != row->end())
        return f->second;
    return 1;
}

double Society::marketBase(const std::string& market, const std::string& base, double district) const
{
    const auto& d = market::data();
    const auto& t = tradePoints();
    const auto* m = t.find(market);
    double region = d.regionFar;
    if (m && m->made.count(base))
        region = d.regionMade;
    else if (m)
        for (const auto& other : m->routes)
            if (const auto* o = t.find(other); o && o->made.count(base))
            {
                region = d.regionNear;
                break;
            }
    double times = 1;
    if (const auto row = d.times.find(market::groupOf(base)); row != d.times.end())
        times = row->second[std::size_t(marketTimes(market) + 2)];
    return stagedSeason(base) * region * district * times;
}

double Society::baseFactor(const std::string& till, const std::string& item) const
{
    const auto base = items::baseOf(item);
    return marketBase(marketOf(till), base, districtFactor(till, base));
}

double Society::leastDistrict(const std::string& market, const std::string& base) const
{
    // The table is filled whole on the first read (or by prepareMarket, before the snapshot threads), and never written
    // after until the markets or quarters change: the threads only read it.
    if (!districtWarm_)
        prepareMarket();
    const auto group = market::groupOf(base);
    if (const auto kept = districtLeast_.find(market + "|" + group); kept != districtLeast_.end())
        return kept->second;
    double least = 1;                               // (A market or group the table doesn't know: worked out, not kept.)
    if (const auto* m = tradePoints().find(market))
        for (const auto& till : m->tills)
            least = std::min(least, districtFactor(till, base));
    return least;
}

std::map<std::string, int> Society::farmCrews() const
{
    // Every post at a farm or works, by the farm's community (communities.json), filled or not: the crew the land keeps,
    // whoever stands in it today.
    std::map<std::string, int> out;
    for (const auto& p : positions_)
        if (const int farm = farmOfPost(p); farm >= 0 && std::size_t(farm) < farms_.size())
            if (const auto* def = farms_[std::size_t(farm)].def)
                if (const auto* site = items::farmPeople().farm(def->id))
                    ++out[site->community];
    return out;
}

void Society::prepareMarket() const
{
    const auto& t = tradePoints();                  // (Society's own markets, rebuilt here if the roster moved.)
    for (const auto& p : positions_)
        if (p.role == "merchant")
        {
            wares(p.founder);                       // (The wares' cache, kept by label and the staged buys: its keeper's
            if (const auto ps = state_.careers.positions.find(p.id); ps != state_.careers.positions.end() && !ps->second.holder.empty())
                wares(ps->second.holder);           // and whoever holds the post now.)
        }
    if (districtWarm_)
        return;
    districtLeast_.clear();
    // Each group's least factor over a market's tills, each till's by its keeper's quarter (looked up once a till).
    const auto& groups = market::data().groups;
    for (const auto& m : t.markets)
    {
        std::map<std::string, double> least;
        for (const auto& [group, _] : groups)
            least[group] = 1;
        for (const auto& till : m.tills)
            if (const auto* row = districtRow(till))
                for (auto& [group, f] : least)
                {
                    auto at = row->find(group);
                    if (at == row->end())
                        at = row->find("all");
                    if (at != row->end())
                        f = std::min(f, at->second);
                }
        for (const auto& [group, f] : least)
            districtLeast_[m.id + "|" + group] = f;
    }
    districtWarm_ = true;
}

Society::SellBounds Society::sellBounds(const std::string& item) const
{
    // What I3's backstop and I5's cap rest on, worked out once for a quote (each over every market, live): the least
    // factor any shop that sets the good out could sell it at now (flat at each market's P, its least district), and the
    // least its recipes' inputs cost a piece made.
    SellBounds b;
    const auto base = items::baseOf(item);
    for (const auto& m : tradePoints().markets)
        if (m.sold.count(base))
            b.least = std::min(b.least, market::factor(marketBase(m.id, base, leastDistrict(m.id, base)), pressure(m.id, base)));
    if (const auto found = market::links().makes.find(base); found != market::links().makes.end())
        for (const auto& r : found->second)
        {
            double cost = 0;
            for (const auto& [in, n] : r.in)
                cost += cheapestEach(in) * n;
            b.craft = std::min(b.craft, cost / std::max(1, r.out));
        }
    b.quality = std::max(1., double(catalogPrice(item)) / double(std::max<std::int64_t>(1, catalogPrice(base))));
    return b;
}

double Society::cheapestEach(const std::string& item) const
{
    // A piece of `item` at the cheapest any shop could sell it now, unrounded (flat at each market's P, at a stall); a
    // good no shop sets out, the catalogue at the clamp's floor at a stall.
    const auto each = double(catalogPrice(item));
    const auto base = items::baseOf(item);
    double least = std::numeric_limits<double>::infinity();
    for (const auto& m : tradePoints().markets)
        if (m.sold.count(base))
            least = std::min(least, market::factor(marketBase(m.id, base, leastDistrict(m.id, base)), pressure(m.id, base)));
    return market::StallShare * each * (std::isfinite(least) ? least : market::FactorLow);
}

std::int64_t Society::craftCap(const std::string& item, int quantity) const
{
    return craftCap(sellBounds(item), quantity);
}

std::int64_t Society::craftCap(const SellBounds& b, int quantity) const
{
    // I5 (7.6): a crafted good sells for at most 1.6 × its inputs' buy price (a labour premium), its quality's share
    // kept (a fine piece is the skill's, not the inputs'). A good no recipe makes: no cap.
    if (!std::isfinite(b.craft))
        return std::numeric_limits<std::int64_t>::max();
    return std::int64_t(std::floor(market::CraftPremium * b.craft * b.quality * quantity + 1e-9));
}

std::int64_t Society::cheapestLot(const std::string& item, int quantity) const
{
    return cheapestLot(item, sellBounds(item), quantity);
}

std::int64_t Society::cheapestLot(const std::string& item, const SellBounds& b, int quantity) const
{
    // I3's backstop (doc 79, 7.6): the cheapest way to buy this many of the good anywhere now, stall-discounted and
    // priced flat at each market's pressure (no shop's ladder starts lower), live over every market; a good no shop
    // sets out, floor(catalogue × 0.80 × 0.9 × q). (A lot's price never falls as the factor rises, so the least factor's
    // lot is the least lot.) Not only as one lot: a lot is rounded (market::boughtLot), so q pieces bought
    // in smaller lots can cost less than one lot of q. The cheapest is the least over every split of q into lots, so no
    // sale of q pieces pays back what any way of buying them now cost. It never falls as q grows (a split of q + 1 less
    // a piece is a split of q), as stagedLeastLot needs.
    const auto each = double(catalogPrice(item));
    const int q = std::clamp(quantity, 1, 99);
    if (!std::isfinite(b.least))
        return std::int64_t(std::floor(each * market::FactorLow * market::StallShare * double(q) + 1e-9));
    return cheapestLots(item, b, q)[std::size_t(q)];
}

std::array<std::int64_t, 100> Society::cheapestLots(const std::string& item, const SellBounds& b, int upTo) const
{
    // cheapestLot for every quantity 1..upTo at once (the rest 0; market::leastSplits), so stagedLeastLot's halving reads
    // one table, not a table a step (Review A: it was the shop list's cost).
    std::array<std::int64_t, 100> least{};
    const auto each = double(catalogPrice(item));
    const int q = std::clamp(upTo, 1, 99);
    if (!std::isfinite(b.least))
    {
        for (int n = 1; n <= q; ++n)
            least[std::size_t(n)] = std::int64_t(std::floor(each * market::FactorLow * market::StallShare * double(n) + 1e-9));
        return least;
    }
    return market::leastSplits(each, b.least, q);
}

Society::LadderStart Society::ladderStart(const std::string& till, const std::string& item, const std::string& holder,
                                         bool atRest) const
{
    // What a ladder starts from (once a quote: the factor before pressure, the depth, P and the holder's push now).
    return ladderStartIn(marketOf(till), item, baseFactor(till, item), holder, atRest);
}

Society::LadderStart Society::ladderStartIn(const std::string& market, const std::string& item, double factor, const std::string& holder,
                                           bool atRest) const
{
    LadderStart s;
    s.market = market;
    s.base = items::baseOf(item);
    s.depth = depthOf(s.market, s.base);
    s.factor = factor;
    s.each = double(catalogPrice(item));
    s.P = atRest ? 0 : pressure(s.market, s.base);
    s.push = atRest ? 0 : pushedBy(s.market, holder, s.base);
    s.cap = marketRules().pushMost;
    return s;
}

Society::StagedLot Society::ladder(const LadderStart& start, int quantity, bool buy, bool stall) const
{
    StagedLot out;
    out.market = start.market;
    out.base = start.base;
    double P = start.P, push = start.push;
    out.P0 = P;
    const double dir = buy ? 1 : -1;
    double sum = 0;
    for (int i = 0; i < quantity; ++i)
    {
        sum += start.each * market::factor(start.factor, P);   // (Each piece at P after the pieces before it: 7.3.)
        const double room = start.cap - dir * push;  // (The holder's cap: past it, the capped P and no more pressure.)
        if (room <= 1e-12)
            continue;
        const double step = std::min(1. / start.depth, room);
        const double next = std::clamp(P + dir * step, -market::PressureMost, market::PressureMost);
        push += next - P;
        P = next;
    }
    out.sum = sum;
    out.P = P;
    out.push = push;
    if (buy)
        out.lot = market::boughtLot(sum, start.each, quantity, stall);
    else
        out.lot = std::int64_t(std::floor(market::SellShare * sum + 1e-9));
    return out;
}

Society::StagedLot Society::stagedLadder(const std::string& till, const std::string& item, int quantity, bool buy,
                                         const std::string& holder, bool stall, bool atRest) const
{
    return ladder(ladderStart(till, item, holder, atRest), quantity, buy, stall);
}

std::int64_t Society::budgetOf(const std::string& till) const
{
    // budget(shop) = 40p × the keeper's band × the slider's ×0.8 to ×1.2 (doc 79, 7.5): the band's `budget` in status.json
    // (destitute 0.4, poor 0.7, comfortable 1.0, rich 1.5), the comfortable 1.0 for a keeper not banded yet. The band
    // reads no player pressure; never raised by what players buy.
    double band = 1.0;
    if (till.rfind("till:", 0) == 0)
        if (const auto post = state_.careers.positions.find(till.substr(5)); post != state_.careers.positions.end() && !post->second.holder.empty())
            if (const auto* st = statusOf(post->second.holder))
                band = statusTable().rows[std::size_t(std::clamp(st->band, 0, status::Bands - 1))].budget;
    const double slider = 1 + .2 * std::clamp(state_.board.slider, -100, 100) / 100.;
    return std::max<std::int64_t>(0, std::llround(market::data().budget * band * slider));
}

bool Society::boardsDayIsToday() const
{
    // The day's counts on the Board (budgets left, pieces taken, pennies sold) are today's while its day is the market's,
    // and also while it is ahead of it: a clock behind the Board (a crash that kept the Board's morning but an hour
    // before dawn, a host's /time set back before dawn) is still the Board's day, never a fresh one with every cap
    // open; the morning comes when the clock passes the Board's day (rollMarket). Only a Board behind the clock (its
    // morning not run yet) reads as a new day.
    return state_.board.day >= marketDay();
}

std::int64_t Society::budgetLeft(const std::string& till) const
{
    const auto& b = state_.board;
    if (boardsDayIsToday())
        if (const auto left = b.budgetLeft.find(till); left != b.budgetLeft.end())
            return left->second;
    return budgetOf(till);
}

int Society::takeAllowance(const std::string& till, const std::string& item) const
{
    // Per shop per day (7.5): gathered and hunted raw goods at least 30 (or depth × takeAllowance where more), made goods
    // 6, a wearable or a sword 2. Raw by what the good is, never by whether some shop buys it (a pawnbroker's cookpots,
    // a maker's bought-in inputs are made goods): a material of the catalogue, a good from the land (items.json `source`:
    // fields, herds, mines, the shore, the hunt; not "service" or "import"), a good the wild gives (an animal's yield,
    // a forage find: the hunted and gathered), or the ore the smithies buy (D5).
    const auto base = items::baseOf(item);
    if (items::wearable(base) || base == "sword" || items::blade(base))
        return 2;
    static const std::set<std::string> hunted = [] {
        std::set<std::string> out;
        for (const auto& sp : wild::species())
            for (const auto& [good, _] : sp.yield)
                out.insert(items::baseOf(good));
        for (const auto& ground : wild::forage().ground)
            for (const auto& g : ground.goods)
                out.insert(items::baseOf(g.item));
        return out;
    }();
    bool raw = hunted.count(base) > 0;
    if (const auto* g = items::good(base))
    {
        if (g->category == "material")
            raw = true;
        for (const auto& from : g->source)
            if (from != "service" && from != "import")
                raw = true;
    }
    for (const auto& [_, list] : market::data().buys)
        if (std::find(list.begin(), list.end(), base) != list.end())
            raw = true;
    if (!raw)
        return 6;
    return std::max(30, int(std::ceil(depthOf(marketOf(till), base) * marketRules().takeAllowance - 1e-9)));
}

int Society::takenToday(const std::string& till, const std::string& item) const
{
    const auto& b = state_.board;
    if (!boardsDayIsToday())
        return 0;
    const auto t = b.takenToday.find(till);
    if (t == b.takenToday.end())
        return 0;
    const auto g = t->second.find(items::baseOf(item));
    return g == t->second.end() ? 0 : g->second;
}

std::int64_t Society::soldToday(const std::string& holder) const
{
    const auto& b = state_.board;
    if (!boardsDayIsToday())
        return 0;
    const auto s = b.soldToday.find(holder);
    return s == b.soldToday.end() ? 0 : s->second;
}

std::int64_t Society::discountRoom(const std::string& chapter) const
{
    const auto& b = state_.board;
    const auto week = marketDay() / 7;
    const auto most = marketRules().discountMost;
    if (b.week < week)                              // (A week ahead of a clock set back is still the Board's week.)
        return most;
    const auto paid = b.discountWeek.find(chapter);
    return std::max<std::int64_t>(0, most - (paid == b.discountWeek.end() ? 0 : paid->second));
}

std::int64_t Society::payFactionDiscount(const std::string& merchant, const std::string& player, const std::string& chapter,
                                        std::int64_t back)
{
    if (back <= 0)
        return 0;
    back = std::min(back, discountRoom(chapter));
    if (back <= 0 || !shift(merchant, player, "", 0, back, "faction discount"))
        return 0;
    discountPaid(chapter, back);
    return back;
}

void Society::discountPaid(const std::string& chapter, std::int64_t pennies)
{
    auto& b = state_.board;
    const auto week = marketDay() / 7;
    if (b.week < week)
    {
        b.week = week;
        b.discountWeek.clear();
    }
    b.discountWeek[chapter] += std::max<std::int64_t>(0, pennies);
}

std::int64_t Society::stagedSoldFor(const std::string& seller, const std::string& item, const LadderStart& start, const StagedLot& sold,
                                    int quantity, const SellBounds& bounds, std::int64_t cheapest) const
{
    // A sold lot (7.5, 7.6): the ladder's sum × 0.55 rounded down once; I2, at most 0.6 × the bought lot here now; I3,
    // at most 0.95 × the cheapest bought lot anywhere now; I5, the crafting cap. Each is non-decreasing in the quantity,
    // so their least is too (stagedLeastLot's search rests on it).
    const auto bought = ladder(start, quantity, true, atStall(seller)).lot;
    const auto least = cheapest >= 0 ? cheapest : cheapestLot(item, bounds, quantity);
    return std::max<std::int64_t>(0, std::min({sold.lot, std::int64_t(std::floor(market::OneShopShare * double(bought) + 1e-9)),
                                               std::int64_t(std::floor(market::BackstopShare * double(least) + 1e-9)),
                                               craftCap(bounds, quantity)}));
}

int Society::stagedLeastLot(const std::string& seller, const std::string& item, const LadderStart& start, int from,
                            const SellBounds& bounds) const
{
    // The smallest lot from `from` to 99 whose sold lot is worth a penny (0: none), by halving: the sold lot never falls
    // as the lot grows. I3's cheapest lots worked out once for the search.
    int lo = std::max(1, from), hi = 99;
    if (lo > hi)
        return 0;
    const auto cheapest = cheapestLots(item, bounds, hi);
    const auto worth = [&](int q) {
        return stagedSoldFor(seller, item, start, ladder(start, q, false, false), q, bounds, cheapest[std::size_t(q)]) >= 1;
    };
    if (!worth(hi))
        return 0;
    while (lo < hi)
    {
        const int mid = lo + (hi - lo) / 2;
        if (worth(mid))
            hi = mid;
        else
            lo = mid + 1;
    }
    return lo;
}

EconomyResult Society::stagedQuote(const std::string& player, const std::string& seller, const std::string& item, int quantity,
                                   bool buy, const std::string& holderIn, StagedLot* lotOut, const SellBounds* known,
                                   int* leastLot) const
{
    // The staged price (doc 79, 7.1-7.6): the per-piece ladder at the Board's pressure, inside the clamp; I1-I5; the
    // shop's budget, its take allowance and the holder's sell total; the till topped up at trade (never "can't afford").
    if (!state_.enabled || !merchant(seller) || !playerAccountId(player) || !account(player) || !account(seller))
        return {false, "This trader is unavailable."};
    if (quantity < 1 || quantity > 99)
        return {false, "Choose a whole quantity from 1 to 99."};
    const auto deals = wares(seller);
    if (!itemValid(item) || std::find(deals.begin(), deals.end(), items::baseOf(item)) == deals.end())
        return {false, "This trader has no use for those goods."};
    const auto till = tillOf(seller);
    if (!account(till))
        return {false, "This trader is unavailable."};
    const auto& m = *account(till);
    const auto& p = *account(player);
    const auto holder = holderIn.empty() ? player : holderIn;
    const auto each = catalogPrice(item);
    if (each > std::numeric_limits<std::int64_t>::max() / 2 / 99)
        return {false, "That trade exceeds safe account capacity.", each, 0};
    const bool stall = buy && atStall(seller);
    const auto bounds = buy ? SellBounds{} : known ? *known : sellBounds(item);
    const auto start = ladderStart(till, item, holder);
    const auto lot = ladder(start, quantity, buy, stall);
    std::array<std::int64_t, 100> cheapest{};
    if (!buy)
        cheapest = cheapestLots(item, bounds, quantity);   // (I3's lots of this many and of one, worked out once.)
    std::int64_t total = buy ? lot.lot : stagedSoldFor(seller, item, start, lot, quantity, bounds, cheapest[std::size_t(quantity)]);
    std::int64_t unit = total;
    if (quantity != 1)
    {
        const auto one = ladder(start, 1, buy, stall);
        unit = buy ? one.lot : stagedSoldFor(seller, item, start, one, 1, bounds, cheapest[1]);
    }
    if (lotOut)
    {
        *lotOut = lot;
        lotOut->lot = total;
    }
    if (!buy && total < 1)
    {
        // A lot worth 0p is refused, and moves nothing (7.3, 7.5); the smallest lot worth a penny named.
        const int least = stagedLeastLot(seller, item, start, quantity + 1, bounds);
        if (leastLot)
            *leastLot = least;
        return {false, least ? "Not worth the counting. I buy those by " + lotWords(least) + "." : "Not worth the counting.", unit, 0};
    }
    if (stock(buy ? m : p, item) < quantity)
        return {false, "There is not enough stock.", unit, total};
    if (buy && p.cash < total)
        return {false, "You cannot afford that purchase.", unit, total};
    if (!buy)
    {
        if (takenToday(till, item) + quantity > takeAllowance(till, item))
            return {false, "I've no room for more today.", unit, total};
        // (A lot past the whole day's budget is turned away in true words, at dawn as at dusk: "spent all" only once the
        // shop has bought something. Review A.)
        if (total > budgetOf(till))
            return {false, "That's more than I lay out in a day.", unit, total};
        if (total > budgetLeft(till))
            return {false, "I've spent all I'll spend today.", unit, total};
        if (soldToday(holder) + total > marketRules().sellMost)
            return {false, "No one here is buying more from you today.", unit, total};
    }
    if ((buy ? m : p).cash > MarketMoneyLimit - total || stock(buy ? p : m, item) > MarketStockLimit - quantity ||
        (!(buy ? p : m).stock.count(item) && (buy ? p : m).stock.size() >= MaxGoodsKinds))
        return {false, "That trade exceeds safe account capacity.", unit, total};
    return {true, "Available", unit, total};
}

bool Society::exchange(const std::string& seller, const std::string& buyer, const std::string& item, int quantity,
                       std::int64_t total, const std::string& kind)
{
    // The staged trade's one move (doc 79, 7.5): the goods and the lot's coins together, with any lot of at least 1p (a
    // dozen loaves for 10p), and one ledger entry as transfer's. transfer keeps its price < 1 refusal for every other caller.
    if (seller == buyer || quantity < 1 || quantity > 99 || !itemValid(item) || total < 1)
        return false;
    const auto* from = account(seller);
    const auto* to = account(buyer);
    if (!from || !to || stock(*from, item) < quantity || to->cash < total || from->cash > MarketMoneyLimit - total ||
        stock(*to, item) > MarketStockLimit - quantity || (!to->stock.count(item) && to->stock.size() >= MaxGoodsKinds))
        return false;
    auto& s = state_.accounts.at(seller);
    auto& b = state_.accounts.at(buyer);
    const int left = s.stock[item] -= quantity;
    if (left == 0 && !keptAtZero(item))
        s.stock.erase(item);
    b.stock[item] += quantity;
    b.cash -= total;
    s.cash += total;
    record(kind, buyer, seller, item, quantity, total);
    return true;
}

void Society::rollMarket()
{
    const auto day = marketDay();
    if (state_.board.day < 0)
        state_.board.day = day;
    else if (day > state_.board.day)
        marketMorning(day);
}

EconomyResult Society::stagedTrade(const std::string& player, const std::string& trader, const std::string& item, int quantity,
                                   bool buy, const std::string& holderIn)
{
    rollMarket();
    const auto holder = holderIn.empty() ? player : holderIn;
    StagedLot lot;
    auto result = stagedQuote(player, trader, item, quantity, buy, holder, &lot);
    if (!result.ok)
        return result;
    const auto till = tillOf(trader);
    if (!buy && cash(till) < result.total)
        stagedSet(till, result.total);             // (The till always has the cash: a raw set, 7.5.)
    const auto& seller = buy ? till : player;
    const auto& buyer = buy ? player : till;
    result.ok = exchange(seller, buyer, item, quantity, result.total, "local trade");
    if (!result.ok)
    {
        result.message = "The trade could not be completed.";
        return result;
    }
    // The shop's shelves moved (doc 79, Phase 6): its business counts a player's buying, and the till goes whole with the
    // Board in the same record, so a crash never puts back what a player bought, or loses what one sold.
    if (buy)
        noteSale(till, item, quantity, true);
    else
        noteTillMoved(till);
    // The Board (7.3): P after the lot's pieces, the holder's push, the day's net, the shop's budget and allowance.
    auto& b = state_.board;
    const double now = marketClock();
    auto& cell = b.cell[lot.market][lot.base];
    const double before = pressure(lot.market, lot.base);
    cell.P = lot.P;
    cell.touched = now;
    const double moved = lot.P - before;
    auto& push = b.pushed[lot.market][holder][lot.base];
    push = {lot.push, now};
    if (std::abs(push.amount) < 1e-6)
        b.pushed[lot.market][holder].erase(lot.base);
    // The cause is what moved the factor (11; the Phase 8 repair): the travellers' only where their pushes, together, are
    // most of P on its side, or where P was at rest or crossed over; a piece or two bought never takes over the move a
    // wagon, a ship or an event made. (Where it stays, a later read says "still" if the travellers turned it.)
    if (std::abs(moved) > 1e-9)
    {
        const bool theirs = cell.cause.empty() || cell.cause == "players" || cell.cause == "dump";
        double players = 0;
        if (!theirs)
            if (const auto pm = b.pushed.find(lot.market); pm != b.pushed.end())
                for (const auto& [who, goods] : pm->second)
                    if (goods.count(lot.base))
                        players += pushedBy(lot.market, who, lot.base);
        if (theirs || before * lot.P <= 0 || players * lot.P >= .5 * lot.P * lot.P)
            cell.cause = !buy && std::abs(moved) >= .1 ? "dump" : "players";
    }
    const std::string cause = !buy && std::abs(moved) >= .1 ? "dump" : "players";   // (This trade's own move, for the recent list.)
    if (std::abs(cell.P) < 1e-6)
        b.cell[lot.market].erase(lot.base);         // (`cell` is gone from here.)
    b.netToday[lot.market][lot.base][holder] += buy ? -quantity : quantity;
    if (!buy)
    {
        b.budgetLeft[till] = budgetLeft(till) - result.total;
        b.takenToday[till][lot.base] += quantity;
        b.soldToday[holder] += result.total;
    }
    if (std::abs(moved) >= .05)
    {
        b.recent.push_back({now, moved, lot.market, lot.base, cause});
        while (b.recent.size() > market::RecentKept)
            b.recent.pop_front();
    }
    result.message = std::string(buy ? "Bought " : "Sold ") + std::to_string(quantity) + " " + itemName(item) + " for " +
                     std::to_string(result.total) + " silver pennies.";
    return result;
}

Society::Listing Society::listing(const std::string& player, const std::string& merchantId, const std::string& item,
                                  const std::string& holderIn) const
{
    // The shop list's row (7.1): a good under 3p listed by the dozen (the viewer's own ladder and cap), the smallest sell
    // lot worth a penny, and ▲ or ▼ where the shown price differs by a penny or more from the price at P = 0.
    Listing l;
    const auto holder = holderIn.empty() ? player : holderIn;
    const auto till = tillOf(merchantId);
    const auto* shelf = account(till);
    l.listCount = catalogPrice(item) < 3 ? 12 : 1;
    const bool stall = atStall(merchantId);
    const auto shown = stagedLadder(till, item, l.listCount, true, holder, stall);
    l.listPrice = shown.lot;
    auto still = ladderStart(till, item, holder);
    still.P = still.push = 0;                       // (The same ladder at rest: P = 0.)
    const auto rest = ladder(still, l.listCount, true, stall);
    if (l.listPrice - rest.lot >= 1)
        l.trend = 1;
    else if (rest.lot - l.listPrice >= 1)
        l.trend = -1;
    if (l.trend != 0)
    {
        const auto base = items::baseOf(item);
        std::string cause = "players";
        if (const auto m = state_.board.cell.find(marketOf(till)); m != state_.board.cell.end())
            if (const auto c = m->second.find(base); c != m->second.end() && !c->second.cause.empty())
                cause = c->second.cause;
        l.cause = cause == "dump" ? "a glut sold in"
                  : cause == "ripple" ? (l.trend > 0 ? "dear since the want of what it's made into" : "cheap since the glut of what it's made from")
                  : cause == "players" ? (l.trend > 0 ? "travellers buying" : "travellers selling")
                  : cause.rfind("ship", 0) == 0 ? shipCause(state_.board, marketOf(till), base, cause, l.trend)
                  : cause == "event" ? eventCause(state_.board, marketOf(till), base, l.trend)
                                       : roadCause(cause, l.trend);
    }
    l.buyCount = std::max(1, std::min(l.listCount, shelf ? stock(*shelf, item) : 0));
    l.buyOne = stagedQuote(player, merchantId, item, 1, true, holder, nullptr);
    l.buy = l.buyCount == 1 ? l.buyOne : stagedQuote(player, merchantId, item, l.buyCount, true, holder, nullptr);
    // The smallest sell lot worth a penny: one, or (one worth nothing) the least found by halving, quoted once. I3's and
    // I5's bounds worked out once for the row.
    // (A piece worth nothing: its refusal found the least lot already, so the search runs once a row: Review A.)
    const auto bounds = sellBounds(item);
    l.sellLot = 0;
    int least = 0;
    l.sellOne = stagedQuote(player, merchantId, item, 1, false, holder, nullptr, &bounds, &least);
    if (l.sellOne.total >= 1 || l.sellOne.message.rfind("Not worth", 0) != 0)
    {
        l.sellLot = 1;                              // (Worth a penny, or refused for another reason: shown with it.)
        l.sell = l.sellOne;
    }
    else if (least)
    {
        l.sellLot = least;
        l.sell = stagedQuote(player, merchantId, item, least, false, holder, nullptr, &bounds);
    }
    else
        l.sell = {false, "Not worth the counting."};
    l.buyingToday = budgetLeft(till);
    return l;
}

std::string Society::priceWords(std::int64_t pennies, int listCount)
{
    // The shown price as it is said (doc 79, 11: "twelve for fourteen pence"; said "fourteen pence the dozen", which
    // fits any sentence a price fits): the list's lot, in words to a hundred.
    static const char* const Ones[] = {"no", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
                                       "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen",
                                       "eighteen", "nineteen"};
    static const char* const Tens[] = {"", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};
    const auto n = std::max<std::int64_t>(0, pennies);
    std::string words;
    if (n == 1)
        words = "a penny";
    else
    {
        if (n < 20)
            words = Ones[n];
        else if (n < 100)
            words = std::string(Tens[n / 10]) + (n % 10 ? std::string("-") + Ones[n % 10] : std::string());
        else if (n == 100)
            words = "a hundred";
        else
            words = std::to_string(n);
        words += " pence";
    }
    return listCount == 12 ? words + " the dozen" : listCount > 1 ? words + " for " + std::to_string(listCount) : words;
}

namespace
{
// A move's cause as talk says it (11): the Board's word turned the move's way, in the words section 11 gives ("travellers
// buying", "someone sold a heap", "the wagon's late", "the Gull came in", "a good harvest", the event's own words); a
// cause the move belies is "still" ("though the wagon came in"), so no line blames it.
void talkCause(const market::Board& b, const std::string& market, const std::string& base, const std::string& cause, int trend,
               std::string& kind, std::string& words, std::string& shipName)
{
    const bool up = trend > 0;
    const auto still = [&](const std::string& w) {
        kind = "still";
        words = "though " + w;
    };
    const auto set = [&](const char* k, const std::string& w) {
        kind = k;
        words = w;
    };
    if (cause.empty() || cause == "players")
        set("players", up ? "travellers buying" : "travellers selling");
    else if (cause == "dump")
        up ? still("someone sold a heap") : set("dump", "someone sold a heap");
    else if (cause == "ripple")
        set("ripple", up ? "the makers wanting more of it" : "plenty of what goes into it");
    else if (cause == "caravan")
        up ? still("the wagon came in") : set("caravan", "the wagon came in");
    else if (cause == "late")
        up ? set("late", "the wagon's late") : still("the wagon's late");
    else if (cause == "raid")
        up ? set("raid", "the wagon was robbed on the road") : still("the wagon was robbed on the road");
    else if (cause == "harvest")
        set("harvest", up ? "a short harvest" : "a good harvest");
    else if (cause.rfind("ship", 0) == 0)
    {
        // (The ship by name while the timetable has it, as the price list's shipCause.)
        const bool takes = cause == "ship takes", late = cause == "ship late";
        const market::Ship* ship = nullptr;
        for (const auto& s : b.ships)
        {
            if (s.port != market || (late ? s.status != "storm" : s.status != "in" && s.status != "sailed"))
                continue;
            const auto& goods = takes ? s.takes : s.brings;
            if (std::any_of(goods.begin(), goods.end(), [&](const auto& g) { return g.first == base; }) && (!ship || s.day > ship->day))
                ship = &s;
        }
        const std::string name = ship ? ship->name : "a ship";
        shipName = ship ? ship->name : std::string();
        if (late && ship && ship->lost)
            up ? set("ship_lost", name + " lost at sea") : still(name + " was lost at sea");
        else if (late)
            up ? set("ship_late", name + " stormbound") : still(name + " is stormbound");
        else if (takes)
            up ? set("ship_takes", name + " buying") : still(name + " bought a load");
        else
            up ? still(name + " came in") : set("ship", name + " came in");
    }
    else if (cause == "event")
    {
        const market::StagedEvent* event = nullptr;
        for (const auto& e : b.events)
            if (e.market == market && (e.status == "on" || e.status == "over"))
                for (const auto& [g, target] : e.goods)
                    if (g == base && (!event || (e.status == "on") > (event->status == "on") ||
                                      ((e.status == "on") == (event->status == "on") && e.day >= event->day)))
                        event = &e;
        double target = 0;
        if (event)
            for (const auto& [g, t] : event->goods)
                if (g == base)
                    target = t;
        if (!event)
            set("players", up ? "travellers buying" : "travellers selling");   // (Its event is gone: P is the rest.)
        else if ((target > 0) == up)
            set("event", event->words);
        else
            still(event->words);
    }
    else
        set("players", up ? "travellers buying" : "travellers selling");
}
} // namespace

Society::PriceMove Society::priceMove(const std::string& till, const std::string& item, const std::string& holder) const
{
    PriceMove m;
    m.base = items::baseOf(item);
    m.till = till;
    m.item = item;
    m.listCount = catalogPrice(item) < 3 ? 12 : 1;
    std::string keeper;
    if (till.rfind("till:", 0) == 0)
        if (const auto post = state_.careers.positions.find(till.substr(5)); post != state_.careers.positions.end())
            keeper = post->second.holder;
    const bool stall = !keeper.empty() && atStall(keeper);
    // (The same ladders as the shop list's row, so the price said is the price shown: listing above.)
    m.shown = stagedLadder(till, item, m.listCount, true, holder, stall).lot;
    auto still = ladderStart(till, item, holder);
    still.P = still.push = 0;
    m.rest = ladder(still, m.listCount, true, stall).lot;
    m.trend = m.shown - m.rest >= 1 ? 1 : m.rest - m.shown >= 1 ? -1 : 0;
    if (m.trend == 0)
        return m;
    const auto market = marketOf(till);
    std::string cause;
    if (const auto c = state_.board.cell.find(market); c != state_.board.cell.end())
        if (const auto g = c->second.find(m.base); g != c->second.end())
            cause = g->second.cause;
    talkCause(state_.board, market, m.base, cause, m.trend, m.cause, m.causeWords, m.ship);
    // Who pushed it most the move's way (a players' move: talk's price_cause names them as the speaker knows them).
    if (m.cause == "players" || m.cause == "dump")
        if (const auto pm = state_.board.pushed.find(market); pm != state_.board.pushed.end())
        {
            double most = .05;
            for (const auto& [who, goods] : pm->second)
                if (goods.count(m.base))
                    if (const double p = pushedBy(market, who, m.base) * m.trend; p > most)
                        most = p, m.pusher = who;
        }
    return m;
}

std::vector<Society::PriceMove> Society::priceMoves(const std::string& market, const std::string& holder, std::size_t most,
                                                    const std::function<int(const std::string& till)>& nearer) const
{
    std::vector<PriceMove> out;
    const auto cells = state_.board.cell.find(market);
    const auto* m = tradePoints().find(market);
    if (cells == state_.board.cell.end() || !m || most == 0)
        return out;
    // Each good's shops in the market (those that set it out, by id), found once while the places and markets stand.
    if (setOutRevision_ != placesRevision())
    {
        setOutCache_.clear();
        setOutRevision_ = placesRevision();
    }
    auto cached = setOutCache_.find(market);
    if (cached == setOutCache_.end())
    {
        cached = setOutCache_.emplace(market, std::map<std::string, std::vector<std::pair<std::string, std::string>>>{}).first;
        auto tills = m->tills;
        std::sort(tills.begin(), tills.end());
        for (const auto& till : tills)
            if (const auto* p = till.rfind("till:", 0) == 0 ? position(till.substr(5)) : nullptr)
                for (const auto& ware : stagedShelfGoods(p->founder))
                {
                    auto& list = cached->second[items::baseOf(ware)];
                    // (A shop's plain ware before its fine one: the price said is the plain one's where it sells both.)
                    if (list.empty() || list.back().first != till)
                        list.push_back({till, ware});
                    else if (ware == items::baseOf(ware))
                        list.back().second = ware;
                }
    }
    for (const auto& [base, cell] : cells->second)
    {
        if (std::abs(pressure(market, base)) < 1e-3)
            continue;
        const auto shops = cached->second.find(base);
        if (shops == cached->second.end() || shops->second.empty())
            continue;
        // The one shop the speaker would quote, moved or not: the nearest (its own, its cell's), else the first by id.
        const auto* shop = &shops->second.front();
        int best = nearer ? nearer(shop->first) : 0;
        if (nearer)
            for (const auto& t : shops->second)
                if (const int r = nearer(t.first); r < best)
                    best = r, shop = &t;
        if (auto move = priceMove(shop->first, shop->second, holder); move.trend != 0)
            out.push_back(std::move(move));
    }
    // (The largest in shown pennies first; the map's order, by good, between equals.)
    std::stable_sort(out.begin(), out.end(), [](const PriceMove& x, const PriceMove& y) {
        return std::abs(x.shown - x.rest) > std::abs(y.shown - y.rest);
    });
    if (out.size() > most)
        out.resize(most);
    return out;
}

void Society::marketTick(double absoluteDay)
{
    marketAt_ = absoluteDay;
    rollMarket();
}

void Society::marketMorning(std::int64_t day)
{
    // The dawn's turn (doc 79, 7.3-7.5): yesterday's net positions ripple through the recipes into this morning's
    // prices, at dawn before any keeper opens, the same moment for every market (P is a market's, shared by its shops);
    // each shop's keeper is then seen working the new stock at the morning restock (stagedRestock, rippled_); the day's
    // counts reset; the Board swept; the slider a step toward the DM's.
    auto& b = state_.board;
    const auto& rules = marketRules();
    const double now = marketClock();
    rippled_.clear();
    rippledDay_ = day;
    for (const auto& [m, goods] : b.netToday)
        for (const auto& [good, holders] : goods)
        {
            // The market's net over all holders (7.4): pieces sold minus pieces bought. Churn (one holder selling what
            // another buys, or one buying back what it sold) nets to nothing and ripples nothing.
            long long total = 0;
            for (const auto& [holder, net] : holders)
                total += net;
            if (total == 0)
                continue;
            const bool glut = total > 0;
            // Each holder's share of it: those whose own net is on its side, by their nets; each holder's part of the
            // P its pieces moved at most its cap (a lone holder's ripple as its own pieces made it).
            long long side = 0;
            for (const auto& [holder, net] : holders)
                if ((net > 0) == glut && net != 0)
                    side += glut ? net : -net;
            const double depth = depthOf(m, good);
            std::vector<std::pair<std::string, double>> parts;   // Holder -> its part of the market's delta (signed).
            for (const auto& [holder, net] : holders)
                if ((net > 0) == glut && net != 0 && side > 0)
                {
                    const double share = double(glut ? net : -net) / double(side);
                    const double part = std::min(std::abs(double(total)) * share / depth, rules.pushMost);
                    parts.push_back({holder, glut ? part : -part});
                }
            for (const auto& [target, step] : market::rippleTargets(good, glut))
            {
                double weight = std::pow(rules.ripple, step);
                if (!glut)                          // (A want upstream, weighted by value: 7.4.)
                    weight *= std::min(1., double(catalogPrice(good)) / double(std::max<std::int64_t>(1, catalogPrice(target))));
                // Against each holder's own cap on that good, in proportion to its share of the net.
                const double halfLife = market::halfLife(target, rules);
                std::vector<std::pair<std::string, double>> charged;
                double r = 0;
                for (const auto& [holder, part] : parts)
                {
                    const auto& pushedTo = b.pushed[m][holder];
                    const auto at = pushedTo.find(target);
                    const double pushed = at == pushedTo.end() ? 0 : market::decayed(at->second.amount, at->second.touched, now, halfLife);
                    const double want = -part * weight;   // (A glut lowers its targets: part > 0 sold, so -.)
                    const double room = std::max(0., rules.pushMost - (want > 0 ? pushed : -pushed));
                    const double one = want > 0 ? std::min(want, room) : -std::min(-want, room);
                    if (std::abs(one) >= 1e-9)
                    {
                        charged.push_back({holder, one});
                        r += one;
                    }
                }
                if (std::abs(r) < 1e-9)
                    continue;
                auto& c = b.cell[m][target];
                const double P = market::decayed(c.P, c.touched, now, halfLife);
                double next = P + r;
                // Ripple-caused |P| at most RippleMost: never pushed past it by a ripple (a player's own may be).
                if (std::abs(next) > market::RippleMost && std::abs(next) > std::abs(P))
                    next = (next > 0 ? 1 : -1) * std::max(std::abs(P), market::RippleMost);
                next = std::clamp(next, -market::PressureMost, market::PressureMost);
                // The cause is the ripple's only where it leaves P on its own side (a want's dearer, a glut's cheaper): one
                // that only eats into a move the other way leaves that move's cause, which still sets the arrow (7.3, 11:
                // the cause is what actually moved the factor; Review A).
                // (And, as a player's trade, only where the ripple is most of P or P was at rest: a little ripple never
                // takes over a wagon's move. Phase 8 repair.)
                const bool itsOwn = ((next > 0 && r > 0) || (next < 0 && r < 0)) &&
                                    (c.cause.empty() || c.cause == "ripple" || P * next <= 0 || std::abs(next - P) >= .5 * std::abs(next));
                c = {next, now, itsOwn ? std::string("ripple") : c.cause};
                rippled_[m].insert(target);
                // Each holder charged its share of what the ripple moved (none past the cap's truncation).
                const double moved = next - P, scale = std::clamp(moved / r, 0., 1.);
                for (const auto& [holder, one] : charged)
                {
                    auto& push = b.pushed[m][holder][target];
                    push = {market::decayed(push.amount, push.touched, now, halfLife) + one * scale, now};
                }
                if (std::abs(moved) >= .02)
                {
                    b.recent.push_back({now, moved, m, target, "ripple"});
                    while (b.recent.size() > market::RecentKept)
                        b.recent.pop_front();
                }
            }
        }
    b.netToday.clear();
    b.takenToday.clear();
    b.soldToday.clear();
    b.budgetLeft.clear();
    // The sweep: every entry decayed to now, those under 0.005 dropped (lazy reads and the sweep agree: decayed()).
    for (auto mi = b.cell.begin(); mi != b.cell.end();)
    {
        for (auto gi = mi->second.begin(); gi != mi->second.end();)
        {
            gi->second.P = market::decayed(gi->second.P, gi->second.touched, now, market::halfLife(gi->first, rules));
            gi->second.touched = now;
            gi = std::abs(gi->second.P) < .005 ? mi->second.erase(gi) : std::next(gi);
        }
        mi = mi->second.empty() ? b.cell.erase(mi) : std::next(mi);
    }
    for (auto mi = b.pushed.begin(); mi != b.pushed.end();)
    {
        for (auto hi = mi->second.begin(); hi != mi->second.end();)
        {
            for (auto gi = hi->second.begin(); gi != hi->second.end();)
            {
                gi->second.amount = market::decayed(gi->second.amount, gi->second.touched, now, market::halfLife(gi->first, rules));
                gi->second.touched = now;
                gi = std::abs(gi->second.amount) < .005 ? hi->second.erase(gi) : std::next(gi);
            }
            hi = hi->second.empty() ? mi->second.erase(hi) : std::next(hi);
        }
        mi = mi->second.empty() ? b.pushed.erase(mi) : std::next(mi);
    }
    for (auto it = b.restocked.begin(); it != b.restocked.end();)
        it = it->second < day - 1 ? b.restocked.erase(it) : std::next(it);
    // The back rooms run down by a fifth a day (D27: what an arrival left goes, eaten, spoilt or sold on); a till the
    // markets no longer know is dropped.
    for (auto it = b.backRoom.begin(); it != b.backRoom.end();)
    {
        if (!account(it->first))
        {
            it = b.backRoom.erase(it);
            continue;
        }
        for (auto& [good, n] : it->second)
            n = n * 4 / 5;
        ++it;
    }
    for (auto it = b.settingOut.begin(); it != b.settingOut.end();)
        it = it->second < now ? b.settingOut.erase(it) : std::next(it);
    // (When wagons last brought each import: two months' worth, a market the Board no longer knows dropped.)
    for (auto mi = b.broughtIn.begin(); mi != b.broughtIn.end();)
    {
        for (auto gi = mi->second.begin(); gi != mi->second.end();)
            gi = gi->second < day - market::BroughtInDays ? mi->second.erase(gi) : std::next(gi);
        mi = mi->second.empty() || !tradePoints().find(mi->first) ? b.broughtIn.erase(mi) : std::next(mi);
    }
    stepSlider(day);
    while (b.recent.size() > market::RecentKept)
        b.recent.pop_front();
    b.day = day;
}

int Society::shelfTarget(const std::string& till, const std::string& ware) const
{
    // shelf(g) × abundance (the season's shelf) × (1 − 0.5 × max(P, 0)) × shelfScale (doc 79, 7.7), plus the day's food
    // provision the households it feeds take (provisionFor: "targets cover the day's staged food provision as well as
    // players"). shelf(g): market.json's `shelves` (moved from city_loop.json's), else the starting stock (4; swords 3);
    // the season's shelf from market.json's groups.
    const auto base = items::baseOf(ware);
    const auto& shelves = market::data().shelves;
    const auto own = shelves.find(base);
    const int shelf = own != shelves.end() ? own->second : base == "sword" ? SmithSwords : GoodsKept;
    std::array<int, 4> by{};
    const double abundance = yearDay_ >= 0 && stagedSeasonGroup(base, nullptr, &by) ? market::seasonal(by, yearDay_) : 1.;
    const double P = std::max(0., pressure(marketOf(till), base));
    const double want = double(shelf) * abundance * (1 - .5 * P) * marketRules().shelfScale + provisionFor(till, ware);
    return shelf <= 0 ? 0 : std::clamp(int(std::ceil(want - 1e-9)), 1, ShelfMost);
}

std::vector<std::string> Society::stagedShelfGoods(const std::string& founder) const
{
    // What a shop sets out (not what it only buys from players): its wares less its buys, unless it makes or supplies them.
    std::vector<std::string> out;
    const auto* r = spec(founder);
    const auto* business = r ? items::businessFor(r->workLabel) : nullptr;
    std::set<std::string> buys, makes;
    if (business)
    {
        for (const auto& g : items::buysFor(business->id))
            buys.insert(g);
        if (const auto extra = market::data().buys.find(business->id); extra != market::data().buys.end())
            buys.insert(extra->second.begin(), extra->second.end());
        for (const auto* craft : items::craftsFor(business->id))
            for (const auto& [made, _] : craft->out)
                makes.insert(made);
        for (const auto& g : items::suppliesFor(business->id))
            makes.insert(g);
    }
    for (const auto& ware : wares(founder))
        if (!buys.count(ware) || makes.count(ware))
            out.push_back(ware);
    return out;
}

bool Society::settingOut(const std::string& keeper) const
{
    // Still setting out the wares from this morning's restock (7.7): what the keeper's activity reads.
    const auto found = state_.board.settingOut.find(tillOf(keeper));
    return found != state_.board.settingOut.end() && marketClock() <= found->second;
}

bool Society::stagedRestock(const std::string& keeper)
{
    // The morning restock (doc 79, 7.7): when its keeper is first at work in the market day, a shop sets out its wares up
    // to their shelf targets: no inputs, no money, no store room; a raw stock change (no ledger entry). A good its market
    // gets only by wagon or cart (imported: D27) comes from the till's back room, never more than it holds (a back room
    // never stocked yet is stocked now, as by an arrival: a world's first morning); every other good refills, as in a
    // market no wagon reaches.
    rollMarket();
    const auto* job = jobOf(keeper);
    if (!job || job->role != "merchant")
        return false;
    const auto till = tillOf(keeper);
    auto found = state_.accounts.find(till);
    if (found == state_.accounts.end() || till == keeper)
        return false;
    const auto day = marketDay();
    // (Once a market day; a restock on a day ahead of a clock set back is not done again for the day before.)
    if (const auto last = state_.board.restocked.find(till); last != state_.board.restocked.end() && last->second >= day)
        return false;
    state_.board.restocked[till] = day;
    auto& a = found->second;
    bool set = false;
    const auto market = marketOf(till);
    for (const auto& ware : stagedShelfGoods(job->founder))
    {
        const int target = shelfTarget(till, ware), have = stock(a, ware);
        if (target <= have || !itemValid(ware) || (!a.stock.count(ware) && a.stock.size() >= MaxGoodsKinds))
            continue;
        int next = std::min(MarketStockLimit, target);
        if (const auto base = items::baseOf(ware); imported(market, base))
        {
            auto& rooms = state_.board.backRoom[till];
            const auto room = rooms.try_emplace(base, target).first;   // (Never stocked yet: as by an arrival.)
            const int take = std::min(room->second, next - have);
            if (take <= 0)
                continue;
            room->second -= take;
            next = have + take;
        }
        a.stock[ware] = next;
        set = true;
        tillsMoved_.insert(till);                   // (Journalled with the Board's side of it: its day, its back room.)
    }
    // The keeper is seen "setting out the wares" for a few minutes (7.7) when anything went out, or when the dawn rippled
    // a good the shop sets out (7.4: the smith works the new ore, though its shelf of bars was full).
    if (!set && rippledDay_ == day)
        if (const auto r = rippled_.find(marketOf(till)); r != rippled_.end())
            for (const auto& ware : stagedShelfGoods(job->founder))
                if (r->second.count(items::baseOf(ware)))
                {
                    set = true;
                    break;
                }
    if (set)
        state_.board.settingOut[till] = marketClock() + market::SettingOutDays;
    return set;
}
} // namespace ratw

// ===================================================================================================== The world's

namespace ratw
{
void World::ensureTradePoints()
{
    // The markets (doc 79, 5.1), found again at each load, whenever the roster moves, and whenever a dead end is found or
    // forgotten (the routes between them and to their homes leave dead ends out): a trade point for each
    // community (a region with homes) that has a merchant post, its id the region's; a merchant outside every community
    // joins the nearest market by path, or one "outside" market; each market's k nearest by path.
    // The Board feeds the bands (doc 79, 5 and 6.5): its Times, its pressure and its shelves, in play and in sims, so the
    // poor go short when players empty a shelf. Once; a test's own hooks are kept.
    if (!society_.stagedHooked())
        society_.setStagedHooks(Society::boardHooks());
    // The street's (doc 79, Phase 6): whether a player watches a till (7.7), and a household's errand-runner sent to one
    // (6.6). They point at this world, so they are set again if it has moved.
    if (streetHooked_ != this)
    {
        streetHooked_ = this;
        society_.setStreetHooks({[this](const Society&, const std::string& till) { return tillWatched(till); },
                                 [this](Society&, const std::string& household, std::string& till) { return sendRunner(household, till); },
                                 [this](Society&, const std::string& household, const std::string& till, const std::string& ware) {
                                     return sendBuyer(household, till, ware);
                                 }});
    }
    if (tradePointsReady_ && tradePointsRevision_ == society_.placesRevision() && tradePointsDeadEnds_ == deadEndsRevision_)
        return;
    const bool wasReady = tradePointsReady_;
    tradePointsReady_ = true;                       // (Before the households are asked for, as before.)
    tradePointsRevision_ = society_.placesRevision();
    tradePointsDeadEnds_ = deadEndsRevision_;
    const auto regionOf = [&](const std::string& cellId) -> std::string {
        const auto* c = cell(cellId);
        return c && !c->region.empty() && c->region != "unassigned" ? c->region : std::string();
    };
    // What they are found from (the repair's check): the households and the regions of their homes, the merchants' posts,
    // their tills and trades, the farm crews and the map's regions. A move of the roster that changes none of it (a post
    // changing hands, an account closed: most of them) finds them as they were, and a rebuild with cold routes cost up to
    // 25 ms (6:16 on DEV, the market-to-market routes); so only a change of these, or of the dead ends, finds them again.
    const auto households = society_.households(true);
    std::uint64_t print = 1469598103934665603ULL;
    const auto mix = [&](const std::string& text) {
        for (const unsigned char ch : text)
            print = (print ^ ch) * 1099511628211ULL;
        print = (print ^ 0xff) * 1099511628211ULL;
    };
    for (const auto& [key, members] : households)
        if (!members.empty())
            if (const auto* life = society_.resident(members.front()))
                mix(life->homeCell), mix(regionOf(life->homeCell));
    for (const auto& p : society_.positions())
        if (p.role == "merchant")
        {
            mix(p.id), mix(p.work.cell), mix(p.founder);
            mix(society_.account("till:" + p.id) ? "1" : "0");
            if (const auto* r = society_.spec(p.founder))
                mix(r->workLabel);
            for (const auto& ware : society_.stagedShelfGoods(p.founder))
                mix(ware);
        }
    for (const auto& [community, n] : society_.farmCrews())
        mix(community), mix(std::to_string(n));
    for (const auto& [id, c] : cells_)
        mix(id), mix(c.region);
    mix(regionOf(spawnCell_)), mix(std::to_string(deadEndsRevision_));
    const bool same = wasReady && print == tradePointsPrint_;
    tradePointsPrint_ = print;
    if (same)
    {
        tradePointsRevision_ = society_.placesRevision();   // (As it stands after the households were asked for.)
        return;
    }
    ++tradePointsBuilt_;                            // (The roads find their markets again: ensureRoadMarkets.)
    std::map<std::string, int> homes;
    std::map<std::string, std::string> homeCellIn;  // Region -> a home cell in it (the first household's).
    for (const auto& [key, members] : households)
    {
        if (members.empty())
            continue;
        const auto* life = society_.resident(members.front());
        if (life)
            if (const auto region = regionOf(life->homeCell); !region.empty())
            {
                ++homes[region];
                homeCellIn.emplace(region, life->homeCell);
            }
    }
    struct Shop
    {
        const Position* post;
        std::string till, region;
    };
    std::vector<Shop> shops;
    std::map<std::string, std::map<std::string, int>> merchantsAt;   // Region -> cell -> merchants.
    for (const auto& p : society_.positions())
        if (p.role == "merchant")
            if (const auto till = "till:" + p.id; society_.account(till))
            {
                const auto region = regionOf(p.work.cell);
                shops.push_back({&p, till, region});
                if (!region.empty())
                    ++merchantsAt[region][p.work.cell];
            }
    // Which regions are communities: where the world has the Country's communities (communities.json), those and the
    // city (the region with the most households); a farmstead out in the wilds is no market, its merchants lone ones.
    // A world without them (a host's own map, a test's): every region with homes.
    std::set<std::string> communities;
    for (const auto& c : items::farmPeople().communities)
        if (homes.count(c.region))
            communities.insert(c.region);
    if (!communities.empty() && !homes.empty())
    {
        communities.insert(std::max_element(homes.begin(), homes.end(), [](const auto& a, const auto& b) { return a.second < b.second; })->first);
        if (const auto spawn = regionOf(spawnCell_); !spawn.empty())
            communities.insert(spawn);              // (Where people arrive: the capital.)
    }
    market::Topology t;
    for (const auto& [region, cells] : merchantsAt)
    {
        if (!homes.count(region) || (!communities.empty() && !communities.count(region)))
            continue;
        market::TradePoint m;
        m.id = region;
        m.place = std::max_element(cells.begin(), cells.end(), [](const auto& a, const auto& b) { return a.second < b.second; })->first;
        m.catchment = double(homes[region]) + market::PassingTrade;   // (Its farm and works crews below.)
        t.markets.push_back(std::move(m));
    }
    const auto marketIndex = [&](const std::string& id) -> int {
        for (std::size_t i = 0; i < t.markets.size(); ++i)
            if (t.markets[i].id == id)
                return int(i);
        return -1;
    };
    for (const auto& s : shops)
    {
        int at = marketIndex(s.region);
        if (at < 0)
        {
            // A lone merchant: the nearest market by path, else "outside" with the depth floor.
            std::size_t best = std::numeric_limits<std::size_t>::max();
            for (std::size_t i = 0; i < t.markets.size(); ++i)
                if (const auto route = routeBetween(s.post->work.cell, t.markets[i].place); route.size() >= 1 && route.size() < best)
                    best = route.size(), at = int(i);
            if (at < 0)
            {
                at = marketIndex("outside");
                if (at < 0)
                {
                    market::TradePoint outside;
                    outside.id = "outside";
                    outside.place = s.post->work.cell;
                    t.markets.push_back(std::move(outside));
                    at = int(t.markets.size() - 1);
                }
            }
        }
        auto& m = t.markets[std::size_t(at)];
        m.tills.push_back(s.till);
        for (const auto& ware : society_.stagedShelfGoods(s.post->founder))
            m.sold.insert(items::baseOf(ware));
        if (const auto* r = society_.spec(s.post->founder))
            if (const auto* business = items::businessFor(r->workLabel))
            {
                market::madeBy(business->id, m.made);
                market::craftedBy(business->id, m.crafted);
                for (const auto& g : items::buysFor(business->id))   // (What its shops buy: a wagon may bring it too.)
                    m.bought.insert(items::baseOf(g));
                if (const auto extra = market::data().buys.find(business->id); extra != market::data().buys.end())
                    for (const auto& g : extra->second)
                        m.bought.insert(items::baseOf(g));
            }
    }
    // Its areas' goods: the farms and works whose community it is (communities.json), where the world has them.
    for (const auto& f : items::farmPeople().farms)
    {
        const int at = marketIndex(f.community);
        if (at < 0 || f.workplaces.empty() || !cells_.count(f.workplaces.front().area))
            continue;
        for (const auto& area : items::farms().areas)
            for (const auto& def : area.farms)
                if (def.id == f.farm)
                    for (const auto& good : def.yields)
                    {
                        t.markets[std::size_t(at)].made.insert(good);
                        t.markets[std::size_t(at)].crafted.insert(good);
                    }
    }
    // Its farm and works crews (7.3).
    for (const auto& [community, n] : society_.farmCrews())
        if (const int at = marketIndex(community); at >= 0)
            t.markets[std::size_t(at)].catchment += n;
    // Where households live: a market's own region's cells, and a region of homes that is no market (a farmstead, a
    // hamlet) the nearest market's by path, as a lone merchant's, so a household's market (Society::marketOfHome) is the
    // Board's own, whatever the towns (lawTown) say.
    std::map<std::string, int> served;               // Region -> market.
    for (const auto& [region, home] : homeCellIn)
    {
        if (const int at = marketIndex(region); at >= 0)
            continue;
        std::size_t best = std::numeric_limits<std::size_t>::max();
        int at = -1;
        for (std::size_t i = 0; i < t.markets.size(); ++i)
            if (t.markets[i].id != "outside")
                if (const auto route = routeBetween(home, t.markets[i].place); route.size() >= 1 && route.size() < best)
                    best = route.size(), at = int(i);
        if (at >= 0)
            served[region] = at;
    }
    for (const auto& [id, c] : cells_)
        if (const int at = marketIndex(c.region); at >= 0)
            t.markets[std::size_t(at)].cells.insert(id);
        else if (const auto s = served.find(c.region); s != served.end())
            t.markets[std::size_t(s->second)].cells.insert(id);
    // Routes: each market's k nearest others by path (pairs with no path left out quietly).
    const auto k = std::size_t(market::data().routes);
    for (auto& m : t.markets)
    {
        std::vector<std::pair<std::size_t, std::string>> near;
        for (const auto& o : t.markets)
            if (o.id != m.id && o.id != "outside" && m.id != "outside")
                if (const auto route = routeBetween(m.place, o.place); route.size() >= 2)
                    near.push_back({route.size(), o.id});
        std::sort(near.begin(), near.end());
        for (std::size_t i = 0; i < near.size() && i < k; ++i)
            m.routes.push_back(near[i].second);
    }
    society_.setTradePoints(std::move(t));
    tradePointsRevision_ = society_.placesRevision();   // (As it stands after the households were asked for.)
}
} // namespace ratw
