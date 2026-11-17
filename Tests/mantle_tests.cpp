// Mantles (Docs/Design/84-living-conversations.md §7.9, §9.4, §11, §17; Phase 9): the .mantle syntax read and written
// back, a mantle checked and compiled into its own bucket, worn only by its wearer, its words a layer between the
// wearer's own and their dialect, its habits seeded by the mantle, an NPC's own lines as an override, the hot reload that
// keeps the old revision when the new one fails, and, in a game (the talk town wearing Tests/fixtures/mantles), who wears
// what, mantle.sync and voice.sync <npc>.
//
//   mantle_tests            the checks (Phase 10: passing mantles on, with synthetic deaths in a small Society, and a
//                           handoff in the talk town)
//   mantle_tests --bench    B3 and B4 (§16): about 800 mantles at the richer size (about 50,700 items) loaded in one
//                           batch, the time and the memory it adds, one mantle swapped, and a pick with and without them
#include "RatwCalendar.h"
#include "RatwGame.h"
#include "RatwJsonDoc.h"
#include "RatwSociety.h"
#include "RatwTalk.h"
#include "RatwWire.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <cstdlib>
#include <malloc.h>
#include <unistd.h>
#include <vector>

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

const std::string Fixture = RATW_SOURCE_DIR "/Tests/fixtures/mantles";

std::string read(const std::string& path)
{
    std::ifstream in(path);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

talk::Library& library()
{
    static talk::Library lib;
    static bool loaded = false;
    if (!loaded)
    {
        std::string problem;
        expect(lib.load(RATW_SOURCE_DIR "/Data/Talk", problem), "the library loads: " + problem);
        loaded = true;
    }
    return lib;
}

talk::Speaker speaker(const std::string& id, const std::string& mantle, const std::string& dialect, const std::string& accent = "broad")
{
    talk::Speaker s;
    s.facts = {{"id", id},          {"name", "Wren Tester"}, {"first", "Wren"},     {"sex", "female"}, {"stage", "adult"},
               {"age", "40"},       {"job", "merchant"},     {"role", "merchant"},  {"from", dialect}, {"accent", accent},
               {"temper", "even"},  {"talk", "plain"},       {"warmth", "warm"},    {"humour", "none"}, {"honesty", "plain"},
               {"piety", "ordinary"}, {"drive", "steady"},   {"working", "yes"},    {"wealth", "comfortable"}};
    if (!mantle.empty())
    {
        s.facts["mantle"] = mantle;
        s.grammar.push_back(talk::mantleLayer(mantle));
    }
    s.grammar.push_back(dialect == "ridgemere" ? "plain" : dialect);
    return s;
}

void theSyntax()
{
    talk::Mantle m;
    std::vector<std::string> errors;
    const auto text = read(Fixture + "/coldwater/cw017.mantle");
    expect(talk::readMantle(text, "cw017", m, errors), "cw017 reads");
    expect(m.id == "cw017" && m.dialect == "coldwater" && m.state == "ready" && m.tier == "full", "the header");
    expect(m.accents == std::vector<std::string>{"broad", "light"}, "the accents, the first as written");
    expect(m.traits.at("warmth") == "warm" && m.quirk == "swims_in_the_lake_even_in_autumn", "traits and quirk");
    expect(m.topics.size() == 3 && m.topics[2].first == "prices" && m.topics[2].second < 0, "topics with their weights");
    std::map<std::string, const talk::MantleItem*> byKey;
    for (const auto& i : m.items)
        byKey[i.key] = &i;
    expect(byKey.count("words.oath") && byKey.at("words.oath")->kind == "symbol" && byKey.at("words.oath")->section == "any",
           "a symbol's key is words.<symbol>");
    expect(byKey.count("opinion.road.1") && byKey.count("opinion.road.2") && byKey.count("opinion.watch.1"), "opinions numbered by issue");
    expect(byKey.count("grown.request.carry") && byKey.at("grown.request.carry")->move == "request", "a named line: <section>.<move>.<name>");
    expect(byKey.count("grown.tell.self.1") && byKey.count("grown.insist.1"), "unnamed lines numbered");
    expect(byKey.count("story.first_swim") && byKey.at("story.first_swim")->section == "grown", "a story keyed by its name");
    expect(byKey.count("grown.scene.shutters"), "a scene: <section>.scene.<name>");
    expect(byKey.count("dlg.shop.greet.1") && byKey.count("dlg.shop.greet.2"), "window lines numbered by node");
    expect(byKey.count("graft.resident.hub") && byKey.at("graft.resident.hub")->body.find("node swim_where") != std::string::npos,
           "a graft is a block");
    expect(byKey.count("young.tell.boast.1") && byKey.at("young.tell.boast.1")->section == "young", "a stage section");
    // Inserting a line moves no key: a named one keeps its name, the rest the next free number.
    const auto more = text + "\ngrown\nline @2 preclose : Right. Off with you.\nline preclose : And you.\n";
    talk::Mantle m2;
    expect(talk::readMantle(more, "cw017", m2, errors), "more lines read: " + (errors.empty() ? "" : errors.front()));
    std::set<std::string> keys;
    for (const auto& i : m2.items)
        keys.insert(i.key);
    expect(keys.count("grown.preclose.1") && keys.count("grown.preclose.2") && keys.count("grown.preclose.3"), "numbers left free are taken");
    // The text from the card and the items, as the game assembles it from the rows, reads back the same.
    talk::Mantle back;
    expect(talk::readMantle(talk::mantleText(m, m.items), "back", back, errors), "the assembled text reads");
    expect(back.items.size() == m.items.size(), "every item comes back");
    for (std::size_t i = 0; i < m.items.size(); ++i)
    {
        const auto& a = m.items[i];
        const auto found = std::find_if(back.items.begin(), back.items.end(), [&](const talk::MantleItem& b) { return b.key == a.key; });
        expect(found != back.items.end() && found->body == a.body && found->section == a.section && found->kind == a.kind,
               "item " + a.key + " comes back the same");
    }
    // What the syntax refuses.
    const std::vector<std::pair<std::string, std::string>> bad{
        {"state ready\n", "starts 'mantle"},
        {"mantle cw17\nstate ready\n", "two letters and three or four digits"},
        {"mantle cw018\nstate maybe\n", "state is draft, ready or retired"},
        {"mantle cw018\naccent loud\n", "accent is one to three"},
        {"mantle cw018\nline greet Hello.\n", "line is 'line"},
        {"mantle cw018\nline @a greet : Hi.\nline @a greet : Yo.\n", "is there twice"},
        {"mantle cw018\nwords\nopinion road maybe : No.\nnonsense here\n", "expected a header line"},
    };
    for (const auto& [t, why] : bad)
    {
        talk::Mantle x;
        std::vector<std::string> e;
        expect(!talk::readMantle(t, "bad", x, e), "refused: " + why);
        bool said = false;
        for (const auto& line : e)
            said = said || line.find(why) != std::string::npos;
        expect(said, "the reason names it: " + why + " (got " + (e.empty() ? "" : e.front()) + ")");
    }
}

void theChecks()
{
    auto& lib = library();
    talk::Mantle m;
    std::string problem;
    // Every fixture mantle passes, in any state.
    for (const auto& f : fs::recursive_directory_iterator(Fixture))
        if (f.path().extension() == ".mantle")
            expect(lib.checkMantle({"", read(f.path().string()), f.path().string()}, m, problem), f.path().string() + ": " + problem);
    // And what the loader refuses, each named by the item's key.
    const std::string head = "mantle cw098\nstate ready\ndialect coldwater\naccent light\ntier standard\n";
    const std::vector<std::pair<std::string, std::string>> bad{
        {"dialect atlantis\n", "not a grammar"},
        {"traits temper=furious\n", "temper=furious"},
        {"topics money +.3\n", "'money' is not a topic"},
        {"words\nnope = #nothing_here#\n", "words.nope"},
        {"opinion moon for 2 : Fine.\n", "opinion.moon.1"},
        {"line flarp : Hello.\n", "'flarp' is not a move"},
        {"line greet [self.zzz=1] : Hello.\n", "unknown fact"},
        {"line farewell : #undefined_sym#.\n", "any.farewell.1"},
        {"line greet : " + std::string(200, 'x') + "\n", "160 characters"},
        {"story bare\n  : Only me.\n", "give the listener a line"},
        {"dlg shop.nowhere : Hi.\n", "shop.nowhere"},
        {"graft resident.hub\nchoice Go -> nowhere\n", "neither its node"},
        {"graft resident.hub\nchoice Go -> lost\nnode lost\nsay Lost.\n", "no way back"},
        {"graft resident.hub\nchoice Go -> hub\nnode lost\nsay Lost.\nchoice Back -> hub\n", "never reached"},
        {"marks oath\n", "a mark names a habit"},
        {"scene two\nwhen concept=talk.weather\na : Rain.\n", "needs a line b always says"},
    };
    for (const auto& [t, why] : bad)
    {
        expect(!lib.checkMantle({"", head + t, "bad"}, m, problem), "refused: " + why);
        expect(problem.find(why) != std::string::npos, "the reason names " + why + ", not: " + problem);
    }
    // Only a ready mantle is worn.
    expect(!lib.setMantle({"", read(Fixture + "/plain/rm090.mantle"), "rm090"}, problem) && problem.find("draft") != std::string::npos,
           "a draft is never worn");
    expect(lib.checkMantle({"", read(Fixture + "/plain/rm090.mantle"), "rm090"}, m, problem), "but it is checked");
}

void wornByItsWearer()
{
    auto lib = library();                           // (A copy: the mantles loaded here are this test's.)
    std::vector<talk::MantleText> texts;
    for (const auto& f : fs::recursive_directory_iterator(Fixture))
        if (f.path().extension() == ".mantle")
            texts.push_back({"", read(f.path().string()), f.path().string()});
    const auto shared = lib.size();
    std::vector<talk::Library::MantleRefusal> refused;
    const auto n = lib.setMantles(texts, refused);
    expect(n == texts.size() - 1 && refused.size() == 1 && refused.front().id == "rm090", "every ready one loads; the draft is refused");
    expect(lib.size() == shared, "the shared library is untouched (each mantle its own bucket)");
    expect(lib.mantle("cw017") && lib.mantle("cw017")->entries > 10 && lib.mantleEntries() > 100, "the buckets hold their rules");
    expect(lib.find("cw017.grown.request.carry") && lib.find("cw017.grown.request.carry")->owner == "cw017", "found by id");

    talk::Query q;
    q.concept = "dlg.shop.greet";
    q.a = speaker("keeper", "cw017", "coldwater");
    q.alone = true;
    q.facts["here.place"] = "shop";
    std::set<std::string> said;
    for (std::uint64_t s = 1; s <= 30; ++s)
        said.insert(lib.pick(q, s, {}, {}).id);
    expect(said.size() == 1 && said.count("cw017.dlg.shop.greet.1"), "the window's greeting is the wearer's own (+5 wins), not " +
                                                                         *said.begin());
    auto other = q;
    other.a = speaker("stranger", "", "coldwater");
    for (std::uint64_t s = 1; s <= 30; ++s)
        expect(lib.pick(other, s, {}, {}).id.rfind("cw017.", 0) != 0, "one who doesn't wear it never says its lines");
    auto wrongWearer = q;
    wrongWearer.a = speaker("keeper2", "sw001", "saltwell");
    for (std::uint64_t s = 1; s <= 30; ++s)
        expect(lib.pick(wrongWearer, s, {}, {}).id.rfind("cw017.", 0) != 0, "another mantle's wearer neither");

    // A whole scene of its own carries a talk (§37): the wearer opens it, on its topic.
    talk::Query w;
    w.concept = "talk.weather";
    w.a = speaker("keeper", "cw017", "coldwater");
    w.b = speaker("neighbour", "", "coldwater");
    w.facts = {{"here.time", "day"}, {"here.place", "street"}, {"here.weather", "clear"}, {"here.outdoors", "clear"},
               {"pair.band", "acquaintances"}, {"here.region", "coldwater"}};
    int own = 0;
    for (std::uint64_t s = 1; s <= 40; ++s)
        own += lib.converse(w, s, {}, {}).id == "cw017.grown.scene.shutters";
    expect(own > 0, "the wearer's own scene is spoken");

    // Its words are a layer between the wearer's own and their dialect; its habits are the mantle's, whoever wears it.
    std::string out;
    talk::Query g;
    g.a = speaker("keeper", "cw017", "coldwater");
    g.alone = true;
    std::set<std::string> swims;
    for (std::uint64_t s = 1; s <= 20; ++s)
    {
        expect(lib.expand("#swim#", g, 0, s, out), "the mantle's own symbol");
        out[0] = char(std::tolower(static_cast<unsigned char>(out[0])));
        swims.insert(out);
    }
    expect(swims.count("every morning till the ice") && swims.count("in before the boats, every day"), "its words: " + *swims.begin());
    std::string oathA, oathB;
    auto g2 = g;
    g2.a = speaker("someone_else", "cw017", "coldwater");
    expect(lib.expand("#oath#", g, 0, 1, oathA) && lib.expand("#oath#", g2, 0, 99, oathB), "the oath");
    expect(oathA == oathB && (oathA == "Deep and dark" || oathA == "Glass and stone"), "a mantle's habit is the mantle's, whoever wears it");
    // Without the mantle, the dialect's (or plain's) oath; a wolf's own layer (an override) over the mantle's.
    auto bare = g;
    bare.a = speaker("keeper", "", "coldwater");
    std::string plainOath;
    lib.expand("#oath#", bare, 0, 1, plainOath);
    expect(plainOath != "Deep and dark" && plainOath != "Glass and stone", "the oath is the mantle's only while it is worn");
    std::string problem;
    const auto layer = talk::Library::personalLayer("keeper");
    expect(lib.setGrammar(layer, "grammar " + layer + "\noath! = By my own whiskers\n", problem), "a wolf's own words: " + problem);
    auto over = g;
    over.a.grammar.insert(over.a.grammar.begin(), layer);
    expect(lib.expand("#oath#", over, 0, 1, out) && out == "By my own whiskers", "the wearer's own layer over the mantle's");

    // A move line of the wearer's answers what is owed (react.news to news), within its share.
    talk::Conversation c;
    c.people = {"teller", "keeper"};
    c.phase = talk::Conversation::Phase::Body;
    c.seed = 5;
    talk::Conversation::Open owed;
    owed.by = 0, owed.to = 1, owed.moves = {"react.news"};
    talk::Query ans;
    ans.a = speaker("teller", "", "coldwater");
    ans.b = speaker("keeper", "cw017", "coldwater");
    ans.b.facts["knows"] = "no";
    ans.facts = w.facts;
    int mine = 0;
    for (std::uint64_t s = 1; s <= 30; ++s)
    {
        c.seed = s;
        const auto a = lib.answer(c, owed, ans, 1, {});
        mine += !a.id.empty() && a.id.rfind("cw017.", 0) == 0;
    }
    expect(mine > 15, "the wearer answers news with their own words (+5): " + std::to_string(mine) + " of 30");
    // Past its share (35% of the talk's lines), the shared lines answer.
    for (int i = 0; i < 4; ++i)
    {
        talk::SaidLine l;
        l.entry = "cw017.grown.react.news.new.1";
        l.who = 1;
        l.part = "edge";
        l.text = "x" + std::to_string(i);
        c.lines.push_back(l);
    }
    for (std::uint64_t s = 1; s <= 30; ++s)
    {
        c.seed = s;
        const auto a = lib.answer(c, owed, ans, 1, {});
        expect(a.id.rfind("cw017.", 0) != 0, "past its share, the mantle is left out");
    }

    // An NPC's own lines are an override in a bucket of their own, never a copy of the library (defect 16).
    expect(lib.setCharacter("keeper", "rule own\nwhen concept=dlg.shop.greet\nsay My very own hello.\n", problem), "an override: " + problem);
    expect(lib.size() == shared, "an override leaves the shared library as it was");
    for (std::uint64_t s = 1; s <= 10; ++s)
    {
        const auto said_ = lib.pick(q, s, {}, {});
        expect(said_.id == "keeper.own" || said_.id.rfind("cw017.", 0) == 0, "the override competes as the wearer's own: " + said_.id);
    }
    expect(lib.setCharacter("keeper", "", problem) && !lib.find("keeper.own"), "an empty text takes it away");
}

void theHotReload()
{
    auto lib = library();
    std::string problem, id;
    const auto text = read(Fixture + "/coldwater/cw017.mantle");
    expect(lib.setMantle({"", text, "cw017"}, problem, &id) && id == "cw017", "loaded: " + problem);
    const auto withRevision = [&](int r) {
        auto t = text;
        t.replace(t.find("state ready"), 11, "revision " + std::to_string(r) + "\nstate ready");
        return t;
    };
    expect(lib.setMantle({"cw017", withRevision(5), "db:cw017"}, problem) && lib.mantle("cw017")->revision == 5, "revision 5 swapped in");
    auto broken = withRevision(6);
    broken += "\ngrown\nline flarp : Nope.\n";
    expect(!lib.setMantle({"cw017", broken, "db:cw017"}, problem), "revision 6 is refused");
    expect(lib.mantle("cw017")->revision == 5 && lib.find("cw017.grown.request.carry"), "revision 5 is still in use");
    expect(!lib.setMantle({"cw018", withRevision(7), "db:cw018"}, problem) && problem.find("not cw018") != std::string::npos,
           "a row whose text names another mantle is refused");
    // A new line in a new revision is heard at once; an item taken out is gone.
    auto added = withRevision(7);
    added.replace(added.find("line accept_decline"), 0, "line @fresh greet : Fresh words for a new revision.\n");
    expect(lib.setMantle({"cw017", added, "db:cw017"}, problem) && lib.find("cw017.grown.greet.fresh"), "the new item is there");
    expect(lib.setMantle({"cw017", withRevision(8), "db:cw017"}, problem) && !lib.find("cw017.grown.greet.fresh"), "and gone again");
    lib.dropMantle("cw017");
    expect(!lib.mantle("cw017") && !lib.find("cw017.grown.request.carry"), "dropped");
}

// In a game: the talk town wearing the fixture's mantles (Options::mantlesFrom), and the DM's actions.
void inTheGame()
{
    const auto dir = fs::temp_directory_path() / ("ratw-mantles-" + std::to_string(::getpid()));
    fs::remove_all(dir);
    fs::copy(Fixture, dir, fs::copy_options::recursive);
    const auto save = (fs::temp_directory_path() / ("ratw-mantle-tests-" + std::to_string(::getpid()) + ".json")).string();
    {
        game::Options o;
        o.talkData = RATW_SOURCE_DIR "/Data/Talk";
        o.voiceData = RATW_SOURCE_DIR "/Data/Voice";
        o.scratch = true;
        o.forkSnapshots = false;
        o.savePath = save;
        o.worldExport = RATW_SOURCE_DIR "/Tests/fixtures/talk_town";
        o.mantlesFrom = dir.string();
        auto g = std::make_unique<game::Game>(o);
        std::vector<std::string> notes;
        g->log = [&](const char*, const std::string& m) { notes.push_back(m); };
        std::string problem;
        expect(g->start(problem), "the talk town starts: " + problem);
        const auto& worn = g->mantleWearers();
        expect(worn.count("tt_co_keeper") && worn.at("tt_co_keeper") == "cw017", "the keeper wears cw017 (written for her)");
        expect(worn.count("tt_sa_storekeeper") && worn.at("tt_sa_storekeeper") == "sw001" && worn.count("tt_ri_pup"), "and the others theirs");
        expect(!worn.count("rm_dagny_slaghand"), "a mantle written for someone not in this world is worn by nobody");
        expect(std::any_of(notes.begin(), notes.end(), [](const std::string& n) { return n.find("RATW_MANTLE loaded 15 mantles") != std::string::npos; }),
               "the load is logged");
        auto q = g->talkQuery("tt_co_keeper", "", "dlg.shop.greet");
        expect(q.a.facts["mantle"] == "cw017", "the wearer's facts name the mantle");
        expect(std::find(q.a.grammar.begin(), q.a.grammar.end(), "m_cw017") != q.a.grammar.end() &&
                   q.a.grammar.back() == "coldwater" && q.a.grammar.front() == "m_cw017",
               "its layer is above the dialect");
        expect(q.a.facts["quirk"] == "swims_in_the_lake_even_in_autumn" && q.a.facts["warmth"] == "warm", "its quirk and traits");
        expect(q.a.facts["op.road"] == "against" && q.a.facts["op_basis.road"] == "mantle", "its opinion over the worked-out stance");
        expect(q.a.facts["fears"] == "deep_winter_ice" && q.a.facts["likes"].find("still_water") != std::string::npos, "its card");
        // The pup wears her young section.
        auto pup = g->talkQuery("tt_ri_pup", "tt_ri_baker", "talk.day");
        expect(pup.a.facts["mantle"] == "rm003" && pup.a.facts["stage"] == "young", "the pup wears rm003");

        // mantle.sync: a new revision heard at once; a broken one refused, the old kept; a draft unloaded.
        const auto file = dir / "coldwater" / "cw017.mantle";
        auto text = read(file.string());
        const auto write = [&](const std::string& t) { std::ofstream(file) << t; };
        auto v2 = text;
        v2.replace(v2.find("state ready"), 11, "revision 2\nstate ready");
        write(v2);
        auto r = g->applyDmAct({std::string("1"), std::string("mantle.sync"), std::string("cw017"), std::string("test"), std::string("{}")});
        expect(r.ok && r.message.find("revision 2 is loaded") != std::string::npos, "mantle.sync loads revision 2: " + r.message);
        auto broken = v2;
        broken.replace(broken.find("revision 2"), 10, "revision 3");
        broken += "\ngrown\nline flarp : Nope.\n";
        write(broken);
        r = g->applyDmAct({std::string("2"), std::string("mantle.sync"), std::string("cw017"), std::string("test"), std::string("{}")});
        expect(!r.ok && r.message.find("revision 2 is still in use") != std::string::npos && r.message.find("flarp") != std::string::npos,
               "a broken revision is refused and the old kept: " + r.message);
        expect(g->talkLibrary().mantle("cw017")->revision == 2 && g->mantleWearers().count("tt_co_keeper"), "still worn, at revision 2");
        auto draft = v2;
        draft.replace(draft.find("state ready"), 11, "state draft");
        write(draft);
        r = g->applyDmAct({std::string("3"), std::string("mantle.sync"), std::string("cw017"), std::string("test"), std::string("{}")});
        expect(r.ok && !g->talkLibrary().mantle("cw017") && !g->mantleWearers().count("tt_co_keeper"), "a draft is not worn: " + r.message);
        expect(g->talkQuery("tt_co_keeper", "", "dlg.shop.greet").a.facts.count("mantle") == 0, "and she speaks from the library");
        write(text);
        r = g->applyDmAct({std::string("4"), std::string("voice.sync"), std::string("tt_co_keeper"), std::string("test"), std::string("{}")});
        expect(g->mantleWearers().count("tt_co_keeper") == 0 || r.ok, "voice.sync <npc> answers");
        r = g->applyDmAct({std::string("5"), std::string("mantle.sync"), std::string("cw017"), std::string("test"), std::string("{}")});
        expect(r.ok && g->mantleWearers().count("tt_co_keeper"), "ready again, worn again: " + r.message);
        r = g->applyDmAct({std::string("6"), std::string("mantle.sync"), std::string("cw999"), std::string("test"), std::string("{}")});
        expect(!r.ok && r.message.find("No mantle cw999") != std::string::npos, "an unknown mantle is said so");

        // The talk town talks, and the wearers' own lines are among what is said.
        for (long i = 0, n = long(2 * calendar::SecondsPerDay / 24 / .05); i < n; ++i)
            g->tick(.05);
    }
    std::error_code ec;
    fs::remove(save, ec);
    fs::remove_all(dir, ec);
}

// Phase 10 (§12): passing mantles on, in a small Society (no world, no economy run): synthetic deaths, the births they owe,
// and what each child is given. The lake town: the Reeds (Ottar and Wenna, of an age for pups) in Coldwater, Ottar's
// mother Agna next door wearing cw001, Myrtle the keeper wearing cw017; in Ridgemere a carter and a clerk.
ResidentSpec person(const std::string& id, const std::string& name, const std::string& role, const std::string& work, int age,
                    const std::string& sex, Spot home, Spot job)
{
    ResidentSpec r;
    r.id = id, r.name = name, r.role = role, r.workLabel = work, r.age = age;
    r.appearance.sex = sex;
    r.home = r.evening = home;
    r.work = r.serve = job;
    r.purse = 40;
    return r;
}

struct Town
{
    std::set<std::string> dead;
    std::map<std::string, std::vector<std::string>> knew;
    std::map<std::string, std::pair<std::string, std::string>> origins;
    static std::string townOf(const std::string& cell)
    {
        return cell.rfind("cw_", 0) == 0 ? "coldwater" : cell.rfind("fh_", 0) == 0 ? "frosthollow" : "ridgemere";
    }
    static std::string pool(const std::string& town) { return town == "ridgemere" ? std::string("plain") : town; }
    CareerWorld world(const Society& s)
    {
        CareerWorld w;
        w.alive = [this](const std::string& id) { return !dead.count(id); };
        w.age = [&s](const std::string& id) { const auto* r = s.spec(id); return r ? r->age : 30; };
        w.regard = [](const std::string&, const std::string&) { return 0.0; };
        w.townOf = [](const std::string& cell) { return townOf(cell); };
        w.dialectOfTown = [](const std::string& town) { return pool(town); };
        w.dialectOfCell = [](const std::string& cell) { return pool(townOf(cell)); };
        w.originOf = [this, &s](const std::string& id) {
            if (const auto o = origins.find(id); o != origins.end())
                return o->second;
            const auto* r = s.spec(id);
            return std::pair<std::string, std::string>{r ? townOf(r->home.cell) : std::string(), "light"};
        };
        w.knownBy = [this](const std::string& id) { return knew.count(id) ? knew[id] : std::vector<std::string>{}; };
        return w;
    }
};

PoolMantle card(const std::string& id, const std::string& dialect, const std::string& fits, std::map<std::string, std::string> traits,
                const std::string& quirk = "", std::vector<std::string> marks = {"oath"})
{
    PoolMantle c;
    c.id = id, c.dialect = dialect, c.fits = fits, c.traits = std::move(traits), c.quirk = quirk, c.marks = std::move(marks);
    c.accents = {"broad", "light"};
    return c;
}

MantleRules lakeRules()
{
    MantleRules rules;
    rules.restDays = 365;
    rules.fallback = {{"frosthollow", "coldwater"}};
    NamePool cw;
    cw.female = {"Wenna", "Ottilie", "Merle", "Linnet"};
    cw.male = {"Orrin", "Ottar", "Heron", "Tarn"};
    cw.namedFor = .25;
    rules.names["coldwater"] = cw;
    NamePool fh;
    fh.female = {"Sigrid", "Agna"};
    fh.male = {"Brand", "Ulf"};
    fh.namedFor = 1;
    rules.names["frosthollow"] = fh;
    return rules;
}

// A child asked for, made as a host would (its record, then welcomed): its id.
std::string makeChild(Society& s, const ResidentRequest& r, int n)
{
    const auto* mother = s.spec(r.templateId);
    auto child = *mother;
    child.id = "born_" + std::to_string(n);
    child.name = r.name;
    child.age = 0;
    child.role = "civilian";
    child.workLabel = "-";
    child.appearance.sex = r.sex;
    if (!r.personality.empty())
        child.personality = r.personality;
    expect(s.addResident(child, 0), "the child is made");
    s.welcome(r, child.id);
    return child.id;
}

void passingOn()
{
    // fits: a hard criterion holds at the age one is or will grow to.
    expect(Society::fitsMantle("stage=adult|old", {{"sex", "male"}}, 0), "a newborn fits an adult's mantle: it will be one");
    expect(!Society::fitsMantle("stage=young|adolescent", {{"sex", "male"}}, 30), "a grown wolf never fits a pup's");
    expect(!Society::fitsMantle("sex=female", {{"sex", "male"}}, 0) && Society::fitsMantle("female", {{"sex", "female"}}, 0),
           "sex as written, bare or not");
    expect(!Society::fitsMantle("role=merchant", {{"role", "civilian"}}, 0) && Society::fitsMantle("self.role!=guard", {{"role", "civilian"}}, 0),
           "a role, with self. or without");
    expect(Society::fitsMantle("age>=60", {}, 0) && !Society::fitsMantle("age<5", {}, 30), "an age, upward as one grows");
    expect(!Society::fitsMantle("quarter=sump", {}, 30), "a fact not known doesn't hold");

    AuthoredRoster roster;
    roster.residents = {
        person("ottar", "Ottar Reed", "civilian", "boiling soap", 32, "male", {"cw_reed_house", 2.5, 2.5}, {"cw_soap", 2.5, 2.5}),
        person("wenna", "Wenna Reed", "civilian", "working the looms", 29, "female", {"cw_reed_house", 3.5, 2.5}, {"cw_looms", 2.5, 2.5}),
        person("agna", "Agna Reed", "civilian", "resting", 69, "female", {"cw_agna_house", 2.5, 2.5}, {"cw_agna_house", 2.5, 2.5}),
        person("myrtle", "Myrtle Stillwater", "merchant", "keeping the stores", 70, "female", {"cw_stores_up", 2.5, 2.5}, {"cw_stores", 2.5, 2.5}),
        person("joss", "Joss Wainwright", "civilian", "carting goods", 50, "male", {"rm_yard", 2.5, 2.5}, {"rm_yard", 3.5, 2.5}),
        person("ida", "Ida Quillon", "civilian", "copying the city rolls", 44, "female", {"rm_rolls", 2.5, 2.5}, {"rm_office", 2.5, 2.5})};
    roster.kin = {{"ottar", "wenna", "spouse"}, {"agna", "ottar", "parent"}};
    Society s(Roster::None);
    s.configure(roster);
    s.setMantleRules(lakeRules());
    s.setMantleCards({card("cw017", "coldwater", "stage=adult|old", {{"temper", "calm"}, {"warmth", "warm"}, {"honesty", "plain"}},
                           "swims_in_the_lake_even_in_autumn", {"oath", "saying"}),
                      card("cw001", "coldwater", "", {{"honesty", "sly"}, {"humour", "dry"}}, "counts_the_gulls"),
                      card("cw090", "coldwater", "stage=young|adolescent|adult|old", {{"honesty", "guarded"}, {"talk", "chatty"}}),
                      card("cw091", "coldwater", "sex=female", {{"honesty", "frank"}}),
                      card("cw092", "coldwater", "", {{"honesty", "plain"}, {"drive", "restless"}}),
                      card("rm090", "plain", "", {{"honesty", "plain"}})});
    Town t;
    auto w = t.world(s);
    s.tendCareers(1.3, w);                          // (The families taken in.)
    std::string why;
    expect(s.bindMantle("myrtle", "cw017", "authored", 1, why) && s.bindMantle("agna", "cw001", "authored", 1, why), "authored: " + why);
    expect(!s.bindMantle("ida", "cw017", "dm", 1, why) && why.find("worn by myrtle") != std::string::npos, "one living wearer at a time: " + why);
    expect(s.wearerOf("cw017") == "myrtle", "Myrtle wears cw017");

    // Myrtle dies: cw017 rests, known well by Wenna (who lives in the birth town).
    t.dead.insert("myrtle");
    t.knew["myrtle"] = {"wenna"};
    const auto notes = s.died("myrtle", 2.5, &w);
    expect(std::any_of(notes.begin(), notes.end(), [](const CareerNote& n) { return n.kind == "mantle_release" && n.detail == "cw017"; }),
           "the death releases her mantle");
    const auto* released = s.mantleOf("myrtle");
    expect(released && released->releasedDay == 2.5 && released->knownBy == std::vector<std::string>{"wenna"}, "on the day, with who knew her");
    expect(s.wearerOf("cw017").empty(), "nobody living wears it");
    auto changes = s.takeMantleChanges();
    expect(changes.size() == 1 && changes[0].kind == "release" && changes[0].mantle == "cw017", "the host is told to write the release");

    // Her child is born days later to the Reeds, the only couple of an age: cw017 is resting (a year), cw001 is worn, so a
    // spare of Coldwater, its card the child's voice (honesty and temperament: Q4).
    int made = 0;
    std::vector<std::string> born;
    const auto days = [&](int from, int to) {
        for (int day = from; day < to; ++day)
        {
            s.tendCareers(day + .3, w);
            for (const auto& r : s.takeRequests())
                if (r.kind == "birth")
                    born.push_back(makeChild(s, r, made++));
        }
    };
    days(2, 12);
    expect(born.size() == 1, "one child for one death");
    const auto* first = s.mantleOf(born[0]);
    expect(first && first->mantle.rfind("cw09", 0) == 0 && first->how == "spare" && first->born,
           "a spare of its dialect: " + (first ? first->mantle + " " + first->how : std::string("none")));
    expect(first != nullptr, "bound");
    const auto* firstCard = s.mantleCard(first->mantle);
    expect(first->traits.size() == 7 && first->traits.at("honesty") == firstCard->traits.at("honesty"), "all seven traits, honesty the card's");
    expect(first->from == "coldwater" && (first->accent == "broad" || first->accent == "light"), "born in Coldwater, its origin for life");
    const auto* child = s.spec(born[0]);
    expect(child->name.find("Reed") != std::string::npos && child->name.rfind("Wenna", 0) != 0 && child->name.rfind("Ottar", 0) != 0,
           "a Coldwater name, its mother's surname, no parent's first name: " + child->name);
    expect(child->personality.find(';') != std::string::npos || child->personality.back() == '.', "a personality from the card: " + child->personality);
    expect(first->mantle != "cw091" || child->appearance.sex == "female", "cw091 fits only a daughter");
    changes = s.takeMantleChanges();
    expect(std::any_of(changes.begin(), changes.end(), [&](const MantleChange& c) { return c.kind == "bind" && c.npc == born[0]; }),
           "the host is told to write the binding");

    // Agna dies on day 30 (Ottar's mother); her child comes when the Reeds can have another (300 days on). Then Joss dies:
    // the Reeds' next pup takes Agna's mantle, rested, as an echo (its last wearer kin of a parent), its marks kept.
    t.dead.insert("agna");
    t.knew["agna"] = {"wenna", "ottar"};
    days(12, 30);
    s.died("agna", 30.5, &w);
    days(30, 400);
    expect(born.size() == 2, "Agna's child too");
    const auto* second = s.mantleOf(born[1]);
    expect(second && second->mantle != "cw001" && second->mantle != "cw017", "nothing rested yet: another spare (" + (second ? second->mantle : std::string("none")) + ")");
    t.dead.insert("joss");
    s.died("joss", 400.5, &w);
    days(400, 700);
    expect(born.size() == 3, "Joss's child");
    const auto* third = s.mantleOf(born[2]);
    expect(third && third->mantle == "cw001" && third->how == "echo" && third->echoOf == "agna" && third->markDialect.empty(),
           "an echo of the grandmother, its marks kept: " + (third ? third->mantle + " " + third->how : std::string()));
    expect(third->traits.at("honesty") == "sly", "and her honesty with it (Q4)");

    // The Reeds have had three: the next birth goes to... nobody else of an age, so it waits. A Frosthollow couple, with
    // no Frosthollow mantle: the Coldwater pool lends one (cw017 rested, known by Wenna, who lives in Coldwater but not
    // in the birth town), its marks drawn from Frosthollow's bank, and a Frosthollow name: the grandfather's (named_for 1).
    expect(s.mantlePools(700, w).size() >= 1, "the pools");
    {
        AuthoredRoster north;
        north.residents = {
            person("brand", "Brand Coldfell", "civilian", "cutting ice", 33, "male", {"fh_hall", 2.5, 2.5}, {"fh_ice", 2.5, 2.5}),
            person("sigrid", "Sigrid Coldfell", "civilian", "mending nets", 30, "female", {"fh_hall", 3.5, 2.5}, {"fh_nets", 2.5, 2.5}),
            person("ulf", "Ulf Coldfell", "civilian", "resting", 71, "male", {"fh_old", 2.5, 2.5}, {"fh_old", 2.5, 2.5}),
            person("old", "Old Wick", "civilian", "resting", 75, "male", {"cw_wick", 2.5, 2.5}, {"cw_wick", 2.5, 2.5})};
        north.kin = {{"brand", "sigrid", "spouse"}, {"ulf", "brand", "parent"}};
        Society n(Roster::None);
        n.configure(north);
        n.setMantleRules(lakeRules());
        n.setMantleCards({card("cw017", "coldwater", "stage=adult|old", {{"honesty", "plain"}}, "", {"oath", "saying"})});
        Town nt;
        auto nw = nt.world(n);
        n.tendCareers(1.3, nw);
        expect(n.bindMantle("old", "cw017", "authored", 1, why), why);
        nt.dead.insert("old");
        nt.knew["old"] = {"brand"};
        n.died("old", 2.5, &nw);
        bool told = false;
        for (int day = 2; day < 380; ++day)
        {
            n.tendCareers(day + .3, nw);
            for (const auto& r : n.takeRequests())
                expect(r.mantle.empty(), "Old Wick's child: nothing rested, no spare, no mantle");
            for (const auto& c : n.takeMantleChanges())
                told = told || (c.kind == "none" && c.dialect == "frosthollow");
        }
        expect(told, "a birth with no mantle to give is born as before, and said so (RATW_MANTLE none for frosthollow)");
        nt.dead.insert("ulf");
        n.died("ulf", 380.5, &nw);
        ResidentRequest asked;
        for (int day = 380; day < 390 && asked.kind.empty(); ++day)
        {
            n.tendCareers(day + .3, nw);
            for (const auto& r : n.takeRequests())
                asked = r;
        }
        expect(asked.kind == "birth" && asked.dialect == "frosthollow", "a Frosthollow birth");
        expect(asked.mantle == "cw017" && asked.redrawMarks && asked.markDialect == "frosthollow" && asked.voice.dialect == "coldwater",
               "a neighbouring pool's mantle, its marks drawn from the child's own dialect: " + asked.mantle);
        expect(asked.sex != "male" || asked.name == "Ulf Coldfell", "a son is named for his grandfather (named_for 1): " + asked.name);
        expect(asked.sex != "female" || asked.name == "Agna Coldfell", "a daughter takes the pool's other name: " + asked.name);
    }

    // The pools (§12.1): Coldwater's cw017 rested and ready, cw091 or cw090 spare; Ridgemere's rm090 spare; births
    // expected for Coldwater (the only couple of an age, though they have had three: none).
    const auto pools = s.mantlePools(700, w);
    const auto cw = std::find_if(pools.begin(), pools.end(), [](const MantlePoolHealth& p) { return p.dialect == "coldwater"; });
    const auto rm = std::find_if(pools.begin(), pools.end(), [](const MantlePoolHealth& p) { return p.dialect == "plain"; });
    expect(cw != pools.end() && rm != pools.end(), "a pool for each dialect with mantles");
    expect(cw->ready == 1 && cw->spare + cw->ready + cw->resting == 2 && rm->spare == 1,
           "the pools by dialect: coldwater spare " + std::to_string(cw->spare) + " resting " + std::to_string(cw->resting) + " ready " +
               std::to_string(cw->ready));
    expect(cw->honesty.size() >= 1 && cw->livingHonesty.size() >= 1, "with their honesty mix against the living's");

    // The ledger survives a save.
    const auto saved = wire::society(s.state());
    std::string error;
    json::Value parsed;
    expect(json::parse(json::dump(saved), parsed, error), "the save parses");
    const auto back = wire::readSociety(parsed);
    expect(back.careers.mantles.size() == s.state().careers.mantles.size() && back.careers.mantles.at(born[2]).echoOf == "agna" &&
               back.careers.mantles.at(born[2]).traits.at("honesty") == "sly" && back.careers.mantles.at("myrtle").releasedDay == 2.5 &&
               back.careers.mantles.at("myrtle").knownBy == std::vector<std::string>{"wenna"},
           "the mantle ledger is saved and read back whole");

    // Brought back before anyone took it up: worn again.
    t.dead.insert("ida");
    s.bindMantle("ida", "rm090", "dm", 700, why);
    s.died("ida", 700.5, &w);
    expect(s.wearerOf("rm090").empty(), "released at death");
    t.dead.erase("ida");
    s.revived("ida");
    expect(s.wearerOf("rm090") == "ida", "brought back, she wears it again");
}

// A harness world shows a handoff (§18, P10's gate): the talk town wearing the fixture. Myrtle Stillwater, the Coldwater
// keeper who wears cw017 (written for her), dies; the Saltwell rakers die too, so the only couple of an age left is the
// Marshes of Coldwater. A game year on (the calendar moved, nothing else run), their child is born: cw017 has rested its
// 365 days and nobody in Coldwater knew her well, so it passes on whole. The child's voice is the card's (Q4), its name
// is Coldwater's, its origin Coldwater for life, its parents its own.
void handoffInTheTalkTown()
{
    const auto dir = fs::temp_directory_path() / ("ratw-handoff-" + std::to_string(::getpid()));
    fs::remove_all(dir);
    fs::copy(Fixture, dir, fs::copy_options::recursive);
    const auto save = (fs::temp_directory_path() / ("ratw-handoff-" + std::to_string(::getpid()) + ".json")).string();
    {
        game::Options o;
        o.talkData = RATW_SOURCE_DIR "/Data/Talk";
        o.voiceData = RATW_SOURCE_DIR "/Data/Voice";
        o.scratch = true;
        o.forkSnapshots = false;
        o.savePath = save;
        o.worldExport = RATW_SOURCE_DIR "/Tests/fixtures/talk_town";
        o.mantlesFrom = dir.string();
        auto g = std::make_unique<game::Game>(o);
        std::vector<std::string> notes;
        g->log = [&](const char*, const std::string& m) { notes.push_back(m); };
        std::string problem;
        expect(g->start(problem), "the talk town starts: " + problem);
        auto& world = g->world();
        auto& society = world.society();
        const auto* authored = society.mantleOf("tt_co_keeper");
        expect(authored && authored->mantle == "cw017" && authored->how == "authored", "the keeper's binding is in the ledger, authored");
        expect(society.mantleRules().names.size() == 8 && society.mantleRules().restDays == 365, "the eight name pools and the rest");
        const auto said = [&](const std::string& what) {
            return std::any_of(notes.begin(), notes.end(), [&](const std::string& n) { return n.find(what) != std::string::npos; });
        };
        const double died = world.calendarDays();
        for (const auto* who : {"tt_co_keeper", "tt_sa_raker", "tt_sa_raker_wife"})
            expect(world.setDead(who, true).ok, std::string("a death: ") + who);
        for (int i = 0; i < 4; ++i)
            g->tick(.05);
        const auto* rest = society.mantleOf("tt_co_keeper");
        expect(rest && rest->releasedDay >= 0 && !g->mantleWearers().count("tt_co_keeper") && said("RATW_MANTLE cw017 rests"),
               "her mantle rests");
        // A year on: the children owed are due, and the day's turn has them born.
        expect(world.advanceCalendar(370).ok, "a year on");
        std::string child;
        for (int i = 0; i < 200 && child.empty(); ++i)
        {
            g->tick(.05);
            for (const auto& [npc, b] : society.state().careers.mantles)
                if (b.mantle == "cw017" && npc != "tt_co_keeper" && b.releasedDay < 0)
                    child = npc;
        }
        expect(!child.empty(), "a child wears cw017");
        const auto* b = society.mantleOf(child);
        const auto* e = world.entity(child);
        expect(b->how == "reused" && b->born && b->from == "coldwater" && b->markDialect.empty(), "reused whole, born in Coldwater: " + b->how);
        const auto& parents = society.state().careers.parents.at(child);
        expect(std::find(parents.begin(), parents.end(), "tt_co_boatman_wife") != parents.end(), "the Marshes' child");
        expect(e && e->name.find("Marsh") != std::string::npos, "a Coldwater name and its mother's surname: " + (e ? e->name : std::string()));
        expect(g->mantleWearers().count(child) && g->mantleWearers().at(child) == "cw017", "and the game hears it wear it");
        auto q = g->talkQuery(child, "tt_co_boatman", "talk.day");
        expect(q.a.facts["mantle"] == "cw017" && q.a.facts["from"] == "coldwater" && q.a.facts["quirk"] == "swims_in_the_lake_even_in_autumn" &&
                   q.a.facts["temper"] == "calm" && q.a.facts["honesty"] == "plain" && q.a.facts["stage"] == "young",
               "its voice is the card's: " + q.a.facts["temper"] + " " + q.a.facts["honesty"]);
        expect(society.honestyOf(child) == "plain", "its honesty the society's too (who turns thief)");
        const auto* spec = society.spec(child);
        expect(spec && spec->personality.rfind("Calm", 0) == 0 && spec->personality.find("swims in the lake") != std::string::npos,
               "a personality from the card, not its mother's: " + (spec ? spec->personality : std::string()));
        expect(said("wears cw017 (reused"), "the handoff is logged");
        std::cout << "handoff: " << e->name << " (" << child << "), born day " << int(world.calendarDays()) << " to Ysolde and Corwin Marsh, "
                  << "wears cw017 (" << b->how << "), last worn by Myrtle Stillwater, who died day " << int(died) << "; temper "
                  << q.a.facts["temper"] << ", honesty " << q.a.facts["honesty"] << ", from " << q.a.facts["from"] << " ("
                  << q.a.facts["accent"] << "); \"" << spec->personality << "\"\n";
        for (const auto& p : world.mantlePools())
            std::cout << "pool " << p.dialect << " spare=" << p.spare << " resting=" << p.resting << " ready=" << p.ready
                      << " births180=" << p.births180 << (p.low ? " LOW" : "") << "\n";
        g->save();
    }
    std::error_code ec;
    fs::remove(save, ec);
    fs::remove_all(dir, ec);
}

// An arrival from the Country binds a mantle from the pool of where it grew up (§12.6), and keeps its own voice.
void arrivalsBindOne()
{
    AuthoredRoster roster;
    roster.residents = {
        person("ivar", "Ivar Nailham", "civilian", "unloading ships at the piers", 40, "male", {"sump_rents", 2.5, 2.5}, {"piers", 3.5, 3.5}),
        person("una", "Una Nailham", "civilian", "unloading ships at the piers", 38, "female", {"sump_rents", 3.5, 2.5}, {"piers", 4.5, 3.5})};
    auto fisher = person("cty_fisher", "Kjell Saltdale", "civilian", "fishes the waters off Greywater", 30, "male", {}, {});
    fisher.description = "A tall grey wolf, who fishes the waters off Greywater.";
    fisher.purse = 49;
    roster.unplaced = {fisher};
    roster.kin = {{"ivar", "una", "spouse"}};
    Society s(Roster::None);
    s.configure(roster);
    s.setMantleRules(lakeRules());
    s.setMantleCards({card("cw095", "coldwater", "sex=male stage=adult|old", {{"honesty", "frank"}}), card("rm095", "plain", "", {})});
    Town t;
    t.origins["cty_fisher"] = {"coldwater", "broad"};
    auto w = t.world(s);
    s.tendCareers(1.3, w);
    t.dead.insert("ivar");
    s.died("ivar", 1.5, &w);
    ResidentRequest arrival;
    for (int day = 2; day <= 14 && arrival.kind.empty(); ++day)
    {
        s.tendCareers(day + .3, w);
        for (auto& r : s.takeRequests())
            if (r.kind == "arrival")
                arrival = r;
    }
    expect(arrival.templateId == "cty_fisher" && arrival.mantle == "cw095" && arrival.mantleHow == "arrival" && arrival.dialect == "coldwater",
           "the fisher comes with a Coldwater mantle: " + arrival.mantle);
    expect(s.arrive(arrival), "he comes in");
    s.welcome(arrival, arrival.templateId);
    const auto* b = s.mantleOf("cty_fisher");
    expect(b && b->mantle == "cw095" && b->how == "arrival" && !b->born && b->traits.empty(), "bound, keeping the voice he has");
}

// B3 and B4 (§16): about 800 mantles at the richer size loaded in one batch.
long rssKb()
{
    std::ifstream in("/proc/self/statm");
    long pages = 0, resident = 0;
    in >> pages >> resident;
    return resident * (sysconf(_SC_PAGESIZE) / 1024);
}

long heapKb() { return long(mallinfo2().uordblks / 1024); }

int bench(int count)
{
    auto& lib = library();
    // Each synthetic mantle is cw017 (23 items, every kind) and five more copies of its grown lines under other names (about 63
    // items, the richer size's mean: 50,700 items for about 800 living wearers), ids in turn.
    const auto base = read(Fixture + "/coldwater/cw017.mantle");
    std::string extra;
    for (int k = 0; k < 5; ++k)
    {
        std::istringstream in(base);
        bool grown = false;
        for (std::string line; std::getline(in, line);)
        {
            if (line == "grown")
                grown = true;
            else if (line == "young" || line.rfind("story", 0) == 0 || line.rfind("scene", 0) == 0)
                grown = false;
            if (grown && line.rfind("line ", 0) == 0)
            {
                const auto colon = line.find(" : ");
                const auto bracket = line.find('[');
                const auto end = bracket != std::string::npos && bracket < colon ? bracket : colon;
                auto head = line.substr(0, end);
                if (head.find('@') != std::string::npos)
                    head.erase(head.find('@'), head.find(' ', head.find('@')) - head.find('@') + 1);
                extra += head.insert(5, "@x" + std::to_string(k) + "_" + std::to_string(extra.size()) + " ") + line.substr(end) +
                         std::string(k == 0 ? " Mind." : k == 1 ? " Truly." : k == 2 ? " Aye." : k == 3 ? " So." : " Well.") + "\n";
            }
        }
    }
    auto withExtra = base;
    withExtra.replace(withExtra.find("\nyoung\n"), 0, "\n" + extra);
    std::vector<talk::MantleText> texts;
    std::size_t items = 0;
    for (int i = 0; i < count; ++i)
    {
        char id[8];
        std::snprintf(id, sizeof id, "zz%04d", i);
        auto t = withExtra;
        t.replace(t.find("mantle cw017"), 12, std::string("mantle ") + id);
        t.replace(t.find("written_for tt_co_keeper"), 24, std::string("written_for tt_") + id);
        texts.push_back({id, t, id});
    }
    {
        talk::Mantle m;
        std::vector<std::string> e;
        talk::readMantle(texts.front().text, "one", m, e);
        items = m.items.size() * std::size_t(count);
    }
    const long before = rssKb(), heapBefore = heapKb();
    const auto t0 = std::chrono::steady_clock::now();
    std::vector<talk::Library::MantleRefusal> refused;
    const auto loaded = lib.setMantles(texts, refused);
    const auto t1 = std::chrono::steady_clock::now();
    const long after = rssKb(), heapAfter = heapKb();
    const double batchMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::string problem;
    std::vector<double> one;
    for (int i = 0; i < 20; ++i)
    {
        const auto a = std::chrono::steady_clock::now();
        lib.setMantle(texts[std::size_t(i * 37 % count)], problem);
        one.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count());
    }
    std::sort(one.begin(), one.end());
    // A pick for a wearer and for one with no mantle, each as the director asks.
    talk::Query q;
    q.concept = "talk.weather";
    q.a = speaker("x", "zz0007", "coldwater");
    q.b = speaker("y", "", "coldwater");
    q.facts = {{"here.time", "day"}, {"here.place", "street"}, {"here.weather", "clear"}, {"here.outdoors", "clear"},
               {"pair.band", "acquaintances"}, {"here.region", "coldwater"}};
    const auto timePick = [&](const talk::Query& query) {
        const auto a = std::chrono::steady_clock::now();
        for (std::uint64_t s = 1; s <= 400; ++s)
            lib.pick(query, s, {}, {});
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count() / 400;
    };
    const double wearer = timePick(q);
    auto none = q;
    none.a = speaker("x", "", "coldwater");
    const double plain = timePick(none);
    std::cout << "B3/B4 bench: " << loaded << " mantles loaded (" << refused.size() << " refused), " << items << " items, "
              << lib.mantleEntries() << " rules and scenes\n"
              << "  batch load (setMantles, parse and index): " << int(batchMs) << " ms (B3: 3,000 ms or less, with the rows read)\n"
              << "  one mantle swapped (mantle.sync's setMantle): median " << one[one.size() / 2] << " ms, slowest " << one.back()
              << " ms (B3: 50 ms or less)\n"
              << "  memory added: " << (after - before) / 1024 << " MB resident, " << (heapAfter - heapBefore) / 1024
              << " MB in use on the heap (B4: 150 MB or less)\n"
              << "  a talk.weather pick: " << wearer << " ms for a wearer, " << plain << " ms for one without (about 0.2 ms today)\n";
    const bool ok = refused.empty() && batchMs <= 3000 && one[one.size() / 2] <= 50 && (after - before) / 1024 <= 150;
    std::cout << (ok ? "within budget\n" : "OVER BUDGET\n");
    return ok ? 0 : 1;
}
// A world database's mantles in a scratch game (RATW_DATABASE_URL names the database; nothing is written), driven from
// stdin, for the end-to-end check of the rows: "sync ID" (mantle.sync, as the DM's action), "voice NPC" (voice.sync NPC),
// "says NPC CONCEPT" (what the resident says for a concept alone), "quit". One line out for each.
int database(const std::string& key)
{
    const char* url = std::getenv("RATW_DATABASE_URL");
    if (!url)
    {
        std::cerr << "--db needs RATW_DATABASE_URL\n";
        return 2;
    }
    game::Options o;
    o.talkData = RATW_SOURCE_DIR "/Data/Talk";
    o.voiceData = RATW_SOURCE_DIR "/Data/Voice";
    o.scratch = true;
    o.forkSnapshots = false;
    o.database = key;
    o.conninfo = url;
    o.savePath = "/tmp/ratw-mantle-db-" + std::to_string(::getpid()) + ".json";
    game::Game g(o);
    g.log = [](const char*, const std::string& m) {
        if (m.find("RATW_MANTLE") != std::string::npos)
            std::cout << "LOG " << m << std::endl;
    };
    std::string problem;
    if (!g.start(problem))
    {
        std::cout << "FAILED " << problem << std::endl;
        return 1;
    }
    std::cout << "READY " << g.mantleWearers().size() << " wearers" << std::endl;
    for (std::string line; std::getline(std::cin, line);)
    {
        std::istringstream in(line);
        std::string what, a, b;
        in >> what >> a >> b;
        if (what == "quit")
            break;
        if (what == "sync" || what == "voice")
        {
            const auto r = g.applyDmAct({std::string("1"), std::string(what == "sync" ? "mantle.sync" : "voice.sync"), a,
                                         std::string("e2e"), std::string("{}")});
            std::string m = r.message;
            std::replace(m.begin(), m.end(), '\n', ' ');
            std::cout << "RESULT " << (r.ok ? "ok " : "refused ") << m << std::endl;
        }
        else if (what == "says")
        {
            auto q = g.talkQuery(a, "", b);
            const auto said = g.talkLibrary().pick(q, 1, {}, {});
            std::cout << "SAYS " << said.id << " | " << (said.lines.empty() ? "" : said.lines.front().second) << std::endl;
        }
    }
    return 0;
}
// Phase 10, end to end in a world database (tools/test_mantle_load.py drives it, on a throwaway world): a game that writes
// (not scratch), driven from stdin: "assign NPC MANTLE" and "release NPC" (the DM's acts), "kill NPC", "days N" (the
// calendar moved N days and the game ticked until the children owed are made), "quit" (saved). One line out for each;
// "LOG" lines are the game's RATW_MANTLE notes, "BORN id name mantle how" each child made.
int databaseLife(const std::string& key)
{
    const char* url = std::getenv("RATW_DATABASE_URL");
    if (!url)
    {
        std::cerr << "--db-life needs RATW_DATABASE_URL\n";
        return 2;
    }
    game::Options o;
    o.talkData = RATW_SOURCE_DIR "/Data/Talk";
    o.voiceData = RATW_SOURCE_DIR "/Data/Voice";
    o.forkSnapshots = false;
    o.database = key;
    o.conninfo = url;
    game::Game g(o);
    g.log = [](const char*, const std::string& m) {
        if (m.find("RATW_MANTLE") != std::string::npos || m.find("RATW_RESIDENT") != std::string::npos)
            std::cout << "LOG " << m << std::endl;
    };
    std::string problem;
    if (!g.start(problem))
    {
        std::cout << "FAILED " << problem << std::endl;
        return 1;
    }
    std::cout << "READY " << g.mantleWearers().size() << " wearers" << std::endl;
    int act = 1;
    std::set<std::string> known;
    for (const auto& [id, life] : g.world().society().state().residents)
        known.insert(id);
    for (std::string line; std::getline(std::cin, line);)
    {
        std::istringstream in(line);
        std::string what, a, b;
        in >> what >> a >> b;
        if (what == "quit")
            break;
        if (what == "assign" || what == "release")
        {
            const auto r = g.applyDmAct({std::to_string(act++), "mantle." + what, a, std::string("e2e"),
                                         what == "assign" ? "{\"mantle\": \"" + b + "\"}" : std::string("{}")});
            std::cout << "RESULT " << (r.ok ? "ok " : "refused ") << r.message << std::endl;
        }
        else if (what == "kill")
        {
            const auto r = g.world().setDead(a, true);
            for (int i = 0; i < 4; ++i)
                g.tick(.05);
            std::cout << "RESULT " << (r.ok ? "ok " : "refused ") << r.message << std::endl;
        }
        else if (what == "days")
        {
            g.world().advanceCalendar(std::atof(a.c_str()));
            for (int i = 0; i < 200; ++i)
                g.tick(.05);
            for (const auto& [id, life] : g.world().society().state().residents)
                if (known.insert(id).second)
                {
                    const auto* binding = g.world().society().mantleOf(id);
                    const auto* e = g.world().entity(id);
                    std::cout << "BORN " << id << " " << (e ? e->name : std::string("?")) << " | "
                              << (binding ? binding->mantle + " " + binding->how : std::string("none")) << std::endl;
                }
            for (const auto& p : g.world().mantlePools())
                std::cout << "POOL " << p.dialect << " spare=" << p.spare << " resting=" << p.resting << " ready=" << p.ready
                          << " births180=" << p.births180 << std::endl;
            std::cout << "RESULT ok day " << int(g.world().calendarDays()) << std::endl;
        }
    }
    g.save();
    std::cout << "SAVED" << std::endl;
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    setenv("RATW_DATA_DIR", (fs::path(RATW_SOURCE_DIR) / "Data").string().c_str(), 0);
    try
    {
        if (argc > 2 && std::string(argv[1]) == "--db")
            return database(argv[2]);
        if (argc > 2 && std::string(argv[1]) == "--db-life")
            return databaseLife(argv[2]);
        if (argc > 1 && std::string(argv[1]) == "--bench")
            return bench(argc > 2 ? std::atoi(argv[2]) : 805);
        theSyntax();
        theChecks();
        wornByItsWearer();
        theHotReload();
        inTheGame();
        passingOn();
        arrivalsBindOne();
        handoffInTheTalkTown();
    }
    catch (const std::exception& e)
    {
        std::cerr << "mantle tests FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "mantle tests passed (" << checks << " checks)\n";
    return 0;
}
