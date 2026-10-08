// Story books and the bookshelf (Docs/Design/51-scenes-and-stars.md, Phase 7; added by the user 2026-10-07). Scenes are
// linked into player-made Stories at any time (before, during, after); each chapter keeps what the book needs once its
// scene is let go; books are shared with friends, a circle, a Chapter or everyone, finished by a majority of their
// recently active wolves (or three quiet days), and linked into volumes. Books are the one kind of Story: an official
// book drives SocialLedger's Story underneath, so it pays as doc 32's always have. Pure rules: RatwBooks.*.
#include "RatwGame.h"
#include "RatwNames.h"

#include <algorithm>
#include <cmath>

namespace ratw::game
{
using json::Value;

namespace
{
const char* const PrivateWarning =
    "This book has private scenes. Once it is finished, anyone who can read the book can read their summaries.";
}

bool Game::canRead(const std::string& viewer, const books::Book& b) const
{
    if (books::hasWolf(b, viewer) || !b.storyline.empty() || b.sharing == "everyone")
        return true;
    if (b.sharing == "friends")
        return std::any_of(b.wolves.begin(), b.wolves.end(), [&](const std::string& w) { return !b.hidden.count(w) && areFriends(viewer, w); });
    if (b.sharing == "circle")
        if (const auto c = circles_.find(b.circle); c != circles_.end())
            return c->second.members.count(accountKey(viewer)) > 0;
    if (b.sharing == "chapter")
        if (const auto* mine = chapters_.of(viewer))
            return mine->id == b.chapter;
    return false;
}

std::string Game::shelfKind(const std::string& viewer, const books::Book& b) const
{
    // Which shelf a book sits on for this viewer (the user's filters): a DM's world storyline; their Chapter's (shared
    // with it, or mostly its wolves); one of their circles'; a friend's; or, with no tie to its other wolves, other.
    if (!b.storyline.empty())
        return "world";
    const auto* mine = chapters_.of(viewer);
    if (mine)
    {
        if (b.sharing == "chapter" && b.chapter == mine->id)
            return "chapter";
        int same = 0;
        for (const auto& w : b.wolves)
            if (const auto* theirs = chapters_.of(w); theirs && theirs->id == mine->id)
                ++same;
        if (same * 2 > int(b.wolves.size()))
            return "chapter";
    }
    if (b.sharing == "circle")
        if (const auto c = circles_.find(b.circle); c != circles_.end() && c->second.members.count(accountKey(viewer)))
            return "circle";
    for (const auto& w : b.wolves)
        if (w != viewer && !b.hidden.count(w) && areFriends(viewer, w))
            return "friend";
    return books::hasWolf(b, viewer) ? "other" : "everyone";
}

json::Value Game::bookSpine(const std::string& viewer, const books::Book& b) const
{
    auto o = Value::object();
    o.add("id", b.id);
    o.add("title", b.title);
    o.add("kind", shelfKind(viewer, b));
    o.add("state", b.state);
    o.add("official", !b.story.empty());
    o.add("chapters", int(b.chapters.size()));
    o.add("last", b.last);
    if (b.state == "finished" && !b.flavour.empty())
        o.add("flavour", b.flavour);
    return o;
}

json::Value Game::bookView(const std::string& viewer, const books::Book& b) const
{
    // A book as this viewer may read it: names as it knows them, a hidden wolf left out (but to itself), a private
    // scene's summary only for its own wolves until the book is finished, and for its wolves what they may do.
    const bool mine = books::hasWolf(b, viewer);
    const auto name = [&](const std::string& who) { return who == viewer ? std::string("You") : names::capitalised(knownName(viewer, who)); };
    auto o = bookSpine(viewer, b);
    o.add("mine", mine);
    o.add("keeper", name(b.keeper));
    o.add("keeperIsMe", b.keeper == viewer);
    o.add("summary", b.summary);
    if (!b.summary.empty())
        o.add("summaryBy", b.summaryModel ? std::string("the model") : name(b.summaryBy));
    o.add("sharing", b.sharing);
    if (const auto c = circles_.find(b.circle); b.sharing == "circle" && c != circles_.end())
        o.add("circleName", c->second.name);
    if (const auto* ch = chapters_.byId(b.chapter); b.sharing == "chapter" && ch)
        o.add("chapterName", ch->name);
    if (!b.storyline.empty())
        o.add("storyline", b.storyline);
    auto wolves = Value::array();
    for (const auto& w : b.wolves)
        if (!b.hidden.count(w) || w == viewer)
            wolves.push(name(w));
    o.add("wolves", wolves);
    if (mine)
        o.add("hidden", b.hidden.count(viewer) > 0);
    auto chapters = Value::array();
    for (const auto& c : b.chapters)
    {
        auto j = Value::object();
        j.add("id", c.id);
        j.add("title", c.title);
        j.add("place", c.place);
        j.add("at", c.at);
        j.add("ended", c.ended);
        j.add("minutes", c.ended > c.at ? int((c.ended - c.at) / 60 + .5) : 0);
        j.add("private", c.privateScene);
        const bool readable = !c.privateScene || b.state == "finished" || std::find(c.wolves.begin(), c.wolves.end(), viewer) != c.wolves.end();
        if (readable)
        {
            j.add("summary", c.summary);
            if (!c.summary.empty())
                j.add("by", c.model ? std::string("the model") : name(c.summaryBy));
        }
        else
            j.add("sealed", true);                  // (A private scene: its summary once the book is finished.)
        auto in = Value::array();
        for (const auto& w : c.wolves)
            if (!b.hidden.count(w) || w == viewer)
                in.push(name(w));
        j.add("wolves", in);
        chapters.push(j);
    }
    o.add("chapterList", chapters);
    if (b.state == "finishing")
    {
        auto f = Value::object();
        const auto active = books::recentlyActive(b, now());
        int yes = 0;
        for (const auto& w : active)
            yes += b.agreed.count(w);
        f.add("agreed", yes);
        f.add("of", int(active.size()));
        f.add("youAgreed", b.agreed.count(viewer) > 0);
        f.add("objected", !b.objected.empty());
        f.add("quietLeft", std::max(0.0, books::rules().quietDays * 86400.0 - (now() - b.finishProposed)));
        o.add("finishing", f);
    }
    if (books::hasPrivate(b))
        o.add("privateWarning", PrivateWarning);
    if (b.state == "finished")
        o.add("flavour", b.flavour);
    // Official: its Story, and whether this wolf has given its word.
    if (const auto st = social_.stories.find(b.story); st != social_.stories.end())
    {
        o.add("story", st->first);
        o.add("storyState", st->second.state);
        o.add("approved", st->second.approvals.count(viewer) > 0);
        o.add("starred", st->second.starred.count(viewer) > 0);
        // Told: a Story Star for one of the others who saw it through (one, as doc 32 has it).
        if (st->second.state == "closed" && !st->second.starred.count(viewer) && st->second.members.count(viewer))
        {
            auto targets = Value::array();
            for (const auto& m : st->second.members)
                if (m != viewer)
                {
                    auto t = Value::object();
                    t.add("id", m);
                    t.add("name", name(m));
                    targets.push(t);
                }
            o.add("starTargets", targets);
        }
    }
    // Its volume: the books linked to it (and theirs), as far as this viewer may read them.
    auto volume = Value::array();
    std::set<std::string> seen{b.id};
    std::vector<std::pair<std::string, std::string>> queue;
    for (const auto& l : b.links)
        queue.push_back({l.book, l.kind});
    while (!queue.empty() && volume.items().size() < 20)
    {
        const auto [id, kind] = queue.front();
        queue.erase(queue.begin());
        if (!seen.insert(id).second)
            continue;
        const auto other = books_.find(id);
        if (other == books_.end() || !canRead(viewer, other->second))
            continue;
        auto v = Value::object();
        v.add("id", id);
        v.add("title", other->second.title);
        v.add("kind", kind);
        v.add("flavour", other->second.flavour);
        volume.push(v);
        for (const auto& l : other->second.links)
            if (!seen.count(l.book))
                queue.push_back({l.book, kind == "related" ? std::string("related") : kind});
    }
    o.add("volume", volume);
    if (mine)
    {
        o.add("nextScene", b.nextScene.count(viewer) > 0);
        // Scenes this wolf was in, for the last seven days (as players are told; they are kept eight), not yet in it.
        auto scenes = Value::array();
        for (const auto& [sid, s] : social_.sessions)
        {
            if (!s.members.count(viewer) || (s.ended > 0 && now() - s.ended > books::rules().linkDays * 86400.0))
                continue;
            if (std::any_of(b.chapters.begin(), b.chapters.end(), [&](const books::Chapter& c) { return c.session == sid; }))
                continue;
            auto j = Value::object();
            j.add("session", sid);
            if (const auto* where = world_.cell(s.cell))
                j.add("place", where->name);
            j.add("at", s.started);
            j.add("ended", s.ended);
            j.add("fight", SocialLedger::isFight(s));
            auto with = Value::array();
            for (const auto& [w, m] : s.members)
                if (w != viewer)
                    with.push(names::capitalised(knownName(viewer, w)));
            j.add("with", with);
            scenes.push(j);
        }
        o.add("scenes", scenes);
        // Its finished books, to link this one to (once it is finished too).
        if (b.state == "finished")
        {
            auto mineFinished = Value::array();
            for (const auto& [id, other] : books_)
                if (id != b.id && other.state == "finished" && books::hasWolf(other, viewer))
                {
                    auto j = Value::object();
                    j.add("id", id);
                    j.add("title", other.title);
                    mineFinished.push(j);
                }
            o.add("finishedBooks", mineFinished);
        }
    }
    return o;
}

void Game::sendBook(Connection* c, const std::string& id)
{
    const auto it = books_.find(id);
    if (!c || it == books_.end() || !canRead(c->entityId, it->second))
        return;
    auto e = Value::object();
    e.add("type", "book");
    e.add("book", bookView(c->entityId, it->second));
    send(c, e);
}

void Game::sendShelf(Connection* c, const std::string& tab, const std::string& filter, int offset)
{
    // The bookshelf, newest first, forty spines at a time: the books this wolf is in (the shelf), or those it isn't in
    // that its friends, circles or Chapter share with it, or everyone may read (Unaffiliated); by the filter's shelf.
    if (!c || c->entityId.empty())
        return;
    const auto& viewer = c->entityId;
    std::vector<const books::Book*> found;
    for (const auto& [id, b] : books_)
    {
        const bool mine = books::hasWolf(b, viewer);
        if ((tab == "unaffiliated") == mine || !canRead(viewer, b))
            continue;
        if (!filter.empty() && filter != "all" && shelfKind(viewer, b) != filter)
            continue;
        found.push_back(&b);
    }
    std::sort(found.begin(), found.end(), [](const books::Book* a, const books::Book* b) { return a->last > b->last; });
    auto e = Value::object();
    e.add("type", "shelf");
    e.add("tab", tab);
    e.add("filter", filter);
    e.add("offset", offset);
    auto list = Value::array();
    for (std::size_t i = std::size_t(std::max(0, offset)); i < found.size() && list.items().size() < 40; ++i)
        list.push(bookSpine(viewer, *found[i]));
    e.add("books", list);
    e.add("more", offset + 40 < int(found.size()));
    // The books this wolf may add a scene to (for the scene card's ADD TO A STORY).
    auto open = Value::array();
    for (const auto& [id, b] : books_)
        if (b.state != "finished" && books::hasWolf(b, viewer))
        {
            auto j = Value::object();
            j.add("id", id);
            j.add("title", b.title);
            open.push(j);
        }
    e.add("open", open);
    send(c, e);
}

Result Game::linkScene(const std::string& who, books::Book& b, const std::string& session, int after)
{
    // A scene into a book (before, during or after: the user): one its wolf was in, open or ended within the seven days
    // players are told, at the place in the story the wolf chooses (newest last by default).
    const auto& r = books::rules();
    const auto it = social_.sessions.find(session);
    if (it == social_.sessions.end() || !it->second.members.count(who))
        return {false, "You weren't in that scene.", {}};
    const auto& s = it->second;
    if (s.ended > 0 && now() - s.ended > r.linkDays * 86400.0)
        return {false, "That scene is too long ago: scenes can be added for " + std::to_string(r.linkDays) + " days.", {}};
    if (b.state == "finished")
        return {false, "That book is finished.", {}};
    if (std::any_of(b.chapters.begin(), b.chapters.end(), [&](const books::Chapter& c) { return c.session == session; }))
        return {false, "That scene is in the book already.", {}};
    if (int(b.chapters.size()) >= r.chapters)
        return {false, "That book is as long as a book gets.", {}};
    books::Chapter c;
    c.id = "ch-" + guid().substr(0, 12);
    c.session = session;
    const auto* where = world_.cell(s.cell);
    c.place = where ? where->name : std::string("somewhere");
    c.title = people::clean("At " + c.place, std::size_t(r.chapterTitle));
    c.at = s.started;
    c.ended = s.ended;
    c.privateScene = s.openness == "private";
    c.linkedBy = who;
    for (const auto& [w, m] : s.members)
        c.wolves.push_back(w);
    const int at = std::clamp(after, -1, int(b.chapters.size()) - 1);   // (After that chapter; -1: before the first.)
    b.chapters.insert(b.chapters.begin() + (at + 1), c);
    for (const auto& w : c.wolves)
        if (!books::hasWolf(b, w) && int(b.wolves.size()) < r.wolves)
            b.wolves.push_back(w);
    b.last = now();
    std::string note;
    // Official: the scene carries its Story on, where the ledger's rules allow (an ended scene that paid two of them).
    if (social_.stories.count(b.story) && s.ended > 0)
        if (const auto carried = social_.extend(b.keeper, b.story, session, now()); !carried.ok)
            note = " (It doesn't count toward the official Story: " + carried.message + ")";
    saveSoon();
    return {true, "The scene is in \"" + b.title + "\"." + note, {}};
}

void Game::booksSceneEnded(const std::string& member, const SocialSession& s)
{
    // A scene ending: its chapters learn when, and who was in it; one flagged "my next scene goes into …" is linked.
    for (auto& [id, b] : books_)
    {
        for (auto& c : b.chapters)
            if (c.session == s.id)
            {
                c.ended = s.ended > 0 ? s.ended : now();
                c.wolves.clear();
                for (const auto& [w, m] : s.members)
                    c.wolves.push_back(w);
            }
        if (b.nextScene.erase(member))
            if (const auto linked = linkScene(member, b, s.id, int(b.chapters.size()) - 1); linked.ok)
                if (auto* c = clientOf(member))
                    system(c, linked.message);
    }
}

void Game::syncStory(const books::Book& b)
{
    // An official book's Story carries on with every chapter whose scene has ended, once it is under way (the ledger
    // takes scenes only into an agreed Story), as far as its rules allow.
    const auto st = social_.stories.find(b.story);
    if (st == social_.stories.end() || st->second.state != "active")
        return;
    for (const auto& c : b.chapters)
        if (c.ended > 0 && std::find(st->second.scenes.begin(), st->second.scenes.end(), c.session) == st->second.scenes.end())
            social_.extend(b.keeper, b.story, c.session, now());
}

void Game::finishBook(books::Book& b)
{
    // Finished: no more chapters; a line or two of flavour for its spine; an official book's Story told (it pays).
    b.state = "finished";
    b.finished = now();
    b.last = now();
    std::string first, last;
    if (!b.chapters.empty())
        first = b.chapters.front().place, last = b.chapters.back().place;
    b.flavour = people::clean(b.title + ": a story in " + std::to_string(b.chapters.size()) +
                                  (b.chapters.size() == 1 ? " chapter" : " chapters") +
                                  (first.empty() ? std::string(".") : first == last ? ", at " + first + "." : ", from " + first + " to " + last + "."),
                              std::size_t(books::rules().flavour));
    if (social_.stories.count(b.story))
    {
        syncStory(b);
        social_.close(b.keeper, b.story, now());
        afterSocial();
    }
    for (const auto& w : b.wolves)
        if (auto* c = clientOf(w))
            system(c, "\"" + b.title + "\" is finished.");
    const auto& r = books::rules();
    const auto day = std::int64_t(now() / 86400);
    auto& today = modelBooks_[b.keeper];
    if (today.first != day)
        today = {day, 0};
    if (mind_.live() && today.second < r.modelADay)
    {
        ++today.second;
        std::vector<std::pair<std::string, std::string>> chapters;
        for (const auto& c : b.chapters)
            chapters.push_back({c.title, c.summary});
        std::weak_ptr<bool> alive = alive_;
        const auto id = b.id;
        mind_.book("flavour", b.title, chapters, [this, alive, id](const std::string& text) {
            if (alive.expired() || text.empty())
                return;
            if (const auto it = books_.find(id); it != books_.end())
            {
                it->second.flavour = people::clean(text, std::size_t(books::rules().flavour), true);
                saveSoon();
            }
        });
    }
    saveSoon();
}

void Game::tendBooks()
{
    for (auto& [id, b] : books_)
        if (books::finishes(b, now()))
            finishBook(b);
}

bool Game::bookCommand(Connection* c, const json::Value& j, Result& result)
{
    // {"type": "book", "verb": "start" | "link" | "next" | "title" | "summary" | "chapter" | "summarise" | "move" |
    //  "remove" | "share" | "hide" | "finish" | "agree" | "object" | "official" | "approve" | "volume" | "open" | "shelf",
    //  "book", "session", "after", "chapter", "to", "title", "text", "sharing", "circle", "other", "kind", "on",
    //  "tab", "filter", "offset"}.
    const auto& id = c->entityId;
    if (id.empty())
        return false;
    const auto verb = j.string("verb");
    const auto& r = books::rules();
    if (verb == "shelf")
    {
        sendShelf(c, j.string("tab", "shelf"), j.string("filter", "all"), int(j.number("offset")));
        result = {true, {}, {}};
        return true;
    }
    if (verb == "start")
    {
        const auto title = people::clean(j.string("title"), std::size_t(r.title));
        int open = 0;
        for (const auto& [bid, b] : books_)
            open += b.state != "finished" && b.keeper == id;
        if (title.empty())
            result = {false, "A book needs a title.", {}};
        else if (open >= r.openBooks)
            result = {false, "You keep " + std::to_string(r.openBooks) + " open books already.", {}};
        else
        {
            books::Book b;
            b.id = "book-" + guid().substr(0, 16);
            b.title = title;
            b.keeper = id;
            b.wolves = {id};
            b.created = b.last = now();
            auto& kept = books_[b.id] = b;
            result = {true, "You begin \"" + title + "\".", {}};
            if (!j.string("session").empty())
            {
                const auto linked = linkScene(id, kept, j.string("session"), 0);
                result.message += linked.ok ? " " + linked.message : " (" + linked.message + ")";
            }
            saveSoon();
            sendBook(c, b.id);
        }
        return true;
    }
    const auto it = books_.find(j.string("book"));
    if (it == books_.end() || !canRead(id, it->second))
    {
        result = {false, "No such book.", {}};
        return true;
    }
    auto& b = it->second;
    if (verb == "open")
    {
        sendBook(c, b.id);
        result = {true, {}, {}};
        return true;
    }
    const bool mine = books::hasWolf(b, id);
    const bool keeper = b.keeper == id;
    const auto chapter = std::find_if(b.chapters.begin(), b.chapters.end(), [&](const books::Chapter& ch) { return ch.id == j.string("chapter"); });
    if (!mine)
        result = {false, "You aren't in that book.", {}};
    else if (verb == "link")
        result = linkScene(id, b, j.string("session"), j.has("after") ? int(j.number("after")) : int(b.chapters.size()) - 1);
    else if (verb == "next")
    {
        if (j.boolean("on"))
            b.nextScene.insert(id);
        else
            b.nextScene.erase(id);
        result = {true, j.boolean("on") ? "Your next scene goes into \"" + b.title + "\"." : "Not your next scene, then.", {}};
    }
    else if (verb == "title")
    {
        const auto title = people::clean(j.string("title"), std::size_t(r.title));
        result = !keeper ? Result{false, "Only its keeper renames it.", {}} : title.empty() ? Result{false, "A book needs a title.", {}}
                                                                                            : Result{true, "Renamed.", {}};
        if (result.ok)
            b.title = title;
    }
    else if (verb == "summary")
    {
        b.summary = people::clean(j.string("text"), std::size_t(r.bookSummary), true);
        b.summaryBy = id;
        b.summaryModel = false;
        result = {true, "The book's summary is saved.", {}};
    }
    else if (verb == "chapter" || verb == "summarise" || verb == "move" || verb == "remove")
    {
        if (chapter == b.chapters.end() && !(verb == "summarise" && j.string("chapter").empty()))
            result = {false, "No such chapter.", {}};
        else if (verb == "chapter")
        {
            if (j.has("title"))
                chapter->title = people::clean(j.string("title"), std::size_t(r.chapterTitle));
            if (j.has("text"))
            {
                chapter->summary = people::clean(j.string("text"), std::size_t(r.summary), true);
                chapter->summaryBy = id;
                chapter->model = false;
            }
            result = {true, "The chapter is saved.", {}};
        }
        else if (verb == "summarise" && chapter != b.chapters.end())
        {
            // From this wolf's own recap of that scene (doc 50): what it perceived, by the model or from the ledger.
            const auto mineRecaps = recaps_.find(id);
            const people::Recap* recap = nullptr;
            if (mineRecaps != recaps_.end())
                for (const auto& rc : mineRecaps->second)
                    if (rc.session == chapter->session)
                        recap = &rc;
            if (!recap)
                result = {false, "You have no recap of that scene to draw on; write its summary instead.", {}};
            else
            {
                chapter->summary = people::clean(recap->text, std::size_t(r.summary), true);
                chapter->summaryBy = id;
                chapter->model = recap->model;
                result = {true, "The chapter's summary is drawn from your recap of it.", {}};
                // With chapters before it, the model tells it again as the next of them, so the story carries on
                // instead of describing the same wolves again (the user), within today's limit.
                std::vector<std::pair<std::string, std::string>> chapters;
                for (auto c = b.chapters.begin(); c != chapter; ++c)
                    if (!c->summary.empty() && (!c->privateScene || b.state == "finished" ||
                                                std::find(c->wolves.begin(), c->wolves.end(), id) != c->wolves.end()))
                        chapters.push_back({c->title, veilFor(id, c->summary)});
                const auto day = std::int64_t(now() / 86400);
                auto& today = modelBooks_[id];
                if (today.first != day)
                    today = {day, 0};
                if (!chapters.empty() && recap->model && mind_.live() && today.second < r.modelADay)
                {
                    ++today.second;
                    chapters.push_back({chapter->title, chapter->summary});
                    std::weak_ptr<bool> alive = alive_;
                    const auto bookId = b.id, chapterId = chapter->id, who = id, drawn = chapter->summary;
                    mind_.book("chapter", b.title, chapters, [this, alive, bookId, chapterId, who, drawn](const std::string& text) {
                        if (alive.expired() || text.empty())
                            return;
                        const auto found = books_.find(bookId);
                        if (found == books_.end())
                            return;
                        for (auto& c : found->second.chapters)
                            if (c.id == chapterId && c.summary == drawn)   // (Unless someone has written it since.)
                            {
                                c.summary = people::clean(veilFor(who, text), std::size_t(books::rules().summary), true);
                                c.model = true;
                                saveSoon();
                                if (auto* to = clientOf(who))
                                    sendBook(to, bookId);
                            }
                    });
                    result = {true, "The chapter's summary is drawn from your recap of it; the model is telling it on from the chapters before.", {}};
                }
            }
        }
        else if (verb == "summarise")
        {
            // The book's summary, by the model from its chapters, within today's limit; else from their titles.
            std::vector<std::pair<std::string, std::string>> chapters;
            std::string titles;
            for (const auto& ch : b.chapters)
            {
                chapters.push_back({ch.title, ch.privateScene && b.state != "finished" ? std::string() : ch.summary});
                titles += (titles.empty() ? "" : "; then ") + ch.title;
            }
            const auto day = std::int64_t(now() / 86400);
            auto& today = modelBooks_[id];
            if (today.first != day)
                today = {day, 0};
            if (chapters.empty())
                result = {false, "The book has no chapters yet.", {}};
            else if (mind_.live() && today.second < r.modelADay)
            {
                ++today.second;
                std::weak_ptr<bool> alive = alive_;
                const auto bookId = b.id;
                const auto who = id;
                mind_.book("summary", b.title, chapters, [this, alive, bookId, who](const std::string& text) {
                    if (alive.expired())
                        return;
                    const auto found = books_.find(bookId);
                    if (found == books_.end() || text.empty())
                        return;
                    found->second.summary = people::clean(text, std::size_t(books::rules().bookSummary), true);
                    found->second.summaryBy = who;
                    found->second.summaryModel = true;
                    saveSoon();
                    if (auto* to = clientOf(who))
                        sendBook(to, bookId);
                });
                result = {true, "The model is writing the book's summary.", {}};
            }
            else
            {
                b.summary = people::clean(b.title + " tells of " + titles + ".", std::size_t(r.bookSummary), true);
                b.summaryBy = id;
                b.summaryModel = false;
                result = {true, "The book's summary is written from its chapters' titles.", {}};
            }
        }
        else if (verb == "move")
        {
            const auto ch = *chapter;
            b.chapters.erase(chapter);
            const int to = std::clamp(int(j.number("to")), 0, int(b.chapters.size()));
            b.chapters.insert(b.chapters.begin() + to, ch);
            result = {true, "Moved.", {}};
        }
        else
        {
            if (b.state == "finished" || (!keeper && chapter->linkedBy != id))
                result = {false, b.state == "finished" ? "That book is finished." : "Only its keeper or whoever added it takes a chapter out.", {}};
            else
            {
                b.chapters.erase(chapter);
                result = {true, "The chapter is taken out.", {}};
            }
        }
    }
    else if (verb == "share")
    {
        const auto sharing = j.string("sharing");
        if (!keeper)
            result = {false, "Only its keeper chooses who may read it.", {}};
        else if (!books::validSharing(sharing))
            result = {false, "Members, friends, a circle, your Chapter or everyone.", {}};
        else if (sharing == "circle" && (!circles_.count(j.string("circle")) || !circles_.at(j.string("circle")).members.count(accountKey(id))))
            result = {false, "One of your circles.", {}};
        else if (sharing == "chapter" && !chapters_.of(id))
            result = {false, "You aren't in a Chapter.", {}};
        else
        {
            b.sharing = sharing;
            b.circle = sharing == "circle" ? j.string("circle") : std::string();
            b.chapter = sharing == "chapter" ? chapters_.of(id)->id : std::string();
            result = {true, "Shared with " + std::string(sharing == "members" ? "its wolves only" : sharing == "friends" ? "friends"
                                                       : sharing == "circle" ? "the circle" : sharing == "chapter" ? "your Chapter" : "everyone") + ".", {}};
        }
    }
    else if (verb == "hide")
    {
        if (j.boolean("on"))
            b.hidden.insert(id);
        else
            b.hidden.erase(id);
        result = {true, j.boolean("on") ? "You are hidden from its readers." : "You are shown in it again.", {}};
    }
    else if (verb == "finish" || verb == "agree" || verb == "object")
    {
        const std::string warning = books::hasPrivate(b) ? std::string(" ") + PrivateWarning : std::string();
        if (b.state == "finished")
            result = {false, "That book is finished.", {}};
        else if (verb == "finish")
        {
            if (!keeper)
                result = {false, "Its keeper proposes finishing it.", {}};
            else if (b.chapters.empty())
                result = {false, "A book needs a chapter before it can be finished.", {}};
            else
            {
                b.state = "finishing";
                b.finishProposed = now();
                b.agreed = {id};
                b.objected.clear();
                result = {true, "You propose to finish \"" + b.title + "\"." + warning, {}};
                for (const auto& w : books::recentlyActive(b, now()))
                    if (w != id)
                        if (auto* other = clientOf(w))
                            system(other, names::capitalised(knownName(w, id)) + " proposes to finish \"" + b.title + "\": agree or object in STORIES." + warning);
            }
        }
        else if (b.state != "finishing")
            result = {false, "No one has proposed finishing it.", {}};
        else if (verb == "agree")
        {
            b.agreed.insert(id);
            b.objected.erase(id);
            result = {true, "You agree to finish it." + warning, {}};
        }
        else
        {
            b.objected.insert(id);
            b.agreed.erase(id);
            result = {true, "You object: it needs a majority of its recent wolves to finish now.", {}};
        }
        if (result.ok && books::finishes(b, now()))
            finishBook(b);
    }
    else if (verb == "official" || verb == "approve")
    {
        // Official (books are the one kind of Story): the ledger's Story underneath, with its rules and pay.
        if (verb == "official")
        {
            const auto first = std::find_if(b.chapters.begin(), b.chapters.end(), [&](const books::Chapter& ch) { return ch.ended > 0; });
            if (!keeper)
                result = {false, "Its keeper proposes making it official.", {}};
            else if (social_.stories.count(b.story))
                result = {false, "It is official already.", {}};
            else if (first == b.chapters.end())
                result = {false, "It needs a chapter whose scene has ended.", {}};
            else
            {
                const auto r2 = social_.propose(id, first->session, people::clean(b.title, 60), now());
                if (!r2.ok)
                    result = {false, r2.message, {}};
                else
                {
                    b.story = r2.message;
                    for (const auto& ch : b.chapters)
                        if (ch.session != first->session && ch.ended > 0)
                            social_.extend(id, b.story, ch.session, now());
                    result = {true, "You propose making it official: two thirds of its wolves must agree within a day.", {}};
                    if (const auto st = social_.stories.find(b.story); st != social_.stories.end())
                        for (const auto& m : st->second.members)
                            if (m != id)
                                if (auto* other = clientOf(m))
                                    system(other, names::capitalised(knownName(m, id)) + " would make \"" + b.title + "\" an official Story: agree in STORIES.");
                }
            }
        }
        else
        {
            const auto r2 = social_.approve(id, b.story, now());
            result = {r2.ok, r2.ok ? "You give your word." : r2.message, {}};
            if (r2.ok)
                syncStory(b);
        }
        if (result.ok)
            afterSocial();
    }
    else if (verb == "volume")
    {
        const auto other = books_.find(j.string("other"));
        const auto kind = j.string("kind");
        if (other == books_.end() || other->first == b.id || !canRead(id, other->second))
            result = {false, "No such book to link.", {}};
        else if (b.state != "finished" || other->second.state != "finished")
            result = {false, "Only finished books link into a volume.", {}};
        else if (!books::validLink(kind))
            result = {false, "Related, sequel or prequel.", {}};
        else
        {
            const auto has = [](const books::Book& x, const std::string& to) {
                return std::any_of(x.links.begin(), x.links.end(), [&](const books::Link& l) { return l.book == to; });
            };
            if (!has(b, other->first))
                b.links.push_back({other->first, kind});
            if (!has(other->second, b.id))
                other->second.links.push_back({b.id, books::inverse(kind)});
            result = {true, "\"" + other->second.title + "\" is its " + kind + ".", {}};
        }
    }
    else
        return false;
    if (result.ok)
    {
        b.last = now();
        saveSoon();
    }
    sendBook(c, b.id);
    return true;
}

Result Game::tieBookToStoryline(const std::string& book, const std::string& storyline, const std::string& by)
{
    // The DM's: a book tied to a world storyline (the World shelf), or untied with "".
    const auto it = books_.find(book);
    if (it == books_.end())
        return {false, "No such book.", {}};
    it->second.storyline = people::clean(storyline, 120);
    saveSoon();
    note("info", "RATW_BOOK_STORYLINE " + book + " '" + it->second.storyline + "' by " + by);
    return {true, it->second.storyline.empty() ? "Untied." : "Tied to " + it->second.storyline + ".", {}};
}

void Game::booksSave(json::Value& root) const
{
    auto list = Value::array();
    for (const auto& [id, b] : books_)
        list.push(books::save(b));
    root.add("books", list);
}

void Game::booksLoad(const json::Value& saved)
{
    books_.clear();
    for (const auto& e : saved.array("books"))
    {
        auto b = books::load(e);
        if (b.id.empty() || b.title.empty() || !characters_.count(b.keeper))
            continue;
        b.wolves.erase(std::remove_if(b.wolves.begin(), b.wolves.end(), [&](const std::string& w) { return !characters_.count(w); }), b.wolves.end());
        books_[b.id] = std::move(b);
    }
}
} // namespace ratw::game
