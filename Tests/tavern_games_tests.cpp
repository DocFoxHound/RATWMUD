// Tavern games' rules (Docs/Design/54-gathering-places.md, 5; Phase 5): Knucklebones' catches, banking and misses;
// Wolves and Deer's board, steps, jumps in chains, pens and the herd holding out, and a simulation of residents' play
// in which neither side wins more than 60%; Liar's Bones' raises, calls and the last with bones.
#include "RatwTavernGames.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace ratw;
using namespace ratw::tavern;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

void knucklebones()
{
    expect(std::abs(catchChance(1, 0, 0) - .9) < 1e-9 && std::abs(catchChance(5, 0, 0) - .5) < 1e-9, "90% at ones to 50% at fives");
    expect(std::abs(catchChance(3, 40, 40) - (.7 + .2)) < 1e-9, "plus DEX and skill, each ÷ 400");
    Rng rng{7};
    auto g = knucklebonesFor(2);
    expect(knucklebonesTry(g, 1, rng) && g.at[0] == 1 && g.turn == 0, "a sure catch: through the ones, still her turn");
    knucklebonesTry(g, 1, rng);
    knucklebonesBank(g);
    expect(g.banked[0] == 2 && g.turn == 1, "banked through the twos; the turn passes");
    knucklebonesTry(g, 1, rng);
    expect(!knucklebonesTry(g, 0, rng) && g.at[1] == 0 && g.banked[1] == 0 && g.turn == 0, "a miss loses what wasn't banked");
    for (int i = 0; i < 3; ++i)
        knucklebonesTry(g, 1, rng);
    expect(g.winner == 0 && g.banked[0] == 5, "through the fives: she wins");
}

void wolvesAndDeerRules()
{
    auto g = wolvesAndDeer();
    int points = 0, deer = 0, wolves = 0;
    for (int y = 0; y < BoardSide; ++y)
        for (int x = 0; x < BoardSide; ++x)
        {
            points += onBoard(x, y);
            deer += g.points[std::size_t(point(x, y))] == 'D';
            wolves += g.points[std::size_t(point(x, y))] == 'W';
        }
    expect(points == 33 && deer == 13 && wolves == 2 && g.turn == 1, "33 points, 13 deer, 2 wolves; the deer first");
    for (const auto& m : legalMoves(g))
        expect(m.to / BoardSide <= m.from / BoardSide, "deer never step back");
    // A chain: a wolf with two deer to jump in a line.
    WolvesAndDeer c;
    for (int y = 0; y < BoardSide; ++y)
        for (int x = 0; x < BoardSide; ++x)
            c.points[std::size_t(point(x, y))] = onBoard(x, y) ? '.' : ' ';
    c.points[std::size_t(point(3, 0))] = 'W';
    c.points[std::size_t(point(3, 1))] = 'D';
    c.points[std::size_t(point(3, 3))] = 'D';
    c.points[std::size_t(point(0, 4))] = 'D';
    c.turn = 0;
    expect(applyMove(c, {point(3, 0), point(3, 2), true}) && c.taken == 1 && c.chain == point(3, 2) && c.turn == 0, "a jump, and another to come");
    expect(!applyMove(c, {point(3, 2), point(2, 2), false}), "mid-chain only jumps");
    expect(applyMove(c, {point(3, 2), point(3, 4), true}) && c.taken == 2 && c.chain == -1 && c.turn == 1, "the chain ends: the deer's turn");
    // Penned: the herd wins.
    WolvesAndDeer p;
    for (int y = 0; y < BoardSide; ++y)
        for (int x = 0; x < BoardSide; ++x)
            p.points[std::size_t(point(x, y))] = onBoard(x, y) ? '.' : ' ';
    // A wolf in the corner of the top arm, deer beside it with deer beyond: the last steps across to close it in.
    p.points[std::size_t(point(2, 0))] = 'W';
    for (const auto [x, y] : {std::pair{3, 0}, std::pair{4, 0}, std::pair{2, 2}, std::pair{3, 1}})
        p.points[std::size_t(point(x, y))] = 'D';
    p.turn = 1;
    expect(applyMove(p, {point(3, 1), point(2, 1), false}) && p.winner == 1, "every way out closed: the herd wins");
}

void balance()
{
    // Residents of the same middling skill, 400 games: neither side wins more than 60%.
    int pack = 0, games = 400;
    for (int i = 0; i < games; ++i)
    {
        Rng rng{std::uint64_t(1000 + i)};
        auto g = wolvesAndDeer();
        while (g.winner < 0)
        {
            const auto m = chooseMove(g, 50, rng);
            if (m.from < 0 || !applyMove(g, m))
                break;
        }
        pack += g.winner == 0;
    }
    const double share = double(pack) / games;
    std::cout << "wolves and deer: the pack wins " << std::lround(share * 100) << "%\n";
    expect(share >= .4 && share <= .6, "neither side wins more than 60%: the pack " + std::to_string(share));
}

void liars()
{
    Rng rng{3};
    auto g = liarsFor(2, rng);
    g.bones = {{2, 2, 5, 1, 6}, {2, 3, 3, 4, 4}};
    expect(liarsBid(g, 2, 3) && g.turn == 1, "two showing three");
    expect(!liarsBid(g, 2, 2) && !liarsBid(g, 1, 6), "a bid must raise");
    expect(liarsBid(g, 2, 4) && g.turn == 0, "the same count, a higher face");
    expect(liarsBid(g, 4, 2) && g.turn == 1, "a higher count, any face");
    // Three twos showing, four bid: the bid was a lie; the bidder (seat 0) loses a bone and starts.
    expect(liarsCall(g, rng) == 0 && g.bones[0].size() == 4 && g.turn == 0 && g.count == 0, "called: the liar loses a bone");
    int count = 0, face = 0;
    bool bluff = false;
    expect(!liarsResidentCalls(g, 0, 50, rng, count, face, bluff) && liarsBid(g, count, face), "a resident opens with a bid");
    // To the end: the last with bones wins.
    while (g.winner < 0)
    {
        if (g.count > 0)
            liarsCall(g, rng);
        else
            liarsBid(g, 1, 1);
    }
    expect(g.winner >= 0 && !g.bones[std::size_t(g.winner)].empty(), "the last with bones wins");
}
} // namespace

int main()
{
    try
    {
        knucklebones();
        wolvesAndDeerRules();
        liars();
        balance();
    }
    catch (const std::exception& error)
    {
        std::cerr << "tavern_games_tests failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "tavern_games_tests passed (" << checks << " checks)\n";
    return 0;
}
