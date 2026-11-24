#pragma once
// A story's foes (Docs/Design/86, C10): how strong each kind of foe is. The table is Data/Storylines/foes.json, or the
// trial table SIM_FOES names (foe_sim's passes read one without editing Data/ until it is accepted).
#include <cstdint>
#include <string>

namespace ratw::foes
{
struct Strength
{
    double skill = 60, strength = 60, hp = 20, flee = 25;
};

// "tough", "fierce", "champion" (a foe's leader) or "follower"; null for anything else.
const Strength* strength(const std::string& kind);
bool leaderKind(const std::string& kind);           // tough, fierce or champion.
// A follower's name ("one of the Grey Captain's band") and description.
std::string followerName(const std::string& leader);
std::string followerLook();
std::string tuned();                                // The table's "tuned" note (empty until a pass is accepted).
std::string loadError();                            // Why the table could not be read ("" when it was).
// The run's mark (FNV-1a, 32 bits, of the run's id): the 8 hex digits of a foe's camp id, and the `coins` of every foe
// event, so the runner gives a foe event only to the run whose foe it was (two runs may name a foe alike).
std::uint32_t runMark(const std::string& run);
// What a blow is worth against one of this kind: 20 / hp (an ordinary bandit's 20 is 1).
double hurtScale(const std::string& kind);
} // namespace ratw::foes
