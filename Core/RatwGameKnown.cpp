// Known wolves and scene recaps (Docs/Design/50-player-card-friends-safety.md, Phase 4; agreed, doc 48 §3.2 and
// decision 35). Each character keeps a list of the wolves it has met: players it shared a scene with, was in a party
// with or learnt the name of; residents once it knows their name, notes or tags them. Each entry has where and when
// they last met, scenes shared, the character's own tag and private note, and up to three recaps of scenes they shared.
// A recap is written from only what this character perceived (the lines delivered to it, as it knew the speakers), by
// the small model when there was enough of a scene and the player allows it, else plainly from the ledger.
#include "RatwGame.h"
#include "RatwNames.h"

#include <algorithm>
#include <cmath>

namespace ratw::game
{
using json::Value;

void Game::meet(const std::string& owner, const std::string& other, const std::string& how)
{
    // `how`: "scene" (counted), "party", "name", "note", "tag", or "heard" (throttled: once every 10 minutes a pair).
    const auto* me = world_.entity(owner);
    if (!me || me->npc || owner == other || other.empty())
        return;
    const auto* them = world_.entity(other);
    if (!them && !characters_.count(other))
        return;
    const bool resident = them && them->npc;
    const auto& r = people::rules();
    const auto key = owner + "|" + other;
    if (how == "heard")
    {
        auto& at = metAt_[key];
        if (now() - at < r.metEvery)
            return;
        at = now();
    }
    auto& list = knownWolves_[owner];
    auto it = list.find(other);
    const bool added = it == list.end();
    if (added)
    {
        // A resident joins the list only when this wolf knows its name, notes it or tags it.
        if (resident && how != "name" && how != "note" && how != "tag")
            return;
        it = list.emplace(other, people::KnownWolf{}).first;
        it->second.firstMet = now();
        it->second.resident = resident;
    }
    auto& k = it->second;
    if (them)
    {
        k.lastMet = now();
        k.lastMetDay = world_.calendarDays();
        k.lastPlace = me->cellId;
        k.label = labelFor(owner, other);         // (As this wolf knew them: shown while they're away.)
    }
    if (how == "scene")
        ++k.scenes;
    if (added)
        people::trimKnown(list, recaps_[owner]);
    if (how != "heard")
        saveSoon();
}

void Game::perceivedLine(const std::string& listener, const std::string& who, const std::string& text)
{
    // What reached a player, for a recap of the scene it is in (in memory only; the newest 120 lines or 6,000
    // characters). Only for players who let the model write their recaps: the written recap needs no lines.
    const auto person = people_.find(accountKey(listener));
    if (person != people_.end() && !person->second.settings.recaps)
        return;
    const auto* me = world_.entity(listener);
    if (!me)
        return;
    const auto& r = people::rules();
    auto& p = perceived_[listener];
    p.lines.push_back({now(), me->cellId, who, text.substr(0, 600)});
    p.characters += p.lines.back().text.size();
    while (!p.lines.empty() && (p.lines.size() > std::size_t(r.bufferLines) || p.characters > std::size_t(r.bufferCharacters)))
    {
        p.characters -= p.lines.front().text.size();
        p.lines.pop_front();
    }
}

void Game::tendScenes()
{
    // Scenes that have ended, and members who stepped out of one, since last time: the members meet each other on
    // their lists, and each gets a recap.
    for (const auto& [sid, s] : social_.sessions)
    {
        if (s.ended > 0)
        {
            if (!scenesDone_.insert(sid).second)
                continue;
            for (const auto& [member, c] : s.members)
                if (!scenesLeft_.erase(sid + "|" + member))
                    endScene(member, s, s.ended);
        }
        else
            for (const auto& [member, c] : s.members)
                if (c.left && scenesLeft_.insert(sid + "|" + member).second)
                    endScene(member, s, now());
    }
    for (auto it = metAt_.begin(); it != metAt_.end();)
        it = now() - it->second >= people::rules().metEvery ? metAt_.erase(it) : std::next(it);
    // Scenes the ledger has let go (eight days after they ended: doc 51) are forgotten here too.
    for (auto it = scenesDone_.begin(); it != scenesDone_.end();)
        it = social_.sessions.count(*it) ? std::next(it) : scenesDone_.erase(it);
}

void Game::seedScenes()
{
    // After a restart: scenes already over (and members already gone) were recapped before it, or lost their lines.
    scenesDone_.clear();
    scenesLeft_.clear();
    for (const auto& [sid, s] : social_.sessions)
    {
        if (s.ended > 0)
            scenesDone_.insert(sid);
        else
            for (const auto& [member, c] : s.members)
                if (c.left)
                    scenesLeft_.insert(sid + "|" + member);
    }
}

void Game::endScene(const std::string& member, const SocialSession& s, double end)
{
    const auto* me = world_.entity(member);
    if (!me || me->npc)
        return;
    booksSceneEnded(member, s);                     // (Its chapters, and "my next scene goes into …": doc 51, Phase 7.)
    std::vector<std::string> others, notes;
    for (const auto& [other, c] : s.members)
        if (other != member && !blocked(member, other))   // (Never on the list of one who blocked them: doc 50, 7.)
        {
            others.push_back(other);
            meet(member, other, "scene");
            // For the end card (doc 51, §8): a first scene together, and how they regard this wolf now, if that changed.
            if (const auto list = knownWolves_.find(member); list != knownWolves_.end())
                if (const auto k = list->second.find(other); k != list->second.end() && k->second.scenes == 1)
                    notes.push_back("You and " + knownName(member, other) + " shared a scene for the first time.");
            const auto regard = regardWords(other, member);
            auto& seen = regardSeen_[member + "|" + other];
            if (regard != seen && regard != "don't know you")
                notes.push_back("How " + knownName(member, other) + " regards you now: they " + regard + ".");
            seen = regard;
        }
    endedNotes_[member] = {s.id, notes};
    if (others.empty())
        return;
    const auto& r = people::rules();
    const auto joined = s.members.at(member).joined;
    const int minutes = int(std::max(0.0, end - std::max(joined, s.started)) / 60 + .5);
    const auto* cell = world_.cell(s.cell);
    const auto place = cell ? cell->name : std::string("somewhere");
    people::Recap recap;
    recap.id = "rcp-" + guid();
    recap.session = s.id;
    recap.place = place;
    recap.at = now();
    recap.minutes = minutes;
    recap.others = others;
    // The written recap, from the ledger alone: whom with, where, how long; names as this wolf knows them.
    {
        std::string with;
        for (std::size_t i = 0; i < others.size(); ++i)
            with += (i == 0 ? "" : i + 1 == others.size() ? " and " : ", ") + knownName(member, others[i]);
        recap.text = "You shared a scene with " + with + " at " + place + "." +
                     (minutes >= 1 ? " It ran " + std::to_string(minutes) + (minutes == 1 ? " minute." : " minutes.") : "");
    }
    // What this wolf perceived of it: lines in the scene's place from when the scene began (one who listened before
    // speaking heard those too; one who came later has nothing earlier from here) until it ended for them.
    std::vector<std::pair<std::string, std::string>> lines;
    double first = 0, last = 0;
    if (const auto p = perceived_.find(member); p != perceived_.end())
        for (const auto& l : p->second.lines)
            if (l.cell == s.cell && l.at >= s.started - 1 && l.at <= end + 1)
            {
                if (lines.empty())
                    first = l.at;
                last = l.at;
                lines.push_back({l.who == "You" ? names::capitalised(labelFor(member, member)) : l.who, l.text});
            }
    // A fight that broke out in it is part of it (the user; doc 51): its log goes into the recap, as this wolf would
    // read the names.
    for (const auto& m : s.moments)
        if (m.kind == "fight")
            if (const auto fight = social_.sessions.find(m.actor); fight != social_.sessions.end())
                for (const auto& l : fight->second.log)
                    lines.push_back({"The fight", veilFor(member, l)});
    const auto person = people_.find(accountKey(member));
    const bool allowed = person == people_.end() || person->second.settings.recaps;
    const auto day = std::int64_t(now() / 86400);
    auto& today = modelRecaps_[member];
    if (today.first != day)
        today = {day, 0};
    const double span = options_.recapModelSeconds >= 0 ? options_.recapModelSeconds : r.modelMinutes * 60;
    const bool model = allowed && mind_.live() && int(lines.size()) >= r.modelLines && last - first >= span &&
                       today.second < r.modelADay;
    if (!model)
    {
        keepRecap(member, std::move(recap));
        return;
    }
    ++today.second;
    // The wolves as this one saw them (the user): each one's look and pronouns (from its sex) from its card, as this
    // wolf may see it, never its out-of-character side.
    std::vector<mind::Client::RecapWolf> wolves;
    std::vector<std::string> everyone{member};
    everyone.insert(everyone.end(), others.begin(), others.end());
    for (const auto& w : everyone)
    {
        if (wolves.size() >= 12)
            break;
        const auto card = cardFor(member, w);
        mind::Client::RecapWolf wolf{w == member ? names::capitalised(labelFor(member, member)) : names::capitalised(knownName(member, w)),
                                     card.string("pronouns"), card.string("description")};
        if (const auto currently = card.string("currently"); !currently.empty())
            wolf.description += (wolf.description.empty() ? "" : " ") + std::string("Currently: ") + currently;
        wolves.push_back(std::move(wolf));
    }
    // A scene in one of its books (linked before it ended): the chapters before it, so the story carries on from them
    // instead of describing the same wolves again (the user); its story becomes that chapter's summary if it has none.
    mind::Client::RecapStory story;
    std::string bookId, chapterId;
    for (const auto& [id, b] : books_)
    {
        if (!books::hasWolf(b, member))
            continue;
        const auto at = std::find_if(b.chapters.begin(), b.chapters.end(), [&](const books::Chapter& c) { return c.session == s.id; });
        if (at == b.chapters.end())
            continue;
        story = {b.title, {}};
        for (auto c = b.chapters.begin(); c != at; ++c)
            if (!c->summary.empty() && (!c->privateScene || b.state == "finished" ||
                                        std::find(c->wolves.begin(), c->wolves.end(), member) != c->wolves.end()))
                story.before.push_back({c->title, veilFor(member, c->summary)});
        bookId = id, chapterId = at->id;
        break;
    }
    std::weak_ptr<bool> alive = alive_;
    mind_.recap(place, names::capitalised(labelFor(member, member)), minutes, lines, wolves, story,
                [this, alive, member, recap, bookId, chapterId](const std::string& text) mutable {
                    if (alive.expired())
                        return;
                    if (!text.empty())
                    {
                        recap.text = veilFor(member, text);   // (Any name this wolf doesn't know, as it would see them.)
                        recap.model = true;
                        if (const auto b = books_.find(bookId); b != books_.end() && b->second.state != "finished")
                            for (auto& c : b->second.chapters)
                                if (c.id == chapterId && c.summary.empty())
                                {
                                    c.summary = people::clean(recap.text, std::size_t(books::rules().summary), true);
                                    c.summaryBy = member;
                                    c.model = true;
                                    saveSoon();
                                }
                    }
                    keepRecap(member, std::move(recap));
                });
}

std::string Game::knownName(const std::string& owner, const std::string& other) const
{
    // By the name this wolf knows, or as it looks; one away, as it looked when last seen.
    if (world_.entity(other) || knowsName(owner, other))
        return labelFor(owner, other);
    if (const auto list = knownWolves_.find(owner); list != knownWolves_.end())
        if (const auto k = list->second.find(other); k != list->second.end() && !k->second.label.empty())
            return k->second.label;
    return "someone";
}

void Game::keepRecap(const std::string& member, people::Recap recap)
{
    auto& list = recaps_[member];
    list.push_back(std::move(recap));
    people::trimRecaps(list);
    saveSoon();
}

json::Value Game::knownView(const std::string& owner, const std::string& other, const people::KnownWolf& k, bool allRecaps) const
{
    // One entry as its owner sees it: by the name it knows, else as it looked when last seen.
    auto o = Value::object();
    o.add("id", other);
    o.add("name", names::capitalised(knownName(owner, other)));
    o.add("resident", k.resident);
    o.add("firstMet", k.firstMet);
    o.add("lastMet", k.lastMet);
    if (const auto* cell = world_.cell(k.lastPlace))
        o.add("place", cell->name);
    if (k.lastMetDay >= 0)
        o.add("day", std::floor(k.lastMetDay));
    o.add("scenes", k.scenes);
    o.add("regard", regardWords(other, owner));
    if (!k.tag.empty())
        o.add("tag", k.tag);
    if (k.customTag)
        o.add("customTag", true);
    if (!k.note.empty())
        o.add("note", k.note);
    if (!k.tie.empty())
        o.add("tie", k.tie);
    if (!k.resident)
        if (const auto p = profiles_.find(other); p != profiles_.end() && p->second.revision > k.readRevision)
            o.add("unread", true);
    auto recaps = Value::array();
    int count = 0;
    if (const auto it = recaps_.find(owner); it != recaps_.end())
        for (auto r = it->second.rbegin(); r != it->second.rend(); ++r)
            if (std::find(r->others.begin(), r->others.end(), other) != r->others.end() && count < people::rules().recapsPerWolf)
            {
                if (allRecaps || count == 0)
                {
                    auto j = Value::object();
                    j.add("id", r->id);
                    j.add("text", r->text);
                    j.add("at", r->at);
                    j.add("place", r->place);
                    j.add("minutes", r->minutes);
                    j.add("model", r->model);
                    recaps.push(j);
                }
                ++count;
            }
    o.add("recaps", recaps);
    o.add("recapCount", count);
    return o;
}

void Game::sendKnown(Connection* c, const std::string& only)
{
    // The whole list, newest met first, each with its latest recap; or one entry with all its recaps.
    if (!c || c->entityId.empty())
        return;
    const auto& owner = c->entityId;
    auto e = Value::object();
    e.add("type", "known");
    auto list = Value::array();
    if (const auto it = knownWolves_.find(owner); it != knownWolves_.end())
    {
        if (!only.empty())
        {
            if (const auto k = it->second.find(only); k != it->second.end())
                e.add("entry", knownView(owner, only, k->second, true));
        }
        else
        {
            std::vector<std::pair<std::string, const people::KnownWolf*>> sorted;
            for (const auto& [id, k] : it->second)
                sorted.push_back({id, &k});
            std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second->lastMet > b.second->lastMet; });
            for (const auto& [id, k] : sorted)
                list.push(knownView(owner, id, *k, false));
        }
    }
    if (only.empty())
        e.add("wolves", list);
    send(c, e);
}

bool Game::knownCommand(Connection* c, const json::Value& j, Result& result)
{
    // {"type": "known", "verb": "list" | "get" | "tag" | "note" | "forget" | "unrecap", "target": id, "tag": "…",
    //  "custom": bool, "text": "…", "recap": id}.
    const auto& owner = c->entityId;
    const auto verb = j.string("verb");
    const auto target = j.string("target");
    if (owner.empty())
        return false;
    const auto& r = people::rules();
    auto& list = knownWolves_[owner];
    const auto entry = list.find(target);
    // A wolf one can see, or one already on the list (a resident joins it by being tagged or noted).
    const bool reachable = entry != list.end() ||
                           (!target.empty() && target != owner && world_.entity(target) && world_.visionClarity(owner, target) > 0);
    if (verb == "list")
    {
        sendKnown(c);
        result = {true, {}, {}};
        return true;
    }
    if (verb == "get")
    {
        sendKnown(c, target);
        result = {true, {}, {}};
        return true;
    }
    if (verb == "tag" || verb == "note")
    {
        std::string cleaned;
        if (!reachable)
            result = {false, "There is no one to note.", target};
        else if (verb == "tag" && !people::validTag(j.string("tag"), j.boolean("custom"), cleaned))
            result = {false, "Not a tag.", target};
        else if (verb == "note" && people::clean(j.string("text"), std::size_t(r.noteMost) + 1, true).size() >
                                       people::clean(j.string("text"), std::size_t(r.noteMost), true).size())
            result = {false, "A note is at most " + std::to_string(r.noteMost) + " letters.", target};
        else
        {
            meet(owner, target, verb);
            auto& k = knownWolves_[owner][target];
            if (verb == "tag")
            {
                k.tag = cleaned;
                k.customTag = !cleaned.empty() && j.boolean("custom");
                result = {true, cleaned.empty() ? "Tag cleared." : "Tagged.", target};
            }
            else
            {
                k.note = people::clean(j.string("text"), std::size_t(r.noteMost), true);
                result = {true, k.note.empty() ? "Note cleared." : "Noted.", target};
            }
        }
    }
    else if (verb == "forget")
    {
        if (entry == list.end())
            result = {false, "They aren't on your list.", target};
        else
        {
            list.erase(entry);
            // Its recaps that were with no one else go too.
            auto& mine = recaps_[owner];
            mine.erase(std::remove_if(mine.begin(), mine.end(), [&](const people::Recap& x) {
                           return std::all_of(x.others.begin(), x.others.end(), [&](const std::string& id) { return id == target || !list.count(id); });
                       }),
                       mine.end());
            result = {true, "Forgotten.", target};
        }
    }
    else if (verb == "unrecap")
    {
        auto& mine = recaps_[owner];
        const auto before = mine.size();
        mine.erase(std::remove_if(mine.begin(), mine.end(), [&](const people::Recap& x) { return x.id == j.string("recap"); }), mine.end());
        result = mine.size() < before ? Result{true, "Recap deleted.", {}} : Result{false, "No such recap.", {}};
    }
    else
        return false;
    if (result.ok)
    {
        saveSoon();
        sendKnown(c, verb == "forget" || verb == "unrecap" ? std::string() : target);
    }
    return true;
}

void Game::readProfile(const std::string& owner, const std::string& other)
{
    // A closer look reads their profile: the "unread" mark goes until it changes again (Total RP 3's).
    const auto list = knownWolves_.find(owner);
    const auto p = profiles_.find(other);
    if (list == knownWolves_.end() || p == profiles_.end())
        return;
    if (const auto k = list->second.find(other); k != list->second.end() && k->second.readRevision != p->second.revision)
    {
        k->second.readRevision = p->second.revision;
        saveSoon();
    }
}

json::Value Game::knownFor(const std::string& owner, const std::string& other) const
{
    const auto list = knownWolves_.find(owner);
    if (list == knownWolves_.end())
        return {};
    const auto k = list->second.find(other);
    return k == list->second.end() ? json::Value{} : knownView(owner, other, k->second, true);
}

void Game::knownSave(json::Value& root) const
{
    // Two lists (game.known_wolves and game.scene_recaps through game.sections), for the game alone.
    auto known = Value::array();
    for (const auto& [owner, list] : knownWolves_)
        for (const auto& [other, k] : list)
        {
            auto e = people::saveKnown(k);
            e.add("owner", owner);
            e.add("other", other);
            known.push(e);
        }
    auto recaps = Value::array();
    for (const auto& [owner, list] : recaps_)
        for (const auto& r : list)
        {
            auto e = people::saveRecap(r);
            e.add("owner", owner);
            recaps.push(e);
        }
    root.add("known", known);
    root.add("recaps", recaps);
}

void Game::knownLoad(const json::Value& saved)
{
    knownWolves_.clear();
    recaps_.clear();
    for (const auto& e : saved.array("known"))
        if (const auto owner = e.string("owner"), other = e.string("other"); characters_.count(owner) && !other.empty() && other != owner)
            knownWolves_[owner][other] = people::loadKnown(e);
    for (const auto& e : saved.array("recaps"))
        if (const auto owner = e.string("owner"); characters_.count(owner))
            recaps_[owner].push_back(people::loadRecap(e));
    for (auto& [owner, list] : recaps_)
        people::trimRecaps(list);
    for (auto& [owner, list] : knownWolves_)
        people::trimKnown(list, recaps_[owner]);
}
} // namespace ratw::game
