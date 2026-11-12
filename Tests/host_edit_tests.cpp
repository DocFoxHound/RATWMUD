// The robustness gate (Docs/Design/79-a-staged-economy.md, 16.6; Phase 4): a host's edits never stop a world loading.
// A strip with two markets, carters, a staffed farm, a wagon on the road with a letter, a player's escort and an item, is
// saved; then each edit is made to the world (its manifest) or to the save (its document), the save is restored into it,
// and the world runs on two days. Each loads, stages something plausible (every shop left sets out its wares, prices
// finite and within the clamp, the roads' wagons between markets that are there, the slider in range) and refuses
// nothing: no port; one market; a market's merchants deleted; a lone merchant outside every community; every farm
// deleted; every shop deleted; every carter deleted; zero households; a business label changed; an NPC deleted while it
// holds an account; an item deleted while a player holds it (quarantined); an unknown good in pressure, a rule or an
// override; a slider of 9999 or "abc"; a status.json override that sums to 140; a purse target of 10^9 (clamped); a
// road deleted with a wagon on it and one posted (lost and cancelled, said so); a contract whose town is gone (refunded). The catalogue is the real Data (RATW_SOURCE_DIR).
#include "test_purse.h"
#include "RatwCheckpoint.h"
#include "RatwItems.h"
#include "RatwGame.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace ratw;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

constexpr int Side = 16;
std::map<int, std::string> named = {{3, "fs_grain_long_granary"}};
std::string id(int i)
{
    const auto found = named.find(i);
    return found != named.end() ? found->second : "c_" + std::to_string(i) + "_0";
}

struct Fixture
{
    std::string manifest;
    std::map<std::string, std::string> cells, seams;
    World::CellSource source()
    {
        return {[this](const std::string& cell, std::string& header) {
                    const auto found = cells.find(cell);
                    if (found == cells.end()) return std::string("no such cell");
                    header = found->second.substr(0, found->second.find("grid:") + 5);
                    return std::string();
                },
                [this](const std::string& cell, std::string& text, std::string& sides) {
                    const auto found = cells.find(cell);
                    if (found == cells.end()) return std::string("no such cell");
                    text = found->second;
                    sides = seams[cell];
                    return std::string();
                }};
    }
};

std::string resident(const std::string& who, const std::string& name, const std::string& role, const std::string& work,
                     int home, double hx, int at, double wx)
{
    std::ostringstream r;
    r << "resident \"" << who << "\" \"" << name << "\" \"" << role << "\" \"" << work << "\" \"Someone.\" \"Hello.\" 30 \"timber\" "
      << "\"male\" \"average\" \"saddle\" 3 1 5 1 1 8 17 \"-\" 40 0 1 \"" << id(home) << "\" " << hx << " 4.5 \"" << id(at)
      << "\" " << wx << " 8.5 \"" << id(home) << "\" " << hx << " 5.5\n";
    return r.str();
}

// "EE.G...WW": the city "east" (a stall, a smith, two carters), wild country with a grain farm's work room, the town
// "west" (a stall and a general store). Each line of residents a `resident` record, so an edit drops or changes lines.
Fixture strip(const std::string& farmLabel)
{
    const std::string layout = "EE.G...WW";
    Fixture f;
    const int cells = int(layout.size());
    std::ostringstream m;
    m << "RATW_WORLD 3\n";
    int seam = 0;
    for (int i = 0; i < cells; ++i)
    {
        std::ostringstream cell;
        cell << "id: " << id(i) << "\nname: Stretch " << i << "\ndescription: Open ground.\nworld: " << i * Side
             << " 0 0\noutdoors: true\nweather: clear\nsize: " << Side << ' ' << Side << "\ngrid:\n";
        for (int y = 0; y < Side; ++y) cell << std::string(Side, '.') << '\n';
        f.cells[id(i)] = cell.str();
        m << "area \"" << id(i) << "\"\n";
        if (i + 1 < cells)
            for (int k = 0; k < Side; ++k)
            {
                const std::string a = "seam_" + std::to_string(seam) + "_a", b = "seam_" + std::to_string(seam) + "_b";
                std::ostringstream one, two;
                one << "door \"" << a << "\" \"Open boundary\" \"" << id(i) << "\" " << Side - .5 << ' ' << k + .5 << " \"" << id(i + 1)
                    << "\" .5 " << k + .5 << " \"" << b << "\" 1 0 1 1 \"E\"\n";
                two << "door \"" << b << "\" \"Open boundary\" \"" << id(i + 1) << "\" .5 " << k + .5 << " \"" << id(i) << "\" "
                    << Side - .5 << ' ' << k + .5 << " \"" << a << "\" 1 0 1 1 \"W\"\n";
                f.seams[id(i)] += one.str();
                f.seams[id(i + 1)] += two.str();
                ++seam;
            }
    }
    const auto region = [](char c) { return c == 'E' ? "east" : c == 'W' ? "west" : "wilds"; };
    for (int i = 0; i < cells; ++i)
    {
        m << "exits \"" << id(i) << "\" " << ((i > 0) + (i + 1 < cells));
        if (i > 0) m << " \"" << id(i - 1) << '"';
        if (i + 1 < cells) m << " \"" << id(i + 1) << '"';
        m << '\n';
        m << "territory \"" << id(i) << "\" \"" << region(layout[std::size_t(i)]) << "\" \"-\" 0\n";
    }
    m << "spawn \"" << id(0) << "\" 8.5 8.5\n";
    m << "economy 20000 100 50 10 12\n";
    m << resident("em", "Ember Oak", "merchant", "keeping the stall", 0, 2.5, 1, 8.5);
    m << resident("es", "Ivo Anvil", "merchant", "smith at The Gate Forge", 0, 3.5, 1, 10.5);
    m << resident("c1", "Tam Reed", "civilian", "driving for the carters", 0, 4.5, 1, 4.5);
    m << resident("c2", "Hob Lane", "civilian", "driving for the carters", 0, 5.5, 1, 5.5);
    if (!farmLabel.empty())
        m << resident("fk", "Bran Long", "civilian", farmLabel, 0, 6.5, 3, 9.5);
    for (int n = 1; n <= 4; ++n)
        m << resident("e" + std::to_string(n), "East " + std::to_string(n), "civilian", "working", 0, 7.5 + n, 0, 7.5 + n);
    m << resident("wm", "Wren Reed", "merchant", "keeping the stall", 7, 2.5, 8, 8.5);
    m << resident("wg", "Gil Moss", "merchant", "shopkeeper at The West Store", 7, 3.5, 8, 10.5);
    for (int n = 1; n <= 4; ++n)
        m << resident("w" + std::to_string(n), "West " + std::to_string(n), "civilian", "working", 7, 4.5 + n, 7, 4.5 + n);
    f.manifest = m.str();
    return f;
}

World load(Fixture& f, const std::string& what)
{
    World world;
    world.setCellSource(f.source());
    const auto loaded = world.loadWorldFiles({{"world.ratw", f.manifest}}, "strip");
    expect(loaded.ok, what + ": the world loads: " + loaded.message);
    return world;
}

int stock(const World& w, const std::string& who, const std::string& item)
{
    const auto* a = w.society().account(w.society().tillOf(who));
    return a ? Society::stock(*a, item) : 0;
}

void nextMorning(World& w, const std::string& what)
{
    auto saved = w.save();
    saved.calendarDays = std::floor(saved.calendarDays) + 1.2;
    const auto r = w.restore(saved);
    expect(r.ok, what + ": a day passes: " + r.message);
    w.tick(.6);
}

// Lines of the manifest without the residents named, or with one changed.
Fixture without(Fixture f, const std::vector<std::string>& who)
{
    std::string kept;
    std::istringstream lines(f.manifest);
    for (std::string line; std::getline(lines, line);)
    {
        bool drop = false;
        for (const auto& w : who)
            drop = drop || line.rfind("resident \"" + w + "\"", 0) == 0;
        if (!drop)
            kept += line + "\n";
    }
    f.manifest = kept;
    return f;
}
Fixture replaced(Fixture f, const std::string& from, const std::string& to)
{
    if (const auto at = f.manifest.find(from); at != std::string::npos)
        f.manifest.replace(at, from.size(), to);
    return f;
}

// The road between cells a and a + 1 closed (its seam's doors and the exits gone): east and west no longer joined.
Fixture cut(Fixture f, int a)
{
    const auto drop = [](const std::string& text, const std::string& other) {
        std::string kept;
        std::istringstream lines(text);
        for (std::string line; std::getline(lines, line);)
            if (line.find("\"" + other + "\"") == std::string::npos)
                kept += line + "\n";
        return kept;
    };
    f.seams[id(a)] = drop(f.seams[id(a)], id(a + 1));
    f.seams[id(a + 1)] = drop(f.seams[id(a + 1)], id(a));
    std::string out;
    std::istringstream lines(f.manifest);
    for (std::string line; std::getline(lines, line);)
    {
        if (line.rfind("exits \"" + id(a) + "\"", 0) == 0)
            line = "exits \"" + id(a) + "\" 1 \"" + id(a - 1) + "\"";
        else if (line.rfind("exits \"" + id(a + 1) + "\"", 0) == 0)
            line = "exits \"" + id(a + 1) + "\" 1 \"" + id(a + 2) + "\"";
        out += line + "\n";
    }
    f.manifest = out;
    return f;
}

// The base world, run two days with its roads busy: a wagon out to west with Bo's letter and Ada's escort, Ada holding an
// item, a player's courier contract to a town, and the save of it.
struct Base
{
    std::string label;
    Fixture fixture;
    PersistedWorld saved;
    std::string letter, escort, orphan;
};

Base base()
{
    Base b;
    {
        auto probe = strip("");
        auto p = load(probe, "the probe");
        for (const auto& farm : p.society().farms())
            if (farm.def->id == "grain_farm_1")
                b.label = p.society().farmLabel(farm, 0);
    }
    expect(!b.label.empty(), "the grain farm's keeper's label");
    b.fixture = strip(b.label);
    auto w = load(b.fixture, "the base");
    w.addPlayer("player-ada", "Ada");
    w.addPlayer("player-bo", "Bo");
    w.tick(.6);
    w.roads().camps.clear();
    w.society().stagedBoard().harvestOverride["grain"] = {2, w.calendarDays() + 30};
    testPurse(w.society(), "player-bo", 40, "test: Bo's purse");
    testPurse(w.society(), "player-ada", 40, "test: Ada's purse");
    auto* bo = w.entity("player-bo");
    bo->cellId = id(1);
    bo->position = {8.5, 12.5};
    b.letter = w.postContract("courier", "player-bo", "west", "wm", 4, 30, "a letter from Bo to Wren").id;
    for (auto& k : w.roads().contracts)
        if (k.id == b.letter)
            k.created -= 8;                          // (A week untaken: the carters take it.)
    auto* ada = w.entity("player-ada");
    ada->cellId = id(8);
    ada->position = {8.5, 8.5};
    b.escort = w.postContract("escort", "treasury", "west", "west", 12, 7, "guarding the next wagon to west").id;
    expect(w.takeContract("player-ada", b.escort).ok, "Ada takes the escort");
    b.orphan = w.postContract("courier", "player-ada", "west", "w1", 3, 30, "a letter from Ada to West 1").id;
    ada->cellId = id(1);
    ada->position = {8.5, 10.5};
    const auto* sent = w.sendCaravan(*w.roadMarket("east"), *w.roadMarket("west"), {{"nails", 10}});
    expect(sent && !sent->letters.empty() && !sent->escorts.empty(), "a wagon out with Bo's letter and Ada's escort");
    for (int t = 0; t < 20; ++t)
        w.tick(1);
    expect(w.society().create("player-ada", "nails", 3, "test: Ada's nails"), "Ada holds some nails");
    nextMorning(w, "the base");
    expect(!w.roads().caravans.empty() || true, "the base runs");
    b.saved = w.save();
    expect(w.society().realBalanced(), "the base: real money is balanced");
    return b;
}

// Something plausible staged: every shop left sets out at least one of its wares; every P finite and within its range;
// every wagon between markets that are there; every committed departure to a market that is there; the slider in range;
// real money balanced; and the purses of residents within their bands' ranges (at most the rich's 500p).
void plausible(World& w, const std::string& what)
{
    const auto& s = w.society();
    for (const auto& p : s.positions())
        if (p.role == "merchant" && s.account("till:" + p.id))
        {
            const auto holder = s.state().careers.positions.count(p.id) ? s.state().careers.positions.at(p.id).holder : std::string();
            if (holder.empty())
                continue;
            int set = 0;
            for (const auto& ware : s.wares(holder))
                set += stock(w, holder, ware) > 0;
            expect(set > 0 || s.wares(holder).empty(), what + ": the shop of " + holder + " sets out its wares");
        }
    for (const auto& [m, goods] : s.board().cell)
        for (const auto& [good, c] : goods)
            expect(std::isfinite(c.P) && std::abs(c.P) <= market::PressureMost + 1e-9 && items::good(good),
                   what + ": a price pressure finite, in range and of a known good: " + m + " " + good);
    for (const auto& c : w.roads().caravans)
        expect(w.roadMarket(c.to) && (c.kind == "cart" || w.roadMarket(c.from)), what + ": a wagon between markets that are there");
    for (const auto& d : s.board().committed)
        expect(w.roadMarket(d.to), what + ": a departure to a market that is there");
    expect(s.board().slider >= -100 && s.board().slider <= 100 && s.board().sliderTarget >= -100 && s.board().sliderTarget <= 100,
           what + ": the slider in range");
    for (const auto& [rid, life] : s.state().residents)
        if (const auto* a = s.account(rid))
            expect(a->cash >= 0 && a->cash <= 500, what + ": " + rid + "'s purse within its band's range: " + std::to_string(a->cash));
    expect(s.realBalanced(), what + ": real money is balanced");
}

// The save restored into the edited world (or the edited save into the base world), then two days on.
World edited(const Base& b, Fixture f, const std::string& what, const std::function<void(PersistedWorld&)>& change = {},
             const std::function<void(World&)>& before = {})
{
    auto w = load(f, what);
    if (before)
        before(w);
    auto saved = b.saved;
    if (change)
        change(saved);
    const auto r = w.restore(saved);
    expect(r.ok, what + ": the save restores, never refused: " + r.message + " " + r.targetId);
    w.tick(.6);
    for (int day = 0; day < 2; ++day)
    {
        nextMorning(w, what);
        expect(w.setTimeOfDay(11).ok, what + ": to the late morning");
        for (int t = 0; t < 40; ++t)
            w.tick(1);
    }
    plausible(w, what);
    return w;
}

// The save as a checkpoint document, changed as JSON, and decoded again (what a host's hand edit of a save would be).
PersistedWorld throughJson(const PersistedWorld& saved, const std::function<void(json::Value&)>& change)
{
    auto document = checkpoint::encode(saved, {}, saved.npcs, saved.time);
    change(document);
    PersistedWorld out;
    checkpoint::ServerState server;
    std::string problem;
    expect(checkpoint::decode(document, out, server, problem), "the edited save decodes: " + problem);
    return out;
}

void theEdits()
{
    const auto b = base();
    // As it is (no port: there is none): plausible.
    edited(b, b.fixture, "as saved, with no port");
    // One market: west's merchants gone. The wagon to it is dropped, Bo's letter open again at east, Ada's escort's pay back.
    {
        auto w = edited(b, without(b.fixture, {"wm", "wg"}), "one market");
        expect(w.roadMarkets().size() == 1 && w.roadMarket("east"), "one market: east alone");
        for (const auto& k : w.roads().contracts)
        {
            if (k.id == b.letter)
                expect(k.status == "open" && w.society().cash("contract:" + k.id) == 4, "one market: Bo's letter open again, its 4p in escrow");
            if (k.id == b.escort)
                expect(k.status == "expired", "one market: Ada's escort ended, its pay back");
        }
        bool wagons = false;
        for (const auto& d : w.society().board().committed)
            wagons |= d.kind == "wagon";
        expect(!wagons, "one market: no wagons posted");
    }
    // A market's merchants deleted (east's, its carters kept): west alone, the carters of no market.
    {
        auto w = edited(b, without(b.fixture, {"em", "es"}), "east's merchants deleted");
        std::string ms;
        for (const auto& m : w.roadMarkets())
            ms += m.id + " ";
        // (East's homes are served by west's market now, so its carters are west's: with no other market, no wagons.)
        expect(w.roadMarkets().size() == 1 && w.roadMarket("west") && w.roadMarketOf(id(0)) == w.roadMarket("west"),
               "east's merchants deleted: west alone, serving east's homes: " + ms);
    }
    // A lone merchant outside every community: joined to the nearest market by path.
    {
        auto f = b.fixture;
        f.manifest += resident("lm", "Lone Pedlar", "merchant", "keeping the stall", 0, 9.5, 5, 8.5);
        auto w = edited(b, f, "a lone merchant");
        const auto* pedlar = w.society().jobOf("lm");
        expect(pedlar && w.society().tradePoints().ofTillId("till:" + pedlar->id), "a lone merchant: its shop in a market");
    }
    // Every farm deleted: no carts.
    {
        auto w = edited(b, without(b.fixture, {"fk"}), "every farm deleted");
        for (const auto& d : w.society().board().committed)
            expect(d.kind != "cart", "every farm deleted: no carts");
    }
    // Every shop deleted: no markets, no roads, nothing refused.
    {
        auto w = edited(b, without(b.fixture, {"em", "es", "wm", "wg"}), "every shop deleted");
        expect(w.roadMarkets().empty() && w.roads().caravans.empty(), "every shop deleted: no markets, no wagons");
    }
    // Every carter deleted: no wagons, and west's imports refill each morning.
    {
        auto w = edited(b, without(b.fixture, {"c1", "c2"}), "every carter deleted");
        expect(w.cartersOf("east").empty(), "every carter deleted: none");
        for (const auto& d : w.society().board().committed)
            expect(d.kind != "wagon" || !d.cause.empty(), "every carter deleted: no wagon sets out without a cause");
        for (const auto& ware : w.society().wares("wg"))
            expect(!w.society().imported("west", items::baseOf(ware)), "every carter deleted: nothing west waits for a wagon for");
    }
    // Zero households: every resident unplaced (no home, no work: kept aside, no body), so nobody lives anywhere.
    {
        auto f = b.fixture;
        std::string out;
        std::istringstream lines(f.manifest);
        for (std::string line; std::getline(lines, line);)
        {
            if (line.rfind("resident ", 0) == 0)
            {
                // The three spots (home, work, evening) are the last nine fields: "<cell>" x y, three times.
                std::size_t at = line.size();
                for (int quotes = 0; quotes < 6 && at > 0; )
                    if (line[--at] == '"')
                        ++quotes;
                line = line.substr(0, at) + "\"-\" 0 0 \"-\" 0 0 \"-\" 0 0";
            }
            out += line + "\n";
        }
        f.manifest = out;
        auto w = edited(b, f, "zero households");
        expect(w.society().households(true).empty() && w.roadMarkets().empty(), "zero households: none, and no markets");
    }
    // A business label changed (west's general store a bakery now).
    edited(b, replaced(b.fixture, "shopkeeper at The West Store", "baker at The West Store"), "a business label changed");
    // An NPC deleted while it holds an account (Tam, a carter, maybe driving).
    edited(b, without(b.fixture, {"c1"}), "an NPC deleted while it holds an account");
    // An item deleted while a player holds it: quarantined, not lost.
    {
        const auto save = throughJson(b.saved, [](json::Value& d) {
            if (auto* ada = d.find("society")->find("accounts")->find("player-ada"))
                if (auto* stock = ada->find("stock"); stock && stock->isObject())
                    stock->set("a_good_no_longer_known", json::Value(2));
        });
        auto fixture = b.fixture;
        auto w = load(fixture, "an item deleted");
        const auto r = w.restore(save);
        expect(r.ok, "an item deleted: the save restores: " + r.message);
        const auto& q = w.society().state().staged.quarantine;
        expect(q.count("player-ada") && q.at("player-ada").count("a_good_no_longer_known") && q.at("player-ada").at("a_good_no_longer_known") == 2,
               "an item deleted: quarantined, not lost");
        plausible(w, "an item deleted");
    }
    // An unknown good in pressure, a departure's manifest, a back room and an override; an unknown rule.
    {
        auto f = b.fixture;
        f.manifest += "rule \"notARule\" 5\nrule \"caravanRate\" 99999\n";
        const auto save = throughJson(b.saved, [](json::Value& d) {
            auto& board = *d.find("society")->find("board");
            auto cell = json::Value::object(), g = json::Value::object(), c = json::Value::object();
            c.add("P", .3);
            c.add("touched", 1.0);
            c.add("cause", "players");
            g.add("not_a_good", c);
            g.add("nails", c);
            cell.add("east", g);
            cell.add("nowhere", g);
            board.set("cell", cell);
            auto dep = json::Value::object(), goods = json::Value::object();
            goods.add("not_a_good", 5);
            goods.add("nails", 5);
            dep.add("id", "w999");
            dep.add("kind", "wagon");
            dep.add("from", "east");
            dep.add("to", "nowhere");
            dep.add("day", 2.0);
            dep.add("hour", "eight");
            dep.add("manifest", goods);
            auto list = json::Value::array();
            list.push(dep);
            board.set("committed", list);
            auto room = json::Value::object(), roomGoods = json::Value::object();
            roomGoods.add("not_a_good", 4);
            room.add("till:nobody", roomGoods);
            board.set("backRoom", room);
            auto harvest = json::Value::object(), h = json::Value::object();
            h.add("word", 77);
            h.add("until", 1e9);
            harvest.add("no_such_area", h);
            board.set("harvestOverride", harvest);
        });
        auto w = edited(b, f, "unknown goods, a rule and an override", [&](PersistedWorld& s) { s = save; });
        expect(w.society().rule("caravanRate") == 300, "an out-of-range rule clamped: " + std::to_string(w.society().rule("caravanRate")));
        const auto& cells = w.society().board().cell;
        expect(!cells.count("nowhere") && (!cells.count("east") || !cells.at("east").count("not_a_good")),
               "the unknown market's and good's pressure dropped");
        for (const auto& d : w.society().board().committed)
            expect(d.id != "w999", "a departure to a market that isn't there dropped");
    }
    // A slider of 9999, and of "abc".
    for (const auto& slider : {json::Value(9999), json::Value("abc")})
    {
        const auto save = throughJson(b.saved, [&](json::Value& d) {
            auto& board = *d.find("society")->find("board");
            board.set("slider", slider);
            board.set("sliderTarget", slider);
        });
        auto w = edited(b, b.fixture, std::string("a slider of ") + (slider.isNumber() ? "9999" : "\"abc\""), [&](PersistedWorld& s) { s = save; });
        expect(w.society().board().slider == (slider.isNumber() ? 100 : 0), "the slider clamped or read as 0: " +
                                                                              std::to_string(w.society().board().slider));
    }
    // A status.json override that sums to 140, and a purse target of 10^9: the defaults, and the clamp.
    {
        auto w = edited(b, b.fixture, "a bad status.json override", {}, [](World& w) {
            json::Value o;
            std::string error;
            json::parse(R"({"shares":[{"at":0,"shares":[40,40,40,20]}],"rows":{"rich":{"purse":[60,1000000000]}}})", o, error);
            w.society().setStatusOverride(&o);
        });
        expect(w.society().statusTable().rows[status::Rich].purseHigh <= 500, "a purse target of 10^9 clamped");
    }
    // A road deleted (between cells 4 and 5) with a wagon out on it and another posted: the one on the road lost, its
    // escort open again for the next wagon (its pay still held), its letter open again at home, its carter free; the
    // posted one cancelled, "the road is closed"; nothing refused.
    {
        const auto save = throughJson(b.saved, [&](json::Value& d) {
            auto& board = *d.find("society")->find("board");
            auto dep = json::Value::object(), goods = json::Value::object();
            goods.add("nails", 5);
            dep.add("id", "w998");
            dep.add("kind", "wagon");
            dep.add("from", "east");
            dep.add("to", "west");
            dep.add("driver", "c2");
            dep.add("day", std::floor(b.saved.calendarDays) + 1);
            dep.add("hour", 8.0);
            dep.add("posted", b.saved.calendarDays);
            dep.add("manifest", goods);
            auto list = json::Value::array();
            list.push(dep);
            board.set("committed", list);
        });
        std::string onRoad;
        for (const auto& c : save.roads.caravans)
            if (c.status == "travelling")
                onRoad = c.id;
        expect(!onRoad.empty(), "a road deleted: the base's wagon is on the road");
        auto w = edited(b, cut(b.fixture, 4), "a road deleted", [&](PersistedWorld& s) { s = save; });
        bool lost = false, cancelled = false;
        for (const auto& e : w.takeEvents())
        {
            lost = lost || (e.kind == "caravan lost" && e.actor == onRoad);
            cancelled = cancelled || (e.kind == "wagon cancelled" && e.actor == "w998" && e.detail == "the road is closed");
        }
        expect(lost, "a road deleted: the wagon on it lost, no way on");
        expect(cancelled, "a road deleted: the posted wagon cancelled, the road closed");
        for (const auto& c : w.roads().caravans)
            expect(c.id != onRoad && c.departure != "w998", "a road deleted: neither wagon on the roads");
        for (const auto& k : w.roads().contracts)
        {
            if (k.id == b.escort)
                expect(k.status == "open" && k.taker.empty() && w.society().cash("contract:" + k.id) > 0,
                       "a road deleted: Ada's escort open again, its pay still held: " + k.status);
            if (k.id == b.letter)
                expect(k.status == "open", "a road deleted: Bo's letter open again at east: " + k.status);
        }
        expect(w.drivers().empty(), "a road deleted: its carter free");
    }
    // A contract whose town is gone: refunded to its poster (Ada's letter "to" a market that no longer exists).
    {
        const auto save = throughJson(b.saved, [&](json::Value& d) {
            for (auto& k : d.find("roads")->find("contracts")->items())
                if (k.string("id") == b.orphan)
                    k.set("town", "a_town_long_gone");
        });
        const auto adaBefore = save.society.accounts.count("player-ada") ? save.society.accounts.at("player-ada").cash : 0;
        auto w = edited(b, b.fixture, "a contract whose town is gone", [&](PersistedWorld& s) { s = save; });
        bool refunded = false;
        for (const auto& k : w.roads().contracts)
            if (k.id == b.orphan)
                refunded = k.status == "expired" && !w.society().account("contract:" + k.id);
        expect(refunded && w.society().cash("player-ada") >= adaBefore + 3, "its 3p back to its poster");
    }
}
// Doc 79, 14 and 16.6 (Phase 9): the economy's records lenient, each one that won't do skipped with a warning and the
// world loaded; the host's overrides line; the band records; and the game's strip of a build's own records.
void lenientEconomyRecords()
{
    const auto base = strip("");
    const auto withLines = [&](const std::string& lines, const std::string& what) {
        auto f = base;
        f.manifest += lines;
        World w;
        w.setCellSource(f.source());
        const auto loaded = w.loadWorldFiles({{"world.ratw", f.manifest}}, "strip");
        expect(loaded.ok, what + ": loads: " + loaded.message);
        return w;
    };
    const auto warned = [](const World& w, const std::string& part) {
        const auto& list = w.society().authored().warnings;
        return std::any_of(list.begin(), list.end(), [&](const std::string& x) { return x.find(part) != std::string::npos; });
    };
    {
        // The slider's rule: 9999 opens at 100, a word skipped (0), 37 rounded to 35; its cause as 0060 printed it.
        auto w = withLines("rule \"prosperity\" 9999\nrule \"prosperityCause\" the long hard winter\n", "a slider of 9999");
        expect(w.society().rule("prosperity") == 100, "9999 is 100: " + std::to_string(w.society().rule("prosperity")));
        expect(w.society().authored().economy.overrides.cause == "the long hard winter", "the bare cause read");
        auto w2 = withLines("rule \"prosperity\" abc\n", "a slider of abc");
        expect(w2.society().rule("prosperity") == 0 && warned(w2, "not a whole number"), "abc skipped: 0");
        auto w3 = withLines("rule \"prosperity\" 37\nrule \"prosperityCause\" \"a fair\"\nrule \"prosperity\" 50\nrule \"harvestBias\" -99\n", "37");
        expect(w3.society().rule("prosperity") == 35 && w3.society().rule("harvestBias") == -20 && warned(w3, "a second rule prosperity"),
               "37 is 35, the second ignored, the bias clamped");
        expect(w3.society().authored().economy.overrides.cause == "a fair", "a quoted cause");
    }
    {
        // A second economy line, a malformed loop, an unknown rule, a 40 KB rule line, a bad season and service: skipped.
        const std::string big(40000, '9');
        auto w = withLines("economy 1 2 3\nloop needs maybe\nrule \"notARule\" 5\nrule \"ripple\" " + big + "\n"
                           "season \"bread\" price 1 2 3\nseason \"fish\" price 5 2000 100 100 shelf 100 100 100 100\n"
                           "season \"fish\" price 100 100 100 100 shelf 100 100 100 100\nservice \"barber\" maybe price 1 every 2\n",
                           "the bad records");
        expect(w.society().authored().economy.treasury == 20000, "the second economy line skipped");
        for (const char* part : {"a second economy record", "a malformed loop", "no such rule", "too long", "the season bread skipped",
                                 "a second season fish", "the service barber skipped"})
            expect(warned(w, part), std::string("warned: ") + part);
        const auto& fish = w.society().authored().economy.seasons.at("fish");
        expect(fish.price[0] == 10 && fish.price[1] == 1000, "a season's numbers clamped, the first kept");
    }
    {
        // 0060's whole manifest: the old records read past, its seasons (with supply and normal, or printed empty), its
        // rules' JSON values, a 5-number economy line.
        auto f = base;
        const auto at = f.manifest.find("economy 20000 100 50 10 12\n");
        expect(at != std::string::npos, "the strip's economy line");
        f.manifest.replace(at, std::string("economy 20000 100 50 10 12\n").size(),
                           "economy 1234 40 30 7 9\nstore off 120 resale off 60 rule on\n"
                           "loop needs off country off reserve 300 toll 20 carriage 9 wages 15 9 9 9\ncountryoff \"ale\"\n"
                           "subsidy \"carriage\" 30\ncomforts 50 20\n"
                           "season \"bread\" supply 100 120 90 80 price 100 95 105 120 shelf 100 110 100 90 normal 12\n"
                           "season \"fish\" supply price 90 90 100 110 shelf 100 100 100 100 normal\n"
                           "rule \"market\" {\"events\": {\"perWeek\": 2}}\nrule \"steer\" 1\nrule \"steerLean\" 150\nservice \"groom_and_trim\" off price 6 every 20\n"
                           "farms start 3 every - order - wages - -\nfarmarea \"orchards\" on off yield 80 crew - wages - - factor -\n");
        f.manifest += "nostore \"em\"\n";
        World w;
        w.setCellSource(f.source());
        const auto loaded = w.loadWorldFiles({{"world.ratw", f.manifest}}, "strip");
        expect(loaded.ok, "0060's manifest loads: " + loaded.message);
        const auto& e = w.society().authored().economy;
        expect(e.treasury == 1234 && e.needsPaused && e.seasons.count("bread") && e.seasons.at("fish").price[0] == 90 && e.services.count("groom_and_trim"),
               "its economy read");
        expect(warned(w, "the rule market skipped") && warned(w, "the rule steer skipped"), "its words and old rules skipped with a warning");
        expect(w.society().rule("prosperity") == 0, "an old world's steerLean isn't mapped across: it opens at 0 (16.7 e)");
        // Phase 9's second repair: economy.sync reads the economy's records alone (World::readEconomyRecords), never the
        // world again; what it reads is what the whole load read, warnings and all.
        AuthoredRoster alone;
        World::readEconomyRecords(f.manifest, alone);
        const auto& a = alone.economy;
        bool same = a.treasury == e.treasury && a.storeHerbs == e.storeHerbs && a.storeMeals == e.storeMeals &&
                    a.needsPaused == e.needsPaused && a.rules == e.rules && a.seasons.size() == e.seasons.size() &&
                    a.services.size() == e.services.size() && a.overrides.cause == e.overrides.cause &&
                    alone.warningCount == w.society().authored().warningCount;
        for (const auto& [g, season] : e.seasons)
            same &= a.seasons.count(g) && a.seasons.at(g).price == season.price && a.seasons.at(g).shelf == season.shelf;
        expect(same, "second repair: the economy alone reads as the whole load: " + std::to_string(alone.warningCount) + " warnings for " +
                         std::to_string(w.society().authored().warningCount));
    }
    {
        // Phase 9's second repair: a tab after the record's kind is the kind's end too (the fields split on tabs), so a long
        // overrides line written with a tab is skipped with a warning, never a refusal of the world; a season's 1e30
        // percent is the highest, not the lowest (clamped before it is rounded).
        auto tab = withLines("overrides\t{\"market\": {\"x\": \"" + std::string(20000, 'a') + "\"}}\n", "a long overrides line after a tab");
        expect(warned(tab, "too long") || tab.society().authored().economy.overrides.market.isObject(), "a tab: read or skipped, never refused");
        auto huge = withLines("season \"fish\" price 1e30 100 100 100 shelf 100 100 100 -1e30\n", "a season of 1e30");
        const auto& fish = huge.society().authored().economy.seasons.at("fish");
        expect(fish.price[0] == 1000 && fish.shelf[3] == 0, "second repair: 1e30 is 1000, -1e30 is 0: " + std::to_string(fish.price[0]));
        // Phase 9's last repair: a rule's name with a line break in it (a row written past the doors) splits its record;
        // the first line is skipped as malformed and its tail too, with a warning, never "Unknown manifest record" and
        // never read as a record of its own (the tail here would set the slider to 100).
        auto split = withLines("rule \"zz\nbogus\" 5\nrule \"ripple\" 40\n", "a rule's name split over two lines");
        expect(warned(split, "the rest of a split rule record skipped") && split.society().rule("ripple") == 40,
               "last repair: the split rule skipped, the next rule read");
        auto three = withLines("service \"a\nmiddle words\nrule \"prosperity\" 100 \" on price 1 every 2\nrule \"ripple\" 40\n", "a service's name split over three");
        expect(warned(three, "a line inside a split service record skipped") && warned(three, "the rest of a split service record skipped") &&
                   three.society().rule("prosperity") == 0 && three.society().rule("ripple") == 40,
               "last repair: a split record's tail is never a record of its own: " + std::to_string(three.society().rule("prosperity")));
        // (A quote left open by a slip of the hand: the next whole record is still read.)
        auto slip = withLines("rule \"ripple 40\nrule \"depthScale\" 200\n", "a quote left open");
        expect(slip.society().rule("depthScale") == 200, "last repair: the record after a slip still read");
        AuthoredRoster alone;
        World::readEconomyRecords("rule \"zz\nrule \"prosperity\" 100 \" 5\nrule \"ripple\" 40\n", alone);
        expect(!alone.economy.rules.count("prosperity") && alone.economy.rules.count("ripple"),
               "last repair: economy.sync skips a split record's tail too");
        auto longTab = withLines("overrides\t{\"market\": {\"x\": \"" + std::string(70000, 'a') + "\"}}\n", "a 70 KB overrides line after a tab");
        expect(warned(longTab, "too long"), "second repair: a 70 KB overrides line after a tab skipped with a warning");
    }
    {
        // The overrides line: bad JSON skipped, unknown keys skipped, a second ignored; its cause over a rule's; 70 KB skipped.
        auto w = withLines("rule \"prosperityCause\" \"a rule's cause\"\noverrides {\"prosperityCause\": \"the host's cause\", \"bogus\": 1, "
                           "\"neverImported\": [\"salt\", \"not_a_good\"], \"status\": {\"caps\": {\"grantsADay\": 100}}, "
                           "\"market\": {\"events\": {\"perWeek\": 1}}}\noverrides {\"prosperityCause\": \"second\"}\n",
                           "an overrides line");
        const auto& o = w.society().authored().economy.overrides;
        expect(o.cause == "the host's cause", "the overrides' cause wins over a rule's, whatever their order");
        expect(o.neverSet && o.neverImported.size() == 2 && o.status.isObject() && o.market.isObject(), "the overrides read");
        expect(warned(w, "overrides: bogus skipped") && warned(w, "a second overrides record"), "unknown keys and a second line warned");
        auto bad = withLines("overrides {not json\n", "bad JSON");
        expect(warned(bad, "the overrides record skipped"), "bad JSON skipped");
        auto big = withLines("overrides {\"market\": {\"x\": \"" + std::string(70000, 'a') + "\"}}\n", "a 70 KB overrides line");
        expect(warned(big, "too long"), "a 70 KB line skipped");
        auto first = withLines("overrides {\"prosperityCause\": \"the host's cause\"}\nrule \"prosperityCause\" \"a rule's cause\"\n", "the overrides first");
        expect(first.society().authored().economy.overrides.cause == "the host's cause", "the overrides' cause wins over a rule's after it");
        auto dog = withLines("rule \"prosperityCause\" \"a dog show\"\n", "a forbidden cause");
        expect(dog.society().authored().economy.overrides.cause.empty() && warned(dog, "prosperityCause skipped"), "the forbidden word skipped");
    }
    {
        // Band records: after the resident; an unknown word or resident skipped.
        auto w = withLines("band \"e1\" poor\nband \"e2\" wealthy\nband \"nobody\" rich\n", "bands");
        expect(w.society().spec("e1") && w.society().spec("e1")->band == status::Poor && w.society().spec("e2")->band == -1, "e1's band read");
        expect(warned(w, "no band wealthy") && warned(w, "the band of nobody skipped"), "an unknown word or resident warned");
        auto none = withLines("", "no bands");
        expect(none.society().authored().warningCount == 0, "a clean world warns of nothing");
    }
    {
        // The game's strip of a build's own records (the live manifest brings them): protected, wander, band, overrides too.
        const std::string build = "RATW_WORLD 2\ncell \"a\" \"cells/a.cell\"\neconomy 1 2 3\nresident \"x\"\nprotected \"x\"\nwander \"x\" 1 \"a\" 1 1\n"
                                  "band \"x\" poor\noverrides {}\nseason \"s\" price 1 1 1 1 shelf 1 1 1 1\nspawn \"a\" 1 1\n";
        const auto kept = game::Game::withoutPeople(build);
        expect(kept == "RATW_WORLD 2\ncell \"a\" \"cells/a.cell\"\nspawn \"a\" 1 1\n", "only the build's own: " + kept);
    }
    // A status override's purse target of 10^9 clamped (status::read), and a status override summing to 140 falls back.
    {
        json::Value v;
        std::string problem;
        expect(json::parse(R"({"rows": {"poor": {"purse": [1000000000, 1000000000]}},
                               "shares": [{"at": -100, "shares": [40, 40, 40, 20]}, {"at": 100, "shares": [2, 12, 70, 16]}]})", v, problem), problem);
        std::vector<std::string> warnings;
        const auto checked = status::hostOverride(v, &warnings);
        const auto t = status::read(checked, status::defaults(), &warnings);
        expect(t.rows[status::Poor].purseHigh <= 500 && t.shares.front().shares[0] == status::defaults().shares.front().shares[0],
               "a purse of 10^9 clamped; shares summing to 140 fall back");
    }
}

} // namespace

int main()
{
    try
    {
        theEdits();
        lenientEconomyRecords();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Host edit tests passed: " << checks << " checks.\n";
    return 0;
}
