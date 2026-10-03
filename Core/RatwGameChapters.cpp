// Chapters as players found and live in them (Docs/Design/32-parties-chapters-factions.md, Part 3; the rules are
// RatwChapters.cpp): founding in a scene, invitations and ranks, the Chapter's two chats, its treasury (a real purse,
// doc 15), its hostile list, renown from what its members do (scenes, Stories), its levels, and what each member sees.
#include "RatwGame.h"

#include <algorithm>
#include <cmath>

namespace ratw::game
{
using json::Value;

namespace
{
std::string treasuryOf(const std::string& chapterId) { return "chapter:" + chapterId; }
} // namespace

void Game::tellChapter(const std::string& chapterId, const std::string& words, const std::string& except)
{
    if (const auto* c = chapters_.byId(chapterId))
        for (const auto& [m, member] : c->members)
            if (m != except)
                if (auto* cl = clientOf(m))
                    system(cl, words);
}

void Game::chapterAdvanced(const std::string& chapterId)
{
    for (const int level : chapters_.advance(chapterId, now()))
        if (const auto* c = chapters_.byId(chapterId))
            tellChapter(chapterId, c->name + " is now a " + chapter::levelName(level) + " (level " + std::to_string(level) + ").");
    chapterViewsDirty_ = true;
}

bool Game::chapterCommand(Connection* c, const Value& j, Result& result)
{
    const std::string id = c->entityId, verb = j.string("verb"), target = j.string("target");
    const double t = now();
    const auto done = [&](const chapter::Outcome& o, const std::string& said) {
        result = {o.ok, o.ok ? said : o.message, target};
        if (o.ok)
        {
            chapterViewsDirty_ = true;
            saveSoon();
        }
    };
    if (verb == "propose")
    {
        std::vector<std::string> founders;
        for (const auto& f : j.array("founders"))
            if (f.isString())
                founders.push_back(f.asString());
        founders.push_back(id);
        std::sort(founders.begin(), founders.end());
        founders.erase(std::unique(founders.begin(), founders.end()), founders.end());
        // Founded in a scene, together: one open scene with all three in it, each having taken part.
        std::string scene;
        for (const auto& [sid, s] : social_.sessions)
        {
            if (s.ended != 0)
                continue;
            bool all = true;
            for (const auto& f : founders)
            {
                const auto m = s.members.find(f);
                all = all && m != s.members.end() && m->second.turns >= 2;
            }
            if (all)
                scene = sid;
        }
        for (const auto& f : founders)
            if (social_.level(f) < options_.chapterFoundingLevel)
            {
                result = {false, "Each founder must be at social level " + std::to_string(options_.chapterFoundingLevel) + " or more.", {}};
                return true;
            }
        const auto* purse = world_.society().account(id);
        if (!purse || purse->cash < chapter::FoundingFee)
        {
            result = {false, "Founding a Chapter costs " + std::to_string(chapter::FoundingFee) + " pennies (two marks).", {}};
            return true;
        }
        const auto o = chapters_.propose(id, founders, mind::trim(j.string("name")), j.string("colour"), mind::trim(j.string("charter")), scene, t);
        if (o.ok)
            for (const auto& f : founders)
                if (f != id)
                    if (auto* cl = clientOf(f))
                        system(cl, names::capitalised(labelFor(f, id)) + " would found the Chapter \"" + mind::trim(j.string("name")) +
                                       "\" with you. Agree in the Chapter window.");
        done(o, "You propose the Chapter. When your fellow founders agree, it is founded.");
        return true;
    }
    if (verb == "agree")
    {
        const auto* p = chapters_.proposalFor(id, t);
        const auto proposal = p ? *p : chapter::Proposal{};
        // The proposer pays the fee when the last founder agrees; the scene must still be going.
        if (p && proposal.agreed.size() + 1 >= proposal.founders.size())
        {
            const auto scene = social_.sessions.find(proposal.scene);
            if (scene == social_.sessions.end() || scene->second.ended != 0)
            {
                chapters_.withdraw(id);
                result = {false, "The scene the Chapter was to be founded in has ended. Begin again in another.", {}};
                return true;
            }
            if (!world_.society().shift(proposal.by, "treasury", "", 0, chapter::FoundingFee, "chapter founding fee"))
            {
                result = {false, "The founder who proposed it can't pay the fee now.", {}};
                return true;
            }
            record(Economy, proposal.by);
        }
        const auto o = chapters_.agree(id, t);
        if (o.ok && !o.message.empty())
        {
            world_.society().openAccount(treasuryOf(o.message));
            for (const auto& f : proposal.founders)
                if (auto* cl = clientOf(f))
                    system(cl, "The Chapter \"" + proposal.name + "\" is founded. It is a Gathering (level I).");
        }
        done(o, o.message.empty() ? "You agree. The others still must." : "Founded.");
        return true;
    }
    if (verb == "withdraw")
        done(chapters_.withdraw(id), "The founding is called off.");
    else if (verb == "invite")
    {
        const auto* them = world_.entity(target);
        if (!them || them->npc || world_.visionClarity(id, target) <= 0)
        {
            result = {false, "You can only invite a player you can see.", target};
            return true;
        }
        const auto o = chapters_.invite(id, target, t);
        if (o.ok)
            if (auto* cl = clientOf(target))
                system(cl, names::capitalised(labelFor(target, id)) + " invites you to join the Chapter \"" + chapters_.byId(o.message)->name +
                               "\". Accept in the Chapter window.");
        done(o, "You invite " + labelFor(id, target) + " to the Chapter.");
    }
    else if (verb == "accept")
    {
        const auto o = chapters_.accept(id, t);
        if (o.ok)
            tellChapter(o.message, names::capitalised(nameOf(id)) + " joins the Chapter as an Initiate.", id);
        done(o, o.ok ? "You join " + chapters_.byId(o.message)->name + " as an Initiate." : "");
    }
    else if (verb == "decline")
        done(chapters_.decline(id), "You decline.");
    else if (verb == "leave")
    {
        const auto* mine = chapters_.of(id);
        const std::string chapterId = mine ? mine->id : std::string();
        const auto o = chapters_.leave(id, t);
        if (o.ok && o.message != "ended")
            tellChapter(chapterId, names::capitalised(nameOf(id)) + " leaves the Chapter." +
                                       (o.message.empty() ? "" : " " + nameOf(o.message) + " is its Head now."));
        done(o, o.message == "ended" ? "You were the last. The Chapter is no more." : "You leave the Chapter.");
    }
    else if (verb == "remove")
    {
        const auto* mine = chapters_.of(id);
        const std::string chapterId = mine ? mine->id : std::string();
        const auto o = chapters_.remove(id, target, t);
        if (o.ok)
        {
            factions_.expelled(chapterId, target, world_.calendarDays());   // Their burden leaves with them, once heard of.
            if (auto* cl = clientOf(target))
                system(cl, "You have been sent from the Chapter.");
            tellChapter(chapterId, names::capitalised(nameOf(target)) + " has been sent from the Chapter.", id);
        }
        done(o, "Sent away.");
    }
    else if (verb == "rank")
        done(chapters_.setRank(id, target, int(j.number("rank", chapter::RankInitiate)), t), "Rank set.");
    else if (verb == "rankname")
        done(chapters_.renameRank(id, int(j.number("rank", -1)), mind::trim(j.string("name"))), "Rank renamed.");
    else if (verb == "meet")
    {
        const auto* me = world_.entity(id);
        const auto* cell = me ? world_.cell(me->cellId) : nullptr;
        done(cell ? chapters_.setMeeting(id, me->cellId, me->position.x, me->position.y, cell->name) : chapter::Outcome{false, "Not here."},
             "The Chapter meets here now.");
        if (result.ok)
            tellChapter(chapters_.of(id)->id, "The Chapter's meeting place is now " + cell->name + ".", id);
    }
    else if (verb == "hostile" || verb == "unhostile")
    {
        // A wolf, another Chapter, or (with Phase 6) a faction.
        std::string kind = "wolf";
        if (chapters_.byId(target))
            kind = "chapter";
        else if (target.rfind("faction:", 0) == 0)
            kind = "faction";
        else if (!world_.entity(target) && !characters_.count(target))
        {
            result = {false, "There is no one to mark.", target};
            return true;
        }
        done(verb == "hostile" ? chapters_.markHostile(id, target, kind, mind::trim(j.string("reason")), t) : chapters_.unmarkHostile(id, target),
             verb == "hostile" ? "Marked hostile to the Chapter." : "No longer marked hostile.");
    }
    else if (verb == "deposit" || verb == "withdraw_money")
    {
        const auto* mine = chapters_.of(id);
        const auto* member = chapters_.member(id);
        const auto amount = std::int64_t(j.number("amount"));
        if (!mine || !member)
            result = {false, "You are not in a Chapter.", {}};
        else if (amount <= 0 || amount > 100000)
            result = {false, "Say how many pennies.", {}};
        else if (verb == "withdraw_money" && member->rank > chapter::RankOfficer)
            result = {false, "Only Officers and the Head draw on the treasury.", {}};
        else
        {
            // An Officer may draw up to 20 pennies a day; the Head, any amount.
            std::int64_t today = 0;
            for (const auto& e : mine->log)
                today += e.kind == "withdrawal" && e.by == id && t - e.at < 86400 ? e.amount : 0;
            if (verb == "withdraw_money" && member->rank == chapter::RankOfficer && today + amount > 20)
                result = {false, "An Officer draws at most 20 pennies a day.", {}};
            else
            {
                const bool in = verb == "deposit";
                const bool moved = world_.society().shift(in ? id : treasuryOf(mine->id), in ? treasuryOf(mine->id) : id, "", 0, amount,
                                                          in ? "chapter deposit" : "chapter withdrawal");
                if (moved)
                {
                    chapters_.log(mine->id, {in ? "deposit" : "withdrawal", id, "", "", amount, t});
                    record(Economy, id);
                    chapterViewsDirty_ = true;
                    saveSoon();
                }
                result = {moved, moved ? (in ? "Deposited." : "Drawn.") : (in ? "You haven't that much." : "The treasury hasn't that much."), {}};
            }
        }
    }
    else if (!estateCommand(c, j, result) && !campCommand(c, j, result) && !holdCommand(c, j, result))   // Ground (doc 32, Part 5).
        return false;
    return true;
}

void Game::chapterTick(double dt)
{
    chapterAccumulator_ += dt;
    if (chapterAccumulator_ < 5)
        return;
    chapterAccumulator_ = 0;
    for (const auto& id : chapters_.tick(now()))
        if (const auto* c = chapters_.byId(id))
            tellChapter(id, "With the Head away so long, the Chapter has a new Head.");
    std::vector<std::string> ids;
    for (const auto& [id, c] : chapters_.all())
        ids.push_back(id);
    for (const auto& id : ids)
        chapterAdvanced(id);
}

void Game::onSettled(const LedgerEntry& entry)
{
    // Renown from what members do (doc 32, 3.3), once a scene: a share of the best paid member's pay (half if only
    // Initiates took part), scaled down past ten active members; +5 if two or more members qualified together; +3 if
    // they did it with someone outside the Chapter.
    const auto paid = social_.paidIn(entry.session);
    if (paid.size() < 2)
        return;
    std::map<std::string, std::vector<std::string>> by;
    for (const auto& p : paid)
    {
        chapters_.touch(p, now());
        if (const auto* c = chapters_.of(p))
            by[c->id].push_back(p);
    }
    for (const auto& [chapterId, members] : by)
    {
        auto* c = chapters_.byId(chapterId);
        if (!c || c->scenesCounted.count(entry.session))
            continue;
        c->scenesCounted.insert(entry.session);
        if (c->scenesCounted.size() > 2000)
            c->scenesCounted.erase(c->scenesCounted.begin());
        int best = 0;
        bool onlyInitiates = true;
        for (const auto& m : members)
        {
            best = std::max(best, social_.paidFor(m, entry.session));
            onlyInitiates = onlyInitiates && c->members.at(m).rank == chapter::RankInitiate;
        }
        double share = best * 0.25 * (onlyInitiates ? 0.5 : 1.0);
        const int active = chapters_.activeMembers(*c, now());
        if (active > 10)
            share *= std::sqrt(10.0 / active);
        const auto actor = members.front();
        chapters_.addRenown(chapterId, "member scene", int(std::round(share)), entry.session, actor, now());
        if (members.size() >= 2)
        {
            chapters_.addRenown(chapterId, "chapter scene", 5, entry.session, actor, now());
            if (const auto s = social_.sessions.find(entry.session); s != social_.sessions.end())
                factionScene(s->second.cell, chapterId);
        }
        if (paid.size() > members.size())
            chapters_.addRenown(chapterId, "outreach", 3, entry.session, actor, now());
        chapterAdvanced(chapterId);
    }
}

void Game::markChapterStory(SocialStory& story)
{
    // A Chapter Story: most of those in it are one Chapter's.
    std::map<std::string, int> count;
    for (const auto& m : story.members)
        if (const auto* c = chapters_.of(m))
            ++count[c->id];
    for (const auto& [id, n] : count)
        if (n * 2 > int(story.members.size()))
            story.chapter = id;
}

void Game::onStoryClosed(const SocialStory& story)
{
    if (story.chapter.empty() || !chapters_.byId(story.chapter))
        return;
    const int amount = std::min(50, 20 + 5 * int(story.scenes.size()));
    chapters_.addRenown(story.chapter, "story", amount, story.id, story.owner, now());
    chapters_.byId(story.chapter)->storiesTold += 1;
    tellChapter(story.chapter, "The Chapter Story \"" + story.name + "\" is told. +" + std::to_string(amount) + " renown.");
    chapterAdvanced(story.chapter);
}

void Game::refreshChapterViews(double dt)
{
    chapterViewsAccumulator_ += dt;
    if (!chapterViewsDirty_ && chapterViewsAccumulator_ < 2)
        return;
    chapterViewsAccumulator_ = 0;
    chapterViewsDirty_ = false;
    chapterViews_.clear();
    const double t = now();
    for (const auto* cl : clients_)
    {
        const auto& id = cl->entityId;
        if (id.empty())
            continue;
        auto v = Value::object();
        bool any = false;
        if (const auto* c = chapters_.of(id))
        {
            any = true;
            const auto& me = c->members.at(id);
            v.add("id", c->id);
            v.add("name", c->name);
            v.add("colour", c->colour);
            v.add("charter", c->charter);
            v.add("level", c->level);
            v.add("levelName", chapter::levelName(c->level));
            v.add("renown", c->renown);
            v.add("rank", me.rank);
            auto ranks = Value::array();
            for (const auto& r : c->rankNames)
                ranks.push(r);
            v.add("rankNames", ranks);
            if (c->level < 5)
            {
                const auto& g = chapter::gateFor(c->level + 1);
                auto next = Value::object();
                next.add("level", c->level + 1);
                next.add("name", chapter::levelName(c->level + 1));
                next.add("renown", g.renown);
                next.add("active", g.active);
                next.add("stories", g.stories);
                next.add("ground", g.ground);
                next.add("haveActive", chapters_.activeMembers(*c, t));
                next.add("haveStories", c->storiesTold);
                v.add("next", next);
            }
            auto members = Value::array();
            for (const auto& [m, mm] : c->members)
            {
                auto o = Value::object();
                o.add("id", m);
                o.add("name", names::capitalised(labelFor(id, m)));
                o.add("rank", mm.rank);
                o.add("online", clientOf(m) != nullptr);
                o.add("active", t - mm.active < chapter::ActiveSeconds);
                members.push(o);
            }
            v.add("members", members);
            if (!c->meetingCell.empty())
            {
                auto meet = Value::object();
                meet.add("cell", c->meetingCell);
                meet.add("name", c->meetingName);
                meet.add("x", c->meetingX);
                meet.add("y", c->meetingY);
                v.add("meeting", meet);
            }
            auto hostiles = Value::array();
            for (const auto& h : c->hostiles)
            {
                auto o = Value::object();
                o.add("target", h.target);
                o.add("kind", h.kind);
                o.add("name", h.kind == "chapter" && chapters_.byId(h.target) ? chapters_.byId(h.target)->name
                                                                               : names::capitalised(labelFor(id, h.target)));
                o.add("reason", h.reason);
                hostiles.push(o);
            }
            v.add("hostiles", hostiles);
            if (const auto* purse = world_.society().account(treasuryOf(c->id)))
                v.add("treasury", double(purse->cash));
            auto renown = Value::array();
            for (std::size_t i = c->renownLog.size() > 8 ? c->renownLog.size() - 8 : 0; i < c->renownLog.size(); ++i)
            {
                auto o = Value::object();
                o.add("kind", c->renownLog[i].kind);
                o.add("amount", c->renownLog[i].amount);
                renown.push(o);
            }
            v.add("renownLog", renown);
            v.add("standings", standingsView(c->id));     // Bands only (doc 32, 4.2b).
            v.add("leases", leasesView(c->id));           // Its rented places (doc 32, 5.2).
            v.add("sites", sitesView(c->id));             // Its own ground (doc 32, 5.3).
            v.add("hold", holdView(c->id, id));           // Treaties, levies, its claim, a House, the sworn (Phase 9).
        }
        if (const auto* inv = chapters_.inviteFor(id, t))
            if (const auto* c = chapters_.byId(inv->first))
            {
                any = true;
                v.add("invite", c->name);
            }
        if (const auto* p = chapters_.proposalFor(id, t))
        {
            any = true;
            auto o = Value::object();
            o.add("name", p->name);
            o.add("colour", p->colour);
            o.add("mine", p->by == id);
            o.add("agreed", p->agreed.count(id) > 0);
            auto founders = Value::array();
            for (const auto& f : p->founders)
            {
                auto k = Value::object();
                k.add("name", names::capitalised(labelFor(id, f)));
                k.add("agreed", p->agreed.count(f) > 0);
                founders.push(k);
            }
            o.add("founders", founders);
            v.add("proposal", o);
        }
        if (any)
            chapterViews_[id] = v;
    }
}
} // namespace ratw::game
