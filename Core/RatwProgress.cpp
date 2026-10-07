// Practice (Docs/Design/49-characters-and-earned-gifts.md): how attributes and skills grow. RatwPractice.* has the
// catalog and the arithmetic; World::practise applies a source to a player and tells them when a skill moves on.
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>

namespace ratw
{
void World::tendProgress()
{
    // Every few seconds, for each player in the world: an apprentice at its master's work learns the trade, three times
    // as fast beside the master (doc 49: "apprentice.<family>" in Data/Progression/skills.json), if the player is at the
    // keys (passive practice needs a player there: doc 49, 4). Places first visited and skills' milestones pay nothing
    // now: growth lines say when a skill moves on.
    if (time_ - progressAt_ < 5)
        return;
    progressAt_ = time_;
    for (const auto& [id, e] : entities_)
    {
        if (e.npc || e.dead || e.cellId.empty())
            continue;
        const auto* job = society_.apprenticedTo(id);
        if (!job || (playerActive && !playerActive(id)))
            continue;
        const auto& positions = society_.state().careers.positions;
        const auto held = positions.find(job->id);
        const auto* master = held == positions.end() ? nullptr : entity(held->second.holder);
        if (!master || master->cellId != e.cellId || std::hypot(master->position.x - e.position.x, master->position.y - e.position.y) > 8)
            continue;
        const std::string family = skillFamily(job->title);
        const std::string skill = family == "trade" ? "commerce" : family == "general" ? "labour" : family;
        PracticeContext context;
        context.teacher = "master";
        context.partner = master->id;
        practise(id, "apprentice." + skill, context);
    }
}

// ------------------------------------------------------------------ Practice (doc 49)

double* World::practiceSlot(Entity& e, const practice::Skill& s)
{
    static const std::pair<const char*, double Entity::*> fields[] = {
        {"strength", &Entity::strength},     {"dexterity", &Entity::dexterity},   {"wisdom", &Entity::wisdom},
        {"endurance", &Entity::endurance},   {"hearing", &Entity::hearing},       {"vision", &Entity::vision},
        {"smell", &Entity::smell},           {"fightingSkill", &Entity::fightingSkill}, {"sneakSkill", &Entity::sneakSkill},
        {"hearingSkill", &Entity::hearingSkill}, {"scentSkill", &Entity::scentSkill}};
    if (s.field.empty())
        return &e.skills[s.id];                     // A trade skill (by family).
    for (const auto& [name, member] : fields)
        if (s.field == name)
            return &(e.*member);
    return nullptr;
}

double World::practiceValue(const Entity& e, const practice::Skill& s)
{
    if (s.field.empty())
    {
        const auto at = e.skills.find(s.id);
        return at == e.skills.end() ? s.start : at->second;
    }
    auto* slot = practiceSlot(const_cast<Entity&>(e), s);   // (Read only.)
    return slot ? *slot : 0;
}

double World::practiceCap(const Entity& e, const practice::Skill& s)
{
    const auto grade = e.grades.find(s.id);
    return practice::capFor(s, e.quickened, grade == e.grades.end() ? std::string() : grade->second);
}

void World::applyBuild(Entity& e, const practice::Build& build)
{
    // Each attribute starts where its grade does (doc 49, 2); the specialty's skill starts higher (3).
    e.grades = build.grades;
    for (const auto& s : practice::skills())
        if (s.attribute)
            if (const auto a = practice::creation().grades.find(s.id); a != practice::creation().grades.end())
            {
                const auto g = a->second.find(build.grades.count(s.id) ? build.grades.at(s.id) : "plain");
                if (double* slot = practiceSlot(e, s); slot && g != a->second.end())
                    *slot = g->second.start;
            }
    e.specialty = build.specialty;
    if (const auto* sp = practice::specialty(build.specialty))
        if (const auto* s = practice::skill(sp->skill); s && s->specialtyStart >= 0)
            if (double* slot = practiceSlot(e, *s))
                *slot = std::max(*slot, s->specialtyStart);
}

double World::teacherNear(const Entity& e, const practice::Skill& s, double now)
{
    // A better player close by speeds learning (doc 49, 4): one 10 or more points better within 6 tiles. Never one of the
    // learner's own account's wolves. Looked for at most every half minute per player and skill, so practice that comes
    // every few seconds walks the cell rarely.
    const auto& r = practice::rules();
    if (teachers_.size() > 4096)
        teachers_.clear();                          // (Players come and go: an old run's entries aren't kept for ever.)
    auto& cached = teachers_[e.id + "|" + s.id];
    if (now < cached.first && now >= cached.first - r.teachCache)
        return cached.second;
    const auto own = [&](const std::string& id) {
        const auto account = accountOf ? accountOf(id) : std::string();
        return account.empty() ? id : account;
    };
    const auto mine = own(e.id);
    const double value = practiceValue(e, s);
    double factor = 1;
    for (const auto* o : entitiesIn(e.cellId))
    {
        if (!o || o == &e || o->npc || o->dead || o->cellId != e.cellId ||
            std::hypot(o->position.x - e.position.x, o->position.y - e.position.y) > r.teachReach)
            continue;
        if (practiceValue(*o, s) >= value + r.teachBetterBy && own(o->id) != mine)
        {
            factor = r.teachNearby;
            break;
        }
    }
    cached = {now + r.teachCache, factor};
    return factor;
}

void World::practise(const std::string& who, const std::string& sourceId, const PracticeContext& context)
{
    auto* e = entity(who);
    const auto* source = practice::source(sourceId);
    if (!practising || !e || e->npc || e->dead || !source)
        return;
    const auto& r = practice::rules();
    const double now = realClock ? realClock() : time_;
    for (const auto& [skillId, base] : source->grows)
    {
        const auto* s = practice::skill(skillId);
        double* value = s ? practiceSlot(*e, *s) : nullptr;
        if (!value || base <= 0)
            continue;
        const double cap = practiceCap(*e, *s);
        const double room = practice::room(*value, cap, r);
        if (room <= 0)
            continue;
        auto& day = e->practice->days[s->id];
        const double soft = practice::soft(day, *s, now, r);
        // Variety (anti-macro): by skill, source and partner, or the 4×4 block of ground where there is no partner.
        const auto where = context.partner.empty()
                               ? e->cellId + ":" + std::to_string(int(std::floor(e->position.x / r.block))) + "," +
                                     std::to_string(int(std::floor(e->position.y / r.block)))
                               : context.partner;
        double gain = base * std::max(0.0, context.amount) * room * soft * practice::variety(e->practice, s->id + "|" + source->id + "|" + where, context.occasion, now, r);
        if (source->byPartner)
            gain *= practice::partnerFactor(context.partnerKind, context.partnerValue, *value, r);
        if (source->partnerDecay)
            gain *= practice::partnerDecay(e->practice, context.partner, context.occasion, now, r);
        double teacher = teacherNear(*e, *s, now);
        if (const auto kind = r.teacherKinds.find(context.teacher); kind != r.teacherKinds.end())
            teacher = std::max(teacher, kind->second);
        gain *= teacher;
        if (gain <= 0)
            continue;
        const double extra = practice::rested(e->practice, gain, now, r);
        day.gained += gain;
        const double before = *value;
        *value = std::min(cap, *value + gain + extra);
        // Growth lines (doc 49, 4): when it passes a whole step, at most one each two minutes; once at the cap.
        if (*value >= cap)
            notice(who, practice::capText(*s));
        else if (practice::lineDue(e->practice, *s, before, *value, now, r) >= 0)
            notice(who, practice::lineText(*s, *value));
    }
}

Result World::setPractice(const std::string& who, const std::string& skillId, double value, double today)
{
    auto* e = entity(who);
    const auto* s = practice::skill(skillId);
    double* slot = e && s ? practiceSlot(*e, *s) : nullptr;
    if (!e || e->npc || !slot)
        return {false, "No such skill.", {}};
    if (!std::isfinite(value) || value < 0)
        return {false, "Not a value a skill can have.", {}};
    *slot = std::min(value, practiceCap(*e, *s));
    if (today >= 0)
        e->practice->days[s->id] = {realClock ? realClock() : time_, today};
    return {true, s->name + " set to " + std::to_string(*slot) + ".", {}};
}
} // namespace ratw
