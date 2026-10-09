// Player storytellers (Docs/Design/58-player-storytellers.md, 4-7). Not the DM app: an account at social level 5, with
// no upheld report in 30 days, applies; a Dungeon Master approves, refuses or revokes. An approved storyteller writes tales
// (storylines of kind "tale", RatwStorylines.h) and runs them for its party, Chapter, circles and friends (who opt in),
// or strangers it admits from a board's call: narration, story characters' lines, open dice, visitors from the DM's list,
// prizes from its own purse or its Chapter's treasury, and an end card where each participant stars it as a Storyteller.
// Every command is checked here; narration and story characters' lines are kept 30 days for the DM, then cleared. A
// milestone of a world story ends with a credits screen built from the ledger (the DM host), up to 3 stars each.
#include "RatwGame.h"

#include "RatwItems.h"
#include "RatwWire.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <regex>

namespace ratw::game
{
using json::Value;

namespace
{
std::string lowered(std::string s)
{
    for (auto& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
bool plainText(const std::string& s)
{
    return std::none_of(s.begin(), s.end(), [](char c) { return static_cast<unsigned char>(c) < 32; });
}
const std::set<std::string> ObjectiveKinds{"told", "place", "talk", "scene", "contract", "hunt", "fight", "gift", "deliver"};
} // namespace

// ------------------------------------------------------------------ Phase 3: becoming a storyteller

bool Game::approvedStoryteller(const std::string& account) const
{
    const auto s = storytellers_.find(account);
    return s != storytellers_.end() && s->second.state == "approved";
}

std::string Game::whyNotApply(const std::string& character) const
{
    const auto& r = storylines::rules();
    const auto account = accountKey(character);
    if (account.empty())
        return "No account.";
    if (const auto s = storytellers_.find(account); s != storytellers_.end() && (s->second.state == "applied" || s->second.state == "approved"))
        return s->second.state == "applied" ? "Your application is with the Dungeon Masters." : "You tell stories already.";
    if (socialLevel(character) < r.applyLevel)
        return "Storytellers are wolves of social level " + std::to_string(r.applyLevel) + " or more.";
    if (upheldReportsWithin(account, r.reportDays) > 0)
        return "Not with a report upheld against you in the last " + std::to_string(r.reportDays) + " days.";
    return {};
}

void Game::storyLogAdd(const std::string& character, const std::string& storyline, const std::string& kind, const std::string& target,
                       const std::string& text, const std::string& detail)
{
    StoryLogEntry e;
    e.id = "slog-" + std::to_string(storyLogNext_++);
    e.account = accountKey(character);
    e.character = character;
    e.storyline = storyline;
    e.kind = kind;
    e.target = target.substr(0, 80);
    e.text = text.substr(0, 1000);
    e.detail = detail.substr(0, 200);
    e.at = now();
    storyLog_.push_back(std::move(e));
    if (storyLog_.size() > 5000)
        storyLog_.erase(storyLog_.begin());
}

void Game::decideStoryteller(const std::string& account, bool approve, const std::string& by, const std::string& reason)
{
    auto& s = storytellers_[account];
    s.state = approve ? "approved" : "refused";
    s.decidedBy = by;
    s.decidedAt = now();
    s.reason = reason.substr(0, 400);
    for (auto* c : clients_)
        if (accountKey(c) == account)
            storylineToast(c, approve ? "The Dungeon Masters have made you a storyteller: the STORYTELLER button is yours."
                                      : "Your application to tell stories was refused" + (reason.empty() ? std::string(".") : ": " + reason));
    saveSoon();
}

void Game::revokeStoryteller(const std::string& account, const std::string& by, const std::string& reason)
{
    // Revoked: its running tales pause, their participants are told; the DM may stop them.
    auto& s = storytellers_[account];
    s.state = "revoked";
    s.decidedBy = by;
    s.decidedAt = now();
    s.reason = reason.substr(0, 400);
    for (const auto& [id, tale] : storylines_.all())
        if (tale.kind == "tale" && tale.authorAccount == account && tale.state == "running")
        {
            if (auto* t = storylines_.find(id))
                t->state = "paused";
            for (const auto& [who, p] : tale.participants)
                if (auto* c = clientOf(who); c && p.active())
                    storylineToast(c, "\"" + tale.title + "\" is paused.");
        }
    storylines_.touch();
    for (auto* c : clients_)
        if (accountKey(c) == account)
            storylineToast(c, "You are no longer a storyteller" + (reason.empty() ? std::string(".") : ": " + reason));
    saveSoon();
}

// ------------------------------------------------------------------ Phase 4: tales

bool Game::castNameTaken(const std::string& name) const
{
    // Never a real wolf's name: any resident's or player's, or a player's alias.
    const auto want = lowered(name);
    for (const auto& [id, e] : world_.entities())
        if (lowered(e.name) == want)
            return true;
    for (const auto& [id, e] : characters_)
        if (lowered(e.name) == want)
            return true;
    for (const auto& [id, list] : aliases_)
        for (const auto& a : list)
            if (lowered(a) == want)
                return true;
    for (const auto& r : world_.society().authored().residents)
        if (lowered(r.name) == want)
            return true;
    return false;
}

bool Game::inTaleScope(const std::string& teller, const std::string& who, const storylines::Storyline& tale) const
{
    // The storyteller's party, Chapter, circles and friends, or a stranger it admitted for this tale; never one blocked.
    if (who == teller || blocked(teller, who))
        return false;
    if (tale.admitted.count(who) || parties_.together(teller, who) || areFriends(teller, who))
        return true;
    if (const auto* a = chapters_.of(teller); a && chapters_.of(who) == a)
        return true;
    const auto x = accountKey(teller), y = accountKey(who);
    for (const auto& [id, circle] : circles_)
        if (circle.members.count(x) && circle.members.count(y))
            return true;
    return false;
}

storylines::Storyline* Game::taleFor(const std::string& character, const std::string& id, bool mine)
{
    // A tale by id, or the first running one it tells (mine) or takes part in.
    if (auto* t = storylines_.find(id); t && t->kind == "tale")
        return (!mine || t->authorCharacter == character) ? t : nullptr;
    if (!id.empty())
        return nullptr;
    for (const auto& [tid, t] : storylines_.all())
        if (t.kind == "tale" && t.state == "running" && (mine ? t.authorCharacter == character : t.takesPart(character)))
            return storylines_.find(tid);
    return nullptr;
}

void Game::deliverStory(const storylines::Storyline& tale, const std::string& teller, const std::string& kind, const std::string& name,
                        const std::string& text)
{
    // To its participants in the storyteller's place, and to wolves in speaking range while its scene is Open or Knock
    // (doc 51's openness); nobody else. Marked as the storyteller's, by the label each reader knows.
    const auto* t = world_.entity(teller);
    if (!t)
        return;
    std::string openness = "knock";
    for (const auto& [sid, s] : social_.sessions)
        if (s.ended == 0 && s.members.count(teller) && !s.openness.empty())
            openness = s.openness;
    for (auto* c : clients_)
    {
        const auto* e = world_.entity(c->entityId);
        if (!e || e->cellId != t->cellId)
            continue;
        const bool part = tale.takesPart(c->entityId) || c->entityId == teller;
        if (!part && (openness == "private" || world_.hearingClarity(c->entityId, teller, Voice::Speak) <= 0))
            continue;
        const auto by = c->entityId == teller ? std::string("you") : labelFor(c->entityId, teller);
        auto ev = Value::object();
        ev.add("type", "story");
        ev.add("kind", kind);
        ev.add("tale", tale.id);
        ev.add("title", tale.title);
        ev.add("speaker", kind == "npc" ? "✦ " + name + " (in " + by + "'s story)" : "✦ STORY · " + tale.title + " · " + by);
        ev.add("text", text);
        ev.add("channel", "ic");
        send(c, ev);
    }
}

std::string Game::rollDice(const std::string& spec, const std::string& seed, std::string& error)
{
    // "2d6+1": 1-10 dice of 2, 4, 6, 8, 10, 12, 20 or 100 sides, a bonus of -20..20; rolled here, seeded by the event.
    static const std::regex form(R"(^\s*(\d{1,2})\s*d\s*(\d{1,3})\s*(?:([+-])\s*(\d{1,2}))?\s*$)");
    std::smatch m;
    const auto& r = storylines::rules();
    if (!std::regex_match(spec, m, form))
    {
        error = "Roll like 2d6+1.";
        return {};
    }
    const int n = std::stoi(m[1]), sides = std::stoi(m[2]), bonus = m[3].matched ? (m[3] == "-" ? -1 : 1) * std::stoi(m[4]) : 0;
    static const std::set<int> Sides{2, 4, 6, 8, 10, 12, 20, 100};
    if (n < 1 || n > r.diceMost || !Sides.count(sides) || std::abs(bonus) > r.diceBonus)
    {
        error = "1 to " + std::to_string(r.diceMost) + " dice of 2, 4, 6, 8, 10, 12, 20 or 100 sides, ±" + std::to_string(r.diceBonus) + " at most.";
        return {};
    }
    std::uint64_t h = std::hash<std::string>{}(seed);
    int total = bonus;
    std::string parts;
    for (int i = 0; i < n; ++i)
    {
        h ^= h >> 33, h *= 0xff51afd7ed558ccdULL, h ^= h >> 33, h *= 0xc4ceb9fe1a85ec53ULL, h ^= h >> 33;
        const int face = int(h % std::uint64_t(sides)) + 1;
        total += face;
        parts += (i ? " + " : "") + std::to_string(face);
    }
    if (bonus)
        parts += (bonus > 0 ? " + " : " - ") + std::to_string(std::abs(bonus));
    return std::to_string(n) + "d" + std::to_string(sides) + (bonus ? (bonus > 0 ? " + " : " - ") + std::to_string(std::abs(bonus)) : "") + ": " +
           parts + " = " + std::to_string(total);
}

Result Game::saveTale(const std::string& teller, const Value& j)
{
    // A draft written or rewritten: 1-10 steps, objectives of the known kinds, a cast of up to 6 whose names are no real
    // wolf's. Only a draft is rewritten.
    const auto& r = storylines::rules();
    storylines::Storyline tale;
    if (auto* old = storylines_.find(j.string("tale")))
    {
        if (old->kind != "tale" || old->authorCharacter != teller || old->state != "draft")
            return {false, "Only your own draft can be rewritten.", {}};
        tale = *old;
        tale.steps.clear();
        tale.cast.clear();
    }
    else
    {
        tale.kind = "tale";
        tale.state = "draft";
        tale.source = "storyteller";
        tale.authorCharacter = teller;
        tale.authorAccount = accountKey(teller);
        tale.began = now();
    }
    tale.title = j.string("title");
    tale.premise = j.string("premise");
    if (tale.title.empty() || int(tale.title.size()) > r.title || !plainText(tale.title) || int(tale.premise.size()) > r.premise)
        return {false, "A tale needs a title (" + std::to_string(r.title) + " letters at most), and a premise of " + std::to_string(r.premise) + " at most.", {}};
    if (j.boolean("chapter"))
        if (const auto* ch = chapters_.of(teller))
            tale.chapter = ch->id;
    const auto& steps = j.array("steps");
    if (int(steps.size()) < r.taleSteps[0] || int(steps.size()) > r.taleSteps[1])
        return {false, "A tale has " + std::to_string(r.taleSteps[0]) + " to " + std::to_string(r.taleSteps[1]) + " steps.", {}};
    for (const auto& s : steps)
    {
        storylines::Step step;
        step.title = s.string("title");
        step.text = s.string("text");
        step.distinct = s.boolean("distinct");
        if (step.title.empty() || int(step.title.size()) > r.stepTitle || int(step.text.size()) > r.stepText)
            return {false, "Each step needs a title (" + std::to_string(r.stepTitle) + " letters at most) and a text of " + std::to_string(r.stepText) + " at most.", {}};
        if (const auto& m = s.object("marker"); m.isObject() && world_.cell(m.string("cell")))
            step.marker = {m.string("cell"), m.string("label", world_.cell(m.string("cell"))->name).substr(0, 60), m.number("x", 8), m.number("y", 8), 0};
        for (const auto& o : s.array("objectives"))
        {
            storylines::Objective obj;
            obj.kind = o.string("kind", "told");
            obj.target = o.string("target").substr(0, 80);
            obj.cell = o.string("cell").substr(0, 80);
            obj.x = o.number("x", -1), obj.y = o.number("y", -1), obj.radius = std::clamp(o.number("radius", 0), 0.0, 16.0);
            obj.count = std::clamp(int(o.number("count", 2)), 1, 12);
            obj.line = o.string("line", step.title).substr(0, 160);
            obj.suits = o.string("suits").substr(0, 20);
            if (!ObjectiveKinds.count(obj.kind))
                return {false, "Objectives are told, place, talk, scene, contract, hunt, fight, gift or deliver.", {}};
            if (step.objectives.size() < 3)
                step.objectives.push_back(std::move(obj));
        }
        if (step.objectives.empty())
            step.objectives.push_back({"told", {}, {}, {}, -1, -1, 0, 2, step.title});
        tale.steps.push_back(std::move(step));
    }
    for (const auto& c : j.array("cast"))
    {
        const auto name = c.string("name");
        if (name.empty())
            continue;
        if (int(name.size()) > r.castName || !plainText(name) || int(tale.cast.size()) >= r.cast)
            return {false, "A cast of " + std::to_string(r.cast) + " at most, each name " + std::to_string(r.castName) + " letters at most.", {}};
        if (castNameTaken(name))
            return {false, "\"" + name + "\" is a real wolf's name: story characters never are.", {}};
        tale.cast.push_back({name, c.string("looks").substr(0, 160)});
    }
    tale.lastActivity = now();
    auto& kept = storylines_.add(std::move(tale));
    storyLogAdd(teller, kept.id, "draft", {}, {}, kept.title);
    saveSoon();
    return {true, "Saved: " + kept.title + ".", kept.id};
}

bool Game::storytellerCommand(Connection* c, const Value& j, Result& result)
{
    const auto id = c ? c->entityId : std::string();
    const auto* me = world_.entity(id);
    const auto verb = j.string("verb");
    const auto account = accountKey(id);
    const auto& r = storylines::rules();
    if (!me || me->npc)
        return false;
    // Participants' verbs need no standing.
    if (verb == "accept" || verb == "decline" || verb == "ask" || verb == "roll" || verb == "star")
    {
        auto* tale = verb == "star" ? nullptr : taleFor(id, j.string("tale"), false);
        if (verb == "star")
            result = starCredit(id, j.string("credits"), j.string("to"));
        else if (verb == "ask")
        {
            auto* t = storylines_.find(j.string("tale"));
            if (!t || t->kind != "tale" || t->state != "running" || !t->calledOn)
                result = {false, "That story isn't taking anyone.", {}};
            else if (blocked(id, t->authorCharacter) || t->takesPart(id))
                result = {false, "You can't ask to join that story.", {}};
            else
            {
                t->asked.insert(id);
                storylines_.touch();
                if (auto* tc = clientOf(t->authorCharacter))
                    storylineToast(tc, names::capitalised(labelFor(t->authorCharacter, id)) + " asks to join \"" + t->title + "\".");
                storyLogAdd(id, t->id, "ask", t->authorCharacter, {}, {});
                result = {true, "You ask to join \"" + t->title + "\": its storyteller will say.", {}};
            }
        }
        else if (verb == "accept" || verb == "decline")
        {
            auto* t = storylines_.find(j.string("tale"));
            if (!t || !t->invited.count(id))
                result = {false, "You haven't been invited to that story.", {}};
            else
            {
                t->invited.erase(id);
                if (verb == "accept")
                {
                    if (storylines_.counting(id, "tale") >= r.talesAtOnce)
                        result = {false, "You take part in as many stories as you can follow.", {}};
                    else
                    {
                        t->participants[id] = {now(), -1};
                        storyLogAdd(id, t->id, "joined", {}, {}, {});
                        logEvent("tale joined", id, t->id, t->title);
                        if (auto* tc = clientOf(t->authorCharacter))
                            storylineToast(tc, names::capitalised(labelFor(t->authorCharacter, id)) + " joins \"" + t->title + "\".");
                        result = {true, "You join \"" + t->title + "\".", {}};
                    }
                }
                else
                    result = {true, "Not now.", {}};
                storylines_.touch();
            }
        }
        else if (!tale || (!tale->takesPart(id) && tale->authorCharacter != id))
            result = {false, "You're in no story here.", {}};
        else
        {
            std::string error;
            const auto rolled = rollDice(j.string("dice"), tale->id + id + std::to_string(sequence_++), error);
            if (rolled.empty())
                result = {false, error, {}};
            else
            {
                const auto why = j.string("for").substr(0, 80);
                deliverStory(*tale, id, "roll", {}, (tale->authorCharacter == id ? "rolls " : "rolls (a participant) ") + rolled + (why.empty() ? "" : " for " + why));
                storyLogAdd(id, tale->id, "roll", {}, {}, rolled + (why.empty() ? "" : " for " + why));
                result = {true, {}, {}};
            }
        }
        saveSoon();
        return true;
    }
    if (verb == "apply")
    {
        const auto note = j.string("note");
        if (const auto why = whyNotApply(id); !why.empty())
            result = {false, why, {}};
        else if (note.size() > 500 || !plainText(note))
            result = {false, "Say what you'd like to run, in 500 letters at most.", {}};
        else
        {
            auto& s = storytellers_[account];
            s = {};
            s.state = "applied";
            s.note = note;
            s.character = id;
            s.appliedAt = now();
            storyLogAdd(id, {}, "apply", {}, {}, note.substr(0, 200));
            result = {true, "Your application is with the Dungeon Masters.", {}};
            saveSoon();
        }
        return true;
    }
    if (!approvedStoryteller(account))
    {
        result = {false, "Only storytellers the Dungeon Masters have approved can do that.", {}};
        return true;
    }
    if (verb == "save")
    {
        result = saveTale(id, j);
        return true;
    }
    auto* tale = taleFor(id, j.string("tale"), true);
    if (!tale)
    {
        result = {false, "Name one of your own tales.", {}};
        return true;
    }
    tale->lastActivity = now();
    if (verb == "start")
    {
        int running = 0;
        for (const auto& [tid, t] : storylines_.all())
            running += t.kind == "tale" && t.authorAccount == account && t.state == "running";
        if (tale->state != "draft" && tale->state != "paused")
            result = {false, "That tale isn't waiting to begin.", {}};
        else if (tale->state == "draft" && running >= r.runningTales)
            result = {false, "You run " + std::to_string(r.runningTales) + " tales at most at once.", {}};
        else
        {
            tale->state = "running";
            tale->participants.try_emplace(id, storylines::Participant{now(), -1});
            storyLogAdd(id, tale->id, "start", {}, {}, tale->title);
            logEvent("tale begun", id, tale->id, tale->title);
            result = {true, "\"" + tale->title + "\" begins.", {}};
        }
    }
    else if (tale->state != "running")
        result = {false, "\"" + tale->title + "\" isn't running.", {}};
    else if (verb == "invite" || verb == "admit")
    {
        const auto who = j.string("who");
        const auto* w = world_.entity(who);
        int count = 0;
        for (const auto& [p, part] : tale->participants)
            count += part.active();
        if (!w || w->npc)
            result = {false, "Invite a wolf by who they are.", {}};
        else if (count + int(tale->invited.size()) >= r.participants)
            result = {false, "A tale has " + std::to_string(r.participants) + " wolves at most.", {}};
        else if (verb == "admit" && !tale->asked.count(who))
            result = {false, "They haven't asked to join.", {}};
        else if (verb == "invite" && !inTaleScope(id, who, *tale))
            result = {false, "Your tales are for your party, Chapter, circles and friends (or those you admit from a call).", {}};
        else if (blocked(id, who))
            result = {false, "You can't invite them.", {}};
        else if (verb == "admit")
        {
            tale->asked.erase(who);
            tale->admitted.insert(who);
            tale->participants[who] = {now(), -1};
            storyLogAdd(id, tale->id, "admit", who, {}, {});
            if (auto* wc = clientOf(who))
                storylineToast(wc, "You're admitted to \"" + tale->title + "\": it is in your journal.");
            result = {true, "Admitted.", {}};
        }
        else
        {
            tale->invited.insert(who);
            storyLogAdd(id, tale->id, "invite", who, {}, {});
            if (auto* wc = clientOf(who))
            {
                auto ev = Value::object();
                ev.add("type", "story");
                ev.add("kind", "invite");
                ev.add("tale", tale->id);
                ev.add("speaker", "✦ STORY");
                ev.add("text", names::capitalised(labelFor(who, id)) + " invites you into their story \"" + tale->title + "\" (" +
                                   std::to_string(tale->steps.size()) + (tale->steps.size() == 1 ? " step" : " steps") + "). See your journal: JOIN or NOT NOW.");
                send(wc, ev);
            }
            result = {true, "Invited.", {}};
        }
    }
    else if (verb == "narrate" || verb == "npc")
    {
        const auto text = j.string("text");
        const auto last = narratedAt_.find(id);
        const auto* member = verb == "npc" ? [&]() -> const storylines::CastMember* {
            for (const auto& m : tale->cast)
                if (lowered(m.name) == lowered(j.string("name")))
                    return &m;
            return nullptr;
        }() : nullptr;
        if (text.empty() || int(text.size()) > r.narrateLetters || !plainText(text))
            result = {false, "Narration is " + std::to_string(r.narrateLetters) + " letters at most.", {}};
        else if (last != narratedAt_.end() && world_.time() - last->second < r.narrateEvery)
            result = {false, "A moment between lines.", {}};
        else if (verb == "npc" && !member)
            result = {false, "That isn't one of the tale's cast.", {}};
        else
        {
            narratedAt_[id] = world_.time();   // (World seconds: the rate holds in tests and fast-forward alike.)
            deliverStory(*tale, id, verb, member ? member->name : std::string(), member ? "says: " + text : text);
            storyLogAdd(id, tale->id, verb, member ? member->name : std::string(), text, {});
            // Counted as the storyteller's roleplay in its scene (doc 08's rules and caps): running a session pays like
            // taking part in one, no more. Its listeners: the tale's wolves here.
            std::vector<std::string> heard;
            for (const auto& [who, p] : tale->participants)
                if (const auto* w = world_.entity(who); w && p.active() && who != id && w->cellId == me->cellId)
                    heard.push_back(who);
            const int words = int(std::count(text.begin(), text.end(), ' ')) + 1;
            social_.record({sequence_++, now(), id, me->cellId, words, false, std::hash<std::string>{}(text), {}, {}}, heard);
            result = {true, {}, {}};
        }
    }
    else if (verb == "tick")
    {
        const auto step = std::size_t(j.number("step", double(tale->current()))), objective = std::size_t(j.number("objective"));
        const auto ticked = storylines_.tick(tale->id, step, objective, j.string("by", id), now(), true);
        if (ticked.empty())
            result = {false, "Nothing to tick there.", {}};
        else
        {
            storyLogAdd(id, tale->id, "tick", {}, {}, std::to_string(step + 1) + "." + std::to_string(objective + 1));
            storylineProgress(ticked);
            result = {true, "Ticked" + std::string(tale->steps[step].objectives[objective].kind == "told" ? "." : " (you judged it done)."), {}};
        }
    }
    else if (verb == "visitor" || verb == "dismiss")
        result = storyVisitor(id, *tale, j);
    else if (verb == "prize")
        result = storyPrize(id, *tale, j);
    else if (verb == "call")
        result = storyCall(id, *tale, j);
    else if (verb == "end")
    {
        const auto how = j.string("how", "done");
        if (how != "done" && how != "failed" && how != "abandoned")
            result = {false, "It ends done, failed or abandoned.", {}};
        else
        {
            endTale(*tale, how);
            result = {true, "\"" + tale->title + "\" ends.", {}};
        }
    }
    else
        result = {false, "There's nothing like that to do.", {}};
    storylines_.touch();
    saveSoon();
    return true;
}

void Game::endTale(storylines::Storyline& tale, const std::string& how)
{
    // The end card: each participant who did a part, or was in it, sees who did what, and may star the storyteller once
    // (doc 51's `tale` kind, tagged Storyteller).
    storylines_.end(tale.id, how, now());
    for (auto& [vid, v] : storyVisitorsOn_)
        if (v.tale == tale.id)
            world_.sendVisitorAway(vid);
    storyLogAdd(tale.authorCharacter, tale.id, "end", {}, {}, how);
    logEvent("tale ended", tale.authorCharacter, tale.id, tale.title.substr(0, 100) + " (" + how + ")");
    Credits cr;
    cr.id = "credits-" + std::to_string(storyLogNext_++);
    cr.kind = "tale";
    cr.title = tale.title;
    cr.storyline = tale.id;
    cr.teller = tale.authorCharacter;
    cr.made = now();
    cr.until = now() + storylines::rules().creditsDays * 86400.0;
    for (const auto& [who, p] : tale.participants)
    {
        if (who == tale.authorCharacter)
            continue;
        cr.people.push_back(who);
        for (const auto& st : tale.steps)
            for (const auto& o : st.objectives)
                if (o.doneBy == who)
                    cr.lines[who].push_back(o.line.empty() ? st.title : o.line);
    }
    if (!cr.people.empty())
        credits_[cr.id] = cr;                       // (Shown to each in it by the next pass: tendStorytellers.)
    note("info", "RATW_TALE_END " + tale.id + " " + how + "; credits for " + std::to_string(cr.people.size()));
}

// ------------------------------------------------------------------ Phase 5: visitors, prizes and calls

Result Game::storyVisitor(const std::string& teller, storylines::Storyline& tale, const Value& j)
{
    const auto& r = storylines::rules();
    if (j.string("verb") == "dismiss")
    {
        const auto vid = j.string("visitor");
        const auto on = storyVisitorsOn_.find(vid);
        if (on == storyVisitorsOn_.end() || on->second.teller != teller)
            return {false, "No such visitor of yours.", {}};
        world_.sendVisitorAway(vid);
        storyVisitorsOn_.erase(on);
        storyLogAdd(teller, tale.id, "dismiss", vid, {}, {});
        return {true, "They go on their way.", {}};
    }
    int mine = 0;
    for (const auto& [vid, v] : storyVisitorsOn_)
        mine += v.teller == teller;
    const auto* def = [&]() -> const StoryVisitorDef* {
        for (const auto& d : storyVisitorDefs_)
            if (d.id == j.string("visitor") && d.enabled)
                return &d;
        return nullptr;
    }();
    const auto* t = world_.entity(teller);
    const double minutes = std::clamp(j.number("minutes", 20), double(r.visitorMinutes[0]), double(r.visitorMinutes[1]));
    if (!def)
        return {false, "Visitors come only from the Dungeon Masters' list.", {}};
    if (mine >= r.visitorsAtOnce)
        return {false, "You have " + std::to_string(r.visitorsAtOnce) + " visitors on stage already.", {}};
    // Outdoors or a public room: never a home, someone's rented place, or a seat of power.
    const auto* cell = t ? world_.cell(t->cellId) : nullptr;
    if (!cell || (!cell->outdoors && (estates_.lease(t->cellId) || !innCells_.count(t->cellId))))
        return {false, "Visitors come only outdoors, or into an inn's common room.", {}};
    // Within reach of the storyteller, on the first tile that takes them.
    const auto vid = "storyvisitor-" + std::to_string(storyLogNext_++);
    Result made{false, "There is no room for them here.", {}};
    for (int ring = 1; ring <= r.visitorReach && !made.ok; ++ring)
        for (int dy = -ring; dy <= ring && !made.ok; ++dy)
            for (int dx = -ring; dx <= ring && !made.ok; ++dx)
                if (std::max(std::abs(dx), std::abs(dy)) == ring)
                    made = world_.addVisitor(vid, def->name, def->description + " (part of a story)", def->appearance, t->cellId,
                                             std::floor(t->position.x) + dx + .5, std::floor(t->position.y) + dy + .5, minutes);
    if (!made.ok)
        return made;
    world_.markStoryVisitor(vid);
    storyVisitorsOn_[vid] = {vid, tale.id, teller, now() + minutes * 60};
    storyLogAdd(teller, tale.id, "visitor", def->id, {}, std::to_string(int(minutes)) + " minutes");
    return {true, def->name + " arrives, for " + std::to_string(int(minutes)) + " minutes. Voice them with /npc.", vid};
}

Result Game::storyPrize(const std::string& teller, storylines::Storyline& tale, const Value& j)
{
    // From its own purse or belongings, or its Chapter's treasury for a Chapter's tale by an Officer or the Head; to a
    // participant only. A move, journalled.
    const auto to = j.string("to");
    const auto coins = std::int64_t(j.number("coins"));
    const auto item = j.string("item");
    const int quantity = std::max(1, int(j.number("quantity", 1)));
    if (!tale.takesPart(to) || to == teller)
        return {false, "Prizes go to the tale's participants.", {}};
    auto& society = world_.society();
    std::string from = teller;
    if (j.boolean("treasury"))
    {
        const auto* ch = chapters_.of(teller);
        const auto* m = chapters_.member(teller);
        if (!ch || ch->id != tale.chapter || !m || m->rank > chapter::RankOfficer)
            return {false, "Only an Officer or the Head gives from the Chapter's treasury, for the Chapter's tale.", {}};
        from = "chapter:" + ch->id;
    }
    bool moved = false;
    if (coins > 0 && item.empty())
        moved = coins <= 1000 && society.account(from) && society.account(from)->cash >= coins &&
                society.shift(from, to, "", 0, coins, "a story's prize");
    else if (!item.empty() && from == teller)
        moved = society.account(teller) && Society::stock(*society.account(teller), item) >= quantity &&
                society.shift(teller, to, item, quantity, 0, "a story's prize");
    if (!moved)
        return {false, "There isn't so much to give.", {}};
    record(Economy | Character, teller);
    record(Economy | Character, to);
    world_.recordEvent({"story prize", teller, to, {}, 0, 0, item, item.empty() ? 0 : quantity, coins, tale.id});
    storyLogAdd(teller, tale.id, "prize", to, {}, coins > 0 ? std::to_string(coins) + "p" : std::to_string(quantity) + " " + item);
    if (auto* c = clientOf(to))
        storylineToast(c, names::capitalised(labelFor(to, teller)) + " gives you a prize in \"" + tale.title + "\": " +
                              (coins > 0 ? std::to_string(coins) + " pennies." : std::to_string(quantity) + " " + item + "."));
    return {true, "Given.", {}};
}

Result Game::storyCall(const std::string& teller, storylines::Storyline& tale, const Value& j)
{
    // A call on the town's board (doc 54): strangers may ask to join; the storyteller admits them, for this tale only.
    const auto* t = world_.entity(teller);
    const auto town = t ? world_.communityOf(t->cellId) : std::string();
    if (town.empty() || !world_.boardSpot(town))
        return {false, "Post a call where a town keeps a notice board.", {}};
    tale.calledOn = true;
    tale.calledTown = town;
    const auto words = j.string("text", "Wolves wanted for a story: \"" + tale.title + "\". Ask to join.").substr(0, 200);
    tale.callText = words;
    storyLogAdd(teller, tale.id, "call", town, {}, words);
    return {true, "Your call is up on " + townWords(town) + "'s board.", {}};
}

void Game::setStoryVisitors(const Value& list)
{
    storyVisitorDefs_.clear();
    for (const auto& o : list.items())
    {
        StoryVisitorDef d;
        d.id = o.string("id");
        d.name = o.string("name").substr(0, 60);
        d.description = o.string("description").substr(0, 400);
        d.enabled = o.boolean("enabled", true);
        if (o.object("appearance").isObject())
            wire::readAppearance(o.object("appearance"), d.appearance);
        if (!d.id.empty() && !d.name.empty())
            storyVisitorDefs_.push_back(std::move(d));
    }
}

void Game::loadStoryVisitors()
{
    // The DM's approved list (live.story_visitors), with a database; none without.
    if (options_.database.empty() || liveWorldId_.empty())
        return;
    const auto rows = worldDb_.exec("SELECT id, name, description, appearance::text, enabled FROM live.story_visitors WHERE world_id = $1 "
                                    "AND approved_by IS NOT NULL ORDER BY id", {liveWorldId_});
    if (!rows.ok)
        return;
    auto list = Value::array();
    for (const auto& row : rows.rows)
        if (row.size() >= 5 && row[0] && row[1])
        {
            auto o = Value::object();
            o.add("id", *row[0]);
            o.add("name", *row[1]);
            o.add("description", row[2] ? *row[2] : std::string());
            Value look;
            std::string error;
            if (row[3] && json::parse(*row[3], look, error))
                o.add("appearance", look);
            o.add("enabled", row[4] && (*row[4] == "t" || *row[4] == "true"));
            list.push(o);
        }
    setStoryVisitors(list);
}

void Game::tendStorytellers(double dt)
{
    storytellerAccumulator_ += dt;
    if (storytellerAccumulator_ < 5)
        return;
    storytellerAccumulator_ = 0;
    if (!storyVisitorsLoaded_)
    {
        storyVisitorsLoaded_ = true;
        loadStoryVisitors();
    }
    // Credits screens not yet shown to those in them (away, or fighting, when they were made).
    for (auto& [id, cr] : credits_)
        for (const auto& who : cr.people)
            if (!cr.delivered.count(who) && clientOf(who) && !world_.inBattle(who))
            {
                cr.delivered.insert(who);
                sendCredits(who, cr);
            }
    // Visitors whose time is up are gone (the world sends them); forget them.
    for (auto it = storyVisitorsOn_.begin(); it != storyVisitorsOn_.end();)
        it = !world_.entity(it->first) || now() > it->second.until + 5 ? storyVisitorsOn_.erase(it) : std::next(it);
    // The kept words: the text cleared past its days; the record past 180 days.
    const double keep = storylines::rules().keepTextDays * 86400.0;
    for (auto& e : storyLog_)
        if (!e.text.empty() && now() - e.at > keep)
            e.text.clear();
    storyLog_.erase(std::remove_if(storyLog_.begin(), storyLog_.end(), [&](const StoryLogEntry& e) { return now() - e.at > 180 * 86400.0; }),
                    storyLog_.end());
    for (auto it = credits_.begin(); it != credits_.end();)
        it = now() > it->second.until ? credits_.erase(it) : std::next(it);
}

// ------------------------------------------------------------------ Phase 6: credits screens

void Game::sendCredits(const std::string& viewer, const Credits& cr)
{
    // Who did what, in the names the viewer knows; the stars it may give (a tale: its storyteller, once; a milestone: up
    // to 3 contributors).
    auto* c = clientOf(viewer);
    if (!c)
        return;
    if (world_.inBattle(viewer))
        return;                                     // (Shown after the fight.)
    auto ev = Value::object();
    ev.add("type", "credits");
    ev.add("id", cr.id);
    ev.add("kind", cr.kind);
    ev.add("title", cr.title);
    if (!cr.story.empty())
        ev.add("story", cr.story);
    auto people = Value::array();
    const auto starred = cr.starred.find(viewer);
    for (const auto& who : cr.people)
    {
        auto o = Value::object();
        o.add("id", who);
        o.add("name", who == viewer ? std::string("You") : names::capitalised(labelFor(viewer, who)));
        auto lines = Value::array();
        if (const auto l = cr.lines.find(who); l != cr.lines.end())
            for (const auto& line : l->second)
                lines.push(storyWords(viewer, line));
        o.add("lines", lines);
        o.add("starred", starred != cr.starred.end() && starred->second.count(who) > 0);
        people.push(o);
    }
    ev.add("people", people);
    if (cr.kind == "tale")
    {
        ev.add("teller", cr.teller);
        ev.add("tellerName", names::capitalised(labelFor(viewer, cr.teller)));
        ev.add("tellerStarred", starred != cr.starred.end() && starred->second.count(cr.teller) > 0);
    }
    ev.add("stars", cr.kind == "tale" ? 1 : storylines::rules().creditsStars);
    send(c, ev);
}

Result Game::starCredit(const std::string& giver, const std::string& creditsId, const std::string& to)
{
    auto it = credits_.find(creditsId);
    if (it == credits_.end())
        return {false, "That screen has closed.", {}};
    auto& cr = it->second;
    if (std::find(cr.people.begin(), cr.people.end(), giver) == cr.people.end())
        return {false, "You weren't in it.", {}};
    auto& mine = cr.starred[giver];
    const bool tale = cr.kind == "tale";
    if (tale ? to != cr.teller : (std::find(cr.people.begin(), cr.people.end(), to) == cr.people.end() || to == giver))
        return {false, tale ? "Star its storyteller." : "Star someone who took part.", {}};
    if (mine.count(to) || int(mine.size()) >= (tale ? 1 : storylines::rules().creditsStars))
        return {false, tale ? "You have starred its storyteller." : "You have given your stars here.", {}};
    if (accountKey(giver) == accountKey(to))
        return {false, "Not your own wolf.", {}};
    stars::Star s;
    s.kind = tale ? "tale" : "milestone";
    s.source = cr.id;
    s.giverAccount = accountKey(giver);
    s.giverCharacter = giver;
    s.recipientAccount = accountKey(to);
    s.recipientCharacter = to;
    s.tag = tale ? "storyteller" : std::string();   // (A milestone star is tagged afterwards, as doc 51's are.)
    s.at = now();
    const auto kept = starBook_.record(s);
    mine.insert(to);
    logEvent(tale ? "tale star" : "milestone star", giver, to, cr.title.substr(0, 100));
    saveSoon();
    sendCredits(giver, cr);
    return {true, std::string(kept.counted ? "Your star is given." : "Your star is given (it doesn't count toward their total: the pair's limits)."), {}};
}

Result Game::creditMilestone(const Value& payload)
{
    // A world story's milestone (doc 58, 7), from the DM host: its lines built from the ledger there, checked here (every
    // wolf a character; at most 40; lines short and plain), rendered per viewer and kept 3 days to star. Deeds for the
    // main contributors (the first three, or those named).
    Credits cr;
    cr.id = "credits-" + std::to_string(storyLogNext_++);
    cr.kind = "milestone";
    cr.story = payload.string("story").substr(0, 80);
    cr.title = payload.string("milestone").substr(0, 80);
    cr.made = now();
    cr.until = now() + storylines::rules().creditsDays * 86400.0;
    if (cr.title.empty())
        return {false, "Name the milestone.", {}};
    for (const auto& p : payload.array("people"))
    {
        const auto who = p.string("id");
        if (!characters_.count(who) && !(world_.entity(who) && !world_.entity(who)->npc))
            continue;
        if (cr.people.size() >= 40)
            break;
        cr.people.push_back(who);
        for (const auto& l : p.array("lines"))
            if (l.isString() && l.asString().size() <= 160 && plainText(l.asString()) && cr.lines[who].size() < 6)
                cr.lines[who].push_back(l.asString());
    }
    if (cr.people.empty())
        return {false, "Nobody to credit.", {}};
    const int weight = payload.string("weight", "great") == "legendary" ? fame::Legendary : fame::Great;
    std::vector<std::string> main;
    for (const auto& m : payload.array("main"))
        if (m.isString() && std::find(cr.people.begin(), cr.people.end(), m.asString()) != cr.people.end())
            main.push_back(m.asString());
    if (main.empty())
        for (std::size_t i = 0; i < cr.people.size() && i < 3; ++i)
            main.push_back(cr.people[i]);
    for (const auto& who : main)
        if (const auto* e = world_.entity(who))
            recordDeed("award", {who}, {}, e->cellId, "milestone", (cr.story.empty() ? std::string() : cr.story + ": ") + cr.title, weight);
    logEvent("milestone", "dm", cr.id, (cr.story.empty() ? std::string() : cr.story + " · ") + cr.title);
    credits_[cr.id] = cr;                           // (Shown to each in it by the next pass, after any fight: tendStorytellers.)
    saveSoon();
    return {true, "Credited " + std::to_string(cr.people.size()) + " wolves.", cr.id};
}

// ------------------------------------------------------------------ The views, and saving

Value Game::storytellerSelf(const std::string& viewer)
{
    // For applicants and storytellers: standing; for storytellers, their tales with progress and the command list; for
    // anyone, open calls in this town (to ask to join), and credits still open to stars.
    auto o = Value::object();
    const auto account = accountKey(viewer);
    const auto s = storytellers_.find(account);
    if (s != storytellers_.end())
    {
        o.add("state", s->second.state);
        if (!s->second.reason.empty())
            o.add("reason", s->second.reason);
    }
    const auto why = whyNotApply(viewer);
    o.add("canApply", why.empty());
    o.add("whyNot", why);
    if (approvedStoryteller(account))
    {
        auto tales = Value::array();
        for (const auto& [id, t] : storylines_.all())
            if (t.kind == "tale" && t.authorAccount == account)
            {
                auto j = Value::object();
                j.add("id", id);
                j.add("title", t.title);
                j.add("state", t.state);
                j.add("step", int(t.current()));
                j.add("steps", int(t.steps.size()));
                auto cast = Value::array();
                for (const auto& m : t.cast)
                    cast.push(m.name);
                j.add("cast", cast);
                auto asked = Value::array();
                for (const auto& who : t.asked)
                {
                    auto a = Value::object();
                    a.add("id", who);
                    a.add("name", names::capitalised(labelFor(viewer, who)));
                    asked.push(a);
                }
                j.add("asked", asked);
                if (t.current() < t.steps.size())
                {
                    auto objectives = Value::array();
                    for (const auto& ob : t.steps[t.current()].objectives)
                    {
                        auto k = Value::object();
                        k.add("line", ob.line);
                        k.add("done", ob.done());
                        objectives.push(k);
                    }
                    j.add("objectives", objectives);
                }
                tales.push(j);
            }
        o.add("tales", tales);
        auto visitors = Value::array();
        for (const auto& d : storyVisitorDefs_)
            if (d.enabled)
            {
                auto v = Value::object();
                v.add("id", d.id);
                v.add("name", d.name);
                visitors.push(v);
            }
        o.add("visitors", visitors);
        auto on = Value::array();
        for (const auto& [vid, v] : storyVisitorsOn_)
            if (v.teller == viewer)
            {
                auto j = Value::object();
                j.add("id", vid);
                j.add("name", world_.entity(vid) ? world_.entity(vid)->name : vid);
                on.push(j);
            }
        o.add("onStage", on);
        auto commands = Value::array();
        for (const char* line : {"/narrate <text>", "/npc <cast name>: <line>", "/roll 2d6+1 for <what>", "/tick <step>.<part>"})
            commands.push(line);
        o.add("commands", commands);
    }
    if (const auto* me = world_.entity(viewer))
    {
        const auto town = world_.communityOf(me->cellId);
        auto calls = Value::array();
        for (const auto& [id, t] : storylines_.all())
            if (t.kind == "tale" && t.state == "running" && t.calledOn && t.calledTown == town && !t.takesPart(viewer) &&
                !blocked(viewer, t.authorCharacter))
            {
                auto j = Value::object();
                j.add("id", id);
                j.add("title", t.title);
                j.add("text", t.callText);
                j.add("by", names::capitalised(labelFor(viewer, t.authorCharacter)));
                j.add("asked", t.asked.count(viewer) > 0);
                calls.push(j);
            }
        if (!calls.items().empty())
            o.add("calls", calls);
    }
    return o;
}

void Game::storytellersSave(Value& root) const
{
    auto list = Value::array();
    for (const auto& [account, s] : storytellers_)
    {
        auto o = Value::object();
        o.add("account", account); o.add("state", s.state); o.add("note", s.note); o.add("character", s.character);
        o.add("decidedBy", s.decidedBy); o.add("reason", s.reason); o.add("appliedAt", s.appliedAt); o.add("decidedAt", s.decidedAt);
        list.push(o);
    }
    root.add("storytellers", list);
    auto log = Value::array();
    for (const auto& e : storyLog_)
    {
        auto o = Value::object();
        o.add("id", e.id); o.add("account", e.account); o.add("character", e.character); o.add("storyline", e.storyline);
        o.add("kind", e.kind); o.add("target", e.target); o.add("text", e.text); o.add("detail", e.detail); o.add("at", e.at);
        log.push(o);
    }
    root.add("storytellerLog", log);
    auto credits = Value::array();
    for (const auto& [id, cr] : credits_)
    {
        auto o = Value::object();
        o.add("id", id); o.add("kind", cr.kind); o.add("title", cr.title); o.add("story", cr.story); o.add("storyline", cr.storyline);
        o.add("teller", cr.teller); o.add("made", cr.made); o.add("until", cr.until);
        auto people = Value::array();
        for (const auto& p : cr.people)
        {
            auto j = Value::object();
            j.add("id", p);
            auto lines = Value::array();
            if (const auto l = cr.lines.find(p); l != cr.lines.end())
                for (const auto& line : l->second)
                    lines.push(line);
            j.add("lines", lines);
            auto starred = Value::array();
            if (const auto st = cr.starred.find(p); st != cr.starred.end())
                for (const auto& to : st->second)
                    starred.push(to);
            j.add("starred", starred);
            people.push(j);
        }
        o.add("people", people);
        credits.push(o);
    }
    root.add("credits", credits);
}

void Game::storytellersLoad(const Value& saved)
{
    storytellers_.clear();
    storyLog_.clear();
    credits_.clear();
    storyLogNext_ = 1;
    for (const auto& o : saved.array("storytellers"))
    {
        StorytellerStanding s;
        s.state = o.string("state"); s.note = o.string("note"); s.character = o.string("character"); s.decidedBy = o.string("decidedBy");
        s.reason = o.string("reason"); s.appliedAt = o.number("appliedAt"); s.decidedAt = o.number("decidedAt", -1);
        if (!o.string("account").empty())
            storytellers_[o.string("account")] = s;
    }
    for (const auto& o : saved.array("storytellerLog"))
    {
        StoryLogEntry e{o.string("id"), o.string("account"), o.string("character"), o.string("storyline"), o.string("kind"),
                        o.string("target"), o.string("text"), o.string("detail"), o.number("at")};
        if (e.id.rfind("slog-", 0) == 0)
            storyLogNext_ = std::max<std::uint64_t>(storyLogNext_, std::strtoull(e.id.c_str() + 5, nullptr, 10) + 1);
        storyLog_.push_back(std::move(e));
    }
    for (const auto& o : saved.array("credits"))
    {
        Credits cr;
        cr.id = o.string("id"); cr.kind = o.string("kind"); cr.title = o.string("title"); cr.story = o.string("story");
        cr.storyline = o.string("storyline"); cr.teller = o.string("teller"); cr.made = o.number("made"); cr.until = o.number("until");
        for (const auto& p : o.array("people"))
        {
            cr.people.push_back(p.string("id"));
            for (const auto& l : p.array("lines"))
                cr.lines[p.string("id")].push_back(l.asString());
            for (const auto& s : p.array("starred"))
                cr.starred[p.string("id")].insert(s.asString());
        }
        const auto n = cr.id.rfind('-');
        if (n != std::string::npos)
            storyLogNext_ = std::max<std::uint64_t>(storyLogNext_, std::strtoull(cr.id.c_str() + n + 1, nullptr, 10) + 1);
        if (!cr.id.empty())
            credits_[cr.id] = std::move(cr);
    }
}
} // namespace ratw::game
