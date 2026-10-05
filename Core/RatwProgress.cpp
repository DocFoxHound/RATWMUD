// Earning XP in the world (Docs/Design/44-levelling.md): work, practice, places first visited, skills reaching their
// milestones, contracts fulfilled. The world says what was earned; the game pays it into the social ledger
// (SocialLedger::award), which knows the amounts, the daily limits and what was paid already.
#include "RatwWorld.h"

#include <cmath>

namespace ratw
{
void World::award(const std::string& who, const std::string& kind, const std::string& source)
{
    // The game settles each: paid, already paid, or not today. Sent again only after ten minutes (a day's limit may have
    // eased), so places and skills looked at every few seconds cost nothing.
    if (const auto* e = entity(who); !e || e->npc)
        return;
    auto& sent = awardSent_[who + "|" + kind + "|" + source];
    if (sent > 0 && time_ - sent < 600)
        return;
    sent = time_;
    awards_.push_back({who, kind, source});
}

std::vector<World::Award> World::takeAwards()
{
    std::vector<Award> out;
    out.swap(awards_);
    return out;
}

void World::tendProgress()
{
    // Every few seconds, for each player in the world: a place it has never been (a discovery), an apprentice at its
    // master's work (once a game day), and its sneaking, listening and tracking reaching 25, 50 or 75.
    if (time_ - progressAt_ < 5)
        return;
    progressAt_ = time_;
    const auto day = std::to_string(std::int64_t(std::floor(calendarDays_)));
    for (const auto& [id, e] : entities_)
    {
        if (e.npc || e.dead || e.cellId.empty())
            continue;
        award(id, "discovery", "visit:" + e.cellId);
        if (const auto* job = society_.apprenticedTo(id))
        {
            const auto& positions = society_.state().careers.positions;
            const auto held = positions.find(job->id);
            const auto* master = held == positions.end() ? nullptr : entity(held->second.holder);
            if (master && master->cellId == e.cellId && std::hypot(master->position.x - e.position.x, master->position.y - e.position.y) <= 8)
                award(id, "work", "apprentice:" + day);
        }
        const std::pair<const char*, double> skills[] = {{"sneak", e.sneakSkill}, {"listening", e.hearingSkill}, {"tracking", e.scentSkill}};
        for (const auto& [name, value] : skills)
            for (const int mark : {25, 50, 75})
                if (value >= mark)
                    award(id, "milestone", std::string(name) + ":" + std::to_string(mark));
    }
}
} // namespace ratw
