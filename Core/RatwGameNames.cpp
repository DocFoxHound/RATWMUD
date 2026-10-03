// Names and introductions as players meet them (Docs/Design/32-parties-chapters-factions.md, 1.5; the rules are
// RatwNames.cpp): what each wolf is called by each viewer, introductions heard in speech, NPCs giving their names when
// they are willing, a player's aliases, and the game's own messages with unknown names replaced by how the wolf looks.
#include "RatwGame.h"

#include <algorithm>
#include <cmath>

namespace ratw::game
{
using json::Value;

std::vector<std::string> Game::namesOf(const std::string& id) const
{
    std::vector<std::string> out;
    const auto* e = world_.entity(id);
    if (!e)
        return out;
    out.push_back(e->name);
    if (const auto found = aliases_.find(id); found != aliases_.end())
        out.insert(out.end(), found->second.begin(), found->second.end());
    return out;
}

bool Game::knowsName(const std::string& viewer, const std::string& id) const
{
    if (!options_.hiddenNames || viewer == id)
        return true;
    if (const auto* e = world_.entity(id); e && e->transient)
        return true;                                  // Folk of the road go by what they are ("Bandit").
    return known_.knows(viewer, id);
}

std::string Game::strangerLabel(const std::string& id) const
{
    // Worked out once a second (refreshLabels), so views built in parallel only read them.
    if (const auto found = labels_.find(id); found != labels_.end())
        return found->second;
    return lookOf(id, nullptr);
}

void Game::refreshLabels(double dt)
{
    if (!options_.hiddenNames || (labelsAccumulator_ += dt) < 1)
        return;
    labelsAccumulator_ = 0;
    // How many hold each trade in each community: "the innkeeper" where there is one, "a guard" where there are several.
    std::map<std::string, int> trades;
    for (const auto& p : world_.society().positions())
        ++trades[world_.communityOf(p.work.cell) + "|" + p.title];
    labels_.clear();
    for (const auto& [id, e] : world_.entities())
        labels_[id] = lookOf(id, &trades);
}

std::string Game::lookOf(const std::string& id, const std::map<std::string, int>* trades) const
{
    const auto* e = world_.entity(id);
    if (!e)
        return "someone";
    // A resident is known by their trade: "the innkeeper"; "a guard" where there are several.
    if (e->npc)
        if (const auto* job = world_.society().jobOf(id); job && !job->title.empty())
        {
            std::string title = job->title;
            for (auto& ch : title)
                ch = char(std::tolower(static_cast<unsigned char>(ch)));
            const auto community = world_.communityOf(job->work.cell);
            int same = 0;
            if (trades)
            {
                const auto found = trades->find(community + "|" + job->title);
                same = found == trades->end() ? 0 : found->second;
            }
            else
                for (const auto& p : world_.society().positions())
                    if (p.title == job->title && world_.communityOf(p.work.cell) == community && ++same > 1)
                        break;
            return same > 1 || job->role == "guard" ? names::article(title) + " " + title : "the " + title;
        }
    return names::describe(e->appearance, e->age);
}

std::string Game::labelFor(const std::string& viewer, const std::string& id) const
{
    if (knowsName(viewer, id))
    {
        if (viewer != id && options_.hiddenNames)
            if (const auto name = known_.nameFor(viewer, id); !name.empty())
                return name;
        return nameOf(id);
    }
    return strangerLabel(id);
}

std::string Game::veilFor(const std::string& viewer, const std::string& text) const
{
    if (!options_.hiddenNames || viewer.empty() || text.empty())
        return text;
    return names::veil(text, veilMap(viewer));
}

std::map<std::string, std::string> Game::veilMap(const std::string& viewer) const
{
    std::map<std::string, std::string> labels;
    if (!options_.hiddenNames || viewer.empty())
        return labels;
    const auto* me = world_.entity(viewer);
    const auto consider = [&](const std::string& id) {
        if (id == viewer)
            return;
        if (const auto* e = world_.entity(id); e && !e->name.empty() && !knowsName(viewer, id))
            labels.emplace(e->name, strangerLabel(id));
        else if (e && !e->name.empty() && options_.hiddenNames)
            if (const auto name = known_.nameFor(viewer, id); !name.empty() && name != e->name)
                labels.emplace(e->name, name);        // Known only by an alias: the alias.
    };
    if (me)
        for (const Entity* e : world_.entitiesIn(me->cellId))
            consider(e->id);
    for (const auto& m : parties_.mates(viewer))
        consider(m);
    for (const auto* c : clients_)
        consider(c->entityId);
    return labels;
}

bool Game::willName(const std::string& npcId, const std::string& playerId) const
{
    const auto* npc = world_.entity(npcId);
    if (!npc || npc->transient || world_.hostile(npcId))
        return false;
    const auto* bond = world_.bonds().find(npcId, playerId);
    return !bond || bond->trust >= -20;
}

bool Game::learnName(const std::string& knower, const std::string& known, const std::string& name, const std::string& how)
{
    if (!options_.hiddenNames || !known_.learn(knower, known, name, how, world_.calendarDays()))
        return false;
    saveSoon();
    return true;
}

void Game::noticeIntroduction(const std::string& author, const std::string& listener, const std::string& heard)
{
    if (!options_.hiddenNames || author == listener || heard.empty())
        return;
    const auto* a = world_.entity(author);
    if (!a)
        return;
    if (a->npc && !willName(author, listener))
        return;
    const auto name = names::introducedName(heard, namesOf(author));
    if (name.empty())
        return;
    const std::string before = labelFor(listener, author);
    if (!learnName(listener, author, name, "introduced"))
        return;
    introducedTo_[author].push_back(listener);
    const auto* l = world_.entity(listener);
    if (l && !l->npc)
        if (auto* c = clientOf(listener))
            system(c, names::capitalised(before) + " is " + name + ".");
    // A friendly resident told a name gives theirs back, when they next speak to them.
    if (l && l->npc && !a->npc && !known_.knows(author, listener))
        if (const auto* bond = world_.bonds().find(listener, author); (!bond || bond->affinity >= 0) && willName(listener, author))
            owedName_.insert(listener + "|" + author);
}

void Game::sendIntroductionReceipt(const std::string& author)
{
    const auto found = introducedTo_.find(author);
    if (found == introducedTo_.end())
        return;
    const auto who = std::move(found->second);
    introducedTo_.erase(found);
    auto* c = clientOf(author);
    if (!c || who.empty())
        return;
    const auto name = known_.nameFor(who.front(), author);
    std::string list;
    for (std::size_t i = 0; i < who.size() && i < 6; ++i)
        list += (i == 0 ? "" : i + 1 == who.size() || i == 5 ? " and " : ", ") + labelFor(author, who[i]);
    if (who.size() > 6)
        list += " and " + std::to_string(who.size() - 6) + " more";
    system(c, "You introduced yourself as " + name + " to " + list + ".");
}

void Game::npcSpokeTo(const std::string& npcId, const std::string& playerId)
{
    if (!options_.hiddenNames || playerId.empty() || known_.knows(playerId, npcId) || !willName(npcId, playerId))
    {
        owedName_.erase(npcId + "|" + playerId);
        return;
    }
    const auto* bond = world_.bonds().find(npcId, playerId);
    const bool owed = owedName_.erase(npcId + "|" + playerId) > 0;
    // Told the player's name and friendly; or seen them about for long enough that names come naturally.
    if (!owed && !(bond && bond->familiarity >= FamiliarEnough))
        return;
    const std::string before = strangerLabel(npcId);
    if (const auto* npc = world_.entity(npcId); npc && learnName(playerId, npcId, npc->name, owed ? "introduced" : "familiar"))
        if (auto* c = clientOf(playerId))
            system(c, names::capitalised(before) + " gives you their name: " + npc->name + ".");
}

bool Game::namesCommand(Connection* c, const Value& j, Result& result)
{
    const std::string id = c->entityId, verb = j.string("verb"), name = mind::trim(j.string("name"));
    auto& mine = aliases_[id];
    const auto* me = world_.entity(id);
    if (!me)
        return false;
    if (verb == "add")
    {
        const auto problem = names::aliasProblem(name, me->name, mine);
        result = {problem.empty(), problem.empty() ? "You may now go by " + name + " as well." : problem, {}};
        if (problem.empty())
            mine.push_back(name);
    }
    else if (verb == "retire")
    {
        const auto at = std::find(mine.begin(), mine.end(), name);
        result = {at != mine.end(), at != mine.end() ? "You no longer go by " + name + ". Those who know you by it still do." :
                                                       "You don't go by that name.", {}};
        if (at != mine.end())
            mine.erase(at);
    }
    else
        return false;
    if (mine.empty())
        aliases_.erase(id);
    if (result.ok)
        saveSoon();
    return true;
}

void Game::seedAcquaintances()
{
    // Before introductions there were no hidden names: those already well acquainted with a player keep each other's.
    const auto seed = [&](const std::string& holder) {
        if (const auto* bonds = world_.bonds().of(holder))
            for (const auto& [other, bond] : *bonds)
            {
                const auto* a = world_.entity(holder);
                const auto* b = world_.entity(other);
                const bool aPlayer = characters_.count(holder) > 0 || (a && !a->npc);
                const bool bPlayer = characters_.count(other) > 0 || (b && !b->npc);
                if (bond.familiarity < FamiliarEnough / 2 || (!aPlayer && !bPlayer))
                    continue;
                const auto nameA = a ? a->name : characters_.count(holder) ? characters_.at(holder).name : std::string();
                const auto nameB = b ? b->name : characters_.count(other) ? characters_.at(other).name : std::string();
                known_.learn(holder, other, nameB, "familiar", world_.calendarDays());
                known_.learn(other, holder, nameA, "familiar", world_.calendarDays());
            }
    };
    for (const auto& [id, e] : world_.entities())
        seed(id);
    for (const auto& [id, e] : characters_)
        seed(id);
}

Value Game::namesView(const std::string& id) const
{
    auto v = Value::object();
    if (const auto* e = world_.entity(id))
        v.add("name", e->name);
    auto list = Value::array();
    if (const auto found = aliases_.find(id); found != aliases_.end())
        for (const auto& a : found->second)
            list.push(a);
    v.add("aliases", list);
    v.add("hidden", options_.hiddenNames);
    return v;
}
} // namespace ratw::game
