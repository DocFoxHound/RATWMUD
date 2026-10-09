// Giving, and the giver's scent (Docs/Design/55-letters-gifts-favours.md, 3; Phase 2). Face to face: an item (not worn,
// nor in the jaws), a quantity, and/or coins, within 2 tiles, both out of a fight. A player accepts or declines within
// 30 s, so nobody is loaded down against their will; a resident takes it unless it dislikes the giver. The goods move by
// Society::shift (kind "a gift", unearned: never a resident's profit), which refuses past the 64 kinds a save may hold.
// The `gift` event warms the receiver by World::bondsFromEvent's rule, once a game day a pair, so pennies can't buy
// affection. Players' goods carry records of who made and gave them (Entity::scents), beside the stack: a gift smells of
// its giver for 7 game days, unless the giver was masked.
#include "RatwGame.h"

#include "RatwItems.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace ratw::game
{
using json::Value;

namespace
{
constexpr double GiveReach = 2, OfferSeconds = 30, GiverDays = 7, MakerDays = 28;
constexpr std::size_t MostScents = 60;
std::string lowerFirst(std::string s)
{
    if (!s.empty())
        s[0] = char(std::tolower(static_cast<unsigned char>(s[0])));
    return s;
}
} // namespace

int Game::spareOf(const std::string& who, const std::string& item) const
{
    const auto* a = world_.society().account(who);
    const auto* e = world_.entity(who);
    if (!a || item.empty())
        return 0;
    int held = Society::stock(*a, item);
    if (e)
    {
        for (const auto& [slot, worn] : e->worn)
            held -= worn == item;
        if (World::swordHeld(*e) == item && e->mouth == "sword")
            held -= 1;
    }
    return std::max(0, held - lentTo(who, item));   // (What is lent to it isn't its to give: doc 55, 8.)
}

std::string Game::goodsWords(const std::string& item, int quantity, std::int64_t coins) const
{
    std::string words;
    if (!item.empty() && quantity > 0)
        words = std::to_string(quantity) + " " + lowerFirst(Society::itemName(item));
    if (coins > 0)
        words += (words.empty() ? "" : " and ") + std::to_string(coins) + "p";
    return words;
}

std::string Game::giveRefusal(const std::string& giver, const std::string& target, const std::string& item, int quantity, std::int64_t coins) const
{
    const auto* g = world_.entity(giver);
    const auto* t = world_.entity(target);
    if (!g || !t || t->dead || giver == target)
        return "Give to whom?";
    if (g->cellId != t->cellId || std::hypot(g->position.x - t->position.x, g->position.y - t->position.y) > GiveReach)
        return "Go nearer first.";
    if (world_.inBattle(giver) || world_.inBattle(target))
        return "Not in the middle of a fight.";
    if (!t->npc && blocked(target, giver))
        return names::capitalised(labelFor(giver, target)) + " won't take anything from you.";
    if ((item.empty() || quantity <= 0) && coins <= 0)
        return "Give what?";
    if (!item.empty() && (quantity < 1 || quantity > 99 || spareOf(giver, item) < quantity))
        return "You haven't that to give (what you wear stays on you).";
    if (coins < 0 || coins > 10000 || world_.society().spendable(giver) < coins)
        return "You haven't that much.";
    if (!item.empty())
        if (const auto* a = world_.society().account(target);
            a && !a->stock.count(item) && a->stock.size() >= MaxGoodsKinds)
            return names::capitalised(labelFor(giver, target)) + " can't carry another kind of thing.";
    return {};
}

void Game::gifted(const std::string& giver, const std::string& receiver, const std::string& item, int quantity, std::int64_t coins)
{
    // The move, the scent, and the warmth (once a game day a pair).
    auto& society = world_.society();
    const auto shortBefore = shortHousehold(receiver);   // (A short household helped to its refill: doc 57, 3.)
    if (coins > 0)
        society.shift(giver, receiver, "", 0, coins, "a gift");
    if (!item.empty() && quantity > 0)
        society.shift(giver, receiver, item, quantity, 0, "a gift");
    const auto* g = world_.entity(giver);
    const bool masked = g && world_.scentMasked(*g);
    if (!item.empty())
        moveScents(giver, receiver, item, quantity, !masked);
    const auto key = giver + "|" + receiver;
    const double today = std::floor(world_.calendarDays());
    const bool warms = !giftBondDay_.count(key) || giftBondDay_[key] < today;
    if (warms)
        giftBondDay_[key] = today;
    world_.recordEvent({warms ? "gift" : "gift again", giver, receiver, g ? g->cellId : std::string(), 0, 0, item, quantity, coins, {}});
    record(Economy | Character, giver);
    troubleGiven(giver, shortBefore);
}

bool Game::giveCommand(Connection* c, const json::Value& j, Result& result)
{
    const auto giver = c->entityId, target = j.string("target"), item = j.string("item");
    const int quantity = item.empty() ? 0 : std::max(1, int(j.number("quantity", 1)));
    const auto coins = std::int64_t(std::max(0., j.number("coins", 0)));
    if (const auto why = giveRefusal(giver, target, item, quantity, coins); !why.empty())
    {
        result = {false, why, target};
        return true;
    }
    const auto* t = world_.entity(target);
    const auto what = goodsWords(item, quantity, coins);
    if (t->npc)
    {
        // A resident takes it, unless it dislikes the giver.
        if (const auto* bond = world_.bonds().find(target, giver); bond && bond->affinity <= -30)
        {
            result = {false, names::capitalised(labelFor(giver, target)) + " won't take anything from you.", target};
            return true;
        }
        gifted(giver, target, item, quantity, coins);
        result = {true, "You give " + labelFor(giver, target) + " " + what + ".", target};
        return true;
    }
    // A player: offered, to accept or decline within 30 s.
    giveOffers_[target] = {giver, item, quantity, coins, world_.time() + OfferSeconds};
    if (auto* other = clientOf(target))
    {
        auto offer = Value::object();
        offer.add("type", "giveOffer");
        offer.add("from", giver);
        offer.add("name", names::capitalised(labelFor(target, giver)));
        offer.add("what", what);
        offer.add("seconds", OfferSeconds);
        send(other, offer);
        system(other, names::capitalised(labelFor(target, giver)) + " offers you " + what + ". Accept or decline (30 seconds).");
    }
    result = {true, "You offer " + labelFor(giver, target) + " " + what + ".", target};
    return true;
}

bool Game::giveAnswer(Connection* c, bool accept, Result& result)
{
    const auto me = c->entityId;
    const auto found = giveOffers_.find(me);
    if (found == giveOffers_.end() || found->second.until < world_.time())
    {
        result = {false, "Nobody is offering you anything.", {}};
        return true;
    }
    const auto offer = found->second;
    giveOffers_.erase(found);
    auto* giverClient = clientOf(offer.from);
    if (!accept)
    {
        if (giverClient)
            system(giverClient, names::capitalised(labelFor(offer.from, me)) + " declines.");
        result = {true, "You decline.", offer.from};
        return true;
    }
    // Again, as things stand now.
    if (const auto why = giveRefusal(offer.from, me, offer.item, offer.quantity, offer.coins); !why.empty())
    {
        result = {false, why == "Go nearer first." ? "They have gone too far off." : "It can't be given now.", offer.from};
        return true;
    }
    const auto what = goodsWords(offer.item, offer.quantity, offer.coins);
    gifted(offer.from, me, offer.item, offer.quantity, offer.coins);
    if (giverClient)
        system(giverClient, names::capitalised(labelFor(offer.from, me)) + " takes " + what + " from you.");
    std::string smell;
    if (!offer.item.empty())
        if (auto line = scentOfItem(me, me, offer.item); !line.empty())
            smell = " " + names::capitalised(line) + ".";
    result = {true, "You take " + what + " from " + labelFor(me, offer.from) + "." + smell, offer.from};
    return true;
}

void Game::tendGives()
{
    for (auto it = giveOffers_.begin(); it != giveOffers_.end();)
        it = it->second.until < world_.time() ? giveOffers_.erase(it) : std::next(it);
}

// ------------------------------------------------------------------ Scent records

void Game::addScent(const std::string& who, const std::string& item, int quantity, const std::string& maker, const std::string& giver)
{
    world_.addScent(who, item, quantity, maker, giver);
}

void Game::moveScents(const std::string& from, const std::string& to, const std::string& item, int quantity, bool giverScent)
{
    // The records go with the goods, oldest first; the giver's own scent on all of them (unless masked). Goods going to a
    // resident or a till lose theirs.
    auto* f = world_.entity(from);
    auto* t = world_.entity(to);
    std::vector<ScentRecord> moved;
    int left = quantity;
    if (f)
        for (auto it = f->scents.begin(); it != f->scents.end() && left > 0;)
        {
            if (it->item != item)
            {
                ++it;
                continue;
            }
            const int n = std::min(left, it->count);
            auto part = *it;
            part.count = n;
            moved.push_back(part);
            left -= n;
            if ((it->count -= n) <= 0)
                it = f->scents.erase(it);
            else
                ++it;
        }
    if (!t || t->npc)
        return;
    for (auto& r : moved)
    {
        if (giverScent)
            r.giver = from, r.givenDay = world_.calendarDays();
        t->scents.push_back(r);
    }
    if (left > 0 && giverScent)
        t->scents.push_back({item, "", from, left, -1, world_.calendarDays()});
    while (t->scents.size() > MostScents)
        t->scents.erase(t->scents.begin());
}

std::string Game::scentOfItem(const std::string& viewer, const std::string& owner, const std::string& item, int held) const
{
    // The newest record within what is held (or `held`, for goods out on a stall): its giver's scent for 7 days, else
    // its maker's for 28.
    const auto* e = world_.entity(owner);
    const auto* a = world_.society().account(owner);
    if (!e || !a)
        return {};
    if (held < 0)
        held = Society::stock(*a, item);
    const double now = world_.calendarDays();
    for (auto it = e->scents.rbegin(); it != e->scents.rend() && held > 0; ++it)
    {
        if (it->item != item)
            continue;
        held -= it->count;
        if (!it->giver.empty() && now - it->givenDay <= GiverDays)
            return "it smells of " + (it->giver == viewer ? std::string("you") : labelFor(viewer, it->giver));
        if (!it->maker.empty() && now - it->madeDay <= MakerDays)
            return "it smells of " + (it->maker == viewer ? std::string("you") : labelFor(viewer, it->maker));
    }
    return {};
}

void Game::makersScent(const std::string& buyer, const std::string& seller, const std::string& item, int quantity)
{
    // Only a maker's own goods: the keeper's business makes it (crafts.json); a reseller adds no scent.
    const auto* spec = world_.society().spec(seller);
    const auto* business = spec ? items::businessFor(spec->workLabel) : nullptr;
    if (!business || quantity <= 0)
        return;
    const auto base = item.substr(0, item.find('~'));
    for (const auto* craft : items::craftsFor(business->id))
        for (const auto& [made, n] : craft->out)
            if (made == base || made == item)
            {
                world_.addScent(buyer, item, quantity, seller, {});
                return;
            }
}
} // namespace ratw::game
