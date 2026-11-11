// Notice boards (Docs/Design/54-gathering-places.md, 2; Phase 2). One by each town's square (World::boardSpot), read from
// within 2 tiles. The work side is built when read: the town's open contracts (World::contractsNear's rule), with TAKE
// IT ON. The public side is players' notices: documents of kind "notice" (doc 55's store), a penny to the board's own
// town, seven game days, three a writer and thirty a board; each carries its writer's scent as a letter does. Residents
// answer a seeking notice's structured *what* (an item: a shop in town that has it; an apprenticeship: a master of the
// trade who trusts the writer), and an offering one (an item: a maker who uses it), never its words, which never reach
// a prompt. Checked on posting and once a game day; one answer a notice.
#include "RatwGame.h"

#include "RatwInjury.h"
#include "RatwItems.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <set>

namespace ratw
{
const Spot* World::boardSpot(const std::string& community)
{
    if (const auto found = boards_.find(community); found != boards_.end())
        return found->second.cell.empty() ? nullptr : &found->second;
    auto& spot = boards_[community];
    // A law town's square; else, for a community that is a market on the roads but plain for law (doc 79, D7), the square
    // around its market's place, so its wagons' notices and contracts have a board.
    const MarketSquare* found = &square(community);
    MarketSquare own;
    if (!found->found)
        if (const auto* m = roadMarket(community); m && !m->market.empty())
        {
            fillSquare(own, m->market, {m->marketX, m->marketY});
            found = &own;
        }
    const auto& sq = *found;
    const auto* c = sq.found ? cell(sq.at.cell) : nullptr;
    if (!c)
        return nullptr;
    // Open ground 2 to 4 tiles off, the nearest first, then by place: the same tile every time.
    const int ax = int(std::floor(sq.at.x)), ay = int(std::floor(sq.at.y));
    for (int r = 2; r <= 4 && spot.cell.empty(); ++r)
        for (int dy = -r; dy <= r && spot.cell.empty(); ++dy)
            for (int dx = -r; dx <= r && spot.cell.empty(); ++dx)
            {
                if (std::max(std::abs(dx), std::abs(dy)) != r)
                    continue;
                const auto* t = c->tile(ax + dx, ay + dy);
                if (!t || t->solid || t->glyph == '+' || t->glyph == 'u')
                    continue;
                const double x = ax + dx + .5, y = ay + dy + .5;
                if (std::any_of(sq.stalls.begin(), sq.stalls.end(), [&](const Spot& s) { return std::hypot(s.x - x, s.y - y) < .9; }))
                    continue;
                spot = {sq.at.cell, x, y};
            }
    return spot.cell.empty() ? nullptr : &spot;
}
} // namespace ratw

namespace ratw::game
{
using json::Value;

namespace
{
constexpr double BoardReach = 2, NoticeDays = 7;
constexpr int NoticeFee = 1, PerWriter = 3, PerBoard = 30, TextMost = 280;
constexpr const char* CartersSign = "the carters";   // (The wagons' notices: doc 79, 8.1.)
constexpr const char* WharfSign = "the Wharf office";   // (The ships' timetable: doc 79, 8.3.)
constexpr const char* StagedAuthor = "staged";        // (Staged events' and causes' notices, signed by their own: Phase 7.)
const char* const Kinds[] = {"seeking", "offering", "event", "lost", "other"};

std::string lowered(std::string s)
{
    for (auto& ch : s)
        ch = char(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

int lettersIn(const std::string& text)
{
    int n = 0;
    for (unsigned char ch : text)
        n += (ch & 0xC0) != 0x80;
    return n;
}
} // namespace

std::string Game::boardNear(const std::string& player)
{
    const auto* e = world_.entity(player);
    if (!e)
        return {};
    const auto community = boardTownFor(e->cellId);
    if (community.empty())
        return {};
    const auto* spot = world_.boardSpot(community);
    return spot && spot->cell == e->cellId && std::hypot(spot->x - e->position.x, spot->y - e->position.y) <= BoardReach ? community : std::string();
}

json::Value Game::boardsView(const std::string& cellId)
{
    auto list = Value::array();
    const auto community = boardTownFor(cellId);
    if (community.empty())
        return list;
    if (const auto* spot = world_.boardSpot(community); spot && spot->cell == cellId)
    {
        auto o = Value::object();
        o.add("id", "board:" + community);
        o.add("x", spot->x);
        o.add("y", spot->y);
        o.add("town", townWords(community));
        o.add("notices", int(documents_.onBoard("board:" + community).size()));
        list.push(o);
    }
    return list;
}

std::string Game::whatWords(const std::string& what) const
{
    if (what.rfind("item:", 0) == 0)
    {
        std::string name = Society::itemName(what.substr(5));
        if (!name.empty())
            name[0] = char(std::tolower(static_cast<unsigned char>(name[0])));
        return name;
    }
    if (what.rfind("apprenticeship:", 0) == 0)
    {
        const auto* b = items::business(what.substr(15));
        return "an apprenticeship" + (b ? " (" + b->label + ")" : std::string());
    }
    if (what == "room")
        return "a room";
    if (what == "partner")
        return "a hunting or work partner";
    return {};
}

std::string Game::noticeAnswerWords(const std::string& viewer, const documents::Document& d) const
{
    if (d.answeredBy.empty() || !world_.entity(d.answeredBy))
        return {};
    const auto who = names::capitalised(labelFor(viewer, d.answeredBy));
    const auto thing = whatWords(d.what);
    if (d.answeredHow == "has")
        return who + " has " + thing + ".";
    if (d.answeredHow == "takes")
        return who + " would buy " + thing + ".";
    if (d.answeredHow == "apprentice")
        return who + " would take on an apprentice; ask at the workshop.";
    return {};
}

void Game::answerNotice(documents::Document& d)
{
    // A resident in the board's town answers its structured `what`, once.
    if (!d.answeredBy.empty() || d.what.empty())
        return;
    const auto community = d.board.substr(6);
    auto& society = world_.society();
    const auto inTown = [&](const std::string& id) {
        const auto* e = world_.entity(id);
        return e && !e->dead && boardTownFor(e->cellId) == community;
    };
    if (d.what.rfind("item:", 0) == 0)
    {
        const auto item = d.what.substr(5);
        for (const auto& p : society.positions())
        {
            const auto holder = society.state().careers.positions.count(p.id) ? society.state().careers.positions.at(p.id).holder : std::string();
            if (holder.empty() || !inTown(holder))
                continue;
            const auto* business = items::businessFor(p.title);
            if (!business)
                continue;
            if (d.noticeKind == "seeking")
            {
                const auto* till = society.account(society.tillOf(holder));
                if (till && Society::stockAll(*till, item) > 0)
                {
                    d.answeredBy = holder, d.answeredHow = "has";
                    return;
                }
            }
            else if (d.noticeKind == "offering")
                for (const auto* craft : items::craftsFor(business->id))
                    for (const auto& [in, n] : craft->in)
                        if (in == item)
                        {
                            d.answeredBy = holder, d.answeredHow = "takes";
                            return;
                        }
        }
    }
    else if (d.what.rfind("apprenticeship:", 0) == 0 && d.noticeKind == "seeking")
    {
        // A master of that trade in town who trusts the writer (30).
        const auto wanted = d.what.substr(15);
        for (const auto& p : society.positions())
        {
            const auto holder = society.state().careers.positions.count(p.id) ? society.state().careers.positions.at(p.id).holder : std::string();
            const auto* business = items::businessFor(p.title);
            const auto* bond = holder.empty() ? nullptr : world_.bonds().find(holder, d.author);
            if (business && business->id == wanted && inTown(holder) && bond && bond->trust >= 30)
            {
                d.answeredBy = holder, d.answeredHow = "apprentice";
                return;
            }
        }
    }
    if (!d.answeredBy.empty())
        world_.recordEvent({"notice answered", d.answeredBy, d.author, {}, 0, 0, d.what, 0, 0, d.id});
}

void Game::sendBoard(Connection* c, const std::string& community)
{
    const auto me = c->entityId;
    auto work = Value::array();
    for (const auto* k : world_.contractsNear(me))
    {
        if (!k->offeredTo.empty() && k->offeredTo != "-" && k->offeredTo != me && k->offeredUntil > world_.calendarDays())
            continue;                               // (Offered to someone else for now: doc 55.)
        auto o = Value::object();
        o.add("id", k->id);
        o.add("kind", k->kind);
        o.add("text", k->detail);
        o.add("reward", k->reward);
        o.add("canTake", k->poster != me);
        work.push(o);
    }
    auto notices = Value::array();
    for (const auto* d : documents_.onBoard("board:" + community))
    {
        if (blocked(me, d->author))
            continue;                               // (A blocked wolf's notices are hidden from the one who blocked: doc 50.)
        auto o = Value::object();
        o.add("id", d->id);
        o.add("kind", d->noticeKind);
        o.add("text", d->text);
        o.add("what", whatWords(d->what));
        o.add("sign", d->sign);
        o.add("scent", scentLine(me, *d));
        o.add("when", injury::dateWords(d->written));
        o.add("days", std::max(0., std::round((d->expires - world_.calendarDays()) * 10) / 10));
        o.add("mine", d->author == me);
        if (const auto answer = noticeAnswerWords(me, *d); !answer.empty())
            o.add("answer", answer);
        notices.push(o);
    }
    auto e = Value::object();
    e.add("type", "board");
    e.add("town", townWords(community));
    e.add("work", work);
    e.add("notices", notices);
    e.add("noticeFee", double(NoticeFee));   // (For the form.)
    if (const auto festival = festivalBoard(community); !festival.isNull())
        e.add("festival", festival);                // (The festival's programme, from three days before: doc 54, 6.)
    send(c, e);
}

bool Game::boardCommand(Connection* c, const json::Value& j, Result& result)
{
    // {"verb": "read"} / "post" {kind, text, what, sign} / "unpost" {id} / "take" {id}.
    const auto me = c->entityId;
    const auto verb = j.string("verb", "read");
    const auto community = boardNear(me);
    if (community.empty())
        return result = {false, "Go to the notice board by the square.", {}}, true;
    if (verb == "read")
    {
        sendBoard(c, community);
        return result = {true, "", {}}, true;
    }
    if (verb == "take")
    {
        result = world_.takeContract(me, j.string("id"));
        if (result.ok)
            record(Roads | Character, me);
        sendBoard(c, community);
        return true;
    }
    const auto board = "board:" + community;
    if (verb == "unpost")
    {
        auto* d = documents_.find(j.string("id"));
        if (!d || d->board != board || d->author != me)
            return result = {false, "That isn't your notice.", {}}, true;
        documents_.erase(d->id);
        record(Economy, me);                        // (Down for good: the journal has the boards whole, Game::record.)
        sendBoard(c, community);
        return result = {true, "You take your notice down.", {}}, true;
    }
    if (verb != "post")
        return result = {false, "Read the board, or pin a notice.", {}}, true;
    auto* w = world_.entity(me);
    const auto kind = j.string("kind", "other");
    auto text = j.string("text");
    text.erase(0, text.find_first_not_of(" \t\r\n"));
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        text.pop_back();
    if (std::none_of(std::begin(Kinds), std::end(Kinds), [&](const char* k) { return kind == k; }))
        return result = {false, "Seeking, offering, an event, lost and found, or other.", {}}, true;
    if (text.empty() || lettersIn(text) > TextMost)
        return result = {false, "A notice is 1 to " + std::to_string(TextMost) + " letters.", {}}, true;
    // What it seeks or offers, from a fixed list: an item from the catalog (by its id or name), an apprenticeship in a
    // trade, a room, a partner.
    std::string what;
    const auto whatType = j.string("whatType"), whatValue = lowered(j.string("whatValue"));
    if ((kind == "seeking" || kind == "offering") && !whatType.empty())
    {
        if (whatType == "item")
        {
            if (items::good(whatValue))
                what = "item:" + whatValue;
            else
                for (const auto& g : items::allGoods())
                    if (lowered(g.name) == whatValue || lowered(g.id) == whatValue)
                        what = "item:" + g.id;
            if (what.empty())
                return result = {false, "No such good in the catalog: \"" + j.string("whatValue") + "\".", {}}, true;
        }
        else if (whatType == "apprenticeship")
        {
            for (const auto& b : items::businesses())
                if (lowered(b.id) == whatValue || lowered(b.label) == whatValue || lowered(b.label).find(whatValue) != std::string::npos)
                    what = "apprenticeship:" + b.id;
            if (what.empty())
                return result = {false, "No such trade: \"" + j.string("whatValue") + "\".", {}}, true;
        }
        else if (whatType == "room" || whatType == "partner")
            what = whatType;
    }
    const auto sign = j.string("sign");
    if (!sign.empty())
    {
        const auto mine = namesOf(me);
        if (std::find(mine.begin(), mine.end(), sign) == mine.end())
            return result = {false, "Sign with one of your own names, or leave it unsigned.", {}}, true;
    }
    const auto onBoard = documents_.onBoard(board);
    // (The carters' wagon notices have pins of their own: they never take one a player needs.)
    if (std::count_if(onBoard.begin(), onBoard.end(), [](const auto* d) {
            return d->author != CartersSign && d->author != WharfSign && d->author != StagedAuthor;
        }) >= PerBoard)
        return result = {false, "Every pin is taken. Try tomorrow.", {}}, true;
    if (std::count_if(onBoard.begin(), onBoard.end(), [&](const auto* d) { return d->author == me; }) >= PerWriter)
        return result = {false, "You have three notices up here already.", {}}, true;
    const auto treasury = world_.society().treasuryOf(community);
    const auto fee = NoticeFee;
    if (world_.society().cash(me) < fee || !world_.society().shift(me, treasury, "", 0, fee, "a notice pinned"))
        return result = {false, "A notice costs " + priceWords(fee) + " to the town.", {}}, true;
    documents::Document d;
    d.kind = "notice";
    d.author = me;
    d.scent = w && world_.scentMasked(*w) ? std::string() : me;
    d.board = board;
    d.noticeKind = kind;
    d.what = what;
    d.text = text;
    d.sign = sign;
    d.state = "delivered";
    d.written = d.deliverAt = world_.calendarDays();
    d.expires = d.written + NoticeDays;
    d.fromTown = community;
    auto& stored = documents_.add(d);
    answerNotice(stored);
    world_.recordEvent({"notice posted", me, {}, w ? w->cellId : std::string(), 0, 0, what, 0, fee, stored.id});
    record(Economy, me);
    sendBoard(c, community);
    return result = {true, "You pin your notice to the board (" + std::to_string(fee) + "p to the town). It stays up seven days.", {}}, true;
}

namespace
{
std::pair<std::string, std::string> wagonNoticeIds(const std::string& departure)
{
    return {"wagon-" + departure + "-from", "wagon-" + departure + "-to"};
}

const std::string RobbedWords = "Robbed on the road: ";   // (Before a robbed wagon's far-end notice.)

// A wagon's notice with its "Escort wanted" taken out (it has set out: nobody can join it now). False if it had none.
bool noEscortWanted(std::string& text)
{
    const auto at = text.find(" Escort wanted");
    if (at == std::string::npos)
        return false;
    const auto end = text.find('.', at);
    text.erase(at, end == std::string::npos ? std::string::npos : end + 1 - at);
    return true;
}
} // namespace

void Game::pinCartersNotice(const std::string& id, const std::string& market, const std::string& text, const std::string& departure)
{
    // (On the board a player at the market's place reads, boardNear's: its law town's where it is in one, else its own.)
    const auto* m = world_.roadMarket(market);
    auto community = m ? boardTownFor(m->market) : std::string();
    if (community.empty())
        community = market;
    if (community.empty() || text.empty())
        return;
    documents::Document d;
    d.id = id;
    d.kind = "notice";
    d.author = CartersSign;
    d.board = "board:" + community;
    d.noticeKind = "event";
    d.what = "wagon:" + departure;
    d.text = json::utf8Prefix(text, TextMost);
    d.sign = CartersSign;
    d.state = "delivered";
    d.written = d.deliverAt = world_.calendarDays();
    d.expires = d.written + NoticeDays;
    d.fromTown = community;
    documents_.add(d);
}

void Game::mendRoadNotices()
{
    // At a start (the repair after Phases 4 and 5 merged): the boards' notices come from the checkpoint, but the Board and
    // the roads may be further on by the journal (a wagon posted, an escort taken, a wagon in or set out after it). The
    // carters' notices are made to say what the Board and the roads say now, as their events would have: a wagon still
    // to set out has both, written afresh; one on the road keeps them, without asking for an escort once it is out; one
    // robbed keeps its far end's, saying so (the raid may be the journal's alone); one come in, gone home or gone keeps
    // none. And a port's timetable with nothing to tell comes down (its words gone since: the ships' rule stopped them),
    // as postShips would have taken it down.
    // (The boards themselves are journalled, Game::record; but a world tick's step and the notice it changes are told
    // apart, the step in that tick's record and the notice after it, so a crash between the two leaves them out of step.)
    std::set<std::string> committed;
    for (const auto& d : world_.society().board().committed)
    {
        if (d.kind != "wagon")
            continue;
        committed.insert(d.id);
        const auto [from, to] = wagonNoticeIds(d.id);
        for (const bool atEnd : {false, true})
        {
            auto text = json::utf8Prefix(world_.departureNotice(d, atEnd), TextMost);
            if (auto* pinned = documents_.find(atEnd ? to : from))
                pinned->text = std::move(text);
            else
                pinCartersNotice(atEnd ? to : from, atEnd ? d.to : d.from, text, d.id);
        }
    }
    std::map<std::string, const Caravan*> onRoad;
    for (const auto& c : world_.roads().caravans)
        if (!c.departure.empty())
            onRoad[c.departure] = &c;
    std::vector<std::string> gone;
    for (const auto& [id, d] : documents_.all())
    {
        if (d.kind != "notice" || d.author != CartersSign || d.what.rfind("wagon:", 0) != 0)
            continue;
        const auto departure = d.what.substr(6);
        if (committed.count(departure))
            continue;
        const auto c = onRoad.find(departure);
        const auto status = c == onRoad.end() ? std::string() : c->second->status;
        const bool far = id == wagonNoticeIds(departure).second;
        if (status == "travelling" || (status == "raided" && far))
            continue;
        gone.push_back(id);
    }
    for (const auto& [id, d] : documents_.all())
        if (d.kind == "notice" && d.author == WharfSign && d.what.rfind("ships:", 0) == 0 && world_.shipsNotice(d.what.substr(6)).empty())
            gone.push_back(id);
    for (const auto& id : gone)
        documents_.erase(id);
    for (const auto& [departure, c] : onRoad)
        if (c->status == "raided")
            if (auto* pinned = documents_.find(wagonNoticeIds(departure).second); pinned && pinned->text.rfind(RobbedWords, 0) != 0)
            {
                pinned->text = RobbedWords + pinned->text;          // (As roadNotices' raid has it.)
                pinned->expires = std::min(pinned->expires, world_.calendarDays() + 2);
            }
    for (const auto& [departure, c] : onRoad)
        if (c->status == "travelling" && !c->cell.empty())
            for (const auto& id : {wagonNoticeIds(departure).first, wagonNoticeIds(departure).second})
                if (auto* pinned = documents_.find(id))
                    noEscortWanted(pinned->text);
    // Staged events (Phase 7): a notice up for each event posted or on, with its words, and none for one over, cancelled
    // or gone (an event's step is the tick's record, its notice the next: a crash between leaves them apart).
    std::map<std::string, const market::StagedEvent*> live;
    for (const auto& e : world_.society().board().events)
        if ((e.status == "due" || e.status == "on") && e.announced && !e.notice.empty())
            live["event-" + e.id] = &e;
    std::vector<std::string> over;
    for (const auto& [id, d] : documents_.all())
        if (d.kind == "notice" && d.author == StagedAuthor && d.what.rfind("event:", 0) == 0 && !live.count(id))
            over.push_back(id);
    for (const auto& id : over)
        documents_.erase(id);
    for (const auto& [id, e] : live)
        if (auto* pinned = documents_.find(id))
            pinned->text = json::utf8Prefix(e->notice, TextMost);
        else
        {
            const auto* kind = world_.society().market().eventKind(e->kind);
            pinStagedNotice(id, e->market, "event:" + e->id, e->notice, kind ? kind->sign : std::string("the crier"));
        }
    // The DM's Times nudges and harvest overrides that lapsed: their notices down.
    std::vector<std::string> lapsed;
    const auto& board = world_.society().board();
    for (const auto& [id, d] : documents_.all())
        if (d.kind == "notice" && d.author == StagedAuthor &&
            ((d.what.rfind("nudge:", 0) == 0 && !board.nudges.count(d.what.substr(6))) ||
             (d.what.rfind("harvest:", 0) == 0 &&
              (!board.harvestOverride.count(d.what.substr(8)) || world_.calendarDays() >= board.harvestOverride.at(d.what.substr(8)).until))))
            lapsed.push_back(id);
    for (const auto& id : lapsed)
        documents_.erase(id);
}

void Game::pinStagedNotice(const std::string& id, const std::string& market, const std::string& what, const std::string& text,
                           const std::string& sign, double until)
{
    // (On the board a player at the market's place reads, as the carters' notices are.)
    const auto* m = world_.roadMarket(market);
    auto community = m ? boardTownFor(m->market) : std::string();
    if (community.empty())
        community = market;
    if (community.empty() || text.empty())
        return;
    documents::Document d;
    d.id = id;
    d.kind = "notice";
    d.author = StagedAuthor;
    d.board = "board:" + community;
    d.noticeKind = "event";
    d.what = what;
    d.text = json::utf8Prefix(text, TextMost);
    d.sign = sign.empty() ? std::string("the crier") : sign;
    d.state = "delivered";
    d.written = d.deliverAt = world_.calendarDays();
    d.expires = d.written + NoticeDays;
    if (until > d.written)
        d.expires = std::min(d.expires, until);   // (A nudge's or a harvest override's: down when it lapses, tendNotices.)
    d.fromTown = community;
    documents_.add(d);
}

void Game::stagedNotices(const WorldEvent& e)
{
    // Staged events and causes (doc 79, 8.4-8.6; Phase 7). (actor: the event's id, or the market or area; target: its
    // market; item: the event's kind; detail: the words.)
    if (e.kind == "event posted" || e.kind == "event begins")
    {
        const auto* ev = world_.stagedEvent(e.actor);
        if (!ev)
            return;
        const auto* kind = world_.society().market().eventKind(ev->kind);
        auto* pinned = documents_.find("event-" + ev->id);
        if (!ev->notice.empty() && pinned && e.kind == "event begins")
            pinned->text = json::utf8Prefix(ev->notice, TextMost);   // (Its notice once begun: a rich cargo's ship is in.)
        else if (!ev->notice.empty() && (e.kind == "event posted" || !pinned))
            pinStagedNotice("event-" + ev->id, ev->market, "event:" + ev->id, ev->notice, kind ? kind->sign : std::string("the crier"));
        if (e.kind == "event begins" && !ev->cry.empty())
            stagedCry(ev->market, ev->cry);
        saveSoon();
    }
    else if (e.kind == "event over" || e.kind == "event cancelled")
    {
        documents_.erase("event-" + e.actor);
        saveSoon();
    }
    else if (e.kind == "times cause")
    {
        // (Every market's board, and its crier; the cause's number names the notice, so a new cause is a new notice.)
        pinStagedNotice("times-" + e.target, e.target, "times:" + e.target, e.detail, "the crier");
        if (const auto& cause = world_.society().board().sliderCause; !cause.cry.empty())
            stagedCry(e.target, cause.cry);
        saveSoon();
    }
    else if (e.kind == "times nudged")
    {
        const auto& nudges = world_.society().board().nudges;
        pinStagedNotice("nudge-" + e.target, e.target, "nudge:" + e.target, e.detail, "the crier",
                        nudges.count(e.target) ? nudges.at(e.target).until : 0);
        saveSoon();
    }
    else if (e.kind == "nudge over")
    {
        documents_.erase("nudge-" + e.target);
        saveSoon();
    }
    else if (e.kind == "harvest over")
    {
        // (The DM's harvest override lifted: its notices down at once. Phase 9's World::clearHarvest.)
        std::vector<std::string> down;
        for (const auto& [id, d] : documents_.all())
            if (d.kind == "notice" && d.author == StagedAuthor && d.what == "harvest:" + e.actor)
                down.push_back(id);
        for (const auto& id : down)
            documents_.erase(id);
        saveSoon();
    }
    else if (e.kind == "prices reset")
    {
        // (The DM's "reset prices" at a market: its board says so, unless the rule overrideNotices is 0. Phase 9.)
        if (world_.society().rule("overrideNotices") > 0 && !e.detail.empty())
        {
            auto words = e.detail;
            words[0] = char(std::toupper(static_cast<unsigned char>(words[0])));
            pinStagedNotice("reset-" + e.target, e.target, "reset:" + e.target, words + ": prices are back to their usual.", "the crier",
                            world_.calendarDays() + 1);
        }
        saveSoon();
    }
    else if (e.kind == "harvest called")
    {
        const auto& overrides = world_.society().board().harvestOverride;
        pinStagedNotice("harvest-" + e.actor + "-" + e.target, e.target, "harvest:" + e.actor, e.detail, "the crier",
                        overrides.count(e.actor) ? overrides.at(e.actor).until : 0);
        saveSoon();
    }
}

void Game::stagedCry(const std::string& market, const std::string& line)
{
    if (line.empty() || pendingCries_.size() >= 64)
        return;
    for (const auto& c : pendingCries_)
        if (c.market == market && c.line == line)
            return;
    pendingCries_.push_back({market, line, world_.calendarDays() + 1});
}

void Game::tendStagedCries()
{
    // A cry waits for the daytime (8:00 to 20:00) and for the market's crier to be free of another call; then the crier goes
    // to the square and calls it, where a player in the square's cell can hear (tendCriers). Not cried in a day: dropped.
    if (pendingCries_.empty())
        return;
    const double now = world_.calendarDays(), hour = (now - std::floor(now)) * 24;
    for (auto it = pendingCries_.begin(); it != pendingCries_.end();)
    {
        if (now > it->until)
        {
            it = pendingCries_.erase(it);
            continue;
        }
        const auto* m = world_.roadMarket(it->market);
        auto community = m ? boardTownFor(m->market) : std::string();
        if (community.empty())
            community = it->market;
        const auto* square = world_.marketSpot(community);
        if (!square)
        {
            it = pendingCries_.erase(it);           // (No square to cry it at: the notice says it.)
            continue;
        }
        if (hour < 8 || hour >= 20 || criers_.count(community))
        {
            ++it;
            continue;
        }
        // (The town's crier, crierOf's: its official, else the innkeeper nearest the square, else its priest; for a market
        // plain for law, the same of those who work at the market.)
        auto crier = crierOf(community);
        if (crier.empty())
            crier = crierOf(community, it->market);
        if (crier.empty())
        {
            it = pendingCries_.erase(it);
            continue;
        }
        CrierCall call{community, crier, square->cell, {it->line}, 0, world_.time(), it->market};
        world_.seatResident(crier, {square->cell, square->x, square->y + 1, "Crying the news at the square."});
        criers_[community] = std::move(call);
        world_.recordEvent({"crier", crier, {}, square->cell, 0, 0, {}, 1, 0, it->line});
        it = pendingCries_.erase(it);
    }
}

void Game::yieldStagedCry(const std::string& community)
{
    // A festival's crier or a legend's comes first: a staged cry holding the crier gives way, its lines not yet called
    // waiting again (a day) for the crier to be free.
    const auto found = criers_.find(community);
    if (found == criers_.end() || found->second.market.empty())
        return;
    const auto call = found->second;
    criers_.erase(found);
    for (auto i = call.next; i < call.lines.size(); ++i)
        stagedCry(call.market, call.lines[i]);
    world_.unseatResident(call.crier);
}

void Game::roadNotices(const WorldEvent& e)
{
    // The carters' notices (doc 79, 8.1 and 8.6): a wagon posted two days ahead is pinned on both ends' boards ("the
    // carters", with the calendar's weekday names), changed when it is set back or overdue, and taken down when it comes
    // in (or is cancelled). Seven days at most, as any notice.
    const auto ids = wagonNoticeIds;
    const auto pin = [&](const std::string& id, const std::string& market, const std::string& text, const std::string& departure) {
        pinCartersNotice(id, market, text, departure);
    };
    if (e.kind == "wagon posted" || e.kind == "wagon delayed")
    {
        // (actor: the departure; item: its source; target: its destination; detail: the two notices, a line each.)
        const auto cut = e.detail.find('\n');
        const auto [from, to] = ids(e.actor);
        pin(from, e.item, e.detail.substr(0, cut), e.actor);
        pin(to, e.target, cut == std::string::npos ? std::string() : e.detail.substr(cut + 1), e.actor);
        saveSoon();
    }
    else if (e.kind == "wagon overdue" && !e.item.empty())
    {
        if (auto* d = documents_.find(ids(e.item).second))
        {
            d->text = json::utf8Prefix(e.detail, TextMost);
            d->expires = std::max(d->expires, world_.calendarDays() + 2);
        }
        saveSoon();
    }
    else if ((e.kind == "caravan arrives" || e.kind == "raid") && !e.item.empty())
    {
        const auto [from, to] = ids(e.item);
        documents_.erase(from);
        if (e.kind == "caravan arrives")
            documents_.erase(to);
        else if (auto* d = documents_.find(to))
        {
            d->text = RobbedWords + d->text;
            d->expires = std::min(d->expires, world_.calendarDays() + 2);
        }
        saveSoon();
    }
    else if (e.kind.rfind("contract ", 0) == 0 && e.detail.rfind("escort:", 0) == 0)
    {
        // An escort asked for, taken or settled (doc 79, Phase 4, third repair): the notices of the wagons still to set
        // out say whether one is wanted as they would if pinned now, so none goes on asking for an escort it has, and
        // one asked for after a raid is asked for on the board as well.
        bool changed = false;
        for (const auto& d : world_.society().board().committed)
        {
            if (d.kind != "wagon")
                continue;
            const auto [from, to] = ids(d.id);
            for (const bool atEnd : {false, true})
                if (auto* pinned = documents_.find(atEnd ? to : from))
                    if (auto text = json::utf8Prefix(world_.departureNotice(d, atEnd), TextMost); text != pinned->text)
                    {
                        pinned->text = std::move(text);
                        changed = true;
                    }
        }
        if (changed)
            saveSoon();
    }
    else if (e.kind == "caravan departs" && !e.item.empty())
    {
        // The wagon has set out: nobody can join it now, whatever its notices asked for.
        const auto [from, to] = ids(e.item);
        bool changed = false;
        for (const auto& id : {from, to})
            if (auto* pinned = documents_.find(id))
                changed = noEscortWanted(pinned->text) || changed;
        if (changed)
            saveSoon();
    }
    else if (e.kind == "wagon cancelled" || ((e.kind == "caravan lost" || e.kind == "caravan dropped") && !e.item.empty()))
    {
        // (A wagon that will never come: cancelled before it set out, or lost or dropped on the road. A lost or dropped
        // one names its departure as the event's item.)
        const auto [from, to] = ids(e.kind == "wagon cancelled" ? e.actor : e.item);
        documents_.erase(from);
        documents_.erase(to);
        saveSoon();
    }
}

void Game::portNotices(const WorldEvent& e)
{
    // The ships' timetable (doc 79, 8.3 and D8; Phase 5): one notice a port, "the Wharf office"'s, on the board a player
    // at the port's market reads; posted afresh whenever its words change and each morning, taken down with nothing to
    // tell or the port gone. (actor: the port's market; detail: the words.)
    if (e.kind != "ships posted" && e.kind != "ships cleared")
        return;
    const auto id = "ships-" + e.actor;
    if (e.kind == "ships cleared" || e.detail.empty())
    {
        documents_.erase(id);
        saveSoon();
        return;
    }
    const auto* m = world_.roadMarket(e.actor);
    auto community = m ? boardTownFor(m->market) : std::string();
    if (community.empty())
        community = e.actor;
    if (community.empty())
        return;
    documents::Document d;
    d.id = id;
    d.kind = "notice";
    d.author = WharfSign;
    d.board = "board:" + community;
    d.noticeKind = "event";
    d.what = "ships:" + e.actor;
    d.text = json::utf8Prefix(e.detail, TextMost);
    d.sign = WharfSign;
    d.state = "delivered";
    d.written = d.deliverAt = world_.calendarDays();
    d.expires = d.written + NoticeDays;
    d.fromTown = community;
    documents_.add(d);
    saveSoon();
}

void Game::tendNotices()
{
    // Once a game hour: notices past their seven days come down (and a ships' notice whose port is gone: Phase 5); once a
    // game day, unanswered ones are looked at again.
    std::vector<std::string> gone;
    for (const auto& [id, d] : documents_.all())
        if (d.kind == "notice" && (d.expires <= world_.calendarDays() || (d.what.rfind("ships:", 0) == 0 && !world_.port(d.what.substr(6)))))
            gone.push_back(id);
    for (const auto& id : gone)
        documents_.erase(id);
    if (const double day = std::floor(world_.calendarDays()); day != noticesDay_)
    {
        noticesDay_ = day;
        std::vector<std::string> open;
        for (const auto& [id, d] : documents_.all())
            if (d.kind == "notice" && d.answeredBy.empty() && !d.what.empty())
                open.push_back(id);
        for (const auto& id : open)
            if (auto* d = documents_.find(id))
                answerNotice(*d);
    }
}
} // namespace ratw::game
