#pragma once
// Working together (Docs/Design/53-hunting-and-working-together.md, 2): the cooperation scaling every joint activity
// uses, the activities and their roles (Data/Together/patterns.json), and a joint at work. Pure; World keeps the joints
// and does the work (RatwTogether.cpp's World part).
#include <map>
#include <string>
#include <vector>

namespace ratw::together
{
struct Role
{
    std::string id, words, yours, practice;        // "digs" of another; "dig" of oneself.
};

struct Activity
{
    std::string id, name, pattern;
    std::vector<Role> roles;
    int most = 4;
    double beat = 4;
    // Farm work (doc 53, 2.6): the menu's verb ("help with the threshing"), its season (0 spring .. 3 winter), the
    // producers it is done at, and the share of a spell's yield each beat brings in at a rate of 1. Its beats come by
    // the clock, while its members are there, not by each picking.
    // Work beside a resident (doc 53, 2.6 and 3): `at` "farm" (its producers, its season) or "workshop" (a maker whose
    // business crafts, any season); `residentRole` the role the resident takes (its index).
    std::string verb, at;
    int season = -1, residentRole = 1;
    std::vector<std::string> producers;
    double share = 0;
    std::vector<std::string> gifts;                // Work Gifts that lift it (doc 53, 4): Winnow and Dry at threshing.
    bool byBeat() const { return !at.empty(); }
};

struct Rules
{
    std::vector<double> steps{.8, .4, .25, .1, .1};
    double leaveTiles = 8, idleSeconds = 60, lendTiles = 6;
    double bondAffinity = 1, bondTrust = .5, bondFamiliarity = 2, residentAffinity = 2, residentTrust = 2;
    int bondBeats = 2;
    std::vector<Activity> activities;
};

const Rules& rules();
Rules parse(const std::string& text);
const Activity* activity(const std::string& id, const Rules& r = rules());

// A member as the scaling sees it: its role, and whether it is a resident hand.
struct Hand
{
    std::string role;
    bool resident = false;
};
// Each wolf's rate against working alone (doc 48, 6.3): players first in the order they joined, resident hands after;
// the 2nd adds 0.8, the 3rd 0.4, the 4th 0.25, the 5th and 6th 0.1, up to `most`; a step halved when every member so
// far shares one role, and halved for a resident hand. Alone 1; two on two angles 1.8; on one 1.4; three 2.2.
double rate(std::vector<Hand> members, int most, const Rules& r = rules());

// `count` goods split exactly among `n` sharers: each gets the whole part, and the rest one by one to sharers in the
// order `order` gives (drawn by chance by the caller). Nothing is made or lost.
std::vector<int> split(int count, int n, const std::vector<int>& order);

// A joint at work: its activity, where it began, what it works (a patch, a producer), and its members.
struct Member
{
    std::string id, role;
    bool resident = false;
    double joined = 0, lastActed = 0;
    int beats = 0;
    // Farm work: pennies earned here, and the fraction of a penny carried to the next beat.
    int earned = 0;
    double carry = 0;
    // A Gifted member's work Gift on the joint (doc 53, 4): until when it holds, and which. While it holds the member
    // counts as another angle and every member works 0.2 faster.
    double giftUntil = -1;
    std::string gift;
    // Keep watch (doc 53, 4): a wolf watching over the others in the wild, not working; its own angle.
    bool watch = false;
};
struct JointWork
{
    std::string id, kind, cellId, target;          // target: the resident whose work it is (farm work).
    double x = 0, y = 0, started = 0;
    double nextBeat = 0;                           // Farm work: when the next beat falls.
    std::vector<Member> members;
    std::map<std::string, int> beatsTogether;      // "a|b" (sorted): beats both worked, for the bond at the end.
};
// The joint's hands as the scaling sees them at `now`: a member whose Gift holds, or who keeps watch, on an angle of
// its own. And the joint's rate: the scaling plus 0.2 for each Gift holding (doc 53, 4).
std::vector<Hand> handsOf(const JointWork& j, double now = -1);
double rateOf(const JointWork& j, double now);
constexpr double GiftLift = .2;
} // namespace ratw::together
