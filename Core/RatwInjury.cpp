// Injuries that outlast a fight (Docs/Design/38-injuries.md, phases 3 and 4). See RatwInjury.h.
#include "RatwInjury.h"

#include "RatwCalendar.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>

namespace ratw::injury
{
namespace
{
struct Kind
{
    const char* type;
    const char* part;
    const char* name;
    const char* causes;              // Space-separated causes it comes from.
    bool lasting;
    bool sided;
};
// Acute (doc 38's "What it is") and lasting ("What they are"). A lasting one's part is the body part it takes, one per
// part (the two ears apart).
constexpr Kind Kinds[] = {
    {"torn_flank", "ribs", "torn flank", "bite", false, false},
    {"bitten_foreleg", "leg", "bitten foreleg", "bite", false, true},
    {"bitten_hindleg", "leg", "bitten hind leg", "bite", false, true},
    {"torn_ear_acute", "ear", "torn ear", "bite", false, true},
    {"wrenched_neck", "neck", "wrenched neck", "bite", false, false},
    {"deep_gash", "ribs", "deep gash", "sword", false, false},
    {"cut_foreleg", "leg", "cut foreleg", "sword", false, true},
    {"cut_muzzle", "muzzle", "cut muzzle", "sword", false, false},
    {"cut_shoulder", "neck", "cut shoulder", "sword", false, true},
    {"bruised_ribs", "ribs", "bruised ribs", "blunt", false, false},
    {"cracked_rib", "ribs", "cracked rib", "blunt", false, false},
    {"sprained_foreleg", "leg", "sprained foreleg", "blunt", false, true},
    {"knocked_senseless", "head", "knocked senseless", "blunt", false, false},
    {"burned_paws", "burns", "burned paws", "fire", false, false},
    {"singed_coat", "burns", "singed coat", "fire", false, false},
    {"burned_muzzle", "burns", "burned muzzle", "fire", false, false},
    {"torn_ear", "ear", "torn ear", "bite sword", true, true},
    {"bent_tail", "tail", "bent tail", "blunt bite", true, false},
    {"scarred_muzzle", "muzzle", "scarred muzzle", "sword bite fire", true, false},
    {"scarred_flank", "flank", "scarred flank", "sword bite", true, true},
    {"burn_scars", "burns", "burn scars", "fire", true, false},
    {"notched_nose", "nose", "notched nose", "bite sword", true, false},
    {"clouded_eye", "eye", "clouded eye", "sword fire blunt", true, true},
    // After a Trance (doc 45): only a full rest takes it away; never rolled from a blow.
    {"trance_fatigue", "trance", "trance fatigue", "trance", false, false},
    // Set from a severe acute injury (never rolled from a cause).
    {"permanent_limp", "leg", "permanent limp", "", true, false},
    {"bad_back", "ribs", "bad back", "", true, false},
    {"stiff_shoulder", "neck", "stiff shoulder", "", true, false},
};

const Kind* kind(const std::string& type)
{
    for (const auto& k : Kinds)
        if (type == k.type)
            return &k;
    return nullptr;
}

bool from(const Kind& k, const std::string& cause)
{
    const std::string causes = std::string(" ") + k.causes + " ";
    return !cause.empty() && causes.find(" " + cause + " ") != std::string::npos;
}

double pick(double low, double high, double roll) { return low + (high - low) * std::clamp(roll, 0.0, .999999); }
std::string capitalised(std::string s)
{
    if (!s.empty() && s[0] >= 'a' && s[0] <= 'z')
        s[0] = char(s[0] - 'a' + 'A');
    return s;
}
} // namespace

bool Effects::any() const
{
    return sprint < 1 || slowWalk || arenaMove > 0 || recovery < 1 || attackStamina > 0 || biteLess > 0 || swordLess > 0 ||
           hearing < 1 || vision < 1 || smell < 1 || initiative < 1 || fireExtra > 0;
}

bool known(const std::string& type) { return kind(type) != nullptr; }
bool lasting(const std::string& type) { const auto* k = kind(type); return k && k->lasting; }
std::string part(const std::string& type) { const auto* k = kind(type); return k ? k->part : ""; }

std::string name(const Injury& i)
{
    const auto* k = kind(i.type);
    std::string n = k ? k->name : i.type;
    if (!i.side.empty() && k && k->sided)
    {
        // "Torn left ear", "Cut right foreleg", "Scarred left flank".
        const auto space = n.find(' ');
        n = space == std::string::npos ? i.side + " " + n : n.substr(0, space) + " " + i.side + n.substr(space);
    }
    return capitalised(n);
}

int severityNow(const Injury& i)
{
    if (i.kind != "acute" || i.restFull <= 0)
        return std::clamp(i.severity, 1, 3);
    const double left = std::clamp(i.restLeft / i.restFull, 0.0, 1.0);
    return std::clamp(int(std::ceil(i.severity * left - 1e-9)), 1, i.severity);
}

std::string severityWord(int severity) { return severity >= 3 ? "severe" : severity == 2 ? "moderate" : "minor"; }

Effects effects(const std::vector<Injury>& injuries)
{
    Effects e;
    if (injuries.empty())
        return e;
    // Acute: what each takes away, summed, then the floors.
    double sprintOff = 0, recoveryOff = 0, hearingOff = 0, visionOff = 0, smellOff = 0, initiativeOff = 0;
    // Lasting: summed apart, at most a quarter of any one.
    double lSprint = 0, lRecovery = 0, lHearing = 0, lVision = 0, lSmell = 0, lDamage = 0;
    for (const auto& i : injuries)
    {
        const std::string p = part(i.type);
        if (i.kind == "lasting")
        {
            if (i.type == "torn_ear") lHearing += .10;
            else if (i.type == "notched_nose") lSmell += .10;
            else if (i.type == "clouded_eye") lVision += .15;
            else if (i.type == "permanent_limp") lSprint += .10;
            else if (i.type == "bad_back") lRecovery += .15;
            else if (i.type == "stiff_shoulder") lDamage += 2;
            continue;
        }
        const int s = severityNow(i);
        if (p == "leg")
        {
            sprintOff += s == 1 ? .05 : s == 2 ? .10 : .20;
            if (s >= 2) e.arenaMove = 1;
            if (s >= 3) e.slowWalk = true;
        }
        else if (p == "ribs")
        {
            recoveryOff += s == 1 ? .10 : s == 2 ? .20 : .35;
            e.attackStamina = std::max(e.attackStamina, s == 2 ? 2.0 : s >= 3 ? 4.0 : 0.0);
        }
        else if (p == "neck")
        {
            e.biteLess = std::max(e.biteLess, s == 1 ? 1.0 : s == 2 ? 2.0 : 4.0);
            e.swordLess = std::max(e.swordLess, s == 1 ? 0.0 : s == 2 ? 2.0 : 4.0);
        }
        else if (p == "muzzle")
            smellOff += s == 1 ? .10 : s == 2 ? .25 : .40;
        else if (p == "ear")
            hearingOff += s == 1 ? .10 : s == 2 ? .25 : .40;
        else if (p == "head")
        {
            if (s == 1) visionOff += .10;
            else if (s == 2) initiativeOff += .10;
            else { initiativeOff += .20; visionOff += .25; }
        }
        else if (p == "trance")
        {
            e.mana = s == 1 ? .85 : s == 2 ? .70 : .55;
            recoveryOff += s == 1 ? .10 : s == 2 ? .20 : .30;
            initiativeOff += s == 1 ? .05 : s == 2 ? .10 : .15;
        }
        else if (p == "burns")
        {
            if (s == 1) e.fireExtra = std::max(e.fireExtra, 1.0);
            else if (s == 2) recoveryOff += .10;
            else { recoveryOff += .20; e.fireExtra = std::max(e.fireExtra, 2.0); }
        }
    }
    e.sprint = std::max(.6, 1 - sprintOff) * (1 - std::min(.25, lSprint));
    e.recovery = std::max(.5, 1 - recoveryOff) * (1 - std::min(.25, lRecovery));
    e.hearing = std::max(.5, 1 - hearingOff) * (1 - std::min(.25, lHearing));
    e.vision = std::max(.5, 1 - visionOff) * (1 - std::min(.25, lVision));
    e.smell = std::max(.5, 1 - smellOff) * (1 - std::min(.25, lSmell));
    e.initiative = 1 - initiativeOff;
    e.biteLess += lDamage;
    e.swordLess += lDamage;
    return e;
}

std::string does(const Injury& i)
{
    if (i.kind == "lasting")
    {
        if (i.type == "torn_ear") return "Hearing 10% less.";
        if (i.type == "notched_nose") return "Smell 10% less.";
        if (i.type == "clouded_eye") return "Sight 15% less.";
        if (i.type == "permanent_limp") return "A sprint 10% slower; it shows in how you move.";
        if (i.type == "bad_back") return "Stamina comes back 15% slower.";
        if (i.type == "stiff_shoulder") return "Bites and sword strokes 2 weaker.";
        return "A mark: it does nothing but tell your story.";
    }
    const std::string p = part(i.type);
    const int s = severityNow(i);
    if (p == "leg") return s == 1 ? "A sprint 5% slower." : s == 2 ? "A sprint 10% slower; a tile off a fight move."
                                                                   : "A sprint 20% slower; a tile off a fight move; a slower walk.";
    if (p == "ribs") return s == 1 ? "Stamina comes back 10% slower." : s == 2 ? "Stamina back 20% slower; attacks cost 2 more breath."
                                                                               : "Stamina back 35% slower; attacks cost 4 more breath.";
    if (p == "neck") return s == 1 ? "Bites 1 weaker." : s == 2 ? "Bites and sword strokes 2 weaker." : "Bites and sword strokes 4 weaker.";
    if (p == "muzzle") return s == 1 ? "Smell 10% less." : s == 2 ? "Smell 25% less." : "Smell 40% less.";
    if (p == "ear") return s == 1 ? "Hearing 10% less." : s == 2 ? "Hearing 25% less." : "Hearing 40% less.";
    if (p == "head") return s == 1 ? "Sight 10% less." : s == 2 ? "Slower to act in a fight (10%)." : "Slower to act (20%); sight 25% less.";
    if (p == "trance") return s == 1 ? "Until a full rest: mana 15% less, slower to act (5%), stamina back 10% slower; no overreaching."
                              : s == 2 ? "Until a full rest: mana 30% less, slower to act (10%), stamina back 20% slower; no overreaching."
                                       : "Until a full rest: mana 45% less, slower to act (15%), stamina back 30% slower; no overreaching.";
    if (p == "burns") return s == 1 ? "Fire hurts you 1 more." : s == 2 ? "Stamina comes back 10% slower."
                                                                        : "Stamina back 20% slower; fire hurts you 2 more.";
    return "";
}

std::string dateWords(double day)
{
    const auto c = calendar::calendarAt(std::max(0.0, day));
    if (!c.valid)
        return "";
    const int length = c.season == calendar::Season::Winter ? 90 : c.season == calendar::Season::Autumn ? 91 : 92;
    const char* when = c.dayOfSeason <= length / 3 ? "early" : c.dayOfSeason <= 2 * length / 3 ? "mid" : "late";
    std::string season = calendar::seasonName(c.season);
    for (auto& ch : season)
        if (ch >= 'A' && ch <= 'Z')
            ch = char(ch - 'A' + 'a');
    return std::string(when) + (std::string(when) == "mid" ? "-" : " ") + season + ", year " + std::to_string(c.year);
}

std::string describe(const Injury& i)
{
    if (i.kind == "lasting")
    {
        std::string how = i.from.empty() ? "in a fight" : "in a fight with " + i.from;
        const auto when = dateWords(i.gotDay);
        return name(i) + ": " + how + (when.empty() ? "" : ", " + when);
    }
    if (i.type == "trance_fatigue")
        return name(i) + ": " + severityWord(i.severity) + ", until a full rest";
    const double days = i.restLeft / 24;
    const std::string left = days < .75 ? "nearly healed" : days < 1.5 ? "about a day more of rest"
                                                                       : "about " + std::to_string(int(std::lround(days))) + " more days of rest";
    return name(i) + ": " + severityWord(severityNow(i)) + ", healing (" + left + ")";
}

std::string visible(const std::vector<Injury>& injuries)
{
    // Acute ones while they last ("limping on a bitten foreleg"), then the marks.
    std::vector<std::string> seen;
    for (const auto& i : injuries)
    {
        if (i.kind != "acute" || part(i.type) == "trance")
            continue;                               // (Trance fatigue doesn't show: doc 45.)
        const std::string p = part(i.type), n = name(i);
        std::string lower = n;
        if (!lower.empty())
            lower[0] = char(std::tolower(static_cast<unsigned char>(lower[0])));
        if (p == "leg") seen.push_back("limping on a " + lower);
        else if (p == "ribs") seen.push_back(i.type == "deep_gash" || i.type == "torn_flank" ? "a " + lower + ", healing" : "moving stiffly, favouring the ribs");
        else if (p == "burns") seen.push_back(i.type == "singed_coat" ? "a singed coat" : lower + ", healing");
        else if (p == "head") seen.push_back("a dazed look");
        else seen.push_back("a " + lower + ", healing");
    }
    for (const auto& i : injuries)
        if (i.kind == "lasting")
        {
            std::string lower = name(i);
            if (!lower.empty())
                lower[0] = char(std::tolower(static_cast<unsigned char>(lower[0])));
            seen.push_back(i.type == "burn_scars" ? "burn scars, the coat grown back patchy"
                           : i.type == "permanent_limp" ? "a limp that never went away"
                           : i.type == "bad_back" ? "a stiff, careful way of moving"
                           : i.type == "stiff_shoulder" ? "a stiff shoulder" : "a " + lower);
        }
    std::sort(seen.begin(), seen.end());
    seen.erase(std::unique(seen.begin(), seen.end()), seen.end());
    std::string out;
    for (std::size_t n = 0; n < seen.size(); ++n)
        out += (n == 0 ? "" : n + 1 == seen.size() ? ", and " : ", ") + seen[n];
    return out;
}

Injury acute(const std::string& cause, double damage, double overkill, bool downing, int downs, double roll, double roll2)
{
    std::vector<const Kind*> options;
    for (const auto& k : Kinds)
        if (!k.lasting && from(k, cause))
            options.push_back(&k);
    if (options.empty())
        for (const auto& k : Kinds)
            if (!k.lasting && from(k, "bite"))
                options.push_back(&k);
    const auto* k = options[std::min(options.size() - 1, std::size_t(std::clamp(roll, 0.0, .999999) * double(options.size())))];
    Injury i;
    i.type = k->type;
    i.cause = cause;
    i.side = k->sided ? (roll2 < .5 ? "left" : "right") : "";
    i.severity = damage > 35 || overkill >= 15 || (downing && downs >= 2) ? 3 : damage > 20 || downing ? 2 : 1;
    // Days of rest by severity (minor 1–2, moderate 3–6, severe 7–14), 24 rest hours a day.
    const double days = i.severity == 1 ? pick(1, 2, roll2) : i.severity == 2 ? pick(3, 6, roll2) : pick(7, 14, roll2);
    i.restFull = i.restLeft = std::round(days * 24);
    return i;
}

Injury lastingFrom(const std::string& cause, const std::vector<Injury>& has, double roll, const Injury* fromAcute)
{
    // One a body part; the two ears and the two flanks apart.
    const auto taken = [&](const std::string& type, const std::string& side) {
        for (const auto& i : has)
            if (i.kind == "lasting" && (i.type == type ? (side.empty() || i.side == side) : part(i.type) == part(type) && part(type) != "ear" && part(type) != "flank"))
                return true;
        return false;
    };
    Injury i;
    i.kind = "lasting";
    i.cause = cause;
    if (fromAcute)
    {
        const std::string p = part(fromAcute->type);
        i.type = p == "leg" ? "permanent_limp" : p == "ribs" ? "bad_back" : p == "neck" ? "stiff_shoulder" : p == "ear" ? "torn_ear"
               : p == "muzzle" ? "scarred_muzzle" : p == "burns" ? "burn_scars" : p == "head" ? "clouded_eye" : "";
        i.side = i.type == "torn_ear" || i.type == "clouded_eye" ? (fromAcute->side.empty() ? "left" : fromAcute->side) : "";
        if (!i.type.empty() && !taken(i.type, i.side))
            return i;
    }
    std::vector<std::pair<const Kind*, std::string>> options;
    for (const auto& k : Kinds)
        if (k.lasting && from(k, cause))
        {
            if (k.sided)
            {
                for (const char* side : {"left", "right"})
                    if (!taken(k.type, side))
                        options.emplace_back(&k, side);
            }
            else if (!taken(k.type, ""))
                options.emplace_back(&k, "");
        }
    if (options.empty())
        return {};
    const auto& [k, side] = options[std::min(options.size() - 1, std::size_t(std::clamp(roll, 0.0, .999999) * double(options.size())))];
    i.type = k->type;
    i.side = side;
    return i;
}

Injury given(const std::string& type, int severity, const std::string& side, const std::string& from)
{
    Injury i;
    const auto* k = kind(type);
    if (!k)
        return i;
    i.type = type;
    i.kind = k->lasting ? "lasting" : "acute";
    i.side = k->sided && (side == "left" || side == "right") ? side : k->sided ? "left" : "";
    i.from = from.substr(0, 80);
    if (i.kind == "acute")
    {
        i.severity = std::clamp(severity, 1, 3);
        i.restFull = i.restLeft = type == "trance_fatigue" ? 1 : (i.severity == 1 ? 1.5 : i.severity == 2 ? 4.5 : 10.5) * 24;
    }
    return i;
}

bool spent(const std::vector<Injury>& injuries)
{
    return std::any_of(injuries.begin(), injuries.end(), [](const Injury& i) { return i.type == "trance_fatigue"; });
}

bool give(std::vector<Injury>& injuries, const Injury& i)
{
    if (i.type.empty())
        return false;
    if (i.kind == "acute")
    {
        addAcute(injuries, i);
        return true;
    }
    if (injuries.size() >= MostKept)
        return false;
    injuries.push_back(i);
    return true;
}

std::string takeAway(std::vector<Injury>& injuries, const std::string& id)
{
    const auto it = std::find_if(injuries.begin(), injuries.end(), [&](const Injury& i) { return i.id == id; });
    if (it == injuries.end())
        return "";
    const auto n = name(*it);
    injuries.erase(it);
    return n;
}

const Injury& addAcute(std::vector<Injury>& injuries, Injury i)
{
    for (auto& had : injuries)
        if (had.kind == "acute" && had.type == i.type && had.side == i.side)
        {
            // The same again: a severity worse, and healing starts over.
            had.severity = std::min(3, std::max(severityNow(had) + 1, i.severity));
            const double days = had.severity == 2 ? 4.5 : 10.5;
            had.restFull = had.restLeft = i.type == "trance_fatigue" ? 1 : std::max(i.restFull, std::round(days * 24));
            had.cause = i.cause;
            had.from = i.from;
            had.gotDay = i.gotDay;
            return had;
        }
    if (injuries.size() >= MostKept)
        injuries.erase(injuries.begin());
    injuries.push_back(std::move(i));
    return injuries.back();
}

std::vector<std::string> heal(std::vector<Injury>& injuries, double hours)
{
    std::vector<std::string> healed;
    if (!(hours > 0))
        return healed;
    for (auto it = injuries.begin(); it != injuries.end();)
    {
        if (it->kind != "acute" || it->type == "trance_fatigue")
        {
            ++it;                                   // (Trance fatigue: only a full rest, doc 45.)
            continue;
        }
        it->restLeft -= hours;
        if (it->restLeft <= 0)
        {
            healed.push_back(name(*it));
            it = injuries.erase(it);
        }
        else
            ++it;
    }
    return healed;
}

void strain(std::vector<Injury>& injuries, double share)
{
    for (auto& i : injuries)
        if (i.kind == "acute" && i.restFull > 0)
            i.restLeft = std::min(i.restFull, i.restLeft + (i.restFull - i.restLeft) * std::clamp(share, 0.0, 1.0));
}

double weariness(double daysSinceFullRest)
{
    return std::clamp((std::floor(std::max(0.0, daysSinceFullRest)) - 1) * .05, 0.0, .25);
}
} // namespace ratw::injury

// ------------------------------------------------------------------ In the world

namespace ratw
{
namespace
{
// A roll in [0, 1) that is the same for the same key (a fight's blow), so a replay gives the same injuries.
double roll(const std::string& key)
{
    std::uint64_t h = std::hash<std::string>{}(key) * 0x9E3779B97F4A7C15ULL;
    h ^= h >> 29;
    h *= 0xBF58476D1CE4E5B9ULL;
    h ^= h >> 32;
    return double(h >> 11) / 9007199254740992.0;
}
} // namespace

std::string World::injuryCause(double downedBase, const std::string& by) const
{
    if (downedBase == battle::DownedFire)
        return "fire";
    if (downedBase == battle::DownedBlunt)
        return "blunt";
    const auto* a = by.empty() ? nullptr : entity(by);
    return a && a->mouth == "sword" ? "sword" : "bite";
}

std::string World::injurerWords(const std::string& by) const
{
    const auto* a = by.empty() ? nullptr : entity(by);
    if (!a)
        return "";
    if (!a->npc)
        return "another wolf";
    if (const_cast<World*>(this)->campOf(by))
        return "a bandit";
    return "a resident";
}

void World::giveLasting(Battle& b, Entity& e, const std::string& cause, const std::string& by, double chance, const Injury* from)
{
    auto& marks = fightMarks_[b.id];
    if (marks.marked.count(e.id))
        return;                                     // One lasting injury a fight, whatever the rolls.
    chance += injury::weariness(calendarDays_ - std::max(0.0, e.fullRestDay));
    if (roll(b.id + "|" + e.id + "|lasting|" + std::to_string(b.seq) + cause) >= chance)
        return;
    auto i = injury::lastingFrom(cause, e.injuries, roll(b.id + "|" + e.id + "|which|" + std::to_string(b.seq)), from);
    if (i.type.empty())
        return;
    i.id = injuryId() + e.id;
    i.from = injurerWords(by);
    i.gotDay = calendarDays_;
    if (from)
        e.injuries.erase(std::remove_if(e.injuries.begin(), e.injuries.end(), [&](const Injury& x) { return x.id == from->id; }),
                         e.injuries.end());    // (The acute injury sets into its lasting form.)
    if (e.injuries.size() >= injury::MostKept)
        e.injuries.erase(e.injuries.begin());
    e.injuries.push_back(i);
    marks.marked.insert(e.id);
    notice(e.id, "That will leave a mark: " + [&] { auto n = injury::name(i); n[0] = char(std::tolower(static_cast<unsigned char>(n[0]))); return n; }() +
                     ". " + injury::does(i));
}

void World::injureOnBlow(Battle& b, BattleFighter& t, double damage, double downedBase, const std::string& by)
{
    auto* e = entity(t.id);
    if (!e || e->npc || by.empty() || damage <= 0)
        return;
    if (b.terms == "spar")
    {
        // A spar (doc 53, 5): a hard blow leaves a minor bruise at worst, and never a lasting mark.
        const std::string key = b.id + "|" + t.id + "|spar|" + std::to_string(b.seq);
        bool bruised = false;
        for (const auto& i : e->injuries)
            bruised = bruised || (i.kind == "acute" && i.type == "bruised_ribs");
        if (!bruised && damage >= 25 && roll(key) < .25)
        {
            auto i = injury::given("bruised_ribs", 1, "", injurerWords(by));
            i.id = injuryId() + e->id;
            i.gotDay = calendarDays_;
            injury::give(e->injuries, i);
            notice(t.id, "That one will bruise: " + injury::describe(i) + ".");
        }
        return;
    }
    const std::string cause = injuryCause(downedBase, by);
    const bool down = e->hurt >= 100 && b.terms == "death";   // (Going down: downFighter gives that one.)
    const std::string key = b.id + "|" + t.id + "|blow|" + std::to_string(b.seq);
    if (!down && damage >= 25 && roll(key) < .25)
    {
        auto i = injury::acute(cause, damage, 0, false, e->downsSinceRest, roll(key + "k"), roll(key + "s"));
        i.id = injuryId() + e->id;
        i.from = injurerWords(by);
        i.gotDay = calendarDays_;
        const auto& now = injury::addAcute(e->injuries, i);
        notice(t.id, "That blow did harm: " + injury::describe(now) + ".");
    }
    if (damage >= 40)
        giveLasting(b, *e, cause, by, .35, nullptr);
}

void World::injureOnDown(Battle& b, BattleFighter& f, double overkill, double downedBase, const std::string& by)
{
    auto* e = entity(f.id);
    if (!e || e->npc)
        return;
    auto& marks = fightMarks_[b.id];
    marks.downed.insert(f.id);
    const std::string cause = injuryCause(downedBase, by);
    // Going down sets healing back by half; the downing blow always leaves an acute injury (decided 2026-10-04).
    injury::strain(e->injuries, .5);
    const std::string key = b.id + "|" + f.id + "|down|" + std::to_string(b.seq);
    // A severe acute injury on the part this blow strikes may set into its lasting form.
    auto i = injury::acute(cause, 0, overkill, true, e->downsSinceRest, roll(key + "k"), roll(key + "s"));
    bool severe = false;
    Injury setting;
    for (const auto& had : e->injuries)
        if (had.kind == "acute" && injury::part(had.type) == injury::part(i.type) && injury::severityNow(had) >= 3)
        {
            severe = true;
            setting = had;
        }
    i.id = injuryId() + e->id;
    i.from = injurerWords(by);
    i.gotDay = calendarDays_;
    const auto& now = injury::addAcute(e->injuries, i);
    notice(f.id, "You come away with an injury: " + injury::describe(now) + ". Rest will heal it; fighting on sets it back.");
    if (severe)
        giveLasting(b, *e, cause, by, .5, &setting);
    if (overkill >= 25)
        giveLasting(b, *e, cause, by, .35, nullptr);
    if (e->downsSinceRest == 3)
        giveLasting(b, *e, cause, by, .30, nullptr);
    else if (e->downsSinceRest >= 4)
        giveLasting(b, *e, cause, by, .60, nullptr);
    if (cause == "fire")
        giveLasting(b, *e, "fire", by, .25, nullptr);
}

void World::injureAtEnd(Battle& b)
{
    auto& marks = fightMarks_[b.id];
    for (auto& f : b.fighters)
    {
        auto* e = entity(f.id);
        if (!e || e->npc || f.status != "fighting" || marks.downed.count(f.id) || e->hurt < 75)
            continue;
        // Limping at the end without going down: 40% an acute injury.
        const std::string key = b.id + "|" + f.id + "|end";
        if (roll(key) >= .40)
            continue;
        auto i = injury::acute(roll(key + "c") < .5 ? "bite" : "blunt", 21, 0, false, e->downsSinceRest, roll(key + "k"), roll(key + "s"));
        i.id = injuryId() + e->id;
        i.gotDay = calendarDays_;
        const auto& now = injury::addAcute(e->injuries, i);
        notice(f.id, "The fight has left its mark on you: " + injury::describe(now) + ".");
    }
    fightMarks_.erase(b.id);
}

void World::strainOnEntering(const std::string& id)
{
    if (auto* e = entity(id); e && !e->npc)
        injury::strain(e->injuries, .1);            // Going into a fight on an unhealed injury.
}

void World::healInjuries(Entity& e, double restHours)
{
    if (e.injuries.empty())
        return;
    for (const auto& n : injury::heal(e.injuries, restHours))
        notice(e.id, "Your " + [&] { auto x = n; x[0] = char(std::tolower(static_cast<unsigned char>(x[0]))); return x; }() + " has healed.");
}

Result World::addInjury(const std::string& id, const std::string& type, int severity, const std::string& side, const std::string& from)
{
    auto* e = entity(id);
    if (!e || e->npc)
        return {false, "No such player.", {}};
    auto i = injury::given(type, severity, side, from);
    if (i.type.empty())
        return {false, "No such injury.", {}};
    i.id = injuryId() + e->id;
    i.gotDay = calendarDays_;
    if (!injury::give(e->injuries, i))
        return {false, "That wolf has as many injuries as can be kept.", {}};
    return {true, injury::name(i), i.id};
}

Result World::removeInjury(const std::string& id, const std::string& injuryId)
{
    auto* e = entity(id);
    if (!e || e->npc)
        return {false, "No such player.", {}};
    const auto n = injury::takeAway(e->injuries, injuryId);
    if (n.empty())
        return {false, "No such injury.", {}};
    return {true, n, injuryId};
}
} // namespace ratw
