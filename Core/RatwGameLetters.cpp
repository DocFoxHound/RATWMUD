// Letters between players (Docs/Design/55-letters-gifts-favours.md, 1 and 2; Phase 1). Written at an inn or tavern, at
// a scriptorium's desk, or in one's Chapter's own place, to a wolf the writer knows by name; sealed, signed with one of
// the writer's own names or not at all, carrying the writer's scent unless masked. The courier's fee goes to the writing
// town's treasury; the courier is a time, not a walker: an hour within a town, more between towns. A letter goes to its
// reader's post town: handed over if they are there, else it waits at that town's inns until they come in, or is sent
// on. Letters are mail: kept until the reader burns them. The server always knows who wrote what (for the Dungeon Master
// and reports); a masked, unsigned letter is anonymous to players, and may be answered once by the same courier.
#include "RatwGame.h"

#include "RatwInjury.h"
#include "RatwItems.h"
#include "RatwNames.h"
#include "RatwWire.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace ratw::game
{
using json::Value;

namespace
{
std::string lowered(std::string s)
{
    for (auto& ch : s)
        ch = char(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

int lettersIn(const std::string& text)                 // (UTF-8: what a reader counts, not bytes.)
{
    int n = 0;
    for (unsigned char ch : text)
        n += (ch & 0xC0) != 0x80;
    return n;
}
} // namespace

// ------------------------------------------------------------------ Places and towns

void Game::refreshLetterPlaces()
{
    // Inns and taverns (the `inn` business) and scriptoria, by the cells their posts work in; once in five minutes.
    if (letterPlacesAt_ >= 0 && world_.time() - letterPlacesAt_ < 300)
        return;
    letterPlacesAt_ = world_.time();
    innCells_.clear();
    scriptoriumCells_.clear();
    for (const auto& p : world_.society().positions())
        if (const auto* business = items::businessFor(p.title))
        {
            if (business->id == "inn")
                innCells_.insert(p.work.cell);
            else if (business->id == "scriptorium")
                scriptoriumCells_.insert(p.work.cell);
        }
}

bool Game::atInn(const std::string& id) const
{
    const auto* e = world_.entity(id);
    return e && innCells_.count(e->cellId);
}

bool Game::writingPlace(const std::string& id) const
{
    const auto* e = world_.entity(id);
    if (!e)
        return false;
    if (innCells_.count(e->cellId) || scriptoriumCells_.count(e->cellId))
        return true;
    // A Chapter's own rented place (doc 32), for its members.
    if (const auto* lease = estates_.lease(e->cellId))
        if (const auto* mine = chapters_.of(id); mine && mine->id == lease->chapter)
            return true;
    return false;
}

std::string Game::townFor(const std::string& cellId) const
{
    if (const auto* t = world_.townOf(cellId))
        return t->id;
    return world_.lawTown(cellId);
}

std::string Game::townWords(const std::string& town) const
{
    for (const auto& s : newcomers::rules().starts)
        if (s.id == town)
            return s.name;
    for (const auto& t : world_.towns())
        if (t.id == town)
            if (const auto* c = world_.cell(t.market))
                return c->name;
    if (town.empty())
        return "the road";
    auto words = town;
    std::replace(words.begin(), words.end(), '_', ' ');
    return names::capitalised(words);
}

std::string Game::postTownOf(const std::string& id) const
{
    // Where its letters go: the town it last left the world or slept a full rest in; else the town it is in; else the
    // capital.
    const Entity* e = world_.entity(id);
    if (!e)
        if (const auto found = characters_.find(id); found != characters_.end())
            e = &found->second;
    if (!e)
        return {};
    if (const auto* l = lodgingOf(id); l && !l->town.empty())
        return l->town;                             // (Its lodgings' town first: doc 54, 4.)
    if (!e->postTown.empty())
        return e->postTown;
    if (auto here = townFor(e->cellId); !here.empty())
        return here;
    return world_.society().capital();
}

int Game::courierCells(const std::string& fromTown, const std::string& toTown) const
{
    if (fromTown == toTown || fromTown.empty() || toTown.empty())
        return 0;
    const auto& towns = world_.towns();
    const auto a = std::find_if(towns.begin(), towns.end(), [&](const auto& t) { return t.id == fromTown; });
    const auto b = std::find_if(towns.begin(), towns.end(), [&](const auto& t) { return t.id == toTown; });
    if (a == towns.end() || b == towns.end())
        return 10;                                  // (A town the roads don't know: a middling way.)
    const auto route = world_.routeBetween(a->market, b->market);
    return route.empty() ? 30 : int(route.size());
}

// ------------------------------------------------------------------ Names and scent

std::string Game::knownByName(const std::string& writer, const std::string& typed, std::string& problem) const
{
    // A wolf the writer knows by the name typed (any of the names it knows them by), never by an account handle.
    const auto want = lowered(names::capitalised(typed));
    std::vector<std::string> found;
    for (const auto& [id, ch] : characters_)
    {
        if (id == writer)
            continue;
        std::vector<std::string> known;
        if (!options_.hiddenNames)
            known = namesOf(id);
        else if (const auto* k = known_.find(writer, id))
            known = k->names;
        for (const auto& n : known)
            if (lowered(n) == want)
            {
                found.push_back(id);
                break;
            }
    }
    if (found.empty())
        problem = "You know no wolf called " + names::capitalised(typed) + ".";
    else if (found.size() > 1)
        problem = "You know more than one wolf called " + names::capitalised(typed) + "; the courier can't tell which.";
    return found.size() == 1 ? found.front() : std::string();
}

std::string Game::scentLine(const std::string& reader, const documents::Document& d) const
{
    // What the reader's nose makes of it (doc 55, 2): by name, by sight, unknown, none (masked), faded.
    const auto& r = documents::rules();
    const auto* me = world_.entity(reader);
    if (me && World::noseAcuity(*me) < .2)
        return {};
    if (d.scent.empty())
        return "It carries no scent at all. Someone took care.";
    if (world_.calendarDays() - d.written > r.scentDays)
        return "Its scent has faded.";
    if (knowsName(reader, d.scent))
        return "It smells of " + labelFor(reader, d.scent) + ".";
    const auto* bond = world_.bonds().find(reader, d.scent);
    const auto known = knownWolves_.find(reader);
    if ((bond && bond->familiarity >= r.sightFamiliarity) || (known != knownWolves_.end() && known->second.count(d.scent)))
        return "It smells of " + labelFor(reader, d.scent) + ".";
    return "A wolf's scent you don't know.";
}

// ------------------------------------------------------------------ Writing

documents::Document* Game::sendLetter(Connection* c, const std::string& toId, std::string text, const std::string& sign, Result& result,
                                      const std::string& replyTo, bool viaCourier, const json::Value& enclose)
{
    const auto& r = documents::rules();
    const auto writer = c->entityId;
    auto* w = world_.entity(writer);
    if (!w || w->npc)
        return result = {false, "No such character.", {}}, nullptr;
    refreshLetterPlaces();
    if (!writingPlace(writer))
        return result = {false, "Letters are written at an inn or tavern, at a scriptorium's desk, or in your Chapter's own place.", {}}, nullptr;
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        text.pop_back();
    text.erase(0, text.find_first_not_of(" \t\r\n"));
    if (text.empty() || lettersIn(text) > r.textMost)
        return result = {false, "A letter is 1 to " + std::to_string(r.textMost) + " letters long.", {}}, nullptr;
    if (!sign.empty())
    {
        const auto mine = namesOf(writer);
        if (std::find(mine.begin(), mine.end(), sign) == mine.end())
            return result = {false, "Sign with one of your own names, or leave it unsigned.", {}}, nullptr;
    }
    // Unreachable alike: a wolf gone and one who has blocked the writer (a block isn't revealed: doc 50).
    if (!characters_.count(toId) || blocked(toId, writer) || toId == writer)
        return result = {false, "The courier can't find them.", {}}, nullptr;
    const double today = std::floor(world_.calendarDays());
    if (documents_.writtenSince(writer, today) >= r.perDay)
        return result = {false, "You have sent as many letters as the courier will take from you today.", {}}, nullptr;
    if (documents_.unread(toId) >= r.unreadMost)
        return result = {false, "Their post is full; the courier won't take more for them.", {}}, nullptr;
    // An enclosure (doc 55, 3): one kind of small thing, a pound in all at most, and up to 50p.
    const auto encItem = enclose.isObject() ? enclose.string("item") : std::string();
    const int encQuantity = encItem.empty() ? 0 : std::max(1, int(enclose.number("quantity", 1)));
    const auto encCoins = enclose.isObject() ? std::int64_t(std::max(0., enclose.number("coins", 0))) : 0;
    if (!encItem.empty())
    {
        const auto* good = items::good(encItem);
        if (!good || good->weight <= 0 || good->weight * encQuantity > 1.0 + 1e-9)   // (Weightless: livestock, not post.)
            return result = {false, "A letter carries only something small: a pound in all at most (a ring, a charm, a cake; not a sword).", {}}, nullptr;
        if (spareOf(writer, encItem) < encQuantity)
            return result = {false, "You haven't that to put in it (what you wear stays on you).", {}}, nullptr;
    }
    if (encCoins > 50)
        return result = {false, "A letter carries 50p at most.", {}}, nullptr;
    const auto from = townFor(w->cellId), post = postTownOf(toId);
    const int cells = courierCells(from, post);
    const bool same = cells == 0;
    const int fee = documents::courierFee(cells, same);
    const auto treasury = world_.society().treasuryOf(from.empty() ? world_.society().capital() : from);
    if (world_.society().spendable(writer) < fee + encCoins || !world_.society().shift(writer, treasury, "", 0, fee, "a letter's postage"))
        return result = {false, "The courier's fee is " + std::to_string(fee) + "p, more than you have.", {}}, nullptr;
    documents::Document d;
    d.kind = "letter";
    d.author = writer;
    d.scent = world_.scentMasked(*w) ? std::string() : writer;     // (Masked, it carries none.)
    d.to = toId;
    d.text = text;
    d.sign = sign;
    d.fromTown = from;
    d.postTown = post;
    d.written = world_.calendarDays();
    const double hours = documents::courierHours(cells, same);
    d.deliverAt = d.written + hours / 24;
    d.replyTo = replyTo;
    d.viaCourier = viaCourier;
    auto& stored = documents_.add(d);
    if (!encItem.empty() || encCoins > 0)
    {
        // Held in escrow, "letter:<id>@<writer>", from writing to taking (the writer's id in it, so a letter lost in a
        // crash still finds its way back: orphanedEnclosures).
        auto& society = world_.society();
        stored.escrow = "letter:" + stored.id + "@" + writer;
        society.openAccount(stored.escrow);
        if (encCoins > 0 && society.shift(writer, stored.escrow, "", 0, encCoins, "a letter's enclosure"))
            stored.encCoins = encCoins;
        if (!encItem.empty() && society.shift(writer, stored.escrow, encItem, encQuantity, 0, "a letter's enclosure"))
        {
            stored.encItem = encItem;
            stored.encQuantity = encQuantity;
            stored.scented = !world_.scentMasked(*w);
            moveScents(writer, stored.escrow, encItem, encQuantity, false);   // (Its old records go: the writer's own comes with it.)
        }
        record(Economy | Character, writer);
    }
    world_.recordEvent({"letter sent", writer, viaCourier ? std::string() : toId, w->cellId, 0, 0, {}, 0, fee, stored.id});
    const auto name = viaCourier ? std::string("the one who wrote to you") : labelFor(writer, toId);
    const int roundHours = std::max(1, int(std::lround(hours)));
    result = {true, "The courier takes your letter to " + name + " (" + std::to_string(fee) + "p). It should reach " + townWords(post) +
                        " in about " + std::to_string(roundHours) + (roundHours == 1 ? " hour." : " hours.") +
                        (sign.empty() ? " Unsigned." : " You signed as " + sign + "."),
              {}};
    saveSoon();
    return &stored;
}

// ------------------------------------------------------------------ Delivering

void Game::deliverLetter(documents::Document& d, bool atAnInn)
{
    // Arrived in the reader's post town: handed over if they are there (or have walked into one of its inns), else it
    // waits at its inns.
    const double was = d.deliverAt;
    auto* c = clientOf(d.to);
    const auto* e = world_.entity(d.to);
    const bool there = c && e && (d.postTown.empty() || townFor(e->cellId) == d.postTown);
    if (there || atAnInn)
    {
        d.state = "delivered";
        documents_.rescheduled(d.id, was);
        world_.recordEvent({"letter delivered", d.to, {}, e ? e->cellId : std::string(), 0, 0, {}, 0, 0, d.id});
        if (c)
        {
            system(c, atAnInn ? "At the inn, a letter waits for you." : "A messenger finds you with a letter.");
            auto arrived = Value::object();
            arrived.add("type", "letterArrived");
            arrived.add("id", d.id);
            send(c, arrived);
        }
        return;
    }
    if (d.state == "travelling")
    {
        d.state = "waiting";
        documents_.rescheduled(d.id, was);
        if (c)
            system(c, "A letter waits for you at the inns of " + townWords(d.postTown) + ".");
    }
}

void Game::tendLetters(double dt)
{
    lettersAccumulator_ += dt;
    if (lettersAccumulator_ < 1)
        return;
    lettersAccumulator_ = 0;
    refreshLetterPlaces();
    tendGives();
    tendGrooming();
    tendResidentLetters();
    tendLoans();
    tendTaverns(1);
    tendStalls();
    tendTables(1);
    tendFestivals(1);
    tendArchive();
    tendFame();                                     // Deeds past their life go (doc 56).
    tendCriers();                                   // Criers calling deeds at the square (doc 56, 7).
    tendChronicle();                                // Chronicles read from the database, sent (doc 56, 8).                                  // Copying at a desk; places been (doc 54, 7).                               // The festival's feast and contests (doc 54, 6).                                  // Tavern games: residents' turns, forfeits (doc 54, 5).                                   // Market stalls clear at 2, or in foul weather (doc 54, 3).
    if (const double hour = std::floor(world_.calendarDays() * 24); hour != noticesHour_)
    {
        noticesHour_ = hour;
        tendNotices();                              // Notices come down; unanswered ones looked at again (doc 54, 2).
    }
    // A letter holding something, unread for 56 game days, goes back to its writer with it (doc 55, 3).
    std::vector<std::string> stale;
    if (const double hour = std::floor(world_.calendarDays() * 24); hour != staleCheckedHour_)   // (Once a game hour.)
    {
        staleCheckedHour_ = hour;
        for (const auto& [id, d] : documents_.all())
            if (!d.escrow.empty() && d.readAt < 0 && world_.calendarDays() - d.written >= 56)
                stale.push_back(id);
    }
    for (const auto& id : stale)
        if (auto* d = documents_.find(id))
        {
            returnEnclosure(*d, "unread");
            documents_.erase(id);
        }
    // Letters in a friend's keeping: handed over when their courier contract is done; back to the town's courier if not.
    for (auto it = carried_.begin(); it != carried_.end();)
    {
        auto* d = documents_.find(*it);
        const Contract* k = nullptr;
        if (d)
            for (const auto& c : world_.roads().contracts)
                if (c.id == d->contract)
                    k = &c;
        if (!d || d->state != "carried")
        {
            it = carried_.erase(it);
            continue;
        }
        if (k && k->status == "done")
        {
            const double was = d->deliverAt;
            d->state = "delivered";
            documents_.rescheduled(d->id, was);
            if (auto* c = clientOf(d->to))
            {
                system(c, names::capitalised(labelFor(d->to, k->taker)) + " hands you a letter.");
                auto arrived = Value::object();
                arrived.add("type", "letterArrived");
                arrived.add("id", d->id);
                send(c, arrived);
            }
            it = carried_.erase(it);
            continue;
        }
        if (!k || k->status == "expired" || k->status == "open")
        {
            const double was = d->deliverAt;
            d->state = "travelling";
            d->deliverAt = world_.calendarDays() + documents::courierHours(0, true) / 24;
            documents_.rescheduled(d->id, was);
            it = carried_.erase(it);
            continue;
        }
        ++it;
    }
    for (const auto& id : documents_.due(world_.calendarDays()))
        if (auto* d = documents_.find(id))
        {
            if (!characters_.count(d->to))
            {
                if (!d->escrow.empty())
                    returnEnclosure(*d, "undelivered");
                documents_.erase(id);               // (Its reader is gone: nobody to read it.)
                continue;
            }
            deliverLetter(*d, false);
        }
    // Wolves in the world: a full rest makes where they are their post town; one in an inn of its post town collects
    // what waits there.
    for (auto* c : clients_)
    {
        auto* e = c ? world_.entity(c->entityId) : nullptr;
        if (!e || e->npc)
            continue;
        if (auto& seen = lastFullRest_[e->id]; seen != e->fullRestDay)
        {
            if (seen != 0 || e->postTown.empty())
                if (const auto here = townFor(e->cellId); !here.empty())
                    e->postTown = here;
            seen = e->fullRestDay;
        }
        if (!atInn(e->id))
            continue;
        const auto here = townFor(e->cellId);
        bool any = false;
        for (const auto* d : documents_.forReader(e->id))
            if (d->state == "waiting" && d->postTown == here)
            {
                deliverLetter(*documents_.find(d->id), true);
                any = true;
            }
        if (any)
            sendLetters(c);
    }
}

// ------------------------------------------------------------------ The letter case

void Game::sendLetters(Connection* c)
{
    const auto me = c->entityId;
    auto list = Value::array();
    std::map<std::string, int> waiting;
    for (const auto* d : documents_.forReader(me))
    {
        if (d->state == "waiting")
        {
            ++waiting[d->postTown];
            continue;
        }
        auto o = Value::object();
        o.add("id", d->id);
        o.add("sign", d->sign);
        if (d->sign.empty() && d->kind == "resident_letter")
            o.add("by", names::capitalised(labelFor(me, d->author)));   // (Unsigned by name: by its role, as known.)
        o.add("scent", scentLine(me, *d));
        o.add("when", injury::dateWords(d->written));
        o.add("from", townWords(d->fromTown));
        o.add("read", d->readAt >= 0);
        o.add("kept", d->kept);
        if (d->readAt >= 0)
            o.add("text", d->text);                 // (Sealed until opened.)
        // Reply: to a wolf known by name, or once by the same courier to an anonymous one.
        // (Residents' letters aren't answered by letter: speaking to them is enough, the user, 2026-10-08.)
        o.add("canReply", d->kind == "letter" && (knowsName(me, d->author) || !d->answered));
        if (!d->contract.empty())
            o.add("work", d->contract);             // TAKE IT ON (doc 55, 5).
        if (!d->occasion.empty())
        {
            o.add("invitation", true);              // ANSWER: COMING / CAN'T COME (doc 55, 6).
            o.add("answer", d->answer);
        }
        if (!d->pact.empty())
        {
            // A pact (doc 55, 8): between whom, who has sealed it, and whether this wolf may seal it now.
            auto seals = Value::array();
            for (const auto& s : d->seals)
                seals.push(s == me ? std::string("you") : names::capitalised(labelFor(me, s)));
            o.add("pact", true);
            o.add("between", names::capitalised(labelFor(me, d->author)) + " and " + names::capitalised(labelFor(me, d->party)));
            o.add("seals", seals);
            o.add("canSeal", std::find(d->seals.begin(), d->seals.end(), me) == d->seals.end());
            o.add("canAskWitness", (me == d->author || me == d->party) && d->witnesses.size() < 3);
        }
        o.add("viaCourier", d->viaCourier);
        if (!d->escrow.empty())
            o.add("enclosed", goodsWords(d->encItem, d->encQuantity, d->encCoins));   // (What it holds, until taken.)
        list.push(o);
    }
    auto w = Value::array();
    for (const auto& [town, n] : waiting)
    {
        auto o = Value::object();
        o.add("id", town);
        o.add("town", townWords(town));
        o.add("count", n);
        w.push(o);
    }
    auto e = Value::object();
    e.add("type", "letters");
    e.add("letters", list);
    e.add("waiting", w);
    e.add("canWrite", writingPlace(me));
    send(c, e);
}

json::Value Game::lettersSelf(const std::string& id) const
{
    int unread = 0, waiting = 0;
    std::string at;
    for (const auto* d : documents_.forReader(id))
    {
        if (d->state == "waiting")
        {
            ++waiting;
            at = townWords(d->postTown);
        }
        else if (d->readAt < 0)
            ++unread;
    }
    if (!unread && !waiting)
        return {};
    auto o = Value::object();
    o.add("unread", unread);
    o.add("waiting", waiting);
    if (waiting)
        o.add("waitingAt", at);
    return o;
}

bool Game::letterCommand(Connection* c, const json::Value& j, Result& result)
{
    const auto me = c->entityId;
    const auto verb = j.string("verb", "list");
    refreshLetterPlaces();
    if (verb == "list")
    {
        sendLetters(c);
        result = {true, "", {}};
        return true;
    }
    if (verb == "write")
    {
        std::string problem;
        const auto to = knownByName(me, j.string("to"), problem);
        if (to.empty())
        {
            result = {false, problem, {}};
            return true;
        }
        // Carried by a friend (doc 55, 8): a player the writer knows by name, who knows the recipient by name too.
        std::string carrier;
        if (!j.string("by").empty())
        {
            carrier = knownByName(me, j.string("by"), problem);
            if (carrier.empty() || carrier == to || !knowsName(carrier, to) || blocked(carrier, me))
            {
                result = {false, carrier.empty() ? problem : "They don't know whom it's for well enough to carry it.", {}};
                return true;
            }
        }
        if (auto* sent = sendLetter(c, to, j.string("text"), j.string("sign"), result, {}, false, j["enclose"]); sent && !carrier.empty())
        {
            // The town's courier gives it up: the fee goes into the contract for the friend instead (refunded from the
            // treasury), and the letter waits in the friend's keeping until handed over.
            auto& society = world_.society();
            const int cells = courierCells(sent->fromTown, sent->postTown);
            const int fee = documents::courierFee(cells, cells == 0);
            society.shift(society.treasuryOf(sent->fromTown.empty() ? society.capital() : sent->fromTown), me, "", 0, fee, "a letter's postage");
            const auto& contract = world_.postContract("courier", me, sent->fromTown, to, fee, 7, "a letter for " + labelFor(carrier, to));
            for (auto& k : world_.roads().contracts)
                if (k.id == contract.id)
                {
                    k.status = "taken";
                    k.taker = carrier;
                    k.offeredTo = carrier;
                }
            const double was = sent->deliverAt;
            sent->state = "carried";
            sent->contract = contract.id;
            documents_.rescheduled(sent->id, was);
            carried_.insert(sent->id);
            if (auto* cc = clientOf(carrier))
                system(cc, names::capitalised(labelFor(carrier, me)) + " gives you a letter to carry to " + labelFor(carrier, to) + " (" +
                               std::to_string(fee) + "p when you hand it over).");
            result = {true, names::capitalised(labelFor(me, carrier)) + " will carry your letter to " + labelFor(me, to) + " (" + std::to_string(fee) + "p).", {}};
            record(Economy | Roads, me);
        }
        return true;
    }
    if (verb == "sendOn")
    {
        // From anywhere: what waits at one town's inns brought on to the town one is in, a penny a letter and the
        // courier's time again.
        const auto* e = world_.entity(me);
        const auto here = e ? townFor(e->cellId) : std::string();
        const auto from = j.string("from");
        std::vector<std::string> ids;
        for (const auto* d : documents_.forReader(me))
            if (d->state == "waiting" && d->postTown == from)
                ids.push_back(d->id);
        if (ids.empty() || here.empty() || here == from)
        {
            result = {false, ids.empty() ? "No letters wait for you there." : "Go to one of this town's inns for them.", {}};
            return true;
        }
        const int fee = documents::rules().sendOnFee * int(ids.size());
        const auto treasury = world_.society().treasuryOf(here);
        if (world_.society().spendable(me) < fee || !world_.society().shift(me, treasury, "", 0, fee, "a letter's postage"))
        {
            result = {false, "Sending them on costs " + std::to_string(fee) + "p, more than you have.", {}};
            return true;
        }
        const int cells = courierCells(from, here);
        for (const auto& id : ids)
            if (auto* d = documents_.find(id))
            {
                const double was = d->deliverAt;
                d->deliverAt = world_.calendarDays() + documents::courierHours(cells, cells == 0) / 24;
                d->postTown = here;
                d->state = "travelling";
                documents_.rescheduled(d->id, was);
            }
        result = {true, "The courier will bring " + std::string(ids.size() == 1 ? "it" : "them") + " on from " + townWords(from) + " to " +
                            townWords(here) + " (" + std::to_string(fee) + "p).",
                  {}};
        sendLetters(c);
        return true;
    }
    auto* d = documents_.find(j.string("id"));
    if (!d || d->to != me || d->state == "travelling")
    {
        result = {false, "You have no such letter.", {}};
        return true;
    }
    if (verb == "read")
    {
        if (d->state == "waiting")
        {
            result = {false, "It waits at the inns of " + townWords(d->postTown) + "; go there, or have it sent on.", {}};
            return true;
        }
        const bool first = d->readAt < 0;
        d->readAt = world_.calendarDays();
        d->state = "read";
        std::string line = first ? "You break the seal." : "";
        // A signature introduces the writer by that name (doc 55, 2).
        if (first && !d->sign.empty())
        {
            line += " It is signed " + d->sign + ".";
            if (characters_.count(d->author) || world_.entity(d->author))
                learnName(me, d->author, d->sign, "letter");   // (A resident's name given in a letter too.)
        }
        result = {true, line, {}};
        sendLetters(c);
        saveSoon();
        return true;
    }
    if (verb == "take")
    {
        std::string said;
        if (d->readAt < 0 || d->escrow.empty())
            result = {false, d->readAt < 0 ? "Open it first." : "There is nothing in it.", {}};
        else
            result = {takeEnclosure(*d, me, said), said, {}};
        sendLetters(c);
        return true;
    }
    if (verb == "answer")
    {
        // An invitation: coming, or can't come.
        auto* o = d->occasion.empty() ? nullptr : world_.occasion(d->occasion);
        if (!o)
        {
            result = {false, d->occasion.empty() ? "It asks nothing of you." : "That day has come and gone.", {}};
            return true;
        }
        const bool yes = j.boolean("yes");
        d->answer = yes ? 1 : -1;
        if (yes)
            o->coming.insert(me);
        else
            o->coming.erase(me);
        result = {true, yes ? "You'll be there." : "You send word that you can't come.", {}};
        sendLetters(c);
        return true;
    }
    if (verb == "takeWork")
    {
        result = d->contract.empty() ? Result{false, "It offers no work.", {}} : world_.takeOfferedContract(me, d->contract);
        if (result.ok)
            record(Roads | Character, me);
        sendLetters(c);
        return true;
    }
    if (verb == "keep")
    {
        d->kept = !d->kept;
        result = {true, d->kept ? "You keep it." : "You no longer keep it apart.", {}};
        sendLetters(c);
        return true;
    }
    if (verb == "burn")
    {
        if (!d->escrow.empty())
            returnEnclosure(*d, "burnt unopened");     // (What it holds goes back: nothing is lost.)
        documents_.erase(d->id);
        result = {true, "You burn the letter.", {}};
        sendLetters(c);
        saveSoon();
        return true;
    }
    if (verb == "reply" && d->kind != "letter")
    {
        result = {false, "Go and speak to them; a letter needs no answer by post.", {}};
        return true;
    }
    if (verb == "reply")
    {
        // To a writer one knows by name; else, once, by the same courier, never learning who it was.
        const bool known = knowsName(me, d->author);
        if (!known && d->answered)
        {
            result = {false, "You have answered it already, by the same courier.", {}};
            return true;
        }
        const auto author = d->author, original = d->id;
        if (auto* sent = sendLetter(c, author, j.string("text"), j.string("sign"), result, original, !known); sent && !known)
            if (auto* again = documents_.find(original))
                again->answered = true;
        if (result.ok && !known)
            result.message = "The courier will take your answer back the way the letter came." +
                             result.message.substr(result.message.find(')') + 1);
        sendLetters(c);
        return true;
    }
    result = {false, "That isn't something you can do with a letter.", {}};
    return true;
}

// ------------------------------------------------------------------ Enclosures (doc 55, 3)

bool Game::takeEnclosure(documents::Document& d, const std::string& reader, std::string& said)
{
    // TAKE: to the reader's purse if it has room; otherwise it stays in the letter.
    auto& society = world_.society();
    const auto what = goodsWords(d.encItem, d.encQuantity, d.encCoins);
    const auto coins = d.encCoins;
    if (d.encCoins > 0 && society.shift(d.escrow, reader, "", 0, d.encCoins, "a letter's enclosure"))
        d.encCoins = 0;
    if (!d.encItem.empty())
    {
        if (!society.shift(d.escrow, reader, d.encItem, d.encQuantity, 0, "a letter's enclosure"))
        {
            said = (coins > 0 && d.encCoins == 0 ? "You take " + std::to_string(coins) + "p from the letter; you have no room for the rest, so it stays in the letter."
                                                 : std::string("You have no room for it; it stays in the letter."));
            if (coins > 0)
                record(Economy | Character, reader);
            return false;
        }
        if (d.scented)
            addScent(reader, d.encItem, d.encQuantity, "", d.author);
    }
    said = "You take " + what + " from the letter.";
    if (!d.encItem.empty())
        if (const auto smell = scentOfItem(reader, reader, d.encItem); !smell.empty())
            said += " " + names::capitalised(smell) + ".";
    d.encItem.clear();
    d.encQuantity = 0;
    d.escrow.clear();
    record(Economy | Character, reader);
    saveSoon();
    return true;
}

void Game::returnEnclosure(documents::Document& d, const std::string& why)
{
    // What a letter holds goes back to its writer (or, if the writer is gone too, to the capital's treasury).
    auto& society = world_.society();
    const auto to = characters_.count(d.author) ? d.author : society.treasuryOf(society.capital());
    if (d.encCoins > 0)
        society.shift(d.escrow, to, "", 0, d.encCoins, "a letter's enclosure");
    if (!d.encItem.empty())
        society.shift(d.escrow, to, d.encItem, d.encQuantity, 0, "a letter's enclosure");
    if (auto* c = clientOf(d.author))
        system(c, "A letter you sent came back " + why + ", with what you put in it: " + goodsWords(d.encItem, d.encQuantity, d.encCoins) + ".");
    world_.recordEvent({"letter returned", d.author, d.to, {}, 0, 0, d.encItem, d.encQuantity, d.encCoins, d.id});
    d.escrow.clear();
    d.encItem.clear();
    d.encQuantity = 0;
    d.encCoins = 0;
    record(Economy, d.author);
}

void Game::orphanedEnclosures()
{
    // An escrow account with no letter (lost in a crash between the economy's journal and the checkpoint) goes back to
    // its writer.
    std::vector<std::pair<std::string, std::string>> lost;
    for (const auto& [id, account] : world_.society().state().accounts)
        if (id.rfind("letter:", 0) == 0 && (account.cash > 0 || !account.stock.empty()))
        {
            const auto at = id.find('@');
            const auto doc = id.substr(7, at == std::string::npos ? std::string::npos : at - 7);
            if (!documents_.find(doc) && at != std::string::npos)
                lost.push_back({id, id.substr(at + 1)});
        }
    auto& society = world_.society();
    for (const auto& [escrow, writer] : lost)
        if (const auto* a = society.account(escrow))
        {
            const auto to = society.account(writer) ? writer : society.treasuryOf(society.capital());
            const auto held = *a;
            if (held.cash > 0)
                society.shift(escrow, to, "", 0, held.cash, "a letter's enclosure");
            for (const auto& [item, n] : held.stock)
                if (n > 0)
                    society.shift(escrow, to, item, n, 0, "a letter's enclosure");
        }
}

// ------------------------------------------------------------------ Pacts (doc 55, 8)

bool Game::pactCommand(Connection* c, const json::Value& j, Result& result)
{
    // {"verb": "write", "with": name, "text"} / "seal", "id" / "witness", "id", "by": name. Rules never judge the prose.
    const auto me = c->entityId;
    const auto verb = j.string("verb");
    std::string problem;
    const auto copies = [&](const std::string& pact) {
        std::vector<documents::Document*> out;
        for (const auto& [id, d] : documents_.all())
            if (d.pact == pact)
                out.push_back(documents_.find(id));
        return out;
    };
    if (verb == "write")
    {
        const auto other = knownByName(me, j.string("with"), problem);
        auto text = j.string("text");
        if (other.empty() || blocked(other, me))
            result = {false, other.empty() ? problem : "They want no pact with you.", {}};
        else if (text.empty() || int(text.size()) > 400 * 4 || [&] {
                     int n = 0;
                     for (unsigned char ch : text)
                         n += (ch & 0xC0) != 0x80;
                     return n > 400;
                 }())
            result = {false, "A pact's terms are 1 to 400 letters.", {}};
        else
        {
            const auto pact = "pact-" + std::to_string(nextPact_++);
            for (const auto& holder : {me, other})
            {
                documents::Document d;
                d.kind = "pact";
                d.author = me;
                d.scent = me;
                d.to = holder;
                d.party = other;
                d.text = text;
                d.pact = pact;
                d.seals = {me};
                d.state = "delivered";
                d.written = d.deliverAt = world_.calendarDays();
                if (holder == me)
                    d.readAt = d.written;
                documents_.add(d);
            }
            if (auto* oc = clientOf(other))
                system(oc, names::capitalised(labelFor(other, me)) + " sets down a pact between you, and asks you to seal it. It is in your letter case.");
            result = {true, "You set down the pact and seal it. A copy waits for " + labelFor(me, other) + " to seal.", {}};
        }
        if (auto* c2 = c)
            sendLetters(c2);
        return true;
    }
    auto* mine = documents_.find(j.string("id"));
    if (!mine || mine->to != me || mine->pact.empty())
    {
        result = {false, "You hold no such pact.", {}};
        return true;
    }
    const auto pact = mine->pact;
    if (verb == "seal")
    {
        if (std::find(mine->seals.begin(), mine->seals.end(), me) != mine->seals.end())
            result = {false, "You have sealed it already.", {}};
        else
        {
            for (auto* d : copies(pact))
                d->seals.push_back(me);
            const bool both = std::find(mine->seals.begin(), mine->seals.end(), mine->author) != mine->seals.end() &&
                              std::find(mine->seals.begin(), mine->seals.end(), mine->party) != mine->seals.end();
            if (both && (me == mine->author || me == mine->party))
                world_.recordEvent({"pact sealed", mine->author, mine->party, {}, 0, 0, {}, 0, 0, pact});
            for (auto* d : copies(pact))
                if (d->to != me)
                    if (auto* oc = clientOf(d->to))
                        system(oc, names::capitalised(labelFor(d->to, me)) + " has sealed the pact.");
            result = {true, me == mine->author || me == mine->party ? "You seal the pact." : "You seal it as a witness.", {}};
        }
    }
    else if (verb == "witness")
    {
        const auto witness = knownByName(me, j.string("by"), problem);
        if (me != mine->author && me != mine->party)
            result = {false, "Only those it binds may ask a witness.", {}};
        else if (witness.empty() || witness == mine->author || witness == mine->party || blocked(witness, me))
            result = {false, witness.empty() ? problem : "Not them.", {}};
        else if (mine->witnesses.size() >= 3)
            result = {false, "Three witnesses already.", {}};
        else
        {
            for (auto* d : copies(pact))
                d->witnesses.push_back(witness);
            auto copy = *mine;
            copy.id.clear();
            copy.to = witness;
            copy.readAt = -1;
            copy.kept = false;
            documents_.add(copy);
            if (auto* wc = clientOf(witness))
                system(wc, names::capitalised(labelFor(witness, me)) + " asks you to witness a pact. It is in your letter case.");
            result = {true, "You ask " + labelFor(me, witness) + " to witness it.", {}};
        }
    }
    else
        result = {false, "Seal it, or ask a witness.", {}};
    sendLetters(c);
    return true;
}

// ------------------------------------------------------------------ Saving

void Game::lettersSave(json::Value& root) const
{
    auto list = Value::array();
    for (const auto& [id, d] : documents_.all())
        list.push(documents::save(d));
    root.add("documents", list);
}

void Game::lettersLoad(const json::Value& saved)
{
    documents_.clear();
    for (const auto& e : saved.array("documents"))
    {
        auto d = documents::load(e);
        if (d.id.empty() || d.to.empty())
            continue;
        const auto& added = documents_.add(std::move(d));
        if (added.state == "carried")
            carried_.insert(added.id);
        if (added.pact.rfind("pact-", 0) == 0)
            if (const auto n = std::strtoull(added.pact.c_str() + 5, nullptr, 10); n >= nextPact_)
                nextPact_ = n + 1;
    }
    orphanedEnclosures();
}
} // namespace ratw::game
