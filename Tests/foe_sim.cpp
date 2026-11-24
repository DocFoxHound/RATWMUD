// Not a test: how strong a story's foes are (Docs/Design/86, C10t), on the real fight rules (Core/RatwBattle.cpp, road folk
// and story camps from Core/RatwStoryFoes.cpp, Data/Storylines/foes.json). Each fight is a World of its own: a foe (or a
// resident made one) with its band in the open yard, the wolves a few strides off; the first wolf sets on the leader, the
// rest join; the wolves play as level_sim plays them (their Gifts too), the foes as the game plays residents and bandits.
//
//   build-core/foe_sim [suite] [fights]          suites: foes (the bar), band, guards, quick, outlaw, scaled, all
//   build-core/foe_sim row "<row>" [fights]      one row: "foe <strength> band <n|scaled> vs <wolves> [guards <g>] [outlaw]"
//
// The wolves use level_sim's side syntax joined by commas ("plain@new", "plain@seasoned+", "fireq@veteran+"). Guards are the
// Watch's: fighting skill 70 (a guard's), an iron sword and the leather kit (doc 47's watch kit), played as the wolves are
// (the sim has no town to post them from). A scaled band is min(8, ceil(wolves / 2)) (doc 86, C1a's band from wolves present).
// SIM_FOES names a trial table (foes.json's shape), so a pass never edits Data/ until it is accepted; SIM_SEED varies the
// fights; SIM_THREADS caps the threads; 2,000 fights a row by default (500 for more than 8 wolves).
// Per row: the wolves' win % (95% interval), wolves downed and hurt per fight, fight seconds (median, p90), leader-first %,
// the band broken and run within 5 s %, foe fled %, 'foe defeated' raised exactly once %; and, for the bar, PASS or MISS.
#define main level_sim_main
#include "level_sim.cpp"
#undef main

#include "RatwStoryFoes.h"

#include <atomic>
#include <cinttypes>

namespace
{
struct Row
{
    std::string text, tier = "tough";
    int band = 0;
    bool scaled = false, outlaw = false;
    int guards = 0;
    std::vector<Wolf> wolves;
    std::vector<std::string> specs;
};

Row parseRow(const std::string& text)
{
    Row r;
    r.text = text;
    std::istringstream in(text);
    std::string word;
    while (in >> word)
    {
        if (word == "foe")
            in >> r.tier;
        else if (word == "band")
        {
            std::string n;
            in >> n;
            r.scaled = n == "scaled";
            r.band = r.scaled ? 0 : std::atoi(n.c_str());
        }
        else if (word == "vs")
        {
            std::string list;
            in >> list;
            for (std::size_t from = 0; from <= list.size();)
            {
                const auto comma = std::min(list.find(',', from), list.size());
                const auto one = list.substr(from, comma - from);
                // ("3*plain@seasoned+": three of a kind.)
                int times = 1;
                auto spec = one;
                if (const auto star = one.find('*'); star != std::string::npos)
                {
                    times = std::atoi(one.substr(0, star).c_str());
                    spec = one.substr(star + 1);
                }
                // ("plain@seasoned+", the spec's spelling: its gear before the band, as level_sim reads it, "plain+@seasoned".)
                if (const auto at = spec.find('@'); at != std::string::npos)
                    if (const auto plus = spec.find('+', at); plus != std::string::npos)
                        spec = spec.substr(0, at) + spec.substr(plus) + spec.substr(at, plus - at);
                for (int i = 0; i < times && !spec.empty(); ++i)
                {
                    r.wolves.push_back(parseWolf(spec, 1));
                    r.specs.push_back(spec);
                }
                from = comma + 1;
            }
        }
        else if (word == "guards")
            in >> r.guards;
        else if (word == "outlaw")
            r.outlaw = true;
    }
    if (r.scaled)
        r.band = std::min(8, int(std::ceil(r.wolves.size() / 2.0)));
    return r;
}

struct Fight
{
    bool won = false, decided = false, leaderDown = false, leaderFirst = false, brokeIn5 = false, fled = false;
    int defeated = 0;                               // 'foe defeated' events.
    int downed = 0, hurt = 0;                       // Wolves (and guards) down at the end; below half health.
    double seconds = 0;
    bool killedOutlaw = false;                      // (An outlaw resident dead: never, unless lethal.)
};

std::atomic<std::uint64_t> seedBase{0};

Fight fightOnce(const Row& row, int trial)
{
    World w;
    for (const auto& e : w.entities())
        if (e.second.npc)
            w.entity(e.first)->leaderId = "test-frozen";
    w.practising = false;
    wolves.clear();
    const std::string cell = "exterior";
    w.ensureLoaded(cell);
    const auto seed = std::to_string(seedBase.load()) + "-" + std::to_string(trial);
    // The foe, at the far end of the path; its band about it.
    const Vec2 post{31.5, 13.5};
    World::StoryFoeSpec spec;
    spec.camp = "foe_simsimsi_" + seed;
    spec.foe = "f" + seed;
    spec.run = "sim";
    spec.name = "The Captain";
    spec.cell = cell;
    spec.x = post.x;
    spec.y = post.y;
    spec.tier = row.tier;
    spec.band = row.band;
    spec.until = w.time() + 3 * 3600;
    std::string leader;
    if (row.outlaw)
    {
        // A resident of the demo world made a foe, brought to the yard first.
        for (const auto& [id, e] : w.entities())
            if (e.npc && !e.dead && !e.transient && w.society().resident(id))
            {
                leader = id;
                break;
            }
        auto* r = w.entity(leader);
        r->cellId = cell;
        r->position = post;
        r->leaderId.clear();
        spec.npc = leader;
        spec.camp = "foe_simsimsi_npc-" + seed;
        if (!w.outlawResident(spec).ok)
            return {};
        r->leaderId = "test-frozen";
    }
    else
    {
        if (!w.addStoryFoe(spec).ok)
            return {};
        leader = "road:" + spec.camp + ":0";
    }
    // The wolves (and any guards) in a block a few strides west of the foe.
    std::vector<std::string> ids;
    std::vector<Wolf> all = row.wolves;
    for (int g = 0; g < row.guards; ++g)
    {
        Wolf guard = geared(Wolf{}, "iron_sword", "leather");
        guard.skill = 70;
        guard.band = "guard";
        all.push_back(guard);
    }
    for (std::size_t i = 0; i < all.size(); ++i)
    {
        const auto& wolf = all[i];
        const std::string id = "player-" + std::to_string(i) + "-" + seed;
        auto& e = w.addPlayer(id, id);
        e.cellId = cell;
        e.position = {26.5 - double(i / 6), 11.5 + double(i % 6)};
        e.fightingSkill = wolf.skill >= 0 ? wolf.skill : levels::fightingSkill(wolf.level, wolf.quickened);
        if (!wolf.blade.empty())
        {
            e.mouth = "sword";
            e.swordKind = wolf.blade == "sword" ? std::string() : wolf.blade;
        }
        if (const auto kit = Kits.find(wolf.kit); kit != Kits.end())
            for (const auto& [slot, item] : kit->second)
                if (slot != "paws" || wolf.gift != "earth" || wolf.quickened)
                    e.worn[slot] = item;
        if (!wolf.gift.empty())
        {
            w.giveGift(id, wolf.gift, wolf.quickened);
            if (wolf.gift == "water")
            {
                w.society().openAccount(id);
                w.society().create(id, "water", 1, "a waterskin");
            }
        }
        wolves[id] = wolf;
        ids.push_back(id);
    }
    const auto started = w.attack(ids[0], leader);
    const auto* b = w.battleOf(ids[0]);
    if (!started.ok || !b)
        return {};
    const auto battleId = b->id;
    const int wolfSide = b->fighter(ids[0])->side;
    for (std::size_t i = 1; i < ids.size(); ++i)
        w.joinBattle(ids[i], battleId, wolfSide);
    for (const auto& id : ids)
        test::takeGround(w, id);
    Fight out;
    double begun = -1, leaderAt = -1;
    std::set<std::string> band;
    for (const auto& f : w.battleOf(ids[0])->fighters)
        if (f.side != wolfSide && f.id != leader)
            band.insert(f.id);
    for (double t = 0; t < 1800; t += .1)
    {
        const auto* now = w.battleOf(ids[0]);
        if (!now)
            for (const auto& id : ids)
                if ((now = w.battleOf(id)))
                    break;
        if (!now || now->over)
        {
            if (now && now->over && !out.decided)
            {
                out.decided = true;
                out.won = now->standing(wolfSide) > 0 && now->standing(1 - wolfSide) == 0;
                out.seconds = begun >= 0 ? w.time() - begun : 0;
            }
            if (!now)
                break;
        }
        if (now && !now->over)
        {
            if (begun < 0 && !now->placing())
                begun = w.time();
            for (const auto& id : ids)
                play(w, id);
            if (leaderAt >= 0 && !out.brokeIn5 && w.time() - leaderAt <= 5)
            {
                bool all = true;
                for (const auto& f : now->fighters)
                    if (band.count(f.id) && f.status == "fighting")
                        all = false;
                out.brokeIn5 = all;
            }
        }
        w.tick(.1);
        for (const auto& e : w.takeEvents())
        {
            if (e.kind == "foe defeated")
            {
                ++out.defeated;
                if (leaderAt < 0)
                {
                    leaderAt = w.time();
                    out.leaderDown = true;
                    // (Leader first: some of the band still on its feet when the leader fell.)
                    const Battle* bb = nullptr;
                    for (const auto& id : ids)
                        if ((bb = w.battleOf(id)))
                            break;
                    if (bb)
                        for (const auto& f : bb->fighters)
                            out.leaderFirst = out.leaderFirst || (band.count(f.id) && (f.status == "fighting" || f.status == "fled"));
                    out.leaderFirst = out.leaderFirst || band.empty();
                    // (The band broken at once: none of it still fighting the moment its leader fell.)
                    bool all = true;
                    if (bb)
                        for (const auto& f : bb->fighters)
                            if (band.count(f.id) && f.status == "fighting")
                                all = false;
                    out.brokeIn5 = all;
                }
            }
            if (e.kind == "foe fled" && e.item == "fled")
                out.fled = true;
        }
    }
    for (double t = 0; t < 1.5; t += .1)
    {
        // (A moment after the fight: the foe let go of, a fled one's 'foe fled' raised.)
        w.tick(.1);
        for (const auto& e : w.takeEvents())
        {
            out.defeated += e.kind == "foe defeated";
            out.fled = out.fled || (e.kind == "foe fled" && e.item == "fled");
        }
    }
    if (out.leaderDown && band.empty())
        out.brokeIn5 = true;
    // (A leader who broke off: its band runs at once with it.)
    for (const auto& id : ids)
        if (const auto* e = w.entity(id))
        {
            out.downed += e->downedLeft > 0 || e->hurt >= 100;
            out.hurt += e->hurt >= 50;
        }
    if (row.outlaw)
        if (const auto* r = w.entity(leader))
            out.killedOutlaw = r->dead;
    return out;
}

struct Result
{
    int fights = 0, decided = 0, wins = 0;
    double lo = 0, hi = 0, win = 0;
    double downed = 0, hurt = 0, downedPerWin = 0;
    double median = 0, p90 = 0;
    double leaderFirst = 0, broke = 0, fled = 0, once = 0, killed = 0;
};

// Wilson's 95% interval for k of n.
void wilson(int k, int n, double& lo, double& hi)
{
    if (n <= 0)
    {
        lo = hi = 0;
        return;
    }
    const double z = 1.96, p = double(k) / n, d = 1 + z * z / n;
    const double c = (p + z * z / (2 * n)) / d, h = z * std::sqrt(p * (1 - p) / n + z * z / (4.0 * n * n)) / d;
    lo = 100 * std::max(0.0, c - h);
    hi = 100 * std::min(1.0, c + h);
}

Result runRow(const Row& row, int fights)
{
    std::vector<Fight> out(std::size_t(std::max(0, fights)));
    int threads = std::max(1, std::min(fights, int(std::thread::hardware_concurrency())));
    if (const char* most = std::getenv("SIM_THREADS"))
        threads = std::max(1, std::min(threads, std::atoi(most)));
    std::vector<std::thread> pool;
    for (int k = 0; k < threads; ++k)
        pool.emplace_back([&, k] {
            for (int i = k; i < fights; i += threads)
                out[std::size_t(i)] = fightOnce(row, i);
        });
    for (auto& t : pool)
        t.join();
    Result r;
    r.fights = fights;
    std::vector<double> lengths;
    int leaderDowns = 0, firsts = 0, broke = 0, once = 0, fled = 0, killed = 0, downedInWins = 0;
    for (const auto& f : out)
    {
        r.decided += f.decided;
        r.wins += f.decided && f.won;
        r.downed += f.downed;
        r.hurt += f.hurt;
        if (f.decided && f.won)
            downedInWins += f.downed;
        if (f.decided)
            lengths.push_back(f.seconds);
        leaderDowns += f.leaderDown;
        firsts += f.leaderDown && f.leaderFirst;
        broke += f.leaderDown && f.brokeIn5;
        once += f.leaderDown && f.defeated == 1;
        fled += f.fled;
        killed += f.killedOutlaw;
    }
    wilson(r.wins, r.decided, r.lo, r.hi);
    r.win = r.decided ? 100.0 * r.wins / r.decided : 0;
    r.downed /= std::max(1, fights);
    r.hurt /= std::max(1, fights);
    r.downedPerWin = r.wins ? double(downedInWins) / r.wins : 0;
    std::sort(lengths.begin(), lengths.end());
    if (!lengths.empty())
    {
        r.median = lengths[lengths.size() / 2];
        r.p90 = lengths[std::min(lengths.size() - 1, lengths.size() * 9 / 10)];
    }
    r.leaderFirst = leaderDowns ? 100.0 * firsts / leaderDowns : 0;
    r.broke = leaderDowns ? 100.0 * broke / leaderDowns : 100;
    r.once = leaderDowns ? 100.0 * once / leaderDowns : 100;
    r.fled = 100.0 * fled / std::max(1, fights);
    r.killed = 100.0 * killed / std::max(1, fights);
    return r;
}

int fightsFor(const Row& row, int given)
{
    if (given > 0)
        return given;
    return row.wolves.size() + std::size_t(row.guards) > 8 ? 500 : 2000;
}

struct Bar
{
    double lo = -1, hi = 101;                       // The band the win % 95% interval must lie in.
    double mostDownedPerWin = -1;
    double overRow = -1, overLo = 0, overHi = 0;    // "8–15 points over the row before": the index of that row.
    std::string why;
};

int misses = 0;

void print(const Row& row, const Result& r, const Bar* bar, const Result* before)
{
    std::string verdict;
    if (bar)
    {
        bool ok = true;
        if (bar->overRow >= 0 && before)
        {
            const double gain = r.win - before->win;
            ok = gain >= bar->overLo && gain <= bar->overHi;
            verdict = (ok ? "PASS" : "MISS") + std::string(" (") + std::to_string(int(std::lround(gain))) + " points over the row before)";
        }
        else
        {
            ok = r.lo >= bar->lo && r.hi <= bar->hi;
            if (bar->mostDownedPerWin >= 0)
                ok = ok && r.downedPerWin <= bar->mostDownedPerWin;
            verdict = ok ? "PASS" : "MISS";
        }
        if (row.outlaw)
            ok = ok && r.killed == 0;
        misses += !ok;
        if (!ok && verdict.rfind("PASS", 0) == 0)
            verdict = "MISS" + verdict.substr(4);
    }
    std::printf("%-70s win %5.1f%% [%5.1f, %5.1f]  down %.2f (%.2f/win) hurt %.2f  %3.0fs/p90 %3.0fs  lead-first %3.0f%%  broke<5s %3.0f%%  "
                "fled %4.1f%%  once %3.0f%%%s  n=%d%s%s\n",
                row.text.c_str(), r.win, r.lo, r.hi, r.downed, r.downedPerWin, r.hurt, r.median, r.p90, r.leaderFirst, r.broke, r.fled, r.once,
                row.outlaw ? (r.killed == 0 ? "  outlaw never killed" : "  OUTLAW KILLED") : "", r.decided,
                bar ? ("  band " + std::to_string(int(bar->lo < 0 ? 0 : bar->lo)) + "-" + std::to_string(int(bar->hi > 100 ? 100 : bar->hi)) +
                       "  " + verdict).c_str() : "",
                bar && !bar->why.empty() ? ("  (" + bar->why + ")").c_str() : "");
    std::fflush(stdout);
}

struct BarRow
{
    std::string row;
    Bar bar;
};

// The bar (engine spec C10t): every row's 95% interval inside its band.
std::vector<BarRow> theBar()
{
    const std::string S = "plain@seasoned+";
    return {
        {"foe follower band 0 vs plain@new", {50, 65, -1, -1, 0, 0, "a follower, 1 v 1, a new wolf bare"}},
        {"foe follower band 0 vs " + S, {75, 90, -1, -1, 0, 0, "a follower, 1 v 1, a seasoned wolf armed"}},
        {"foe tough band 0 vs " + S, {25, 40, -1, -1, 0, 0, "a lone wolf should look for help"}},
        {"foe tough band 2 vs 3*" + S, {85, 95, .5, -1, 0, 0, "three seasoned"}},
        {"foe tough band 2 vs 3*plain@new", {50, 70, -1, -1, 0, 0, "three new, bare"}},
        {"foe fierce band 4 vs 3*" + S, {50, 65, -1, -1, 0, 0, ""}},
        {"foe fierce band 4 vs 5*" + S, {80, 92, -1, -1, 0, 0, ""}},
        {"foe fierce band 4 vs 5*" + S + " guards 2", {-1, 101, -1, 6, 8, 15, "guards help, never carry"}},
        {"foe fierce band 4 vs 2*" + S + ",fireq@veteran+", {-1, 85, -1, -1, 0, 0, "a Quickened lifts a party"}},
        {"foe champion band 8 vs 5*" + S, {25, 45, -1, -1, 0, 0, ""}},
        {"foe champion band 8 vs 8*" + S + " guards 2", {70, 85, -1, -1, 0, 0, "Ch 9's Pass"}},
        {"foe champion band 0 vs fireq@veteran+", {-1, 35, -1, -1, 0, 0, "no wolf solos the Grey Captain"}},
        {"foe fierce band scaled vs 4*" + S, {60, 85, -1, -1, 0, 0, "a crowd is not a walkover"}},
        {"foe fierce band scaled vs 8*" + S, {60, 85, -1, -1, 0, 0, ""}},
        {"foe fierce band scaled vs 12*" + S, {60, 85, -1, -1, 0, 0, ""}},
        {"foe fierce band scaled vs 16*" + S, {60, 85, -1, -1, 0, 0, ""}},
        {"foe tough band 2 vs 3*" + S + " outlaw", {85, 95, -1, -1, 0, 0, "as tough band 2; the resident never killed"}},
    };
}

// The fight lengths' bar: median tough 20-60 s, fierce 40-120 s, champion 90-240 s (over the rows of that strength).
void lengths(const std::vector<std::pair<Row, Result>>& rows)
{
    const std::map<std::string, std::pair<double, double>> want = {{"tough", {20, 60}}, {"fierce", {40, 120}}, {"champion", {90, 240}}};
    for (const auto& [tier, band] : want)
    {
        std::vector<double> m;
        for (const auto& [row, r] : rows)
            if (row.tier == tier && !row.outlaw)
                m.push_back(r.median);
        if (m.empty())
            continue;
        std::sort(m.begin(), m.end());
        const double mid = m[m.size() / 2];
        const bool ok = mid >= band.first && mid <= band.second;
        misses += !ok;
        std::printf("fight length, %-8s median of its rows' medians %5.0fs  band %.0f-%.0fs  %s\n", tier.c_str(), mid, band.first, band.second,
                    ok ? "PASS" : "MISS");
    }
    // The leader rule over every row: the band breaks within 5 s >= 95%; 'foe defeated' once 100%.
    double broke = 100, once = 100;
    for (const auto& [row, r] : rows)
    {
        broke = std::min(broke, r.broke);
        once = std::min(once, r.once);
    }
    const bool ok = broke >= 95 && once >= 100;
    misses += !ok;
    std::printf("leader down: band broke within 5 s, worst row %5.1f%% (>= 95%%); 'foe defeated' once, worst row %5.1f%% (100%%)  %s\n", broke, once,
                ok ? "PASS" : "MISS");
}

void suite(const std::string& name, int given)
{
    const std::string S = "plain@seasoned+";
    if (name == "foes")
    {
        const auto bar = theBar();
        std::vector<std::pair<Row, Result>> done;
        for (std::size_t i = 0; i < bar.size(); ++i)
        {
            const auto row = parseRow(bar[i].row);
            const auto r = runRow(row, fightsFor(row, given));
            const Result* before = bar[i].bar.overRow >= 0 ? &done[std::size_t(bar[i].bar.overRow)].second : nullptr;
            print(row, r, &bar[i].bar, before);
            done.push_back({row, r});
        }
        lengths(done);
        std::printf("the bar: %s (%d row%s out)\n", misses ? "MISSED" : "PASSED", misses, misses == 1 ? "" : "s");
        return;
    }
    std::vector<std::string> rows;
    if (name == "band")
        for (int band : {0, 2, 4, 6, 8})
            for (int n : {1, 3, 5, 8, 12})
                rows.push_back("foe fierce band " + std::to_string(band) + " vs " + std::to_string(n) + "*" + S);
    else if (name == "guards")
        for (const auto* tier : {"tough", "fierce", "champion"})
            for (int g : {0, 1, 2, 4})
                rows.push_back(std::string("foe ") + tier + " band 4 vs 5*" + S + " guards " + std::to_string(g));
    else if (name == "quick")
        rows = {"foe fierce band 4 vs 2*" + S + ",fireq@veteran+", "foe fierce band 4 vs 2*" + S + ",earthq@veteran+",
                "foe fierce band 4 vs 2*" + S + ",windq@veteran+", "foe champion band 0 vs fireq@veteran+", "foe champion band 0 vs earthq@veteran+",
                "foe champion band 4 vs 4*" + S + ",fireq@veteran+"};
    else if (name == "outlaw")
        for (const auto* tier : {"tough", "fierce", "champion"})
            rows.push_back(std::string("foe ") + tier + " band 2 vs 3*" + S + " outlaw");
    else if (name == "scaled")
        for (int n : {4, 8, 12, 16})
            rows.push_back("foe fierce band scaled vs " + std::to_string(n) + "*" + S);
    for (const auto& text : rows)
    {
        const auto row = parseRow(text);
        print(row, runRow(row, fightsFor(row, given)), nullptr, nullptr);
    }
}
} // namespace

int main(int argc, char** argv)
{
    const std::string what = argc > 1 ? argv[1] : "foes";
    if (const char* seed = std::getenv("SIM_SEED"))
        seedBase = std::strtoull(seed, nullptr, 10);
    tally = false;
    if (!ratw::foes::loadError().empty())
        std::fprintf(stderr, "foes.json: %s (the starting table)\n", ratw::foes::loadError().c_str());
    std::printf("foe table: tough %.0f/%.0f/%.0f/%.0f fierce %.0f/%.0f/%.0f/%.0f champion %.0f/%.0f/%.0f/%.0f follower %.0f/%.0f/%.0f/%.0f "
                "(skill/strength/hp/flee)%s\n",
                ratw::foes::strength("tough")->skill, ratw::foes::strength("tough")->strength, ratw::foes::strength("tough")->hp,
                ratw::foes::strength("tough")->flee, ratw::foes::strength("fierce")->skill, ratw::foes::strength("fierce")->strength,
                ratw::foes::strength("fierce")->hp, ratw::foes::strength("fierce")->flee, ratw::foes::strength("champion")->skill,
                ratw::foes::strength("champion")->strength, ratw::foes::strength("champion")->hp, ratw::foes::strength("champion")->flee,
                ratw::foes::strength("follower")->skill, ratw::foes::strength("follower")->strength, ratw::foes::strength("follower")->hp,
                ratw::foes::strength("follower")->flee, std::getenv("SIM_FOES") ? (std::string(" from ") + std::getenv("SIM_FOES")).c_str() : "");
    if (what == "row")
    {
        if (argc < 3)
        {
            std::fprintf(stderr, "foe_sim row \"foe <strength> band <n|scaled> vs <wolves> [guards <g>] [outlaw]\" [fights]\n");
            return 2;
        }
        const auto row = parseRow(argv[2]);
        print(row, runRow(row, fightsFor(row, argc > 3 ? std::atoi(argv[3]) : 0)), nullptr, nullptr);
        return 0;
    }
    const int fights = argc > 2 ? std::atoi(argv[2]) : 0;
    if (what == "all")
        for (const auto* s : {"foes", "band", "guards", "quick", "outlaw", "scaled"})
            suite(s, fights);
    else if (what == "foes" || what == "band" || what == "guards" || what == "quick" || what == "outlaw" || what == "scaled")
        suite(what, fights);
    else
    {
        std::fprintf(stderr, "foe_sim [foes|band|guards|quick|outlaw|scaled|all] [fights] | foe_sim row \"...\" [fights]\n");
        return 2;
    }
    return 0;
}
