// Tavern games at the table (Docs/Design/54-gathering-places.md, 5; Phase 5). The rules are RatwTavernGames.cpp's;
// here, the tables: a `T` tile in a common room (Game::innCells_) or a venue opened for the night, played from within
// 1.5 tiles. A wolf starts a game there (Knucklebones, Wolves and Deer, Liar's Bones), others join, or it asks the room
// and a resident takes a seat: awake, not at work, 14 or more (16 for stakes). The game begins with two or more (Wolves
// and Deer, two). Liar's Bones may be played for 0 to 5p each, held in `table:<id>` and paid to the winner less a
// penny to the house (the inn's till; a venue's landlord; else the town). Residents stake only from purses of 30p or
// more, 3 staked games a game day, never over a twentieth of the purse. A seated resident keeps its seat (World's
// errand) and moves after two seconds. Each move shows to the table and to watchers within 4 tiles; talk is talk, and
// counts toward scenes like any other. A wolf gone from the table a minute, or not moving for ten, forfeits its stake.
// Playing sharpens a wolf's skill at the game (Entity::gameSkills, 0..100): faster against a better player.
// Knucklebones' catches read it; against residents a sharp Liar's Bones player now and then sees a bluffer's tell.
#include "RatwGame.h"

#include "RatwItems.h"

#include <algorithm>
#include <cmath>

namespace ratw::game
{
using json::Value;

namespace
{
constexpr double TableReach = 1.5, WatchReach = 4, ResidentMoveSeconds = 2, AwaySeconds = 60, IdleSeconds = 600,
                 UnbegunSeconds = 600;
constexpr std::int64_t StakeMost = 5, HousePenny = 1, ResidentPurseLeast = 30;
constexpr int StakedGamesADay = 3, LogLines = 8;

int seatsMost(const std::string& game) { return game == "wolves" ? 2 : 4; }
std::string gameName(const std::string& game)
{
    return game == "knucklebones" ? "Knucklebones" : game == "wolves" ? "Wolves and Deer" : "Liar's Bones";
}
template <class T> std::string account(const T& t) { return "table:" + t.id; }   // (Its stakes.)
template <class T> int turnOf(const T& t)
{
    if (!t.begun)
        return -1;
    if (t.game == "knucklebones")
        return t.knuckles.winner >= 0 ? -1 : t.knuckles.turn;
    if (t.game == "wolves")
        return t.board.winner >= 0 ? -1 : t.board.turn;
    return t.liars.winner >= 0 ? -1 : t.liars.turn;
}
template <class T> int winnerOf(const T& t)
{
    return t.game == "knucklebones" ? t.knuckles.winner : t.game == "wolves" ? t.board.winner : t.liars.winner;
}
} // namespace

std::string Game::seatName(const std::string& viewer, const std::string& who) const
{
    return who == viewer ? std::string("you") : labelFor(viewer, who);
}

std::string Game::tableNear(const Entity& e, int& tx, int& ty)
{
    // The nearest table within reach, in a common room or a place opened for the night.
    refreshLetterPlaces();
    const auto* venue = wholeLodgingAt(e.cellId);
    if (!innCells_.count(e.cellId) && !(venue && venue->open))
        return {};
    const auto* c = world_.cell(e.cellId);
    if (!c)
        return {};
    double best = TableReach + 1e-9;
    std::string key;
    const int cx = int(std::floor(e.position.x)), cy = int(std::floor(e.position.y));
    for (int y = cy - 2; y <= cy + 2; ++y)
        for (int x = cx - 2; x <= cx + 2; ++x)
            if (const auto* t = c->tile(x, y); t && t->glyph == 'T')
                if (const double d = std::hypot(x + .5 - e.position.x, y + .5 - e.position.y); d <= best)
                {
                    best = d;
                    tx = x, ty = y;
                    key = e.cellId + "|" + std::to_string(x) + "|" + std::to_string(y);
                }
    return key;
}

void Game::tableSay(Table& t, const std::string& who, const std::string& what)
{
    // The table's own log (shown in its panel), and a line to whoever sits at it or watches within 4 tiles.
    t.log.push_back({who, what});
    while (int(t.log.size()) > LogLines)
        t.log.erase(t.log.begin());
    for (auto* other : clients_)
        if (other)
            if (const auto* o = world_.entity(other->entityId);
                o && o->cellId == t.cell && std::hypot(o->position.x - t.x - .5, o->position.y - t.y - .5) <= WatchReach)
                system(other, who.empty() ? what : names::capitalised(seatName(other->entityId, who)) + what);
}

void Game::endTable(Table& t, int winner, const std::string& why)
{
    // The pot to the winner, less the house's penny when there were stakes; else back to those still seated.
    auto& society = world_.society();
    const auto* pot = society.account(account(t));
    const auto coins = pot ? pot->cash : 0;
    std::string paid;
    if (winner >= 0 && winner < int(t.seats.size()))
    {
        const auto& who = t.seats[std::size_t(winner)];
        const auto cut = coins > 0 ? std::min(HousePenny, coins) : 0;
        if (cut > 0)
            society.shift(account(t), t.house, "", 0, cut, "the house's penny");
        if (coins - cut > 0)
            society.shift(account(t), who, "", 0, coins - cut, "a game won");
        paid = coins - cut > 0 ? " (" + std::to_string(coins - cut) + "p)" : "";
        world_.recordEvent({"table game", who, {}, t.cell, 0, 0, t.game, int(t.seats.size()), coins, gameName(t.game)});
        for (auto* other : clients_)
            if (other && std::find(t.seats.begin(), t.seats.end(), other->entityId) != t.seats.end())
                system(other, why + " " + names::capitalised(seatName(other->entityId, who)) + (who == other->entityId ? " win" : " wins") +
                                  paid + ".");
    }
    else if (coins > 0 && !t.seats.empty())
    {
        const auto share = coins / std::int64_t(t.seats.size());
        for (const auto& who : t.seats)
            if (share > 0)
                society.shift(account(t), who, "", 0, share, "stakes returned");
        if (const auto* left = society.account(account(t)); left && left->cash > 0)
            society.shift(account(t), t.house, "", 0, left->cash, "the house's penny");
    }
    // Practice: each wolf at the table sharpens, faster against a better player.
    if (winner >= 0)
        for (const auto& who : t.seats)
        {
            auto* e = world_.entity(who);
            if (!e || e->npc)
                continue;
            double best = 0;
            for (const auto& other : t.seats)
                if (other != who)
                    if (const auto* o = world_.entity(other))
                        best = std::max(best, o->npc ? tavern::residentSkill(o->id, o->age) : (o->gameSkills.count(t.game) ? o->gameSkills.at(t.game) : 0.));
            auto& skill = e->gameSkills[t.game];
            const int before = int(skill);
            skill = std::min(100., skill + (best > skill ? 2. : 1.) * (1 - skill / 120));
            if (int(skill) > before)
                if (auto* c = clientOf(who))
                    system(c, "Your " + std::string(t.game == "knucklebones" ? "knucklebones" : t.game == "wolves" ? "wolves and deer" : "liar's bones") +
                                  " sharpened (" + std::to_string(int(skill)) + ").");
            record(Character, who);
        }
    for (const auto& who : t.seats)
    {
        world_.unseatResident(who);
        record(Economy, who);
    }
    society.closeAccount(account(t));
    t.seats.clear();
    t.begun = false;
    t.lastMove = world_.time();                     // (Its last lines stay a minute.)
}

void Game::leaveTable(Table& t, const std::string& who, const std::string& why)
{
    const auto seat = std::find(t.seats.begin(), t.seats.end(), who);
    if (seat == t.seats.end())
        return;
    world_.unseatResident(who);
    if (!t.begun)
    {
        t.seats.erase(seat);
        tableSay(t, who, why);
        return;
    }
    // A begun game: the stake stays in the pot. With one left, it wins; else the game ends, the pot shared.
    const int index = int(seat - t.seats.begin());
    t.seats.erase(seat);
    tableSay(t, who, why);
    if (t.seats.size() == 1)
        endTable(t, 0, "The game is over.");
    else
    {
        (void)index;
        endTable(t, -1, "The game breaks up.");
        tableSay(t, {}, "The game breaks up; the stakes go back to those who stayed.");
    }
}

bool Game::tableCommand(Connection* c, const json::Value& j, Result& result)
{
    // {"verb": "start", "game", "stake"} / "join" / "resident" / "begin" / "move" {action | from, to | count, face} /
    // "leave" / "invite" {target}.
    const auto me = c->entityId;
    const auto verb = j.string("verb");
    auto* e = world_.entity(me);
    if (!e)
        return result = {false, "No such character.", {}}, true;
    auto& society = world_.society();
    // The table one sits at.
    Table* mine = nullptr;
    for (auto& [key, t] : tables_)
        if (std::find(t.seats.begin(), t.seats.end(), me) != t.seats.end())
            mine = &t;
    if (verb == "leave")
    {
        if (!mine)
            return result = {false, "You aren't at a game.", {}}, true;
        leaveTable(*mine, me, " left the table.");
        return result = {true, mine->begun ? "You leave the game; your stake stays in the pot." : "You leave the table.", {}}, true;
    }
    if (verb == "invite")
    {
        if (!mine || mine->begun)
            return result = {false, "Start a game at a table first.", {}}, true;
        const auto target = j.string("target");
        const auto* o = world_.entity(target);
        auto* oc = clientOf(target);
        if (!o || o->npc || !oc || o->cellId != e->cellId || blocked(target, me))
            return result = {false, "Ask someone in the room.", {}}, true;
        system(oc, names::capitalised(labelFor(target, me)) + " asks you to a game of " + gameName(mine->game) +
                       (mine->stake > 0 ? " for " + std::to_string(mine->stake) + "p" : "") + " at the table. Go to it and JOIN.");
        return result = {true, "You ask " + labelFor(me, target) + " to the game.", {}}, true;
    }
    if (verb == "move")
    {
        if (!mine || !mine->begun)
            return result = {false, "You aren't in a game.", {}}, true;
        auto& t = *mine;
        const int seat = int(std::find(t.seats.begin(), t.seats.end(), me) - t.seats.begin());
        if (turnOf(t) != seat)
            return result = {false, "It isn't your turn.", {}}, true;
        const auto action = j.string("action");
        if (t.game == "knucklebones")
        {
            if (action == "bank")
            {
                if (t.knuckles.at[std::size_t(seat)] <= t.knuckles.banked[std::size_t(seat)])
                    return result = {false, "Catch something first.", {}}, true;
                tavern::knucklebonesBank(t.knuckles);
            }
            else
            {
                const auto skill = e->gameSkills.count("knucklebones") ? e->gameSkills.at("knucklebones") : 0.;
                tavern::knucklebonesTry(t.knuckles, tavern::catchChance(t.knuckles.at[std::size_t(seat)] + 1, e->dexterity, skill), t.rng);
            }
            tableSay(t, me, " " + t.knuckles.last + ".");
        }
        else if (t.game == "wolves")
        {
            if (action == "stop")
                tavern::endChain(t.board);
            else if (!tavern::applyMove(t.board, {int(j.number("from", -1)), int(j.number("to", -1)), false}))
                return result = {false, "That move can't be made.", {}}, true;
            tableSay(t, me, ": " + (action == "stop" ? std::string("the wolf stopped") : t.board.last) + ".");
        }
        else
        {
            if (action == "call")
            {
                const auto bidder = t.liars.bidder;
                const int loser = tavern::liarsCall(t.liars, t.rng);
                if (loser < 0)
                    return result = {false, "There is no bid to call.", {}}, true;
                (void)bidder;
                tableSay(t, me, " " + t.liars.last + ".");
                tableSay(t, t.seats[std::size_t(loser)], " lost a bone.");
            }
            else if (!tavern::liarsBid(t.liars, int(j.number("count", 0)), int(j.number("face", 0))))
                return result = {false, "Bid more of a face, or the same count of a higher face.", {}}, true;
            else
                tableSay(t, me, " " + t.liars.last + ".");
        }
        t.lastMove = world_.time();
        if (const int w = winnerOf(t); w >= 0)
            endTable(t, w, gameName(t.game) + " is over.");
        return result = {true, "", {}}, true;
    }
    // Starting, joining, asking the room, beginning: at a table.
    int tx = 0, ty = 0;
    const auto key = tableNear(*e, tx, ty);
    if (key.empty())
        return result = {false, "Sit at a table in an inn's or tavern's common room.", {}}, true;
    auto found = tables_.find(key);
    if (verb == "start")
    {
        if (mine)
            return result = {false, "You are at a game already.", {}}, true;
        if (found != tables_.end() && !found->second.seats.empty())
            return result = {false, "A game is going at this table; join it, or find another.", {}}, true;
        const auto game = j.string("game", "knucklebones");
        if (game != "knucklebones" && game != "wolves" && game != "liars")
            return result = {false, "Knucklebones, Wolves and Deer, or Liar's Bones.", {}}, true;
        const auto stake = game == "liars" ? std::int64_t(std::clamp(j.number("stake", 0), 0., double(StakeMost))) : 0;
        if (stake > 0 && society.spendable(me) < stake)
            return result = {false, "You haven't " + std::to_string(stake) + "p to stake.", {}}, true;
        Table t;
        t.id = "t" + std::to_string(std::hash<std::string>{}(key + std::to_string(world_.time())) % 1000000000);
        t.cell = e->cellId, t.x = tx, t.y = ty, t.game = game, t.stake = stake, t.made = world_.time(), t.lastMove = world_.time();
        t.rng.state = std::hash<std::string>{}(t.id) | 1;
        // The house: the inn's till, a venue's landlord, else the town.
        for (const auto& [id, life] : society.state().residents)
            if (const auto* job = society.jobOf(id); job && job->work.cell == t.cell)
                if (const auto* b = items::businessFor(job->title); b && b->id == "inn")
                {
                    t.house = society.tillOf(id);
                    break;
                }
        if (t.house.empty())
            if (const auto* venue = wholeLodgingAt(t.cell))
                t.house = venue->account;
        if (t.house.empty())
            t.house = society.treasuryOf(world_.lawTown(t.cell));
        t.seats.push_back(me);
        tables_[key] = t;
        tableSay(tables_[key], me, " set" + std::string(" out ") + gameName(game) + " at the table" +
                                       (stake > 0 ? ", for " + std::to_string(stake) + "p each" : "") + ".");
        return result = {true, "You set out " + gameName(game) + ". Ask someone to play, or ask the room.", {}}, true;
    }
    if (found == tables_.end() || found->second.seats.empty())
        return result = {false, "Set out a game first.", {}}, true;
    auto& t = found->second;
    if (verb == "join")
    {
        if (mine)
            return result = {false, "You are at a game already.", {}}, true;
        if (t.begun || int(t.seats.size()) >= seatsMost(t.game))
            return result = {false, "There's no seat at this game.", {}}, true;
        if (t.stake > 0 && society.spendable(me) < t.stake)
            return result = {false, "You haven't " + std::to_string(t.stake) + "p to stake.", {}}, true;
        t.seats.push_back(me);
        tableSay(t, me, " sat down to " + gameName(t.game) + ".");
        return result = {true, "", {}}, true;
    }
    if (std::find(t.seats.begin(), t.seats.end(), me) == t.seats.end())
        return result = {false, "Join the game first.", {}}, true;
    if (verb == "resident")
    {
        // Someone in the room: awake, not at work, old enough, not already playing; for stakes, one that can spare them.
        if (t.begun || int(t.seats.size()) >= seatsMost(t.game))
            return result = {false, "There's no seat at this game.", {}}, true;
        const auto today = std::to_string(std::int64_t(std::floor(world_.calendarDays())));
        const Entity* pick = nullptr;
        double nearest = 1e9;
        for (const auto* r : world_.entitiesIn(t.cell))
        {
            if (!r || !r->npc || r->dead || r->age < (t.stake > 0 ? 16 : 14) || world_.seatedResident(r->id) || world_.inBattle(r->id))
                continue;
            const auto life = society.state().residents.find(r->id);
            if (life == society.state().residents.end())
                continue;
            const auto& task = life->second.task;
            if (task == "sleep" || task == "trade" || task == "work" || task == "patrol" || task == "preaching" || task == "guard")
                continue;
            if (const auto* job = society.jobOf(r->id); job && task == job->title)
                continue;                           // (At its work.)
            if (t.stake > 0)
            {
                const auto* purse = society.account(r->id);
                if (!purse || purse->cash < ResidentPurseLeast || t.stake * 20 > purse->cash ||
                    stakedToday_[r->id + "|" + today] >= StakedGamesADay)
                    continue;
            }
            if (const double d = std::hypot(r->position.x - t.x - .5, r->position.y - t.y - .5); d < nearest)
                nearest = d, pick = r;
        }
        if (!pick)
            return result = {false, "Nobody in the room is free to play" + std::string(t.stake > 0 ? " for stakes" : "") + ".", {}}, true;
        // A seat at the table: an open point beside it.
        const auto* cell = world_.cell(t.cell);
        World::Seat seat{t.cell, t.x + .5, t.y + 1.5, "At a game of " + gameName(t.game) + "."};
        for (const auto [dx, dy] : {std::pair{0, 1}, std::pair{0, -1}, std::pair{1, 0}, std::pair{-1, 0}})
            if (const auto* tile = cell ? cell->tile(int(t.x) + dx, int(t.y) + dy) : nullptr; tile && !tile->solid && tile->glyph != 'T')
            {
                seat.x = t.x + dx + .5, seat.y = t.y + dy + .5;
                bool taken = false;
                for (const auto& s : t.seats)
                    if (const auto* o = world_.entity(s))
                        taken |= std::hypot(o->position.x - seat.x, o->position.y - seat.y) < .6;
                if (!taken)
                    break;
            }
        world_.seatResident(pick->id, seat);
        t.seats.push_back(pick->id);
        tableSay(t, pick->id, " took a seat at " + gameName(t.game) + ".");
        return result = {true, "", {}}, true;
    }
    if (verb == "begin")
    {
        if (t.begun)
            return result = {false, "The game is under way.", {}}, true;
        if (int(t.seats.size()) < 2 || (t.game == "wolves" && t.seats.size() != 2))
            return result = {false, "Two players at least (Wolves and Deer: two).", {}}, true;
        if (t.stake > 0)
        {
            society.openAccount(account(t));
            std::vector<std::string> paid;
            for (const auto& s : t.seats)
            {
                if (society.spendable(s) < t.stake || !society.shift(s, account(t), "", 0, t.stake, "a stake"))
                {
                    for (const auto& p : paid)
                        society.shift(account(t), p, "", 0, t.stake, "stakes returned");
                    return result = {false, names::capitalised(seatName(me, s)) + " can't put in the stake.", {}}, true;
                }
                paid.push_back(s);
            }
            const auto today = std::to_string(std::int64_t(std::floor(world_.calendarDays())));
            for (const auto& s : t.seats)
                if (const auto* o = world_.entity(s); o && o->npc)
                    ++stakedToday_[s + "|" + today];
        }
        t.begun = true;
        t.lastMove = world_.time();
        if (t.game == "knucklebones")
            t.knuckles = tavern::knucklebonesFor(int(t.seats.size()));
        else if (t.game == "wolves")
            t.board = tavern::wolvesAndDeer();
        else
            t.liars = tavern::liarsFor(int(t.seats.size()), t.rng);
        tableSay(t, {}, gameName(t.game) + " begins" + (t.stake > 0 ? ", " + std::to_string(t.stake * std::int64_t(t.seats.size())) + "p in the pot" : "") + ".");
        world_.recordEvent({"table game begun", me, {}, t.cell, 0, 0, t.game, int(t.seats.size()), t.stake, t.id});
        return result = {true, "", {}}, true;
    }
    return result = {false, "That isn't something a table does.", {}}, true;
}

void Game::residentTurn(Table& t)
{
    const int seat = turnOf(t);
    if (seat < 0 || seat >= int(t.seats.size()))
        return;
    const auto& who = t.seats[std::size_t(seat)];
    const auto* r = world_.entity(who);
    if (!r || !r->npc)
        return;
    const double skill = tavern::residentSkill(r->id, r->age);
    const auto said = [&](const std::string& what) { tableSay(t, who, " " + what + "."); };
    if (t.game == "knucklebones")
    {
        const double chance = tavern::catchChance(t.knuckles.at[std::size_t(seat)] + 1, r->dexterity, skill);
        if (tavern::knucklebonesBanks(t.knuckles, chance, skill, t.rng))
            tavern::knucklebonesBank(t.knuckles);
        else
            tavern::knucklebonesTry(t.knuckles, chance, t.rng);
        said(t.knuckles.last);
    }
    else if (t.game == "wolves")
    {
        const auto m = tavern::chooseMove(t.board, skill, t.rng);
        if (m.from < 0 || !tavern::applyMove(t.board, m))
            tavern::endChain(t.board);
        said(t.board.last);
    }
    else
    {
        int count = 0, face = 0;
        bool bluff = false;
        if (tavern::liarsResidentCalls(t.liars, seat, skill, t.rng, count, face, bluff) && t.liars.count > 0)
        {
            const int loser = tavern::liarsCall(t.liars, t.rng);
            said(t.liars.last + (loser >= 0 ? "; a bone lost" : ""));
        }
        else if (tavern::liarsBid(t.liars, count, face) || tavern::liarsBid(t.liars, t.liars.count + 1, std::max(1, t.liars.face)))
        {
            said(t.liars.last);
            // A tell, for a sharp eye: now and then, when it bids past what it believes.
            if (bluff)
                for (const auto& s : t.seats)
                    if (const auto* o = world_.entity(s); o && !o->npc)
                        if (const double eye = o->gameSkills.count("liars") ? o->gameSkills.at("liars") : 0.; t.rng.unit() < eye / 200)
                            if (auto* oc = clientOf(s))
                                system(oc, names::capitalised(labelFor(s, who)) + "'s ear twitches.");
        }
        else
        {
            tavern::liarsCall(t.liars, t.rng);
            said(t.liars.last);
        }
    }
    t.lastMove = world_.time();
}

void Game::tendTables(double dt)
{
    tablesAccumulator_ += dt;
    if (tablesAccumulator_ < 1)
        return;
    tablesAccumulator_ = 0;
    for (auto it = tables_.begin(); it != tables_.end();)
    {
        auto& t = it->second;
        // Seated wolves gone from the table a minute (or gone from the world), or not moving for ten: they forfeit.
        for (const auto& who : std::vector<std::string>(t.seats))
        {
            const auto* e = world_.entity(who);
            const bool here = e && !e->dead && e->cellId == t.cell && std::hypot(e->position.x - t.x - .5, e->position.y - t.y - .5) <= 3 &&
                              (e->npc || clientOf(who));
            if (here)
                t.awaySince.erase(who);
            else if (!t.awaySince.count(who))
                t.awaySince[who] = world_.time();
            else if (world_.time() - t.awaySince[who] > (e && e->npc ? 2 * AwaySeconds : AwaySeconds))
            {
                t.awaySince.erase(who);
                if (t.seats.empty())
                    break;
                leaveTable(t, who, " left the table.");
            }
        }
        if (!t.seats.empty() && t.begun)
        {
            const int seat = turnOf(t);
            const auto* e = seat >= 0 && seat < int(t.seats.size()) ? world_.entity(t.seats[std::size_t(seat)]) : nullptr;
            if (e && e->npc && world_.time() - t.lastMove >= ResidentMoveSeconds)
            {
                residentTurn(t);
                if (const int w = winnerOf(t); w >= 0)
                    endTable(t, w, gameName(t.game) + " is over.");
            }
            else if (e && !e->npc && world_.time() - t.lastMove > IdleSeconds)
                leaveTable(t, e->id, " stopped playing.");
        }
        if (!t.seats.empty() && !t.begun && world_.time() - t.made > UnbegunSeconds)
        {
            for (const auto& s : t.seats)
                world_.unseatResident(s);
            t.seats.clear();
        }
        // Done with: kept a minute for its last lines, then gone.
        if (t.seats.empty() && world_.time() - t.lastMove > 60)
            it = tables_.erase(it);
        else
            ++it;
    }
}

json::Value Game::tableSelf(const std::string& viewer)
{
    const auto* e = world_.entity(viewer);
    if (!e)
        return {};
    // The table one sits at, else the nearest game within 4 tiles to watch, else a table to set a game out at.
    const Table* shown = nullptr;
    double nearest = WatchReach + 1e-9;
    for (const auto& [key, t] : tables_)
    {
        if (std::find(t.seats.begin(), t.seats.end(), viewer) != t.seats.end())
        {
            shown = &t;
            break;
        }
        if (t.cell == e->cellId)
            if (const double d = std::hypot(e->position.x - t.x - .5, e->position.y - t.y - .5); d <= nearest && (!t.seats.empty() || !t.log.empty()))
                nearest = d, shown = &t;
    }
    if (!shown)
    {
        int tx = 0, ty = 0;
        if (tableNear(*e, tx, ty).empty())
            return {};
        auto o = Value::object();
        o.add("offer", true);
        return o;
    }
    const auto& t = *shown;
    const int mySeat = int(std::find(t.seats.begin(), t.seats.end(), viewer) - t.seats.begin());
    const bool seated = mySeat < int(t.seats.size());
    auto o = Value::object();
    o.add("id", t.id);
    o.add("game", t.game);
    o.add("name", gameName(t.game));
    o.add("stake", double(t.stake));
    o.add("begun", t.begun);
    o.add("seat", seated ? mySeat : -1);
    o.add("turn", turnOf(t));
    o.add("max", seatsMost(t.game));
    auto seats = Value::array();
    for (const auto& s : t.seats)
    {
        const auto* p = world_.entity(s);
        auto so = Value::object();
        so.add("id", s);
        so.add("name", names::capitalised(seatName(viewer, s)));
        so.add("resident", p && p->npc);
        seats.push(so);
    }
    o.add("seats", seats);
    auto log = Value::array();
    for (const auto& [who, what] : t.log)
        log.push(who.empty() ? what : names::capitalised(seatName(viewer, who)) + what);
    o.add("log", log);
    if (t.begun)
    {
        auto state = Value::object();
        if (t.game == "knucklebones")
        {
            auto banked = Value::array(), at = Value::array();
            for (std::size_t i = 0; i < t.knuckles.banked.size(); ++i)
                banked.push(t.knuckles.banked[i]), at.push(t.knuckles.at[i]);
            state.add("banked", banked);
            state.add("at", at);
            if (seated)
            {
                const double skill = e->gameSkills.count("knucklebones") ? e->gameSkills.at("knucklebones") : 0.;
                state.add("chance", std::round(tavern::catchChance(t.knuckles.at[std::size_t(mySeat)] + 1, e->dexterity, skill) * 100));
            }
        }
        else if (t.game == "wolves")
        {
            state.add("points", std::string(t.board.points.begin(), t.board.points.end()));
            state.add("taken", t.board.taken);
            state.add("plies", t.board.plies);
            state.add("chain", t.board.chain);
            auto moves = Value::array();
            if (seated && turnOf(t) == mySeat)
                for (const auto& m : tavern::legalMoves(t.board))
                {
                    auto mo = Value::array();
                    mo.push(m.from);
                    mo.push(m.to);
                    moves.push(mo);
                }
            state.add("moves", moves);
        }
        else
        {
            auto counts = Value::array();
            for (const auto& b : t.liars.bones)
                counts.push(int(b.size()));
            state.add("counts", counts);
            auto mine = Value::array();
            if (seated)
                for (const int f : t.liars.bones[std::size_t(mySeat)])
                    mine.push(f);
            state.add("mine", mine);
            state.add("count", t.liars.count);
            state.add("face", t.liars.face);
            state.add("bidder", t.liars.bidder);
            int total = 0;
            for (const auto& b : t.liars.bones)
                total += int(b.size());
            state.add("total", total);
        }
        o.add("state", state);
    }
    return o;
}
} // namespace ratw::game
