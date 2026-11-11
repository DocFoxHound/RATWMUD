// Careers: positions, skill, apprentices, estates and succession (Docs/Design/26-living-npcs.md, Phase 4).
#include "RatwSociety.h"
#include "RatwCalendar.h"
#include "RatwItems.h"

#include <algorithm>
#include <iterator>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <set>
#include <tuple>

namespace ratw
{
namespace
{
std::string surname(const std::string& name)
{
    const auto space = name.find_last_of(' ');
    return space == std::string::npos ? std::string() : name.substr(space + 1);
}
std::string skillKey(const std::string& resident, const std::string& position)
{
    return resident + "|" + position;
}
// skillKey, made in a buffer of the thread's own, for the lookups made every second (skill, practise).
const std::string& skillKeyNow(const std::string& resident, const std::string& position)
{
    thread_local std::string key;
    key.assign(resident).append(1, '|').append(position);
    return key;
}
// Placeholder names for those born here, until naming is designed.
const char* const FirstNames[] = {"Alder", "Briar", "Cinder", "Dusk", "Ember", "Fennel", "Gale", "Hazel", "Ivy", "Juniper",
                                  "Kestrel", "Linden", "Moss", "Nettle", "Oriel", "Pike", "Quill", "Rowan", "Sorrel",
                                  "Thorn", "Umber", "Vesper", "Willow", "Yarrow", "Wren", "Tansy", "Bramble", "Sedge"};
std::uint64_t mix(const std::string& a, std::int64_t b)
{
    return std::hash<std::string>{}(a) * 1099511628211ULL + std::uint64_t(b) * 2654435761ULL;
}
std::string lower(std::string s)
{
    for (auto& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
// How much a position is a step up: keeping a shop, then the watch, then paid work, then unpaid.
int rank(const Position* p)
{
    return !p ? -1 : p->role == "merchant" ? 3 : p->role == "guard" ? 2 : p->paid ? 1 : 0;
}
// A lot from 0 to 1, fixed by its words.
double lotOf(const std::string& words)
{
    return double(std::hash<std::string>{}(words) % 100000) / 100000;
}
// Masters take on an apprentice now and then, not the first day they could: about once a week.
bool thisWeek(const std::string& id, std::int64_t day)
{
    return (std::hash<std::string>{}(id) + std::uint64_t(day)) % 7 == 0;
}

// Jobs by class (Docs/Design/67-ridgemere-2-the-city.md, Progress, "Jobs by class"): the words of a post's title, after
// the city's own rules (tools/worldgen/ridgemere2_people.py, Placement.job_class: UPPER_WORDS, MIDDLE_WORDS, LOWER_WORDS,
// LEISURE), and the Country's work the unplaced NPCs were written with.
const char* const UpperWords[] = {"reigning at", "the queen,", "the king's daughter", "chancellor", "royal secretary",
                                  "the crown's treasurer", "collecting the crown's customs", "harbourmaster",
                                  "running the city office", "keeping the rolls", "hearing cases", "overseeing",
                                  "warden of the market hall", "pleading cases", "drafting the council's", "lending",
                                  "visiting the sick of the heights"};
const char* const MiddleWords[] = {"clerking", "copying", "tallying", "weighing", "couriers", "teaching", "herald", "usher",
                                   "keeping order in court", "keeping the washhouse", "keeping the bathhouse",
                                   "keeping the fell cellars", "keeping the watch's", "keeping the bonded warehouse",
                                   "keeping the long store", "keeping the new store", "keeping the iron store"};
const char* const LowerWords[] = {"serving", "in service to", "keeping the chambers", "stewarding", "cooking for the king",
                                  "lighting the lamps", "working at", "washing at", "driving for", "keeping the greatly stables",
                                  "unloading", "working the cranes", "carrying from", "portering", "crying wares", "gate",
                                  "garden", "groom", "stable",
                                  // The Country's work (the unplaced NPCs' trades: its fields, woods, waters and mines).
                                  "works the fields", "logging", "fishes the", "milks the herd", "keeps rabbits", "keeps pigs",
                                  "keeps sheep", "raises sheep", "digs ore", "digs clay", "cutting stone", "cuts wood",
                                  "tends the orchard", "tends the vines", "traps", "farms the", "a servant"};
const char* const LeisureWords[] = {"at home in the heights", "reading in the gardens", "walking the gardens"};
// No work of their own (a pup's, an elder's, the house's): such a wolf is of its family's class.
const char* const IdleWords[] = {"lessons", "plays ", "playing", "sits ", "keeps the house", "watches the world", "minds the shop",
                                 "minding the shop", "between voyages"};
// The city's independent larger businesses (by their buildings), which their keeper owns: an upper post. A Great House's
// works are run for it: a middle one, as any other shop, stall or counter.
const char* const LargeBusinesses[] = {"carters_yard", "rainwater_brewery", "rag_mill", "wharf_tannery", "grey_gull_inn",
                                       "sailors_rest"};
template <std::size_t N>
bool anyOf(const std::string& text, const char* const (&words)[N])
{
    for (const char* word : words)
        if (text.find(word) != std::string::npos)
            return true;
    return false;
}
// How far a wolf's class is from a post's: 0 the same (or the Watch's and the clergy's, any), 1 the next (upper and
// middle, middle and lower), 2 apart.
int classApart(const std::string& post, const std::string& wolf)
{
    const auto rank = [](const std::string& c) { return c == "upper" ? 2 : c == "middle" ? 1 : 0; };
    return post == "any" ? 0 : std::abs(rank(post) - rank(wolf));
}
// A wolf's class from its label as written (an unplaced NPC's): "" for no work of its own.
std::string labelClass(const ResidentSpec& r)
{
    const auto t = lower(r.workLabel);
    if (t.empty() || t == "-" || anyOf(t, IdleWords))
        return {};
    if (anyOf(t, LeisureWords) || Society::houseHead(r.workLabel))
        return "upper";
    if (r.role == "guard" || Society::clergy(r.workLabel))
        return "middle";                            // (Their own cases: the Watch's rank goes by service.)
    if (anyOf(t, UpperWords))
        return "upper";
    if (anyOf(t, MiddleWords))
        return "middle";
    if (anyOf(t, LowerWords))
        return "lower";
    return "middle";                                // A shop, a stall, an inn of a town, a trade.
}
// The words of a title that can say a wolf's trade suits it ("fishmonger", "butcher", "brewing"): five letters or more.
std::vector<std::string> tradeWords(const std::string& title)
{
    static const std::set<std::string> Common{"keeping", "working", "helping", "serving", "works", "market", "square", "house",
                                              "street", "lane", "the", "with", "from"};
    std::vector<std::string> words;
    std::string word;
    for (const char ch : lower(title) + " ")
        if (std::isalpha(static_cast<unsigned char>(ch)))
            word += ch;
        else
        {
            if (word.size() >= 5 && !Common.count(word))
                words.push_back(word.substr(0, word.size() - (word.back() == 's' ? 1 : 0)));
            word.clear();
        }
    return words;
}
} // namespace

std::string birthSex(const std::string& couple, std::int64_t day)
{
    // (A high bit of the mix: its lowest bit follows the day's alone.)
    return (mix(couple + "|sex", day) >> 37) & 1 ? "male" : "female";
}

const std::vector<std::string>& facilityPrefixes()
{
    // The accounts a society may open (openAccount). The old economy's (fund:, bank:, outside:, farm:, crew:, estate:,
    // house:) are not among them: no code opens one now, and Society::classify calls them Dead, so a save's are dropped
    // at restore (doc 79, 14). caravan: is dead too but still opened by the roads until Phase 4's account-free
    // departures, and dropped at a restart. (chapter: doc 32's treasuries; home: doc 36; town: doc 35's Town Works,
    // watch, church; letter: doc 55's enclosures; let:, stall:, table: and fest: doc 54's chests, stalls, stakes and
    // festival pots; project: doc 57's town projects; stores: the towns' treasuries; till: a business's.)
    static const std::vector<std::string> prefixes{"stores:", "caravan:", "bandits:", "contract:", "ground:", "chapter:", "home:", "town:",
                                                   "till:", "letter:", "let:", "stall:", "table:", "fest:", "project:"};
    return prefixes;
}

bool facilityAccount(const std::string& id)
{
    for (const auto& prefix : facilityPrefixes())
        if (id.rfind(prefix, 0) == 0)
            return id.size() <= 80 && id.size() > prefix.size();
    return false;
}

bool Society::bequeath(const std::string& from, const std::string& to, const std::string& item, int quantity,
                       std::int64_t coins)
{
    return shift(from, to, item, quantity, coins, "inheritance");
}

bool Society::openAccount(const std::string& id)
{
    if (!facilityAccount(id) || state_.accounts.count(id) || state_.accounts.size() >= MaxAccounts)
        return false;
    state_.accounts[id] = {};
    if (id.rfind("till:", 0) == 0)
        tills_[id.substr(5)] = id;                   // (A business's till, by its position: tillOf.)
    return true;
}

bool Society::closeAccount(const std::string& id, const std::string& heir)
{
    const auto found = state_.accounts.find(id);
    if (!facilityAccount(id) || found == state_.accounts.end() || found->second.cash != 0)
        return false;
    for (const auto& [item, count] : found->second.stock)
        if (count != 0)
            return false;
    // Goods it holds in quarantine (doc 79, 14) go to its heir; with no heir to take them, it stays open, keeping them.
    if (!handOverQuarantine(id, heir))
        return false;
    state_.accounts.erase(found);
    if (id.rfind("till:", 0) == 0)
        tills_.erase(id.substr(5));
    ++rosterRevision_;
    return true;
}

int Society::consume(const std::string& account, const std::string& item, int quantity, const std::string& kind)
{
    const auto found = state_.accounts.find(account);
    if (found == state_.accounts.end() || !itemValid(item) || quantity <= 0)
        return 0;
    const int used = std::min(quantity, stock(found->second, item));
    if (used <= 0)
        return 0;
    // (The last of a good gone, its entry goes too, as in a sale: an account holds only what it has, and every look for food
    // scans fewer. The old three keep their place.)
    if ((found->second.stock[item] -= used) == 0 && item != "herbs" && item != "meal" && item != "sword")
        found->second.stock.erase(item);
    record(kind, account, "consumed", item, std::min(used, 99), 0);
    return used;
}

bool Society::shift(const std::string& from, const std::string& to, const std::string& item, int quantity,
                    std::int64_t coins, const std::string& kind)
{
    // Existing money and goods only; what the receiver can't hold stays where it was. (A staged payer paying a player or
    // escrow is topped up first: doc 79, 9.2.)
    if (coins > 0)
        topUp(from, to, coins);
    const auto a = state_.accounts.find(from), b = state_.accounts.find(to);
    if (a == state_.accounts.end() || b == state_.accounts.end() || from == to || coins < 0 || quantity < 0 ||
        a->second.cash < coins || (!item.empty() && stock(a->second, item) < quantity))
        return false;
    // Never more kinds of goods than a save may hold (restore refuses past MaxGoodsKinds: doc 55 found the gap).
    if (!item.empty() && quantity > 0 && !b->second.stock.count(item) && b->second.stock.size() >= MaxGoodsKinds)
        return false;
    coins = std::min(coins, MoneyCap - b->second.cash);
    if (!item.empty())
        quantity = std::min(quantity, StockCap - stock(b->second, item));
    if (coins <= 0 && quantity <= 0)
        return false;
    a->second.cash -= std::max<std::int64_t>(0, coins);
    b->second.cash += std::max<std::int64_t>(0, coins);
    if (!item.empty() && quantity > 0)
    {
        if ((a->second.stock[item] -= quantity) == 0 && item != "herbs" && item != "meal" && item != "sword")
            a->second.stock.erase(item);           // (As in consume.)
        b->second.stock[item] += quantity;
    }
    record(kind, from, to, item, std::min(quantity, 99), std::max<std::int64_t>(0, coins));
    return true;
}

const char* skillFamily(const std::string& title)
{
    // A first sorting of the world's job titles by keyword; a proper list of skills will replace it.
    static const std::vector<std::pair<const char*, std::vector<const char*>>> Families{
        {"watch", {"watch", "guard", "patrol", "gate", "wall", "captain", "sentry"}},
        {"trade", {"keeping", "shop", "stall", "inn", "tavern", "market", "trader", "selling", "pawn", "ledger", "merchant"}},
        {"craft", {"smith", "forge", "glass", "weav", "sew", "tann", "carv", "joiner", "shipwright", "rope", "cooper",
                   "potter", "brew", "bak", "cook", "kiln", "mason"}},
        {"labour", {"saw", "quarry", "log", "haul", "unload", "dig", "rak", "burn", "fish", "farm", "herd", "gather",
                    "forag", "barge", "dock", "carry", "labour"}},
        {"service", {"serv", "wash", "clean", "steward", "house", "tend", "nurse", "heal", "scribe", "clerk", "tall"}},
        {"travel", {"road", "travel", "carter", "courier", "rider", "caravan", "scout"}}};
    const auto t = lower(title);
    for (const auto& [family, words] : Families)
        for (const char* word : words)
            if (t.find(word) != std::string::npos)
                return family;
    return "general";
}

int Society::retireAge(const std::string& resident)
{
    return RetireLeast + int(std::hash<std::string>{}(resident + "|retire") % (RetireMost - RetireLeast + 1));
}

double Society::tradeLevel(const Position& p) const
{
    return p.level;                                 // (A business's: its hardest craft's; help and other posts none.)
}

bool Society::important(const std::string& resident) const
{
    // In a master's place (a skilled trade, at its level), keeping its own shop, or the clergy.
    const auto* job = jobOf(resident);
    if (!job)
        return false;
    const double level = tradeLevel(*job);
    return (level >= TradeSkilled && skill(resident, job->id) >= level) || job->role == "merchant" || clergy(job->title);
}

double Society::familySkill(const std::string& resident, const std::string& family) const
{
    double best = 0;
    const auto prefix = resident + "|";
    for (auto it = state_.careers.skill.lower_bound(prefix); it != state_.careers.skill.end() && it->first.rfind(prefix, 0) == 0; ++it)
        if (const auto* p = position(it->first.substr(prefix.size())); p && family == skillFamily(p->title))
            best = std::max(best, it->second);
    return best;
}

bool Society::marry(const std::string& a, const std::string& b)
{
    auto& spouses = state_.careers.spouses;
    if (a == b || !state_.residents.count(a) || !state_.residents.count(b) || spouses.count(a) || spouses.count(b))
        return false;
    spouses[a] = b;
    spouses[b] = a;
    return true;
}
const std::string* Society::spouse(const std::string& resident) const
{
    const auto found = state_.careers.spouses.find(resident);
    return found == state_.careers.spouses.end() ? nullptr : &found->second;
}

std::vector<ResidentRequest> Society::takeRequests(std::size_t most)
{
    std::vector<ResidentRequest> out;
    if (most >= requests_.size())
    {
        out.swap(requests_);
        return out;
    }
    out.assign(std::make_move_iterator(requests_.begin()), std::make_move_iterator(requests_.begin() + std::ptrdiff_t(most)));
    requests_.erase(requests_.begin(), requests_.begin() + std::ptrdiff_t(most));
    return out;
}

CareerNote Society::welcome(const ResidentRequest& request, const std::string& id)
{
    auto& c = state_.careers;
    if (!state_.residents.count(id))
        return {"refused", id, {}, "No such resident to welcome."};
    if (request.kind == "birth")
    {
        c.parents[id] = request.parents;
        // A child's first pennies (starting money, 2026-10-05), from the better-off parent: given, never made.
        const std::string* giver = nullptr;
        for (const auto& parent : request.parents)
            if (const auto* purse = account(parent); purse && (!giver || purse->cash > account(*giver)->cash))
                giver = &parent;
        if (giver)
            shift(*giver, id, "", 0, std::min<std::int64_t>(ChildPurse, account(*giver)->cash),
                  "a child's first pennies");
        forgetCareers();
        stagedJoined(id, request.parents);          // (Doc 79, 6.5: the household's band at once.)
        return {"birth", id, request.parents.empty() ? std::string() : request.parents[0],
                request.parents.size() > 1 ? "child of " + request.parents[0] + " and " + request.parents[1] : std::string()};
    }
    // Placed by the farms' rule (doc 71, Phase C): its post is its own (founded with it: addResident), or none (family).
    if (request.kind == "farm")
    {
        stagedJoined(id, {});                       // (Doc 79, 6.5: placed in a post, its household's band at once.)
        return {"newcomer", id, {}, request.workLabel == "-" ? "came in from the Country with their family"
                                                               : "came in from the Country to be " + request.workLabel};
    }
    // An arrival from the Country takes up the post it came for. (Its town's talk of it as a newcomer: "is new in town".)
    auto found = c.positions.find(request.positionId);
    const auto* p = position(request.positionId);
    if (!p || found == c.positions.end() || !found->second.holder.empty())
        return {"newcomer", id, {}, "came in from the Country, but the post was taken; looking for work"};
    for (auto& [otherId, other] : c.positions)
        if (other.apprentice == id)
            other.apprentice.clear();
    found->second.holder = id;
    found->second.vacantSince = -1;
    auto& known = c.skill[skillKey(id, p->id)];
    known = std::max({known, 5.0, request.skill});     // What it knows of the work from the Country (arrivalFor).
    forgetCareers();
    stagedJoined(id, {});                           // (Doc 79, 6.5: an arrival placed in a post joins its household's band.)
    return {"newcomer", id, found->second.lastHolder, "came in from the Country to take up " + p->title};
}

void Society::birthFailed(const ResidentRequest& request)
{
    if (request.kind == "birth" && !request.inPlaceOf.empty())
        state_.careers.birthsOwed[request.inPlaceOf] = double(std::max<std::int64_t>(0, state_.careers.day));
}

bool Society::arrive(const ResidentRequest& request)
{
    const auto away = std::find_if(authored_.unplaced.begin(), authored_.unplaced.end(),
                                   [&](const ResidentSpec& r) { return r.id == request.templateId; });
    if (roster_ != Roster::Authored || (request.kind != "arrival" && request.kind != "farm") || away == authored_.unplaced.end() ||
        request.home.cell.empty())
        return false;
    if (request.kind == "farm")
    {
        // Placed by the farms' rule (doc 71, Phase C): at its home, with a post of its own at its work (founded with it,
        // as an authored resident's is), or none (a keeper's family). Its purse moves from its farm crew's founding purse
        // (or, a market's, the Country's), as far as that holds it: nothing is made.
        ResidentSpec r = *away;
        r.home = r.evening = request.home;
        const bool post = request.workLabel != "-" && !request.work.cell.empty();
        r.work = post ? request.work : request.home;
        r.workLabel = post ? request.workLabel : "-";
        r.role = post && request.role == "merchant" ? "merchant" : "civilian";
        r.paid = post;
        if (post)
        {
            r.startHour = request.startHour;
            r.endHour = request.endHour;
        }
        r.route.clear();
        r.wander.clear();
        r.serve = r.role == "merchant" ? request.serve : Spot{};
        if (!addResident(r, 0))                     // (Its authored purse stays its record's: a shop's float, below.)
            return false;
        authored_.unplaced.erase(std::find_if(authored_.unplaced.begin(), authored_.unplaced.end(),
                                              [&](const ResidentSpec& u) { return u.id == r.id; }));
        stagedSet(r.id, std::min<std::int64_t>(request.purse, ArrivalPurse));   // (A plain amount: doc 79, 6.5.)
        if (r.role == "merchant")
        {
            foundShops();                           // (A community's shop, doc 71, Phase C: its till opened.)
            indexTills();
        }
        countFarmPosts();
        return true;
    }
    // Placed without work (doc 66, 2): it lives at home, holds the post it came for (welcome), and keeps its talk, story
    // and families; its own post as written stays its record's (a live world's row keeps its label).
    ResidentSpec r = *away;
    r.home = r.work = r.evening = request.home;
    r.workLabel = "-";
    r.role = "civilian";
    r.paid = false;
    r.route.clear();
    r.wander.clear();
    r.serve = {};
    if (!addResident(r, 0))
        return false;
    authored_.unplaced.erase(std::find_if(authored_.unplaced.begin(), authored_.unplaced.end(),
                                          [&](const ResidentSpec& u) { return u.id == r.id; }));
    // Its purse a plain amount (doc 79, 6.5), the night's set drawing it to its household's.
    stagedSet(r.id, std::min<std::int64_t>(request.purse, ArrivalPurse));
    return true;
}

const std::vector<items::WageRule>& Society::postRules()
{
    // The posts' rules (city_loop.json's `wages` as they stood at doc 79, the file gone since Phase 3; the order matters:
    // the first rule with a word in a title wins): what kind of post a title is (postKind) and its class (postClass).
    static const std::vector<items::WageRule> rules = {
        {"the Crown's and the offices' heads", "treasury", "keeper",
         {"the queen", "the king's daughter", "running the city office", "reigning", "chancellor", "royal secretary",
          "the crown's treasurer", "hearing cases", "keeping the rolls", "collecting the crown's customs",
          "warden of the market", "abbot", "keeping the washhouse", "keeping the bathhouse", "commanding the watch"}},
        {"the Houses' heads", "estates", "keeper",
         {"lending", "overseeing the grayrock wharf", "harbourmaster", "overseeing the counting house"}},
        {"the sea trade", "estates", "",
         {"unloading ships", "working the cranes", "carrying from", "tallying in the", "keeping the iron store",
          "keeping the long store", "keeping the new store", "keeping the wharf's books", "wharf office"}},
        {"the Houses' people", "estates", "",
         {"ruling house", "keeping greatly house", "keeping grayrock hall", "keeping timberland court", "stewarding",
          "serving at greatly house", "serving in the hall", "in service to the", "keeping the fell",
          "running the house's", "on house business", "keeping the greatly stables", "counting house",
          "at home in the heights", "reading in the gardens", "walking the gardens", "visiting the sick"}},
        {"public posts", "treasury", "",
         {"reigning", "the crown's", "royal secretary", "herald", "usher", "the king's household",
          "keeping the chambers", "chancellor", "the council", "hearing cases", "pleading cases",
          "keeping order in court", "magistrate", "clerking", "copying", "keeping the rolls", "records office",
          "customs", "couriers", "lamps", "warden of the market", "the watch", "minding the shop door", "portering",
          "crying wares", "washhouse", "bathhouse", "teaching", "abbey", "abbot", "cathedral", "chapel", "priest",
          "deacon", "sexton", "choir"}},
    };
    return rules;
}

Society::PostKind Society::postKind(const Position& p, ClassPass* pass) const
{
    PostKind k;
    if (p.role == "merchant")
    {
        k.kind = "keeper";
        return k;
    }
    // A farm's post (doc 71, Phase C) or a works' (doc 72): the kept farm list's (RatwFarms.cpp).
    if (const int farm = farmOfPost(p, &k.farmKeeper); farm >= 0)
    {
        k.kind = "farm";
        k.farm = farm;
        return k;
    }
    const auto title = lower(p.title);
    const auto& rules = postRules();
    if (p.role == "guard")
    {
        // The watch, and its head (a treasury rule that names a wage: "commanding the watch").
        k.kind = "watch";
        for (std::size_t r = 0; r < rules.size() && k.rule < 0; ++r)
            if (rules[r].payer == "treasury" && !rules[r].wage.empty())
                for (const auto& word : rules[r].words)
                    if (title.find(word) != std::string::npos)
                    {
                        k.rule = int(r);
                        break;
                    }
        return k;
    }
    // A hand: a post whose work room holds exactly one shop.
    int shops = 0;
    if (pass)
    {
        if (!pass->shopsBuilt)
        {
            for (const auto& q : positions_)
                if (q.role == "merchant")
                {
                    auto& [n, last] = pass->shops[q.work.cell];
                    ++n, last = &q;
                }
            pass->shopsBuilt = true;
        }
        if (const auto found = pass->shops.find(p.work.cell); found != pass->shops.end())
            shops = found->second.first, k.shop = found->second.second;
    }
    else
        for (const auto& q : positions_)
            if (q.role == "merchant" && q.work.cell == p.work.cell)
                ++shops, k.shop = &q;
    if (shops == 1)
    {
        k.kind = "hand";
        return k;
    }
    k.shop = nullptr;
    for (std::size_t r = 0; r < rules.size(); ++r)
        for (const auto& word : rules[r].words)
            if (title.find(word) != std::string::npos)
            {
                k.kind = rules[r].payer == "treasury" ? "public" : "estate";
                k.rule = int(r);
                return k;
            }
    return k;
}

std::string Society::postClass(const Position& p, ClassPass* pass) const
{
    const auto kind = postKind(p, pass);
    if (kind.kind.empty())
        return {};                                  // No post that pays: lessons, play, leisure.
    if (p.role == "guard" || clergy(p.title))
        return "any";
    if (kind.kind == "farm")
        return kind.farmKeeper ? "middle" : "lower";   // (A farm's keeper and its hands: doc 71, Phase C.)
    const auto t = lower(p.title);
    if (houseHead(p.title))
        return "upper";
    if (kind.kind == "keeper")
        return anyOf(p.work.cell, LargeBusinesses) ? "upper" : "middle";
    if (kind.kind == "hand")
    {
        // Helping in the family's own shop, living over it: middle; a hand in someone else's business: lower.
        const auto* founder = spec(p.founder);
        return founder && !p.work.cell.empty() && founder->home.cell.rfind(p.work.cell, 0) == 0 ? "middle" : "lower";
    }
    // A House's own affairs and the offices' heads (the post rules that pay a keeper's wage), and a House's family at
    // home in the Heights (kept by its estates), are upper; then the words.
    for (const auto& rule : postRules())
        if (rule.wage == "keeper")
            for (const auto& word : rule.words)
                if (t.find(word) != std::string::npos)
                    return "upper";
    if (anyOf(t, LeisureWords) || anyOf(t, UpperWords))
        return "upper";
    if (anyOf(t, MiddleWords))
        return "middle";
    if (anyOf(t, LowerWords))
        return "lower";
    return "middle";
}

std::string Society::classOf(const std::string& id, int depth, ClassPass* pass) const
{
    // "" while unknown (the public classOf: middle). In a pass, each (id, depth) once.
    if (pass)
    {
        if (const auto found = pass->classes.find({id, depth}); found != pass->classes.end())
            return found->second;
        auto cls = workOutClass(id, depth, pass);
        pass->classes.emplace(std::pair{id, depth}, cls);
        return cls;
    }
    return workOutClass(id, depth, nullptr);
}

std::string Society::workOutClass(const std::string& id, int depth, ClassPass* pass) const
{
    const auto* r = spec(id);
    const auto* away = r ? nullptr : unplaced(id);
    if (!r && !away)
        return {};
    // Its own work as written: the post a founder was written with (the class pass matched them: doc 67), or an unplaced
    // NPC's label.
    if (r)
    {
        if (const auto* own = position("job:" + id))
        {
            const auto cls = postClass(*own, pass);
            if (cls == "any")
                return "middle";
            if (!cls.empty())
                return cls;
            if (anyOf(lower(own->title), LeisureWords))
                return "upper";
        }
    }
    else if (auto cls = labelClass(*away); !cls.empty())
        return cls;
    if (depth >= 3)
        return {};
    // Else its family's: a spouse, then a parent (as made in play, and as written).
    std::vector<std::string> kin;
    if (const auto s = state_.careers.spouses.find(id); s != state_.careers.spouses.end())
        kin.push_back(s->second);
    if (const auto ps = state_.careers.parents.find(id); ps != state_.careers.parents.end())
        kin.insert(kin.end(), ps->second.begin(), ps->second.end());
    const auto written = [&](const auto& k) {
        if (k.kind == "spouse" && (k.a == id || k.b == id))
            kin.insert(kin.begin(), k.a == id ? k.b : k.a);
        else if (k.kind != "spouse" && k.b == id)
            kin.push_back(k.a);
    };
    if (pass)
    {
        // (The same records in the same order, from the pass's index of the written kin by wolf.)
        if (!pass->kinBuilt)
        {
            for (const auto& k : authored_.kin)
                if (k.kind == "spouse")
                {
                    pass->kin[k.a].push_back(&k);
                    if (k.b != k.a)
                        pass->kin[k.b].push_back(&k);
                }
                else
                    pass->kin[k.b].push_back(&k);
            pass->kinBuilt = true;
        }
        if (const auto found = pass->kin.find(id); found != pass->kin.end())
            for (const auto* k : found->second)
                written(*k);
    }
    else
        for (const auto& k : authored_.kin)
            written(k);
    for (const auto& other : kin)
        if (other != id)
            if (auto cls = classOf(other, depth + 1, pass); !cls.empty())
                return cls;
    // Else its household's: most of those it lives with.
    if (r && depth == 0)
    {
        const auto* life = resident(id);
        std::map<std::string, int> count;
        if (pass && life && !life->homeCell.empty())
        {
            // (The same residents in the same order, by id, from the pass's index of homes.)
            if (!pass->homesBuilt)
            {
                for (const auto& [other, theirs] : state_.residents)
                    if (!theirs.homeCell.empty())
                        pass->homes[theirs.homeCell].push_back(&other);
                pass->homesBuilt = true;
            }
            if (const auto home = pass->homes.find(life->homeCell); home != pass->homes.end())
                for (const auto* other : home->second)
                    if (*other != id)
                        if (auto cls = classOf(*other, 2, pass); !cls.empty())
                            ++count[cls];
        }
        else if (!pass)
            for (const auto& [other, theirs] : state_.residents)
                if (other != id && life && !life->homeCell.empty() && theirs.homeCell == life->homeCell)
                    if (auto cls = classOf(other, 2); !cls.empty())
                        ++count[cls];
        std::string most;
        for (const auto& [cls, n] : count)
            if (most.empty() || n > count[most])
                most = cls;
        return most;
    }
    return {};
}

void Society::buildPositions()
{
    ++rosterRevision_;
    houses_.clear();
    housesKnown_ = housesTold_ = false;
    positions_.clear();
    positionIndex_.clear();
    for (const auto& r : authored_.residents)
    {
        if (r.workLabel == "-")
            continue;                                  // Born here or come for another's post: no job of their own.
        Position p;
        p.id = "job:" + r.id;
        p.founder = r.id;
        p.title = r.workLabel;
        p.role = r.role;
        p.work = r.work;
        p.serve = r.serve;
        p.startHour = r.startHour;
        p.endHour = r.endHour;
        p.route = r.route;
        p.paid = r.paid;
        // (A business's trade: the skill its hardest craft needs. Doc 64, Phase 8.)
        if (p.role == "merchant" || items::producerFor(p.title))
            if (const auto* business = items::businessFor(p.title))
                for (const auto* k : items::craftsFor(business->id))
                    p.level = std::max(p.level, double(k->skill));
        positionIndex_[p.id] = positions_.size();
        positions_.push_back(std::move(p));
    }
    forgetCareers();
}

void Society::defaultCareers()
{
    // Every founder holds the job they were written with, about as skilled as its trade needs and its years suggest
    // (doc 64, Phase 8): about half of a skilled trade's founders at its level, the rest short of it and working toward
    // it; a plain post's a little general experience.
    state_.careers = {};
    for (const auto& p : positions_)
    {
        state_.careers.positions[p.id].holder = p.founder;
        const auto* r = spec(p.founder);
        const double age = r ? r->age : 30, level = tradeLevel(p), u = lotOf(p.id + "|skill");
        state_.careers.skill[skillKey(p.founder, p.id)] =
            level <= 0 ? std::clamp(10 + (age - 16) * .8 + 10 * (u - .5), 5., 45.)
                       : std::clamp(level - 8 + (age - 25) * .4 + 12 * u, 5., SkillTop);
    }
    forgetCareers();
}

void Society::reconcileCareers()
{
    // A saved career for a position or resident the world no longer has is let go; a new position is its founder's.
    auto& c = state_.careers;
    for (auto it = c.positions.begin(); it != c.positions.end();)
        it = positionIndex_.count(it->first) ? std::next(it) : c.positions.erase(it);
    const auto known = [&](const std::string& id) { return id.empty() || state_.residents.count(id) || playerAccountId(id); };
    for (auto& [id, ps] : c.positions)
    {
        if (!known(ps.holder))
            ps.holder.clear();
        if (!known(ps.apprentice))
            ps.apprentice.clear();
        if (!known(ps.lastHolder))
            ps.lastHolder.clear();
        if (ps.holder.empty() && ps.vacantSince < 0)
            ps.vacantSince = 0;
    }
    for (const auto& p : positions_)
        if (!c.positions.count(p.id))
        {
            c.positions[p.id].holder = p.founder;
            const auto* r = spec(p.founder);
            c.skill.emplace(skillKey(p.founder, p.id), std::clamp(30.0 + (r ? r->age : 20), 40.0, 90.0));
        }
    for (auto it = c.skill.begin(); it != c.skill.end();)
        it = std::isfinite(it->second) && it->second >= 0 && it->second <= 100 ? std::next(it) : c.skill.erase(it);
    for (auto it = c.mourning.begin(); it != c.mourning.end();)
        it = state_.residents.count(it->first) && std::isfinite(it->second.until) ? std::next(it) : c.mourning.erase(it);
    for (auto it = c.estates.begin(); it != c.estates.end();)
        it = state_.accounts.count(it->first) && std::isfinite(it->second) ? std::next(it) : c.estates.erase(it);
    forgetCareers();
}

void Society::indexCareers() const
{
    if (careersIndexed_)
        return;
    ++rosterRevision_;
    heldBy_.clear();
    learning_.clear();
    for (const auto& [id, ps] : state_.careers.positions)
    {
        if (!ps.holder.empty())
            heldBy_[ps.holder] = id;
        if (!ps.apprentice.empty())
            learning_[ps.apprentice] = id;
    }
    careersIndexed_ = true;
}

const std::vector<Position>& Society::positions() const
{
    return positions_;
}
const Position* Society::position(const std::string& id) const
{
    const auto found = positionIndex_.find(id);
    return found == positionIndex_.end() ? nullptr : &positions_[found->second];
}
const Position* Society::jobOf(const std::string& resident) const
{
    indexCareers();
    const auto found = heldBy_.find(resident);
    return found == heldBy_.end() ? nullptr : position(found->second);
}
const Position* Society::apprenticedTo(const std::string& resident) const
{
    indexCareers();
    const auto found = learning_.find(resident);
    return found == learning_.end() ? nullptr : position(found->second);
}
double Society::skill(const std::string& resident, const std::string& positionId) const
{
    const auto found = state_.careers.skill.find(skillKeyNow(resident, positionId));
    return found == state_.careers.skill.end() ? 0 : found->second;
}
double* Society::skillSlot(const std::string& resident, const std::string& positionId, bool make)
{
    if (make)
        return &state_.careers.skill[skillKeyNow(resident, positionId)];
    const auto found = state_.careers.skill.find(skillKeyNow(resident, positionId));
    return found == state_.careers.skill.end() ? nullptr : &found->second;
}
double Society::practise(const std::string& resident, const std::string& positionId, double rate)
{
    auto& s = state_.careers.skill[skillKeyNow(resident, positionId)];
    s = std::min(100.0, s + rate * (1 - s / 100));
    return s;
}
bool Society::household(const std::string& a, const std::string& b) const
{
    const auto* x = resident(a);
    const auto* y = resident(b);
    return a != b && x && y && !x->homeCell.empty() && x->homeCell == y->homeCell;
}
bool Society::family(const std::string& a, const std::string& b) const
{
    if (a == b)
        return false;
    const auto& c = state_.careers;
    if (const auto s = c.spouses.find(a); s != c.spouses.end() && s->second == b)
        return true;
    const auto childOf = [&](const std::string& child, const std::string& parent) {
        const auto p = c.parents.find(child);
        return p != c.parents.end() && std::find(p->second.begin(), p->second.end(), parent) != p->second.end();
    };
    if (childOf(a, b) || childOf(b, a))
        return true;
    const auto* x = spec(a);
    const auto* y = spec(b);
    return x && y && household(a, b) && !surname(x->name).empty() && surname(x->name) == surname(y->name);
}
const Mourning* Society::mourning(const std::string& resident) const
{
    const auto found = state_.careers.mourning.find(resident);
    return found == state_.careers.mourning.end() ? nullptr : &found->second;
}
void Society::mourn(const std::string& resident, const std::string& whom, double until)
{
    if (!state_.residents.count(resident) || resident == whom || !std::isfinite(until))
        return;
    auto& m = state_.careers.mourning[resident];
    if (m.until < until)
        m = {until, whom};
}

std::vector<CareerNote> Society::died(const std::string& resident, double day)
{
    std::vector<CareerNote> notes;
    auto& c = state_.careers;
    for (auto& [id, ps] : c.positions)
    {
        if (ps.holder == resident)
        {
            ps.holder.clear();
            ps.lastHolder = resident;
            ps.vacantSince = day;
            notes.push_back({"vacancy", resident, {}, position(id) ? position(id)->title : id});
        }
        if (ps.apprentice == resident)
            ps.apprentice.clear();
    }
    if (state_.accounts.count(resident) && !playerAccountId(resident))
        c.estates[resident] = day;
    // Births follow deaths (the user, 2026-11-01): a resident's death owes the world a child, due 1 to BirthDelayMost
    // days after (a lot of the dead's). A player's never does.
    if (state_.residents.count(resident) && !playerAccountId(resident) && !dead_.count(resident))
        c.birthsOwed[resident] = std::floor(day) + 1 + double(mix(resident + "|born", std::int64_t(std::floor(day))) % BirthDelayMost);
    dead_.insert(resident);
    forgetCareers();
    return notes;
}

std::vector<CareerNote> Society::revived(const std::string& resident)
{
    std::vector<CareerNote> notes;
    auto& c = state_.careers;
    c.estates.erase(resident);
    c.birthsOwed.erase(resident);                   // (Brought back before their child was born: none is owed.)
    dead_.erase(resident);
    if (!jobOf(resident))
        for (auto& [id, ps] : c.positions)
            if (ps.holder.empty() && ps.lastHolder == resident)
            {
                ps.holder = resident;
                ps.vacantSince = -1;
                notes.push_back({"returned to work", resident, {}, position(id) ? position(id)->title : id});
                break;
            }
    forgetCareers();
    return notes;
}

void Society::splitHouseholds(std::int64_t day, const CareerWorld& world, std::vector<CareerNote>& notes)
{
    // A crowded household (more than HouseholdMost living there) sends a young grown member (18 to 32) to a home of its
    // own, weekly: a free home in its town, one written for someone and now empty (doc 64, Phase 8; the user, 2026-10-15:
    // households split only into free homes). With none free it stays together.
    if (day % 7 != 3)
        return;
    std::map<std::string, std::vector<std::string>> homes;
    std::set<std::string> taken;
    for (const auto& [id, life] : state_.residents)
        if (world.alive(id) && !life.homeCell.empty())
        {
            homes[life.homeCell].push_back(id);
            taken.insert(life.homeCell);
            if (!life.relocationCell.empty())
                taken.insert(life.relocationCell);  // (One already on its way there.)
        }
    const auto townOf = [&](const std::string& cell) { return day_.communityOf ? day_.communityOf(cell) : std::string(); };
    std::map<std::string, std::vector<Spot>> free;
    for (const auto& r : authored_.residents)
        if (!r.home.cell.empty() && !communalHome(r.home.cell) && taken.insert(r.home.cell).second)
            free[townOf(r.home.cell)].push_back(r.home);
    for (const auto& [home, members] : homes)
    {
        if (int(members.size()) <= HouseholdMost || communalHome(home))
            continue;
        auto& open = free[townOf(home)];
        if (open.empty())
            continue;
        std::string leaving;
        double best = -1;
        for (const auto& id : members)
            if (const int age = world.age(id); age >= 18 && age <= 32 && !state_.careers.spouses.count(id))
                if (const double l = lotOf(id + "|leaves|" + std::to_string(day)); l > best)
                    best = l, leaving = id;
        if (leaving.empty())
            continue;
        const auto to = open.back();
        if (moveHome(leaving, to.cell, to.x, to.y))
        {
            open.pop_back();
            notes.push_back({"left home", leaving, {}, "a home of its own at " + to.cell});
        }
    }
}

std::vector<CareerNote> Society::tendCareers(double now, const CareerWorld& world)
{
    std::vector<CareerNote> notes;
    auto& c = state_.careers;
    const auto today = std::int64_t(std::floor(now));
    if (today == c.day || roster_ != Roster::Authored)
        return notes;
    c.day = today;
    const auto alive = [&](const std::string& id) { return !id.empty() && world.alive(id); };

    // Families as authored (doc 62, decision 8), kept as those made in play are: a couple married, a child's parents.
    // Each day, so a restored save and a family added later both take; nothing already there is changed.
    for (const auto& k : authored_.kin)
    {
        if (!state_.residents.count(k.a) || !state_.residents.count(k.b))
            continue;
        if (k.kind == "spouse")
        {
            if (!c.spouses.count(k.a) && !c.spouses.count(k.b))
                c.spouses[k.a] = k.b, c.spouses[k.b] = k.a;
        }
        else if (auto& ps = c.parents[k.b]; ps.size() < 2 && std::find(ps.begin(), ps.end(), k.a) == ps.end())
            ps.push_back(k.a);
    }
    // A founding couple's children count toward their three, and their last was born when the youngest was.
    if (!authored_.kin.empty())
        for (const auto& [a, b] : c.spouses)
        {
            if (!(a < b))
                continue;
            int children = 0, youngest = 1000;
            for (const auto& [child, ps] : c.parents)
                if (std::find(ps.begin(), ps.end(), a) != ps.end() && std::find(ps.begin(), ps.end(), b) != ps.end())
                    ++children, youngest = std::min(youngest, world.age(child));
            const auto key = a + "|" + b;
            if (children > (c.births.count(key) ? c.births.at(key) : 0))
                c.births[key] = children;
            if (children && !c.lastBirth.count(key))
                c.lastBirth[key] = now - double(std::max(0, youngest)) * calendar::DaysPerYear;
        }

    // Mourning passes.
    for (auto it = c.mourning.begin(); it != c.mourning.end();)
        it = it->second.until <= now ? c.mourning.erase(it) : std::next(it);

    // A day after a death the estate is settled: shared among the family, or to the town if there is none.
    for (auto it = c.estates.begin(); it != c.estates.end();)
    {
        const auto dead = it->first;
        if (alive(dead))
        {
            it = c.estates.erase(it);
            continue;
        }
        if (now - it->second < 1)
        {
            ++it;
            continue;
        }
        std::vector<std::string> heirs;
        for (const auto& [id, life] : state_.residents)
            if (family(dead, id) && alive(id))
                heirs.push_back(id);
        auto& estate = state_.accounts.at(dead);
        if (heirs.empty())
            heirs.push_back("treasury");
        // Shares of the estate as it stood, not of what is left after each heir.
        const auto shares = std::int64_t(heirs.size());
        // (The staged economy, doc 79, 6.5: between NPCs the goods alone are inherited; the coins are staged and drop.)
        if (classify(dead) == AccountKind::Staged)
            stagedSet(dead, 0);
        const std::int64_t cash = estate.cash;
        for (std::size_t h = 0; h < heirs.size(); ++h)
        {
            const std::int64_t coins = cash / shares + (h == 0 ? cash % shares : 0);
            if (coins > 0)
                bequeath(dead, heirs[h], "", 0, coins);
        }
        // And all its goods (doc 64, Phase 8: kin inherit everything), each shared the same way.
        std::vector<std::string> goods;
        for (const auto& [item, n] : estate.stock)
            if (n > 0)
                goods.push_back(item);
        for (const auto& item : goods)
        {
            const int total = stock(estate, item);      // Likewise taken once, before sharing.
            for (std::size_t h = 0; h < heirs.size() && total > 0; ++h)
            {
                const int quantity = total / int(heirs.size()) + (h == 0 ? total % int(heirs.size()) : 0);
                if (quantity > 0)
                    bequeath(dead, heirs[h], item, quantity, 0);
            }
        }
        notes.push_back({"estate settled", dead, heirs.size() == 1 ? heirs[0] : std::string(),
                         heirs[0] == "treasury" ? "no family: to the town" : "shared among " + std::to_string(heirs.size()) + " of the family"});
        it = c.estates.erase(it);
    }

    // Retirement (doc 64, Phase 8): a wolf in paid work (a trade, the watch) leaves its post at its own age once there's
    // someone to take it on (its apprentice; family or a local out of work, as skilled as the trade needs), and the post
    // falls to them; with nobody, it works on. The clergy, a great House's head and the unpaid (a storyteller, a beggar)
    // keep theirs: the realm's elders as they were written. (Each retiring wolf's successor is its own: one wolf free
    // to take a post doesn't let a dozen retire.)
    std::set<std::string> claimed;
    for (auto& p : positions_)
    {
        auto& ps = c.positions[p.id];
        if (ps.holder.empty() || !alive(ps.holder) || !p.paid || clergy(p.title) || houseHead(p.title) ||
            world.age(ps.holder) < retireAge(ps.holder))
            continue;
        const auto kind = skillFamily(p.title);
        const auto reach = reachOf(p);              // (A works' post: its crew's home ground, doc 72.)
        std::string successor = alive(ps.apprentice) && !claimed.count(ps.apprentice) ? ps.apprentice : std::string();
        for (auto other = state_.residents.begin(); successor.empty() && other != state_.residents.end(); ++other)
        {
            const auto& id = other->first;
            const int age = world.age(id);
            if (id == ps.holder || claimed.count(id) || !alive(id) || age < 16 || age >= retireAge(id) || apprenticedTo(id) ||
                std::max(skill(id, p.id), familySkill(id, kind)) < p.level)
                continue;
            if ((this->family(ps.holder, id) && rank(jobOf(id)) < rank(&p)) ||
                (!jobOf(id) && (!world.near || world.near(reach, other->second.homeCell))))
                successor = id;
        }
        if (successor.empty())
            continue;
        claimed.insert(successor);
        notes.push_back({"retired", ps.holder, {}, p.title});
        ps.lastHolder = ps.holder;
        ps.holder.clear();
        ps.vacantSince = now;
        forgetCareers();
    }

    // Empty positions are filled, not all at once (doc 64, Phase 8; the user's succession rule, 2026-10-14):
    //  - its apprentice after two days;
    //  - a masters' trade, after a day, a master idle or working below its level;
    //  - family after three, a local out of work after five: as skilled as the trade needs (its best at that kind of
    //    work);
    //  - else, after five, the best-trained wolf about who isn't in a master's place or an important post: it heads the
    //    business and works toward mastership;
    //  - a post that pays, after PostWaitDays: a wolf of the city with no post that pays, of its class (grown pups
    //    first), else an unplaced NPC of the world's own list, of its class, from the Country (the user, 2026-11-01).
    // Whoever moves up leaves its own post empty in turn.
    for (auto& p : positions_)
    {
        auto& ps = c.positions[p.id];
        if (!ps.holder.empty() || ps.vacantSince < 0)
            continue;
        const double waited = now - ps.vacantSince, level = tradeLevel(p);
        const auto reach = reachOf(p);              // (Where its holders live: a works' crew's home ground, doc 72.)
        const auto family = skillFamily(p.title);
        const auto trained = [&](const std::string& id) { return std::max(skill(id, p.id), familySkill(id, family)); };
        std::string chosen, how;
        if (waited >= 2 && alive(ps.apprentice))
            chosen = ps.apprentice, how = "the apprentice steps up";
        if (chosen.empty() && waited >= 1 && level >= TradeMasters)
        {
            double best = -1;
            for (const auto& [id, life] : state_.residents)
            {
                const auto* own = jobOf(id);
                if (!alive(id) || world.age(id) < 16 || apprenticedTo(id) || trained(id) < level || (own && tradeLevel(*own) >= level) ||
                    (own && own->role == "merchant") || (world.near && !world.near(reach, life.homeCell)))
                    continue;
                if (const double t = trained(id); t > best)
                    best = t, chosen = id;
            }
            if (!chosen.empty())
                how = "a master comes to the trade";
        }
        if (chosen.empty() && waited >= 3 && !ps.lastHolder.empty())
        {
            double best = -1;
            // Family carry on a family's trade when it is a step up for them (or they have no work): the son takes
            // over the stall, but the stallkeeper doesn't leave it to fetch errands.
            for (const auto& [id, life] : state_.residents)
                if (this->family(ps.lastHolder, id) && alive(id) && world.age(id) >= 16 && rank(jobOf(id)) < rank(&p) && trained(id) >= level)
                {
                    const double score = skill(id, p.id) * 2 + world.regard(id, ps.lastHolder) + world.age(id) / 10.0;
                    if (score > best)
                        best = score, chosen = id;
                }
            if (!chosen.empty())
                how = "the family carries it on";
        }
        if (chosen.empty() && waited >= 5)
        {
            double best = -1;
            for (const auto& [id, life] : state_.residents)
                if (!jobOf(id) && !apprenticedTo(id) && alive(id) && world.age(id) >= 16 && world.age(id) < retireAge(id) && trained(id) >= level &&
                    (!world.near || world.near(reach, life.homeCell)))
                {
                    const double score = skill(id, p.id) * 2 + (ps.lastHolder.empty() ? 0 : world.regard(ps.lastHolder, id));
                    if (score > best)
                        best = score, chosen = id;
                }
            if (!chosen.empty())
                how = "someone local takes it on";
        }
        if (chosen.empty() && waited >= 5 && level > 0)
        {
            // The user's rule: the best-trained, not already in a master's place or an important post.
            double best = -1;
            for (const auto& [id, life] : state_.residents)
                if (alive(id) && world.age(id) >= 16 && world.age(id) < retireAge(id) && !apprenticedTo(id) && !important(id) &&
                    rank(jobOf(id)) < rank(&p) && (!world.near || world.near(reach, life.homeCell)))
                    if (const double t = trained(id); t > best)
                        best = t, chosen = id;
            if (!chosen.empty())
                how = "the best-trained takes it on, to work toward mastership";
        }
        // A post that pays, empty PostWaitDays (the user, 2026-11-01): a wolf of the city with no post that pays (grown
        // pups first) of the post's class; else an unplaced NPC of the world's own list, of its class, come in from the
        // Country (one whose trade suits the post first); then the same of the next class. Nobody new is ever made.
        if (chosen.empty() && waited >= PostWaitDays && !postClass(p).empty() &&
            std::none_of(requests_.begin(), requests_.end(), [&](const ResidentRequest& r) { return r.kind == "arrival" && r.positionId == p.id; }))
            for (int step = 0; step < 2 && chosen.empty(); ++step)
            {
                chosen = cityWolfFor(p, world, step);
                if (!chosen.empty())
                {
                    how = step ? "from the city, of the next class" : "from the city";
                    break;
                }
                ResidentRequest r;
                if (arrivalFor(p, ps, world, step, r))
                {
                    requests_.push_back(r);
                    ++fills_[step ? "from the Country, of the next class" : "from the Country"];
                    notes.push_back({"arrival sent for", r.templateId, ps.lastHolder, p.title});
                    break;
                }
            }
        if (chosen.empty())
            continue;
        ++fills_[how];
        for (auto& [otherId, other] : c.positions)
        {
            if (other.holder == chosen)
            {
                other.holder.clear();
                other.lastHolder = chosen;
                other.vacantSince = now;
                notes.push_back({"vacancy", chosen, {}, position(otherId) ? position(otherId)->title : otherId});
            }
            if (other.apprentice == chosen)
                other.apprentice.clear();
        }
        ps.holder = chosen;
        ps.vacantSince = -1;
        auto& known = c.skill[skillKey(chosen, p.id)];
        known = std::max({known, 5.0, familySkill(chosen, family)});   // (Its hand at that kind of work carries over.)
        notes.push_back({"succession", chosen, ps.lastHolder, p.title + ": " + how});
        forgetCareers();
    }

    // Apprenticeships (every trade has them: the user, 2026-09-27; doc 64, Phase 8): a qualified hand takes on a young
    // wolf now and then, until it reaches the trade's level (TradeSkilled for a plain trade): family and household
    // first, one out of work, or one working alongside it; a masters' trade takes the young from plain work too. A
    // skilled trade's master must be at its level; a plain trade's, its years or TradeSkilled.
    for (auto& p : positions_)
    {
        auto& ps = c.positions[p.id];
        const double level = tradeLevel(p), target = std::max(level, TradeSkilled);
        const auto reach = reachOf(p);
        if (!ps.apprentice.empty() && (!alive(ps.apprentice) || ps.apprentice == ps.holder))
            ps.apprentice.clear();
        if (!ps.apprentice.empty() && skill(ps.apprentice, p.id) >= target)
        {
            notes.push_back({"apprenticeship completed", ps.apprentice, ps.holder, p.title});
            ps.apprentice.clear();
        }
        const bool qualified = skill(ps.holder, p.id) >= target || (level < TradeSkilled && world.age(ps.holder) >= 35);
        if (!ps.apprentice.empty() || !alive(ps.holder) || p.role == "guard" || !thisWeek(p.id, today) || !qualified)
            continue;
        std::string chosen;
        int bestTier = -1, bestAge = 1000;
        indexCareers();
        for (const auto& [id, life] : state_.residents)
        {
            const int age = world.age(id);
            if (id == ps.holder || !alive(id) || age < 12 || age >= 24 || apprenticedTo(id))
                continue;
            const auto* own = jobOf(id);
            const bool alongside = own && own->work.cell == p.work.cell;
            if (own && !alongside)
                continue;                             // Busy with work elsewhere.
            const int tier = this->family(ps.holder, id) ? 3 : household(ps.holder, id) ? 2
                             : alongside && world.regard(ps.holder, id) >= 30 ? 1
                             : !own && (!world.near || world.near(reach, life.homeCell)) ? 0 : -1;
            if (tier > bestTier || (tier == bestTier && tier >= 0 && age < bestAge))
                bestTier = tier, bestAge = age, chosen = id;
        }
        if (bestTier < 0)
            continue;
        // (Taken from plain work: its post falls empty for another.)
        if (const auto* own = jobOf(chosen); own && own->work.cell != p.work.cell)
        {
            auto& was = c.positions[own->id];
            was.lastHolder = chosen;
            was.holder.clear();
            was.vacantSince = now;
            notes.push_back({"vacancy", chosen, {}, own->title});
        }
        ps.apprentice = chosen;
        notes.push_back({"apprenticeship", chosen, ps.holder, p.title});
        forgetCareers();
    }

    staffFarms(world, notes);                      // (The farms' people: doc 71, Phase C; every area is on since doc 79.)

    splitHouseholds(today, world, notes);

    tendBirths(now, world);
    return notes;
}

void Society::tendBirths(double now, const CareerWorld& world)
{
    // Births follow deaths (the user, 2026-11-01: "Birthrates should match death rates for this game, otherwise we are
    // going to end up with a shitton of NPCs real fast. Player count should be ignored for that count."): each child owed
    // for a resident's death (died) is born when due, one to a couple: a married couple at home together, the mother of
    // an age to bear (18 to 45), a year or so since their last, no more than three; those with fewest children first,
    // then by a lot of the day. A child nobody can have today waits for tomorrow. There are no other births.
    auto& c = state_.careers;
    std::vector<std::pair<double, std::string>> due;
    for (const auto& [dead, day] : c.birthsOwed)
        if (day <= now)
            due.emplace_back(day, dead);
    if (due.empty())
        return;
    std::sort(due.begin(), due.end());
    const auto today = std::int64_t(std::floor(now));
    struct Couple
    {
        std::string a, b;
        const ResidentSpec* mother;
        int children;
        double lot;
    };
    std::vector<Couple> couples;
    for (const auto& [a, b] : c.spouses)
    {
        if (!(a < b) || !world.alive(a) || !world.alive(b) || !household(a, b))
            continue;
        const auto* x = spec(a);
        const auto* y = spec(b);
        const auto* mother = x && x->appearance.sex == "female" && world.age(a) >= 18 && world.age(a) <= 45 ? x
                             : y && y->appearance.sex == "female" && world.age(b) >= 18 && world.age(b) <= 45 ? y : nullptr;
        if (!mother)
            continue;
        const auto key = a + "|" + b;
        const int children = c.births.count(key) ? c.births.at(key) : 0;
        const auto last = c.lastBirth.find(key);
        if (children >= 3 || (last != c.lastBirth.end() && now - last->second < 300))
            continue;
        couples.push_back({a, b, mother, children, lotOf(key + "|" + std::to_string(today))});
    }
    std::sort(couples.begin(), couples.end(),
              [](const Couple& x, const Couple& y) { return std::tie(x.children, x.lot, x.a) < std::tie(y.children, y.lot, y.a); });
    for (std::size_t i = 0; i < due.size() && i < couples.size(); ++i)
    {
        const auto& k = couples[i];
        const auto key = k.a + "|" + k.b;
        const auto* home = resident(k.mother->id);
        ResidentRequest r;
        r.kind = "birth";
        r.templateId = k.mother->id;
        const auto family = surname(k.mother->name);
        r.name = std::string(FirstNames[mix(key, today + 7) % std::size(FirstNames)]) + (family.empty() ? "" : " " + family);
        r.age = 0;
        r.home = home ? Spot{home->homeCell, home->homeX, home->homeY} : k.mother->home;
        r.parents = {k.a, k.b};
        r.sex = birthSex(key, today);
        r.inPlaceOf = due[i].second;
        c.lastBirth[key] = now;
        ++c.births[key];
        c.birthsOwed.erase(due[i].second);
        requests_.push_back(r);
    }
}

std::string Society::cityWolfFor(const Position& p, const CareerWorld& world, int classStep) const
{
    // A wolf of working age with no post that pays (none, or a pup's or an elder's: lessons, play), not apprenticed,
    // living within reach, its class classStep from the post's: grown pups first (raised here: a parent known, under 25),
    // then the best at that kind of work, then a lot of the post's.
    const auto wanted = postClass(p);
    const auto reach = reachOf(p);                  // (A works' post: its crew's home ground, doc 72.)
    const auto family = std::string(skillFamily(p.title));
    std::string chosen;
    std::tuple<int, double, double> best{-1, 0, 0};
    for (const auto& [id, life] : state_.residents)
    {
        if (!world.alive(id))
            continue;
        const int age = world.age(id);
        if (age < 16 || age >= retireAge(id) || apprenticedTo(id) || (world.near && !world.near(reach, life.homeCell)))
            continue;
        if (const auto* own = jobOf(id); own && !postClass(*own).empty())
            continue;
        if (classApart(wanted, classOf(id)) != classStep)
            continue;
        const bool pup = state_.careers.parents.count(id) && age < 25;
        const std::tuple<int, double, double> score{pup ? 1 : 0, std::max(skill(id, p.id), familySkill(id, family)),
                                                    lotOf(id + "|" + p.id)};
        if (chosen.empty() || score > best)
            best = score, chosen = id;
    }
    return chosen;
}

bool Society::arrivalFor(const Position& p, const PositionState& ps, const CareerWorld& world, int classStep,
                         ResidentRequest& out) const
{
    // Who comes: an unplaced NPC of the world's own list (doc 66, 2) of working age, its class classStep from the post's,
    // not already sent for: the one whose trade suits the post best (its kind of work; the post's words in its label or
    // description), then a lot of the post's. Any sex.
    const auto wanted = postClass(p);
    const auto reach = reachOf(p);                  // (A works' post: its crew's home ground, doc 72.)
    const auto family = std::string(skillFamily(p.title));
    const auto words = tradeWords(p.title);
    const ResidentSpec* chosen = nullptr;
    std::pair<int, double> best{-1, 0};
    for (const auto& r : authored_.unplaced)
    {
        if (r.protectedNpc || r.age < 16 || r.age >= retireAge(r.id) || classApart(wanted, classOf(r.id)) != classStep ||
            std::any_of(requests_.begin(), requests_.end(),
                        [&](const ResidentRequest& q) { return (q.kind == "arrival" || q.kind == "farm") && q.templateId == r.id; }))
            continue;
        const bool kind = family != "general" && family == skillFamily(r.workLabel);
        const auto theirs = lower(r.workLabel + " " + r.description);
        int shared = 0;
        for (const auto& w : words)
            shared += theirs.find(w) != std::string::npos;
        const std::pair<int, double> score{(kind ? 2 : 0) + std::min(shared, 2), lotOf(r.id + "|" + p.id)};
        if (!chosen || score > best)
            best = score, chosen = &r;
    }
    if (!chosen)
        return false;
    // Where it stays (never alone at the work, and no home built): the dead's home, if the last holder died and nobody
    // lives there now; else with a household of its class within reach of the work that has room (fewer than
    // HouseholdMost), the smallest first; else the communal lodgings within reach. Nowhere: not today.
    std::map<std::string, std::vector<std::string>> homes;
    for (const auto& [id, life] : state_.residents)
        if (world.alive(id) && !life.homeCell.empty())
            homes[life.homeCell].push_back(id);
    Spot home;
    if (!ps.lastHolder.empty() && !world.alive(ps.lastHolder))
        if (const auto* was = resident(ps.lastHolder);
            was && !was->homeCell.empty() && !communalHome(was->homeCell) && !homes.count(was->homeCell))
            home = {was->homeCell, was->homeX, was->homeY};
    if (home.cell.empty())
    {
        const auto cls = classOf(chosen->id);
        std::pair<int, double> roomiest{0, 0};
        for (const auto& [cell, members] : homes)
        {
            if (communalHome(cell) || int(members.size()) >= HouseholdMost || (world.near && !world.near(reach, cell)) ||
                cell.rfind("fs_", 0) == 0)
                continue;                           // (A farmhouse is its keeper's family's alone, as staffFarms keeps it.)
            std::map<std::string, int> count;
            for (const auto& m : members)
                ++count[classOf(m)];
            std::string most;
            for (const auto& [c, n] : count)
                if (most.empty() || n > count[most])
                    most = c;
            if (most != cls)
                continue;
            const std::pair<int, double> score{-int(members.size()), lotOf(cell + "|" + chosen->id)};
            if (home.cell.empty() || score > roomiest)
            {
                const auto* first = resident(members.front());
                roomiest = score, home = {cell, first->homeX, first->homeY};
            }
        }
    }
    if (home.cell.empty() && world.lodgings)
        if (const auto spots = world.lodgings(reach); !spots.empty())
            home = spots.front();
    if (home.cell.empty())
        return false;
    out = {};
    out.kind = "arrival";
    out.templateId = chosen->id;
    out.name = chosen->name;
    out.age = chosen->age;
    out.home = home;
    out.positionId = p.id;
    out.purse = std::clamp<std::int64_t>(chosen->purse, 0, ArrivalPurse);
    // Its trade of the post's kind: it knows the work as well as the trade needs (as one who did it in the Country);
    // otherwise it is new to it.
    out.skill = family != "general" && family == skillFamily(chosen->workLabel) ? std::max(35.0, tradeLevel(p)) : 5.0;
    return true;
}

CareerNote Society::apprentice(const std::string& player, const std::string& positionId, const CareerWorld& world, double)
{
    const auto* p = position(positionId);
    auto found = state_.careers.positions.find(positionId);
    if (!p || found == state_.careers.positions.end() || !playerAccountId(player))
        return {"refused", player, {}, "There is no such trade to learn."};
    auto& ps = found->second;
    if (ps.holder.empty() || !world.alive(ps.holder))
        return {"refused", player, {}, "Nobody holds that position to teach it."};
    if (!ps.apprentice.empty())
        return {"refused", player, ps.holder, "They already have an apprentice."};
    if (apprenticedTo(player))
        return {"refused", player, ps.holder, "You are already apprenticed."};
    // A master takes on someone they know and trust (Bonds: regard of 30 or more).
    if (world.regard(ps.holder, player) < 30)
        return {"refused", player, ps.holder, "They don't know you well enough to take you on."};
    ps.apprentice = player;
    forgetCareers();
    return {"apprenticeship", player, ps.holder, p->title};
}

CareerNote Society::appoint(const std::string& positionId, const std::string& resident, const CareerWorld& world, double day)
{
    // Speaking for a resident (doc 57, 3): the post is theirs at once, as if succession had chosen them.
    const auto* p = position(positionId);
    auto found = state_.careers.positions.find(positionId);
    if (!p || found == state_.careers.positions.end() || !state_.residents.count(resident) || (world.alive && !world.alive(resident)))
        return {"refused", resident, {}, "There is no such post."};
    if (!found->second.holder.empty())
        return {"refused", resident, found->second.holder, "That post is taken."};
    if (p->role == "guard")
        return {"refused", resident, {}, "The watch chooses its own."};
    auto& c = state_.careers;
    for (auto& [otherId, other] : c.positions)
    {
        if (other.holder == resident)
        {
            other.holder.clear();
            other.lastHolder = resident;
            other.vacantSince = day;
        }
        if (other.apprentice == resident)
            other.apprentice.clear();
    }
    auto& ps = found->second;
    ps.holder = resident;
    ps.vacantSince = -1;
    auto& known = c.skill[skillKey(resident, p->id)];
    known = std::max({known, 10.0, .5 * familySkill(resident, skillFamily(p->title))});
    forgetCareers();
    return {"succession", resident, ps.lastHolder, p->title + ": spoken for"};
}

CareerNote Society::apprenticeTo(const std::string& positionId, const std::string& youth, const CareerWorld& world, double)
{
    // Sponsoring an apprenticeship (doc 57, 3): begun now rather than at the weekly chance, under its rules.
    const auto* p = position(positionId);
    auto found = state_.careers.positions.find(positionId);
    if (!p || found == state_.careers.positions.end() || !state_.residents.count(youth))
        return {"refused", youth, {}, "There is no such trade to learn."};
    auto& ps = found->second;
    const auto alive = [&](const std::string& id) { return !id.empty() && (!world.alive || world.alive(id)); };
    const auto age = [&](const std::string& id) { return world.age ? world.age(id) : (spec(id) ? spec(id)->age : 30); };
    if (!alive(ps.holder) || !alive(youth))
        return {"refused", youth, ps.holder, "Nobody holds that trade to teach it."};
    if (p->role == "guard")
        return {"refused", youth, ps.holder, "The watch takes no apprentices."};
    if (!ps.apprentice.empty())
        return {"refused", youth, ps.holder, "They already have an apprentice."};
    if (age(ps.holder) < 35 && skill(ps.holder, p->id) < 70)
        return {"refused", youth, ps.holder, "They are too young at the trade to teach it."};
    const int years = age(youth);
    if (years < 12 || years > 25)
        return {"refused", youth, ps.holder, "Apprentices are twelve to twenty-five."};
    if (apprenticedTo(youth))
        return {"refused", youth, ps.holder, "They are apprenticed already."};
    if (const auto* own = jobOf(youth); own && own->work.cell != p->work.cell)
        return {"refused", youth, ps.holder, "They are busy with work elsewhere."};
    ps.apprentice = youth;
    forgetCareers();
    return {"apprenticeship", youth, ps.holder, p->title};
}
} // namespace ratw
