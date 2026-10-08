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

namespace ratw
{
const Spot* World::boardSpot(const std::string& community)
{
    if (const auto found = boards_.find(community); found != boards_.end())
        return found->second.cell.empty() ? nullptr : &found->second;
    auto& spot = boards_[community];
    const auto& sq = square(community);
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
    const auto community = townFor(e->cellId);
    if (community.empty())
        return {};
    const auto* spot = world_.boardSpot(community);
    return spot && spot->cell == e->cellId && std::hypot(spot->x - e->position.x, spot->y - e->position.y) <= BoardReach ? community : std::string();
}

json::Value Game::boardsView(const std::string& cellId)
{
    auto list = Value::array();
    const auto community = townFor(cellId);
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
        return e && !e->dead && townFor(e->cellId) == community;
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
    if (int(onBoard.size()) >= PerBoard)
        return result = {false, "Every pin is taken. Try tomorrow.", {}}, true;
    if (std::count_if(onBoard.begin(), onBoard.end(), [&](const auto* d) { return d->author == me; }) >= PerWriter)
        return result = {false, "You have three notices up here already.", {}}, true;
    const auto treasury = world_.society().treasuryOf(community);
    if (world_.society().spendable(me) < NoticeFee || !world_.society().shift(me, treasury, "", 0, NoticeFee, "a notice pinned"))
        return result = {false, "A notice costs a penny to the town.", {}}, true;
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
    world_.recordEvent({"notice posted", me, {}, w ? w->cellId : std::string(), 0, 0, what, 0, NoticeFee, stored.id});
    record(Economy, me);
    sendBoard(c, community);
    return result = {true, "You pin your notice to the board (1p to the town). It stays up seven days.", {}}, true;
}

void Game::tendNotices()
{
    // Once a game hour: notices past their seven days come down; once a game day, unanswered ones are looked at again.
    std::vector<std::string> gone;
    for (const auto& [id, d] : documents_.all())
        if (d.kind == "notice" && d.expires <= world_.calendarDays())
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
