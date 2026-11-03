# PROTOTYPE (what-if only): fair food in a household. RATW_P_FAIRFOOD=1: the members with least food first, everyone to
# shortDays before anyone to foodDays, and the food picked among what the household can pay for.
import sys
p = sys.argv[1]
s = open(p).read()
start = s.index("        // Food: every member topped up to foodDays; one under shortDays again each hour until the shops shut.")
end = s.index("        if (shortHere)")
old = s[start:end]
new = """        static const bool fair = knob("RATW_P_FAIRFOOD", 0) > 0;
        bool shortHere = false;
        if (fair)
        {
            std::vector<std::pair<double, std::string>> order;
            for (const auto& m : members)
                if (const auto* wallet = account(m))
                    order.push_back({onHand(*wallet), m});
            std::sort(order.begin(), order.end());
            for (const double target : {cfg.shortDays * NourishADay, cfg.foodDays * NourishADay})
            {
                if (!first && target > cfg.shortDays * NourishADay)
                    break;
                for (const auto& [unused, m] : order)
                {
                    double have = onHand(*account(m));
                    for (int k = 0; k < 6 && have < target; ++k)
                    {
                        std::int64_t most = 0;
                        for (const auto& x : members)
                            most = std::max(most, cash(x));
                        std::vector<std::string> foods;
                        std::vector<double> value;
                        for (const std::vector<const OpenShop*>* list : {&shops, static_cast<const std::vector<const OpenShop*>*>(&all)})
                        {
                            for (const auto* s : *list)
                                for (const auto& [item, n] : s->account->stock)
                                    if (n > 0 && edible(item) && seasonalCost(item, 1) <= most &&
                                        std::find(foods.begin(), foods.end(), item) == foods.end())
                                    {
                                        foods.push_back(item);
                                        value.push_back(double(nourishment(item)) / (double(std::max<std::int64_t>(1, catalogPrice(item))) * priceFactor(item)));
                                    }
                            if (!foods.empty())
                                break;
                        }
                        const auto key = m + "|" + std::to_string(day) + "|" + std::to_string(hod) + "|" + std::to_string(k) + "|" + std::to_string(int(target));
                        const int pick = weighted(value, roll(key));
                        if (pick < 0)
                            break;
                        const auto& food = foods[std::size_t(pick)];
                        const int pieces = int(std::ceil((target - have) / std::max(1, nourishment(food))));
                        const int got = buy(shops, members, m, food, std::max(1, pieces), 0, key);
                        if (got < 1)
                            break;
                        loopToday_.foodBought += got;
                        have += double(got) * nourishment(food);
                    }
                }
            }
            for (const auto& m : members)
                if (const auto* wallet = account(m))
                    shortHere = shortHere || onHand(*wallet) < cfg.shortDays * NourishADay;
        }
        else
        {
""" + old.replace("        bool shortHere = false;\n", "") + "        }\n"
s = s[:start] + new + s[end:]
open(p, 'w').write(s)
print('ok')
