#pragma once
// Tavern games (Docs/Design/54-gathering-places.md, 5; Phase 5): the three games' rules and residents' play, pure (no
// world, no game). Game::tables_ (RatwGameTables.cpp) seats the players, holds the stakes and calls these.
//
// - Knucklebones (2 to 4): rounds "ones" to "fives". On a turn a player tries its next round, each try's chance from
//   90% at ones to 50% at fives, plus DEX ÷ 400 and skill ÷ 400; after a catch it goes on or banks; a miss passes the
//   turn and loses what it hadn't banked. The first through fives wins.
// - Wolves and Deer (2): the cross-shaped board of 33 points. Two wolves (at the top) against 13 deer (the bottom three
//   rows). Deer step forward (up) or sideways; wolves step any way along the lines (across and down, no diagonals) or
//   take a deer by jumping it into the point beyond, in chains. The deer move first. The pack wins at 7 deer taken
//   (WolvesWinAt) or when the deer can't move; the herd by penning both wolves, or by holding out 200 moves.
// - Liar's Bones (2 to 4): five hidden bones each, rolled each round; bid how many across the table show a face
//   (raising the count, or the face at the same count), or call "liar" on the last bid: if fewer show, the bidder loses
//   a bone, else the caller; the loser starts the next round. The last with bones wins.
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ratw::tavern
{
// A small, seeded random source (splitmix64): the same seed plays the same game.
struct Rng
{
    std::uint64_t state = 1;
    std::uint64_t next();
    double unit();                                  // [0, 1)
    int below(int n);                               // [0, n)
};

// A resident's skill at the games (0..100): from its age and its id, so some are sharp.
double residentSkill(const std::string& id, int age);

// ------------------------------------------------------------------ Knucklebones
struct Knucklebones
{
    std::vector<int> banked, at;                    // Rounds through, banked and this turn, per seat.
    int turn = 0, winner = -1;
    std::string last;                               // What just happened, in words ("caught the threes").
};
Knucklebones knucklebonesFor(int seats);
double catchChance(int round, double dex, double skill);   // Round 1..5.
// A try at the next round (true if caught); the turn passes on a miss, the game ends at fives.
bool knucklebonesTry(Knucklebones& g, double chance, Rng& rng);
void knucklebonesBank(Knucklebones& g);
// A resident's choice after a catch: bank what it has, or go on.
bool knucklebonesBanks(const Knucklebones& g, double nextChance, double skill, Rng& rng);
extern const char* const RoundNames[5];

// ------------------------------------------------------------------ Wolves and Deer
constexpr int BoardSide = 7, WolvesWinAt = 7, HerdHoldsOut = 200;
bool onBoard(int x, int y);                         // The cross: 33 points of a 7 x 7 square.
struct WolvesAndDeer
{
    std::array<char, BoardSide * BoardSide> points{};   // 'W', 'D', '.' (empty), ' ' (off the board).
    int taken = 0, plies = 0;
    int turn = 1;                                   // 0: the wolves' seat; 1: the deer's (the deer move first).
    int chain = -1;                                 // A wolf that has jumped and may jump again (its point), else -1.
    int winner = -1;                                // 0 the pack, 1 the herd.
    std::string last;
};
struct Move
{
    int from = -1, to = -1;
    bool jump = false;
};
WolvesAndDeer wolvesAndDeer();
std::vector<Move> legalMoves(const WolvesAndDeer& g);
bool applyMove(WolvesAndDeer& g, const Move& m);   // False if not legal.
void endChain(WolvesAndDeer& g);                    // A wolf mid-chain stops jumping: the deer's turn.
Move chooseMove(const WolvesAndDeer& g, double skill, Rng& rng);
inline int point(int x, int y) { return y * BoardSide + x; }

// ------------------------------------------------------------------ Liar's Bones
constexpr int LiarsBones = 5;
struct LiarsGame
{
    std::vector<std::vector<int>> bones;            // Each seat's faces (1..6); empty when out.
    int turn = 0, winner = -1;
    int count = 0, face = 0, bidder = -1;           // The standing bid (count 0: none yet this round).
    int lastLoser = -1;
    std::vector<std::vector<int>> shown;            // The last call's bones, shown to all; empty between calls.
    std::string last;
};
LiarsGame liarsFor(int seats, Rng& rng);
bool liarsBid(LiarsGame& g, int count, int face);   // False if it doesn't raise, or is out of range.
int liarsCall(LiarsGame& g, Rng& rng);             // The seat that lost a bone (-1 if there was no bid to call).
int liarsShowing(const LiarsGame& g, int face);
// A resident's turn: call (true), or the bid it makes (count, face). `bluff` is set when the bid is beyond what it
// believes (for a tell).
bool liarsResidentCalls(const LiarsGame& g, int seat, double skill, Rng& rng, int& count, int& face, bool& bluff);
} // namespace ratw::tavern
