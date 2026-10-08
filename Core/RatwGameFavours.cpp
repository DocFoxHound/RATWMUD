// Grooming (Docs/Design/55-letters-gifts-favours.md, 7; Phase 3), as the game asks and does it: GROOM on a wolf within
// 1.5 tiles, both out of a fight and still; a player accepts within 30 s (grooming is intimate), a resident only from a
// player it likes (liking 40, trust 30); residents never groom players (the user). Fifteen seconds, both staying within
// 1.5 tiles: moving apart or a fight ends it with nothing. Once a game day for the groomer; GROOM YOURSELF once a game
// day. Done, World::applyGrooming does the rest, and the groomer's line is posted as its action (its own words, or a
// stock line), counted as any action is.
#include "RatwGame.h"

#include <algorithm>
#include <cstdlib>
#include "RatwItems.h"
#include <cmath>

namespace ratw::game
{
using json::Value;

namespace
{
constexpr double GroomReach = 1.5, GroomSeconds = 15, AskSeconds = 30;
constexpr std::size_t WordsMost = 200;
bool still(const Entity& e)
{
    return e.path.empty() && std::hypot(e.velocity.x, e.velocity.y) < .05;
}
} // namespace

std::string Game::groomRefusal(const std::string& groomer, const std::string& target) const
{
    const auto* g = world_.entity(groomer);
    const auto* t = world_.entity(target);
    if (!g || !t || t->dead)
        return "Groom whom?";
    const double today = std::floor(world_.calendarDays());
    if (groomer == target)
        return std::floor(g->groomedSelfDay) == today ? "You have groomed yourself today already." : std::string();
    if (std::floor(g->groomedOtherDay) == today)
        return "You have groomed another today; tomorrow, again.";
    if (g->cellId != t->cellId || std::hypot(g->position.x - t->position.x, g->position.y - t->position.y) > GroomReach)
        return "Go closer first.";
    if (world_.inBattle(groomer) || world_.inBattle(target))
        return "Not in the middle of a fight.";
    if (!still(*g) || !still(*t))
        return "Be still first, both of you.";
    if (!t->npc && (blocked(target, groomer) || blocked(groomer, target)))
        return names::capitalised(labelFor(groomer, target)) + " wants none of that.";
    if (std::any_of(groomings_.begin(), groomings_.end(), [&](const Grooming& x) { return x.groomer == groomer || x.groomed == target; }))
        return "One grooming at a time.";
    return {};
}

void Game::startGrooming(const std::string& groomer, const std::string& groomed, const std::string& words)
{
    groomings_.push_back({groomer, groomed, words, world_.time()});
    if (auto* c = clientOf(groomer))
        system(c, groomer == groomed ? "You set to grooming yourself." : "You set to grooming " + labelFor(groomer, groomed) + ". Stay close.");
    if (groomer != groomed)
        if (auto* c = clientOf(groomed))
            system(c, names::capitalised(labelFor(groomed, groomer)) + " grooms you. Stay close.");
}

bool Game::groomCommand(Connection* c, const json::Value& j, Result& result)
{
    const auto me = c->entityId;
    const auto target = j.string("target") == "self" ? me : j.string("target");
    auto words = j.string("words");
    if (words.size() > WordsMost * 4)
        words.resize(WordsMost * 4);
    if (const auto why = groomRefusal(me, target); !why.empty())
    {
        result = {false, why, target};
        return true;
    }
    const auto* t = world_.entity(target);
    if (target == me)
    {
        startGrooming(me, me, words);
        result = {true, "", {}};
        return true;
    }
    if (t->npc)
    {
        // A resident lets a wolf it likes groom it.
        const auto* bond = world_.bonds().find(target, me);
        if (!bond || bond->affinity < 40 || bond->trust < 30)
        {
            result = {false, names::capitalised(labelFor(me, target)) + " draws back: it doesn't know you well enough for that.", target};
            return true;
        }
        startGrooming(me, target, words);
        result = {true, "", target};
        return true;
    }
    groomOffers_[target] = {me, words, world_.time() + AskSeconds};
    if (auto* other = clientOf(target))
    {
        auto ask = Value::object();
        ask.add("type", "groomOffer");
        ask.add("from", me);
        ask.add("name", names::capitalised(labelFor(target, me)));
        send(other, ask);
        system(other, names::capitalised(labelFor(target, me)) + " would groom you. Accept or decline (30 seconds).");
    }
    result = {true, "You ask to groom " + labelFor(me, target) + ".", target};
    return true;
}

bool Game::groomAnswer(Connection* c, bool accept, Result& result)
{
    const auto me = c->entityId;
    const auto found = groomOffers_.find(me);
    if (found == groomOffers_.end() || found->second.until < world_.time())
    {
        result = {false, "Nobody has asked to groom you.", {}};
        return true;
    }
    const auto ask = found->second;
    groomOffers_.erase(found);
    auto* asker = clientOf(ask.from);
    if (!accept)
    {
        if (asker)
            system(asker, names::capitalised(labelFor(ask.from, me)) + " would rather not.");
        result = {true, "You decline.", ask.from};
        return true;
    }
    if (const auto why = groomRefusal(ask.from, me); !why.empty())
    {
        result = {false, why == "Go closer first." ? "They have gone too far off." : why, ask.from};
        return true;
    }
    startGrooming(ask.from, me, ask.words);
    result = {true, "", ask.from};
    return true;
}

void Game::tendGrooming()
{
    for (auto it = groomOffers_.begin(); it != groomOffers_.end();)
        it = it->second.until < world_.time() ? groomOffers_.erase(it) : std::next(it);
    std::vector<Grooming> done;
    for (auto it = groomings_.begin(); it != groomings_.end();)
    {
        const auto* g = world_.entity(it->groomer);
        const auto* t = world_.entity(it->groomed);
        const bool apart = !g || !t || g->cellId != t->cellId || std::hypot(g->position.x - t->position.x, g->position.y - t->position.y) > GroomReach ||
                           world_.inBattle(it->groomer) || world_.inBattle(it->groomed);
        if (apart)
        {
            for (const auto* who : {&it->groomer, &it->groomed})
                if (auto* c = clientOf(*who))
                    system(c, "The grooming is broken off.");
            it = groomings_.erase(it);
            continue;
        }
        if (world_.time() - it->started >= GroomSeconds)
        {
            done.push_back(*it);
            it = groomings_.erase(it);
            continue;
        }
        ++it;
    }
    for (const auto& x : done)
    {
        auto* g = world_.entity(x.groomer);
        auto* t = world_.entity(x.groomed);
        if (!g || !t)
            continue;
        const bool self = x.groomer == x.groomed;
        const double today = std::floor(world_.calendarDays());
        (self ? g->groomedSelfDay : g->groomedOtherDay) = today;
        world_.applyGrooming(x.groomer, x.groomed);
        // The groomer's line, posted as its action (its own words, or a stock line in its name).
        auto line = x.words;
        if (line.empty())
            line = self ? std::string("grooms ") + (g->appearance->sex == "female" ? "herself" : "himself") + ", working the tangles out of the ruff."
                        : "grooms " + t->name + "'s ruff, slow and careful.";
        if (auto* c = clientOf(x.groomer))
        {
            auto post = Value::object();
            post.add("type", "chat");
            post.add("text", "/action " + line);    // (An action, counted at half weight, as any is: doc 32, 1.1.)
            command(c, json::dump(post));
            system(c, self ? "Groomed, a little: for two hours, a little less scent and a little better." : "Done: " + labelFor(x.groomer, x.groomed) +
                                                                                                         " is Well-groomed.");
        }
        if (!self)
            if (auto* c = clientOf(x.groomed))
                system(c, "Well-groomed, by " + labelFor(x.groomed, x.groomer) +
                              ": for a day (or until you next sleep a full rest), wounds heal a little faster and set less often, residents warm to you "
                              "sooner, and for two hours your scent carries less.");
    }
}

// ------------------------------------------------------------------ Lending (doc 55, 8)

int Game::lentTo(const std::string& borrower, const std::string& item) const
{
    int n = 0;
    for (const auto& l : loans_)
        if (l.borrower == borrower && l.item == item)
            n += l.quantity;
    return n;
}

bool Game::lendCommand(Connection* c, const json::Value& j, Result& result)
{
    const auto me = c->entityId, target = j.string("target"), item = j.string("item");
    const int quantity = std::max(1, int(j.number("quantity", 1)));
    const int days = std::clamp(int(j.number("days", 3)), 1, 7);
    const auto* t = world_.entity(target);
    if (!t || t->npc)
        result = {false, "Lend to whom? (To another player.)", target};
    else if (const auto why = giveRefusal(me, target, item, quantity, 0); !why.empty())
        result = {false, why, target};
    else
    {
        lendOffers_[target] = {me, item, quantity, days, world_.time() + 30};
        const auto what = goodsWords(item, quantity, 0);
        if (auto* other = clientOf(target))
        {
            auto offer = Value::object();
            offer.add("type", "lendOffer");
            offer.add("from", me);
            offer.add("name", names::capitalised(labelFor(target, me)));
            offer.add("what", what);
            offer.add("days", days);
            send(other, offer);
            system(other, names::capitalised(labelFor(target, me)) + " offers to lend you " + what + " for " + std::to_string(days) +
                              (days == 1 ? " day" : " days") + ". Accept or decline (30 seconds).");
        }
        result = {true, "You offer to lend " + labelFor(me, target) + " " + what + ".", target};
    }
    return true;
}

bool Game::lendAnswer(Connection* c, bool accept, Result& result)
{
    const auto me = c->entityId;
    const auto found = lendOffers_.find(me);
    if (found == lendOffers_.end() || found->second.until < world_.time())
    {
        result = {false, "Nobody is offering you a loan.", {}};
        return true;
    }
    const auto offer = found->second;
    lendOffers_.erase(found);
    if (!accept)
    {
        if (auto* l = clientOf(offer.from))
            system(l, names::capitalised(labelFor(offer.from, me)) + " declines the loan.");
        result = {true, "You decline.", offer.from};
        return true;
    }
    if (const auto why = giveRefusal(offer.from, me, offer.item, offer.quantity, 0); !why.empty() ||
        !world_.society().shift(offer.from, me, offer.item, offer.quantity, 0, "lent"))
    {
        result = {false, "It can't be lent now.", offer.from};
        return true;
    }
    moveScents(offer.from, me, offer.item, offer.quantity, false);
    loans_.push_back({"loan-" + std::to_string(nextLoan_++), offer.from, me, offer.item, offer.quantity, world_.calendarDays() + offer.days});
    world_.recordEvent({"lent", offer.from, me, {}, 0, 0, offer.item, offer.quantity, 0, loans_.back().id});
    record(Economy | Character, me);
    const auto what = goodsWords(offer.item, offer.quantity, 0);
    if (auto* l = clientOf(offer.from))
        system(l, names::capitalised(labelFor(offer.from, me)) + " borrows " + what + " for " + std::to_string(offer.days) + " days.");
    result = {true, "You borrow " + what + " for " + std::to_string(offer.days) + " days. Wear it, use it; it isn't yours to sell or give.", offer.from};
    return true;
}

bool Game::returnLoan(Connection* c, const json::Value& j, Result& result)
{
    // The borrower, within 2 tiles of the lender, hands it back.
    const auto me = c->entityId;
    const auto it = std::find_if(loans_.begin(), loans_.end(), [&](const Loan& l) { return l.id == j.string("loan") && l.borrower == me; });
    if (it == loans_.end())
    {
        result = {false, "You have borrowed no such thing.", {}};
        return true;
    }
    const auto* b = world_.entity(me);
    const auto* l = world_.entity(it->lender);
    if (!b || !l || b->cellId != l->cellId || std::hypot(b->position.x - l->position.x, b->position.y - l->position.y) > 2)
    {
        result = {false, "Go to " + (l ? labelFor(me, it->lender) : std::string("them")) + " to hand it back, or let the courier take it at the day.", {}};
        return true;
    }
    if (!world_.society().shift(me, it->lender, it->item, it->quantity, 0, "lent"))
    {
        result = {false, "You haven't it all to hand back.", {}};
        return true;
    }
    world_.fitWorn(me);                             // (Taken off, if it was worn.)
    world_.recordEvent({"loan returned", me, it->lender, {}, 0, 0, it->item, it->quantity, 0, it->id});
    if (auto* lc = clientOf(it->lender))
        system(lc, names::capitalised(labelFor(it->lender, me)) + " hands back " + goodsWords(it->item, it->quantity, 0) + ".");
    result = {true, "You hand back " + goodsWords(it->item, it->quantity, 0) + ".", it->lender};
    loans_.erase(it);
    record(Economy | Character, me);
    return true;
}

void Game::tendLoans()
{
    // Once a game hour: what is due goes back by courier (a penny from the borrower); what is gone becomes a debt, and
    // the lender trusts the borrower less.
    const double hour = std::floor(world_.calendarDays() * 24);
    if (hour == loansHour_)
        return;
    loansHour_ = hour;
    for (auto it = lendOffers_.begin(); it != lendOffers_.end();)
        it = it->second.until < world_.time() ? lendOffers_.erase(it) : std::next(it);
    auto& society = world_.society();
    for (auto it = loans_.begin(); it != loans_.end();)
    {
        if (it->due > world_.calendarDays())
        {
            ++it;
            continue;
        }
        const auto* a = society.account(it->borrower);
        const int have = a ? Society::stock(*a, it->item) : 0;
        const int back = std::min(have, it->quantity);
        if (back > 0)
        {
            society.shift(it->borrower, it->lender, it->item, back, 0, "lent");
            world_.fitWorn(it->borrower);           // (Taken off, if it was worn.)
            society.shift(it->borrower, society.treasuryOf(society.capital()), "", 0, 1, "a courier's fee");
        }
        if (back < it->quantity)
        {
            const auto* good = items::good(it->item.substr(0, it->item.find('~')));
            const std::int64_t owed = std::int64_t(good ? good->price : 1) * (it->quantity - back);
            world_.bonds().addOwed(it->lender, it->borrower, owed, world_.calendarDays());
            world_.bonds().change(it->lender, it->borrower, {0, -10, 0, 0, 0}, world_.calendarDays());
            world_.recordEvent({"loan unreturned", it->borrower, it->lender, {}, 0, 0, it->item, it->quantity - back, owed, it->id});
        }
        else
            world_.recordEvent({"loan returned", it->borrower, it->lender, {}, 0, 0, it->item, back, 0, it->id});
        const auto what = goodsWords(it->item, it->quantity, 0);
        if (auto* lc = clientOf(it->lender))
            system(lc, back == it->quantity ? "A courier brings back what you lent: " + what + "."
                                            : "What you lent came back short: they owe you for the rest.");
        if (auto* bc = clientOf(it->borrower))
            system(bc, back == it->quantity ? "A courier takes back what you borrowed (1p)." : "What you borrowed is gone: you owe for it now.");
        record(Economy, it->borrower);
        it = loans_.erase(it);
    }
}

void Game::loansSave(json::Value& root) const
{
    auto list = Value::array();
    for (const auto& l : loans_)
    {
        auto o = Value::object();
        o.add("id", l.id);
        o.add("lender", l.lender);
        o.add("borrower", l.borrower);
        o.add("item", l.item);
        o.add("quantity", l.quantity);
        o.add("due", l.due);
        list.push(o);
    }
    root.add("loans", list);
}

void Game::loansLoad(const json::Value& saved)
{
    loans_.clear();
    for (const auto& o : saved.array("loans"))
        if (!o.string("id").empty() && !o.string("borrower").empty())
        {
            loans_.push_back({o.string("id"), o.string("lender"), o.string("borrower"), o.string("item"), std::max(1, int(o.number("quantity", 1))),
                              o.number("due")});
            if (const auto n = std::strtoull(o.string("id").c_str() + 5, nullptr, 10); n >= nextLoan_)
                nextLoan_ = n + 1;
        }
}
} // namespace ratw::game
