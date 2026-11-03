# Prototype rules for what-if runs only (never committed): each behind an environment variable, off by default.
import sys
p = sys.argv[1]
s = open(p).read()

def rep(old, new, count=1):
    global s
    assert s.count(old) == count, (old[:80], s.count(old))
    s = s.replace(old, new)

# Helpers.
rep("""double roll(const std::string& key)
{
    return double(fnv(key) % 1000003ULL) / 1000003.;
}
} // namespace""", """double roll(const std::string& key)
{
    return double(fnv(key) % 1000003ULL) / 1000003.;
}
// PROTOTYPE knobs (what-if runs): read once from the environment.
double knob(const char* name, double fallback)
{
    const char* v = std::getenv(name);
    return v && *v ? std::atof(v) : fallback;
}
std::unordered_map<std::string, double> protoDrawAvg;   // Till -> a running mean of its day's draws (RATW_P_FLOAT_DAYS).
std::unordered_map<std::string, std::int64_t> protoShort; // Till -> what it could not pay tonight (RATW_P_TREASURY_MODE=wages).
} // namespace""")
rep('#include <set>\n', '#include <set>\n#include <cstdlib>\n#include <unordered_map>\n')

# 1. The float: a running mean of the till's draws instead of yesterday's alone.
rep("""    drawnYesterday_ = std::move(drawnToday_);
    drawnToday_.clear();""", """    if (static const double days = knob("RATW_P_FLOAT_DAYS", 0); days > 0)
    {
        for (auto& [till, avg] : protoDrawAvg)
            avg -= avg / days;
        for (const auto& [till, coins] : drawnToday_)
            protoDrawAvg[till] += double(coins) / days;
    }
    drawnYesterday_ = std::move(drawnToday_);
    drawnToday_.clear();""")
rep("""        const auto drawn = drawnYesterday_.find(payer);
        return std::max(TillFloat, drawn == drawnYesterday_.end() ? 0 : drawn->second);""", """        if (static const double days = knob("RATW_P_FLOAT_DAYS", 0); days > 0)
        {
            const auto avg = protoDrawAvg.find(payer);
            return std::max(TillFloat, avg == protoDrawAvg.end() ? std::int64_t(0) : std::int64_t(avg->second * knob("RATW_P_FLOAT_SHARE", 1)));
        }
        const auto drawn = drawnYesterday_.find(payer);
        return std::max(TillFloat, drawn == drawnYesterday_.end() ? 0 : drawn->second);""")

# 2. Profit: the cap in days of wages, and a share of the excess to the hands.
rep("""        loopToday_.wagesOwed += owed;
        loopToday_.wagesPaid += paid;""", """        loopToday_.wagesOwed += owed;
        loopToday_.wagesPaid += paid;
        if (paid < owed && payer.rfind("till:", 0) == 0)
            protoShort[payer] = owed - paid;""")
rep("""            const auto cap = float_ + 7 * owed;
            if (const auto excess = cash(payer) - cap; held != state_.careers.positions.end() && !held->second.holder.empty() &&
                                                       excess > 0 && shift(payer, held->second.holder, "", 0, excess, "profit"))
                loopToday_.profit += excess;""", """            static const double capDays = knob("RATW_P_CAPDAYS", 7);
            static const double handShare = knob("RATW_P_HANDSHARE", 0) / 100.;
            const auto cap = float_ + std::int64_t(capDays * double(owed));
            if (auto excess = cash(payer) - cap; held != state_.careers.positions.end() && !held->second.holder.empty() && excess > 0)
            {
                // The hands' share, equally among them (the keeper is the till's own post).
                std::vector<const std::string*> hands;
                for (const auto& d : dues)
                    if (*d.holder != held->second.holder)
                        hands.push_back(d.holder);
                if (handShare > 0 && !hands.empty())
                {
                    const auto each = std::int64_t(double(excess) * handShare) / std::int64_t(hands.size());
                    for (const auto* h : hands)
                        if (each > 0 && shift(payer, *h, "", 0, each, "profit"))
                            loopToday_.profit += each, excess -= each;
                }
                if (excess > 0 && shift(payer, held->second.holder, "", 0, excess, "profit"))
                    loopToday_.profit += excess;
            }""")

# 3. Alms: the level a head.
rep("""        const auto want = AlmsADay * std::int64_t(members.size()) - held;""",
    """        static const auto level = std::int64_t(knob("RATW_P_ALMS", double(AlmsADay)));
        const auto want = level * std::int64_t(members.size()) - held;""")

# 4. The treasury over a cap: buys the city's goods (as the Country does) or pays the short tills' wages.
rep("""void Society::payWages()
{""", """void Society::treasuryWageSupport()
{
    // PROTOTYPE: the treasury over RATW_P_TREASURY_CAP pays what the short tills could not, from what is over the cap.
    static const auto cap = std::int64_t(knob("RATW_P_TREASURY_CAP", 0));
    std::int64_t room = cash("treasury") - cap;
    for (const auto& [till, short_] : protoShort)
    {
        if (room <= 0)
            break;
        // Shared over the till's posts as the till shares its own: each post the same share of its wage.
        std::vector<std::pair<std::string, int>> posts;
        int owed = 0;
        for (const auto& p : positions_)
            if (const auto held = state_.careers.positions.find(p.id); held != state_.careers.positions.end() && !held->second.holder.empty())
                if (const auto pay = payOf(p); pay.payer == till && pay.wage > 0)
                    posts.push_back({held->second.holder, pay.wage}), owed += pay.wage;
        const auto give = std::min(room, short_);
        for (const auto& [holder, wage] : posts)
            if (const auto coins = give * wage / std::max(1, owed); coins > 0 && shift("treasury", holder, "", 0, coins, "wage support"))
                room -= coins, loopToday_.wagesPaid += coins;
    }
    protoShort.clear();
}

void Society::payWages()
{""")
rep("""        payWages();
        giveAlms();""", """        payWages();
        if (const char* mode = std::getenv("RATW_P_TREASURY_MODE"); mode && std::string(mode) == "wages")
            treasuryWageSupport();
        protoShort.clear();
        giveAlms();""")
# The "buy" mode: the treasury as a second buyer of the Country's basket, with what is over its cap.
rep("""        const auto fund = marketFund(market.id);
        if (!account(fund) || market.basket.empty())
            continue;
        // What it owes its posts tonight stays in the purse with its reserve.
        std::int64_t owed = 0;
        for (const auto& p : positions_)
            if (const auto held = state_.careers.positions.find(p.id);
                held != state_.careers.positions.end() && !held->second.holder.empty())
                if (const auto pay = payOf(p); pay.payer == fund)
                    owed += pay.wage;
        const auto spare = cash(fund) - authored_.economy.countryReserve - owed;""", """        const char* mode = std::getenv("RATW_P_TREASURY_MODE");
        const bool city = protoTreasuryBuying_;
        const auto fund = city ? std::string("treasury") : marketFund(market.id);
        if (!account(fund) || market.basket.empty())
            continue;
        (void)mode;
        // What it owes its posts tonight stays in the purse with its reserve.
        std::int64_t owed = 0;
        for (const auto& p : positions_)
            if (const auto held = state_.careers.positions.find(p.id);
                held != state_.careers.positions.end() && !held->second.holder.empty())
                if (const auto pay = payOf(p); pay.payer == fund)
                    owed += pay.wage;
        const std::int64_t reserve = city ? std::int64_t(knob("RATW_P_TREASURY_CAP", 0)) : authored_.economy.countryReserve;
        const auto spare = cash(fund) - reserve - owed;""")
rep("""                if (cash(fund) - price * q - carriage < authored_.economy.countryReserve + owed)
                    q = int(std::max<std::int64_t>(0, (cash(fund) - authored_.economy.countryReserve - owed) / std::max<std::int64_t>(1, price + 1)));
                if (q < 1 || !sale(*till, fund, *w.item, q, price, "to the Country"))
                    continue;
                consume(fund, *w.item, q, "carried to the Country");""", """                if (cash(fund) - price * q - carriage < reserve + owed)
                    q = int(std::max<std::int64_t>(0, (cash(fund) - reserve - owed) / std::max<std::int64_t>(1, price + 1)));
                if (q < 1 || !sale(*till, fund, *w.item, q, price, city ? "public works" : "to the Country"))
                    continue;
                consume(fund, *w.item, q, city ? "used by the city" : "carried to the Country");
                if (city)
                {
                    loopToday_.subsidy += price * q;  // (PROTOTYPE: the city's purchases counted as its subsidy.)
                    continue;
                }""")
rep("""        if (hod == ShopShut - 1)
            loopToday_.countryUnspent""", """        if (city)
            continue;
        if (hod == ShopShut - 1)
            loopToday_.countryUnspent""")
rep("""        if (authored_.economy.country)
            countryBuys(hod);""", """        if (authored_.economy.country)
            countryBuys(hod);
        if (const char* mode = std::getenv("RATW_P_TREASURY_MODE"); mode && std::string(mode) == "buy")
        {
            protoTreasuryBuying_ = true;
            countryBuys(hod);
            protoTreasuryBuying_ = false;
        }""")
open(p, 'w').write(s)

h = sys.argv[2]
t = open(h).read()
old = "    void payWages();\n"
assert t.count(old) == 1
t = t.replace(old, old + "    void treasuryWageSupport();          // PROTOTYPE (what-if runs).\n    bool protoTreasuryBuying_ = false;   // PROTOTYPE.\n")
open(h, 'w').write(t)
print('ok')
