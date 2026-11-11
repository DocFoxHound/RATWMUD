// NPC economic status (Docs/Design/79-a-staged-economy.md, section 6, Phase 2): the table and the cut's arithmetic.
#include "RatwStatus.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <sstream>

namespace ratw::status
{
namespace
{
const char* const Names[Bands] = {"destitute", "poor", "comfortable", "rich"};

double clampNumber(const json::Value& v, const char* key, double fallback, double low, double high, const std::string& where,
                   std::vector<std::string>* warnings)
{
    const auto* found = v.find(key);
    if (!found)
        return fallback;
    if (!found->isNumber() || !std::isfinite(found->asNumber()))
    {
        if (warnings)
            warnings->push_back(where + key + " is not a number: kept " + std::to_string(fallback));
        return fallback;
    }
    const double n = found->asNumber();
    if (n < low || n > high)
    {
        if (warnings)
            warnings->push_back(where + key + " out of range: clamped");
        return std::clamp(n, low, high);
    }
    return n;
}

// A [low, high] pair of pennies, each clamped, low <= high.
void pair(const json::Value& v, const char* key, std::int64_t& low, std::int64_t& high, std::int64_t most, const std::string& where,
          std::vector<std::string>* warnings)
{
    const auto* found = v.find(key);
    if (!found)
        return;
    const auto& items = found->items();
    if (!found->isArray() || items.size() != 2 || !items[0].isNumber() || !items[1].isNumber() ||
        !std::isfinite(items[0].asNumber()) || !std::isfinite(items[1].asNumber()))
    {
        if (warnings)
            warnings->push_back(where + key + " is not a pair of numbers: kept");
        return;
    }
    auto a = std::int64_t(std::clamp(items[0].asNumber(), 0., double(most)));
    auto b = std::int64_t(std::clamp(items[1].asNumber(), 0., double(most)));
    if (a > b)
        std::swap(a, b);
    if ((warnings && double(a) != items[0].asNumber()) || (warnings && double(b) != items[1].asNumber()))
        warnings->push_back(where + key + " clamped to " + std::to_string(a) + "-" + std::to_string(b));
    low = a, high = b;
}

std::vector<std::string> words(const json::Value& v)
{
    std::vector<std::string> out;
    for (const auto& w : v.items())
        if (w.isString() && !w.asString().empty() && w.asString().size() <= 80 && out.size() < 32)
            out.push_back(w.asString());
    return out;
}

void numbers(const json::Value& v, std::map<std::string, double>& into, double low, double high, const std::string& where,
             std::vector<std::string>* warnings)
{
    for (const auto& [key, n] : v.fields())
        if (key.size() <= 40 && n.isNumber() && std::isfinite(n.asNumber()))
            into[key] = std::clamp(n.asNumber(), low, high);
        else if (warnings)
            warnings->push_back(where + key + " ignored");
}

std::filesystem::path statusFile()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
        return fs::path(dir) / "Economy" / "status.json";
    for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
    {
        if (fs::exists(at / "Data" / "Economy" / "status.json", ec))
            return at / "Data" / "Economy" / "status.json";
        if (at == at.parent_path())
            break;
    }
#ifdef RATW_SOURCE_DIR
    return fs::path(RATW_SOURCE_DIR) / "Data" / "Economy" / "status.json";
#else
    return fs::path("Data") / "Economy" / "status.json";
#endif
}
} // namespace

const char* bandName(int band)
{
    return band >= 0 && band < Bands ? Names[band] : Names[Comfortable];
}

int bandOf(const std::string& word)
{
    for (int b = 0; b < Bands; ++b)
        if (word == Names[b])
            return b;
    return -1;
}

std::uint64_t hash(const std::string& words)
{
    std::uint64_t h = 1469598103934665603ULL;
    for (const unsigned char c : words)
        h = (h ^ c) * 1099511628211ULL;
    return h;
}

double lot(const std::string& words)
{
    return double(hash(words) % 1000003ULL) / 1000003.;
}

double fortune(const Table& t, const std::string& household, std::int64_t day)
{
    const std::int64_t week = day >= 0 ? day / 7 : (day - 6) / 7;
    const double into = double(day - week * 7) / 7;
    const auto noise = [&](std::int64_t w) { return t.noise * (2 * lot(household + "|noise|" + std::to_string(w)) - 1); };
    return t.lot * (2 * lot(household + "|fortune") - 1) + noise(week) * (1 - into) + noise(week + 1) * into;
}

std::int64_t Table::cap(const std::string& name) const
{
    const auto found = caps.find(name);
    return found == caps.end() ? 0 : found->second;
}

double Table::event(const std::string& name) const
{
    const auto found = events.find(name);
    return found == events.end() ? 0 : found->second;
}

Table builtIn()
{
    Table t;
    t.shares = {{-100, {18, 45, 32, 5}}, {0, {5, 25, 60, 10}}, {100, {2, 12, 70, 16}}};
    t.rows[Destitute] = {1, {"bread", "fish", "meal"}, 45, 0, 3, .4, 1, 0, 1, 2};
    t.rows[Poor] = {2, {"bread", "fish", "meal"}, 12, 3, 12, .7, .35, 1, 3, 4};
    t.rows[Comfortable] = {3, {"meal", "bread", "fish"}, 1, 12, 60, 1, .05, 2, 8, 8};
    t.rows[Rich] = {3, {"cheese~fine", "cheese", "meal"}, 0, 60, 300, 1.5, 0, 5, 25, 20};
    t.classScore = {{"upper", 2}, {"middle", 1}, {"lower", 0}};
    t.homeScore = {{"house", .5}, {"cottage", 0}, {"flat", -.2}, {"room", -.5}};
    t.districtScore = {{"heights", .5}, {"sump", -.5}};
    t.events = {{"bereaved", -.5}, {"lostPost", -1}, {"robbed", -.2}, {"failing", -.3},
                {"helped", .4},    {"helpedMost", 1}, {"earning", .3}, {"earningMost", 1}};
    t.homes = {{"house", {"hall", "court", "manor", "residence", "fellhall", "grounds", "villa", "gatehouse", "estate", "palazzo"}},
               {"flat", {"flat", "rooms", "rents", "buildings", "loft", "stair", "tenement"}},
               {"room", {"barracks", "bunkhouse", "quarters", "watch_house", "guardhouse", "lodging", "shack", "den", "warrens",
                         "rookery", "sinks", "leaks", "shed"}}};
    t.caps = {{"rewardsADay", 200}, {"theftADay", 20}, {"stakesADay", 30}, {"courierADay", 3},
              {"welcomeDays", 7},   {"welcomeCoins", 20}, {"welcomeHerbs", 2}, {"welcomeMeals", 3}};
    t.pay = {{"keeper", 14}, {"hand", 8}, {"public", 8}, {"watch", 10}, {"farm", 8}, {"estate", 10}};
    // Street life (Phase 6): everyday wares' pieces a day, a luxury a week (6.5: 10% comfortable, 40% rich), services (a
    // barber rarely for the poor), alms from a passer-by.
    const double shops[Bands] = {0, .3, 1, 2}, luxury[Bands] = {0, 0, .1, .4}, services[Bands] = {0, .25, 1, 1.5},
                 alms[Bands] = {0, 0, .15, .3};
    for (int b = 0; b < Bands; ++b)
        t.rows[std::size_t(b)].shops = shops[b], t.rows[std::size_t(b)].luxury = luxury[b],
        t.rows[std::size_t(b)].services = services[b], t.rows[std::size_t(b)].alms = alms[b];
    return t;
}

bool sharesValid(const std::vector<SharePoint>& shares, std::string* why)
{
    const auto fail = [&](const std::string& w) {
        if (why)
            *why = w;
        return false;
    };
    if (shares.size() < 2)
        return fail("fewer than two rows");
    if (shares.front().at != -100 || shares.back().at != 100)
        return fail("the rows don't run from -100 to 100");
    for (std::size_t i = 0; i < shares.size(); ++i)
    {
        double sum = 0;
        for (const double s : shares[i].shares)
        {
            if (!std::isfinite(s) || s < MinShare - 1e-9)
                return fail("a share under " + std::to_string(int(MinShare)) + "%");
            sum += s;
        }
        if (std::abs(sum - 100) > 1e-6)
            return fail("a row doesn't sum to 100");
        if (i > 0)
        {
            if (shares[i].at <= shares[i - 1].at)
                return fail("the rows aren't in rising order");
            if (shares[i].shares[Destitute] > shares[i - 1].shares[Destitute] + 1e-9)
                return fail("destitute rises as the slider rises");
            if (shares[i].shares[Rich] < shares[i - 1].shares[Rich] - 1e-9)
                return fail("rich falls as the slider rises");
        }
    }
    return true;
}

Table read(const json::Value& v, const Table& base, std::vector<std::string>* warnings)
{
    Table t = base;
    if (!v.isObject())
    {
        if (warnings && !v.isNull())
            warnings->push_back("status: not an object: the defaults kept");
        return t;
    }
    // The shares: read whole, normalised, checked; anything wrong and the base's stand.
    if (const auto* shares = v.find("shares"))
    {
        std::vector<SharePoint> read;
        bool shaped = shares->isArray() && shares->size() >= 2 && shares->size() <= 21;
        for (const auto& row : shares->items())
        {
            SharePoint p;
            const auto* at = row.find("at");
            const auto* list = row.find("shares");
            if (!row.isObject() || !at || !at->isNumber() || !std::isfinite(at->asNumber()) || !list || !list->isArray() ||
                list->size() != Bands)
            {
                shaped = false;
                break;
            }
            p.at = at->asNumber();
            double sum = 0;
            for (int b = 0; b < Bands; ++b)
            {
                const auto& s = list->items()[std::size_t(b)];
                p.shares[std::size_t(b)] = s.isNumber() && std::isfinite(s.asNumber()) ? s.asNumber() : -1;
                sum += p.shares[std::size_t(b)];
            }
            if (sum > 0 && std::abs(sum - 100) > 1e-6)
            {
                if (warnings)
                    warnings->push_back("status: a share row summing to " + std::to_string(sum) + " normalised");
                for (auto& s : p.shares)
                    s = s * 100 / sum;
            }
            read.push_back(p);
        }
        std::sort(read.begin(), read.end(), [](const SharePoint& a, const SharePoint& b) { return a.at < b.at; });
        std::string why = "not a list of rows";
        if (shaped && sharesValid(read, &why))
            t.shares = std::move(read);
        else if (warnings)
            warnings->push_back("status: the share table falls back to the default (" + why + ")");
    }
    t.timesShift = clampNumber(v, "timesShift", t.timesShift, 0, 100, "status: ", warnings);
    t.smoothing = clampNumber(v, "smoothing", t.smoothing, 1, 200, "status: ", warnings);
    const auto& hys = v.object("hysteresis");
    t.hysteresisDays = int(clampNumber(hys, "days", t.hysteresisDays, 0, 365, "status: hysteresis.", warnings));
    t.hysteresisEdge = clampNumber(hys, "edge", t.hysteresisEdge, 0, 50, "status: hysteresis.", warnings);
    t.shortHard = clampNumber(v, "shortHard", t.shortHard, 0, 10, "status: ", warnings);
    t.shortBoom = clampNumber(v, "shortBoom", t.shortBoom, 0, 10, "status: ", warnings);
    t.shortPressureMost = clampNumber(v, "shortPressureMost", t.shortPressureMost, 1, 10, "status: ", warnings);
    t.shortPressureAt = clampNumber(v, "shortPressureAt", t.shortPressureAt, .01, 5, "status: ", warnings);
    const auto& rows = v.object("rows");
    for (int b = 0; b < Bands; ++b)
    {
        const auto& r = rows.object(Names[b]);
        if (r.isNull())
            continue;
        auto& row = t.rows[std::size_t(b)];
        const std::string where = std::string("status: rows.") + Names[b] + ".";
        row.meals = clampNumber(r, "meals", row.meals, 0, 6, where, warnings);
        if (const auto* food = r.find("food"); food && food->isArray())
            if (auto list = words(*food); !list.empty())
                row.food = std::move(list);
        row.shortChance = clampNumber(r, "short", row.shortChance, 0, 100, where, warnings);
        pair(r, "purse", row.purseLow, row.purseHigh, 500, where, warnings);
        row.budget = clampNumber(r, "budget", row.budget, 0, 5, where, warnings);
        row.theft = clampNumber(r, "theft", row.theft, 0, 5, where, warnings);
        pair(r, "pocket", row.pocketLow, row.pocketHigh, 500, where, warnings);
        row.goodsMost = int(clampNumber(r, "goodsMost", row.goodsMost, 0, 99, where, warnings));
        row.shops = clampNumber(r, "shops", row.shops, 0, 10, where, warnings);
        row.luxury = clampNumber(r, "luxury", row.luxury, 0, 1, where, warnings);
        row.services = clampNumber(r, "services", row.services, 0, 5, where, warnings);
        row.alms = clampNumber(r, "alms", row.alms, 0, 1, where, warnings);
    }
    const auto& score = v.object("score");
    if (score.isObject())
    {
        numbers(score.object("class"), t.classScore, -5, 5, "status: score.class.", warnings);
        numbers(score.object("home"), t.homeScore, -5, 5, "status: score.home.", warnings);
        numbers(score.object("district"), t.districtScore, -5, 5, "status: score.district.", warnings);
        numbers(score.object("events"), t.events, -5, 5, "status: score.events.", warnings);
        t.noEarner = clampNumber(score, "noEarner", t.noEarner, -5, 5, "status: score.", warnings);
        t.thinEarner = clampNumber(score, "thinEarner", t.thinEarner, -5, 5, "status: score.", warnings);
        t.thinMouths = int(clampNumber(score, "thinMouths", t.thinMouths, 1, 64, "status: score.", warnings));
        t.lot = clampNumber(score, "lot", t.lot, 0, 2, "status: score.", warnings);
        t.noise = clampNumber(score, "noise", t.noise, 0, 1, "status: score.", warnings);
        t.eventDays = int(clampNumber(score, "eventDays", t.eventDays, 1, 365, "status: score.", warnings));
        if (const auto* times = score.find("times"); times && times->isArray() && times->size() == 5)
            for (std::size_t i = 0; i < 5; ++i)
                if (const auto& n = times->items()[i]; n.isNumber() && std::isfinite(n.asNumber()))
                    t.times[i] = std::clamp(n.asNumber(), -5., 5.);
    }
    for (const char* kind : {"house", "flat", "room"})
        if (const auto* list = v.object("homes").find(kind); list && list->isArray())
            t.homes[kind] = words(*list);
    const auto& hold = v.object("hold");
    t.holdOneOff = int(clampNumber(hold, "oneOff", t.holdOneOff, 0, 60, "status: hold.", warnings));
    t.holdSteady = int(clampNumber(hold, "steady", t.holdSteady, 0, 120, "status: hold.", warnings));
    t.leanDays = int(clampNumber(v, "leanDays", t.leanDays, 0, 60, "status: ", warnings));
    pair(v, "treasury", t.treasuryFloor, t.treasuryCeiling, 1000000, "status: ", warnings);
    for (const auto& [key, n] : v.object("caps").fields())
        if (key.size() <= 40 && n.isNumber() && std::isfinite(n.asNumber()))
            t.caps[key] = std::int64_t(std::clamp(n.asNumber(), 0., 100000.));
    for (const auto& [key, n] : v.object("pay").fields())
        if (key.size() <= 40 && n.isNumber() && std::isfinite(n.asNumber()))
            t.pay[key] = std::int64_t(std::clamp(n.asNumber(), 0., 1000.));
    // Street life (Phase 6).
    const auto& street = v.object("street");
    if (street.isObject())
    {
        t.theftBase = clampNumber(street, "theftBase", t.theftBase, 0, 1, "status: street.", warnings);
        if (const auto* times = street.find("theftTimes"); times && times->isArray() && times->size() == 5)
            for (std::size_t i = 0; i < 5; ++i)
                if (const auto& n = times->items()[i]; n.isNumber() && std::isfinite(n.asNumber()))
                    t.theftTimes[i] = std::clamp(n.asNumber(), 0., 5.);
        numbers(street.object("theftHonesty"), t.theftHonesty, 0, 10, "status: street.theftHonesty.", warnings);
        t.slyRich = clampNumber(street, "slyRich", t.slyRich, 0, 1, "status: street.", warnings);
        t.doleDays = clampNumber(street, "doleDays", t.doleDays, 0, 2, "status: street.", warnings);
        t.beggarsACell = int(clampNumber(street, "beggarsACell", t.beggarsACell, 0, 8, "status: street.", warnings));
        // The most in a cell by the slider (the user's decision, 2026-11-11): 2 to 21 points from -100 to 100, each most
        // 0..8, never rising as the slider rises; a curve that fails falls back to the base's whole, with a warning.
        if (const auto* cells = street.find("beggarsInACell"))
        {
            std::vector<Table::CellPoint> read;
            bool shaped = cells->isArray() && cells->size() >= 2 && cells->size() <= 21;
            for (const auto& row : cells->items())
            {
                const auto* at = row.find("at");
                const auto* most = row.find("most");
                if (!row.isObject() || !at || !at->isNumber() || !std::isfinite(at->asNumber()) || !most || !most->isNumber() ||
                    !std::isfinite(most->asNumber()) || most->asNumber() < 0 || most->asNumber() > 8)
                {
                    shaped = false;
                    break;
                }
                read.push_back({at->asNumber(), most->asNumber()});
            }
            std::sort(read.begin(), read.end(), [](const Table::CellPoint& a, const Table::CellPoint& b) { return a.at < b.at; });
            for (std::size_t i = 0; shaped && i < read.size(); ++i)
                shaped = (i == 0 || (read[i].at > read[i - 1].at && read[i].most <= read[i - 1].most + 1e-9));
            shaped = shaped && !read.empty() && std::abs(read.front().at + 100) < 1e-9 && std::abs(read.back().at - 100) < 1e-9;
            if (shaped)
                t.beggarsInACell = std::move(read);
            else if (warnings)
                warnings->push_back("status: street.beggarsInACell falls back to the default (2 to 21 points from -100 to 100, "
                                    "each most 0 to 8, never rising)");
        }
        numbers(street.object("beggarsClass"), t.beggarsClass, 0, 10, "status: street.beggarsClass.", warnings);
        t.beggarsSpread = clampNumber(street, "beggarsSpread", t.beggarsSpread, .05, 1, "status: street.", warnings);
        t.beggarsElders = clampNumber(street, "beggarsElders", t.beggarsElders, 0, 1, "status: street.", warnings);
    }
    return t;
}

double beggarsInACellAt(const Table& t, double slider, int times)
{
    const auto& c = t.beggarsInACell;
    if (c.empty())
        return 0;
    slider = std::clamp((std::isfinite(slider) ? slider : 0.) + t.timesShift * std::clamp(times, -2, 2) / 2., -100., 100.);
    if (slider <= c.front().at)
        return c.front().most;
    for (std::size_t i = 1; i < c.size(); ++i)
        if (slider <= c[i].at)
        {
            const double f = (slider - c[i - 1].at) / std::max(1e-9, c[i].at - c[i - 1].at);
            return c[i - 1].most + (c[i].most - c[i - 1].most) * f;
        }
    return c.back().most;
}

const Table& defaults()
{
    static const Table table = [] {
        auto t = builtIn();
        std::ifstream in(statusFile(), std::ios::binary);
        if (!in)
            return t;
        std::stringstream text;
        text << in.rdbuf();
        json::Value v;
        std::string error;
        if (!json::parse(text.str(), v, error))
            return t;
        return read(v, t);
    }();
    return table;
}

std::array<double, Bands> sharesAt(const Table& t, double slider, bool neverBroke)
{
    slider = std::clamp(std::isfinite(slider) ? slider : 0., -100., 100.);
    std::array<double, Bands> out = t.shares.empty() ? std::array<double, Bands>{5, 25, 60, 10} : t.shares.front().shares;
    for (std::size_t i = 1; i < t.shares.size(); ++i)
        if (slider <= t.shares[i].at || i + 1 == t.shares.size())
        {
            const auto& a = t.shares[i - 1];
            const auto& b = t.shares[i];
            const double f = std::clamp((slider - a.at) / std::max(1e-9, b.at - a.at), 0., 1.);
            for (std::size_t k = 0; k < Bands; ++k)
                out[k] = a.shares[k] + (b.shares[k] - a.shares[k]) * f;
            break;
        }
    if (neverBroke)
    {
        out[Poor] += out[Destitute];
        out[Destitute] = 0;
    }
    return out;
}

std::array<int, Bands> cut(const std::array<double, Bands>& shares, int n)
{
    std::array<int, Bands> counts{};
    if (n <= 0)
        return counts;
    double total = 0;
    for (const double s : shares)
        total += std::max(0., s);
    if (total <= 0)
    {
        counts[Comfortable] = n;
        return counts;
    }
    std::array<double, Bands> exact{};
    int given = 0;
    for (std::size_t b = 0; b < Bands; ++b)
    {
        exact[b] = double(n) * std::max(0., shares[b]) / total;
        counts[b] = int(std::floor(exact[b] + 1e-9));
        given += counts[b];
    }
    std::array<std::size_t, Bands> order{0, 1, 2, 3};
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return exact[a] - counts[a] > exact[b] - counts[b] + 1e-12;
    });
    for (std::size_t i = 0; given < n; i = (i + 1) % Bands)
        if (shares[order[i]] > 0)
            ++counts[order[i]], ++given;
    return counts;
}

double shortChance(const Table& t, int band, double slider)
{
    if (band < 0 || band >= Bands)
        return 0;
    slider = std::clamp(std::isfinite(slider) ? slider : 0., -100., 100.);
    const double times = slider < 0 ? 1 + (t.shortHard - 1) * (-slider / 100) : 1 + (t.shortBoom - 1) * (slider / 100);
    return std::clamp(t.rows[std::size_t(band)].shortChance / 100 * times, 0., 1.);
}

std::vector<int> assign(const Table& t, const std::vector<Candidate>& households, const CutOptions& o)
{
    const std::size_t n = households.size();
    std::vector<int> band(n, Comfortable);
    std::vector<double> percentile(n, 50);          // Where each stands in its market's pool, 0..100.
    std::vector<bool> inPool(n, false);
    std::map<std::string, std::vector<std::size_t>> pools;
    for (std::size_t i = 0; i < n; ++i)
    {
        const auto& h = households[i];
        inPool[i] = h.fixed < 0 && !h.lifted && h.atMost == Rich && h.atLeast == Destitute;
        if (inPool[i])
            pools[h.market].push_back(i);
    }
    // The bands by rank in each market's pool, and the edges (the score where each band starts) for those outside it.
    std::map<std::string, std::array<int, Bands>> counts;
    std::map<std::string, std::array<double, Bands>> shares;
    for (auto& [market, pool] : pools)
    {
        std::stable_sort(pool.begin(), pool.end(), [&](std::size_t a, std::size_t b) {
            return households[a].score < households[b].score ||
                   (households[a].score == households[b].score && households[a].key < households[b].key);
        });
        const auto times = o.times.count(market) ? std::clamp(o.times.at(market), -2, 2) : 0;
        shares[market] = sharesAt(t, o.slider + t.timesShift * times / 2., o.neverBroke);
        counts[market] = cut(shares[market], int(pool.size()));
        int rank = 0;
        for (const auto i : pool)
        {
            int b = 0, edge = counts[market][0];
            while (b < Bands - 1 && rank >= edge)
                edge += counts[market][std::size_t(++b)];
            band[i] = b == Destitute && !households[i].reason ? Poor : b;
            percentile[i] = pool.size() > 1 ? 100. * rank / double(pool.size() - 1) : 50;
            ++rank;
        }
    }
    // Those outside the pool, by their score among it (a market with no pool: by the realm's, else comfortable).
    std::vector<std::size_t> realm;
    for (std::size_t i = 0; i < n; ++i)
        if (inPool[i])
            realm.push_back(i);
    std::stable_sort(realm.begin(), realm.end(), [&](std::size_t a, std::size_t b) { return households[a].score < households[b].score; });
    for (std::size_t i = 0; i < n; ++i)
    {
        if (inPool[i])
            continue;
        const auto& h = households[i];
        const auto found = pools.find(h.market);
        const auto& pool = found != pools.end() ? found->second : realm;
        if (pool.empty())
        {
            band[i] = Comfortable;
            continue;
        }
        const auto& c = found != pools.end() ? counts[h.market] : cut(sharesAt(t, o.slider, o.neverBroke), int(pool.size()));
        int below = 0;
        for (const auto j : pool)
            below += households[j].score < h.score ? 1 : 0;
        int b = 0, edge = c[0];
        while (b < Bands - 1 && below >= edge)
            edge += c[std::size_t(++b)];
        band[i] = b;
        percentile[i] = 100. * below / double(std::max<std::size_t>(1, pool.size()));
    }
    // A band set outright (a DM's override, the homeless, a House's). Never Broke empties destitute (16.1): the homeless'
    // and beggars' guarantee then gives poor; only the DM's own override still holds a household destitute.
    const auto fixedBand = [&](const Candidate& h) {
        const int b = std::clamp(h.fixed, 0, Bands - 1);
        return b == Destitute && o.neverBroke && !h.overridden ? int(Poor) : b;
    };
    // The guarantees and the bounds every band keeps, applied again after the hysteresis.
    const auto bound = [&](std::size_t i, int b) {
        const auto& h = households[i];
        if (h.fixed >= 0)
            return fixedBand(h);
        if (!h.recent)                              // (Near its home: one band away, two down where it has a reason.)
            b = std::clamp(b, std::max(0, h.implied - (h.reason ? 2 : 1)), std::min(Bands - 1, h.implied + 1));
        b = std::clamp(b, h.atLeast, h.atMost);
        if (b == Destitute && !h.reason)
            b = Poor;
        if (b == Destitute && o.neverBroke)
            b = Poor;
        return b;
    };
    for (std::size_t i = 0; i < n; ++i)
    {
        const auto& h = households[i];
        int b = bound(i, band[i]);
        if (h.previous >= 0 && h.previous < Bands && o.keepPrevious && !h.overridden)
            b = h.previous;
        else if (h.previous >= 0 && h.previous < Bands && b != h.previous && !o.now && !h.overridden && h.fixed < 0)
        {
            // At most one band a week, and only past the edge by hysteresisEdge percentiles.
            const auto& sh = shares.count(h.market) ? shares[h.market] : sharesAt(t, o.slider, o.neverBroke);
            double edgeUp = 0;
            for (int k = 0; k <= h.previous && k < Bands; ++k)
                edgeUp += sh[std::size_t(k)];
            const double edgeDown = edgeUp - sh[std::size_t(h.previous)];
            const bool weekPast = o.day - h.since >= t.hysteresisDays;
            const bool past = b > h.previous ? percentile[i] > edgeUp + t.hysteresisEdge || edgeUp >= 100
                                             : percentile[i] < edgeDown - t.hysteresisEdge || edgeDown <= 0;
            // (A bound that moved, a guarantee or a lost reason, applies at once.)
            const int kept = bound(i, h.previous);
            if (kept != h.previous)
                b = kept;
            else if (weekPast && past)
                b = h.previous + (b > h.previous ? 1 : -1);
            else
                b = h.previous;
        }
        band[i] = h.fixed >= 0 ? fixedBand(h) : b;
    }
    // The realm's minimum: a destitute and a rich household where there are ten or more, at the lowest and highest scores
    // of those whose bounds allow it (a reason for destitute; near enough its home) and the hysteresis too: one band from
    // where it stands and past its week (a new household, or the DM's "now", either way; at restore, none moves).
    const auto mayMove = [&](std::size_t i, int target) {
        const auto& h = households[i];
        if (o.keepPrevious)
            return false;
        if (o.now || h.previous < 0 || h.previous >= Bands)
            return true;
        return band[i] == h.previous && std::abs(band[i] - target) <= 1 && o.day - h.since >= t.hysteresisDays;
    };
    if (n >= 10)
    {
        const auto has = [&](int b) { return std::find(band.begin(), band.end(), b) != band.end(); };
        if (!o.neverBroke && !has(Destitute))
        {
            std::size_t best = n;
            for (std::size_t i = 0; i < n; ++i)
                if (inPool[i] && bound(i, Destitute) == Destitute && mayMove(i, Destitute) && (best == n || households[i].score < households[best].score))
                    best = i;
            if (best < n)
                band[best] = Destitute;
        }
        if (!has(Rich))
        {
            std::size_t best = n;
            for (std::size_t i = 0; i < n; ++i)
                if (inPool[i] && bound(i, Rich) == Rich && mayMove(i, Rich) && (best == n || households[i].score > households[best].score))
                    best = i;
            if (best < n)
                band[best] = Rich;
        }
    }
    return band;
}

std::string homeKind(const Table& t, const std::string& homeCell)
{
    if (homeCell.empty())
        return {};
    // The cell's words: "rm_greatly_house_up_2" -> rm, greatly, house, up, 2 (also matched joined, "watch_house").
    for (const char* kind : {"room", "flat", "house"})
        if (const auto found = t.homes.find(kind); found != t.homes.end())
            for (const auto& w : found->second)
            {
                for (std::size_t at = homeCell.find(w); at != std::string::npos; at = homeCell.find(w, at + 1))
                {
                    const bool startOk = at == 0 || homeCell[at - 1] == '_';
                    const auto end = at + w.size();
                    const bool endOk = end == homeCell.size() || homeCell[end] == '_' ||
                                       (homeCell[end] == 's' && (end + 1 == homeCell.size() || homeCell[end + 1] == '_'));
                    if (startOk && endOk)
                        return kind;
                }
            }
    return "cottage";
}
} // namespace ratw::status
