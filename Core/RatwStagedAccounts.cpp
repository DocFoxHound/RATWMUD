// Doc 79, Phase 2 (Docs/Design/79-a-staged-economy.md, sections 6 and 9): the bands and the staged accounts. Each
// household scored, ranked and cut into a band once a day; the account kinds and the faucet door; the welcome's cap; the
// staged payers topped up; holds, lean days, the goods trim; the bands and the staged day settled at restore; the Mind's
// "Means" note.
#include "RatwCalendar.h"
#include "RatwSociety.h"
#include "RatwTalk.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace ratw
{
namespace
{
bool startsWith(const std::string& s, const char* prefix)
{
    return s.rfind(prefix, 0) == 0;
}
bool has(const std::string& s, const char* word)
{
    return s.find(word) != std::string::npos;
}
// A number of pence as a word, for talk and the Mind ("eight pence").
std::string pence(std::int64_t n)
{
    static const char* const Small[] = {"no",      "a penny", "two",      "three",    "four",    "five",     "six",
                                        "seven",   "eight",   "nine",     "ten",      "eleven",  "twelve",   "thirteen",
                                        "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen", "twenty"};
    if (n >= 0 && n <= 20)
        return n == 1 ? std::string(Small[1]) : std::string(Small[n]) + " pence";
    return std::to_string(n) + " pence";
}
} // namespace

// ---- The account kinds (9.1) ----

Society::AccountKind Society::classify(const std::string& id)
{
    if (playerAccountId(id) || startsWith(id, "chapter:"))
        return AccountKind::Real;
    for (const char* p : {"contract:", "letter:", "let:", "stall:", "table:", "fest:", "project:", "ground:", "bandits:"})
        if (startsWith(id, p))
            return AccountKind::Escrow;
    // (house: has no kept reader: Phase 0's list. The old economy's own are dropped at restore: doc 79, 14.)
    for (const char* p : {"caravan:", "fund:", "bank:", "outside:", "farm:", "crew:", "estate:", "house:"})
        if (startsWith(id, p))
            return AccountKind::Dead;
    return AccountKind::Staged;                     // The treasury, stores:, till:, town:, home:, residents, "made".
}

void Society::setHolder(const std::string& player, const std::string& holder)
{
    if (!playerAccountId(player) || holder.size() > 200)
        return;
    if (holder.empty() || holder == player)
        holders_.erase(player);
    else
        holders_[player] = holder;
}

std::string Society::holderOf(const std::string& account) const
{
    const auto found = holders_.find(account);
    return found == holders_.end() ? account : found->second;
}

std::string Society::faucetGroup(const std::string& reason)
{
    // (The DM's grants and fines first, whatever their reason's words: doc 79, 12; Phase 9's grantPlayer.)
    if (startsWith(reason, "dm grant: ") || startsWith(reason, "dm fine: "))
        return "grant";
    if (has(reason, "stolen"))
        return "theft";
    if (has(reason, "welcome") || has(reason, "provisions to start with"))
        return "welcome";
    if (has(reason, "reward") || has(reason, "prize") || has(reason, "bounty") || has(reason, "archive") ||
        has(reason, "escort") || has(reason, "festival purse"))
        return "reward";
    if (has(reason, "stake") || has(reason, "winnings") || has(reason, "table") || has(reason, "game won"))
        return "stakes";
    if (has(reason, "refund") || has(reason, "returned"))
        return "refund";
    return "trade";
}

void Society::tally(const std::string& holder, const std::string& reason, std::int64_t made, std::int64_t sunk,
                    std::int64_t goodsMade, std::int64_t goodsSunk)
{
    auto& t = state_.staged.faucets[state_.budgetDay][holder.substr(0, 200)][reason.substr(0, 80)];
    t.made += made;
    t.sunk += sunk;
    t.goodsMade += goodsMade;
    t.goodsSunk += goodsSunk;
    // D16 (9.4): only a registered faucet counts towards the balance, so a coin from an unregistered path leaves
    // realBalanced() false: a test fails on it while play only logs it (it is still paid and tallied for the report).
    const bool registered = registeredFaucet(reason);
    if (registered)
        faucetTotal_ += made;
    else
        unregisteredMade_ += made;
    sinkTotal_ += sunk;
    if (stagedChanged_.size() < 65536)
        stagedChanged_.insert({state_.budgetDay, holder.substr(0, 200)});
    // D16: a faucet by a reason in no group the door knows is logged (once a day a reason), never refused. The reasons are
    // the call sites' own words, so the reason names where it came from.
    if ((made > 0 || goodsMade > 0) && !registered)
        if (const auto key = std::to_string(state_.budgetDay) + "|" + reason.substr(0, 80);
            doorNoted_.size() < 4096 && doorNoted_.insert(key).second && doorNotes_.size() < 256)
            doorNotes_.push_back("an unregistered faucet path, \"" + reason.substr(0, 80) + "\": " + std::to_string(made) + "p and " +
                                 std::to_string(goodsMade) + "p of goods to " + holder.substr(0, 80));
}

bool Society::registeredFaucet(const std::string& reason)
{
    // The faucets the door knows (9.3): every group but the catch-all, and the trade group's own words (sales and
    // purchases, wages and shares, chests, stalls and letters, finds in a fight, gifts and help, and the DM's own
    // transfers, which the Director journals as "operator transfer").
    if (faucetGroup(reason) != "trade")
        return true;
    for (const char* word : {"trade", "sale", "sold", "sell", "bought", "buy", "market", "wage", "pay", "share", "rent", "fee",
                             "chest", "stall", "enclosure", "letter", "picked up", "knocked loose", "gift", "help", "alms",
                             "restitution", "inheritance", "lent", "hunt", "goods", "price", "the odd pennies", "faction",
                             "project", "dividend", "profit", "provisions", "carriage", "courier", "inn", "meal", "food",
                             "operator transfer"})
        if (has(reason, word))
            return true;
    return false;
}

std::vector<std::string> Society::takeDoorNotes()
{
    std::vector<std::string> out;
    out.swap(doorNotes_);
    if (doorNoted_.size() > 2048)
        doorNoted_.clear();
    return out;
}

std::vector<std::pair<std::int64_t, std::string>> Society::takeStagedChanged()
{
    std::vector<std::pair<std::int64_t, std::string>> out(stagedChanged_.begin(), stagedChanged_.end());
    stagedChanged_.clear();
    return out;
}

std::int64_t Society::faucetToday(const std::string& holder, const std::string& group) const
{
    const auto day = state_.staged.faucets.find(state_.budgetDay);
    if (day == state_.staged.faucets.end())
        return 0;
    const auto of = day->second.find(holder);
    if (of == day->second.end())
        return 0;
    std::int64_t n = 0;
    for (const auto& [reason, t] : of->second)
        if (faucetGroup(reason) == group)
            n += group == "stakes" ? t.made - t.sunk : t.made + t.goodsMade;
    return std::max<std::int64_t>(0, n);
}

std::int64_t Society::faucetRoom(const std::string& player, const std::string& cap) const
{
    const std::int64_t most = statusTable().cap(cap);
    const std::string group = cap == "theftADay" ? "theft" : cap == "stakesADay" ? "stakes" : cap == "rewardsADay" ? "reward"
                              : cap == "grantsADay" ? "grant" : cap;
    return std::max<std::int64_t>(0, most - faucetToday(holderOf(player), group));
}

void Society::topUp(const std::string& payer, const std::string& payee, std::int64_t coins)
{
    // Staged payers topped up on demand (9.2): a staged account that must pay a player (or escrow) more than it holds is
    // set to the amount first. Never a dead account, never a real one, and never a shop's till: a till pays players from
    // its own daily budget, topped up only by Phase 1's rules (its take allowance and sell total, doc 79, 7 and 19).
    if (coins <= 0 || classify(payer) != AccountKind::Staged || payer == "made" || startsWith(payer, "till:"))
        return;
    const auto to = classify(payee);
    if (to != AccountKind::Real && to != AccountKind::Escrow)
        return;
    const auto* a = account(payer);
    if (!a || a->cash >= coins || coins > MoneyCap)
        return;
    topUps_ += coins - a->cash;
    stagedSet(payer, coins);
}

std::int64_t Society::payable(const std::string& payer, std::int64_t amount) const
{
    if (amount <= 0)
        return 0;
    // (A till is never topped up by the door (topUp): it pays what it holds, as topUp says.)
    if (classify(payer) == AccountKind::Staged && account(payer) && !startsWith(payer, "till:"))
        return std::min<std::int64_t>(amount, MoneyCap);
    return std::min(amount, cash(payer));
}

std::int64_t Society::cappedPayout(const std::string& escrow, const std::string& player, std::int64_t coins, const std::string& cap,
                                   const std::string& back)
{
    // An escrow's payout to a player under a daily cap (9.3: festival prizes, contract, bounty and escort rewards): its real
    // coins whole, its staged-sourced ones (paid first, as the door counts them) only within the holder's room today. The
    // rest goes back to the staged side first, so the door counts only what the player gets.
    if (coins <= 0 || classify(escrow) != AccountKind::Escrow || classify(player) != AccountKind::Real)
        return coins;
    const auto src = state_.staged.escrowSource.find(escrow);
    const std::int64_t fromStaged = src == state_.staged.escrowSource.end() ? 0 : std::min(coins, src->second);
    const std::int64_t over = fromStaged - std::min(fromStaged, faucetRoom(player, cap));
    if (over <= 0)
        return coins;
    const auto to = !back.empty() && account(back) && classify(back) == AccountKind::Staged ? back : std::string("treasury");
    if (account(to))
        shift(escrow, to, "", 0, over, "over the day's cap: returned");
    return coins - over;
}

void Society::shareStakes(const std::string& pot, const std::vector<std::string>& seats, const std::string& house)
{
    const auto* a = account(pot);
    const auto coins = a ? a->cash : 0;
    if (coins <= 0 || seats.empty())
        return;
    const auto share = coins / std::int64_t(seats.size());
    auto order = seats;
    std::stable_partition(order.begin(), order.end(), [&](const std::string& who) { return classify(who) != AccountKind::Real; });
    for (const auto& who : order)
        if (share > 0)
            if (const auto paid = cappedPayout(pot, who, share, "stakesADay", house); paid > 0)
                shift(pot, who, "", 0, paid, "stakes returned");
    if (const auto* left = account(pot); left && left->cash > 0)
        shift(pot, house, "", 0, left->cash, "the house's penny");
}

void Society::stagedDoor(const std::string& kind, const std::string& from, const std::string& to, const std::string& item,
                         int quantity, std::int64_t coins)
{
    const auto fk = classify(from), tk = classify(to);
    const bool fromStaged = fk == AccountKind::Staged || fk == AccountKind::Dead;
    const bool toStaged = tk == AccountKind::Staged || tk == AccountKind::Dead;
    auto& st = state_.staged;
    const auto day = state_.budgetDay;
    const auto& table = statusTable();
    // Coins: always from `from` to `to` (a sale's buyer pays its seller; a shift moves both the same way).
    if (coins > 0)
    {
        if (fromStaged && tk == AccountKind::Real)
            tally(holderOf(to), kind, coins, 0, 0, 0);
        else if (fk == AccountKind::Real && toStaged)
            tally(holderOf(from), kind, 0, coins, 0, 0);
        else if (fromStaged && tk == AccountKind::Escrow)
            st.escrowSource[to] += coins;
        else if (fk == AccountKind::Escrow)
        {
            // Escrow pays its staged-sourced coins first: a faucet when they reach a player, nothing counted twice when
            // they go back to the staged side (only the real part is a sink then).
            std::int64_t part = 0;
            if (const auto src = st.escrowSource.find(from); src != st.escrowSource.end())
            {
                part = std::min(coins, src->second);
                if ((src->second -= part) <= 0)
                    st.escrowSource.erase(src);
            }
            if (tk == AccountKind::Real && part > 0)
                tally(holderOf(to), kind, part, 0, 0, 0);
            else if (toStaged && coins - part > 0)
                tally(from, kind, 0, coins - part, 0, 0);
            else if (tk == AccountKind::Escrow && part > 0)
                st.escrowSource[to] += part;
        }
    }
    // Goods (for the report): a sale's go the other way from its coins; anything else's from `from` to `to`.
    if (!item.empty() && quantity > 0)
    {
        const bool sale = coins > 0;
        const auto& gFrom = sale ? to : from;
        const auto& gTo = sale ? from : to;
        const auto gf = sale ? tk : fk, gt = sale ? fk : tk;
        const auto worth = catalogPrice(item) * quantity;
        const bool gfStaged = gf == AccountKind::Staged || gf == AccountKind::Dead;
        const bool gtStaged = gt == AccountKind::Staged || gt == AccountKind::Dead;
        if (gfStaged && gt == AccountKind::Real)
            tally(holderOf(gTo), kind, 0, 0, worth, 0);
        else if (gf == AccountKind::Real && gtStaged)
            tally(holderOf(gFrom), kind, 0, 0, 0, worth);
        else if (startsWith(gFrom, "bandits:") && gt == AccountKind::Real)
            tally(holderOf(gTo), kind, 0, 0, worth, 0);   // (A camp's hoard holds only staged goods: the cargo of the wagons it raided, 8.1.)
    }
    // What the bands and purses read of it: a theft (the victim robbed, lean, held), a player's gift or pay (help, earning,
    // a hold), a prize or a return from escrow (a hold).
    const auto resident = [&](const std::string& id) { return state_.residents.count(id) > 0; };
    const auto hold = [&](const std::string& id, int days) {
        if (resident(id) && days > 0)
            st.holds[id] = std::max(st.holds[id], day + days);
    };
    if (kind == "stolen")
    {
        if (resident(from))
        {
            noteStatusEvent(from, "robbed");
            if (table.leanDays > 0)
                st.lean[from] = std::max(st.lean[from], day + table.leanDays);
            hold(from, table.holdOneOff);
        }
        hold(to, table.holdOneOff);
        return;
    }
    // (Earning from players is steady pay only (6.3, 6.7): companion and camp (Hold) wages, lodging and Chapter rent,
    // repairs. Paid into a shop's till, the till's post-holder takes the event and the steady hold. A purchase, its odd
    // pennies, a stall sale or a surcharge is not earning, whoever it pays.)
    static const std::set<std::string> steadyPay{"companion wage", "camp wage", "wages", "rent", "chapter rent", "repair"};
    const bool steady = steadyPay.count(kind) > 0;
    std::string earner;
    if (fk == AccountKind::Real && coins > 0 && item.empty() && steady && startsWith(to, "till:"))
        if (const auto held = state_.careers.positions.find(to.substr(5)); held != state_.careers.positions.end())
            earner = held->second.holder;
    if (fk == AccountKind::Real && resident(to))
    {
        const auto worth = coins + (item.empty() ? 0 : catalogPrice(item) * quantity);
        if (has(kind, "gift") || has(kind, "help") || has(kind, "alms"))
        {
            noteStatusEvent(to, "helped", std::min(table.event("helpedMost"), table.event("helped") + double(worth) / 100.));
            hold(to, table.holdOneOff);
        }
        else if (coins > 0 && steady)
            earner = to;
    }
    else if (fk == AccountKind::Escrow && resident(to) && coins > 0)
        hold(to, table.holdOneOff);
    else if (resident(from) && tk == AccountKind::Real && coins > 0)
        hold(from, table.holdOneOff);
    if (!earner.empty() && resident(earner))
    {
        noteStatusEvent(earner, "earning", std::min(table.event("earningMost"), table.event("earning") + double(coins) / 100.));
        hold(earner, table.holdSteady);
    }
}

std::int64_t Society::realTotal() const
{
    std::int64_t n = 0;
    for (const auto& [id, a] : state_.accounts)
        if (const auto k = classify(id); k == AccountKind::Real)
            n += a.cash;
        else if (k == AccountKind::Escrow)
        {
            const auto src = state_.staged.escrowSource.find(id);
            n += a.cash - (src == state_.staged.escrowSource.end() ? 0 : std::min(a.cash, src->second));
        }
    return n;
}

void Society::openRealBalance()
{
    realOpening_ = realTotal();
    faucetTotal_ = sinkTotal_ = unregisteredMade_ = 0;
}

bool Society::realBalanced() const
{
    return realOpening_ < 0 || realTotal() == realOpening_ + faucetTotal_ - sinkTotal_;
}

bool Society::leanNow(const std::string& account) const
{
    const auto found = state_.staged.lean.find(account);
    return found != state_.staged.lean.end() && found->second > state_.budgetDay;
}

bool Society::heldNow(const std::string& account) const
{
    const auto found = state_.staged.holds.find(account);
    return found != state_.staged.holds.end() && found->second > state_.budgetDay;
}

void Society::noteStatusEvent(const std::string& resident, const std::string& kind, double weight)
{
    if (!state_.residents.count(resident) || kind.empty() || kind.size() > 40)
        return;
    if (weight == 0)
        weight = statusTable().event(kind);
    auto& list = state_.staged.events[resident];
    // (One of a kind a day; at most 12 kept, the oldest going first.)
    for (auto& e : list)
        if (e.kind == kind && e.day == state_.budgetDay)
        {
            e.weight = weight > 0 ? std::max(e.weight, weight) : std::min(e.weight, weight);
            return;
        }
    list.push_back({kind, state_.budgetDay, weight});
    if (list.size() > 12)
        list.erase(list.begin());
}

// ---- The status table ----

const status::Table& Society::statusTable() const
{
    return statusOverride_ ? *statusOverride_ : status::defaults();
}

void Society::setStatusOverride(const json::Value* v, std::vector<std::string>* warnings)
{
    if (!v || v->isNull())
        statusOverride_.reset();
    else
        statusOverride_ = std::make_shared<const status::Table>(status::read(*v, status::defaults(), warnings));
}

// ---- The bands (6.2-6.4) ----

const Society::Status* Society::statusOf(const std::string& id) const
{
    const auto found = status_.find(id);
    return found == status_.end() ? nullptr : &found->second;
}

std::string Society::householdOf(const std::string& id) const
{
    const auto found = status_.find(id);
    return found == status_.end() ? std::string() : found->second.household;
}

void Society::stagedBands(std::int64_t day, bool now)
{
    // (A test's or the DM's pass on its own: today's households first.)
    if (stagedDay_ != day)
        stagedHomesFor(day);
    const auto& t = statusTable();
    auto& st = state_.staged;
    // (A world's first bands: the report, made before them, is worked out again: World::tendRoads.)
    struct FirstBands
    {
        Society& s;
        bool first;
        ~FirstBands()
        {
            if (first && !s.state_.staged.bands.empty())
                s.reportStale_ = true;
        }
    } firstBands{*this, st.bands.empty()};
    // The slider the bands read (the Board's one), its day's step if the dawn hasn't taken it; "now" goes at once.
    stepSlider(day, now);
    // Who leads a House and their kin (the head's spouse, parents and children, and family at home), the post-holders now,
    // and who is mourning kin (each a fact of the day).
    std::set<std::string> houseKin;
    {
        std::unordered_map<std::string, std::vector<const std::string*>> byHome, childrenOf;
        for (const auto& [id, life] : state_.residents)
            byHome[life.homeCell].push_back(&id);
        for (const auto& [child, parents] : state_.careers.parents)
            for (const auto& parent : parents)
                childrenOf[parent].push_back(&child);
        for (const auto& h : houses())
        {
            const auto held = state_.careers.positions.find(h.head);
            if (held == state_.careers.positions.end() || held->second.holder.empty())
                continue;
            const auto& head = held->second.holder;
            houseKin.insert(head);
            if (const auto sp = state_.careers.spouses.find(head); sp != state_.careers.spouses.end())
                houseKin.insert(sp->second);
            if (const auto ps = state_.careers.parents.find(head); ps != state_.careers.parents.end())
                houseKin.insert(ps->second.begin(), ps->second.end());
            if (const auto cs = childrenOf.find(head); cs != childrenOf.end())
                for (const auto* c : cs->second)
                    houseKin.insert(*c);
            if (const auto life = state_.residents.find(head); life != state_.residents.end())
                if (const auto at = byHome.find(life->second.homeCell); at != byHome.end() && !life->second.homeCell.empty())
                    for (const auto* r : at->second)
                        if (family(*r, head))
                            houseKin.insert(*r);
        }
    }
    // A post's talk category, once per kind of post (talk::jobCategory reads many words; it reads the age only as under 15).
    if (jobCategories_.size() > 65536)
        jobCategories_.clear();
    const auto category = [&](const std::string& m) -> const std::string& {
        const auto* s = spec(m);
        const auto* post = jobOf(m);
        const auto& title = post ? post->title : std::string();
        const int age = s ? s->age : 30;
        const std::string key = title + '\x1f' + (s ? s->workLabel : std::string()) + '\x1f' + (s ? s->role : std::string()) +
                                (age < 15 ? "\x1f" "y" : "\x1f" "a");
        auto found = jobCategories_.find(key);
        if (found == jobCategories_.end())
            found = jobCategories_.emplace(key, talk::jobCategory(title, s ? s->workLabel : std::string(), s ? s->role : std::string(), age)).first;
        return found->second;
    };
    ClassPass pass;
    std::set<std::string> posted;
    // An earner: a post that pays, or one of the Watch (a guard by its post or by what it was written as: DEV's patrol
    // guards placed with no work spot load with no post), whom its town pays, not the market (doc 79, 10).
    const auto earner = [&](const std::string& id) {
        const auto* post = jobOf(id);
        if (post && !postClass(*post, &pass).empty())
            return true;
        const auto* s = spec(id);
        return s && s->ofTheWatch();
    };
    for (const auto& [id, life] : state_.residents)
        if (!dead_.count(id) && earner(id))
            posted.insert(id);
    // A lost post: held at the last pass, held no longer, alive and not yet of an age to retire.
    for (const auto& id : postedAtPass_)
        if (!posted.count(id) && state_.residents.count(id) && !dead_.count(id))
            if (const auto* s = spec(id); s && s->age < retireAge(id))
                noteStatusEvent(id, "lostPost");
    postedAtPass_ = posted;
    for (const auto& [id, m] : state_.careers.mourning)
        if (m.until > double(day) && family(id, m.whom))
        {
            bool noted = false;
            if (const auto found = st.events.find(id); found != st.events.end())
                for (const auto& e : found->second)
                    noted |= e.kind == "bereaved" && day - e.day < 7;
            if (!noted)
                noteStatusEvent(id, "bereaved");
        }
    // Each household a candidate.
    std::vector<status::Candidate> list;
    std::vector<const std::string*> keys;
    std::map<std::string, int> times;
    const bool neverBroke = ownRule("neverBroke") > 0;
    for (auto& [key, home] : stagedHomes_)
    {
        status::Candidate c;
        c.key = key;
        const auto& members = home.members;
        const bool homeless = startsWith(key, "|");
        std::string homeCell;
        if (!members.empty())
            if (const auto found = state_.residents.find(members.front()); found != state_.residents.end())
                homeCell = found->second.homeCell;
        // (The Board's market, not the Watch's town (lawTown): the two phases name markets alike, whatever the towns.)
        c.market = marketOfHome(homeCell, members);
        home.market = c.market;
        if (!times.count(c.market))
            times[c.market] = hooks_.times ? std::clamp(hooks_.times(*this, c.market), -2, 2) : 0;
        // The score: class, home, earners, fortune, district, events, Times.
        const std::string cls = home.cls == "homeless" ? std::string("lower") : home.cls;
        const auto kind = homeless ? std::string("room") : status::homeKind(t, homeCell);
        int earners = 0;
        for (const auto& m : members)
            earners += posted.count(m) ? 1 : 0;
        double score = 0;
        if (const auto f = t.classScore.find(cls); f != t.classScore.end())
            score += f->second;
        if (const auto f = t.homeScore.find(kind); f != t.homeScore.end())
            score += f->second;
        score += earners == 0 ? t.noEarner : earners == 1 && int(members.size()) >= t.thinMouths ? t.thinEarner : 0;
        score += status::fortune(t, key, day);
        // The district: the quarter most of the household lives in, by the voice table (a label a host removed adds 0).
        std::map<std::string, int> quarter;
        bool servantsOnly = !members.empty();
        for (const auto& m : members)
        {
            if (const auto q = quarters_.find(m); q != quarters_.end())
                ++quarter[q->second];
            if (category(m) != "servant")
                servantsOnly = false;
        }
        std::string district;
        int most = 0;
        for (const auto& [q, k] : quarter)
            if (k > most)
                district = q, most = k;
        if (const auto f = t.districtScore.find(district); f != t.districtScore.end())
            score += f->second;
        // Events in the last eventDays (helped and earning each capped).
        bool reason = earners == 0 || homeless || homeCell.empty();
        double helped = 0, earning = 0;
        for (const auto& m : members)
            if (const auto found = st.events.find(m); found != st.events.end())
                for (const auto& e : found->second)
                {
                    if (day - e.day >= t.eventDays || e.day > day)
                        continue;
                    c.recent = true;
                    if (e.kind == "helped")
                        helped += e.weight * (1 - double(day - e.day) / t.eventDays);   // (Fading over the days.)
                    else if (e.kind == "earning")
                        earning += e.weight;
                    else
                        score += e.weight;
                    reason |= e.kind == "lostPost" || e.kind == "bereaved";
                }
        score += std::min(helped, t.event("helpedMost")) + std::min(earning, t.event("earningMost"));
        score += t.times[std::size_t(times[c.market] + 2)];
        c.score = score;
        c.reason = reason;
        c.implied = cls == "upper" ? status::Rich : cls == "middle" ? status::Comfortable : status::Poor;
        if (kind == "room")
            c.implied = std::max(0, c.implied - 1);
        else if (kind == "house" && cls == "lower")
            c.implied = status::Comfortable;
        if (homeless)
            c.implied = status::Destitute;
        // The guarantees, in their order (6.3).
        bool beggar = false, house = false, watch = false;
        int authored = -1;                          // (A member's authored band, the highest of them: doc 79, 14; Phase 9.)
        for (const auto& m : members)
        {
            beggar |= category(m) == "beggar";
            house |= houseKin.count(m) > 0;
            if (const auto* s = spec(m))
                watch |= s->ofTheWatch(), authored = std::max(authored, s->band);
        }
        // The DM's timed override first, then the band the host authored for a member (the Band picker: standing, for the
        // whole household), then the guarantees and the score.
        // (In force before its day `until`: a band for N days from today is N days, the report's and the DM tool's.)
        if (const auto o = st.overrides.find(key); o != st.overrides.end() && o->second.until > day)
            c.fixed = std::clamp(o->second.band, 0, status::Bands - 1), c.overridden = true;
        else if (authored >= 0)
            c.fixed = std::min(authored, status::Bands - 1), c.overridden = true, c.reason = c.reason || authored == status::Destitute;
        else if (homeless || beggar)
            c.fixed = status::Destitute, c.reason = true;
        else if (house)
            c.fixed = status::Rich;
        else if (district == "sump")
            c.atMost = status::Poor;
        else if ((district == "heights" || district == "estate") && !servantsOnly)
            c.atLeast = status::Comfortable;
        // A household with one of the Watch in it is paid by its town (doc 79, 10: the Watch stays as it is): never poor
        // or destitute; only the DM's override sets it lower (and even then it never begs, sleeps rough or takes the dole).
        if (watch && !c.overridden)
        {
            if (c.fixed >= 0 && c.fixed < status::Comfortable)
                c.fixed = -1;
            if (c.fixed < 0)
                c.atLeast = std::max(c.atLeast, int(status::Comfortable)), c.atMost = std::max(c.atMost, int(status::Comfortable));
        }
        if (const auto b = st.bands.find(key); b != st.bands.end())
        {
            c.previous = b->second.band;
            c.since = b->second.since;
            c.lifted = b->second.liftedUntil >= day;
        }
        home.lifted = c.lifted;
        list.push_back(std::move(c));
        keys.push_back(&key);
    }
    status::CutOptions options;
    options.slider = prosperity();
    options.times = times;
    options.neverBroke = neverBroke;
    options.day = day;
    options.now = now;
    options.keepPrevious = restoring_;
    const auto bands = status::assign(t, list, options);
    // Saved {band, since, liftedUntil}; the households gone, dropped. Each member's status cached.
    std::map<std::string, StagedState::Band> saved;
    status_.clear();
    for (std::size_t i = 0; i < list.size(); ++i)
    {
        const auto& key = *keys[i];
        auto& home = stagedHomes_.at(key);
        StagedState::Band b;
        if (const auto was = st.bands.find(key); was != st.bands.end())
            b = was->second;
        if (b.band != bands[i] || st.bands.find(key) == st.bands.end())
            b.since = day;
        b.band = bands[i];
        home.band = bands[i];
        if (!restoring_)
            saved[key] = b;
        for (const auto& m : home.members)
        {
            Status s;
            s.band = bands[i];
            s.cls = home.cls;
            s.household = key;
            s.market = list[i].market;
            s.score = list[i].score;
            s.overridden = list[i].overridden;
            s.guaranteed = list[i].fixed >= 0 || list[i].atMost < status::Rich || list[i].atLeast > status::Destitute;
            s.reason = list[i].reason;
            s.liftedUntil = b.liftedUntil;
            status_[m] = std::move(s);
        }
    }
    if (!restoring_)
        st.bands = std::move(saved);
    stagedKeepers();
}

void Society::stagedJoined(const std::string& id, const std::vector<std::string>& kin)
{
    // A born child or an arrival placed in a post (6.5) takes its household's band at once: a parent's (a child under 16
    // is in its parents' household), else that of those it lodges with (not in a communal home: alone there). Alone, the
    // bands are worked out again at the next hour.
    if (stagedDay_ < 0)
        return;
    const auto life = state_.residents.find(id);
    if (life == state_.residents.end())
        return;
    std::string peer;
    for (const auto& k : kin)
        if (k != id && status_.count(k))
        {
            peer = k;
            break;
        }
    if (const auto& home = life->second.homeCell; peer.empty() && !home.empty() && !communalHome(home))
        for (const auto& [other, l] : state_.residents)
            if (other != id && l.homeCell == home && status_.count(other))
            {
                peer = other;
                break;
            }
    if (peer.empty())
    {
        stagedRecount_ = true;
        return;
    }
    const auto s = status_.at(peer);
    status_[id] = s;
    if (const auto h = stagedHomes_.find(s.household); h != stagedHomes_.end())
        if (std::find(h->second.members.begin(), h->second.members.end(), id) == h->second.members.end())
            h->second.members.push_back(id);
}

bool Society::overrideBand(const std::string& household, int band, std::int64_t until)
{
    if (household.empty() || household.size() > 200)
        return false;
    if (band < 0)
        state_.staged.overrides.erase(household);
    else
        state_.staged.overrides[household] = {std::clamp(band, 0, status::Bands - 1), until};
    if (stagedDay_ >= 0)
        stagedBands(stagedDay_);                    // (Today's pass again: the override at once, nothing else moves.)
    return true;
}

bool Society::liftHousehold(const std::string& household, std::int64_t until)
{
    const auto found = state_.staged.bands.find(household);
    if (found == state_.staged.bands.end())
        return false;
    found->second.liftedUntil = std::max(found->second.liftedUntil, until);
    if (stagedDay_ >= 0)
        stagedBands(stagedDay_);
    return true;
}

// ---- Food from the shelf (6.5) ----

int Society::tillShelf(Society& s, const std::string& till, const std::string& good, int want)
{
    const auto found = s.state_.accounts.find(till);
    if (found == s.state_.accounts.end() || want <= 0)
        return 0;
    const int got = std::min(want, stock(found->second, good));
    if (got > 0 && (found->second.stock[good] -= got) == 0 && !keptAtZero(good))
        found->second.stock.erase(good);
    return std::max(0, got);
}

void Society::stagedKeepers()
{
    // Each household's food shop (a merchant's till that sets the band's food out, in its market; by a fixed lot among
    // them) and the food it gives: the first of the band's foods sold there. None: food made (fed from elsewhere). A
    // till sells a food only if the morning restock fills it (stagedShelfGoods, what stagedRestock sets out): a till
    // that only holds some (a starting stock, leftovers, a buy) is never chosen, as nothing would refill it.
    std::map<std::string, std::map<std::string, std::vector<std::string>>> sellers;   // Food -> market -> tills.
    const auto& t = statusTable();
    std::set<std::string> foods;
    for (const auto& row : t.rows)
        for (const auto& f : row.food)
            if (itemValid(f))
                foods.insert(f);
    for (const auto& p : positions_)
    {
        if (p.role != "merchant")
            continue;
        const auto held = state_.careers.positions.find(p.id);
        const auto till = "till:" + p.id;
        const auto* a = account(till);
        if (held == state_.careers.positions.end() || held->second.holder.empty() || !a)
            continue;
        const auto market = marketOf(till);
        const auto sold = spec(p.founder) ? stagedShelfGoods(p.founder) : std::vector<std::string>{};
        for (const auto& f : foods)
            if (std::find(sold.begin(), sold.end(), f) != sold.end())
                sellers[f][market].push_back(till);
    }
    for (auto& [key, home] : stagedHomes_)
    {
        home.till.clear();
        home.food.clear();
        const auto& row = t.rows[std::size_t(std::clamp(home.band, 0, status::Bands - 1))];
        for (const auto& f : row.food)
        {
            if (!itemValid(f))
                continue;
            if (home.food.empty())
                home.food = f;                      // (What is made where no shop sells any.)
            const auto by = sellers.find(f);
            if (by == sellers.end())
                continue;
            const auto here = by->second.find(home.market);
            if (here == by->second.end() || here->second.empty())
                continue;
            home.food = f;
            home.till = here->second[status::hash(key + "|food") % here->second.size()];
            break;
        }
        if (home.food.empty())
            home.food = itemValid("meal") ? "meal" : "";
    }
    // What each food shop gives its households in a day (provisionFor, for its shelf target): each head's band's meals
    // (and tomorrow's buying ahead) in pieces of its food, at most the two days' (and ahead) a larder holds, as stagedFood.
    provisionDemand_.clear();
    const double tomorrow = double(stagedDay_ + 1) + .5;
    const int ahead = calendar::weekdayOf(tomorrow) == calendar::Restday || calendar::festivalDay(tomorrow) ? std::max(0, ownRule("buyAheadDays")) : 0;
    const double mealWorth = std::max(1, nourishment("meal"));
    for (const auto& [key, home] : stagedHomes_)
    {
        if (home.till.empty() || home.food.empty() || home.lifted)
            continue;
        const auto& row = t.rows[std::size_t(std::clamp(home.band, 0, status::Bands - 1))];
        const double each = std::max(1, nourishment(home.food));
        const double head = std::min(row.meals * mealWorth + ahead * NourishADay, (2 + ahead) * NourishADay);
        const int pieces = int(std::ceil(head / each - 1e-9)) * int(home.members.size());
        if (pieces > 0)
            provisionDemand_[home.till][home.food] = std::min(ShelfMost, provisionDemand_[home.till][home.food] + pieces);
    }
    streetKeepers();                                // (Doc 79, Phase 6: the day's wares, luxuries and services by market.)
}

int Society::provisionFor(const std::string& till, const std::string& ware) const
{
    const auto found = provisionDemand_.find(till);
    if (found == provisionDemand_.end())
        return 0;
    const auto food = found->second.find(ware);
    return food == found->second.end() ? 0 : food->second;
}

std::vector<std::pair<std::string, std::string>> Society::takeProvisions()
{
    std::vector<std::pair<std::string, std::string>> out;
    out.swap(provisions_);
    return out;
}

// ---- The goods trim and the prunes (9.2) ----

void Society::stagedTrim(const std::string& id, int band)
{
    const auto found = state_.accounts.find(id);
    if (found == state_.accounts.end())
        return;
    auto& wallet = found->second;
    const auto& row = statusTable().rows[std::size_t(std::clamp(band, 0, status::Bands - 1))];
    // Food above two days' (more the night before a Restday or a festival) goes, the most-held kinds first.
    const double today = double(state_.budgetDay) + .5;
    const int ahead = calendar::weekdayOf(today) == calendar::Restday || calendar::festivalDay(today) ? std::max(0, ownRule("buyAheadDays")) : 0;
    const double most = (2 + ahead) * NourishADay;
    double have = 0;
    std::vector<std::pair<int, std::string>> foods;   // (Nourishment, item): the richest dropped first.
    for (const auto& [item, n] : wallet.stock)
        if (n > 0 && nourishment(item) > 0)
        {
            have += double(nourishment(item)) * n;
            if (items::makerOf(item).empty())
                foods.push_back({nourishment(item), item});
        }
    std::sort(foods.rbegin(), foods.rend());
    for (const auto& [each, item] : foods)
    {
        if (have <= most + 1e-9)
            break;
        const int drop = std::min(stock(wallet, item), int(std::floor((have - most) / each + 1e-9)));
        if (drop <= 0)
            continue;
        if ((wallet.stock[item] -= drop) == 0 && !keptAtZero(item))
            wallet.stock.erase(item);
        have -= double(drop) * each;
    }
    // Other goods above what the band holds (never something worn or a maker's marked piece: those are its own).
    for (auto it = wallet.stock.begin(); it != wallet.stock.end();)
    {
        const auto& [item, n] = *it;
        const bool keep = nourishment(item) > 0 || items::wearable(item) || !items::makerOf(item).empty() || item == "sword";
        if (!keep && n > row.goodsMost)
        {
            it->second = row.goodsMost;
            if (it->second == 0 && !keptAtZero(item))
            {
                it = wallet.stock.erase(it);
                continue;
            }
        }
        ++it;
    }
}

void Society::stagedPrune(std::int64_t day)
{
    auto& st = state_.staged;
    const auto& t = statusTable();
    for (auto it = st.events.begin(); it != st.events.end();)
    {
        auto& list = it->second;
        list.erase(std::remove_if(list.begin(), list.end(), [&](const StagedState::Event& e) { return day - e.day >= t.eventDays; }),
                   list.end());
        it = list.empty() || !state_.residents.count(it->first) ? st.events.erase(it) : std::next(it);
    }
    for (auto* m : {&st.holds, &st.lean})
        for (auto it = m->begin(); it != m->end();)
            it = it->second <= day || !state_.accounts.count(it->first) ? m->erase(it) : std::next(it);
    for (auto it = st.faucets.begin(); it != st.faucets.end();)
        it = day - it->first >= 28 ? st.faucets.erase(it) : std::next(it);
    for (auto it = st.overrides.begin(); it != st.overrides.end();)
        it = it->second.until <= day ? st.overrides.erase(it) : std::next(it);
    // (Street life, Phase 6: a dole more than a week old, and fed fame's weeks gone by.)
    for (auto it = st.dole.begin(); it != st.dole.end();)
        it = day - it->second >= 7 ? st.dole.erase(it) : std::next(it);
    for (auto it = st.fed.begin(); it != st.fed.end();)
        it = day - it->second >= 7 ? st.fed.erase(it) : std::next(it);
    for (auto it = st.welcomed.begin(); it != st.welcomed.end();)
        it = day / std::max<std::int64_t>(1, t.cap("welcomeDays")) - it->second > 1 ? st.welcomed.erase(it) : std::next(it);
    for (auto it = st.escrowSource.begin(); it != st.escrowSource.end();)
    {
        const auto* a = account(it->first);
        if (!a || a->cash <= 0)
            it = st.escrowSource.erase(it);
        else
        {
            it->second = std::min(it->second, a->cash);
            ++it;
        }
    }
}

// ---- At restore (9.2) ----

void Society::stagedRestored()
{
    status_.clear();
    stagedHomes_.clear();
    stagedDay_ = -1;
    stagedHour_ = -1;
    postedAtPass_.clear();
    runnerWaiting_.clear();                         // (Doc 79, Phase 6: a buy waiting or a runner walking isn't saved.)
    foodDeferred_.clear();
    fedOn_.clear();
    buysDeferred_.clear();
    openRealBalance();
    if (roster_ != Roster::Authored)
        return;
    // The staged hour as saved, so a restart across 0:00 still runs that night's set (Phase 0's open item 10); the bands
    // worked out from the saved state, each keeping its saved band, so a restart never makes them jump.
    const auto& st = state_.staged;
    stagedHour_ = st.hour;
    const auto day = st.day >= 0 ? st.day : state_.budgetDay;
    restoring_ = true;
    stagedHouseholds(day);
    restoring_ = false;
    stagedDay_ = st.day >= 0 ? st.day : -1;        // (A save from before: the next hour turns the day.)
    // Staged accounts out of range settled (a purse over twice the most any band holds, 500p: an old save's, or a host's
    // edit); saved budgets, holds and lean days kept; real and escrow untouched. (In range, a purse drifts nightly.)
    const auto& t = statusTable();
    for (const auto& [key, home] : stagedHomes_)
        for (const auto& id : home.members)
            if (const auto* a = account(id); a && !heldNow(id) && a->cash > 2 * 500)
                stagedSet(id, stagedPurse(id, key, home.cls, day));
    for (const auto& [id, a] : std::map<std::string, EconomyAccount>(state_.accounts))
        if (id == "treasury" || startsWith(id, "stores:"))
            if (a.cash < t.treasuryFloor || a.cash > t.treasuryCeiling)
                stagedSet(id, std::clamp(a.cash, t.treasuryFloor, t.treasuryCeiling));
    openRealBalance();
}

// ---- Pay words and the Mind's Means (11) ----

std::int64_t Society::payWord(const std::string& id) const
{
    const auto* post = jobOf(id);
    if (!post || postClass(*post).empty())
        return 0;
    auto kind = postKind(*post).kind;
    const auto& pay = statusTable().pay;
    auto found = pay.find(kind);
    if (found == pay.end())
        found = pay.find("hand");
    const std::int64_t base = found == pay.end() ? 8 : found->second;
    return std::max<std::int64_t>(1, std::llround(double(base) * (1 + prosperity() / 400)));
}

std::string Society::meansNote(const std::string& id, const std::string& holder) const
{
    const auto* s = statusOf(id);
    if (!s)
        return {};
    static const char* const GetsBy[] = {"has next to nothing and goes hungry some days", "gets by week to week",
                                         "gets by well enough", "wants for nothing"};
    std::string note = std::string("Means: ") + status::bandName(s->band) + "; " + GetsBy[std::clamp(s->band, 0, 3)];
    if (const auto pay = payWord(id); pay > 0)
        note += "; pay about " + pence(pay) + " a day";
    note += ".";
    if (hooks_.times && !s->market.empty())
    {
        static const char* const Times[] = {"hard", "lean", "ordinary", "good", "boom"};
        const int times = std::clamp(hooks_.times(*this, s->market), -2, 2);
        if (times != 0)
        {
            // (And why, in the words people use for it: the slider's cause, a harvest, the roads' troubles, the DM's
            // nudge: 8.5, Phase 7. Never a staged event's words: an event is no part of the Times, 5.3.)
            const auto cause = timesCause(s->market);
            note += " Times here: " + std::string(Times[times + 2]) + (cause.empty() ? std::string() : ", since " + cause) + ".";
        }
    }
    // The market's largest move in shown price, with that price and its cause (11, Phase 8: "Salt dear, four pence the
    // dozen: the wagon was robbed on the road."), so the Mind never invents a figure or a reason.
    // (Read off the speaker's own shop where it sets the good out, then a shop in its cell, at the player's list: Phase 8
    // repair.)
    const auto own = tillOf(id);
    const auto* post = jobOf(id);
    const auto nearer = [&](const std::string& till) {
        if (till == own)
            return 0;
        const auto* p = till.size() > 5 ? position(till.substr(5)) : nullptr;
        return p && post && p->work.cell == post->work.cell ? 1 : 2;
    };
    if (!s->market.empty())
        if (const auto moves = priceMoves(s->market, holder, 1, nearer); !moves.empty())
        {
            const auto& m = moves.front();
            std::string name = itemName(m.base);
            if (const auto cut = name.find(" ("); cut != std::string::npos)
                name.erase(cut);
            if (!name.empty())
                name[0] = char(std::toupper(static_cast<unsigned char>(name[0])));
            note += " " + name + (m.cause == "still" ? " still " : " ") + (m.trend > 0 ? "dear" : "cheap") + ", " +
                    priceWords(m.shown, m.listCount) + (m.cause == "still" ? ", " : ": ") + m.causeWords + ".";
        }
    return cutAtSentence(note, 300);
}

std::string Society::cutAtSentence(const std::string& words, std::size_t most)
{
    if (words.size() <= most)
        return words;
    const auto end = words.rfind(". ", most - 1);
    if (end != std::string::npos && end > 0)
        return words.substr(0, end + 1);
    const auto space = words.rfind(' ', most);
    return words.substr(0, space == std::string::npos ? most : space);
}
} // namespace ratw
