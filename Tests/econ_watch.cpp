// Watches the economy of a streamed world export left to its residents alone (no players), for as many game days as
// asked (Docs/Design/42-money-in-circulation.md: "a month-long run"). The world runs as world_check runs it (20 ticks a
// second, simulation tiers on), and every coin and good that moves is tallied from the society's journal.
//
//   econ_watch EXPORT_DIR DAYS OUT_DIR [--from HOUR] [--deterministic] [--threads N] [--hours] [--step SECONDS]
//              [--players SCENARIO[,SCENARIO...]] [--players-from DAY] [--players-days N] [--players-bodies HOW]
//              [--players-staple ITEM] [--hold-pace PACE_CSV] [--ask-payers] [--without RULE[,RULE...]]
//              [--stop-on-sentinel KINDS|all] [--model-shares FILE] [--sentinel-weeks N]
//
// --from HOUR: the hour of the first day to start at (6 unless asked); past 24, hours into the calendar (6006: day 250
// at six in the morning).
// --hours: residents' work takes its calibrated crafts.json hours (Docs/Design/64-economy-migration.md, Phase 3); without
// it, the game's old pace (each craft's `seconds` as its hours).
// --step SECONDS: the world seconds each of the watch's ticks (0.05 unless asked: 20 a second, as world_check's), the
// days asked kept. World::tick moves the world in steps of 1/30 s at most whatever it is given, and its schedules come
// every half second of world time, so a longer step saves only each tick's own work (streaming, views, the journal,
// this watch's tallies); past half a second, the scripted players' looks (twice a world second) come once a tick.
// --players: scripted players who push (doc 64, Phase 13; Tests/econ_players.h has each): hoard, corner, dump, whale,
// poach, kill, theft, visit, winter, flood, strike, buyout, drain, gift. They act from --players-from DAY (7 unless
// asked: the realm's money stands from day 2;
// a run that starts later starts them on its first day, and says so) for --players-days N days from when they start (to
// the run's end unless asked), as the game lets players act, with money from the capital's treasury by the ledger; then
// they leave (their bodies out of the world, a Hold's hands back to their lives). --players-bodies: how theft's and
// visit's bodies get about: walk (unless asked: up to each mark, as a player walks), still (where they arrived), or jump
// (World::teleport beside each mark, a Dungeon Master's move: the first runs', to compare). --players-staple: what
// flood sells (raw_meat unless asked).
// --hold-pace: the realm's pace (memory.pace, Society::holdPace) each day as another run's pace.csv has it, whatever this
// run's hunger, food and hands would make it: a run with players set beside the run without them differs by what the
// players did, not by the pace they moved (doc 64, Phase 13: strike's starving hands held the pace at 0 for a month).
// It wants --hours (the pace is stepped only there) and a pace for every society's day the run turns; else refused.
// --ask-payers: asks Society::payerOf of every resident with a post before the first tick and every game hour, as a talk
// scene or a harness might, and writes payers.csv (doc 64, Phase 13, round 6: who pays the capital's posts). A run with it
// must give the same digest as one without: what is asked of the society, read only, never moves it.
// --without: a trial's, round 7's rules of the wage book left out (doc 64, Phase 13, round 7; Society::wageRulesOff):
// children (a hire's place a child holds counts filled, as before), employees (the going rate weighed by hands wanted,
// keeper and all, as before), stalls (a stall never lets its help go, as before); and round 8's: sellers (every business
// with no craft lets its surplus help go, not only the stalls and the moneychangers), apprentices (an apprentice is let
// go as any hand), unpaid (the out of work look for crews from waking too), caravans (a town's budget counts its tax
// only, not its caravans' net takings). With round 8's four left out, a run is round 7's (1e192c0) to the digest. And
// round 9's: weekly (a town's caravans' net takings in its month, as round 8, not on top of its week's budget), giveway
// (a field's far taker on its way keeps its place), masters (any apprentice is kept, as round 8, not only the keeper's).
// With those three left out, a run is round 8's (f13f80e) to the digest. And round 9's follow-ups: posts (a town's
// caravans' week goes to its works with its posts paid from the tax budget and its purse, not its posts' shortfall
// first), crews (a town's works crew is its own town's alone), oneplace (a wolf done with its day's share of a hire may
// take a second). With all six left out, a run is round 8's to the digest.
// And round 9's tills: owners (a lone owner tops up a business short of its wages as before). And round 10's, lane A:
// houses (a House's budget pays its retainers, finery, works and estate crew only; its posts, caravans and investments
// draw on its purse as before), housetills (a House's business it doesn't top up isn't free to trade on as a lone
// owner's: no record, its manager not free for hire), church (the church's alms bought on top of its relief, no floor),
// cheapalms (alms buy the shop's most nourishing food, not the cheapest that feeds), carry (a House's budget unpaid
// last week stays in its purse, not carried to its works and crews). With those five left out, a run is
// 4ef067b's to the digest.
//
// The log starts with the run's stamp (also run.txt): the command, the git state the binary was built from (taken at
// build time: Tests/econ_watch_stamp.cmake), and when it was built; a warning if the code it was built from has
// changed since (not the docs), or a note if that can't be told; and the Data it reads, with its git state now (a line
// to read, not a warning: Data is read at run time).
// OUT_DIR gets, for each game day (a day's record, N, made at its midnight: the state at the end of day N-1 and what
// happened in it; the run's last, "end", where it stops, with no rows of its own in the per-day CSVs):
//   days.jsonl      money by holder, every collector's and till's purse, residents' purses (median, poorest and richest
//                   tenth, Gini, shares), hunger, contracts, each town's residents (held, median, short, broke, hungry,
//                   starving), and whether money is conserved;
//   flows.csv       day, kind, from-holder, to-holder: entries, coins, goods;
//   wages.csv       day, resident, position title, wages received;
//   materials.csv   day, good: maker-hours short of it, supplier-hours short of it, the stock held world-wide, brought
//                   in, crafted, used in crafting, bought, carted in;
//   events.csv      day, kind, detail: everything the world logged that wasn't a plain ledger entry;
//   food.csv        day, food: brought in, crafted, used in crafting, eaten, spoiled, bought by residents (counts);
//   food_towns.csv  day, town, food: brought in, crafted, used, eaten, spoiled, bought by residents and what they paid
//                   (a good made or used up where its holder is; a sale where its seller is);
//   prices.csv      day, good, kind of sale: units and coins (the average price paid, against the catalog's);
//   larders.csv     day, town: households, people, their days of food on hand (larder and pockets, at 50 a wolf a
//                   day: mean, and how many households hold under 1, 1 to 2, 2 to 5, over 5), and the food (in
//                   nourishment) on its shops' shelves and with its producers;
//   food_moves.csv  day, kind, from-holder, to-holder, food: units (every movement of food, by why and between whom);
//   town_flows.csv  day, town, kind, from-holder, to-holder: coins that move between two of one town's accounts;
//   trade.csv       day, from-town, to-town, kind: entries, coins, goods, for coins that change towns (a resident's town
//                   is its home's; a shop's, where its keeper works; the shared church, the capital's treasury, the great
//                   houses and the roads count as places of their own);
//   work.csv        day, account: its hours of work credited and lost, the till it works for (a producer's work is the
//                   farmer's, its hands its till's), and its hours in hand at the day's turn;
//   tills.csv       day, till: its cash, working money, what it owes, its sales and orders (doc 64, Phase 5), and its
//                   hands, hands wanted, and its running hires' slots and slots taken (each till once a day); and
//                   (round 9) its owner's weekly looks in a row that found it short of a week's wages, the day its
//                   owner stopped topping it up (-1 if it hasn't), and what its keeper's odd jobs paid it that day;
//   residents.csv   day, resident: home, kind of holder, title, age, cash;
//   books.csv       day, employer: its book (wage, the going rate for its skill, open days, calm weeks, short pay), its
//                   posts open and filled, its skill (doc 64, Phase 6);
//   hires.csv       day, hire (at 23:00): its payer, kind, slots, pay a slot, last day, slots taken and those open at
//                   the day's dawn, and who took them (each as jobless, child, works, owner (a keeper its till can't pay,
//                   its owner stopped: round 9), idle, called, better or other, and its own employer, as it stands at
//                   23:00), and of them those who worked the day's spells, those
//                   from another town, those of them who worked, and those lodged away from home (doc 64, Phase 13:
//                   a hire taken from afar);
//   gaveway.csv     day, hour, each taker of a field's full place that gave way to a nearer hand: its town, how far it
//                   lives from the field (tiles, straight across), the field's till and town, and the hand it gave way
//                   to and its town (doc 64, Phase 13, round 9);
//   payers.csv      (--ask-payers) day, resident: its home and work cells, title, who pays it and as what, at the day's
//                   record;
//   pace.csv        day, "turn", the society's day, the realm's pace that day, to the last digit, at each of the
//                   society's days' turns (--hold-pace's to read; a turn can fall on either side of the watch's
//                   midnight, so not at the day's records);
//   orders.csv      day, the roads' standing orders: buyer, town, seller's town, good, a week's, delivered, short, unpaid;
//   refusals.csv    day, buyer, good, the day its refusal was marked, its till's cash: the buyers whose order was cut or
//                   ended for what they couldn't pay for (RoadsState::refused), at each day's end;
//   shortpay.csv    day, employer, what (a hand's spell of "wage", a "hire"'s day), times, owed, its purse when it fell
//                   short (the least), and at the day's end: each employer marked short of pay that day, for its weekly
//                   review (doc 64, Phase 13, round 7);
//   players.csv     day, player, scenario, action, item, quantity, coins: what --players' players did (kill's rows name
//                   the resident who died; coins are pennies that moved, but for a Hold's "short", whose quantity is
//                   the pennies it lacked, item "pennies lacking").
// And the run's tracking (doc 64, Phase 13, round 10, lane 0), weekly from the run's first day (each week's rows at
// the record of its last midnight, and the run's last part-week at its end), for one run's files to tell where the
// money and goods went without scratch scripts (tools/econ_model/report.py reads them):
//   accounts.csv    day, account, holder, town, its balance a week before (opening) and now, coins in and out in the
//                   week and the entries, and what of its balance's change they don't explain (0 when the journal has
//                   every coin): every House, town treasury, town account (watch, works, church ...), fund, the
//                   capital's treasury, the church, every caravan (its town the one it set out from) and every till
//                   (a keeper's own purse that is its shop's till too);
//   account_flows.csv  day, account, in or out, kind: entries, coins (the same accounts, by kind of entry);
//   account_peers.csv  day, account, in or out, peer, the peer's holder, coins: each account's five biggest payers
//                   and payees of the week, and the rest as one "(others: N)";
//   goods.csv       day, town, good (its base): what's held at the week's end on shelves (tills), in homes (larders
//                   and pockets) and in stores (the towns', the Houses', caravans' ...), and in the week: made
//                   (crafted), brought in (from the land, hunted, a year's store ...), used in crafting, eaten (and
//                   drunk), spoiled, worn out or used up otherwise, carried in from and out to other places (a
//                   caravan's is "(road)"), sold (units and coins) and its mean price;
//   hunger.csv      day, town: residents, hungry, starving, and of them those holding less than the cheapest food a
//                   resident paid for there in the last week (money-hunger; the rest is food-hunger), its shops'
//                   shelves' food (units), and that cheapest price (each day's record);
//   sentinels.csv   day, kind, subject, value, threshold, weeks, detail: each sentinel fired at a week's check (and
//                   in the log): drain (a House, a town's or the capital's treasury, the church or a caravan paying
//                   out over 1.3 times what came in, at least 100p, four weeks in a row; with its top three kinds
//                   out), share (a kind of holder's share of the money, as compare.py groups them, off the model's
//                   (tools/econ_model/model_shares.csv) by over half again or under two-thirds, and by over 2 points,
//                   four weeks in a row, from the fourth week), hoard (a treasury or the church holding over three
//                   months of its income: the last four weeks' in, each payer's net of what it was paid back, so a
//                   caravan's road money comes off its takings; and more than it held four weeks before), hunger (a
//                   town with hungry wolves, at least 5 and 1 in 100, while its shelves hold a unit of food for each), gini (the households' Gini up over 0.02
//                   on 30 days before). Each fires once and again only after its condition has cleared.
// --stop-on-sentinel KINDS|all: the run ends at the first week's check one of those fires (drain, share, hoard, hunger,
// gini), every file written to there, as a run's end. --model-shares FILE: the model's shares the share sentinel holds
// the run to (tools/econ_model/model_shares.csv unless asked; without it, that sentinel is off, and the log says so).
// --sentinel-weeks N: the weeks in a row a drain or a share takes to fire, and the weeks of income a hoard is weighed
// on (4 unless asked: a month; 1, for a trial's or a test's short run).
// days.jsonl's going is the realm's going rate ("") and each town's as its employers pay it (a town without a rate of
// its own, the realm's: doc 64, Phase 13, round 7); its model block has the rates the society keeps (going, "" the
// realm's, and the towns with their own), and with --players a players block:
// each scenario's players, their money and goods, and what they did that day (the players act after the midnight's
// record is made, so what they do at midnight is the new day's, in every file).
// A line a day goes to stdout, so a long run can be followed. --deterministic does the world's work by counts, never the
// clock (World::setDeterministic): the run repeats exactly, and ends with a digest of where everyone is and what they do.
// --threads N: how many threads the world's work is spread over (8 unless asked: four cores and their twins; 1, none but
// the main one). The same run gives the same digest on any number.
#include "RatwItems.h"
#include "RatwPool.h"
#include "RatwWorld.h"
#include "econ_players.h"
#include "econ_watch_stamp.h"                       // (Made at build time, beside the build: RATW_BUILT_*.)

#include <sys/stat.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

using namespace ratw;
namespace fs = std::filesystem;

namespace
{
std::string slurp(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}
std::string csv(const std::string& s)
{
    std::string out = "\"";
    for (const char c : s)
        out += c == '"' ? std::string("\"\"") : std::string(1, c);
    return out + "\"";
}
std::string json(const std::string& s)
{
    std::string out = "\"";
    for (const char c : s)
        if (c == '"' || c == '\\')
            out += std::string("\\") + c;
        else if (static_cast<unsigned char>(c) >= 0x20)
            out += c;
    return out + "\"";
}

// The run's stamp (doc 64, Phase 13: two runs once straddled a fix unseen): how it was run, the git state this binary
// was built from (econ_watch_stamp.h, written at each build: Tests/econ_watch_stamp.cmake), and when it was built; and a
// warning when the code it was built from has changed since (econ::sourceDrift: a docs edit is none), or a note when
// that can't be told.
std::string runStamp(int argc, char** argv)
{
    std::ostringstream out;
    out << "run:";
    for (int i = 0; i < argc; ++i)
        out << ' ' << argv[i];
    out << "\nbuilt from: " << RATW_BUILT_SOURCE << " at " << RATW_BUILT_DESCRIBE << " (" << RATW_BUILT_COMMIT << ")";
    if (const auto drift = econ::sourceDrift(RATW_BUILT_SOURCE, RATW_BUILT_COMMIT, RATW_BUILT_CODE, RATW_BUILT_PATHS); !drift.empty())
        out << "\n" << drift;
    out << "\n" << econ::dataState(econ::dataDir(RATW_SOURCE_DIR));   // (The Data it reads, as it stands now: the review's.)
    struct stat built{};
    char when[64] = "(unknown)";
    if (::stat("/proc/self/exe", &built) == 0)
    {
        const std::time_t t = built.st_mtime;
        std::strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S UTC", std::gmtime(&t));
    }
    out << "\nbuilt: " << when << " (linked; econ_watch.cpp compiled " << __DATE__ << " " << __TIME__ << ", local time)\n";
    return out.str();
}

// What kind of holder an account is: a resident by its work, or a facility by its prefix.
std::string holder(const World& server, const std::string& id)
{
    const auto& society = server.society();
    if (id == "treasury")
        return "capital treasury";
    if (id == "outside")
        return "outside";
    if (id.rfind("stores:", 0) == 0)
        return "town treasury";
    if (id.rfind("town:", 0) == 0)
        return "town " + id.substr(id.rfind(':') + 1);
    if (id.rfind("home:", 0) == 0)
        return "home stores";
    if (id.rfind("till:", 0) == 0)
    {
        // A great house's business's till, an owner-run shop's, or a farm's or site's (doc 46, Phase 2).
        if (society.state().houses.owner.count(id.substr(5)))
            return "house till";
        const auto* p = society.position(id.substr(5));
        return p && p->role != "merchant" ? "farm till" : "shop till";
    }
    if (id.rfind("house:", 0) == 0)
        return "great house";
    if (playerAccountId(id))
        return "player";
    if (facilityAccount(id))
        return id.substr(0, id.find(':'));
    if (society.spec(id))
    {
        const auto* job = society.jobOf(id);
        const auto* e = server.entity(id);
        // (A producer, bringing goods in from the land, apart from the rest of the paid: doc 42, "Pressure".)
        const auto* r = society.spec(id);
        return job ? (job->role == "merchant"                       ? "merchant"
                      : job->role == "guard"                        ? "guard"
                      : r && items::producerFor(r->workLabel)       ? "producer"
                      : job->paid                                   ? "paid civilian"
                                                                    : "unpaid civilian")
             : society.apprenticedTo(id) ? "apprentice"
             : e && e->age < 16          ? "child"
                                         : "adult without work";
    }
    return id.empty() ? "nobody" : "other";
}

struct Flow
{
    std::int64_t entries = 0, coins = 0, goods = 0;
};
struct Material
{
    std::int64_t makerShort = 0, supplierShort = 0, held = 0, broughtIn = 0, crafted = 0, used = 0, bought = 0, carted = 0;
};
// An account's week (round 10, lane 0): its coins in and out by kind, and by whom.
struct AccountWeek
{
    std::map<std::string, Flow> in, out;
    std::unordered_map<std::string, std::int64_t> from, to;
};
// A town's good's week: what's held at its end, and what was made, used, moved and sold in it.
struct GoodsWeek
{
    std::int64_t shelf = 0, homes = 0, stores = 0, made = 0, broughtIn = 0, used = 0, eaten = 0, spoiled = 0, worn = 0,
                 carriedIn = 0, carriedOut = 0, sold = 0, soldCoins = 0;
};
// Entries that move goods with coins the same way (Society::shift), not a sale's (Society::transfer: the buyer pays
// the seller, the goods go the other way). Of those an NPC world makes, inheritance and theft move both.
bool bothForward(const std::string& kind)
{
    return kind == "inheritance" || kind == "stolen" || kind == "sale of a business" || kind == "a house no more" ||
           kind == "the shop's till" || kind == "a gift" || kind == "picked up" || kind == "operator transfer";
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        std::cerr << "usage: econ_watch EXPORT_DIR DAYS OUT_DIR [--from HOUR] [--deterministic] [--threads N] [--hours] [--step SECONDS]\n"
                     "                  [--players SCENARIO[,SCENARIO...]] [--players-from DAY] [--players-days N]\n"
                     "                  [--players-bodies walk|still|jump] [--players-staple ITEM] [--hold-pace PACE_CSV]\n"
                     "                  [--ask-payers] [--without children|employees|stalls[,...]]\n"
                     "                  [--stop-on-sentinel drain|share|hoard|hunger|gini[,...]|all] [--model-shares FILE]\n"
                     "                  [--sentinel-weeks N]\n";
        return 2;
    }
    const fs::path root = argv[1];
    const double days = std::atof(argv[2]);
    const fs::path out = argv[3];
    double from = 6;
    double step = .05;                              // --step: world seconds a tick.
    bool deterministic = false;
    unsigned threads = 8;
    std::vector<std::string> scenarios;             // --players (doc 64, Phase 13).
    std::int64_t playersFrom = 7, playersDays = 0;
    auto bodies = econ::Players::Bodies::Walk;
    std::string staple = econ::Players::DefaultStaple;
    std::map<std::int64_t, double> heldPace;        // --hold-pace: the society's day -> its pace.
    bool askPayers = false;                         // --ask-payers.
    std::set<std::string> stopOn;                   // --stop-on-sentinel (round 10, lane 0).
    int sentinelWeeks = 4;                          // --sentinel-weeks.
    fs::path modelShares = fs::path(RATW_SOURCE_DIR) / "tools" / "econ_model" / "model_shares.csv";
    for (int i = 4; i < argc; ++i)
        if (std::string(argv[i]) == "--from" && i + 1 < argc)
            from = std::atof(argv[++i]);
        else if (std::string(argv[i]) == "--deterministic")
            deterministic = true;
        else if (std::string(argv[i]) == "--hours")
            Society::setWorkByHours(true);          // Residents' work takes its calibrated hours (doc 64, Phase 3).
        else if (std::string(argv[i]) == "--step" && i + 1 < argc)
        {
            step = std::atof(argv[++i]);
            if (!(step > 0 && step <= 60))
            {
                std::cerr << "--step is world seconds a tick, over 0 and up to 60 (World::tick's most), not " << argv[i] << "\n";
                return 2;
            }
        }
        else if (std::string(argv[i]) == "--threads" && i + 1 < argc)
            threads = unsigned(std::max(1, std::atoi(argv[++i])));
        else if (std::string(argv[i]) == "--players" && i + 1 < argc)
            for (std::stringstream list(argv[++i]); list.good();)
            {
                std::string name;
                std::getline(list, name, ',');
                const auto& known = econ::Players::names();
                if (std::find(known.begin(), known.end(), name) == known.end())
                {
                    std::cerr << "no such players' scenario: " << name << " (";
                    for (const auto& n : known)
                        std::cerr << (n == known.front() ? "" : ", ") << n;
                    std::cerr << ")\n";
                    return 2;
                }
                if (std::find(scenarios.begin(), scenarios.end(), name) == scenarios.end())
                    scenarios.push_back(name);
            }
        else if (std::string(argv[i]) == "--players-from" && i + 1 < argc)
            playersFrom = std::max<std::int64_t>(0, std::atoll(argv[++i]));
        else if (std::string(argv[i]) == "--players-days" && i + 1 < argc)
            playersDays = std::max<std::int64_t>(0, std::atoll(argv[++i]));
        else if (std::string(argv[i]) == "--players-bodies" && i + 1 < argc)
        {
            if (!econ::Players::bodiesNamed(argv[++i], bodies))
            {
                std::cerr << "--players-bodies is walk, still or jump, not " << argv[i] << "\n";
                return 2;
            }
        }
        else if (std::string(argv[i]) == "--players-staple" && i + 1 < argc)
        {
            staple = argv[++i];
            if (!econ::Players::stapleKnown(staple))
            {
                std::cerr << "--players-staple is a good of the catalogue its sellers price (not meal, herbs or sword), not " << staple << "\n";
                return 2;
            }
        }
        else if (std::string(argv[i]) == "--ask-payers")
            askPayers = true;
        else if (std::string(argv[i]) == "--stop-on-sentinel" && i + 1 < argc)
            for (std::stringstream list(argv[++i]); list.good();)
            {
                std::string name;
                std::getline(list, name, ',');
                if (name == "all")
                    stopOn.insert({"drain", "share", "hoard", "hunger", "gini"});
                else if (name == "drain" || name == "share" || name == "hoard" || name == "hunger" || name == "gini")
                    stopOn.insert(name);
                else
                {
                    std::cerr << "--stop-on-sentinel is drain, share, hoard, hunger, gini or all, not " << name << "\n";
                    return 2;
                }
            }
        else if (std::string(argv[i]) == "--model-shares" && i + 1 < argc)
            modelShares = argv[++i];
        else if (std::string(argv[i]) == "--sentinel-weeks" && i + 1 < argc)
            sentinelWeeks = std::max(1, std::atoi(argv[++i]));
        else if (std::string(argv[i]) == "--without" && i + 1 < argc)
        {
            unsigned off = 0;
            for (std::stringstream list(argv[++i]); list.good();)
            {
                std::string name;
                std::getline(list, name, ',');
                if (name == "children")
                    off |= Society::ChildrenOpen;
                else if (name == "employees")
                    off |= Society::ByEmployees;
                else if (name == "stalls")
                    off |= Society::StallsShed;
                else if (name == "sellers")
                    off |= Society::SellersOnly;
                else if (name == "apprentices")
                    off |= Society::KeepApprentices;
                else if (name == "unpaid")
                    off |= Society::UnpaidFirst;
                else if (name == "caravans")
                    off |= Society::CaravanIncome;
                else if (name == "weekly")
                    off |= Society::CaravansAsTheyCome;
                else if (name == "giveway")
                    off |= Society::FarGiveWay;
                else if (name == "masters")
                    off |= Society::KeepersApprentice;
                else if (name == "posts")
                    off |= Society::PostsFirst;
                else if (name == "crews")
                    off |= Society::CrewsAbroad;
                else if (name == "oneplace")
                    off |= Society::OnePlace;
                else if (name == "owners")
                    off |= Society::OwnersStop;
                else if (name == "houses")
                    off |= Society::HouseBudget;
                else if (name == "housetills")
                    off |= Society::HouseTills;
                else if (name == "church")
                    off |= Society::ChurchBudget;
                else if (name == "cheapalms")
                    off |= Society::CheapAlms;
                else if (name == "carry")
                    off |= Society::CarryParts;
                else
                {
                    std::cerr << "--without is children, employees, stalls, sellers, apprentices, unpaid, caravans, weekly, giveway, masters, posts, crews, oneplace, owners, houses, housetills, church, cheapalms or carry, not "
                              << name << "\n";
                    return 2;
                }
            }
            Society::wageRulesOff(off);
        }
        else if (std::string(argv[i]) == "--hold-pace" && i + 1 < argc)
        {
            heldPace = econ::readPace(slurp(argv[++i]));
            if (heldPace.empty())
            {
                std::cerr << "--hold-pace wants a run's pace.csv (day,when,society_day,pace), not " << argv[i] << "\n";
                return 2;
            }
        }
    // (The pace is held only where it is stepped, at work by the hours: without --hours a held run would quietly be a
    // free one. Round 6, the tidy's review.)
    if (!heldPace.empty() && !Society::workByHours())
    {
        std::cerr << "--hold-pace holds the pace of work by the hours: it wants --hours\n";
        return 2;
    }
    fs::create_directories(out);
    Society::noteShorts(true);                      // (shortpay.csv: read only, as every measure here.)
    const auto stamp = runStamp(argc, argv);
    std::cout << stamp << std::flush;
    std::ofstream(out / "run.txt") << stamp;

    std::map<std::string, std::string> files;
    for (const auto& entry : fs::recursive_directory_iterator(root))
        if (entry.is_regular_file())
            files[fs::relative(entry.path(), root).generic_string()] = slurp(entry.path());
    auto find = [&](const std::string& path) -> const std::string* {
        const auto it = files.find(path);
        return it == files.end() ? nullptr : &it->second;
    };
    World server;
    Pool pool(threads - 1);                         // (The main thread takes a share too.)
    if (threads > 1)
        server.setParallel([&](std::size_t count, const std::function<void(std::size_t)>& job) { pool.run(count, job); });
    server.setCellSource({
        [&](const std::string& id, std::string& header) {
            const auto* cell = find("cells/" + id + ".cell");
            if (!cell)
                return std::string("no cell file for ") + id;
            header = cell->substr(0, cell->find("grid:") + 5);
            return std::string();
        },
        [&](const std::string& id, std::string& cell, std::string& seams) {
            const auto* text = find("cells/" + id + ".cell");
            if (!text)
                return std::string("no cell file for ") + id;
            cell = *text;
            const auto* sides = find("seams/" + id);
            seams = sides ? *sides : std::string();
            return std::string();
        }});
    std::map<std::string, std::string> manifest;
    for (const auto& [path, text] : files)
        if (path.rfind("cells/", 0) != 0 && path.rfind("seams/", 0) != 0)
            manifest[path] = text;
    if (const auto loaded = server.loadWorldFiles(manifest, root.string()); !loaded.ok)
    {
        std::cerr << "World failed to load: " << loaded.message << '\n';
        return 1;
    }
    server.setTimeOfDay(std::fmod(from, 24));
    if (from >= 24)
    {
        // Later in the calendar (a winter's run): the world saved and restored at that moment, as the tests set a day.
        auto later = server.save();
        later.calendarDays = from / 24;
        if (const auto restored = server.restore(later); !restored.ok)
        {
            std::cerr << "Couldn't start at day " << from / 24 << ": " << restored.message << '\n';
            return 1;
        }
    }
    server.setDeterministic(deterministic);
    if (!heldPace.empty())
    {
        // Every society's day the run will turn (its days are the calendar's: Society::decide) in the file, or the run
        // would go back to stepping its own pace from the first one missing, without a word (round 6, the tidy's review).
        const auto first = std::int64_t(std::floor(server.calendarDays())), last = std::int64_t(std::floor(server.calendarDays() + days));
        for (auto d = server.society().state().budgetDay >= first ? first + 1 : first; d <= last; ++d)
            if (!heldPace.count(d))
            {
                std::cerr << "--hold-pace: the file has no pace for the society's day " << d << ", and the run goes to day " << last
                          << " (from " << first << "): a held run wants every day held\n";
                return 2;
            }
        server.society().holdPace(heldPace);
    }
    const auto& society = server.society();
    server.society().keepBriefs(true);
    std::unique_ptr<econ::Players> players;
    if (!scenarios.empty())
    {
        players = std::make_unique<econ::Players>(server, scenarios, playersFrom, playersDays, bodies);
        players->setStaple(staple);
    }

    std::ofstream daysOut(out / "days.jsonl"), flowsOut(out / "flows.csv"), wagesOut(out / "wages.csv"),
        materialsOut(out / "materials.csv"), eventsOut(out / "events.csv"), tradeOut(out / "trade.csv"),
        foodOut(out / "food.csv"), foodTownsOut(out / "food_towns.csv"), pricesOut(out / "prices.csv"),
        lardersOut(out / "larders.csv"), townFlowsOut(out / "town_flows.csv"),
        foodMovesOut(out / "food_moves.csv"), paceOut(out / "pace.csv"), orchestraOut(out / "orchestrator.jsonl"), orchestraTownsOut(out / "orchestrator_towns.csv"),
        gaveWayOut(out / "gaveway.csv");
    gaveWayOut << "day,hour,taker,taker_town,tiles,field,field_town,hand,hand_town\n";
    orchestraTownsOut << "day,town,people,distress,raw,kind,food_cost,hungry,starving,short,poor,idle,shop_food_days,takings_ratio,net_inflow,player_in,player_out,player_goods\n";
    flowsOut << "day,kind,from,to,entries,coins,goods\n";
    paceOut << "day,when,society_day,pace\n" << std::setprecision(17);
    wagesOut << "day,resident,title,coins\n";
    materialsOut << "day,item,maker_hours_short,supplier_hours_short,held,brought_in,crafted,used,bought,carted_in\n";
    eventsOut << "day,kind,actor,target,coins,detail\n";
    tradeOut << "day,from,to,kind,entries,coins,goods\n";
    foodOut << "day,item,brought_in,crafted,used,eaten,spoiled,bought\n";
    struct Food
    {
        std::int64_t in = 0, crafted = 0, used = 0, eaten = 0, spoiled = 0, bought = 0;
    };
    std::map<int, std::map<std::string, Food>> foods;
    foodTownsOut << "day,town,item,brought_in,crafted,used,eaten,spoiled,bought,paid\n";
    pricesOut << "day,item,kind,units,coins,catalog\n";
    lardersOut << "day,town,households,people,mean_days,under_1,d1_2,d2_5,over_5,shop_food,producer_food\n";
    townFlowsOut << "day,town,kind,from,to,entries,coins\n";
    foodMovesOut << "day,kind,from,to,item,units\n";
    std::map<int, std::map<std::tuple<std::string, std::string, std::string, std::string>, std::int64_t>> foodMoves;
    struct TownFood
    {
        std::int64_t in = 0, crafted = 0, used = 0, eaten = 0, spoiled = 0, bought = 0, paid = 0;
    };
    std::map<int, std::map<std::pair<std::string, std::string>, TownFood>> townFoods;
    std::map<int, std::map<std::pair<std::string, std::string>, std::pair<std::int64_t, std::int64_t>>> prices;
    std::map<int, std::map<std::tuple<std::string, std::string, std::string, std::string>, Flow>> townFlows;

    // Where each account is (refreshed each day's end): a resident's town, a till's, a town's own purses'.
    std::map<std::string, std::string> places;
    const auto refreshPlaces = [&] {
        places.clear();
        for (const auto& [id, life] : society.state().residents)
            places[id] = server.communityOf(life.homeCell);
        for (const auto& r : society.authored().residents)
            if (r.role == "merchant" && society.state().residents.count(r.id))
                places[society.tillOf(r.id)] = server.communityOf(r.work.cell);
    };
    const auto placeOf = [&](const std::string& id) -> std::string {
        if (const auto found = places.find(id); found != places.end())
            return found->second.empty() ? "(country)" : found->second;
        if (id == Society::SharedChurch)
            return "(church)";
        if (id == "treasury")
            return "(capital)";
        if (id.rfind("stores:", 0) == 0)
            return id.substr(7);
        if (id.rfind("town:", 0) == 0)
            return id.substr(5, id.find(':', 5) - 5);
        if (id.rfind("house:", 0) == 0)
            return "(houses)";
        if (id.rfind("home:", 0) == 0 && id.rfind(':') > 5)
        {
            const auto town = server.communityOf(id.substr(5, id.rfind(':') - 5));
            return town.empty() ? "(country)" : town;
        }
        if (id.rfind("caravan", 0) == 0 || id.rfind("contract", 0) == 0)
            return "(road)";
        return {};
    };
    refreshPlaces();
    std::map<int, std::map<std::tuple<std::string, std::string, std::string>, Flow>> trade;
    {
        // roster.csv: who works at what, where (the producer or business their work makes them).
        std::ofstream roster(out / "roster.csv");
        roster << "resident,town,work_town,title,producer,business\n";
        for (const auto& r : society.authored().residents)
        {
            if (!society.state().residents.count(r.id))
                continue;
            const auto* job = society.jobOf(r.id);
            const auto* p = items::producerFor(r.workLabel);
            const auto* b = r.role == "merchant" ? items::businessFor(r.workLabel) : nullptr;
            roster << csv(r.id) << ',' << csv(placeOf(r.id)) << ',' << csv(server.communityOf(r.work.cell)) << ','
                   << csv(job ? job->title : std::string()) << ',' << csv(p ? p->id : std::string()) << ','
                   << csv(b ? b->id : std::string()) << '\n';
        }
    }

    // The run's tracking (doc 64, Phase 13, round 10, lane 0): the week's accounts of interest, the towns' goods, and
    // the food's cheapest price. All read only, as every measure here.
    std::set<std::string> ownTills;                 // Keepers whose own purse is their shop's till.
    std::map<std::string, std::string> tillTowns;   // A post's till -> the town its work is in.
    const auto refreshTills = [&] {
        ownTills.clear();
        for (const auto& r : society.authored().residents)
            if (r.role == "merchant" && society.state().residents.count(r.id) && society.tillOf(r.id) == r.id)
                ownTills.insert(r.id);
        for (const auto& p : society.positions())
            tillTowns["till:" + p.id] = server.communityOf(p.work.cell);
    };
    refreshTills();
    // Where an account is, for the tracking: as placeOf, and a farm's or a House's till where its post's work is (placeOf
    // knows only the keepers' tills), a caravan's where it set out from.
    const auto townOf = [&](const std::string& id) -> std::string {
        auto town = placeOf(id);
        if (town.empty() && id.rfind("till:", 0) == 0)
            if (const auto found = tillTowns.find(id); found != tillTowns.end())
                town = found->second.empty() ? "(country)" : found->second;
        return town.empty() ? "(none)" : town;
    };
    const auto trackedAccount = [&](const std::string& id) {
        return id.rfind("till:", 0) == 0 || id.rfind("house:", 0) == 0 || id.rfind("stores:", 0) == 0 ||
               id.rfind("town:", 0) == 0 || id.rfind("caravan:", 0) == 0 || id.rfind("fund:", 0) == 0 || id == "treasury" ||
               ownTills.count(id) > 0;
    };
    std::unordered_map<std::string, AccountWeek> accountWeeks;
    std::map<std::string, std::int64_t> opening;    // Each tracked account's balance at the week's start.
    for (const auto& [id, a] : society.state().accounts)
        if (trackedAccount(id))
            opening[id] = a.cash;
    // (The founding's entries made before the first tick, not yet taken, come in the first week: its opening is what each
    // held before them.)
    const auto unpaid = [&](const std::string& from, const std::string& to, std::int64_t coins) {
        if (coins <= 0)
            return;
        if (const auto found = opening.find(from); found != opening.end())
            found->second += coins;
        if (const auto found = opening.find(to); found != opening.end())
            found->second -= coins;
    };
    for (const auto& e : server.recentEvents())
        if (e.kind == "economy")
            unpaid(e.actor, e.target, e.coins);
    for (const auto& e : society.pendingJournal())
        unpaid(e.from, e.to, e.coins);
    std::map<std::string, std::string> caravanFrom;  // A caravan's account -> the town it set out from (kept when it's gone).
    const auto weekOf = [&](const std::string& id) -> AccountWeek& {
        const auto [it, made] = accountWeeks.try_emplace(id);
        if (made && id.rfind("caravan:", 0) == 0 && !caravanFrom.count(id))
            for (const auto& c : server.roads().caravans)
                if (c.account == id)
                    caravanFrom[id] = c.from;
        return it->second;
    };
    std::map<std::pair<std::string, std::string>, GoodsWeek> goodsWeek;   // (Town, good) -> its week.
    std::map<std::string, std::map<int, double>> foodPrices;   // Town -> day -> the least a resident paid a unit of food.

    // The day's tallies.
    // By the day each entry was made (the world stamps it), so the midnight's decisions (a month's reckoning) fall on
    // the new day.
    std::map<int, std::map<std::tuple<std::string, std::string, std::string>, Flow>> flows;
    std::map<int, std::map<std::string, std::int64_t>> wages;      // Resident -> wages that day.
    std::map<int, std::map<std::string, Material>> materials;
    int today = 0;
    std::int64_t hoursSampled = 0;

    // The makers and suppliers, and what each makes from and sells (fixed for the run).
    struct Maker
    {
        std::string id;
        std::vector<const items::Craft*> crafts;
        std::vector<std::string> supplies;
    };
    std::vector<Maker> makers;
    for (const auto& r : society.authored().residents)
        if (r.role == "merchant")
            if (const auto* business = items::businessFor(r.workLabel))
            {
                Maker m{r.id, items::craftsFor(business->id), items::suppliesFor(business->id)};
                if (!m.crafts.empty() || !m.supplies.empty())
                    makers.push_back(std::move(m));
            }
    std::set<std::string> ingredients;
    for (const auto& m : makers)
    {
        for (const auto* k : m.crafts)
            for (const auto& [item, n] : k->in)
                ingredients.insert(item);
        for (const auto& s : m.supplies)
            ingredients.insert(s);
    }

    // An hour's look at the workshops: who wants to make something and lacks what it takes, and which suppliers' shelves
    // are bare, as RatwCrafting.cpp judges them.
    const auto sampleMaterials = [&] {
        ++hoursSampled;
        for (const auto& m : makers)
        {
            const auto* till = society.account(society.tillOf(m.id));
            const auto* e = server.entity(m.id);
            if (!till || !e || e->dead)
                continue;
            for (const auto* k : m.crafts)
            {
                const auto& made = k->out.front().first;
                const bool supplied = std::find(m.supplies.begin(), m.supplies.end(), made) != m.supplies.end();
                const bool low = Society::stockAll(*till, made) < (supplied || items::traded(made) ? Society::SuppliesKept / 2 : Society::GoodsKept);
                if (!low)
                    continue;
                for (const auto& [item, n] : k->in)
                    if (Society::stockAll(*till, item) < n)
                        ++materials[today][item].makerShort;
            }
            for (const auto& s : m.supplies)
                if (Society::stockAll(*till, s) < Society::SuppliesKept / 4)
                    ++materials[today][s].supplierShort;
        }
    };

    std::map<std::string, std::pair<std::int64_t, std::int64_t>> tillSold;   // Till -> units sold and coins taken since the last measure.
    std::map<std::string, std::int64_t> oddPaid;    // Resident -> what its odd jobs paid it since the last measure (round 9).
    const auto absorb = [&] {
        for (auto& e : server.takeEvents())
        {
            const int d = int(std::floor(e.day));
            if (e.kind == "economy")
            {
                // The tracking's (round 10, lane 0): the accounts' week, coins from the payer (actor) to the payee.
                if (e.coins > 0)
                {
                    if (trackedAccount(e.actor))
                    {
                        auto& w = weekOf(e.actor);
                        auto& f = w.out[e.detail];
                        ++f.entries, f.coins += e.coins;
                        w.to[e.target] += e.coins;
                    }
                    if (trackedAccount(e.target))
                    {
                        auto& w = weekOf(e.target);
                        auto& f = w.in[e.detail];
                        ++f.entries, f.coins += e.coins;
                        w.from[e.actor] += e.coins;
                    }
                }
                // And the towns' goods: a sale's go from its seller (the target) to its buyer, the rest from the actor to
                // the target; made from nothing, or used up ("consumed").
                if (e.quantity > 0 && !e.item.empty() && itemValid(e.item))
                {
                    const bool sale = e.coins > 0 && !bothForward(e.detail);
                    const auto& src = sale ? e.target : e.actor;
                    const auto& dst = sale ? e.actor : e.target;
                    const auto base = items::baseOf(e.item);
                    const auto& k = e.detail;
                    if (src == dst)
                    {
                        // (A meal cooked in one's own pocket.)
                    }
                    else if (dst == "consumed")
                    {
                        auto& g = goodsWeek[{townOf(src), base}];
                        (k == "used in crafting" ? g.used
                         : k == "spoiled"         ? g.spoiled
                         : k.rfind("eat", 0) == 0 || k == "drunk" ? g.eaten
                                                                  : g.worn) += e.quantity;
                    }
                    else if (src == "made" || src == "outside" || src == "herb patch")
                    {
                        auto& g = goodsWeek[{townOf(dst), base}];
                        (k == "crafted" ? g.made : g.broughtIn) += e.quantity;
                    }
                    else
                    {
                        const auto from = townOf(src), to = townOf(dst);
                        if (from != to)
                            goodsWeek[{from, base}].carriedOut += e.quantity, goodsWeek[{to, base}].carriedIn += e.quantity;
                        if (sale)
                        {
                            auto& g = goodsWeek[{from, base}];
                            g.sold += e.quantity, g.soldCoins += e.coins;
                            if (k == "resident food purchase")
                            {
                                const double each = double(e.coins) / e.quantity;
                                auto& least = foodPrices[from].try_emplace(d, each).first->second;
                                least = std::min(least, each);
                            }
                        }
                    }
                }
                if (e.coins > 0 && e.quantity > 0 && e.target.rfind("till:", 0) == 0)
                    tillSold[e.target].first += e.quantity, tillSold[e.target].second += e.coins;
                if (e.coins > 0 && e.detail.rfind("an odd job: ", 0) == 0)
                    oddPaid[e.target] += e.coins;
                if (e.detail == "gave way to a nearer hand")    // (Its quantity the tiles, its item the field's till.)
                    gaveWayOut << d << ',' << std::fixed << std::setprecision(2) << (e.day - d) * 24 << std::defaultfloat << ','
                               << csv(e.actor) << ',' << csv(society.townOfResident(e.actor)) << ',' << e.quantity << ',' << csv(e.item)
                               << ',' << csv(society.employerTownOf(e.item)) << ',' << csv(e.target) << ','
                               << csv(society.townOfResident(e.target)) << '\n';
                auto& f = flows[d][{e.detail, holder(server, e.actor), holder(server, e.target)}];
                ++f.entries;
                f.coins += e.coins;
                f.goods += e.quantity;
                if (e.coins > 0)
                    if (const auto a = placeOf(e.actor), b = placeOf(e.target); !a.empty() && !b.empty() && a != b)
                    {
                        auto& t = trade[d][{a, b, e.detail}];
                        ++t.entries;
                        t.coins += e.coins;
                        t.goods += e.quantity;
                    }
                if (e.detail.find("wage") != std::string::npos && e.coins > 0 && society.spec(e.target))
                    wages[d][e.target] += e.coins;
                if (!e.item.empty() && e.coins > 0 && e.quantity > 0)
                {
                    auto& pr = prices[d][{items::baseOf(e.item), e.detail}];
                    pr.first += e.quantity;
                    pr.second += e.coins;
                }
                if (e.coins > 0)
                    if (const auto a = placeOf(e.actor); !a.empty() && a == placeOf(e.target))
                    {
                        auto& t = townFlows[d][{a, e.detail, holder(server, e.actor), holder(server, e.target)}];
                        ++t.entries;
                        t.coins += e.coins;
                    }
                if (!e.item.empty() && Society::edible(e.item))
                    foodMoves[d][{e.detail, holder(server, e.actor), holder(server, e.target), items::baseOf(e.item)}] += e.quantity;
                if (!e.item.empty() && Society::edible(e.item))
                {
                    // Where: the holder made or used up, else the seller (the target of a sale).
                    const bool made = e.actor == "made" || e.actor == "outside";
                    auto town = placeOf(made ? e.target : e.target == "consumed" ? e.actor : e.target);
                    if (town.empty())
                        town = placeOf(e.actor);
                    auto& tf = townFoods[d][{town.empty() ? "(none)" : town, items::baseOf(e.item)}];
                    const auto& k = e.detail;
                    if (k == "brought in")
                        tf.in += e.quantity;
                    else if (k == "crafted")
                        tf.crafted += e.quantity;
                    else if (k == "used in crafting")
                        tf.used += e.quantity;
                    else if (k == "eat")
                        tf.eaten += e.quantity;
                    else if (k == "spoiled")
                        tf.spoiled += e.quantity;
                    else if (k == "resident food purchase")
                        tf.bought += e.quantity, tf.paid += e.coins;
                    auto& f = foods[d][items::baseOf(e.item)];
                    if (k == "brought in")
                        f.in += e.quantity;
                    else if (k == "crafted")
                        f.crafted += e.quantity;
                    else if (k == "used in crafting")
                        f.used += e.quantity;
                    else if (k == "eat" || k == "eaten")
                        f.eaten += e.quantity;
                    else if (k == "spoiled")
                        f.spoiled += e.quantity;
                    else if (k == "resident food purchase")
                        f.bought += e.quantity;
                }
                if (!e.item.empty())
                {
                    const auto base = items::baseOf(e.item);
                    auto& m = materials[d][base];
                    if (e.detail == "brought in")
                        m.broughtIn += e.quantity;
                    else if (e.detail == "crafted")
                        m.crafted += e.quantity;
                    else if (e.detail == "used in crafting")
                        m.used += e.quantity;
                    else if (e.detail == "materials bought")
                        m.bought += e.quantity;
                    else if (e.detail == "materials carted in")
                        m.carted += e.quantity;
                }
            }
            else if (e.kind != "movement" && e.kind != "conversation" && e.kind != "speech")
                eventsOut << d << ',' << csv(e.kind) << ',' << csv(e.actor) << ',' << csv(e.target) << ',' << e.coins << ','
                          << csv(e.detail) << '\n';
        }
    };

    std::map<std::string, std::int64_t> startCash;   // Each account's cash at the start, for the end's comparison.
    for (const auto& [id, a] : society.state().accounts)
        startCash[id] = a.cash;

    // Each town's larders: how many days of food its households hold (larder and pockets, at 50 a wolf a day), and the
    // food on its shops' shelves and with its producers (in nourishment).
    const auto writeLarders = [&](int day) {
        // (A producer's goods for sale aren't its household's food.)
        const auto nourishIn = [&](const EconomyAccount& a, const std::string& holder = std::string()) {
            std::int64_t n = 0;
            for (const auto& [item, q] : a.stock)
                if (const auto* g = q > 0 ? items::good(item) : nullptr; g && !g->drink && g->nourish > 0 &&
                                                                          (holder.empty() || !society.forSale(holder, item)))
                    n += std::int64_t(q) * g->nourish;
            return n;
        };
        struct Larders
        {
            int households = 0, people = 0, under1 = 0, d12 = 0, d25 = 0, over5 = 0;
            double days = 0;
            std::int64_t shop = 0, producer = 0;
        };
        std::map<std::string, Larders> towns;
        std::map<std::string, std::vector<std::string>> homes;
        for (const auto& [id, life] : society.state().residents)
            if (const auto* e = server.entity(id); e && !e->dead && !life.homeCell.empty())
                homes[life.homeCell].push_back(id);
        for (const auto& [home, members] : homes)
        {
            std::int64_t food = 0;
            if (const auto* larder = society.account(Society::homeStore(home, "larder")))
                food += nourishIn(*larder);
            for (const auto& id : members)
            {
                const auto* r = society.spec(id);
                if (const auto* a = society.account(id); a && !(r && r->role == "merchant" && society.tillOf(id) == id))
                    food += nourishIn(*a, id);
            }
            const auto town = server.communityOf(home);
            auto& t = towns[town.empty() ? "(country)" : town];
            const double days = double(food) / (50. * double(members.size()));
            ++t.households;
            t.people += int(members.size());
            t.days += days;
            (days < 1 ? t.under1 : days < 2 ? t.d12 : days <= 5 ? t.d25 : t.over5) += 1;
        }
        for (const auto& r : society.authored().residents)
        {
            if (!society.state().residents.count(r.id))
                continue;
            const auto* e = server.entity(r.id);
            if (!e || e->dead)
                continue;
            if (r.role == "merchant")
            {
                if (const auto* till = society.account(society.tillOf(r.id)))
                    towns[placeOf(society.tillOf(r.id)).empty() ? "(country)" : placeOf(society.tillOf(r.id))].shop += nourishIn(*till);
            }
            else if (items::producerFor(r.workLabel))
                if (const auto* a = society.account(r.id))
                    towns[placeOf(r.id).empty() ? "(country)" : placeOf(r.id)].producer += nourishIn(*a);
        }
        for (const auto& [town, t] : towns)
            lardersOut << day << ',' << csv(town) << ',' << t.households << ',' << t.people << ','
                       << (t.households ? t.days / t.households : 0) << ',' << t.under1 << ',' << t.d12 << ',' << t.d25 << ','
                       << t.over5 << ',' << t.shop << ',' << t.producer << '\n';
        lardersOut.flush();
    };

    // The measures of doc 63's model (tools/econ_model/model.py, measure()), so a run can be set beside a model run
    // (Docs/Design/64-economy-migration.md, Phase 0; tools/econ_model/compare.py): households' money (by home) and its
    // Gini and poorer half's share, the going labourer's day wage and prices in labourer's day-wages (LD), idle wolves,
    // masters, apprentices, elders, and the births and deaths since the last day.
    std::set<std::string> lastAlive;
    bool firstMeasure = true;
    double householdGini = 0;                       // The households' Gini at the latest measure.
    std::ofstream residentsOut(out / "residents.csv");
    residentsOut << "day,resident,home,holder,title,age,cash\n";
    std::ofstream tillsOut(out / "tills.csv");
    tillsOut << "day,till,holder,title,cash,working,owed,rate_units,materials_day,orders_week,sold_units,takings,hands,wanted,"
                "hire_slots,hire_taken,resale_day,short_weeks,stopped,keeper_odd_pay\n";
    std::ofstream workOut(out / "work.csv");
    workOut << "day,account,title,credited,lost,till,banked\n";
    std::unordered_map<std::string, std::pair<double, double>> workBefore;
    std::ofstream booksOut(out / "books.csv");
    booksOut << "day,key,town,wage,rate,open_days,calm,short_pay,vacancies,hands,skill\n";
    // `rows`: the day's rows in residents.csv, tills.csv, work.csv and books.csv too (not at the run's "end", which would
    // repeat its last day's under the same day).
    const auto modelMeasures = [&](int day, bool rows) {
        std::map<std::string, std::int64_t> homes;     // Home cell (or the resident, homeless) -> the household's money.
        // A barracks, a bunkhouse, quarters or lodgings (over eight grown, or communal) share no purse
        // (Society::tendHouseholds): each who lives there is a household of its own.
        std::map<std::string, int> grownAt;
        for (const auto& [id, life] : society.state().residents)
            if (const auto* e = server.entity(id); e && !e->dead && e->age >= 16 && !life.homeCell.empty())
                ++grownAt[life.homeCell];
        std::set<std::string> alive;
        int idle = 0, masters = 0, apprentices = 0, elders = 0;
        for (const auto& [id, life] : society.state().residents)
        {
            const auto* e = server.entity(id);
            if (!e || e->dead)
                continue;
            alive.insert(id);
            const auto* a = society.account(id);
            const bool shared = !life.homeCell.empty() && (grownAt[life.homeCell] > 8 || Society::communalHome(life.homeCell));
            homes[life.homeCell.empty() || shared ? id : life.homeCell] += (a ? a->cash : 0);
            if (rows)
            {
                const auto* job = society.jobOf(id);
                residentsOut << day << ',' << csv(id) << ',' << csv(life.homeCell) << ',' << csv(holder(server, id)) << ','
                             << csv(job ? job->title : "") << ',' << e->age << ',' << (a ? a->cash : 0) << '\n';
            }
            const bool apprentice = society.apprenticedTo(id) != nullptr;
            apprentices += apprentice;
            elders += e->age >= Society::retireAge(id);
            idle += e->age >= 16 && e->age < Society::retireAge(id) && !society.jobOf(id) && !apprentice;
        }
        // Each business's till against its working money (doc 64, Phase 5): what it keeps back, and why; and its hands
        // against the hands it wants (doc 64, Phase 13), and its hires running: their slots, and those taken. Once a
        // till: a producer's work is credited to the farmer and its hands are its till's, so work.csv may have two rows
        // for one till.
        std::map<std::string, std::pair<int, int>> hiring;   // Payer -> slots, taken.
        for (const auto& j : society.oddJobs())
            if (j.until >= day - 1)
            {
                auto& h = hiring[j.payer];
                h.first += j.slots;
                h.second += int(j.stage.size());
            }
        for (const auto& p : society.positions())
            if (const auto* till = rows ? society.account("till:" + p.id) : nullptr)
            {
                const auto id = "till:" + p.id;
                const auto held = society.state().careers.positions.find(p.id);
                const auto keeper = held == society.state().careers.positions.end() ? std::string() : held->second.holder;
                // (`resale_day`: the part of `materials_day` bought to sell on, kept for ResaleDays by the hours: doc 64,
                // Phase 13, round 8.)
                double units = 0, materials = 0, resale = 0;
                if (const auto* rates = society.sellRates(id))
                    for (const auto& [good, rate] : *rates)
                        if (items::good(good) && !Society::storePriced(good))
                        {
                            units += rate;
                            bool resold = false;
                            const double cost = keeper.empty() ? 0 : rate * society.materialsCost(keeper, good, &resold);
                            materials += cost;
                            resale += resold ? cost : 0;
                        }
                const auto sold = tillSold[id];
                const auto hires = hiring.find(id);
                // (Round 9: its owner's looks in a row that found it short of a week's wages, the day its owner stopped
                // topping it up, -1 if it hasn't, and what its keeper's odd jobs paid it since the last measure.)
                const auto shortTill = society.state().memory.shortTills.find(id);
                const auto paidOdd = keeper.empty() ? oddPaid.end() : oddPaid.find(keeper);
                tillsOut << day << ',' << csv(id) << ',' << csv(holder(server, id)) << ',' << csv(p.title) << ',' << till->cash << ','
                         << society.workingMoney(p.id) << ',' << society.owed(id) << ',' << units << ',' << materials << ','
                         << society.ordersCostOf(id) << ',' << sold.first << ',' << sold.second << ',' << society.handsOf(id) << ','
                         << society.handsWanted(id) << ',' << (hires == hiring.end() ? 0 : hires->second.first) << ','
                         << (hires == hiring.end() ? 0 : hires->second.second) << ',' << resale << ','
                         << (shortTill == society.state().memory.shortTills.end() ? 0 : shortTill->second.weeks) << ','
                         << (shortTill == society.state().memory.shortTills.end() ? -1 : shortTill->second.stopped) << ','
                         << (paidOdd == oddPaid.end() ? 0 : paidOdd->second) << '\n';
            }
        tillSold.clear();
        if (rows)
            oddPaid.clear();
        // Each business's and producer's hours of work today, and those lost to a full bank (doc 64, Phase 13), with the
        // till it is for (a producer's work is credited to the farmer, its hands to its till: tills.csv), and its hours
        // in hand at the day's turn.
        std::vector<std::string> worked;
        if (rows)
            for (const auto& [account, by] : society.workBy())
                worked.push_back(account);
        std::sort(worked.begin(), worked.end());
        for (const auto& account : worked)
        {
            const auto& by = society.workBy().at(account);
            auto& before = workBefore[account];
            const auto till = account.rfind("till:", 0) == 0 ? account : society.tillOf(account);
            const auto* p = till.rfind("till:", 0) == 0 ? society.position(till.substr(5)) : nullptr;
            workOut << day << ',' << csv(account) << ',' << csv(p ? p->title : "") << ',' << by.first - before.first << ','
                    << by.second - before.second << ',' << csv(till) << ',' << society.workBankedOf(account) << '\n';
            before = by;
        }
        // Every employer's book (doc 64, Phase 6): its wage against the going rate for its skill, the week's open days,
        // its calm weeks, whether it fell short paying; its posts open and filled.
        if (rows)
            for (const auto& [key, book] : society.state().memory.employers)
            {
                const auto town = society.employerTownOf(key);
                const double skill = society.employerSkill(key);
                booksOut << day << ',' << csv(key) << ',' << csv(town) << ',' << book.wage << ','
                         << society.goingRate(town) * (1 + skill / items::ladder().skillWage) << ',' << book.openDays << ','
                         << book.calm << ',' << (book.shortPay ? 1 : 0) << ',' << society.vacanciesOf(key) << ','
                         << society.handsOf(key) << ',' << skill << '\n';
            }
        booksOut.flush();
        tillsOut.flush();
        workOut.flush();
        residentsOut.flush();
        // Posts standing empty (not let go), and of them producers' and shops' (doc 64, Phase 8).
        int vacant = 0, vacantTrade = 0;
        for (const auto& p : society.positions())
            if (const auto ps = society.state().careers.positions.find(p.id);
                ps != society.state().careers.positions.end() && ps->second.holder.empty() && !society.state().memory.closed.count(p.id))
                ++vacant, vacantTrade += p.role == "merchant" || items::producerFor(p.title);
        // Masters (doc 64, Phase 8): wolves at their trade's level in a masters' trade (Society::TradeMasters).
        for (const auto& id : alive)
            if (const auto* job = society.jobOf(id); job && job->level >= Society::TradeMasters && society.skill(id, job->id) >= job->level)
                ++masters;
        int births = 0, deaths = 0;
        if (!firstMeasure)
        {
            for (const auto& id : alive)
                births += !lastAlive.count(id);
            for (const auto& id : lastAlive)
                deaths += !alive.count(id);
        }
        lastAlive = alive;
        firstMeasure = false;
        std::vector<std::int64_t> money;
        std::int64_t total = 0;
        for (const auto& [home, m] : homes)
        {
            money.push_back(std::max<std::int64_t>(0, m));
            total += std::max<std::int64_t>(0, m);
        }
        std::sort(money.begin(), money.end());
        const auto h = money.size();
        double gini = 0, poorer = 0;
        if (h && total > 0)
        {
            double weighted = 0, low = 0;
            for (std::size_t i = 0; i < h; ++i)
            {
                weighted += double(i + 1) * double(money[i]);
                if (i < h / 2)
                    low += double(money[i]);
            }
            gini = 2 * weighted / (double(h) * double(total)) - double(h + 1) / double(h);
            poorer = low / double(total);
        }
        householdGini = gini;                       // (The gini sentinel's: round 10, lane 0.)
        const auto town = society.capital().empty() ? server.communityOf(society.state().residents.begin()->second.homeCell)
                                                    : society.capital();
        const double wage = society.dayWage(town, "labour");
        std::ostringstream out;
        out << "{\"people\":" << alive.size() << ",\"households\":" << h << ",\"household_gini\":" << gini
            << ",\"poorer_half\":" << poorer << ",\"median_household\":" << (h ? money[h / 2] : 0) << ",\"idle\":" << idle
            << ",\"masters\":" << masters << ",\"apprentices\":" << apprentices << ",\"elders\":" << elders
            << ",\"births\":" << births << ",\"deaths\":" << deaths << ",\"vacant\":" << vacant << ",\"vacant_trade\":" << vacantTrade << ",\"wage\":" << wage << ",\"town\":" << json(town)
            << ",\"going\":{";
        // Every town's going rate, and the realm's ("") (doc 64, Phase 6).
        {
            const std::map<std::string, double> going(society.goingRates().begin(), society.goingRates().end());
            bool firstTown = true;
            for (const auto& [where, rate] : going)
            {
                out << (firstTown ? "" : ",") << json(where) << ':' << rate;
                firstTown = false;
            }
        }
        out << "},\"ld\":{";
        bool firstGood = true;
        for (const char* good : {"wheat", "flour", "bread", "firewood", "timber", "iron_ore", "iron_bar", "sword", "iron_sword",
                                 "steel_sword", "rapier", "wool_scarf", "mining_pick"})
        {
            if (!items::good(good))
                continue;
            // What the sellers holding it ask (doc 64, Phase 4: each its own), on average; with none, the going price.
            double asked = 0;
            int sellers = 0;
            for (const auto& [id, a] : society.state().accounts)
                if (Society::costed(id) && id.rfind("fund:", 0) != 0 && Society::stock(a, good) > 0)
                    asked += society.priceAt(id, good), ++sellers;
            const double price = sellers ? asked / sellers : society.townPrice(town, good);
            out << (firstGood ? "" : ",") << json(good) << ':' << (wage > 0 ? price / wage : 0.0);
            firstGood = false;
        }
        out << "}";
        // Sellers' margins (doc 64, Phase 4): their mean, and the shares under 5% and over 30%.
        double sum = 0;
        int count = 0, low = 0, high = 0;
        for (const auto& [seller, goods] : society.margins())
            for (const auto& [good, m] : goods)
                sum += m, ++count, low += m < .05, high += m > .3;
        // Employers' wages (doc 64, Phase 6): how many have books, their mean against the going rate, the share over 1.3x.
        {
            double sumWage = 0;
            int books = 0, over = 0;
            for (const auto& [key, book] : society.state().memory.employers)
            {
                const double rate = society.employerWage(key) / std::max(1e-9, wage);
                sumWage += rate, ++books, over += rate > 1.3;
            }
            out << ",\"employers\":{\"count\":" << books << ",\"mean\":" << (books ? sumWage / books : 0.) << ",\"over\":"
                << (books ? double(over) / books : 0.) << "}";
        }
        out << ",\"work\":{\"credited\":" << society.workCredited() << ",\"lost\":" << society.workLost()
            << ",\"pace\":" << society.state().memory.pace << "}";
        out << ",\"margins\":{\"count\":" << count << ",\"mean\":" << (count ? sum / count : 0.) << ",\"low\":"
            << (count ? double(low) / count : 0.) << ",\"high\":" << (count ? double(high) / count : 0.) << "}}";
        return out.str();
    };

    const auto snapshot = [&](int day, const char* when) {
        if (std::string(when) == "day end")
            writeLarders(day);
        // Money by kind of holder, and every collector's and till's purse.
        std::map<std::string, std::pair<std::int64_t, std::int64_t>> byHolder;   // total, how many
        std::map<std::string, std::int64_t> purses;
        std::vector<std::pair<std::int64_t, std::string>> all;
        std::vector<std::int64_t> people;
        int short_ = 0, broke = 0, starving = 0, hungry = 0, grownShort = 0;
        std::int64_t residentTotal = 0;
        for (const auto& [id, a] : society.state().accounts)
        {
            const auto kind = holder(server, id);
            auto& h = byHolder[kind];
            h.first += a.cash;
            ++h.second;
            all.push_back({a.cash, id});
            if (id == "treasury" || id.rfind("stores:", 0) == 0 || id.rfind("town:", 0) == 0 || id.rfind("house:", 0) == 0 ||
                id.rfind("till:", 0) == 0 || id.rfind("fund:", 0) == 0)
                purses[id] = a.cash;
        }
        for (const auto& [id, life] : society.state().residents)
        {
            const auto* e = server.entity(id);
            if (!e || e->dead)
                continue;
            const auto* a = society.account(id);
            const std::int64_t cash = a ? a->cash : 0;
            const std::int64_t wealth = cash;
            people.push_back(wealth);
            residentTotal += wealth;
            short_ += cash < 6;
            grownShort += cash < 6 && e->age >= 16;     // (Children spend their stipends freely; the larder feeds them.)
            broke += cash <= 0;
            hungry += life.hunger >= 70;
            starving += life.hunger >= 90;
        }
        // Merchants' own purses are their shops' tills unless a house owns the business; count those too.
        for (const auto& m : makers)
            if (society.tillOf(m.id) == m.id)
                if (const auto* a = society.account(m.id))
                    purses["shop:" + m.id] = a->cash;
        std::sort(people.begin(), people.end());
        std::sort(all.rbegin(), all.rend());
        const auto n = people.size();
        double gini = 0;
        if (n && residentTotal > 0)
        {
            double weighted = 0;
            for (std::size_t i = 0; i < n; ++i)
                weighted += double(i + 1) * double(people[i]);
            gini = 2 * weighted / (double(n) * double(residentTotal)) - double(n + 1) / double(n);
        }
        const auto sumRange = [&](std::size_t a, std::size_t b) {
            std::int64_t s = 0;
            for (std::size_t i = a; i < b && i < n; ++i)
                s += people[i];
            return s;
        };
        const auto supply = society.moneySupply();
        std::int64_t contractsOpen = 0, contractsTaken = 0, contractsDone = 0, contractsExpired = 0;
        for (const auto& k : server.roads().contracts)
            (k.status == "open" ? contractsOpen : k.status == "taken" ? contractsTaken : k.status == "done" ? contractsDone : contractsExpired) += 1;
        std::size_t caravans = 0;
        for (const auto& c : server.roads().caravans)
            caravans += c.status == "travelling" || c.status == "returning";

        daysOut << "{\"day\":" << day << ",\"when\":" << json(when) << ",\"calendar\":" << server.calendarDays()
                << ",\"supply\":" << supply << ",\"conserved\":" << (society.conserved() ? "true" : "false")
                << ",\"minted\":" << society.state().minted << ",\"sunk\":" << society.state().sunk << ",\"residents\":" << n
                << ",\"resident_total\":" << residentTotal << ",\"median\":" << (n ? people[n / 2] : 0)
                << ",\"poorest_tenth\":" << (n ? sumRange(0, n / 10) / std::int64_t(std::max<std::size_t>(1, n / 10)) : 0)
                << ",\"richest_tenth\":" << (n ? sumRange(n - n / 10, n) / std::int64_t(std::max<std::size_t>(1, n / 10)) : 0)
                << ",\"richest_resident\":" << (n ? people.back() : 0) << ",\"gini\":" << gini
                << ",\"top_tenth_share\":" << (residentTotal ? double(sumRange(n - n / 10, n)) / double(residentTotal) : 0)
                << ",\"bottom_half_share\":" << (residentTotal ? double(sumRange(0, n / 2)) / double(residentTotal) : 0)
                << ",\"short\":" << short_ << ",\"grown_short\":" << grownShort << ",\"broke\":" << broke << ",\"hungry\":" << hungry << ",\"starving\":" << starving
                << ",\"contracts\":{\"open\":" << contractsOpen << ",\"taken\":" << contractsTaken << ",\"done\":" << contractsDone
                << ",\"expired\":" << contractsExpired << "},\"caravans\":" << caravans << ",\"holders\":{";
        bool first = true;
        for (const auto& [kind, h] : byHolder)
        {
            daysOut << (first ? "" : ",") << json(kind) << ":[" << h.first << ',' << h.second << ']';
            first = false;
        }
        daysOut << "},\"top_accounts\":[";
        for (std::size_t i = 0; i < std::min<std::size_t>(15, all.size()); ++i)
            daysOut << (i ? "," : "") << "[" << json(all[i].second) << ',' << all[i].first << ']';
        daysOut << "],\"purses\":{";
        first = true;
        for (const auto& [id, cash] : purses)
        {
            daysOut << (first ? "" : ",") << json(id) << ':' << cash;
            first = false;
        }
        // Who is hungry, and why: work, purse, food carried, what it is doing.
        daysOut << "},\"hungry_residents\":[";
        first = true;
        for (const auto& [id, life] : society.state().residents)
        {
            const auto* e = server.entity(id);
            if (!e || e->dead || life.hunger < 70)
                continue;
            const auto* a = society.account(id);
            const auto* job = society.jobOf(id);
            daysOut << (first ? "" : ",") << "{\"id\":" << json(id) << ",\"title\":" << json(job ? job->title : "(none)")
                    << ",\"age\":" << e->age << ",\"hunger\":" << life.hunger << ",\"cash\":" << (a ? a->cash : 0)
                    << ",\"food\":" << json(a ? Society::bestFood(*a) : "") << ",\"task\":" << json(life.task)
                    << ",\"reason\":" << json(life.reason) << ",\"cell\":" << json(e->cellId) << ",\"goal\":" << json(life.goalCell)
                    << ",\"home\":" << json(life.homeCell) << ",\"offstage\":" << (e->offstage ? "true" : "false") << "}";
            first = false;
        }
        // Each town's residents by home (Docs/Design/42, "Where money pools"): how many, what they hold, the median, and
        // how many are short (under 6p), broke, hungry and starving.
        struct Town
        {
            std::vector<std::int64_t> purses;
            int short_ = 0, broke = 0, hungry = 0, starving = 0;
            int foodShops = 0, food = 0;            // Shops holding anything to eat, and how many meals' worth they hold.
            std::int64_t producers = 0, keepers = 0, town = 0;   // Held by its producers, its shops' tills, the town.
        };
        std::map<std::string, Town> towns;
        for (const auto& [id, life] : society.state().residents)
        {
            const auto* e = server.entity(id);
            if (!e || e->dead)
                continue;
            const auto* a = society.account(id);
            const std::int64_t cash = a ? a->cash : 0;
            auto& t = towns[server.communityOf(life.homeCell)];
            t.purses.push_back(cash);
            t.short_ += cash < 6;
            t.broke += cash <= 0;
            t.hungry += life.hunger >= 70;
            t.starving += life.hunger >= 90;
            if (const auto* r = society.spec(id); r && items::producerFor(r->workLabel))
                t.producers += cash;
        }
        refreshPlaces();
        for (const auto& [id, a] : society.state().accounts)
        {
            const bool till = id.rfind("till:", 0) == 0 || (society.spec(id) && society.tillOf(id) == id &&
                                                            society.jobOf(id) && society.jobOf(id)->role == "merchant");
            const bool own = id.rfind("stores:", 0) == 0 || (id.rfind("town:", 0) == 0 && id != Society::SharedChurch);
            if ((till || own) && places.count(id) + own > 0)
            {
                const auto where = own ? placeOf(id) : places.at(id);
                (till ? towns[where].keepers : towns[where].town) += a.cash;
            }
        }
        // And each town's shops' food (by where each keeper works).
        for (const auto& r : society.authored().residents)
            if (r.role == "merchant" && society.state().residents.count(r.id))
                if (const auto* till = society.account(society.tillOf(r.id)))
                {
                    int meals = 0;
                    for (const auto& [item, count] : till->stock)
                        if (count > 0 && Society::edible(item))
                            meals += count;
                    auto& t = towns[server.communityOf(r.work.cell)];
                    t.food += meals;
                    t.foodShops += meals > 0;
                }
        daysOut << "],\"towns\":{";
        first = true;
        for (auto& [community, t] : towns)
        {
            std::sort(t.purses.begin(), t.purses.end());
            std::int64_t held = 0;
            for (const auto c : t.purses)
                held += c;
            daysOut << (first ? "" : ",") << json(community.empty() ? "(none)" : community) << ":{\"residents\":" << t.purses.size()
                    << ",\"held\":" << held << ",\"median\":" << (t.purses.empty() ? 0 : t.purses[t.purses.size() / 2])
                    << ",\"food_shops\":" << t.foodShops << ",\"food\":" << t.food << ",\"short\":" << t.short_
                    << ",\"broke\":" << t.broke << ",\"hungry\":" << t.hungry << ",\"starving\":" << t.starving
                    << ",\"producers_held\":" << t.producers << ",\"keepers_held\":" << t.keepers << ",\"town_held\":" << t.town << "}";
            first = false;
        }
        daysOut << "},\"condition\":{";
        first = true;
        for (const auto& [community, c] : society.state().memory.condition)
        {
            daysOut << (first ? "" : ",") << json(community) << ':' << c;
            first = false;
        }
        // The going rate (doc 64, Phase 13, round 7): the realm's, and each town's as its employers pay it.
        daysOut << "},\"going\":{\"\":" << society.goingRate("");
        for (const auto& [community, c] : society.state().memory.condition)
            daysOut << ',' << json(community) << ':' << society.goingRate(community);
        daysOut << "},\"model\":" << modelMeasures(day, std::string(when) != "end");
        if (players)
            daysOut << ",\"players\":" << players->takeDay();
        daysOut << "}\n";
        daysOut.flush();
        return std::make_tuple(supply, society.conserved(), n ? people[n / 2] : 0, gini, grownShort, broke, starving);
    };

    const auto writeDay = [&](int day) {
        for (const auto& [key, f] : flows[day])
            flowsOut << day << ',' << csv(std::get<0>(key)) << ',' << csv(std::get<1>(key)) << ',' << csv(std::get<2>(key)) << ','
                     << f.entries << ',' << f.coins << ',' << f.goods << '\n';
        for (const auto& [key, f] : trade[day])
            tradeOut << day << ',' << csv(std::get<0>(key)) << ',' << csv(std::get<1>(key)) << ',' << csv(std::get<2>(key)) << ','
                     << f.entries << ',' << f.coins << ',' << f.goods << '\n';
        tradeOut.flush();
        trade.erase(trade.begin(), trade.upper_bound(day));
        for (const auto& [item, f] : foods[day])
            foodOut << day << ',' << csv(item) << ',' << f.in << ',' << f.crafted << ',' << f.used << ',' << f.eaten << ','
                    << f.spoiled << ',' << f.bought << '\n';
        foodOut.flush();
        foods.erase(foods.begin(), foods.upper_bound(day));
        for (const auto& [key, f] : townFoods[day])
            foodTownsOut << day << ',' << csv(key.first) << ',' << csv(key.second) << ',' << f.in << ',' << f.crafted << ','
                         << f.used << ',' << f.eaten << ',' << f.spoiled << ',' << f.bought << ',' << f.paid << '\n';
        for (const auto& [key, p] : prices[day])
        {
            const auto* good = items::good(key.first);
            pricesOut << day << ',' << csv(key.first) << ',' << csv(key.second) << ',' << p.first << ',' << p.second << ','
                      << (good ? good->price : 0) << '\n';
        }
        for (const auto& [key, f] : townFlows[day])
            townFlowsOut << day << ',' << csv(std::get<0>(key)) << ',' << csv(std::get<1>(key)) << ',' << csv(std::get<2>(key)) << ','
                         << csv(std::get<3>(key)) << ',' << f.entries << ',' << f.coins << '\n';
        for (const auto& [key, q] : foodMoves[day])
            foodMovesOut << day << ',' << csv(std::get<0>(key)) << ',' << csv(std::get<1>(key)) << ',' << csv(std::get<2>(key)) << ','
                         << csv(std::get<3>(key)) << ',' << q << '\n';
        foodMovesOut.flush();
        foodMoves.erase(foodMoves.begin(), foodMoves.upper_bound(day));
        foodTownsOut.flush();
        pricesOut.flush();
        townFlowsOut.flush();
        townFoods.erase(townFoods.begin(), townFoods.upper_bound(day));
        prices.erase(prices.begin(), prices.upper_bound(day));
        townFlows.erase(townFlows.begin(), townFlows.upper_bound(day));
        for (const auto& [id, coins] : wages[day])
        {
            const auto* job = society.jobOf(id);
            wagesOut << day << ',' << csv(id) << ',' << csv(job ? job->title : std::string("(none)")) << ',' << coins << '\n';
        }
        // What is held of each ingredient world-wide, at the day's end.
        auto& mats = materials[day];
        std::map<std::string, std::int64_t> held;
        for (const auto& [id, a] : society.state().accounts)
            for (const auto& [item, q] : a.stock)
                if (ingredients.count(items::baseOf(item)))
                    held[items::baseOf(item)] += q;
        for (const auto& item : ingredients)
            mats[item].held = held[item];
        for (const auto& [item, m] : mats)
            if (ingredients.count(item) || m.crafted || m.broughtIn)
                materialsOut << day << ',' << csv(item) << ',' << m.makerShort << ',' << m.supplierShort << ',' << m.held << ','
                             << m.broughtIn << ',' << m.crafted << ',' << m.used << ',' << m.bought << ',' << m.carted << '\n';
        flowsOut.flush();
        wagesOut.flush();
        materialsOut.flush();
        eventsOut.flush();
        std::int64_t wageTotal = 0;
        for (const auto& [id, c] : wages[day])
            wageTotal += c;
        std::int64_t makerShort = 0;
        for (const auto& [item, m] : mats)
            makerShort += m.makerShort;
        flows.erase(flows.begin(), flows.upper_bound(day));
        wages.erase(wages.begin(), wages.upper_bound(day));
        materials.erase(materials.begin(), materials.upper_bound(day));
        return std::make_pair(wageTotal, makerShort);
    };

    // The day's hires at 23:00 (doc 64, Phase 13: how many of the slots posted are taken, and by whom): a business's or
    // an institution's hire running today, its slots taken, those open at its dawn (taken at the day before's 23:00;
    // one posted since, all), and its takers as they stand now.
    std::ofstream hiresOut(out / "hires.csv");
    hiresOut << "day,hire,payer,kind,slots,pay_slot,until,field,taken,open_at_dawn,takers,worked,away,away_worked,lodged\n";
    std::map<std::string, int> takenBefore;
    const auto sampleHires = [&](std::int64_t day) {
        std::map<std::string, int> taken;
        for (const auto& j : society.oddJobs())
        {
            if (j.until < day)
                continue;                           // (A day's odd job, or a hire run out.)
            std::string takers;
            int worked = 0, away = 0, awayWorked = 0, lodged = 0;
            for (const auto& [who, stage] : j.stage)
            {
                // (Its day's spells done: paid, stage 2. From away: of another town than the hire's, by the rule the hire's
                // own town is found by, asked as it stands: round 6, the round-5 review.)
                const bool far = society.townOfResident(who) != j.community;
                worked += stage == 2;
                away += far;
                awayWorked += far && stage == 2;
                lodged += society.state().memory.lodging.count(who) > 0;
                const auto* e = server.entity(who);
                const auto* job = society.jobOf(who);
                const int age = e ? e->age : 30;
                const auto payer = job ? society.payerOf(who, *job, age) : Society::Payer{};
                const auto key = job ? society.employerKey(payer, *job) : std::string();
                const char* kind = age < 16                                                 ? "child"
                                   : !job                                                   ? "jobless"
                                   : job->title == Society::LabourTitle                     ? "works"
                                   : society.ownerUnpaid(who, *job)                         ? "owner"
                                   : society.idleHand(who, payer)                           ? "idle"
                                   : j.field && society.calledToTheLand(who, *job, age, day) ? "called"
                                   : job->paid                                              ? "better"
                                                                                            : "other";
                takers += (takers.empty() ? "" : ";") + std::string(kind) + (key.empty() ? "" : "@" + key);
            }
            const auto before = takenBefore.find(j.id);
            hiresOut << day << ',' << csv(j.id) << ',' << csv(j.payer) << ',' << csv(j.kind) << ',' << j.slots << ','
                     << (j.slots > 0 ? j.pay / j.slots : 0) << ',' << j.until << ',' << (j.field ? 1 : 0) << ',' << j.stage.size()
                     << ',' << j.slots - (before == takenBefore.end() ? 0 : before->second) << ',' << csv(takers) << ',' << worked << ','
                     << away << ',' << awayWorked << ',' << lodged << '\n';
            taken[j.id] = int(j.stage.size());
        }
        takenBefore.swap(taken);
        hiresOut.flush();
    };
    std::ofstream playersOut;
    if (players)
        playersOut.open(out / "players.csv"), playersOut << "day,player,scenario,action,item,quantity,coins\n";
    const auto writePlayers = [&] {
        if (!players)
            return;
        for (const auto& m : players->takeMoves())
            playersOut << m.day << ',' << csv(m.player) << ',' << csv(m.scenario) << ',' << csv(m.action) << ',' << csv(m.item)
                       << ',' << m.quantity << ',' << m.coins << '\n';
        playersOut.flush();
    };

    // The run's tracking's files (round 10, lane 0): see the top of the file.
    std::ofstream accountsOut(out / "accounts.csv"), accountFlowsOut(out / "account_flows.csv"),
        accountPeersOut(out / "account_peers.csv"), goodsOut(out / "goods.csv"), hungerOut(out / "hunger.csv"),
        sentinelsOut(out / "sentinels.csv");
    accountsOut << "day,account,holder,town,opening,balance,in,out,in_entries,out_entries,unexplained\n";
    accountFlowsOut << "day,account,dir,kind,entries,coins\n";
    accountPeersOut << "day,account,dir,peer,peer_holder,coins\n";
    goodsOut << "day,town,item,shelf,homes,stores,made,brought_in,used,eaten,spoiled,worn,carried_in,carried_out,sold,"
                "sold_coins,mean_price\n";
    hungerOut << "day,town,residents,hungry,starving,hungry_poor,starving_poor,shelf_food,cheapest\n";
    sentinelsOut << "day,kind,subject,value,threshold,weeks,detail\n";
    const auto accountTown = [&](const std::string& id) -> std::string {
        if (id.rfind("caravan:", 0) == 0)
        {
            if (const auto found = caravanFrom.find(id); found != caravanFrom.end())
                return found->second;
            for (const auto& c : server.roads().caravans)
                if (c.account == id)
                    return caravanFrom[id] = c.from;
            return "(road)";
        }
        return townOf(id);
    };

    // Each town's hungry and starving, by home, and of them those holding less than the cheapest food a resident paid
    // for there in the last week (the realm's cheapest where the town had no sale); its shops' food (by where each
    // keeper works). At each day's record, to hunger.csv; and the hunger sentinel's.
    struct TownHunger
    {
        int residents = 0, hungry = 0, starving = 0, hungryPoor = 0, starvingPoor = 0;
        std::int64_t shelf = 0;
        double cheapest = 0;
    };
    const auto measureHunger = [&](int day) {
        std::map<std::string, TownHunger> towns;
        double realm = 0;
        for (auto& [town, byDay] : foodPrices)
        {
            byDay.erase(byDay.begin(), byDay.lower_bound(day - 7));
            double least = 0;
            for (const auto& [d, p] : byDay)
                least = least > 0 ? std::min(least, p) : p;
            if (least > 0)
                towns[town].cheapest = least, realm = realm > 0 ? std::min(realm, least) : least;
        }
        for (const auto& r : society.authored().residents)
            if (r.role == "merchant" && society.state().residents.count(r.id))
                if (const auto* till = society.account(society.tillOf(r.id)))
                {
                    const auto town = server.communityOf(r.work.cell);
                    auto& t = towns[town.empty() ? "(country)" : town];
                    for (const auto& [item, count] : till->stock)
                        if (count > 0 && Society::edible(item))
                            t.shelf += count;
                }
        for (const auto& [id, life] : society.state().residents)
        {
            const auto* e = server.entity(id);
            if (!e || e->dead)
                continue;
            const auto town = server.communityOf(life.homeCell);
            auto& t = towns[town.empty() ? "(country)" : town];
            const auto* a = society.account(id);
            const double cash = double(a ? a->cash : 0), cheapest = t.cheapest > 0 ? t.cheapest : realm;
            ++t.residents;
            t.hungry += life.hunger >= 70;
            t.starving += life.hunger >= 90;
            t.hungryPoor += life.hunger >= 70 && (cheapest > 0 ? cash < cheapest : cash <= 0);
            t.starvingPoor += life.hunger >= 90 && (cheapest > 0 ? cash < cheapest : cash <= 0);
        }
        return towns;
    };
    const auto writeHunger = [&](int day) {
        const auto towns = measureHunger(day);
        for (const auto& [town, t] : towns)
            if (t.residents || t.shelf)
                hungerOut << day << ',' << csv(town) << ',' << t.residents << ',' << t.hungry << ',' << t.starving << ','
                          << t.hungryPoor << ',' << t.starvingPoor << ',' << t.shelf << ',' << t.cheapest << '\n';
        hungerOut.flush();
        return towns;
    };

    // The sentinels (round 10, lane 0), checked at each week's record: see the top of the file.
    constexpr double DrainRatio = 1.3, DrainLeast = 100, ShareBand = 1.5, ShareLeast = .02, HoardMonths = 3, GiniRise = .02;
    constexpr int HungryLeast = 5;
    const int SentinelWeeks = sentinelWeeks;
    std::map<int, std::vector<double>> modelTargets;   // Day -> the model's homes, firms, Houses, church, town.
    {
        std::ifstream file(modelShares);
        std::string line;
        for (std::getline(file, line); std::getline(file, line);)
        {
            std::stringstream row(line);
            std::string cell;
            std::vector<double> values;
            while (std::getline(row, cell, ','))
                values.push_back(std::atof(cell.c_str()));
            if (values.size() == 6)
                modelTargets[int(values[0])] = {values.begin() + 1, values.end()};
        }
        std::cout << (modelTargets.empty() ? "no model shares in " : "model shares from ") << modelShares.string()
                  << (modelTargets.empty() ? ": the share sentinel is off\n" : "\n");
    }
    static const char* const Groups[] = {"homes", "firms", "Houses", "church", "town"};
    // A kind of holder's group, as compare.py's GROUPS (with the grown wolves out of work among the homes: compare.py
    // leaves them in its "other").
    const auto groupOf = [](const std::string& k) {
        if (k == "merchant" || k == "guard" || k == "producer" || k == "paid civilian" || k == "unpaid civilian" ||
            k == "apprentice" || k == "child" || k == "retired" || k == "resident" || k == "player" ||
            k == "adult without work" || k.rfind("resident", 0) == 0)
            return 0;
        if (k == "shop till" || k == "house till" || k == "farm till")
            return 1;
        if (k == "great house")
            return 2;
        if (k == "town church")
            return 3;
        if (k == "town treasury" || k == "capital treasury" || k.rfind("town ", 0) == 0)
            return 4;
        return -1;
    };
    struct Streak
    {
        int weeks = 0;
        bool fired = false;
        std::int64_t in = 0, out = 0;
        std::map<std::string, std::int64_t> outKinds;
    };
    std::map<std::string, Streak> drains, drifts, hoards, hungers;
    Streak giniStreak;
    std::deque<std::map<std::string, std::unordered_map<std::string, std::int64_t>>> treasuryWeeks;   // Peer nets, by week.
    std::deque<std::map<std::string, std::int64_t>> treasuryOpenings;   // And each one's balance at its week's start.
    std::map<int, double> giniByDay;
    std::set<std::string> firedKinds;               // This week's, for --stop-on-sentinel.
    int firstDay = int(std::floor(server.calendarDays()));
    const auto fire = [&](int day, const std::string& kind, const std::string& subject, double value, double threshold,
                          int weeks, const std::string& detail) {
        sentinelsOut << day << ',' << kind << ',' << csv(subject) << ',' << value << ',' << threshold << ',' << weeks << ','
                     << csv(detail) << '\n';
        std::cout << "SENTINEL day " << day << " " << kind << " " << subject << ": " << detail << "\n";
        firedKinds.insert(kind);
    };
    const auto weeksOf = [](int n) { return std::to_string(n) + (n == 1 ? " week" : " weeks"); };
    const auto pennies_ = [](double coins) {
        std::ostringstream s;
        s << std::fixed << std::setprecision(0) << coins << "p";
        return s.str();
    };
    const auto checkSentinels = [&](int day, const std::map<std::string, TownHunger>& hunger) {
        // Drain: a House, a treasury, the church or a caravan paying out over 1.3 times what came in, four weeks in a row.
        std::set<std::string> checked;
        for (const auto& [id, w] : accountWeeks)
            if (id.rfind("house:", 0) == 0 || id.rfind("stores:", 0) == 0 || id == "treasury" || id == Society::SharedChurch ||
                id.rfind("caravan:", 0) == 0)
                checked.insert(id);
        for (auto& [id, s] : drains)
            checked.insert(id);
        for (const auto& id : checked)
        {
            std::int64_t in = 0, outgo = 0;
            const auto w = accountWeeks.find(id);
            if (w != accountWeeks.end())
            {
                for (const auto& [k, f] : w->second.in)
                    in += f.coins;
                for (const auto& [k, f] : w->second.out)
                    outgo += f.coins;
            }
            auto& s = drains[id];
            if (outgo < DrainLeast || double(outgo) <= DrainRatio * double(in))
            {
                s = {};
                continue;
            }
            ++s.weeks, s.in += in, s.out += outgo;
            for (const auto& [k, f] : w->second.out)
                s.outKinds[k] += f.coins;
            if (s.weeks != SentinelWeeks)
                continue;
            std::vector<std::pair<std::int64_t, std::string>> kinds;
            for (const auto& [k, c] : s.outKinds)
                kinds.push_back({-c, k});
            std::sort(kinds.begin(), kinds.end());
            std::ostringstream detail;
            const auto* a = society.account(id);
            detail << "out/in " << std::fixed << std::setprecision(2) << (s.in ? double(s.out) / double(s.in) : 0.)
                   << " over " << weeksOf(SentinelWeeks) << " (out " << pennies_(double(s.out)) << ", in " << pennies_(double(s.in))
                   << "), balance " << pennies_(double(a ? a->cash : 0)) << "; out most: ";
            for (std::size_t i = 0; i < kinds.size() && i < 3; ++i)
                detail << (i ? "; " : "") << kinds[i].second << " " << pennies_(double(-kinds[i].first));
            // (Its value 999 when nothing came in.)
            fire(day, "drain", id, s.in ? double(s.out) / double(s.in) : 999, DrainRatio, s.weeks, detail.str());
        }
        for (auto it = drains.begin(); it != drains.end();)
            it = it->second.weeks == 0 ? drains.erase(it) : std::next(it);
        // Share: a kind of holder's share off the model's, four weeks in a row, from the run's fourth week.
        if (!modelTargets.empty() && day - firstDay >= 7 * SentinelWeeks)
        {
            double held[5] = {0, 0, 0, 0, 0}, total = 0;
            for (const auto& [id, a] : society.state().accounts)
                if (a.cash > 0)
                {
                    total += double(a.cash);
                    if (const int g = groupOf(holder(server, id)); g >= 0)
                        held[g] += double(a.cash);
                }
            auto target = modelTargets.upper_bound(day);
            if (target != modelTargets.begin())
                --target;
            for (int g = 0; g < 5 && total > 0; ++g)
            {
                const double share = held[g] / total, model = target->second[std::size_t(g)];
                auto& s = drifts[Groups[g]];
                if (std::abs(share - model) <= ShareLeast || (share <= model * ShareBand && share * ShareBand >= model))
                {
                    s = {};
                    continue;
                }
                if (++s.weeks == SentinelWeeks)
                {
                    std::ostringstream detail;
                    detail << std::fixed << std::setprecision(3) << "hold " << share << " of the money against the model's "
                           << model << " (its day " << target->first << "), off by over half again or a third " << weeksOf(SentinelWeeks)
                           << " in a row";
                    fire(day, "share", Groups[g], share, model, s.weeks, detail.str());
                }
            }
        }
        // Hoard: a treasury or the church holding over three months (28 days) of its last four weeks' income, and more
        // than it held four weeks before (a town founded with a few months' reserve that it is spending is no hoard).
        {
            std::map<std::string, std::int64_t> opened;
            for (const auto& [id, c] : opening)
                if (id.rfind("stores:", 0) == 0 || id == "treasury" || id == Society::SharedChurch)
                    opened[id] = c;
            treasuryOpenings.push_back(std::move(opened));
            std::map<std::string, std::unordered_map<std::string, std::int64_t>> nets;
            for (const auto& [id, w] : accountWeeks)
                if (id.rfind("stores:", 0) == 0 || id == "treasury" || id == Society::SharedChurch)
                {
                    auto& net = nets[id];
                    for (const auto& [peer, c] : w.from)
                        net[peer] += c;
                    for (const auto& [peer, c] : w.to)
                        net[peer] -= c;
                }
            treasuryWeeks.push_back(std::move(nets));
            if (treasuryWeeks.size() > std::size_t(SentinelWeeks))
                treasuryWeeks.pop_front(), treasuryOpenings.pop_front();
            std::set<std::string> treasuries;
            for (const auto& [id, a] : society.state().accounts)
                if (id.rfind("stores:", 0) == 0 || id == "treasury" || id == Society::SharedChurch)
                    treasuries.insert(id);
            for (const auto& id : treasuries)
            {
                std::unordered_map<std::string, std::int64_t> net;
                for (const auto& week : treasuryWeeks)
                    if (const auto found = week.find(id); found != week.end())
                        for (const auto& [peer, c] : found->second)
                            net[peer] += c;
                std::int64_t income = 0;
                for (const auto& [peer, c] : net)
                    income += std::max<std::int64_t>(0, c);
                const auto* a = society.account(id);
                // (Its income a month: four weeks', over the weeks in hand.)
                const double month = double(income) * 4 / double(treasuryWeeks.size());
                const double months = income > 0 && a ? double(a->cash) / month : 0;
                const auto before = treasuryOpenings.front().find(id);
                const bool grew = a && before != treasuryOpenings.front().end() && a->cash > before->second;
                auto& s = hoards[id];
                if (treasuryWeeks.size() < std::size_t(SentinelWeeks) || months <= HoardMonths || !grew)
                {
                    s = {};
                    continue;
                }
                ++s.weeks;
                if (!s.fired)
                {
                    s.fired = true;
                    std::ostringstream detail;
                    detail << "holds " << pennies_(double(a->cash)) << ", " << std::fixed << std::setprecision(1) << months
                           << " months of its income (" << pennies_(month) << " a month, each payer net of what it was paid), up from "
                           << pennies_(double(before->second)) << " " << weeksOf(int(treasuryWeeks.size())) << " before";
                    fire(day, "hoard", id, months, HoardMonths, s.weeks, detail.str());
                }
            }
        }
        // Hunger: a town with hungry wolves while its shelves hold a unit of food for each.
        for (const auto& [town, t] : hunger)
        {
            auto& s = hungers[town];
            if (t.hungry < std::max(HungryLeast, (t.residents + 99) / 100) || t.shelf < t.hungry)
            {
                s = {};
                continue;
            }
            ++s.weeks;
            if (!s.fired)
            {
                s.fired = true;
                std::ostringstream detail;
                detail << t.hungry << " hungry (" << t.starving << " starving) of " << t.residents << " while its shelves hold "
                       << t.shelf << " food; " << t.hungryPoor << " of them hold less than the cheapest, " << std::fixed
                       << std::setprecision(1) << t.cheapest << "p (money-hunger), " << t.hungry - t.hungryPoor << " could pay (food-hunger)";
                fire(day, "hunger", town, t.hungry, std::max(HungryLeast, (t.residents + 99) / 100), s.weeks, detail.str());
            }
        }
        // Gini: the households' up over 0.02 on 30 days before.
        if (const auto then = giniByDay.find(day - 30); then != giniByDay.end())
        {
            if (householdGini - then->second <= GiniRise)
                giniStreak = {};
            else if (++giniStreak.weeks, !giniStreak.fired)
            {
                giniStreak.fired = true;
                std::ostringstream detail;
                detail << std::fixed << std::setprecision(3) << "the households' Gini " << householdGini << ", up "
                       << householdGini - then->second << " on day " << day - 30 << "'s " << then->second;
                fire(day, "gini", "households", householdGini - then->second, GiniRise, giniStreak.weeks, detail.str());
            }
        }
        sentinelsOut.flush();
    };

    // The week's accounts and goods (round 10, lane 0), at a week's record and the run's end; then a new week.
    int lastWeek = firstDay;
    const auto writeWeek = [&](int day) {
        // (Every account tracked that held money or moved any: a caravan long gone has neither.)
        std::set<std::string> ids;
        for (const auto& [id, c] : opening)
            ids.insert(id);
        for (const auto& [id, w] : accountWeeks)
            ids.insert(id);
        for (const auto& [id, a] : society.state().accounts)
            if (trackedAccount(id))
                ids.insert(id);
        for (const auto& id : ids)
        {
            const auto* a = society.account(id);
            const std::int64_t balance = a ? a->cash : 0;
            const auto was = opening.find(id);
            const std::int64_t before = was == opening.end() ? 0 : was->second;
            const auto w = accountWeeks.find(id);
            if (w == accountWeeks.end() && balance == 0 && before == 0)
                continue;
            std::int64_t in = 0, outgo = 0, inEntries = 0, outEntries = 0;
            if (w != accountWeeks.end())
            {
                for (const auto& [k, f] : w->second.in)
                    in += f.coins, inEntries += f.entries;
                for (const auto& [k, f] : w->second.out)
                    outgo += f.coins, outEntries += f.entries;
            }
            accountsOut << day << ',' << csv(id) << ',' << csv(holder(server, id)) << ',' << csv(accountTown(id)) << ',' << before
                        << ',' << balance << ',' << in << ',' << outgo << ',' << inEntries << ',' << outEntries << ','
                        << balance - before - in + outgo << '\n';
            if (w == accountWeeks.end())
                continue;
            for (const auto* dir : {"in", "out"})
            {
                const auto& kinds = std::string(dir) == "in" ? w->second.in : w->second.out;
                for (const auto& [k, f] : kinds)
                    accountFlowsOut << day << ',' << csv(id) << ',' << dir << ',' << csv(k) << ',' << f.entries << ',' << f.coins << '\n';
                // Its five biggest payers (payees), and the rest as one.
                const auto& peers = std::string(dir) == "in" ? w->second.from : w->second.to;
                std::vector<std::pair<std::int64_t, std::string>> top;
                for (const auto& [peer, c] : peers)
                    top.push_back({-c, peer});
                std::sort(top.begin(), top.end());
                std::int64_t rest = 0;
                for (std::size_t i = 0; i < top.size(); ++i)
                    if (i < 5)
                        accountPeersOut << day << ',' << csv(id) << ',' << dir << ',' << csv(top[i].second) << ','
                                        << csv(holder(server, top[i].second)) << ',' << -top[i].first << '\n';
                    else
                        rest -= top[i].first;
                if (top.size() > 5)
                    accountPeersOut << day << ',' << csv(id) << ',' << dir << ',' << csv("(others: " + std::to_string(top.size() - 5) + ")")
                                    << ",\"\"," << rest << '\n';
            }
        }
        // What each town holds of each good at the week's end: on shelves (tills, and keepers' purses that are their
        // tills), in homes (larders, and the residents' own), in stores (the rest).
        for (const auto& [id, a] : society.state().accounts)
        {
            if (a.stock.empty())
                continue;
            const bool shelf = id.rfind("till:", 0) == 0 || ownTills.count(id);
            const bool home = !shelf && (id.rfind("home:", 0) == 0 || society.state().residents.count(id));
            const auto town = accountTown(id);
            for (const auto& [item, q] : a.stock)
                if (q > 0)
                {
                    auto& g = goodsWeek[{town, items::baseOf(item)}];
                    (shelf ? g.shelf : home ? g.homes : g.stores) += q;
                }
        }
        for (const auto& [key, g] : goodsWeek)
            goodsOut << day << ',' << csv(key.first) << ',' << csv(key.second) << ',' << g.shelf << ',' << g.homes << ',' << g.stores
                     << ',' << g.made << ',' << g.broughtIn << ',' << g.used << ',' << g.eaten << ',' << g.spoiled << ',' << g.worn
                     << ',' << g.carriedIn << ',' << g.carriedOut << ',' << g.sold << ',' << g.soldCoins << ','
                     << (g.sold ? double(g.soldCoins) / double(g.sold) : 0.) << '\n';
        accountsOut.flush();
        accountFlowsOut.flush();
        accountPeersOut.flush();
        goodsOut.flush();
    };
    const auto newWeek = [&](int day) {
        refreshTills();                             // (A keeper's purse that has become its shop's till is tracked from now.)
        opening.clear();
        for (const auto& [id, a] : society.state().accounts)
            if (trackedAccount(id))
                opening[id] = a.cash;
        accountWeeks.clear();
        goodsWeek.clear();
        lastWeek = day;
    };

    snapshot(int(std::floor(server.calendarDays())), "start");
    giniByDay[firstDay] = householdGini;
    std::cout << "residents " << society.state().residents.size() << ", makers and suppliers " << makers.size()
              << ", ingredients " << ingredients.size() << ", from calendar day " << server.calendarDays() << "\n"
              << std::flush;

    const auto wallBegin = std::chrono::steady_clock::now();
    const long ticks = long(days * 24 * 600 / step);
    const long minuteTicks = std::max(1L, std::lround(60 / step));   // (A world minute's ticks: 1200 at 0.05.)
    int day = int(std::floor(server.calendarDays()));
    today = day;
    double nextHour = std::floor(server.calendarDays() * 24 + 1) / 24;
    // RATW_TRACE=id[,id...]: each traced wolf's place, task and activity, whenever one of them changes.
    std::vector<std::string> traced;
    if (const char* t = std::getenv("RATW_TRACE"))
        for (std::stringstream list(t); list.good();)
        {
            std::string id;
            std::getline(list, id, ',');
            if (!id.empty())
                traced.push_back(id);
        }
    std::map<std::string, std::string> lastTrace;
    // RATW_STOCK=account[,account...]: each named account's cash and stock every three game hours, to stock.csv.
    std::vector<std::string> stocked;
    if (const char* t = std::getenv("RATW_STOCK"))
        for (std::stringstream list(t); list.good();)
        {
            std::string id;
            std::getline(list, id, ',');
            if (!id.empty())
                stocked.push_back(id);
        }
    std::ofstream stockOut;
    if (!stocked.empty())
        stockOut.open(out / "stock.csv"), stockOut << "day,account,cash,stock\n";
    int nextStockHour = 0;
    // --ask-payers: every resident with a post asked who pays it, read only; written at the day's record.
    std::ofstream payersOut;
    if (askPayers)
        payersOut.open(out / "payers.csv"), payersOut << "day,resident,home,work,title,payer,paid_as\n";
    const auto askAll = [&](int recordDay) {
        for (const auto& [id, life] : society.state().residents)
            if (const auto* job = society.jobOf(id))
            {
                const auto* e = server.entity(id);
                const auto payer = society.payerOf(id, *job, e ? e->age : 30);
                if (recordDay >= 0)
                    payersOut << recordDay << ',' << csv(id) << ',' << csv(life.homeCell) << ',' << csv(job->work.cell) << ','
                              << csv(job->title) << ',' << csv(payer.account) << ',' << csv(payer.whom) << '\n';
            }
        payersOut.flush();
    };
    if (askPayers)
        askAll(-1);                                 // (Before the first tick: before the society has decided anything.)
    std::string stopped;                            // The sentinel that ended the run (--stop-on-sentinel).
    long ticksRun = ticks;
    double tickSeconds = 0;                         // (The server's own time in the day, apart from this watch's.)
    std::int64_t paceDay = -1;                      // (The society's day pace.csv last had.)
    for (long i = 0; i < ticks; ++i)
    {
        const auto tickBegin = std::chrono::steady_clock::now();
        server.tick(step);
        // The pace each of the society's days sets at its turn (Society::tendLabour), every digit of it.
        if (const auto turned = society.state().budgetDay; turned != paceDay)
        {
            paceDay = turned;
            paceOut << std::int64_t(std::floor(server.calendarDays())) << ",turn," << turned << ',' << society.state().memory.pace << '\n'
                    << std::flush;
        }
        if (i % minuteTicks == 0)
            server.makeRequestedResidents();        // (Births and newcomers, as the game makes them from files: doc 64.)
        tickSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - tickBegin).count();
        absorb();
        for (const auto& id : traced)
            if (const auto* e = server.entity(id))
            {
                const auto* life = society.resident(id);
                std::ostringstream now;
                now << e->cellId << " | " << (life ? life->task + " -> " + life->goalCell : std::string()) << " | " << e->activity
                    << (e->offstage ? " (offstage)" : "");
                for (const auto& k : server.roads().contracts)
                    if (k.taker == id && k.status == "taken")
                        if (const auto* job = society.jobOf(k.source))
                            now << " [contract " << k.id << ": from " << k.source << " at " << (job->role == "merchant" ? job->serve.cell : job->work.cell)
                                << ", carried " << k.carried << "]";
                if (lastTrace[id] != now.str())
                {
                    lastTrace[id] = now.str();
                    std::cerr << "TRACE day " << std::fixed << std::setprecision(4) << server.calendarDays() << std::defaultfloat << " " << id
                              << " @" << int(e->position.x) << "," << int(e->position.y) << ": " << now.str() << "\n";
                }
            }
        if (server.calendarDays() >= nextHour)
        {
            nextHour += 1. / 24;
            sampleMaterials();
            if (askPayers)
                askAll(-1);
            if (const double now = server.calendarDays(); int(std::floor((now - std::floor(now)) * 24 + 1e-9)) == 23)
                sampleHires(std::int64_t(std::floor(now)));
            if (!stocked.empty() && nextStockHour++ % 3 == 0)
                for (const auto& id : stocked)
                    if (const auto* a = society.account(id))
                    {
                        stockOut << std::fixed << std::setprecision(3) << server.calendarDays() << std::defaultfloat << ",\"" << id << "\"," << a->cash << ",\"";
                        for (const auto& [item, n] : a->stock)
                            if (n > 0)
                                stockOut << item << ":" << n << " ";
                        stockOut << "\"\n";
                    }
        }
        if (const int now = int(std::floor(server.calendarDays())); now != day)
        {
            today = now;
            if (askPayers)
                askAll(now);
            // The day's own cost (ms a tick, by part and by stage of the schedules), to see what grows as days pass.
            {
                static World::TickProfile before;
                static auto dayBegin = wallBegin;
                static long ticksBefore = 0;
                const auto& p = server.tickProfile();
                const double n = double(std::max<long>(1, i - ticksBefore));
                const double dayWall = std::chrono::duration<double>(std::chrono::steady_clock::now() - dayBegin).count();
                // (Every part: the server's tick, its parts, what of it no part counts, and this watch's own bookkeeping.)
                const auto part = [&](double now_, double then) { return (now_ - then) / n; };
                const double server_ = tickSeconds * 1000 / n;
                const double counted = part(p.schedules.total, before.schedules.total) + part(p.movement.total, before.movement.total) +
                                       part(p.separation.total, before.separation.total) + part(p.views.total, before.views.total) +
                                       part(p.streaming.total, before.streaming.total);
                std::cout << std::fixed << std::setprecision(4) << "  cost day " << now << ": " << dayWall * 1000 / n << " ms a tick (society "
                          << part(p.stages[0], before.stages[0]) << ", roads " << part(p.stages[2], before.stages[2]) << ", errands "
                          << part(p.stages[4], before.stages[4]) << ", movement " << part(p.movement.total, before.movement.total)
                          << ", views " << part(p.views.total, before.views.total) << ", streaming " << part(p.streaming.total, before.streaming.total)
                          << "; crime " << part(p.stages[3], before.stages[3]) << ", bonds " << part(p.stages[1], before.stages[1])
                          << ", wants " << part(p.stages[5], before.stages[5]) << ", routes " << part(p.stages[6], before.stages[6])
                          << ", separation " << part(p.separation.total, before.separation.total) << "; server " << server_
                          << ", uncounted " << server_ - counted << ", watch " << dayWall * 1000 / n - server_ << ")\n" << std::defaultfloat;
                tickSeconds = 0;
                // What the society holds, to see what accumulates: accounts, kinds of goods held in all and at most in one.
                std::size_t kinds = 0, most = 0;
                std::string mostAt;
                for (const auto& [aid, a] : society.state().accounts)
                {
                    kinds += a.stock.size();
                    if (a.stock.size() > most)
                        most = a.stock.size(), mostAt = aid;
                }
                std::cout << "  holds day " << now << ": accounts " << society.state().accounts.size() << ", kinds " << kinds << ", most "
                          << most << " (" << mostAt << "), odd jobs " << society.oddJobs().size() << ", residents "
                          << society.state().residents.size() << ", ledger " << society.state().ledger.size() << "\n";
                before = p;
                dayBegin = std::chrono::steady_clock::now();
                ticksBefore = i;
            }
            // The economy's monitor's measures (doc 46; doc 64, Phase 12): each in full, and its towns.
            for (const auto& b : server.society().takeBriefs())
            {
                orchestraOut << orchestra::briefText(b) << '\n';
                for (const auto& t : b.towns)
                    orchestraTownsOut << b.day << ',' << csv(t.id) << ',' << t.people << ',' << t.distress << ',' << t.raw << ',' << csv(t.kind)
                                      << ',' << t.foodCost << ',' << t.hungry << ',' << t.starving << ',' << t.short_ << ',' << t.poor << ','
                                      << t.idle << ',' << t.shopFoodDays << ',' << t.takingsRatio << ',' << t.netInflow << ',' << t.playerIn
                                      << ',' << t.playerOut << ',' << t.playerGoods << '\n';
                std::cout << "  measured for day " << b.day << ": land distress " << std::fixed << std::setprecision(3) << b.landDistress
                          << ", residents hold " << b.residentShare << ", the poorer half " << b.bottomShare << ", Gini " << b.gini
                          << std::defaultfloat << (b.decided ? " (the week weighed)" : "") << "\n";
            }
            orchestraOut.flush();
            orchestraTownsOut.flush();
            const auto [wageTotal, makerShort] = writeDay(day);
            writePlayers();
            const auto [supply, conserved, median, gini, short_, broke, starving] = snapshot(now, "day end");
            // The tracking's (round 10, lane 0): the day's hunger, and at a week's record its accounts, goods and
            // sentinels.
            giniByDay[now] = householdGini;
            const auto hunger = writeHunger(now);
            if ((now - firstDay) % 7 == 0)
            {
                firedKinds.clear();
                checkSentinels(now, hunger);
                writeWeek(now);
                newWeek(now);
                for (const auto& kind : firedKinds)
                    if (stopOn.count(kind))
                        stopped = kind;
            }
            // The roads' standing orders at the day's end (doc 64, Phase 3b): what each asks a week and how short it
            // has fallen since it was last agreed, and what its buyer couldn't pay for.
            {
                static std::ofstream orders(out / "orders.csv");
                static bool header = (orders << "day,buyer,town,from,item,per_week,delivered,shortfall,unpaid\n", true);
                (void)header;
                for (const auto& o : server.roads().orders)
                    orders << now << ",\"" << o.buyer << "\",\"" << o.town << "\",\"" << o.from << "\",\"" << o.item << "\","
                           << o.perWeek << "," << o.delivered << "," << o.shortfall << "," << o.unpaid << "\n";
                // And the buyers' refusals standing (RoadsState::refused; round 6, the roads' review): since when, and
                // the money in the buyer's till now, against which a new order's week is weighed.
                static std::ofstream refusals(out / "refusals.csv");
                static bool refusalsHeader = (refusals << "day,buyer,item,since,cash\n", true);
                (void)refusalsHeader;
                for (const auto& [mark, since] : server.roads().refused)
                {
                    const auto bar = mark.rfind('|');
                    const auto* till = society.account(mark.substr(0, bar));
                    refusals << now << ",\"" << mark.substr(0, bar) << "\",\"" << mark.substr(bar + 1) << "\"," << since << ","
                             << (till ? till->cash : 0) << "\n";
                }
            }
            // The employers marked short of pay in the day (doc 64, Phase 13, round 7): what fell short, how often,
            // what it owed, and its purse then (the least) and now.
            {
                static std::ofstream shorts(out / "shortpay.csv");
                static bool shortsHeader = (shorts << "day,employer,what,times,owed,cash_least,cash_end\n", true);
                (void)shortsHeader;
                std::map<std::pair<std::string, std::string>, std::tuple<int, std::int64_t, std::int64_t>> marked;
                for (const auto& n : society.takeShorts())
                {
                    auto& [times, owed, least] = marked.try_emplace({n.key, n.what}, 0, 0, n.cash).first->second;
                    ++times, owed += n.owed, least = std::min(least, n.cash);
                }
                for (const auto& [key, m] : marked)
                {
                    const auto* purse = society.account(key.first.substr(0, key.first.find('|')));
                    shorts << day << "," << csv(key.first) << "," << key.second << "," << std::get<0>(m) << "," << std::get<1>(m) << ","
                           << std::get<2>(m) << "," << (purse ? purse->cash : 0) << "\n";
                }
                shorts.flush();
            }
            const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - wallBegin).count();
            std::cout << "day " << now << " (" << int(wall / 60) << " min): supply " << supply << (conserved ? "" : " NOT CONSERVED")
                      << ", median purse " << median << "p, gini " << std::fixed << std::setprecision(3) << gini
                      << std::defaultfloat << ", grown short " << short_ << ", broke " << broke << ", starving " << starving
                      << ", wages " << wageTotal << "p, maker-hours short " << makerShort << std::fixed << std::setprecision(1)
                      << ", going " << society.goingRate("") << "p";
            // (The towns with a rate of their own: the others pay the realm's.)
            for (const auto& [where, rate] : society.goingRates())
                if (!where.empty())
                    std::cout << ", " << where << " " << rate << "p";
            // (Three figures after, as the gini left them before round 7: else the run's last line read "2e+01 min".)
            std::cout << std::defaultfloat << std::setprecision(3) << "\n" << std::flush;
            day = now;
            if (!stopped.empty())
            {
                ticksRun = i + 1;
                std::cout << "stopped on day " << now << ": the " << stopped << " sentinel fired (--stop-on-sentinel)\n";
                break;
            }
        }
        // Between ticks, as a player's command comes in; after the midnight's record, so what they do at midnight is the
        // new day's in days.jsonl as in players.csv and flows.csv.
        if (players)
            players->step();
    }
    absorb();
    writeDay(day);
    writePlayers();
    for (const auto& b : server.society().takeBriefs())
        orchestraOut << orchestra::briefText(b) << '\n';
    snapshot(int(std::floor(server.calendarDays())), "end");
    // The last part-week's accounts and goods (unless the run ended on a week's record).
    if (const int last = int(std::floor(server.calendarDays())); last != lastWeek)
        writeWeek(last);
    // As world_check's: where everyone ended up and what they were doing, and the money.
    std::uint64_t digest = 1469598103934665603ULL;
    const auto mix = [&](const void* data, std::size_t size) {
        for (std::size_t b = 0; b < size; ++b)
            digest = (digest ^ static_cast<const unsigned char*>(data)[b]) * 1099511628211ULL;
    };
    const auto mixText = [&](const std::string& text) { mix(text.data(), text.size() + 1); };
    for (const auto& [id, e] : server.entities())
    {
        mixText(id);
        mixText(e.cellId);
        mix(&e.position.x, sizeof e.position.x);
        mix(&e.position.y, sizeof e.position.y);
        mixText(e.activity);
        if (const auto* a = society.account(id))
            mix(&a->cash, sizeof a->cash);
    }
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - wallBegin).count();
    const double daysRun = days * double(ticksRun) / double(std::max(1L, ticks));
    std::cout << "done: " << ticksRun << " ticks, " << wall / 60 << " min (" << wall * 1000 / double(std::max(1L, ticksRun))
              << " ms a tick, " << wall / 60 / std::max(1e-9, daysRun) << " min a game day); digest "
              << std::hex << digest << std::dec << "\n";
    // Where the ticks went (ms a tick): the world's parts, and the schedules by stage.
    const auto& p = server.tickProfile();
    const auto per = [&](double ms) { return ms / double(std::max<std::int64_t>(1, ticksRun)); };
    std::cout << std::fixed << std::setprecision(4) << "ms a tick: streaming " << per(p.streaming.total) << ", schedules "
              << per(p.schedules.total) << ", movement " << per(p.movement.total) << ", separation " << per(p.separation.total)
              << ", views " << per(p.views.total) << "\nschedules: society " << per(p.stages[0]) << ", bonds " << per(p.stages[1])
              << ", roads " << per(p.stages[2]) << ", crime " << per(p.stages[3]) << ", errands " << per(p.stages[4])
              << ", streaming's wants " << per(p.stages[5]) << ", route planning " << per(p.stages[6]) << "\n"
              << std::defaultfloat;
    return 0;
}
