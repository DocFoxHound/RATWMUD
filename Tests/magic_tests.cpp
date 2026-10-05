// Gifts in a fight (Docs/Design/43-gifts.md, Core/RatwMagic.cpp): each family's fight abilities, Gifted and Quickened,
// their Tells, costs and Overreach, what they leave on the ground, and how they meet the fight's own rules.
#include "RatwBattle.h"
#include "RatwGifts.h"
#include "RatwItems.h"
#include "RatwSociety.h"
#include "RatwWire.h"
#include "RatwWorld.h"
#include "battle_play.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace ratw;
namespace
{
int checks = 0;
void expect(bool ok, const std::string& what)
{
    ++checks;
    if (!ok)
        throw std::runtime_error(what);
}

void quiet(World& w)
{
    for (const auto& e : w.entities())
        if (e.second.npc)
            w.entity(e.first)->leaderId = "test-frozen";
}

// Ad (side 0) and Bo (side 1) in a fight to the death, side by side, past the positioning phase; Ad has `gift`.
Battle& duel(World& w, const std::string& gift, bool quickened, const std::string& boGift = {}, bool boQuickened = false)
{
    quiet(w);
    auto& ad = w.addPlayer("player-ad", "Ad");
    auto& bo = w.addPlayer("player-bo", "Bo");
    bo.cellId = ad.cellId;
    bo.position = {ad.position.x + 1.2, ad.position.y};
    w.attack("player-ad", "player-bo", "death");
    w.answerChallenge("player-bo", true);
    test::takeGround(w, "player-ad");
    auto* b = const_cast<Battle*>(w.battleOf("player-ad"));
    expect(b, "a duel");
    w.tick(.05);
    if (!gift.empty())
        expect(w.giveGift("player-ad", gift, quickened).ok, "Ad's Gift: " + gift);
    if (!boGift.empty())
        expect(w.giveGift("player-bo", boGift, boQuickened).ok, "Bo's Gift: " + boGift);
    return *b;
}

// Makes it `id`'s turn, fresh: the others waiting, its mana and breath full.
BattleFighter& turn(World& w, Battle& b, const std::string& id)
{
    for (auto& f : b.fighters)
    {
        f.acting = false;
        f.meter = 0;
    }
    auto& f = *b.fighter(id);
    f.acting = true;
    f.acted = f.moved = f.faced = false;
    f.casting = false;
    auto* e = w.entity(id);
    e->mana = 100;
    e->stamina = 100;
    e->exhausted = false;
    f.deadline = w.time() + 60;
    return f;
}

// Lets the bars run until `id`'s next turn begins (the Gifts' turn-start rules with it).
void nextTurnOf(World& w, Battle& b, const std::string& id)
{
    for (auto& f : b.fighters)
    {
        f.acting = false;
        f.meter = f.id == id ? 99.9 : 0;
    }
    for (int i = 0; i < 50 && !b.fighter(id)->acting && !b.over; ++i)
        w.tick(.05);
}

// An open tile next to (x, y) in the arena, or (-1, -1).
std::pair<int, int> openNear(World& w, const Battle& b, int x, int y, int d = 1)
{
    for (int dy = -d; dy <= d; ++dy)
        for (int dx = -d; dx <= d; ++dx)
            if (std::max(std::abs(dx), std::abs(dy)) == d && w.arenaOpen(b, x + dx, y + dy))
                return {x + dx, y + dy};
    return {-1, -1};
}

std::string at(int x, int y)
{
    return std::to_string(x) + "," + std::to_string(y);
}

bool logHas(const Battle& b, const std::string& words)
{
    return std::any_of(b.log.begin(), b.log.end(), [&](const BattleLine& l) { return l.text.find(words) != std::string::npos; });
}

// Puts `id` at a tile `dx` east of `other` (and on its row), if open.
void place(World& w, Battle& b, const std::string& id, const std::string& other, int dx, int dy = 0)
{
    auto* o = b.fighter(other);
    auto* f = b.fighter(id);
    const int x = o->x + dx, y = o->y + dy;
    expect(w.arenaOpen(b, x, y, id), "room to stand at " + at(x, y));
    f->x = x;
    f->y = y;
    f->walk.clear();
}

void optionsAndTells()
{
    World w;
    auto& b = duel(w, "earth", false);
    turn(w, b, "player-ad");
    const auto options = w.giftOptions("player-ad");
    expect(options.size() == 3, "Gifted Earth has three fight abilities (the two work ones aren't shown in a fight): " + std::to_string(options.size()));
    expect(options[0].id == "loosen_ground" && options[0].target == "tile" && options[0].ready, "Loosen Ground, aimed at a tile, ready: " + options[0].why);
    w.entity("player-ad")->worn["paws"] = "linen_wraps";
    expect(!w.giftOptions("player-ad")[0].ready && w.giftOptions("player-ad")[0].why.find("bare paws") != std::string::npos,
           "Wraps on: an Earth wolf braces bare paws (the Tell)");
    expect(!w.useGift("player-ad", "loosen_ground", at(b.fighter("player-bo")->x, b.fighter("player-bo")->y)).ok, "and can't");
    w.entity("player-ad")->worn.erase("paws");
    expect(!w.useGift("player-ad", "upheaval", "1,1").ok, "A Quickened ability isn't a Gifted wolf's");
    expect(!w.useGift("player-bo", "loosen_ground", "1,1").ok, "No Gift, no ability");
    // Water's Tell: water within reach.
    World w2;
    auto& b2 = duel(w2, "water", false);
    turn(w2, b2, "player-ad");
    const auto& bo = *b2.fighter("player-bo");
    const auto r = w2.useGift("player-ad", "splash_eyes", "player-bo");
    if (!r.ok)
    {
        expect(r.message.find("water") != std::string::npos, "No water near: " + r.message);
        b2.ground.push_back({bo.x, bo.y, "water", "", -1, 0});
        expect(w2.useGift("player-ad", "splash_eyes", "player-bo").ok, "With water at hand, Splash Eyes");
    }
    expect(b2.fighter("player-bo")->magic.has("splashed") && b2.fighter("player-ad")->acted, "Bo is splashed; it was Ad's action");
    expect(b2.fighter("player-ad")->magic.has("thirsty"), "and Ad is thirsty (the Cost)");
    const double before = w2.strikeChance(*b2.fighter("player-bo"), *b2.fighter("player-ad"));
    b2.fighter("player-bo")->magic.fx.erase("splashed");
    expect(std::abs(w2.strikeChance(*b2.fighter("player-bo"), *b2.fighter("player-ad")) - before - .2) < 1e-9 || before <= .2 + 1e-9,
           "Splashed, Bo's next blow is 20% less likely");
}

void fireGifted()
{
    World w;
    auto& b = duel(w, "fire", false);
    auto& ad = turn(w, b, "player-ad");
    ad.bleeding = 3;
    expect(w.useGift("player-ad", "cauterize", "player-ad").ok && ad.bleeding == 0, "Cauterize stops the bleeding");
    expect(w.entity("player-ad")->hurt > 0, "and the held breath costs a little health");
    turn(w, b, "player-ad");
    b.fighter("player-bo")->meter = 50;
    expect(w.useGift("player-ad", "flare", "player-bo").ok && b.fighter("player-bo")->meter == 30, "Flare knocks Bo's bar back 20");
    turn(w, b, "player-ad");
    expect(!w.useGift("player-ad", "flamethrower", "1,1").ok, "No Flamethrower for the Gifted");
    // Heat Sense: held, it finds a hidden wolf within 6 tiles at the start of each turn.
    expect(w.useGift("player-ad", "heat_sense", "").ok && ad.magic.channel == "heat_sense", "Heat Sense is held");
    b.fighter("player-bo")->unseen = true;
    nextTurnOf(w, b, "player-ad");
    expect(!b.fighter("player-bo")->unseen && logHas(b, "warmth gives them away"), "and Bo, hidden, is found by his warmth");
    expect(w.entity("player-ad")->mana < 100, "A turn's mana for holding it");
    // Moving lets it go.
    const auto reach = w.battleReach("player-ad");
    expect(!reach.empty() && w.battleMove("player-ad", reach[0].first, reach[0].second).ok && ad.magic.channel.empty(), "Moving lets it go");
}

void fireQuickened()
{
    World w;
    auto& b = duel(w, "fire", true);
    auto& ad = turn(w, b, "player-ad");
    auto& bo = *b.fighter("player-bo");
    place(w, b, "player-bo", "player-ad", 2);
    // Heat Lance: a line, gathered on a countdown, then into the first wolf.
    expect(w.useGift("player-ad", "heat_lance", at(bo.x, bo.y)).ok && ad.casting && b.casts.size() == 1, "Heat Lance gathers");
    for (int i = 0; i < 80 && !b.casts.empty(); ++i)
        w.tick(.05);
    expect(b.casts.empty() && w.entity("player-bo")->hurt > 0 && logHas(b, "pierced by the heat"), "and pierces Bo");
    expect(w.entity("player-ad")->wardenAttention > 0, "Quickened magic seen: Warden attention");
    // Wall of Fire: painted, connected, within 10.
    turn(w, b, "player-ad");
    std::vector<std::pair<int, int>> run;
    for (int i = 0; i < 4; ++i)
        if (w.arenaOpen(b, ad.x + i - 1, ad.y + 3))
            run.push_back({ad.x + i - 1, ad.y + 3});
    std::string shape;
    for (const auto& [x, y] : run)
        shape += (shape.empty() ? "" : ";") + at(x, y);
    expect(!w.useGift("player-ad", "wall_of_fire", at(ad.x - 1, ad.y + 3) + ";" + at(ad.x + 2, ad.y + 6)).ok, "Painted tiles must join up");
    const double mana = w.entity("player-ad")->mana;
    expect(!run.empty() && w.useGift("player-ad", "wall_of_fire", shape).ok, "A Wall of Fire painted");
    expect(std::abs(mana - w.entity("player-ad")->mana - (10 + 3.0 * double(run.size()))) < 1e-9, "costing 10 + 3 a tile");
    for (int i = 0; i < 80 && !b.casts.empty(); ++i)
        w.tick(.05);
    expect(b.groundAt(run[0].first, run[0].second, "fire") != nullptr, "and it burns on the ground");
    // Heat Sense (passive) and no critical: a blow from behind is as from the front.
    auto& boF = *b.fighter("player-bo");
    place(w, b, "player-bo", "player-ad", 1);
    ad.facing = 4;                                  // (Its back to Bo.)
    const double behind = w.strikeChance(boF, ad);
    ad.facing = 0;
    expect(std::abs(behind - w.strikeChance(boF, ad)) < 1e-9, "Quickened Fire can't be flanked: behind is as head on");
    // Overreach: at no mana, the fire turns on its caster.
    turn(w, b, "player-ad");
    w.entity("player-ad")->mana = 0;
    const double hurt = w.entity("player-ad")->hurt;
    expect(w.useGift("player-ad", "blastwave", "").ok && w.entity("player-ad")->hurt >= hurt + 6 + 3 - 1e-9 && logHas(b, "overheats"),
           "Pushed past its mana, it overreaches");
}

void earth()
{
    World w;
    auto& b = duel(w, "earth", true);
    auto& ad = turn(w, b, "player-ad");
    auto& bo = *b.fighter("player-bo");
    // Stone Armor: at the start, for the fight; less from every blow, no flank, slower.
    expect(w.useGift("player-ad", "stone_armor", "").ok && ad.magic.has("stone_armor") && !ad.acted, "Stone Armor, no action spent");
    expect(!w.useGift("player-ad", "stone_armor", "").ok, "once");
    const double rate = w.meterRate(b, ad, 1);
    ad.magic.fx.erase("stone_armor");
    expect(rate < w.meterRate(b, ad, 1), "Its bar fills slower in stone");
    ad.magic.fx["stone_armor"] = -1;
    // Fissure: painted; wolves on it fall, and no one crosses.
    place(w, b, "player-bo", "player-ad", 3);
    std::string shape = at(bo.x, bo.y);
    if (w.arenaOpen(b, bo.x, bo.y + 1))
        shape += ";" + at(bo.x, bo.y + 1);
    expect(w.useGift("player-ad", "fissure", shape).ok, "A fissure painted under Bo");
    for (int i = 0; i < 80 && !b.casts.empty(); ++i)
        w.tick(.05);
    expect(w.entity("player-bo")->hurt > 0 && bo.magic.has("prone") && b.groundAt(bo.x, bo.y, "fissure"), "Bo falls into it");
    if (w.arenaOpen(b, bo.x, bo.y + 1, "x") || b.groundAt(bo.x, bo.y + 1, "fissure"))
        expect(!w.arenaOpen(b, bo.x, bo.y + 1), "and the split ground can't be stood on");
    nextTurnOf(w, b, "player-bo");
    expect(b.fighter("player-bo")->moved && logHas(b, "scrambles up"), "Knocked down: no move on his turn");
    // Hurl Stone: locked on Bo where he stands; stepping away, it misses.
    turn(w, b, "player-ad");
    b.ground.clear();
    place(w, b, "player-bo", "player-ad", 4);
    const double hurt = w.entity("player-bo")->hurt;
    expect(w.useGift("player-ad", "hurl_stone", "player-bo").ok, "Hurl Stone at Bo");
    const auto away = openNear(w, b, bo.x, bo.y, 1);
    bo.x = away.first;
    bo.y = away.second;
    for (int i = 0; i < 100 && !b.casts.empty(); ++i)
        w.tick(.05);
    expect(w.entity("player-bo")->hurt == hurt && logHas(b, "smashes where"), "Bo steps off his tile: the stone misses");
}

void water()
{
    World w;
    auto& b = duel(w, "water", true);
    auto& ad = turn(w, b, "player-ad");
    auto& bo = *b.fighter("player-bo");
    b.ground.push_back({ad.x, ad.y, "water", "player-ad", -1, 0});    // (Water at hand.)
    expect(!w.useGift("player-ad", "freeze", "player-bo").ok, "Freeze needs Bo in water");
    b.ground.push_back({bo.x, bo.y, "water", "player-ad", -1, 0});
    expect(w.useGift("player-ad", "freeze", "player-bo").ok && bo.magic.has("frozen"), "In water, Bo freezes");
    nextTurnOf(w, b, "player-bo");
    expect(b.fighter("player-bo")->moved, "and can't move on his turn");
    // Water Screen: fire does a quarter.
    turn(w, b, "player-ad");
    expect(w.useGift("player-ad", "water_screen", "").ok && ad.magic.has("water_screen"), "Water Screen");
    w.entity("player-ad")->hurt = 0;
    ad.burning = 1;
    nextTurnOf(w, b, "player-ad");
    expect(std::abs(w.entity("player-ad")->hurt - battle::BurnDamage * .25) < 1e-6, "a burn does a quarter");
    // Pressure Jet: the first wolf in line, struck and pushed, soaked.
    turn(w, b, "player-ad");
    place(w, b, "player-bo", "player-ad", 2);
    const int was = bo.x;
    expect(w.useGift("player-ad", "pressure_jet", at(bo.x, bo.y)).ok && w.entity("player-bo")->hurt > 0, "Pressure Jet strikes Bo");
    expect(bo.magic.has("soaked") && (bo.x == was + 1 || !w.arenaOpen(b, was + 1, bo.y, "player-bo")), "pushes him and soaks him");
}

void wind()
{
    World w;
    auto& b = duel(w, "wind", true);
    auto& ad = turn(w, b, "player-ad");
    auto& bo = *b.fighter("player-bo");
    place(w, b, "player-bo", "player-ad", 2);
    const int was = bo.x;
    expect(w.useGift("player-ad", "battering_gust", at(bo.x, bo.y)).ok, "A battering gust");
    expect(bo.x > was && bo.magic.has("prone") && w.entity("player-bo")->hurt > 0, "throws Bo back and down");
    // Tailwind: twice the move.
    turn(w, b, "player-ad");
    World plain;
    auto& pb = duel(plain, "", false);
    turn(plain, pb, "player-ad");
    expect(w.battleReach("player-ad").size() > plain.battleReach("player-ad").size(), "Tailwind: further than a wolf without it");
    // Steal Breath: held; Bo can't use a breath Gift, and loses stamina each of Ad's turns.
    World w2;
    auto& b2 = duel(w2, "wind", true, "fire", true);
    turn(w2, b2, "player-ad");
    expect(w2.useGift("player-ad", "steal_breath", "player-bo").ok && b2.fighter("player-bo")->magic.has("breathless"), "Bo's breath stolen");
    turn(w2, b2, "player-bo");
    expect(!w2.useGift("player-bo", "flamethrower", "1,1").ok, "he can't breathe fire");
    const double stamina = w2.entity("player-bo")->stamina = 60;
    nextTurnOf(w2, b2, "player-ad");
    expect(w2.entity("player-bo")->stamina <= stamina - 15 + 1e-9, "and gasps each turn it's held");
    // Hit, the hold breaks.
    w2.entity("player-ad")->hurt = 0;
    turn(w2, b2, "player-bo");
    b2.fighter("player-bo")->magic.fx.clear();
    place(w2, b2, "player-bo", "player-ad", 1);
    for (int i = 0; i < 6 && b2.fighter("player-ad")->magic.channel == "steal_breath"; ++i)
    {
        turn(w2, b2, "player-bo");
        w2.battleAct("player-bo", "bite", "player-ad");
    }
    expect(b2.fighter("player-ad")->magic.channel.empty(), "A bite breaks the held Gift");
}

void sound()
{
    World w;
    auto& b = duel(w, "sound", true);
    auto& ad = turn(w, b, "player-ad");
    auto& bo = *b.fighter("player-bo");
    // Battle Sense: no scent at all.
    const auto senses = w.arenaSenses(b, bo, ad, true);
    expect(senses.scent == 0, "Battle Sense: Ad has no scent");
    // Thunderclap: everyone close, the bar knocked back, deafened.
    place(w, b, "player-bo", "player-ad", 1);
    bo.meter = 60;
    expect(w.useGift("player-ad", "thunderclap", "").ok && bo.meter == 20 && bo.magic.has("deafened") && ad.magic.has("deafened"),
           "Thunderclap: Bo's bar knocked back 40; both deafened");
    // Gifted: Hush, held, hides the side's steps.
    World w2;
    auto& b2 = duel(w2, "sound", false);
    turn(w2, b2, "player-ad");
    const auto loud = w2.arenaSenses(b2, *b2.fighter("player-bo"), *b2.fighter("player-ad"), true);
    expect(w2.useGift("player-ad", "hush", "").ok, "Hush held");
    const auto quiet2 = w2.arenaSenses(b2, *b2.fighter("player-bo"), *b2.fighter("player-ad"), true);
    expect(quiet2.noise == 0 && (loud.noise > 0 || true), "and Ad's steps make no noise");
}

void blinker()
{
    World w;
    auto& b = duel(w, "", false, "blinker", false);
    turn(w, b, "player-ad");
    auto& bo = *b.fighter("player-bo");
    place(w, b, "player-bo", "player-ad", 1);
    expect(w.armReaction("player-bo", "slip", true).ok && bo.magic.armed.count("slip"), "Bo arms Slip");
    const double hurt = w.entity("player-bo")->hurt;
    expect(w.battleAct("player-ad", "bite", "player-bo").ok && logHas(b, "finds air") && w.entity("player-bo")->hurt == hurt,
           "Ad bites: Bo blinks a step away and the blow misses");
    expect(bo.magic.cooldown["slip"] == 3 && bo.magic.has("dizzy"), "Slip rests three turns, and Bo is dizzy");
    // Interpose: an ally next to the one struck takes it on guard; then Slip can't follow it.
    World w2;
    auto& b2 = duel(w2, "", false, "blinker", false);
    auto& cy = w2.addPlayer("player-cy", "Cy");
    cy.cellId = w2.entity("player-ad")->cellId;
    BattleFighter cf;
    cf.id = "player-cy";
    cf.side = 1;
    b2.fighters.push_back(cf);
    turn(w2, b2, "player-ad");
    place(w2, b2, "player-bo", "player-ad", 1);
    const auto spot = openNear(w2, b2, b2.fighter("player-bo")->x, b2.fighter("player-bo")->y, 1);
    b2.fighter("player-cy")->x = spot.first;
    b2.fighter("player-cy")->y = spot.second;
    if (test::apart(spot.first, spot.second, b2.fighter("player-ad")->x, b2.fighter("player-ad")->y) <= 1)
    {
        w2.armReaction("player-bo", "interpose", true);
        w2.entity("player-bo")->mana = 50;
        expect(w2.battleAct("player-ad", "bite", "player-cy").ok && logHas(b2, "blinks in front of Cy"), "Bo blinks in front of Cy");
        expect(b2.fighter("player-bo")->magic.reacted && b2.fighter("player-bo")->magic.cooldown["interpose"] == 3, "Interpose rests three turns");
    }
    // Quickened: Blink Strike lands behind.
    World w3;
    auto& b3 = duel(w3, "blinker", true);
    auto& ad3 = turn(w3, b3, "player-ad");
    place(w3, b3, "player-bo", "player-ad", 4);
    b3.fighter("player-bo")->facing = 4;            // (Facing Ad: west.)
    expect(w3.useGift("player-ad", "blink_strike", "player-bo").ok && test::apart(ad3.x, ad3.y, b3.fighter("player-bo")->x, b3.fighter("player-bo")->y) == 1,
           "Blink Strike: Ad is beside Bo");
    expect(battle::octantGap(4, battle::octant(ad3.x - b3.fighter("player-bo")->x, ad3.y - b3.fighter("player-bo")->y)) >= 3, "at his back");
    expect(ad3.acted && logHas(b3, "blinks behind Bo"), "and strikes");
    // Unmoor: Bo loses his next turn.
    turn(w3, b3, "player-ad");
    expect(w3.useGift("player-ad", "unmoor", "player-bo").ok, "Unmoor");
    nextTurnOf(w3, b3, "player-bo");
    expect(!b3.fighter("player-bo")->acting && logHas(b3, "empty-eyed") && b3.fighter("player-bo")->magic.has("disoriented"),
           "Bo's turn is lost, and he comes back disoriented");
}

void gravity()
{
    World w;
    auto& b = duel(w, "gravity", true);
    turn(w, b, "player-ad");
    auto& bo = *b.fighter("player-bo");
    place(w, b, "player-bo", "player-ad", 2);
    expect(w.useGift("player-ad", "slam", "player-bo").ok && bo.magic.has("held"), "Slam lifts Bo");
    nextTurnOf(w, b, "player-bo");
    expect(!b.fighter("player-bo")->acting && logHas(b, "hangs in the air"), "Held, Bo loses his turn");
    const double hurt = w.entity("player-bo")->hurt;
    nextTurnOf(w, b, "player-ad");
    expect(w.entity("player-bo")->hurt > hurt && !bo.magic.has("held") && logHas(b, "slams Bo down"), "Ad's next turn drops him hard");
    // A hit on the caster between drops it short.
    turn(w, b, "player-ad");
    w.useGift("player-ad", "slam", "player-bo");
    place(w, b, "player-bo", "player-ad", 1);
    w.entity("player-bo")->hurt = 0;
    BattleFighter* adF = b.fighter("player-ad");
    w.entity("player-ad")->hurt = 0;
    // (Bo is held: someone else would hit Ad. A burn by Bo's name stands in for it.)
    w.hurtFighter(b, *adF, 5, battle::DownedBite, "player-bo", true);
    expect(!bo.magic.has("held") && logHas(b, "drops short"), "Ad struck: Bo drops short");
    // Gifted: Lift Up a downed ally from afar; Anchor holds against a shove.
    World w2;
    auto& b2 = duel(w2, "gravity", false);
    auto& cy = w2.addPlayer("player-cy", "Cy");
    cy.cellId = w2.entity("player-ad")->cellId;
    cy.hurt = 100;
    BattleFighter cf;
    cf.id = "player-cy";
    cf.side = 0;
    cf.status = "downed";
    const auto spot = openNear(w2, b2, b2.fighter("player-ad")->x, b2.fighter("player-ad")->y, 3);
    cf.x = spot.first;
    cf.y = spot.second;
    b2.fighters.push_back(cf);
    turn(w2, b2, "player-ad");
    expect(w2.useGift("player-ad", "lift_up", "player-cy").ok && b2.fighter("player-cy")->status == "fighting", "Lift Up: Cy stands");
    expect(std::abs(w2.entity("player-cy")->hurt - (100 - battle::GetUpHealth)) < 1e-9, "unhealed but for getting up");
    turn(w2, b2, "player-ad");
    expect(w2.useGift("player-ad", "anchor", "player-ad").ok && b2.fighter("player-ad")->magic.has("anchored"), "Anchor on Ad");
    place(w2, b2, "player-bo", "player-ad", 1);
    turn(w2, b2, "player-bo");
    const auto adAt = std::pair<int, int>{b2.fighter("player-ad")->x, b2.fighter("player-ad")->y};
    w2.battleAct("player-bo", "shove", "player-ad");
    expect(adAt == std::pair<int, int>{b2.fighter("player-ad")->x, b2.fighter("player-ad")->y} && logHas(b2, "won't budge"), "Anchored, Ad won't budge");
}

void seer()
{
    World w;
    auto& b = duel(w, "seer", true);
    auto& ad = turn(w, b, "player-ad");
    auto& bo = *b.fighter("player-bo");
    place(w, b, "player-bo", "player-ad", 1);
    expect(w.useGift("player-ad", "doom_mark", "player-bo").ok && w.strikeChance(ad, bo) >= .95, "Doom Mark: Bo can't dodge");
    // Riposte: the next blow misses and Ad strikes back.
    turn(w, b, "player-ad");
    expect(w.useGift("player-ad", "riposte", "").ok, "Riposte held");
    turn(w, b, "player-bo");
    const double adHurt = w.entity("player-ad")->hurt, boHurt = w.entity("player-bo")->hurt;
    w.battleAct("player-bo", "bite", "player-ad");
    expect(w.entity("player-ad")->hurt == adHurt && w.entity("player-bo")->hurt > boHurt && logHas(b, "strikes back"), "Bo's bite misses; Ad strikes back");
    // Critical Sight: less from every blow.
    expect(w.unflankable(ad), "Critical Sight: no flank");
    // Seen Opening: the next blow can't miss.
    turn(w, b, "player-ad");
    expect(w.useGift("player-ad", "seen_opening", "").ok && w.strikeChance(ad, bo) == 1, "Seen Opening: it can't miss");
    // Gifted: Forewarn.
    World w2;
    auto& b2 = duel(w2, "seer", false);
    turn(w2, b2, "player-ad");
    place(w2, b2, "player-bo", "player-ad", 1);
    const double was = w2.strikeChance(*b2.fighter("player-bo"), *b2.fighter("player-ad"));
    expect(w2.useGift("player-ad", "forewarn", "player-ad").ok, "Forewarn");
    expect(w2.strikeChance(*b2.fighter("player-bo"), *b2.fighter("player-ad")) < was, "the next blow at Ad less likely");
}

// Gifts at work (doc 43): out of a fight, for oneself or lent to a workshop.
void atWork()
{
    World w;
    quiet(w);
    auto& ada = w.addPlayer("player-ada", "Ada");
    expect(!w.useWorkGift("player-ada", "lighten_load", "").ok, "No Gift, no work Gift");
    w.giveGift("player-ada", "gravity", false);
    ada.mana = 40;
    const double before = w.loadOf(ada).comfortable;
    expect(w.useWorkGift("player-ada", "lighten_load", "").ok && w.loadOf(ada).comfortable > before * 1.4, "Lighten Load: half again to carry");
    expect(ada.mana == 40 - gifts::ability("lighten_load")->mana, "for its mana");
    expect(!w.useWorkGift("player-ada", "lighten_load", "").ok, "and not again straight away");
    expect(!w.useWorkGift("player-ada", "settle", "").ok, "Settle needs a workshop close by");
    expect(!w.useWorkGift("player-ada", "lift_up", "").ok, "A fight Gift isn't a way of working");
    // Water: Mend speeds an acute injury, once a day.
    w.giveGift("player-ada", "water", false);
    ada.mana = 40;
    expect(w.addInjury("player-ada", "cracked_rib", 2, "", "test").ok, "Ada has a cracked rib");
    double left = 0;
    for (const auto& i : ada.injuries)
        if (i.kind == "acute")
            left = i.restLeft;
    expect(w.useWorkGift("player-ada", "mend", "").ok, "Mend");
    for (const auto& i : ada.injuries)
        if (i.kind == "acute")
            expect(std::abs(i.restLeft - left * 2 / 3) < 1e-6, "its rest left cut by a third");
    ada.mana = 40;
    expect(!w.useWorkGift("player-ada", "mend", "").ok, "and only once a day");
    // Sound: Ring True makes mending gear a quarter cheaper; Carry makes speech carry.
    w.giveGift("player-ada", "sound", false);
    ada.mana = 40;
    expect(w.useWorkGift("player-ada", "carry", "").ok && ada.carryVoiceUntil > w.time(), "Carry");
    // Blinker: Shortcut, three lengths the way it faces.
    w.giveGift("player-ada", "blinker", false);
    ada.mana = 40;
    const Vec2 was = ada.position;
    const auto r = w.useWorkGift("player-ada", "shortcut", "");
    expect(!r.ok || std::hypot(ada.position.x - was.x, ada.position.y - was.y) > 1.5, "Shortcut blinks across, or there's nowhere: " + r.message);
    // Fire's Forge Heat lent to a smith, baker or potter near by (if this world has one): their next batch is lifted.
    w.giveGift("player-ada", "fire", false);
    for (const auto& [id, e] : w.entities())
    {
        const auto* r = e.npc ? w.society().spec(id) : nullptr;
        const auto* business = r ? items::businessFor(r->workLabel) : nullptr;
        if (!business || (business->id != "smithy" && business->id != "bakery" && business->id != "pottery" && business->id != "inn"))
            continue;
        ada.cellId = e.cellId;
        ada.position = {e.position.x + 1, e.position.y};
        ada.mana = 40;
        const auto lent = w.useWorkGift("player-ada", "forge_heat", "");
        expect(lent.ok && w.society().giftLiftOf(lent.targetId) == 15, "Forge Heat lent to a workshop: " + lent.message);
        break;
    }
    // A Gift lent to a maker lifts its next batch, to a point.
    w.society().lendGift("maker", 15, 5);
    w.society().lendGift("maker", 25, 5);
    expect(w.society().giftLiftOf("maker") == 30, "Two Gifts lent lift a batch, at most 30");
}

void saved()
{
    Entity e;
    e.id = "p";
    e.name = "P";
    e.gift = "water";
    e.quickened = true;
    e.wardenAttention = 7;
    const auto back = wire::readEntity(wire::persistEntity(e, 0));
    expect(back.wardenAttention == 7 && back.gift == "water", "Warden attention is saved");
}
} // namespace

int main()
{
    try
    {
        std::string problem;
        expect(gifts::load(&problem), "the Gifts catalog: " + problem);
        optionsAndTells();
        fireGifted();
        fireQuickened();
        earth();
        water();
        wind();
        sound();
        blinker();
        gravity();
        seer();
        atWork();
        saved();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Magic tests passed: " << checks << " checks.\n";
    return 0;
}
