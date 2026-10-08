// Training grounds and sparring (Docs/Design/53-hunting-and-working-together.md, 5; Phase 5). A spar is a duel's fourth
// term: it ends at yield, nothing bleeds or burns, a blade strikes blunted and a hard blow leaves a minor bruise at worst
// (the hooks are one line each in RatwBattle.cpp and RatwInjury.cpp). Training grounds are data
// (Data/Together/training.json) over the ground that is there; on one, a resident trainer spars with a player who asks,
// and a wolf with no partner practises at the post. Sparring teaches fighting by partner and ground.
#include "RatwJsonDoc.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace ratw
{
namespace
{
struct TrainingRules
{
    std::vector<std::string> regions{"training_grounds"}, cells, cellWords;
    double post = .3, ground = 1.5, postSeconds = 20, bladeBlunted = .5;
};

std::filesystem::path trainingFile()
{
    // Data/Together: RATW_DATA_DIR, else the working directory or one above it, else the source tree.
    namespace fs = std::filesystem;
    std::error_code ec;
    if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
        return fs::path(dir) / "Together" / "training.json";
    for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
    {
        if (fs::exists(at / "Data" / "Together" / "training.json", ec))
            return at / "Data" / "Together" / "training.json";
        if (at == at.parent_path())
            break;
    }
#ifdef RATW_SOURCE_DIR
    return fs::path(RATW_SOURCE_DIR) / "Data" / "Together" / "training.json";
#else
    return fs::path("Data") / "Together" / "training.json";
#endif
}

const TrainingRules& training()
{
    static const TrainingRules rules = [] {
        TrainingRules r;
        std::ifstream in(trainingFile());
        std::stringstream text;
        text << in.rdbuf();
        json::Value doc;
        std::string error;
        if (!in || !json::parse(text.str(), doc, error) || !doc.isObject())
        {
            std::cerr << "[warn] RATW_TRAINING no Data/Together/training.json: only the region training_grounds trains\n";
            return r;
        }
        const auto strings = [&](const char* key, std::vector<std::string>& out) {
            if (const auto* list = doc.find(key); list && list->isArray())
            {
                out.clear();
                for (const auto& v : list->items())
                    out.push_back(v.asString());
            }
        };
        strings("regions", r.regions);
        strings("cells", r.cells);
        strings("cellWords", r.cellWords);
        r.post = doc.number("post", r.post);
        r.ground = doc.number("ground", r.ground);
        r.postSeconds = doc.number("postSeconds", r.postSeconds);
        r.bladeBlunted = doc.number("bladeBlunted", r.bladeBlunted);
        return r;
    }();
    return rules;
}
} // namespace

bool World::trainingGround(const std::string& cellId) const
{
    const auto& r = training();
    const auto* c = cell(cellId);
    if (!c)
        return false;
    if (std::find(r.regions.begin(), r.regions.end(), c->region) != r.regions.end() ||
        std::find(r.cells.begin(), r.cells.end(), cellId) != r.cells.end())
        return true;
    for (const auto& word : r.cellWords)
        if (!word.empty() && cellId.find(word) != std::string::npos)
            return true;
    return false;
}

bool World::trainer(const std::string& id) const
{
    // A resident on a training ground who is a guard on duty there, or whose post trains or drills.
    const auto* e = entity(id);
    if (!e || !e->npc || e->dead || e->downedLeft > 0 || !trainingGround(e->cellId) || e->age < battle::YoungestFighter)
        return false;
    if (guardOnDuty(id))
        return true;
    const auto* job = society_.jobOf(id);
    return job && (job->title.find("train") != std::string::npos || job->title.find("drill") != std::string::npos);
}

Result World::sparWithTrainer(const std::string& player, const std::string& trainerId)
{
    const auto* p = entity(player);
    const auto* t = entity(trainerId);
    if (!p || p->npc || p->dead || !t)
        return {false, "Spar with whom?", {}};
    if (p->downedLeft > 0)
        return {false, "You are down.", {}};
    if (!trainer(trainerId))
        return {false, t->name + " isn't one to spar with here.", trainerId};
    if (inBattle(player) || inBattle(trainerId))
        return {false, inBattle(player) ? "You are already in a fight." : t->name + " is busy in the ring.", trainerId};
    if (const auto why = tooLoadedToFight(player); !why.empty())
        return {false, why, trainerId};
    if (p->age < battle::YoungestFighter)
        return {false, "Not one so young.", trainerId};
    if (p->cellId != t->cellId || std::hypot(p->position.x - t->position.x, p->position.y - t->position.y) > battle::StartReach)
        return {false, "Get closer first.", trainerId};
    if (const auto settle = settleUntil_.find(player); settle != settleUntil_.end() && time_ < settle->second)
        return {false, "You are still finding your feet after that fight.", trainerId};
    auto started = startBattle(player, trainerId, false, "spar");
    if (started.ok)
        started.message = t->name + " nods and squares up: a spar, bruises only.";
    return started;
}

Result World::tourneyBout(const std::string& a, const std::string& b, const Spot& at)
{
    auto* ea = entity(a);
    auto* eb = entity(b);
    if (!ea || !eb || ea->dead || eb->dead || ea->downedLeft > 0 || eb->downedLeft > 0)
        return {false, "Not fit to fight.", {}};
    if (inBattle(a) || inBattle(b))
        return {false, "Already in a fight.", {}};
    for (auto* e : {ea, eb})
    {
        e->cellId = at.cell;
        e->position = {at.x + (e == ea ? 0. : 1.), at.y};   // (Side by side: a timid one never closes in.)
        e->path.clear();
        e->velocity = {};
    }
    // A player starts it (as with a trainer's spar); two residents, the first of them.
    const bool swap = ea->npc && !eb->npc;
    return startBattle(swap ? b : a, swap ? a : b, !ea->npc && !eb->npc, "spar");
}

Result World::practiseAtPost(const std::string& player)
{
    const auto* p = entity(player);
    if (!p || p->npc || p->dead)
        return {false, "No such character.", {}};
    if (!trainingGround(p->cellId))
        return {false, "There is no practice post here; go to a training ground.", {}};
    if (inBattle(player) || p->downedLeft > 0)
        return {false, "Not now.", {}};
    if (const auto next = postNext_.find(player); next != postNext_.end() && time_ < next->second)
        return {false, "You are still at it.", {}};
    const auto& r = training();
    postNext_[player] = time_ + r.postSeconds;
    PracticeContext context;
    context.occasion = "post:" + std::to_string(std::int64_t(calendarDays_));
    context.amount = r.post * r.ground;
    practise(player, "spar.post", context);
    return {true, "You work at the post: bites, feints and footwork.", {}};
}

double World::sparBlow(const Battle& b, double damage, const std::string& by) const
{
    if (b.terms != "spar" || by.empty())
        return damage;
    const auto* striker = entity(by);
    return striker && striker->mouth == "sword" ? damage * training().bladeBlunted : damage;   // Blunted blades.
}

double World::sparPractice(const Battle& b, const BattleFighter* foe) const
{
    // A trainer teaches less than a player already by the practice engine's own weight for a resident partner (doc 49):
    // nothing more here (the user, 2026-10-08: the two don't stack). Only the ground counts.
    (void)foe;
    if (b.terms != "spar")
        return 1;
    return trainingGround(b.cellId) ? training().ground : 1.;
}
} // namespace ratw
