// Tavern games' rules and residents' play (Docs/Design/54-gathering-places.md, 5): see RatwTavernGames.h.
#include "RatwTavernGames.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace ratw::tavern
{
std::uint64_t Rng::next()
{
    std::uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

double Rng::unit() { return double(next() >> 11) / double(1ULL << 53); }

int Rng::below(int n) { return n <= 1 ? 0 : int(next() % std::uint64_t(n)); }

double residentSkill(const std::string& id, int age)
{
    const double h = double(std::hash<std::string>{}(id + "|games") % 1000) / 1000;
    return std::clamp(15 + 55 * h + std::min(age, 50) * .5, 0., 100.);
}

// ------------------------------------------------------------------ Knucklebones

const char* const RoundNames[5] = {"ones", "twos", "threes", "fours", "fives"};

Knucklebones knucklebonesFor(int seats)
{
    Knucklebones g;
    g.banked.assign(std::size_t(std::max(1, seats)), 0);
    g.at = g.banked;
    return g;
}

double catchChance(int round, double dex, double skill)
{
    return std::clamp(.9 - .1 * (std::clamp(round, 1, 5) - 1) + dex / 400 + skill / 400, .05, .98);
}

namespace
{
void nextTurn(Knucklebones& g)
{
    g.turn = (g.turn + 1) % int(g.at.size());
    g.at[std::size_t(g.turn)] = g.banked[std::size_t(g.turn)];
}
} // namespace

bool knucklebonesTry(Knucklebones& g, double chance, Rng& rng)
{
    if (g.winner >= 0)
        return false;
    auto& at = g.at[std::size_t(g.turn)];
    const int round = at + 1;
    if (rng.unit() < chance)
    {
        at = round;
        g.last = std::string("caught the ") + RoundNames[round - 1];
        if (at >= 5)
        {
            g.banked[std::size_t(g.turn)] = 5;
            g.winner = g.turn;
        }
        return true;
    }
    g.last = std::string("missed the ") + RoundNames[round - 1];
    at = g.banked[std::size_t(g.turn)];
    nextTurn(g);
    return false;
}

void knucklebonesBank(Knucklebones& g)
{
    if (g.winner >= 0)
        return;
    g.banked[std::size_t(g.turn)] = g.at[std::size_t(g.turn)];
    g.last = std::string("banked through the ") + RoundNames[std::max(0, g.banked[std::size_t(g.turn)] - 1)];
    nextTurn(g);
}

bool knucklebonesBanks(const Knucklebones& g, double nextChance, double skill, Rng& rng)
{
    const int gained = g.at[std::size_t(g.turn)] - g.banked[std::size_t(g.turn)];
    if (gained <= 0)
        return false;
    // A sharper player weighs the next catch against what it would lose; any player now and then goes on regardless.
    const bool wise = gained >= 2 || nextChance < .55 + skill / 1000;
    return rng.unit() < .1 ? !wise : wise;
}

// ------------------------------------------------------------------ Wolves and Deer

bool onBoard(int x, int y)
{
    return x >= 0 && y >= 0 && x < BoardSide && y < BoardSide && ((x >= 2 && x <= 4) || (y >= 2 && y <= 4));
}

WolvesAndDeer wolvesAndDeer()
{
    WolvesAndDeer g;
    for (int y = 0; y < BoardSide; ++y)
        for (int x = 0; x < BoardSide; ++x)
            g.points[std::size_t(point(x, y))] = !onBoard(x, y) ? ' ' : y >= 4 ? 'D' : '.';
    g.points[std::size_t(point(2, 0))] = 'W';
    g.points[std::size_t(point(4, 0))] = 'W';
    return g;
}

namespace
{
constexpr int Dirs[4][2] = {{0, -1}, {1, 0}, {-1, 0}, {0, 1}};
char at(const WolvesAndDeer& g, int x, int y) { return onBoard(x, y) ? g.points[std::size_t(point(x, y))] : ' '; }

void jumpsFrom(const WolvesAndDeer& g, int from, std::vector<Move>& out)
{
    const int x = from % BoardSide, y = from / BoardSide;
    for (const auto& d : Dirs)
        if (at(g, x + d[0], y + d[1]) == 'D' && at(g, x + 2 * d[0], y + 2 * d[1]) == '.')
            out.push_back({from, point(x + 2 * d[0], y + 2 * d[1]), true});
}

void settle(WolvesAndDeer& g)
{
    if (g.winner >= 0)
        return;
    if (g.taken >= WolvesWinAt)
        g.winner = 0;
    else if (g.plies >= HerdHoldsOut)
        g.winner = 1;
    else if (legalMoves(g).empty())
        g.winner = g.turn == 0 ? 1 : 0;             // The pack penned; the herd stuck.
}
} // namespace

std::vector<Move> legalMoves(const WolvesAndDeer& g)
{
    std::vector<Move> out;
    if (g.winner >= 0)
        return out;
    if (g.turn == 0 && g.chain >= 0)
    {
        jumpsFrom(g, g.chain, out);
        return out;
    }
    for (int y = 0; y < BoardSide; ++y)
        for (int x = 0; x < BoardSide; ++x)
        {
            const char c = at(g, x, y);
            if (g.turn == 0 && c == 'W')
            {
                for (const auto& d : Dirs)
                    if (at(g, x + d[0], y + d[1]) == '.')
                        out.push_back({point(x, y), point(x + d[0], y + d[1]), false});
                jumpsFrom(g, point(x, y), out);
            }
            else if (g.turn == 1 && c == 'D')
                for (int k = 0; k < 3; ++k)         // Forward (up) and sideways; never back.
                    if (at(g, x + Dirs[k][0], y + Dirs[k][1]) == '.')
                        out.push_back({point(x, y), point(x + Dirs[k][0], y + Dirs[k][1]), false});
        }
    return out;
}

bool applyMove(WolvesAndDeer& g, const Move& m)
{
    const auto moves = legalMoves(g);
    const auto found = std::find_if(moves.begin(), moves.end(), [&](const Move& o) { return o.from == m.from && o.to == m.to; });
    if (found == moves.end())
        return false;
    const auto move = *found;
    g.points[std::size_t(move.to)] = g.points[std::size_t(move.from)];
    g.points[std::size_t(move.from)] = '.';
    ++g.plies;
    if (g.turn == 1)
    {
        g.turn = 0;
        g.last = "a deer stepped";
    }
    else if (move.jump)
    {
        g.points[std::size_t((move.from + move.to) / 2)] = '.';
        ++g.taken;
        g.last = "a wolf took a deer";
        std::vector<Move> more;
        jumpsFrom(g, move.to, more);
        if (!more.empty() && g.taken < WolvesWinAt)
            g.chain = move.to;
        else
            g.chain = -1, g.turn = 1;
    }
    else
    {
        g.turn = 1;
        g.last = "a wolf moved";
    }
    settle(g);
    return true;
}

void endChain(WolvesAndDeer& g)
{
    if (g.chain < 0 || g.winner >= 0)
        return;
    g.chain = -1;
    g.turn = 1;
    settle(g);
}

namespace
{
constexpr double Roughness = .12;   // (Tuned by simulation: the pack wins 45-53% at any skill, both alike.)
int jumpsAvailable(const WolvesAndDeer& g)
{
    // How many jumps the wolves would have, were it their turn.
    auto w = g;
    w.turn = 0;
    w.chain = -1;
    int n = 0;
    for (const auto& m : legalMoves(w))
        n += m.jump;
    return n;
}
int wolfMobility(const WolvesAndDeer& g)
{
    auto w = g;
    w.turn = 0;
    w.chain = -1;
    return int(legalMoves(w).size());
}
} // namespace

Move chooseMove(const WolvesAndDeer& g, double skill, Rng& rng)
{
    const auto moves = legalMoves(g);
    if (moves.empty())
        return {};
    // Each move weighed by its reckoning, the weighing the rougher the duller the player.
    const double rough = 2 + (100 - std::clamp(skill, 0., 100.)) * Roughness;
    double best = -1e9;
    Move pick = moves.front();
    for (const auto& m : moves)
    {
        auto after = g;
        applyMove(after, m);
        double score = rng.unit() * rough;
        if (after.winner == g.turn)
            score += 1000;
        if (g.turn == 0)
        {
            score += m.jump ? 20 : 0;
            score += after.chain >= 0 ? 8 : 0;
            score += 3 * jumpsAvailable(after);     // Threats it leaves the herd to answer.
            score += .3 * wolfMobility(after);
        }
        else
        {
            score -= 12 * jumpsAvailable(after);    // Never leave a deer to be taken.
            score -= .8 * wolfMobility(after);      // Close the pack in.
            score += m.to < m.from - 1 ? .6 : 0;    // Forward.
        }
        if (score > best)
            best = score, pick = m;
    }
    return pick;
}

// ------------------------------------------------------------------ Liar's Bones

namespace
{
void rollAll(LiarsGame& g, Rng& rng)
{
    for (auto& b : g.bones)
        for (auto& f : b)
            f = 1 + rng.below(6);
}
int nextIn(const LiarsGame& g, int from)
{
    const int n = int(g.bones.size());
    for (int k = 1; k <= n; ++k)
        if (!g.bones[std::size_t((from + k) % n)].empty())
            return (from + k) % n;
    return from;
}
int totalBones(const LiarsGame& g)
{
    int n = 0;
    for (const auto& b : g.bones)
        n += int(b.size());
    return n;
}
} // namespace

LiarsGame liarsFor(int seats, Rng& rng)
{
    LiarsGame g;
    g.bones.assign(std::size_t(std::max(2, seats)), std::vector<int>(LiarsBones, 1));
    rollAll(g, rng);
    return g;
}

int liarsShowing(const LiarsGame& g, int face)
{
    int n = 0;
    for (const auto& b : g.bones)
        n += int(std::count(b.begin(), b.end(), face));
    return n;
}

bool liarsBid(LiarsGame& g, int count, int face)
{
    if (g.winner >= 0 || face < 1 || face > 6 || count < 1 || count > totalBones(g))
        return false;
    if (!(count > g.count || (count == g.count && face > g.face)))
        return false;
    g.count = count, g.face = face, g.bidder = g.turn;
    g.shown.clear();
    g.last = "bid " + std::to_string(count) + " showing " + std::to_string(face);
    g.turn = nextIn(g, g.turn);
    return true;
}

int liarsCall(LiarsGame& g, Rng& rng)
{
    if (g.winner >= 0 || g.count == 0 || g.bidder < 0)
        return -1;
    const int showing = liarsShowing(g, g.face);
    const int loser = showing >= g.count ? g.turn : g.bidder;
    g.shown = g.bones;
    g.last = "called liar: " + std::to_string(showing) + " showing " + std::to_string(g.face) + (showing >= g.count ? ", the bid stands" : ", the bid was a lie");
    g.bones[std::size_t(loser)].pop_back();
    g.lastLoser = loser;
    int left = 0, last = -1;
    for (std::size_t i = 0; i < g.bones.size(); ++i)
        if (!g.bones[i].empty())
            ++left, last = int(i);
    if (left <= 1)
    {
        g.winner = last;
        return loser;
    }
    rollAll(g, rng);
    g.count = 0, g.face = 0, g.bidder = -1;
    g.turn = g.bones[std::size_t(loser)].empty() ? nextIn(g, loser) : loser;
    return loser;
}

bool liarsResidentCalls(const LiarsGame& g, int seat, double skill, Rng& rng, int& count, int& face, bool& bluff)
{
    const auto& mine = g.bones[std::size_t(seat)];
    const int others = totalBones(g) - int(mine.size());
    const auto expected = [&](int f) { return double(std::count(mine.begin(), mine.end(), f)) + others / 6.; };
    // Call when the standing bid is well past what it believes: a sharp player's sense is truer.
    if (g.count > 0)
    {
        const double margin = .4 + (1 - skill / 100) * (rng.unit() * 2 - .6);
        if (g.count > expected(g.face) + margin)
            return true;
    }
    // Else raise on its best face.
    int best = 1;
    for (int f = 2; f <= 6; ++f)
        if (expected(f) > expected(best) || (expected(f) == expected(best) && rng.unit() < .5))
            best = f;
    face = best;
    if (g.count == 0)
        count = std::max(1, int(std::floor(expected(best) - rng.unit())));
    else
        count = best > g.face ? g.count : g.count + 1;
    if (count > totalBones(g))
        return true;                                // (Nothing left to bid: call.)
    bluff = count > expected(face) + .5;
    return false;
}
} // namespace ratw::tavern
