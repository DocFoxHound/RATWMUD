# PROTOTYPE (what-if only):
# RATW_P_SHELF_DAYS=N: a ware's shelf follows the larger of yesterday's sales and their running mean over N days (so a
#   Restday's few sales don't empty the next day's shelves);
# RATW_P_COMFORT_ITEMS=N: the most comforts a household buys a day (20 now);
# RATW_P_PROFIT_TAX=P: P percent of a till's profit goes to the treasury.
import sys
p = sys.argv[1]
s = open(p).read()

def rep(old, new):
    global s
    assert s.count(old) == 1, old[:70]
    s = s.replace(old, new)

rep("""std::unordered_map<std::string, double> protoDrawAvg;""", """std::unordered_map<std::string, double> protoDrawAvg;
std::unordered_map<std::string, std::unordered_map<std::string, double>> protoSoldAvg;   // RATW_P_SHELF_DAYS.""")
rep("""    shelf_.clear();
    for (const auto& [till, sold] : soldToday_)
        for (const auto& [ware, n] : sold)
            if (const int shelf = std::min(StoreDrawMost, int(std::ceil(n * 1.25))); shelf > startingStock(ware))
                shelf_[till][ware] = shelf;""", """    shelf_.clear();
    if (static const double days = knob("RATW_P_SHELF_DAYS", 0); days > 0)
    {
        for (auto& [till, wares] : protoSoldAvg)
            for (auto& [ware, avg] : wares)
                avg -= avg / days;
        for (const auto& [till, sold] : soldToday_)
            for (const auto& [ware, n] : sold)
                protoSoldAvg[till][ware] += double(n) / days;
        for (const auto& [till, wares] : protoSoldAvg)
            for (const auto& [ware, avg] : wares)
            {
                double n = avg;
                if (const auto t = soldToday_.find(till); t != soldToday_.end())
                    if (const auto w = t->second.find(ware); w != t->second.end())
                        n = std::max(n, double(w->second));
                if (const int shelf = std::min(StoreDrawMost, int(std::ceil(n * 1.25))); shelf > startingStock(ware))
                    shelf_[till][ware] = shelf;
            }
    }
    else
    for (const auto& [till, sold] : soldToday_)
        for (const auto& [ware, n] : sold)
            if (const int shelf = std::min(StoreDrawMost, int(std::ceil(n * 1.25))); shelf > startingStock(ware))
                shelf_[till][ware] = shelf;""")
rep("""        for (int k = 0; k < 20 && budget > 0 && !cfg.comforts.empty(); ++k)""",
    """        static const int comfortItems = int(knob("RATW_P_COMFORT_ITEMS", 20));
        for (int k = 0; k < comfortItems && budget > 0 && !cfg.comforts.empty(); ++k)""")
rep("""                if (excess > 0 && shift(payer, held->second.holder, "", 0, excess, "profit"))
                    loopToday_.profit += excess;""", """                if (static const double tax = knob("RATW_P_PROFIT_TAX", 0) / 100.; tax > 0)
                    if (const auto due = std::int64_t(double(excess) * tax); due > 0 && shift(payer, "treasury", "", 0, due, "profit tax"))
                        excess -= due;
                if (excess > 0 && shift(payer, held->second.holder, "", 0, excess, "profit"))
                    loopToday_.profit += excess;""")
open(p, 'w').write(s)
print('ok')
