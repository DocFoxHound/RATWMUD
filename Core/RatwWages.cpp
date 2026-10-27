// Who pays whom (Docs/Design/42-money-in-circulation.md, Phase 2). A position is paid by whoever it works for: a
// shop's help by its keeper, the watch and the town's own by the town, the clergy by the church, a great house's staff
// by its head. Those who bring goods in from the land live by selling them, and children draw no wages.
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ratw
{
namespace
{
std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}
bool hasAny(const std::string& text, std::initializer_list<const char*> words)
{
    for (const char* w : words)
        if (text.find(w) != std::string::npos)
            return true;
    return false;
}
// A hand at a trade's works (at the forge, working the saws, smoking fish): the words of its title, and the kinds of shop
// that pay it (payerOf).
const std::vector<std::pair<const char*, std::vector<const char*>>> trades = {
    {"forge", {"smithy", "ironworks", "foundry"}}, {"saws", {"sawmill", "carpenter"}}, {"smoking fish", {"smokehouse", "fishmonger"}},
    {"rope", {"ropewalk", "chandlery"}}, {"glass", {"glassworks"}}, {"kiln", {"brickworks", "pottery"}}, {"salt", {"saltworks"}},
    {"tann", {"tannery"}}, {"brew", {"brewery"}}, {"dye", {"dyeworks"}}, {"ships", {}}, {"mill", {"mill"}}, {"cooper", {"cooperage"}},
    {"the catch", {"fishmonger", "smokehouse"}}, {"foreman", {}}, {"overseeing", {}},
    // (With work by the hours only, from here: doc 64, Phase 13, round 6. The docks' timber, for the joiners.)
    {"hauling timber", {"sawmill", "carpenter"}}};
constexpr std::size_t TradesAtBothPaces = 16;      // (The first of `trades`: at either pace.)
// What a position's title alone says of who pays it (payerOf), worked out once a title: each thread remembers.
struct TitleWages
{
    bool household = false;                         // Work nobody pays a wage for.
    bool church = false;                            // The clergy, and those who tend the sick.
    int trade = -1;                                 // The first of `trades` its title names, or -1.
    int hoursTrade = -1;                            // The same of all of them, with work by the hours.
    bool forHire = false;                           // Work for hire (Society::forHire).
    bool estate = false;                            // A great house's grounds: its gardens, its gate.
};
const TitleWages& titleWages(const std::string& title)
{
    thread_local std::unordered_map<std::string, TitleWages> known;
    if (const auto found = known.find(title); found != known.end())
        return found->second;
    TitleWages w;
    const auto t = lower(title);
    w.household = hasAny(t, {"keeps the house", "keeping the house", "apprenticed", "sells from", "picks through", "sits ", "sits by",
                             "telling stories", "plays", "beg"});
    w.church = Society::clergy(title) || hasAny(t, {"the sick", "the hurt"});
    for (std::size_t i = 0; i < trades.size() && w.hoursTrade < 0; ++i)
        if (t.find(trades[i].first) != std::string::npos)
            w.hoursTrade = int(i);
    w.trade = w.hoursTrade < int(TradesAtBothPaces) ? w.hoursTrade : -1;
    w.forHire = hasAny(t, {"for hire", "runs messages"});
    w.estate = hasAny(t, {"tending the gardens", "keeping the gate"});
    return known.emplace(title, w).first->second;
}
} // namespace

bool Society::clergy(const std::string& title)
{
    thread_local std::unordered_map<std::string, bool> known;      // (Asked of the same titles every decision.)
    if (const auto found = known.find(title); found != known.end())
        return found->second;
    const bool is = hasAny(lower(title), {"chapel", "priest", "acolyte", "cathedral", "choir", "prelate", "shrine", "chaplain",
                                          "sexton", "deacon", "abbey", "temple", "church", "the mass", "confession", "the crypt",
                                          "healer"});
    return known.emplace(title, is).first->second;
}

bool Society::forHire(const std::string& title)
{
    return titleWages(title).forHire;
}

bool Society::houseHead(const std::string& title)
{
    // The head of a great house (a lord, a lady, a hall's keeper): unpaid, living on the house's own money.
    thread_local std::unordered_map<std::string, bool> known;      // (As clergy.)
    if (const auto found = known.find(title); found != known.end())
        return found->second;
    const auto t = lower(title);
    // (Keeping a house's own seat: "keeping Grayrock Hall", "keeping Mossgrave Court"; not a caretaker of a public
    // hall, "keeping the Rain Hall" (doc 61).)
    const bool is = t.rfind("ruling ", 0) == 0 || t.rfind("holding court", 0) == 0 ||
                    (t.rfind("keeping ", 0) == 0 && t.rfind("keeping the ", 0) != 0 && hasAny(title, {" Hall", " House", " Manor", " Court"}));
    return known.emplace(title, is).first->second;
}

namespace
{
// The house a room is of ("villa_x_up", "hall_down": "villa_x", "hall"), or the room itself.
std::string houseOfRoom(const std::string& cell)
{
    for (const std::string floor : {"_up", "_down"})
        if (cell.size() > floor.size() && cell.compare(cell.size() - floor.size(), floor.size(), floor) == 0)
            return cell.substr(0, cell.size() - floor.size());
    return cell;
}
} // namespace

void Society::indexEmployers() const
{
    if (!employers_.empty() || positions_.empty())
        return;
    for (const auto& p : positions_)
        if (p.role == "merchant" || (!p.paid && p.role == "civilian" && houseHead(p.title)))
        {
            employers_[p.work.cell].push_back(p.id);
            // (A great house's seat: its head's work room and the floors above and below it; the first head's, as here.)
            if (p.role != "merchant")
                for (const auto seat = houseOfRoom(p.work.cell); const auto& room : {seat, seat + "_up", seat + "_down"})
                    seats_.try_emplace(room, p.id);
        }
    employers_[""];                                  // (Indexed, even with no employers at all.)
}

const House* Society::seatOf(const std::string& cell) const
{
    indexEmployers();
    const auto head = seats_.find(cell);
    if (head == seats_.end())
        return nullptr;
    for (const auto& h : houses())
        if (h.head == head->second)
            return account(h.id) ? &h : nullptr;
    return nullptr;
}

Society::Payer Society::payerOf(const std::string& resident, const Position& job, int age) const
{
    return payerFor(resident, nullptr, job, age);
}

Society::Payer Society::payerOf(const ResidentRecord& resident, const Position& job, int age) const
{
    return payerFor(resident.entry->first, &resident, job, age);
}

Society::Payer Society::payerFor(const std::string& resident, const ResidentRecord* record, const Position& job, int age) const
{
    if (age < 16)
        return {"", "a child"};
    // (By its record while the records hold: its town, treasury and own work, and its own job's title, as they were found.)
    const bool holds = record && recordsHold(), placed = holds && placesHold(), ownJob = holds && &job == record->job;
    const auto treasury = [&] { return placed ? record->treasury : treasuryOfResident(resident); };
    // A fortress's garrison is the crown's: paid from the capital's treasury, not the fortress's own small purse.
    std::string spare;
    const auto& town = placed ? record->town : (spare = communityOfResident(resident));
    const bool fortress = town.find("fortress") != std::string::npos;
    if (job.role == "guard")
        return {fortress ? std::string("treasury") : treasury(), "the town"};
    if (ownJob ? record->outworkJob : outworkTitled(job.title))
        return {"", "lives by what it brings in"};
    if (ownJob ? record->labourJob : job.title == LabourTitle)
        return {treasury(), "the Town Works"};
    if (holds ? record->producer != nullptr : [&] {
            const auto* r = spec(resident);
            return r && items::producerFor(r->workLabel);
        }())
        return {"", "lives by what it brings in"};
    // An apprentice: its master's business, on half pay (doc 64, Phase 8).
    if (job.title.rfind("apprenticed: ", 0) == 0)
        if (const auto* learning = holds ? record->learning : apprenticedTo(resident))
            if (const auto held = state_.careers.positions.find(learning->id); held != state_.careers.positions.end() && !held->second.holder.empty())
                return {tillOf(held->second.holder), "the shop"};
    const auto& said = titleWages(job.title);
    // Work nobody pays a wage for (doc 42, 2026-10-05): keeping one's own house, learning a trade, hawking, scavenging,
    // sitting by the well. The household keeps them, or they live by what they sell.
    if (said.household)
        return {"", "lives on the household"};
    if (said.church)
        return {churchOf(treasury()), "the church"};
    const bool hours = workByHours();
    // An unpaid post, paid by whoever it works for (doc 42, Phase 3), with work by the hours (doc 64, Phase 13, round 6):
    // one worked at its holder's own home (at home in the Heights, at lessons, at leisure) is its household's own life,
    // no wage. Before, the household's richest paid it, a servant as often as not, or the town did.
    // (Its life by its record while the records hold.)
    static const std::string nowhere;
    const auto own = holds ? state_.residents.end() : state_.residents.find(resident);
    const auto* life = holds ? &record->entry->second : own == state_.residents.end() ? nullptr : &own->second;
    const auto& home = life ? life->homeCell : nowhere;
    if (hours && !job.paid && job.role == "civilian" && !home.empty() && houseOfRoom(home) == houseOfRoom(job.work.cell))
        return {"", "lives on the household"};
    // Someone with a shop, or a great house, where it works: the first of them there who isn't itself.
    indexEmployers();
    if (const auto found = employers_.find(job.work.cell); found != employers_.end())
        for (const auto& pid : found->second)
        {
            const auto held = state_.careers.positions.find(pid);
            if (pid == job.id || held == state_.careers.positions.end() || held->second.holder.empty() ||
                held->second.holder == resident || !account(held->second.holder))
                continue;
            const auto* employer = position(pid);
            // (A house's staff, with work by the hours, from the house's own purse, as its retainers are: doc 64, Phase
            // 13, round 6. Before, and at the seconds' pace, from its head's.)
            if (const House* h = hours && employer && employer->role != "merchant" ? seatOf(job.work.cell) : nullptr; h && h->head == pid)
                return {h->id, "the house"};
            // (A shop pays from its till: doc 46, Phase 2. Before, from its keeper's purse, even a house's manager's.)
            return {tillOf(held->second.holder), employer && employer->role == "merchant" ? "the shop" : "the house"};
        }
    if (hours)
    {
        // A great house's seat's other rooms (the chambers above, the kitchen below), with work by the hours: the house
        // pays its staff there as at its head's own room (doc 64, Phase 13, round 6; before, the town did).
        if (const auto* h = seatOf(job.work.cell); h && h->head != job.id)
            return {h->id, "the house"};
        // An unpaid post of a great house's family away from its seat and any shop (the House's money counted, its
        // business abroad): the house's. Before, the town's.
        if (const auto* h = job.paid ? nullptr : seatOf(home))
            return {h->id, "the house"};
        // A house's grounds (its gardens, its gate): the house whose name its home bears ("the Fell gatehouse", "the
        // Grayrock cottages"), else its town's richest, as a trade's works hands (below). Before, the town.
        if (said.estate)
        {
            const House* named = nullptr;
            const House* richest = nullptr;
            std::int64_t most = -1;
            for (const auto& h : houses())
            {
                const auto* purse = account(h.id);
                if (!purse)
                    continue;
                // (Its family's name a word of the home's: "the_fell_gatehouse" names House Fell, "pinefell_house"
                // doesn't; "Grayrock Hall", Grayrock. The grounds may lie out of its town: the hills beyond the city.)
                const auto word = h.name.rfind("House ", 0) == 0 ? lower(h.name.substr(6))
                                  : h.name.rfind("the ", 0) == 0 ? std::string()
                                                                 : lower(h.name.substr(0, h.name.find(' ')));
                for (auto at = home.find(word); !named && !word.empty() && at != std::string::npos; at = home.find(word, at + 1))
                    if ((at == 0 || home[at - 1] == '_') && (at + word.size() == home.size() || home[at + word.size()] == '_'))
                        named = &h;
                if (h.community == town && purse->cash > most)
                    most = purse->cash, richest = &h;
            }
            if (named || richest)
                return {(named ? named : richest)->id, "the house"};
        }
    }
    // A household's servant ("serving the household", "keeping the chambers"): paid by the richest of the household
    // whose home it works in. With work by the hours, a live-in servant too, by the household's richest not in service
    // itself (doc 64, Phase 13, round 6): before, a servant working where it lives went to the town, and the richest
    // found could be a fellow servant.
    if (hours)
    {
        if (const auto master = richestAt(job.work.cell, resident, true); !master.empty())
            return {master, "the house"};
    }
    else if (!life || life->homeCell != job.work.cell)
        if (const auto master = richestAt(job.work.cell, resident); !master.empty())
            return {master, "the house"};
    // A hand at a trade's works (at the forge, working the saws, smoking fish): paid by a keeper of that trade in its
    // town, or else by its town's richest great house, who own the industry.
    if (const int trade = hours ? said.hoursTrade : said.trade; trade >= 0)
    {
        const auto& kinds = trades[std::size_t(trade)].second;
        const auto keeper = [&](const Position& p, const items::Business* business) -> const std::string* {
            const auto held = state_.careers.positions.find(p.id);
            if (business && std::find(kinds.begin(), kinds.end(), business->id) != kinds.end() &&
                held != state_.careers.positions.end() && !held->second.holder.empty())
                return &held->second.holder;
            return nullptr;
        };
        if (placesHold())
        {
            // (Its town's merchants, in the same order: placeRecords.)
            if (const auto here = merchantsByTown_.find(town); here != merchantsByTown_.end())
                for (const auto& [p, business] : here->second)
                    if (const auto* holder = keeper(*p, business))
                        return {tillOf(*holder), "the shop"};
        }
        else
            for (const auto& p : positions_)
            {
                if (p.role != "merchant" || communityOfResident(p.founder) != town)
                    continue;
                if (const auto* holder = keeper(p, items::businessFor(p.title)))
                    return {tillOf(*holder), "the shop"};
            }
        std::string house;
        std::int64_t most = -1;
        for (const auto& h : houses())
            if (const auto* purse = account(h.id); purse && h.community == town && purse->cash > most)
                most = purse->cash, house = h.id;
        if (!house.empty())
            return {house, "the house"};
    }
    // Work for hire that nobody's post pays (no shop where it works), with work by the hours (doc 64, Phase 13, round
    // 6): the household keeps them, and they take hires as the jobless do (RatwResidents.cpp). Before, the town paid
    // them as the payer of last resort.
    if (hours && said.forHire)
        return {"", "lives on the household"};
    return {fortress ? std::string("treasury") : treasury(), "the town"};
}

double Society::dayWage(const std::string& town, const std::string& kind) const
{
    // The going rate (doc 64, Phase 6): a labourer's day in the town; the clergy a tenth over, an odd job a third of a day.
    const double going = goingRate(town);
    return kind == "clergy" ? going * ClergyShare : kind == "odd job" ? going / 3 : going;
}

std::string Society::wageKind(const Payer& payer, const Position& job) const
{
    if (job.role == "guard")
        return "guard";
    if (payer.whom == "the church")
        return "clergy";
    if (payer.whom == "the shop" || payer.whom == "the house")
        return "help";
    return "labour";                                 // (The town's own posts and its works.)
}

bool Society::inService(const std::string& resident, const std::string& homeCell) const
{
    // Its own paid post worked at home: a live-in servant, not the household's master.
    const auto* job = jobOf(resident);
    return job && job->paid && job->role == "civilian" && houseOfRoom(job->work.cell) == houseOfRoom(homeCell);
}

void Society::rollRichest()
{
    // The household living in each cell, by its richest member; with work by the hours, its richest not in service too.
    richest_.clear();
    richestFree_.clear();
    richestDay_ = state_.budgetDay;
    std::map<std::string, std::int64_t> most, mostFree;
    for (const auto& [id, life] : state_.residents)
        if (const auto* purse = account(id); purse && !life.homeCell.empty())
        {
            if (!most.count(life.homeCell) || purse->cash > most[life.homeCell])
                most[life.homeCell] = purse->cash, richest_[life.homeCell] = id;
            if (workByHours() && !inService(id, life.homeCell) && (!mostFree.count(life.homeCell) || purse->cash > mostFree[life.homeCell]))
                mostFree[life.homeCell] = purse->cash, richestFree_[life.homeCell] = id;
        }
}

std::string Society::richestIn(const std::string& homeCell, bool free) const
{
    std::string who;
    std::int64_t most = 0;
    for (const auto& [id, life] : state_.residents)
        if (const auto* purse = account(id); purse && life.homeCell == homeCell && (who.empty() || purse->cash > most) &&
                                             (!free || !inService(id, life.homeCell)))
            most = purse->cash, who = id;
    return who;
}

std::string Society::richestAt(const std::string& homeCell, const std::string& besides, bool free) const
{
    // The room itself, or the house it is a room of (a palazzo's salon below its family's chambers: "villa_x",
    // "villa_x_up"); with work by the hours, a room below too (a hall's kitchen, "hall_down": doc 64, Phase 13, round 6).
    const auto up = homeCell.size() > 3 && homeCell.compare(homeCell.size() - 3, 3, "_up") == 0 ? homeCell.substr(0, homeCell.size() - 3)
                                                                                              : std::string();
    const auto down = workByHours() && homeCell.size() > 5 && homeCell.compare(homeCell.size() - 5, 5, "_down") == 0
                          ? homeCell.substr(0, homeCell.size() - 5)
                          : std::string();
    const auto& day = free ? richestFree_ : richest_;
    const bool known = richestDay_ == state_.budgetDay;
    for (const auto& cell : {homeCell, homeCell + "_up", up, down, down.empty() ? std::string() : down + "_up"})
    {
        if (cell.empty())
            continue;
        std::string who;
        if (!known)
            who = richestIn(cell, free);            // (Asked before the day's are known: for this asking alone, never kept.)
        else if (const auto found = day.find(cell); found != day.end())
            who = found->second;
        if (!who.empty() && who != besides)
            return who;
    }
    return {};
}
} // namespace ratw
