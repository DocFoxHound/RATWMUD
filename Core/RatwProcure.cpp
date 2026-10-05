#include "RatwWorld.h"

#include "RatwItems.h"
#include "RatwNames.h"

#include <algorithm>
#include <cctype>
#include <cmath>

// Contracts for goods (Docs/Design/35-items-crafting-industry.md, Part 7). A town's own buyers (the Town Works, the
// watch, the church; Society::townBuyers) ask for what their town's shops couldn't sell them, and the world posts each
// ask as a "procure" contract in that town: so many of a good, for a reward of the goods' price and a premium, held in
// escrow from the buyer's own funds. Anyone in town can hear of it from a merchant and take it on; the one who took it
// delivers what they carry of it, any quality, to any merchant of that town, a few at a time if they like, and is paid
// by the piece. The goods go to the buyer. A contract left undelivered lapses and the rest of the reward goes back.
namespace ratw
{
namespace
{
std::string lowerFirst(std::string s)
{
    if (!s.empty())
        s[0] = char(std::tolower(static_cast<unsigned char>(s[0])));
    return s;
}
// "upper_accord" -> "Upper Accord": a town's id as a name.
std::string placeTitle(std::string id)
{
    bool start = true;
    for (auto& ch : id)
    {
        if (ch == '_' || ch == '-')
            ch = ' ';
        else if (start)
            ch = char(std::toupper(static_cast<unsigned char>(ch)));
        start = ch == ' ';
    }
    return id;
}
} // namespace

void World::postProcurements()
{
    for (const auto& want : society_.takeProcurements())
    {
        // One standing ask at a time for each buyer and good.
        const bool asked = std::any_of(roads_.contracts.begin(), roads_.contracts.end(), [&](const Contract& k) {
            return k.kind == "procure" && k.poster == want.account && k.item == want.item && (k.status == "open" || k.status == "taken");
        });
        if (asked || want.quantity <= 0)
            continue;
        const auto* funds = society_.account(want.account);
        const auto reward = std::int64_t(std::ceil(want.price * want.quantity * items::contractPremium()));
        if (!funds || funds->cash < reward)
            continue;                                   // (It can't pay for it: it does without.)
        const auto* good = items::good(want.item);
        const std::string what = std::to_string(want.quantity) + " " + lowerFirst(good ? good->name : want.item);
        auto& c = postContract("procure", want.account, want.community, want.account, reward, 7,
                               names::capitalised(want.buyer) + " of " + placeTitle(want.community) + " wants " + what);
        c.item = want.item;
        c.quantity = want.quantity;
    }
}

std::vector<const Contract*> World::deliverable(const std::string& player) const
{
    std::vector<const Contract*> out;
    const auto* p = entity(player);
    const auto* here = p ? townOf(p->cellId) : nullptr;
    const auto* purse = society_.account(player);
    if (!here || !purse)
        return out;
    for (const auto& c : roads_.contracts)
        if (c.kind == "procure" && c.status == "taken" && c.taker == player && c.town == here->id && Society::stockAll(*purse, c.item) > 0)
            out.push_back(&c);
    return out;
}

Result World::deliverContract(const std::string& player, const std::string& contractId)
{
    const auto* p = entity(player);
    if (!p || p->npc)
        return {false, "No such character.", {}};
    for (auto& c : roads_.contracts)
    {
        if (c.id != contractId)
            continue;
        if (c.kind != "procure" || c.status != "taken" || c.taker != player)
            return {false, "That isn't work you've taken on.", contractId};
        const auto* here = townOf(p->cellId);
        if (!here || here->id != c.town)
            return {false, "That is wanted in another town.", contractId};
        const auto* purse = society_.account(player);
        if (!purse)
            return {false, "You carry nothing.", contractId};
        // What they carry of it, any quality, plainest first, up to what is still wanted.
        int given = 0;
        for (const auto& kind : Society::kindsHeld(*purse, c.item))
        {
            const int n = std::min(Society::stock(*society_.account(player), kind), c.quantity - c.delivered - given);
            if (n > 0 && society_.shift(player, c.poster, kind, n, 0, "contract delivery"))
                given += n;
            if (c.delivered + given >= c.quantity)
                break;
        }
        if (given <= 0)
            return {false, "You carry none of what is wanted.", contractId};
        const auto escrow = "contract:" + c.id;
        const int left = c.quantity - c.delivered;
        const auto pay = given >= left ? c.reward : c.reward * given / left;
        c.delivered += given;
        if (pay > 0 && society_.shift(escrow, player, "", 0, pay, "contract reward"))
            c.reward -= pay;
        const auto* good = items::good(c.item);
        const std::string what = lowerFirst(good ? good->name : c.item);
        if (c.delivered >= c.quantity)
        {
            society_.closeAccount(escrow);
            c.status = "done";
            recordEvent({"contract done", player, c.target, p->cellId, 0, 0, c.item, c.quantity, pay, c.kind + ": " + c.detail});
            return {true, "You deliver the last " + std::to_string(given) + " " + what + ", and are paid " + std::to_string(pay) +
                              "p. The work is done.",
                    contractId};
        }
        recordEvent({"contract delivery", player, c.target, p->cellId, 0, 0, c.item, given, pay, c.kind + ": " + c.detail});
        return {true, "You deliver " + std::to_string(given) + " " + what + " and are paid " + std::to_string(pay) + "p; " +
                          std::to_string(c.quantity - c.delivered) + " more are wanted.",
                contractId};
    }
    return {false, "There is no such work.", contractId};
}
} // namespace ratw
