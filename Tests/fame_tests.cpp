// Fame and memory (Docs/Design/56-fame-and-memory.md). Phase 1, the deed ledger: a camp broken makes one notable deed;
// its witnesses hold it with the name they knew the doer by (or by look), and warm to the doer once; merchants in other
// towns hear nothing; a promise kept makes a small deed and a "keeps promises" rumour; capped kinds stop; a DM award and
// a revoke; all of it across a restart.
#include "RatwGame.h"
#include "RatwWorld.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>

using namespace ratw;
namespace fs = std::filesystem;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

#include "town_fixture.h"

const Belief* beliefOf(World& w, const std::string& holder, const std::string& claim)
{
    if (const auto* mine = w.beliefsOf(holder))
        for (const auto& b : *mine)
            if (b.claim == claim)
                return &b;
    return nullptr;
}

void say(Town& t, Client& c, const std::string& text, const std::string& to)
{
    auto command = parsed(cmd({{"type", "chat"}, {"text", text}, {"channel", "ic"}, {"volume", "speak"}}));
    auto targets = json::Value::array();
    if (!to.empty())
        targets.push(to);                           // (Addressed, it answers; else it only overhears.)
    command.add("targets", targets);
    t.g.command(&c, json::dump(command));
    t.g.settle();
    t.run(1);
}

void ledger()
{
    const auto root = fs::temp_directory_path() / ("ratw-fame-" + std::to_string(::getpid()));
    const auto save = (fs::temp_directory_path() / ("ratw-fame-" + std::to_string(::getpid()) + ".json")).string();
    fs::remove(save);
    const auto world = writeStrip(root);
    std::string camp, award;
    {
        Town t(world, save);
        auto& w = t.g.world();
        t.enter(t.ash, 1, "ash", "Ash", 1, 9.5);
        t.enter(t.bo, 2, "bo", "Bo", 1, 13.5);
        // The stall's keeper learns her name; the innkeeper, out of earshot, doesn't.
        w.entity("ui")->cellId = cellId(4);
        say(t, t.ash, "Good day. I'm Ash.", "um");
        t.place(t.ash, 1, 10.5);
        auto* um = w.entity("um");
        auto* ui = w.entity("ui");
        um->cellId = ui->cellId = cellId(1);
        um->position = {9.5, 8.5};
        ui->position = {12.5, 8.5};
        t.run(.5);
        const double liking = w.bonds().find("um", t.ash.entityId) ? w.bonds().find("um", t.ash.entityId)->affinity : 0;
        // She breaks a camp (the world's event: World::clearCamp records it).
        w.recordEvent({"camp cleared", t.ash.entityId, "camp-1", cellId(1), 0, 0, {}, 0, 0, "by Ash"});
        const auto mine = t.g.deeds().byDoer(t.ash.entityId);
        expect(mine.size() == 1 && mine[0]->kind == "broke_camp" && mine[0]->weight == fame::Notable, "one notable deed");
        camp = mine[0]->id;
        const auto& deed = *mine[0];
        const auto witness = [&](const std::string& id) -> const fame::Witness* {
            for (const auto& wi : deed.witnesses)
                if (wi.id == id)
                    return &wi;
            return nullptr;
        };
        expect(witness("um") && witness("um")->as.at(t.ash.entityId) == "Ash", "the stall's keeper saw it, and knew her as Ash");
        expect(witness("ui") && witness("ui")->as.at(t.ash.entityId).empty(),
               "the innkeeper saw it, by look only: " + (witness("ui") ? witness("ui")->as.at(t.ash.entityId) : std::string("not a witness")));
        expect(!deed.looks.at(t.ash.entityId).empty() && deed.names.at(t.ash.entityId) == std::vector<std::string>{"Ash"},
               "her look then, and the name it travels under: " + deed.looks.at(t.ash.entityId));
        const auto* heard = beliefOf(w, "um", "deed:" + camp);
        expect(heard && heard->as == "Ash" && heard->confidence >= .85, "the keeper believes it, of Ash");
        heard = beliefOf(w, "ui", "deed:" + camp);
        expect(heard && heard->as.empty(), "the innkeeper believes it, of a wolf it knows by look");
        expect(!beliefOf(w, "sm", "deed:" + camp) && !beliefOf(w, "rm", "deed:" + camp), "merchants in other towns hear nothing yet");
        expect(w.bonds().find("um", t.ash.entityId) && w.bonds().find("um", t.ash.entityId)->affinity >= liking + 4.9,
               "a notable deed warms the keeper to her, once");
        // A promise kept: a small deed, and the rumour that she keeps them.
        w.promise(t.ash.entityId, "um", "bring a pot of honey", 3);
        w.recordEvent({"gift", t.ash.entityId, "um", cellId(1), 0, 0, "honey", 1, 0, {}});
        bool kept = false;
        for (const auto* d : t.g.deeds().byDoer(t.ash.entityId))
            kept |= d->kind == "kept_promise" && d->beneficiary == "um";
        expect(kept, "a promise kept makes a small deed");
        expect(beliefOf(w, "um", "keeps promises"), "and the rumour that she keeps her word forms (the event is recorded now)");
        // Capped: a fourth tending of one wolf in a season makes no deed.
        int tended = 0;
        for (int i = 0; i < 5; ++i)
            tended += !t.g.recordDeed("tended", {t.ash.entityId}, "ui", cellId(1), "test").empty();
        expect(tended == 3, "three tendings of one resident a season, then no more deeds: " + std::to_string(tended));
        // A DM award (great), and a revoke that drops its beliefs.
        award = t.g.recordDeed("award", {t.bo.entityId}, "town:" + w.lawTown(cellId(1)), cellId(1), "dm", "held the gate against the storm", fame::Great);
        expect(!award.empty() && t.g.deeds().find(award)->weight == fame::Great, "a DM's award");
        const auto doomed = t.g.recordDeed("award", {t.bo.entityId}, {}, cellId(1), "dm", "a mistake", fame::Great);
        expect(beliefOf(w, "um", "deed:" + doomed) != nullptr, "believed until revoked");
        expect(t.g.revokeDeed(doomed) && !beliefOf(w, "um", "deed:" + doomed) && t.g.deeds().find(doomed)->revoked, "revoked: no one holds it");
        expect(t.g.recordDeed("no_such_kind", {t.ash.entityId}, {}, cellId(1), "test").empty(), "an unknown kind makes nothing");
        // Her chronicle (Phase 5), with no database: from what is in memory, and said to be partial.
        t.ash.events.clear();
        t.g.command(&t.ash, cmd({{"type", "social"}, {"verb", "chronicle"}}));
        t.g.settle();
        const auto* page = t.ash.last("chronicle");
        std::string told;
        if (page)
            for (const auto& e : page->array("entries"))
                told += e.string("text") + "\n";
        expect(page && page->boolean("partial") && contains(told, "You drove the bandits off the road") && contains(told, "Year 1:"),
               "her chronicle: " + told);
        t.g.command(&t.ash, cmd({{"type", "social"}, {"verb", "chronicle"}}));
        t.g.settle();
        expect(contains(t.ash.said(), "being written"), "once in 30 seconds");
        t.g.save();
    }
    {
        Town t(world, save);
        t.enter(t.ash, 1, "ash", "Ash", 1, 9.5);
        const auto* d = t.g.deeds().find(camp);
        expect(d && d->kind == "broke_camp" && d->witnesses.size() >= 2 && t.g.deeds().find(award), "the deeds survive a restart");
        const auto* heard = beliefOf(t.g.world(), "um", "deed:" + camp);
        expect(heard && heard->as == "Ash", "and the beliefs, with the name they were held under");
    }
    fs::remove_all(root);
    fs::remove(save);
}

// Phase 2, deeds travel and are recognised: a notable deed's town word grows from a quarter to all in 3 game days, each
// resident answering the same every time; recognised by a name it travels under, by a distinctive look but not a plain
// one, and not once the look has changed; joining up at an introduction; an alias doesn't connect; a great deed reaches
// the next town only with a caravan, a notable one only with a guarded one; deed talk names the doer as the teller knows
// them, and gossip never speaks a name the teller wasn't given; the game's greeting by the deed.
void travels()
{
    const auto root = fs::temp_directory_path() / ("ratw-fame2-" + std::to_string(::getpid()));
    const auto world = writeStrip(root);
    Town t(world, {}, 120, true);
    auto& w = t.g.world();
    t.enter(t.ash, 1, "ash", "Ash", 1, 10.5);
    const auto ash = t.ash.entityId;
    const auto upper = w.lawTown(cellId(1)), ferro = w.lawTown(cellId(4));
    const auto put = [&](const char* who, int cell, double x, double y = 8.5) {
        auto* e = w.entity(who);
        e->cellId = cellId(cell);
        e->position = {x, y};
        e->path.clear();
        e->velocity = {};
    };
    const auto meet = [&](const char* who, const std::string& name, bool addressed = true) {
        const auto* e = w.entity(who);
        const auto cell = e->cellId;
        const auto pos = e->position;
        const auto* me = w.entity(ash);
        const auto mine = me->cellId;
        const auto at = me->position;
        put(who, 1, 11.5);
        t.place(t.ash, 1, 10.5);
        t.run(.5);
        say(t, t.ash, "Good day. I'm " + name + ".", addressed ? std::string(who) : std::string());
        auto* back = w.entity(who);
        back->cellId = cell;
        back->position = pos;
        t.place(t.ash, std::stoi(mine.substr(2)), at.x, at.y);
    };
    const auto marked = [&](bool on) {
        auto* e = w.entity(ash);
        e->appearance->markings.clear();
        if (on)
            e->appearance->markings.push_back({"blaze", "#f2f2f2", .95});
        t.run(1.5);                                 // (Labels are worked out once a second.)
    };
    // Everyone of Upper Accord away at home (c_0), so nobody witnesses but whom we place.
    for (const char* who : {"u1", "u2", "u3", "um", "ui"})
        put(who, 0, 3.5 + 2 * (who[1] - '0' > 0 ? who[1] - '0' : 4), 4.5);
    meet("um", "Ash");                              // The keeper knows her as Ash: deeds it sees travel under the name.
    meet("u1", "Ash");                              // u1 knows her too, but sees nothing.
    // A plain-looking wolf's notable deed, seen by the keeper.
    put("um", 1, 11.5);
    t.run(.5);
    const auto a = t.g.recordDeed("broke_camp", {ash}, "town:" + upper, cellId(1), "test");
    put("um", 0, 9.5, 4.5);
    const auto* deedA = t.g.deeds().find(a);
    expect(deedA && deedA->names.at(ash) == std::vector<std::string>{"Ash"} && deedA->towns.size() == 1 && deedA->towns[0].town == upper,
           "a notable deed: in its town's word, travelling under the name the keeper knew");
    expect(std::abs(fame::reach(deedA->towns[0], fame::Notable, deedA->day) - .25) < 1e-9 &&
               std::abs(fame::reach(deedA->towns[0], fame::Notable, deedA->day + 3) - 1) < 1e-9,
           "its reach: a quarter at once, all over town in 3 game days");
    w.advanceCalendar(3.1);
    t.run(1.5);
    const auto u1 = t.g.fameBriefing("u1", ash, false);
    expect(contains(u1, "this wolf drove the bandits off the road") && u1 == t.g.fameBriefing("u1", ash, false),
           "u1, who never saw it, has heard and ties it to Ash by name, the same every time: " + u1);
    expect(t.g.fameBriefing("u2", ash, false).empty(), "u2, who knows no name, can't tie a plain-looking wolf to it");
    // A distinctive look: a resident who knows no name thinks it might be her.
    marked(true);
    const auto b = t.g.recordDeed("report_thief", {ash}, {}, cellId(1), "test");
    w.advanceCalendar(3.1);
    t.run(1.5);
    const auto u2 = t.g.fameBriefing("u2", ash, false);
    expect(contains(u2, "you think it might be them"), "by a distinctive look, unsure: " + u2);
    marked(false);
    expect(!contains(t.g.fameBriefing("u2", ash, false), "might be them"), "the look changed: it no longer matches");
    marked(true);
    // Joining up: told her name, u3 ties the deed to it, and is briefed once that it has just realised.
    meet("u3", "Ash");
    const auto* deedB = t.g.deeds().find(b);
    expect(std::find(deedB->names.at(ash).begin(), deedB->names.at(ash).end(), "Ash") != deedB->names.at(ash).end(),
           "the deed travels under her name now");
    // (Its reply to the introduction was briefed that it has just realised; after that, it remembers it said so.)
    const auto after = t.g.fameBriefing("u3", ash);
    expect(!contains(after, "just realised") && contains(after, "helped the watch") && contains(after, "spoken of it to them before"),
           "u3 has realised who she is, and spoke of it once: " + after);
    // An alias doesn't connect a deed done under another name (and a plain look can't).
    marked(false);
    t.g.command(&t.ash, cmd({{"type", "names"}, {"verb", "add"}, {"name", "Kestrel"}}));
    t.g.settle();
    put("um", 1, 11.5);
    t.run(.5);
    const auto c = t.g.recordDeed("broke_camp", {ash}, "town:" + upper, cellId(1), "test");
    put("um", 0, 9.5, 4.5);
    meet("ui", "Kestrel");
    w.advanceCalendar(3.1);
    t.run(1.5);
    expect(!contains(t.g.fameBriefing("ui", ash, false), "drove"), "known as Kestrel, the innkeeper can't tie Ash's deeds to her");
    (void)c;
    // Between towns: a great deed with any caravan; a notable one only with a guarded one.
    meet("sm", "Ash");
    const auto g = t.g.recordDeed("award", {ash}, "town:" + upper, cellId(1), "dm", "held the bridge in the flood", fame::Great);
    const auto inTown = [&](const std::string& id, const std::string& town) {
        for (const auto& word : t.g.deeds().find(id)->towns)
            if (word.town == town)
                return word.carrier;
        return std::string();
    };
    // (The keeper's own belief of deed A may reach Ser Ferro's merchants with the carters, as any rumour does; the town's
    // word is what crosses only by caravan.)
    expect(inTown(g, ferro).empty() && !contains(t.g.fameBriefing("sm", ash, false), "held the bridge"),
           "Ser Ferro hasn't heard: no caravan yet: " + t.g.fameBriefing("sm", ash, false));
    w.recordEvent({"caravan arrives", "car-1", ferro, cellId(4), 0, 0, {}, 0, 0, "from " + upper});
    expect(inTown(g, ferro) == "caravan", "a caravan from Upper Accord carries the great deed");
    w.advanceCalendar(6.1);
    t.run(1.5);
    expect(contains(t.g.fameBriefing("sm", ash, false), "held the bridge"), "a caravan carried it, and Ser Ferro has heard");
    const auto* notable = t.g.deeds().find(a);
    bool inFerro = false;
    for (const auto& word : notable->towns)
        inFerro |= word.town == ferro;
    expect(!inFerro, "the notable deed didn't cross with an unguarded caravan");
    w.recordEvent({"contract done", ash, "sm", cellId(4), 0, 0, {}, 0, 5, "escort: to Ser Ferro"});
    w.recordEvent({"caravan arrives", "car-2", ferro, cellId(4), 0, 0, {}, 0, 0, "from " + upper});
    for (const auto& word : t.g.deeds().find(a)->towns)
        inFerro |= word.town == ferro;
    expect(inFerro, "a caravan she guarded carries it");
    // Talk: the keeper (who knows her as Ash) sees a fresh deed and tells the innkeeper (who doesn't), naming her as Ash.
    put("um", 1, 11.5);
    put("ui", 0, 12.5, 4.5);
    t.place(t.ash, 1, 10.5);
    t.run(.5);
    const auto fresh = t.g.recordDeed("festival_won", {ash}, "town:" + upper, cellId(1), "test", "the race at the Greening");
    expect(!fresh.empty(), "a fresh deed the keeper saw");
    t.run(30);                                      // (A quiet spell, so talk may start.)
    put("um", 1, 9.5, 9.5);
    put("ui", 1, 10.5, 9.5);
    t.place(t.ash, 1, 10.5, 11.5);
    bool told = false;
    for (const auto& pick : w.ambientPicks({ash}))
        if (pick.topic.kind == "deed" && pick.teller == "um")
            told = pick.topic.blanks.at("subject") == "Ash" && contains(pick.topic.blanks.at("deed"), "won the race");
    expect(told, "the keeper tells the innkeeper of the deed, naming her as Ash");
    // Gossip by a teller who was never told her name speaks her look, never her name.
    w.forgetClaim("deed:" + a);
    w.forgetClaim("deed:" + b);
    w.forgetClaim("deed:" + c);
    w.forgetClaim("deed:" + g);
    w.forgetClaim("deed:" + fresh);
    put("um", 0, 9.5, 4.5);
    t.run(30);
    put("ui", 1, 10.5, 9.5);
    put("u2", 1, 9.5, 9.5);
    w.believe("u2", ash, "keeps promises", "saw it", .95);
    bool leak = false, gossip = false;
    for (const auto& pick : w.ambientPicks({ash}))
        if (pick.topic.subject == ash)
        {
            gossip = true;
            for (const auto& f : pick.topic.facts)
                leak |= contains(f, "Ash");
            for (const auto& [k, v] : pick.topic.blanks)
                leak |= contains(v, "Ash");
        }
    expect(gossip && !leak, "gossip about her by a wolf who never heard her name names her by look");
    // The game's own greeting: u1 (who knows her as Ash) greets her by the deed.
    put("u2", 0, 5.5, 4.5);
    put("ui", 0, 12.5, 4.5);
    put("u1", 1, 11.5, 11.5);
    t.run(1);
    t.ash.events.clear();
    say(t, t.ash, "Hello.", "u1");
    std::string greeting;
    for (int i = 0; i < 80 && greeting.empty(); ++i)
    {
        t.run(.5);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));   // (Replies come from the dialogue thread.)
        for (const auto& e : t.ash.events)
            if (contains(e.string("text"), "Heard you") || contains(e.string("text"), "you drove") || contains(e.string("text"), "You drove") ||
                contains(e.string("text"), "held the bridge") || contains(e.string("text"), "helped the watch"))
                greeting = e.string("text");
    }
    std::string all;
    for (const auto& e : t.ash.events)
        all += json::dump(e).substr(0, 160) + " | ";
    expect(!greeting.empty(), "u1 greets her by a deed: " + all + " briefing: " + t.g.fameBriefing("u1", ash, false));
    fs::remove_all(root);
}

// Phase 3, nicknames: a notable deed coins one, by the witness who likes the doer best, who is told it coined it; three
// small deeds of a family coin one and two don't; failing a fond witness, the innkeeper coins one once word is half
// round town; no two wolves of a town share one; three a wolf at most; asking folk not to use one ends it in briefings
// and prevents a re-coin; across a restart.
void nicknames()
{
    const auto root = fs::temp_directory_path() / ("ratw-fame3-" + std::to_string(::getpid()));
    const auto save = (fs::temp_directory_path() / ("ratw-fame3-" + std::to_string(::getpid()) + ".json")).string();
    fs::remove(save);
    const auto world = writeStrip(root);
    std::string nick, nickId;
    {
        Town t(world, save);
        auto& w = t.g.world();
        t.enter(t.ash, 1, "ash", "Ash", 1, 10.5);
        t.enter(t.bo, 2, "bo", "Bo", 1, 13.5);
        t.enter(t.cy, 3, "cy", "Cy", 1, 6.5);
        const auto ash = t.ash.entityId, bo = t.bo.entityId, cy = t.cy.entityId;
        const auto upper = w.lawTown(cellId(1));
        const auto put = [&](const char* who, int cell, double x, double y = 8.5) {
            auto* e = w.entity(who);
            e->cellId = cellId(cell);
            e->position = {x, y};
            e->path.clear();
        };
        for (const char* who : {"u1", "u2", "u3", "um", "ui"})
            put(who, 0, 4.5, 4.5);
        // The keeper likes her well; the innkeeper less. Both see her break a camp.
        w.bonds().change("um", ash, {30, 10, 20, 0, 0}, w.calendarDays());
        w.bonds().change("ui", ash, {10, 0, 20, 0, 0}, w.calendarDays());
        put("um", 1, 11.5);
        put("ui", 1, 9.5);
        t.run(.5);
        const auto a = t.g.recordDeed("broke_camp", {ash}, "town:" + upper, cellId(1), "test");
        const auto* d = t.g.deeds().find(a);
        expect(d && !d->nickname.empty(), "a notable deed coins a nickname");
        const auto* n = t.g.deeds().nicknames().count(d->nickname) ? &t.g.deeds().nicknames().at(d->nickname) : nullptr;
        expect(n && n->coinedBy == "um" && n->family == "road", "coined by the witness who likes her best: " + (n ? n->coinedBy + " " + n->text : std::string()));
        nick = n->text;
        nickId = n->id;
        bool coined = false;
        expect(t.g.nicknameFor("um", ash, &coined) == nick && coined, "the keeper calls her by it, and knows it coined it");
        expect(contains(t.g.fameBriefing("um", ash, false), "You were the first to call them '" + nick + "'"), "the Mind is told who coined it");
        expect(contains(t.g.fameBriefing("ui", ash, false), "Folk call them '" + nick + "'"), "the innkeeper, who saw it too, knows the nickname");
        // Small deeds of one family: two coin nothing, the third does (each for a fond beneficiary).
        for (const char* who : {"u1", "u2", "u3"})
        {
            put(who, 1, 13.5);
            w.bonds().change(who, bo, {25, 5, 20, 0, 0}, w.calendarDays());
        }
        t.run(.5);
        t.g.recordDeed("kept_promise", {bo}, "u1", cellId(1), "test");
        t.g.recordDeed("kept_promise", {bo}, "u2", cellId(1), "test");
        int bos = 0;
        for (const auto* bn : t.g.deeds().nicknamesOf(bo))
            bos += !bn->dropped;
        expect(bos == 0, "two small deeds of a family: no nickname yet");
        t.g.recordDeed("kept_promise", {bo}, "u3", cellId(1), "test");
        const auto bosNicks = t.g.deeds().nicknamesOf(bo);
        expect(bosNicks.size() == 1 && bosNicks[0]->family == "honest", "the third coins one: " + (bosNicks.empty() ? std::string() : bosNicks[0]->text));
        expect(bosNicks[0]->text != nick, "no two wolves of a town share a nickname");
        // No fond witness: the innkeeper coins one once word is half round town.
        for (const char* who : {"u1", "u2", "u3", "um", "ui"})
            put(who, 0, 4.5, 4.5);
        t.run(.5);
        const auto c = t.g.recordDeed("report_thief", {cy}, {}, cellId(1), "test");
        expect(t.g.deeds().find(c)->nickname.empty(), "nobody saw Cy's deed: no nickname at once");
        w.advanceCalendar(1.2);
        t.run(1.5);
        const auto cys = t.g.deeds().nicknamesOf(cy);
        expect(cys.size() == 1 && cys[0]->coinedBy == "ui", "the innkeeper coins one when word is half round town");
        expect(cys[0]->text != nick && cys[0]->text != bosNicks[0]->text, "and it is nobody else's in town");
        // Three a wolf at most.
        put("um", 1, 11.5);
        t.run(.5);
        for (int i = 0; i < 4; ++i)
            t.g.recordDeed("award", {ash}, "town:" + upper, cellId(1), "dm", "a deed for the record", fame::Great);
        int worn = 0;
        for (const auto* an : t.g.deeds().nicknamesOf(ash))
            worn += !an->dropped;
        expect(worn == 3, "three nicknames a wolf at most: " + std::to_string(worn));
        put("um", 0, 4.5, 4.5);
        // She asks folk not to use the first: it ends at once, and that deed never earns it again.
        t.g.command(&t.ash, cmd({{"type", "social"}, {"verb", "dropnickname"}, {"id", nickId}}));
        t.g.settle();
        expect(contains(t.ash.said(), "rather not be called that"), "she is told: " + t.ash.said());
        expect(t.g.fameBriefing("um", ash, false).find(nick) == std::string::npos && t.g.deeds().nicknames().at(nickId).dropped,
               "the keeper no longer uses it");
        expect(t.g.deeds().find(a)->noNickname, "the camp deed won't coin it again");
        w.advanceCalendar(1.2);
        t.run(1.5);
        expect(t.g.deeds().find(a)->nickname == nickId, "nor does the innkeeper later");
        const auto view = t.selfOf(t.ash).array("nicknames");
        bool shown = false;
        for (const auto& v : view)
            shown |= v.string("id") == nickId && v.boolean("dropped");
        expect(shown, "the sheet shows it, marked as dropped");
        t.g.save();
    }
    {
        Town t(world, save);
        t.enter(t.ash, 1, "ash", "Ash", 1, 10.5);
        const auto& kept = t.g.deeds().nicknames();
        expect(kept.count(nickId) && kept.at(nickId).text == nick && kept.at(nickId).dropped, "nicknames survive a restart");
    }
    fs::remove_all(root);
    fs::remove(save);
}

// Phase 4, festival criers: at the festival's crier slot (20:00), the innkeeper (no official in the test town) calls the
// season's deeds that travel under a name, heaviest first, and the nicknames coined, a line a minute where a player
// hears; a deed nobody can name isn't called; called deeds are all over town at once and residents at the square believe
// them under the name; a town with no player at its square has its deeds' reach set all the same; a legendary deed is
// called in every town at noon the next day.
void criers()
{
    const auto root = fs::temp_directory_path() / ("ratw-fame4-" + std::to_string(::getpid()));
    const auto world = writeStrip(root);
    Town t(world, {}, 120, true);
    auto& w = t.g.world();
    t.enter(t.ash, 1, "ash", "Ash", 1, 10.5);
    t.enter(t.bo, 2, "bo", "Bo", 4, 10.5);
    t.enter(t.cy, 3, "cy", "Cy", 1, 6.5);
    const auto ash = t.ash.entityId, bo = t.bo.entityId, cy = t.cy.entityId;
    const auto upper = w.lawTown(cellId(1)), ferro = w.lawTown(cellId(4));
    const auto put = [&](const char* who, int cell, double x, double y = 8.5) {
        auto* e = w.entity(who);
        e->cellId = cellId(cell);
        e->position = {x, y};
        e->path.clear();
    };
    for (const char* who : {"u1", "u2", "u3", "um", "ui"})
        put(who, 0, 4.5, 4.5);
    // The keeper knows Ash by name and likes her: her camp deed travels under "Ash" and gets a nickname.
    put("um", 1, 11.5);
    t.run(.5);
    say(t, t.ash, "Good day. I'm Ash.", "um");
    w.bonds().change("um", ash, {30, 10, 20, 0, 0}, w.calendarDays());
    const auto a = t.g.recordDeed("broke_camp", {ash}, "town:" + upper, cellId(1), "test");
    put("um", 0, 4.5, 4.5);
    const auto nick = t.g.nicknameFor("um", ash);
    expect(!nick.empty(), "Ash's deed has a nickname: " + nick);
    // Cy's deed: nobody knew a name.
    t.run(.5);
    const auto c = t.g.recordDeed("report_thief", {cy}, {}, cellId(1), "test");
    // Bo's in Ser Ferro, seen by its keeper who knows him.
    put("sm", 4, 11.5);
    t.run(.5);
    say(t, t.bo, "Good day. I'm Bo.", "sm");
    const auto b = t.g.recordDeed("broke_camp", {bo}, "town:" + ferro, cellId(4), "test");
    t.place(t.bo, 1, 14.5);                         // (Nobody at Ser Ferro's square to hear its crier.)
    // A festival in both towns, today.
    expect(w.callFestival(upper, "Test Fair", 0).ok && w.callFestival(ferro, "Ferro Fair", 0).ok, "festivals called");
    const double hour = (w.calendarDays() - std::floor(w.calendarDays())) * 24;
    w.advanceCalendar((19.9 - hour + (hour > 19.9 ? 24 : 0)) / 24);
    put("u1", 1, 9.5, 9.5);                         // (A resident at the square, who never heard of it.)
    t.place(t.ash, 1, 8.5, 10.5);
    t.run(1);
    t.ash.events.clear();
    w.advanceCalendar(.15 / 24);
    std::string calls;
    for (int i = 0; i < 9; ++i)
    {
        t.run(60);
        for (const auto& e : t.ash.events)
            if (e.string("type") == "roleplay")
                calls += e.string("text") + "\n";
        t.ash.events.clear();
    }
    expect(contains(calls, "Ash") && contains(calls, "drove the bandits off"), "the crier calls Ash's deed: " + calls);
    expect(contains(calls, nick), "and her nickname");
    expect(!contains(calls, "helped the watch"), "not a deed nobody can name");
    const auto* deedA = t.g.deeds().find(a);
    expect(std::abs(fame::reach(deedA->towns[0], deedA->weight, w.calendarDays()) - 1) < 1e-9, "called: all over town at once");
    const Belief* heard = beliefOf(w, "u1", "deed:" + a);
    expect(heard && heard->as == "Ash", "a resident at the square believes it, of Ash");
    const auto* deedB = t.g.deeds().find(b);
    double ferroReach = 0;
    for (const auto& word : deedB->towns)
        if (word.town == ferro)
            ferroReach = fame::reach(word, deedB->weight, w.calendarDays());
    expect(std::abs(ferroReach - 1) < 1e-9, "Ser Ferro's crier called Bo's deed with nobody there to hear, and the town has heard all the same");
    (void)c;
    // A legendary deed: called in every town at noon the next day.
    t.g.recordDeed("award", {cy}, "town:" + upper, cellId(1), "dm", "drove the Sea-Beast back into the deep", fame::Legendary);
    const double now = (w.calendarDays() - std::floor(w.calendarDays())) * 24;
    w.advanceCalendar((36.05 - now) / 24);
    t.place(t.ash, 1, 8.5, 10.5);
    t.ash.events.clear();
    std::string legend;
    for (int i = 0; i < 7 && legend.empty(); ++i)
    {
        t.run(60);
        for (const auto& e : t.ash.events)
            if (e.string("type") == "roleplay" && contains(e.string("text"), "Sea-Beast"))
                legend = e.string("text");
    }
    expect(contains(legend, "Cy"), "the legend called at noon the next day: " + legend);
    fs::remove_all(root);
}

// Phase 6, unfinished business and welcome back: an open promise and a half-done procure contract listed, each leaving
// when settled; a wolf away keeps every resident's regard while one who stays loses some; back after a break, a resident
// who last saw her before it is briefed once that she is back after a long while and greets her so, one who met her
// since isn't; the welcome card.
void welcomeBack()
{
    const auto root = fs::temp_directory_path() / ("ratw-fame6-" + std::to_string(::getpid()));
    const auto world = writeStrip(root);
    Town t(world, {}, 120, true, 1);                // (A break is a second long here.)
    auto& w = t.g.world();
    t.enter(t.ash, 1, "ash", "Ash", 1, 10.5);
    t.enter(t.bo, 2, "bo", "Bo", 1, 12.5);
    const auto ash = t.ash.entityId, bo = t.bo.entityId;
    const auto put = [&](const char* who, int cell, double x, double y = 8.5) {
        auto* e = w.entity(who);
        e->cellId = cellId(cell);
        e->position = {x, y};
        e->path.clear();
    };
    // Unfinished business.
    w.promise(ash, "um", "bring a pot of honey", 3);
    auto& k = w.postContract("procure", "um", w.lawTown(cellId(1)), "um", 10, 5, "smoked fish for the stall");
    k.item = "smoked_fish";
    k.quantity = 5;
    k.delivered = 3;
    k.taker = ash;
    k.status = "taken";
    t.run(2.5);
    std::string open;
    for (const auto& u : t.selfOf(t.ash).array("unfinished"))
        open += u.string("text") + "\n";
    expect(contains(open, "You promised") && contains(open, "bring a pot of honey") && contains(open, "3 of 5 handed in"),
           "her open threads: " + open);
    w.recordEvent({"gift", ash, "um", cellId(1), 0, 0, "honey", 1, 0, {}});
    for (auto& c : w.roads().contracts)
        if (c.taker == ash)
            c.status = "done";
    t.run(2.5);
    open.clear();
    for (const auto& u : t.selfOf(t.ash).array("unfinished"))
        open += u.string("text") + "\n";
    expect(!contains(open, "honey") && !contains(open, "handed in"), "settled, they leave the list: " + open);
    // Regard while away: both are known to the keeper; Ash leaves, Bo stays (and never speaks to it).
    for (const auto* who : {&ash, &bo})
        w.bonds().change("um", *who, {0, 0, 40, 0, 0}, w.calendarDays());
    w.bonds().change("ui", ash, {0, 0, 20, 0, 0}, w.calendarDays());
    const double before = w.bonds().find("um", ash)->familiarity, boBefore = w.bonds().find("um", bo)->familiarity;
    w.advanceCalendar(.2);
    t.run(.5);
    t.g.disconnect(&t.ash);
    std::this_thread::sleep_for(std::chrono::milliseconds(1300));   // (Longer than the break.)
    for (int day = 0; day < 40; ++day)
    {
        w.advanceCalendar(1);
        t.run(.5);
    }
    expect(w.bonds().find("um", ash) && std::abs(w.bonds().find("um", ash)->familiarity - before) < 1e-6,
           "away 40 days: the keeper knows her as well as ever");
    expect(w.bonds().find("um", bo) == nullptr || w.bonds().find("um", bo)->familiarity < boBefore - 5, "Bo, who stayed and never came by, is less known");
    // The innkeeper meets her in the meantime... (a contact after she left): it won't greet her as long gone.
    w.bonds().change("ui", ash, {0, 0, 1, 0, 0}, w.calendarDays());
    t.ash = Client{};
    t.enter(t.ash, 5, "ash", "Ash", 1, 10.5);
    t.run(1);
    const auto* card = t.ash.last("welcome");
    expect(card && card->number("days") >= 0, "the welcome card on coming back");
    const auto briefing = t.g.awayBriefingFor("um", ash);
    expect(contains(briefing, "back after a long while") && contains(briefing, "weeks ago"), "the keeper is told: " + briefing);
    expect(t.g.awayBriefingFor("ui", ash).empty(), "the innkeeper, who saw her since, isn't");
    // The keeper's greeting: back after a long while.
    put("um", 1, 11.5);
    t.run(.5);
    t.ash.events.clear();
    say(t, t.ash, "Hello.", "um");
    std::string greeting;
    for (int i = 0; i < 80 && greeting.empty(); ++i)
    {
        t.run(.5);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        for (const auto& e : t.ash.events)
            if (e.string("type") == "roleplay" && contains(e.string("text"), "since"))
                greeting = e.string("text");
    }
    expect(!greeting.empty(), "the keeper greets her as someone back after a long while");
    t.run(1);
    expect(t.g.awayBriefingFor("um", ash).empty(), "once they have spoken, no more");
    fs::remove_all(root);
}
} // namespace

int main()
{
    try
    {
        ledger();
        travels();
        nicknames();
        criers();
        welcomeBack();
    }
    catch (const std::exception& error)
    {
        std::cerr << "fame_tests failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "fame_tests passed (" << checks << " checks)\n";
    return 0;
}
