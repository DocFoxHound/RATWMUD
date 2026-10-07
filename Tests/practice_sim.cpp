// Not a test: how fast skills grow by practice (Docs/Design/49-characters-and-earned-gifts.md, 13), on the real rules
// (Core/RatwPractice.*: room, the daily soft limit, rested practice, variety, the same partner again, a teacher) and the
// numbers in Data/Progression/skills.json. Each profile is an evening's play, repeated over simulated days; for each
// skill it grows, the table says how many days it takes to reach the "seasoned" and "veteran" bands (shares of the way
// from its start to its cap, from skills.json), and how much the soft limit held back. For the user's balance pass.
//
//   build-levels/practice_sim
//
// SIM_DAYS (365) how long; SIM_WEEK (4) evenings played a week (7: every day); SIM_PROFILE one profile by name;
// SIM_TEACHER (1) a teacher's factor on everything (1.5: a better player near; 3: an apprentice at the master's side).
// A source the catalog doesn't have yet (a fight's blows come with doc 49's phase 2) is listed as missing.
#include "RatwPractice.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace ratw;
namespace
{
// One kind of practice in an evening: a source, how many times, how far apart (seconds), and what changes as it goes
// (a new partner every `partnerEvery` times; a new 4×4 block of ground every `moveEvery`; a named occasion per `occasionEvery`).
struct Bout
{
    std::string source;
    int times = 1;
    double apart = 60;
    std::string partnerKind;
    int partnerEvery = 0, moveEvery = 1, occasionEvery = 0;
};
struct Profile
{
    std::string name, about;
    std::vector<Bout> bouts;
};

const std::vector<Profile>& profiles()
{
    static const std::vector<Profile> all = {
        {"tracker", "an evening in the wild nosing for game: 20 sniffs a minute apart, 6 trails found",
         {{"nose.use", 20, 60, "", 0, 1, 0}, {"track.found", 6, 180, "", 0, 1, 0}}},
        {"stalker", "five hunts stalking game: unnoticed close by every 10 s for 2 minutes, then an ambush",
         {{"sneak.arena", 60, 10, "animal", 12, 1, 12}, {"sneak.ambush", 5, 120, "animal", 1, 1, 1}}},
        {"prowler", "ten minutes creeping about town past residents (a check every 0.4 s, a new resident every 2 minutes)",
         {{"sneak.world", 1500, .4, "resident", 300, 300, 0}}},
        {"watcher", "a stalker caught twice by ear and once by nose",
         {{"notice.sound", 2, 600, "", 1, 1, 1}, {"notice.scent", 1, 600, "", 1, 1, 1}}},
        {"fighter", "an evening of fights (doc 49's phase 2: blows and fight ends)",
         {{"fight.blow", 40, 15, "player", 10, 1, 10}, {"fight.end", 4, 300, "player", 1, 1, 1}}},
    };
    return all;
}

struct Track
{
    double value = 0, start = 0, cap = 0, gained = 0, held = 0, rested = 0;
    int seasoned = -1, veteran = -1;
};

int envInt(const char* name, int fallback)
{
    const char* v = std::getenv(name);
    return v && *v ? std::max(1, std::atoi(v)) : fallback;
}
double envDouble(const char* name, double fallback)
{
    const char* v = std::getenv(name);
    return v && *v ? std::max(0.0, std::atof(v)) : fallback;
}

void run(const Profile& p, int days, int week, double teacher)
{
    const auto& r = practice::rules();
    std::printf("\n%s: %s\n", p.name.c_str(), p.about.c_str());
    std::map<std::string, Track> tracks;
    std::vector<std::string> missing;
    for (const auto& b : p.bouts)
    {
        const auto* source = practice::source(b.source);
        if (!source)
        {
            missing.push_back(b.source);
            continue;
        }
        for (const auto& [skill, base] : source->grows)
            if (const auto* s = practice::skill(skill); s && !tracks.count(skill))
                tracks[skill] = {s->start, s->start, s->cap};
    }
    PracticeState state;
    for (int day = 0; day < days; ++day)
    {
        if (day % 7 >= week)
            continue;                               // Not an evening played.
        double t = day * 86400.0 + 19 * 3600;       // From seven in the evening.
        int occasion = 0;
        for (const auto& b : p.bouts)
        {
            const auto* source = practice::source(b.source);
            if (!source)
                continue;
            for (int i = 0; i < b.times; ++i, t += b.apart)
            {
                const auto partner = b.partnerEvery > 0 ? b.partnerKind + std::to_string(day * 1000 + i / b.partnerEvery) : std::string();
                const auto where = std::to_string(day * 1000 + i / std::max(1, b.moveEvery));
                const auto named = b.occasionEvery > 0 ? b.source + std::to_string(day * 1000 + occasion + i / b.occasionEvery) : std::string();
                for (const auto& [skill, base] : source->grows)
                {
                    const auto* s = practice::skill(skill);
                    auto& k = tracks[skill];
                    const double room = practice::room(k.value, k.cap, r);
                    if (!s || room <= 0)
                        continue;
                    auto& d = state.days[skill];
                    const double soft = practice::soft(d, *s, t, r);
                    double full = base * room * teacher *
                                  practice::variety(state, skill + "|" + b.source + "|" + (partner.empty() ? where : partner), named, t, r);
                    if (source->byPartner)
                        full *= practice::partnerFactor(b.partnerKind, -1, k.value, r);
                    if (source->partnerDecay)
                        full *= practice::partnerDecay(state, partner, named, t, r);
                    const double gain = full * soft;
                    if (gain <= 0)
                        continue;
                    const double extra = practice::rested(state, gain, t, r);
                    d.gained += gain;
                    k.held += full - gain;
                    k.rested += extra;
                    k.value = std::min(k.cap, k.value + gain + extra);
                    k.gained += gain + extra;
                }
            }
            occasion += 100;
        }
        for (auto& [skill, k] : tracks)
        {
            const double way = (k.value - k.start) / std::max(1e-9, k.cap - k.start);
            if (k.seasoned < 0 && way >= r.seasoned)
                k.seasoned = day + 1;
            if (k.veteran < 0 && way >= r.veteran)
                k.veteran = day + 1;
        }
    }
    std::printf("  %-10s %9s %9s %10s %10s %13s %8s\n", "skill", "start", "cap", "seasoned", "veteran", "soft held", "rested");
    for (const auto& [skill, k] : tracks)
    {
        const auto when = [](int d) { return d < 0 ? std::string("never") : "day " + std::to_string(d); };
        const bool sense = k.cap < 5;
        std::printf("  %-10s %9.*f %9.*f %10s %10s %12.0f%% %7.0f%%   (end %.*f)\n", skill.c_str(), sense ? 2 : 0, k.start, sense ? 2 : 0, k.cap,
                    when(k.seasoned).c_str(), when(k.veteran).c_str(), 100 * k.held / std::max(1e-12, k.gained + k.held),
                    100 * k.rested / std::max(1e-12, k.gained), sense ? 2 : 1, k.value);
    }
    for (const auto& m : missing)
        std::printf("  (no source \"%s\" in the catalog yet)\n", m.c_str());
}
} // namespace

int main()
{
    std::string error;
    if (!practice::load(&error))
    {
        std::fprintf(stderr, "practice catalog: %s\n", error.c_str());
        return 1;
    }
    const int days = envInt("SIM_DAYS", 365), week = std::min(7, envInt("SIM_WEEK", 4));
    const double teacher = envDouble("SIM_TEACHER", 1);
    const char* only = std::getenv("SIM_PROFILE");
    std::printf("Practice pace over %d days, %d evening%s a week, teacher ×%.2g (Data/Progression/skills.json; bands: seasoned %.0f%%, "
                "veteran %.0f%% of the way to the cap).\n",
                days, week, week == 1 ? "" : "s", teacher, 100 * practice::rules().seasoned, 100 * practice::rules().veteran);
    for (const auto& p : profiles())
        if (!only || p.name == only)
            run(p, days, week, teacher);
    return 0;
}
