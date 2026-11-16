// Talk (RatwTalk.h; Docs/Design/62-talk.md): the file formats and their checks, who says what in a scene, rules and
// how one is chosen (tiers with chance, strict priority, filler, not heard first), the grammar (layers, conditions,
// habits, modifiers, pronouns, binding), and the real library loading whole.
#include "RatwMarket.h"
#include "RatwTalk.h"
#include "RatwTopics.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <map>
#include <stdexcept>
#include <string>
#include <unistd.h>

using namespace ratw::talk;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

const std::string Dir = RATW_SOURCE_DIR "/Data/Talk";

// The real facts, settings, grammars and groups, without the real rules and scenes.
Library foundation()
{
    Library lib;
    std::string problem;
    expect(lib.loadFoundation(Dir, problem), "the foundation loads: " + problem);
    return lib;
}

Speaker person(const std::string& id, const std::string& name, const std::string& sex, const std::string& stage,
               const std::string& job, const std::string& role)
{
    Speaker s;
    s.facts = {{"id", id}, {"name", name}, {"sex", sex}, {"stage", stage}, {"job", job}, {"role", role}, {"age", "40"}};
    return s;
}

Query moment(const std::string& concept)
{
    Query q;
    q.concept = concept;
    q.facts = {{"here.region", "oakhaven"}, {"here.place", "market"}, {"pair.band", "friends"}, {"here.time", "day"},
               {"here.season", "autumn"}, {"here.day", "market"}, {"here.weather", "rain"}, {"topic.item", "herbs"},
               {"topic.dir", "up"}, {"topic.price", "3 pennies"}, {"here.town", "Oakhaven"}};
    q.a = person("maren", "Maren", "female", "adult", "baker", "merchant");
    q.b = person("ivo", "Ivo", "male", "old", "fisher", "civilian");
    return q;
}

const std::function<bool(const std::string&)> None = [](const std::string&) { return false; };

const char* Sample = R"(
group woodsy = oakhaven mirebridge

scene herbs_dear
when topic=prices topic.item=herbs topic.dir=up here.region=woodsy place=market|street
weight 2
a : Herbs at #topic.price# now, #b.name#.
b[merchant] : I pay more, you pay more.
b[old] : In my day a bundle cost less than a smile.
b : Robbery.
a?[rivals] : And I suppose you're pleased.
a?[friends] : Come by tonight; I'll share mine.

scene anywhere_talk
when topic=smalltalk
a : Morning, #b.name#. | Well met, #b.name#.
b : Morning.

scene needs_subject
when topic=smalltalk
weight 5
a : Did you hear about #topic.subject#?
b : No.

scene a_bark
when concept=bark
a[merchant] : Fresh bread! Still warm!
a : Mind the puddles.
)";
} // namespace

void theFormat()
{
    auto lib = foundation();
    std::string problem;
    expect(lib.parse(Sample, "sample.talk", problem), "the sample parses: " + problem);
    expect(lib.size() == 4 && lib.has("talk.prices") && lib.has("bark"), "four scenes, indexed by concept");
    const auto bad = [&](const std::string& text, const std::string& says) {
        auto l = foundation();
        std::string p;
        expect(!l.parse(text, "bad.talk", p) && p.find(says) != std::string::npos, "refused (" + says + "): " + p);
        expect(l.size() == 0, "and nothing of it loaded");
    };
    bad("scene x\nwhen topic=smalltalk\na : hi\n", "needs a line b always says");
    bad("scene x\nwhen topic=smalltalk\nb : hi\n", "needs a line a always says");
    bad("scene x\na : hi\nb : yo\n", "has no concept");
    bad("scene x\nwhen topic=gossiping\na : hi\nb : yo\n", "is not a concept");
    bad("scene x\nwhen topic=smalltalk place=moon\na : hi\nb : yo\n", "'moon' is not a place");
    bad("scene x\nwhen topic=smalltalk here.region=atlantis\na : hi\nb : yo\n", "not a region");
    bad("scene x\nwhen topic=smalltalk wealthy=yes\na : hi\nb : yo\n", "unknown fact 'wealthy'");
    bad("scene x\nwhen topic=smalltalk\na : hello #topic.nobody#\nb : yo\n", "no fact topic.nobody");
    bad("scene x\nwhen topic=smalltalk\na : hello #a.shoe#\nb : yo\n", "a person has no fact 'shoe'");
    bad("scene x\nwhen topic=smalltalk\na : #wibble#, then\nb : yo\n", "#wibble# is not in the plain grammar");
    bad("scene x\nwhen topic=smalltalk\na : hello #a.name\nb : yo\n", "a # without its closing #");
    bad("scene x\nwhen topic=smalltalk\na[wizard] : hi\na : hi\nb : yo\n", "'wizard' is not");
    bad("scene x\nwhen topic=smalltalk\na : hi\nb : yo\nscene x\nwhen topic=smalltalk\na : hi\nb : yo\n", "defined twice");
    bad("scene x\nwhen concept=bark\na : hi\nb : yo\n", "one speaker (a) only");
    bad("scene x\nwhen topic=smalltalk\na : " + std::string(170, 'w') + "\nb : yo\n", "160 characters");
    bad("scene x\nwhen topic=smalltalk self.age>=old\na : hi\nb : yo\n", "not a number");
    bad("scene x\nwhen topic=smalltalk here.place>=2\na : hi\nb : yo\n", "can't be compared");
    bad("rule x\nwhen concept=bark\na : hi\n", "a rule says its words with 'say'");
    bad("rule x\nwhen concept=bark\n", "says nothing");
    bad("scene x\nwhen topic=smalltalk\nsay hi\n", "'say' is a rule's");
    bad("a : hi\n", "expected 'rule <id>' or 'scene <id>' first");
    bad("rule x\nwhen concept=bark\npriority 1.5\nsay hi\n", "whole number");
}

void whoSaysWhat()
{
    auto lib = foundation();
    std::string problem;
    lib.parse(Sample, "sample.talk", problem);
    const auto& herbs = *lib.find("herbs_dear");
    auto q = moment("talk.prices");
    auto r = lib.render(herbs, q, 1);
    expect(r.lines.size() == 3, "a, b, and a's optional turn for friends");
    expect(r.lines[0].second == "Herbs at 3 pennies now, Ivo.", "facts filled: " + r.lines[0].second);
    expect(r.lines[1].second.find("In my day") != std::string::npos, "b's first fitting line: b is old, not a merchant");
    expect(r.lines[2].second.find("share mine") != std::string::npos && r.lines[2].first == 0, "friends: the optional turn");
    q.b.facts["role"] = "merchant";
    expect(lib.render(herbs, q, 1).lines[1].second.find("I pay more") != std::string::npos, "the first tag that holds wins");
    q.facts["pair.band"] = "strangers";
    expect(lib.render(herbs, q, 1).lines.size() == 2, "optional turns that don't hold are left out");
    q.facts["here.region"] = "mirebridge";
    expect(!lib.render(herbs, q, 1).id.empty(), "a group stands for its regions");
    q.facts["here.region"] = "larkfield";
    expect(lib.render(herbs, q, 1).id.empty(), "elsewhere, the scene doesn't fit");
    q.facts["here.region"] = "oakhaven";
    q.facts.erase("topic.price");
    expect(lib.render(herbs, q, 1).id.empty(), "a fact that's missing: not used");
}

// Alternatives written in parallel answer each other: the second reply to the second remark.
void parallelAlternatives()
{
    auto lib = foundation();
    std::string problem;
    expect(lib.parse(R"(
scene paired
when topic=smalltalk
a : one | two | three
b : reply one | reply two | reply three
a[merchant] : (a merchant's aside)
b : end one | end two | end three
)", "paired.talk", problem), problem);
    auto q = moment("talk.smalltalk");
    std::set<std::string> openings;
    for (std::uint64_t seed = 0; seed < 60; ++seed)
    {
        const auto r = lib.pick(q, seed, None, {});
        expect(r.lines.size() == 4 && r.lines[2].second == "(a merchant's aside)", "every turn spoken");
        expect(r.lines[1].second == "reply " + r.lines[0].second && r.lines[3].second == "end " + r.lines[0].second,
               "replies follow the remark they answer: " + r.lines[0].second + " / " + r.lines[1].second);
        openings.insert(r.lines[0].second);
    }
    expect(openings.size() == 3, "and each opening comes up");
    auto caps = foundation();
    expect(caps.parse(R"(
scene caps
when topic=player
a : #topic.subject# again. Is #topic.subject# lost? Look! #topic.subject#.
b : Hm.
)", "caps.talk", problem), problem);
    auto p = moment("talk.player");
    p.facts["topic.subject"] = "that grey stranger";
    expect(caps.pick(p, 1, None, {}).lines[0].second == "That grey stranger again. Is that grey stranger lost? Look! That grey stranger.",
           "words opening a sentence take a capital");
    auto the = foundation();
    expect(the.parse(R"(
scene feast
when topic=festival
a : The #here.festival# bonfire! Lighting it for #here.festival#. #here.festival# at last.
b : Hm.
)", "feast.talk", problem), problem);
    auto f = moment("talk.festival");
    f.facts["here.festival"] = "the Greening";
    expect(the.pick(f, 1, None, {}).lines[0].second == "The Greening bonfire! Lighting it for the Greening. The Greening at last.",
           "a festival's \"the\" isn't said twice");
}

void choosing()
{
    auto lib = foundation();
    std::string problem;
    lib.parse(Sample, "sample.talk", problem);
    auto q = moment("talk.smalltalk");
    for (std::uint64_t seed = 0; seed < 20; ++seed)
        expect(lib.pick(q, seed, None, {}).id == "anywhere_talk", "a scene whose facts are missing is never picked");
    q.facts["topic.subject"] = "Old Rook";
    int subject = 0;
    for (std::uint64_t seed = 0; seed < 200; ++seed)
        subject += lib.pick(q, seed, None, {}).id == "needs_subject";
    expect(subject > 120, "within a tier, the heavier comes up more often (" + std::to_string(subject) + "/200)");
    const auto heardIt = [](const std::string& id) { return id == "needs_subject"; };
    for (std::uint64_t seed = 0; seed < 50; ++seed)
        expect(lib.pick(q, seed, heardIt, {}).id == "anywhere_talk", "one not heard comes before one heard");
    const auto heardAll = [](const std::string&) { return true; };
    expect(!lib.pick(q, 3, heardAll, {}).id.empty(), "with all heard, one is still said");
    expect(lib.pick(q, 3, None, {"needs_subject", "anywhere_talk"}).id.empty(), "never one said here lately");
    // A whole tier heard: the nearest tier with one not heard, before any repeat (phase 7).
    auto fall = foundation();
    expect(fall.parse("rule specific\nwhen concept=talk.smalltalk self.job=baker\nsay Flour.\n"
                      "rule general\nwhen concept=talk.smalltalk\nsay Weather.\n", "fall.talk", problem), problem);
    const auto heardSpecific = [](const std::string& id) { return id == "specific"; };
    for (std::uint64_t seed = 0; seed < 50; ++seed)
        expect(fall.pick(moment("talk.smalltalk"), seed, heardSpecific, {}).id == "general", "the best tier heard: the next");
    int best = 0;
    for (std::uint64_t seed = 0; seed < 50; ++seed)
        best += fall.pick(moment("talk.smalltalk"), seed, heardAll, {}).id == "specific";
    expect(best > 30, "all heard: as if none were (" + std::to_string(best) + "/50)");
    auto bark = moment("bark");
    bark.alone = true;
    bark.b = {};
    expect(lib.pick(bark, 1, None, {}).lines[0].second == "Fresh bread! Still warm!", "a bark: the merchant's call");
    expect(lib.find("a_bark")->repeat, "barks may be repeated");

    // Tiers: the most specific usually, the rest some of the time; twenty general rules can't drown out one specific.
    auto tiers = foundation();
    std::string many = "rule poor\nwhen concept=bark self.job=baker here.place=market\nsay Specific.\n";
    for (int i = 0; i < 20; ++i)
        many += "rule general" + std::to_string(i) + "\nwhen concept=bark\nsay General " + std::to_string(i) + ".\n";
    expect(tiers.parse(many, "tiers.talk", problem), problem);
    int specific = 0;
    for (std::uint64_t seed = 0; seed < 1000; ++seed)
        specific += tiers.pick(bark, seed, None, {}).id == "poor";
    expect(specific > 740 && specific < 860, "two tiers: the best 80% (" + std::to_string(specific) + "/1000)");
    expect(tiers.find("poor")->score == 2 && tiers.find("general0")->score == 0, "a score is the criteria tested");
    Trace why;
    tiers.pick(bark, 3, None, {}, &why);
    expect(why.held.size() == 21 && why.why.find("tier") != std::string::npos, "and why is told: " + why.why);

    auto three = foundation();
    expect(three.parse(R"(
rule best
when concept=bark self.job=baker self.role=merchant
say Best.
rule next
when concept=bark self.job=baker
say Next.
rule low
when concept=bark
say Low.
)", "three.talk", problem), problem);
    std::map<std::string, int> got;
    for (std::uint64_t seed = 0; seed < 2000; ++seed)
        ++got[three.pick(bark, seed, None, {}).id];
    expect(got["best"] > 1260 && got["best"] < 1540 && got["next"] > 300 && got["next"] < 500 && got["low"] > 130 && got["low"] < 270,
           "three tiers: 70/20/10 (" + std::to_string(got["best"]) + "/" + std::to_string(got["next"]) + "/" + std::to_string(got["low"]) + ")");

    // What must be said is; filler only when nothing else is.
    auto strict = foundation();
    expect(strict.parse(R"(
rule must
when concept=bark here.day=market
priority 10
say It's market day!
rule specific
when concept=bark self.job=baker self.role=merchant here.place=market here.time=day
say Bread!
rule filler
when concept=bark
priority -1
say Hm.
)", "strict.talk", problem), problem);
    for (std::uint64_t seed = 0; seed < 50; ++seed)
        expect(strict.pick(bark, seed, None, {}).id == "must", "priority 10 always wins when it holds");
    bark.facts["here.day"] = "work";
    for (std::uint64_t seed = 0; seed < 50; ++seed)
        expect(strict.pick(bark, seed, None, {}).id == "specific", "filler never speaks while anything else holds");
    bark.facts["here.place"] = "street";
    expect(strict.pick(bark, 1, None, {}).id == "filler", "but does when nothing else does");

    // One person's own line outranks everything general about people like them.
    auto named = foundation();
    expect(named.parse(R"(
rule maren
when concept=bark self.id=maren
say Maren's own.
rule bakers
when concept=bark self.job=baker self.role=merchant here.place=street here.time=day
say Any baker's.
)", "named.talk", problem), problem);
    expect(named.find("maren")->score == 5 && named.find("bakers")->score == 4, "a named person counts 5");
    // Numbers compare.
    auto old = foundation();
    expect(old.parse("rule elder\nwhen concept=bark self.age>=60\nsay My bones.\n", "old.talk", problem), problem);
    expect(old.pick(bark, 1, None, {}).id.empty(), "forty is not sixty");
    bark.a.facts["age"] = "61";
    expect(old.pick(bark, 1, None, {}).id == "elder", "sixty-one is");
}

void grammar()
{
    auto lib = foundation();
    std::string problem;
    expect(lib.parseGrammar(R"(
grammar plain
dove = dear
ask = , no?
tool = hammer | anvil
cheese = cheddar | brie | stilton | gouda | edam | feta
child = child
child[here.region=oakhaven] = pup

grammar testvale
greet = Shade on you
greet.broad = Shade an' shelter
child[other.stage=young] = sapling
farewell += Mind the roots
pet!! = one | two | three | four | five | six | seven | eight

grammar maren
oath! = Crumbs | Flour and fire | Yeast take it
)", "test.grammar", problem), problem);
    auto q = moment("bark");
    q.alone = true;
    q.b = {};
    const auto say = [&](const std::string& text, std::uint64_t seed = 1) {
        std::string out;
        lib.expand(text, q, 0, seed, out);
        return out;
    };
    expect(say("#greet#, then.") == "Good day, then." || say("#greet#, then.").find(", then.") != std::string::npos, "plain words");
    for (std::uint64_t seed = 0; seed < 10; ++seed)
        expect(say("#greet#", seed) != "Shade on you", "without the layer, plain");
    q.a.grammar = {"maren", "testvale"};
    for (std::uint64_t seed = 0; seed < 10; ++seed)
        expect(say("#greet#", seed) == "Shade on you", "a dialect's word takes the place of plain's");
    std::set<std::string> farewells;
    for (std::uint64_t seed = 0; seed < 60; ++seed)
        farewells.insert(say("#farewell#", seed));
    expect(farewells.count("Mind the roots") && farewells.count("Farewell"), "+= adds to what's below");
    expect(say("A #child#") == "A pup", "the most specific entry that holds");
    // Accents (phase 3): the dialect is the layer named for where they're from; none, light or broad.
    q.a.facts["from"] = "testvale";
    const auto count = [&](const std::string& accent, const std::string& word) {
        q.a.facts["accent"] = accent;
        int n = 0;
        for (std::uint64_t seed = 0; seed < 400; ++seed)
            n += say("#greet#", seed) == word;
        return n;
    };
    expect(count("none", "Shade on you") == 0, "no accent: plain's words only");
    const int light = count("light", "Shade on you");
    expect(light > 140 && light < 260, "a light accent: the dialect's word about half the time (" + std::to_string(light) + "/400)");
    const int broad = count("broad", "Shade on you"), broader = count("broad", "Shade an' shelter");
    expect(broad + broader == 400 && broader > 100 && broad > 100, "broad: always the dialect's, and its .broad words too");
    expect(count("light", "Shade an' shelter") == 0, "but .broad words only for the broadest");
    q.a.facts.erase("from");
    q.a.facts.erase("accent");
    std::set<std::string> oaths;
    for (std::uint64_t seed = 0; seed < 30; ++seed)
        oaths.insert(say("#oath#", seed));
    expect(oaths.size() == 1, "a habit is chosen once per person, for good");
    q.a.facts["id"] = "someone_else";
    std::set<std::string> others;
    for (const auto* id : {"a", "b", "c", "d", "e", "f", "g", "h"})
    {
        q.a.facts["id"] = id;
        others.insert(say("#oath#"));
    }
    expect(others.size() >= 2, "and others have theirs");
    expect(say("#tool.a#. #tool.a.capitalize#?") == "A hammer. A hammer?" || say("#tool.a#. #tool.a.capitalize#?").find("An anvil") != std::string::npos,
           "the .a modifier: " + say("#tool.a#. #tool.a.capitalize#?"));
    expect(say("two #dove.s#") == "two dears", "the .s modifier");
    expect(say("Wet#ask#.") == "Wet, no?", "a tag that asks takes the line's stop");
    // Pronouns, binding, and lines for one sex.
    auto two = moment("talk.smalltalk");
    std::string out;
    lib.expand("#other.they# and #other.their# boat; #self.they# and #self.themself#.", two, 0, 1, out);
    expect(out == "He and his boat; she and herself.", "pronouns from sex: " + out);
    for (std::uint64_t seed = 0; seed < 10; ++seed)
    {
        lib.expand("[cheese:#cheese#]so #cheese#, #cheese#, #cheese#.", two, 0, seed, out);
        const auto first = out.substr(3, out.find(',') - 3);
        expect(out == "so " + first + ", " + first + ", " + first + ".", "a bound symbol stays itself: " + out);
    }
    std::set<std::string> addresses;
    two.b.facts["stage"] = "young";
    for (std::uint64_t seed = 0; seed < 40; ++seed)
    {
        lib.expand("#address#", two, 0, seed, out);
        addresses.insert(out);
    }
    expect(addresses.count("Lad") && !addresses.count("Lass"), "^m for a boy, never ^f");
    // A habit for each one they speak to: the same name for the same friend, other names for others.
    two.a.grammar = {"testvale"};
    std::set<std::string> forIvo, forOthers;
    for (std::uint64_t seed = 0; seed < 20; ++seed)
    {
        lib.expand("#pet#", two, 0, seed, out);
        forIvo.insert(out);
    }
    for (const auto* id : {"a", "b", "c", "d", "e", "f"})
    {
        two.b.facts["id"] = id;
        lib.expand("#pet#", two, 0, 1, out);
        forOthers.insert(out);
    }
    expect(forIvo.size() == 1 && forOthers.size() >= 3, "a fond name per friend, for good");
    const auto bad = [&](const std::string& text, const std::string& says) {
        auto l = foundation();
        std::string p;
        expect(!l.parseGrammar(text, "bad.grammar", p) && p.find(says) != std::string::npos, "refused (" + says + "): " + p);
    };
    bad("grammar frosthollow\nnot_in_plain = x\n", "is not in the plain grammar");
    bad("grammar plain\nloop = #loop#\n", "uses itself");
    bad("grammar plain\nd1 = #d2#\nd2 = #d3#\nd3 = #d4#\nd4 = #d5#\nd5 = #d6#\nd6 = #d7#\nd7 = x\n", "deep");
    bad("grammar plain\nlong = " + std::string(170, 'w') + "\n", "160 characters");
    bad("grammar plain\nx[place=moon] = y\n", "'moon' is not a place");
    bad("grammar plain\ngreet += Hi\n", "nothing below it");
    bad("grammar plain\ngreet.broad = Hi\n", "plain has no accent");
    bad("x = y\n", "expected 'grammar <name>' first");
}

void chainsAndMemory()
{
    // Phase 4: a rule hands the next beat to the other, who answers with their own best rule; what's said is remembered.
    auto lib = foundation();
    std::string problem;
    expect(lib.parse(R"(
rule complain_poor
when concept=talk.prices self.wealth=poor
say Robbery, these prices.
then other: prices.answer_complaint
remember self: complained_prices 1d
remember pair: talked_money=prices 7d
remember town: heard_complaints 3d
rule answer_merchant
when concept=prices.answer_complaint self.role=merchant
say I pay more, you pay more.
rule answer
when concept=prices.answer_complaint
say Nothing to be done.
rule again
when concept=bark self.mem.complained_prices
say Still too dear.
rule fresh
when concept=bark !self.mem.complained_prices
say Morning.
rule once_ever
when concept=talk.day
once
say I'll say this once.
then self: talk.smalltalk
rule follow_self
when concept=talk.smalltalk
cooldown 2h
say And another thing.
rule once_each
when concept=talk.festival
once per other
say First time I've told you.
)", "chains.talk", problem), problem);
    auto q = moment("talk.prices");
    q.a.facts["wealth"] = "poor";
    q.b.facts["role"] = "merchant";
    // Doc 84 §8.2 (Phase 3): a reply is no longer forced to the best tier: the merchant's own answer most of the time
    // (the best of two tiers: 80%), anyone's now and then.
    int asMerchant = 0;
    for (std::uint64_t seed = 1; seed <= 200; ++seed)
        asMerchant += lib.converse(q, seed, None, {}).lines.at(1).second == "I pay more, you pay more.";
    expect(asMerchant > 130 && asMerchant < 190, "the merchant answers as a merchant, mostly (" + std::to_string(asMerchant) + " of 200)");
    std::uint64_t merchantSeed = 1;
    while (lib.converse(q, merchantSeed, None, {}).lines.at(1).second != "I pay more, you pay more.")
        ++merchantSeed;
    auto said = lib.converse(q, merchantSeed, None, {});
    expect(said.lines.size() == 2 && said.lines[0] == std::pair<int, std::string>{0, "Robbery, these prices."} &&
               said.lines[1] == std::pair<int, std::string>{1, "I pay more, you pay more."},
           "a poor wolf complains, the merchant answers as a merchant");
    expect(said.chain == std::vector<std::string>{"complain_poor", "answer_merchant"}, "both beats are told");
    q.b.facts["role"] = "civilian";
    expect(lib.converse(q, 1, None, {}).lines.at(1).second == "Nothing to be done.", "anyone else, as anyone");
    const auto has = [&](const std::string& scope, const std::string& owner, const std::string& key, const std::string& value, double days) {
        return std::any_of(said.effects.begin(), said.effects.end(), [&](const Effect& e) {
            return e.scope == scope && e.owner == owner && e.key == key && e.value == value && std::abs(e.days - days) < 1e-9;
        });
    };
    expect(has("person", "maren", "complained_prices", "yes", 1) && has("pair", "ivo|maren", "talked_money", "prices", 7) &&
               has("town", "oakhaven", "heard_complaints", "yes", 3),
           "and remembered: by the speaker, the two of them, the town");
    auto bark = moment("bark");
    bark.alone = true;
    expect(lib.pick(bark, 1, None, {}).id == "fresh", "nothing remembered: the fresh line");
    for (const auto& e : said.effects)
        Library::remembered(e, bark);
    expect(bark.a.facts.at("mem.complained_prices") == "yes" && lib.pick(bark, 1, None, {}).id == "again",
           "remembered: self.mem.complained_prices holds");
    // Once, cooldowns, once per other; then self.
    auto day = moment("talk.day");
    auto first = lib.converse(day, 1, None, {});
    expect(first.lines.size() == 2 && first.lines[1].first == 0, "then self: the same speaker goes on");
    for (const auto& e : first.effects)
        Library::remembered(e, day);
    expect(day.a.facts.count("said:once_ever") && lib.converse(day, 1, None, {}).id.empty(), "once is once");
    expect(std::any_of(first.effects.begin(), first.effects.end(),
                       [](const Effect& e) { return e.key == "said:follow_self" && std::abs(e.days - 2.0 / 24) < 1e-9; }),
           "a cooldown is remembered for its time");
    auto fest = moment("talk.festival");
    const auto told = lib.pick(fest, 1, None, {});
    expect(told.effects.size() == 1 && told.effects[0].scope == "pair" && told.effects[0].key == "said:once_each", "once per other: the pair's");
    Library::remembered(told.effects[0], fest);
    expect(lib.pick(fest, 1, None, {}).id.empty(), "not to the same one again");
    fest.b.facts["id"] = "someone_else";
    fest.facts.erase("pair.said:once_each");
    expect(lib.pick(fest, 1, None, {}).id == "once_each", "but to another");

    // A line that moves how one feels about the other (decision 9): an effect for the game to bound and make.
    auto kind = foundation();
    expect(kind.parse("rule compliment\nwhen concept=talk.smalltalk\nsay You look well.\nregard other: affinity +1\nregard self: trust 1\n",
                      "kind.talk", problem), problem);
    const auto warm = kind.pick(moment("talk.smalltalk"), 1, None, {});
    const auto made = [&](const std::string& owner, const std::string& axis) {
        return std::any_of(warm.effects.begin(), warm.effects.end(), [&](const Effect& e) {
            return e.scope == "regard" && e.owner == owner && e.key == axis && std::atof(e.value.c_str()) == 1 && e.why == "compliment";
        });
    };
    expect(made("ivo|maren", "affinity") && made("maren|ivo", "trust"), "the one spoken to warms; the speaker trusts");
    for (const auto* text : {"rule x\nwhen concept=bark\nsay Hm.\nregard other: love +1\n", "rule x\nwhen concept=bark\nsay Hm.\nregard other: affinity +9\n"})
    {
        auto l = foundation();
        std::string p;
        expect(!l.parse(text, "bad.talk", p) && p.find("regard is") != std::string::npos, std::string("refused: ") + p);
    }

    // Nothing is said twice in one talk (doc 84 §8.2, C6): a ping-pong of two rules stops when it would come round again.
    auto loop = foundation();
    expect(loop.parse("rule ping\nwhen concept=talk.smalltalk\nsay Ping.\nthen other: talk.day\nrule pong\nwhen concept=talk.day\nsay Pong.\nthen other: talk.smalltalk\n",
                      "loop.talk", problem), problem);
    const auto lines = loop.converse(moment("talk.smalltalk"), 1, None, {}).lines;
    expect(lines.size() == 2 && lines[1] == std::pair<int, std::string>{1, "Pong."}, "ping, pong, and not ping again");

    // A shrug ends it (settings.json "ends"): the next beat would answer what wasn't said.
    auto shrug = foundation();
    expect(shrug.parse("rule ask\nwhen concept=talk.smalltalk\nsay How's the leg?\nthen other: talk.day\n"
                       "rule mm\nwhen concept=talk.day\nsay Mm.\nthen other: talk.festival\n"
                       "rule advice\nwhen concept=talk.festival\nsay Then see the healer.\n", "shrug.talk", problem), problem);
    const auto short_ = shrug.converse(moment("talk.smalltalk"), 1, None, {}).lines;
    expect(short_.size() == 2 && short_[1].second == "Mm.", "a shrug, and that's the end of it");

    // Marks: a scene's branches, written in one place.
    auto marks = foundation();
    expect(marks.parse(R"(
scene work_how_is_it
when topic=work
a : How's the work?
b[business=failing] #worried : Slow. Too slow.
b[wealth=rich] #proud : Couldn't be better.
b : Steady.
a@worried[friends] : If it comes to it, you come to me first.
a@proud : Must be nice.
a : Good. That's good.
)", "marks.talk", problem), problem);
    auto work = moment("talk.work");
    work.b.facts["business"] = "failing";
    auto r = marks.pick(work, 1, None, {});
    expect(r.lines.size() == 3 && r.lines[2].second == "If it comes to it, you come to me first.", "worried, among friends");
    work.b.facts["business"] = "steady";
    work.b.facts["wealth"] = "rich";
    expect(marks.pick(work, 1, None, {}).lines[2].second == "Must be nice.", "proud");
    work.b.facts["wealth"] = "comfortable";
    expect(marks.pick(work, 1, None, {}).lines[2].second == "Good. That's good.", "neither");

    const auto bad = [&](const std::string& text, const std::string& says) {
        auto l = foundation();
        std::string p;
        expect(!l.parse(text, "bad.talk", p) && p.find(says) != std::string::npos, "refused (" + says + "): " + p);
    };
    // Words a speaker may or may not have (phase 7): self.spouse is there to name, !self.spouse isn't.
    auto named = foundation();
    expect(named.parse("rule wed\nwhen concept=bark self.spouse\nsay #self.spouse# snores.\nrule single\nwhen concept=bark !self.spouse\nsay Hm.\n",
                       "named.talk", problem), problem);
    auto alone = moment("bark");
    alone.alone = true;
    expect(named.pick(alone, 1, None, {}).id == "single", "no spouse: not named");
    alone.a.facts["spouse"] = "Edda";
    const auto wed = named.pick(alone, 1, None, {});
    expect(wed.id == "wed" && wed.lines.at(0).second == "Edda snores.", "a spouse: named");
    bad("rule x\nwhen concept=bark self.wealth\nsay Hm.\n", "is not a sex");

    bad("rule x\nwhen concept=bark\nsay Hm.\nthen other: talk.nowhere\n", "is not a concept");
    bad("rule x\nwhen concept=bark\nsay Hm.\nthen everyone: talk.day\n", "then is 'then other");
    bad("rule x\nwhen concept=bark\nsay Hm.\nremember self: Bad-Key 1d\n", "lower-case");
    bad("rule x\nwhen concept=bark\nsay Hm.\nremember self: x fortnight\n", "remember is");
    bad("rule x\nwhen concept=bark\nsay Hm.\nonce\ncooldown 1h\n", "one of once");
    bad("scene x\nwhen topic=work\na : Hi.\nb : Hi.\nthen other: talk.day\n", "a scene is whole");
    bad("scene x\nwhen topic=work\na : Hi.\nb #Bad : Hi.\nb : Yo.\n", "marks are lower-case");
}

void playerAnswers()
{
    // Phase 5: the router's lines, as rules (Data/Talk/rules/player.talk): by how well they know the player and their
    // tone, always the best tier, the game's answer said in their own words.
    Library lib;
    std::string problem;
    expect(lib.load(Dir, problem), problem);
    const auto ask = [&](const std::string& intent, const Facts& facts, const std::string& tone, std::uint64_t seed) {
        auto q = moment("player." + intent);
        q.a.facts["tone"] = tone;
        for (const auto& [k, v] : facts)
            q.facts[k] = v;
        const auto said = lib.converse(q, seed, None, {});
        return said.lines.empty() ? std::string() : said.lines.front().second;
    };
    std::set<std::string> seen;
    for (std::uint64_t seed = 0; seed < 12; ++seed)
    {
        const auto line = ask("greet", {{"topic.meeting", "known"}, {"topic.player", "Ada"}}, "warm", seed);
        expect(line.find("Ada") != std::string::npos, "a warm greeting for Ada, who is known: " + line);
        seen.insert(line);
    }
    expect(seen.size() >= 3, "and more than one of them");
    bool own = false;
    for (std::uint64_t seed = 0; seed < 30; ++seed)
        own |= ask("greet", {{"topic.meeting", "stranger"}, {"topic.greeting", "Welcome to the Fair Scale."}}, "plain", seed) ==
               "Welcome to the Fair Scale.";
    expect(own, "a stranger may hear the resident's own greeting");
    for (std::uint64_t seed = 0; seed < 30; ++seed)
        expect(ask("greet", {{"topic.meeting", "stranger"}}, "gruff", seed).find("#") == std::string::npos &&
                   !ask("greet", {{"topic.meeting", "stranger"}}, "gruff", seed).empty(),
               "lines whose facts are missing are left out");
    const auto price = ask("trade", {{"topic.answer", "a meal is 6 pennies."}}, "formal", 1);
    expect(price == "Certainly. A meal is 6 pennies." || price == "Of course. A meal is 6 pennies." || price == "Allow me. A meal is 6 pennies.",
           "the game's answer, opened in their tone: " + price);
    expect(ask("trade", {}, "plain", 0).empty(), "no answer, no line: a model speaks instead");
    std::set<std::string> plain;
    for (std::uint64_t seed = 0; seed < 40; ++seed)
        plain.insert(ask("trade", {{"topic.answer", "a meal is 6 pennies."}}, "plain", seed));
    expect(plain == std::set<std::string>{"A meal is 6 pennies.", "Well, a meal is 6 pennies."},
           "no opener, or one that runs on in lower case (" + std::to_string(plain.size()) + ")");
    expect(ask("job", {{"topic.answer", "I'm Wren."}}, "sly", 2).find("I'm Wren.") != std::string::npos, "\"I\" stays itself");
    const auto bye = ask("bye", {{"topic.meeting", "known"}, {"topic.player", "Ada"}}, "formal", 3);
    expect(!bye.empty(), "a known wolf's farewell, from a formal one (who has no known lines of its own): " + bye);
    // In dialect: a Frosthollow miner, broad, says farewell their way some of the time.
    std::set<std::string> farewells;
    for (std::uint64_t seed = 0; seed < 60; ++seed)
    {
        auto q = moment("player.bye");
        q.a.facts["tone"] = "plain";
        q.a.facts["from"] = "frosthollow";
        q.a.facts["accent"] = "broad";
        q.a.grammar = {"frosthollow"};
        q.facts["topic.meeting"] = "stranger";
        const auto said = lib.converse(q, seed, None, {});
        if (!said.lines.empty())
            farewells.insert(said.lines.front().second);
    }
    expect(farewells.count("Keep your lamp lit.") || farewells.count("Lamp lit."), "and in Frosthollow's words");
    // The voice card a model is given.
    Speaker miner;
    miner.facts = {{"id", "tn_fh_miner"}, {"name", "Thistle"}, {"from", "frosthollow"}, {"accent", "broad"}};
    miner.grammar = {"frosthollow"};
    const auto card = lib.voiceCard(miner);
    expect(card.find("Grew up in Frosthollow and speaks broad Frosthollow") != std::string::npos && card.find("'Warm hearth'") != std::string::npos &&
               card.find("'deep'") != std::string::npos && card.find("Their own oath, always: '") != std::string::npos && card.size() <= 480,
           "a voice card: where they're from, their words, their oath: " + card);
    miner.facts["accent"] = "none";
    expect(lib.voiceCard(miner).find("lost the accent") != std::string::npos, "and one who lost the accent: " + lib.voiceCard(miner));
}

void jobs()
{
    // From the post's title and the work label only, read as words: the work first, then the place's name.
    const auto job = [](const std::string& label, const std::string& role = "civilian") { return jobCategory(label, label, role, 40); };
    expect(jobCategory("Master Smith of Oakhaven", "", "civilian", 40) == "smith", "smith");
    expect(job("fishes the grey coast") == "fisher", "fisher");
    expect(job("stands watch", "guard") == "guard", "guard");
    expect(jobCategory("", "plays about the town", "civilian", 9) == "child", "children are children");
    expect(job("keeps Oakhaven Mercantile", "merchant") == "merchant", "shopkeeper");
    expect(job("shopkeeper at Larkfield General", "merchant") == "merchant", "Larkfield is no field");
    expect(job("digs ore at Deepvein") == "miner" && job("logging at Oakfell") == "woodcutter", "the work, not the place");
    expect(job("works at The Wharf Tannery") == "crafter", "a works' name read from its last word: a tannery, not a wharf");
    expect(job("works at The Grayrock Ironworks") == "smith" && job("works at The Fell Sawmill") == "woodcutter", "works by name");
    expect(job("helping at The Wet Loaf", "merchant") == "baker" && job("helping at Commons Sundries") == "merchant",
           "a shop's helper: its trade if its name says it, else a merchant");
    expect(job("keeps the house") == "none" && job("sits by the well") == "none", "keeping house is no trade");
    expect(job("keeping the taproom", "merchant") == "innkeeper" && job("keeps The Acorn Inn", "merchant") == "innkeeper", "innkeepers");
    expect(job("keeping the chapel") == "priest" && job("keeps The Snow Chapel") == "priest", "priests");
    expect(job("keeps rabbits at Kitwarren") == "herder" && job("milks the herd at Whiteleas") == "herder", "herders");
    expect(job("rakes the salt at the Salt Pans") == "farmer", "salters");
}

// Doc 79, 10.3 (Phase 8): the salience rule. A failing shop's keeper never says it can't complain: the words are passed
// over for another of the line's alternatives, and a line with none left isn't said; the same keeper thriving may.
void salience()
{
    auto lib = foundation();
    std::string problem;
    expect(lib.settings().salience.size() >= 1, "settings.json has its salience rules");
    expect(lib.parse(R"(
rule sal_fine
when concept=talk.work
say Can't complain. | Slow, slow.
rule sal_only
when concept=talk.day
say Oh, can't complain, truly.
)", "sal.talk", problem), "salience test rules load: " + problem);
    std::set<std::string> failing, thriving;
    bool onlySaid = false;
    for (std::uint64_t seed = 0; seed < 40; ++seed)
    {
        auto q = moment("talk.work");
        q.a.facts["business"] = "failing";
        q.a.facts["wealth"] = "comfortable";
        if (const auto said = lib.pick(q, seed, None, {}); !said.lines.empty())
            failing.insert(said.lines.front().second);
        q.a.facts["business"] = "thriving";
        if (const auto said = lib.pick(q, seed, None, {}); !said.lines.empty())
            thriving.insert(said.lines.front().second);
        auto d = moment("talk.day");
        d.a.facts["business"] = "failing";
        onlySaid |= !lib.pick(d, seed, None, {}).lines.empty();
    }
    expect(failing == std::set<std::string>{"Slow, slow."}, "a failing keeper never says it can't complain (" + std::to_string(failing.size()) + ")");
    expect(thriving.count("Can't complain."), "a thriving one may");
    expect(!onlySaid, "and a line with nothing else to say isn't said");
}

// Doc 79, 11 (Phase 8): the staged economy's talk. Each new topic has something to say; a price is never named but the
// list's (no figure in any price line: the shown-price gate), and a line never blames a cause the move doesn't have.
void theStagedEconomysTalk()
{
    Library lib;
    std::string problem;
    expect(lib.load(Dir, problem), "the library loads whole:\n" + problem);
    const auto price = [](const std::string& cause, const std::string& words, const std::string& dir) {
        auto q = moment("talk.prices");
        q.facts["topic.item"] = "salt";
        q.facts["topic.what"] = "salt";
        q.facts["topic.is"] = "is";
        q.facts["topic.dir"] = dir;
        q.facts["topic.price"] = "fourteen pence the dozen";
        q.facts["topic.cause"] = cause;
        q.facts["topic.cause_words"] = words;
        return q;
    };
    // No figure in any line that may speak of a price (only #topic.price#: the list's price, in words).
    for (const auto& e : lib.entries())
    {
        bool money = false;
        for (const auto& c : e.concepts)
            money |= c == "talk.prices" || c == "talk.price_cause" || c.rfind("prices.", 0) == 0 || c == "talk.times" ||
                     c == "talk.event" || c == "talk.ships" || c == "talk.harvest";
        const bool ours = e.file.find("economy8") != std::string::npos;
        if (!money && !ours)
            continue;
        for (const auto& t : e.turns)
            for (const auto& v : t.variants)
                for (const auto& text : v.texts)
                {
                    const bool figure = std::any_of(text.begin(), text.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
                    expect(!figure, e.id + " names no figure (the shown-price gate): " + text);
                    if (money)
                        for (const char* unit : {"#topic.price# a bundle", "#topic.price# a loaf", "#topic.price# each", "#topic.price# the "})
                            expect(text.find(unit) == std::string::npos, e.id + ": nothing after the price, which may end \"the dozen\": " + text);
                }
    }
    // A move travellers made is never blamed on the roads, the sea, the harvest or the season; a late wagon's may be.
    const auto heard = [&](const Query& base, int seeds) {
        std::vector<std::string> out;
        for (const auto& wealth : {"destitute", "poor", "comfortable", "rich"})
            for (const auto& job : {"merchant", "laborer", "baker", "healer", "noble"})
                for (int seed = 0; seed < seeds; ++seed)
                {
                    auto q = base;
                    q.a.facts["wealth"] = wealth;
                    q.a.facts["job"] = job;
                    q.a.facts["role"] = std::string(job) == "merchant" ? "merchant" : "civilian";
                    q.a.facts["class"] = std::string(wealth) == "rich" ? "upper" : "lower";
                    q.b.facts["wealth"] = "poor";
                    for (const auto& [who, line] : lib.converse(q, std::uint64_t(seed), None, {}).lines)
                        out.push_back(line);
                }
        return out;
    };
    const auto lowered = [](std::string s) {
        for (auto& c : s)
            c = char(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    for (const auto& dir : {"up", "down"})
    {
        const auto lines = heard(price("players", dir == std::string("up") ? "travellers buying" : "travellers selling", dir), 6);
        expect(lines.size() >= 40, std::string("prices ") + dir + " are talked of");
        std::string blamed;
        for (const auto& line : lines)
        {
            const auto low = lowered(line);
            for (const char* cause : {"wagon", "caravan", "carter", "ship", "harvest", "winter price", "the frost", "the snow",
                                      "drover", "good catch", "blight", "wedding", "bandit", "the roads", "the pass"})
                if (low.find(cause) != std::string::npos)
                    blamed += "\n  " + std::string(cause) + ": " + line;
        }
        expect(blamed.empty(), std::string("a move travellers made (") + dir + ") isn't blamed on the roads, the sea or the season:" + blamed);
    }
    bool blamed = false;
    for (const auto& line : heard(price("late", "the wagon's late", "up"), 6))
        blamed |= lowered(line).find("wagon") != std::string::npos || lowered(line).find("carter") != std::string::npos;
    expect(blamed, "a late wagon's move is put down to it");
    for (const auto& line : heard(price("players", "travellers buying", "up"), 2))
        expect(line.find("fourteen pence the dozen") != std::string::npos || line.find("pence") == std::string::npos,
               "the only price said is the list's: " + line);
    // Every new topic speaks: wagons due and their news, ships, the harvest, a staged event, the Times; and a traveller's
    // buying up by name.
    const std::vector<std::pair<std::string, Facts>> topics{
        {"talk.caravan", {{"topic.kind", "due"}, {"topic.when", "soon"}, {"topic.where", "Coldwater"}, {"topic.what", "nails"}, {"topic.who", "Tam Reed"}}},
        {"talk.caravan", {{"topic.kind", "late"}, {"topic.where", "Coldwater"}, {"topic.what", "nails"}, {"topic.why", "a broken axle"}}},
        {"talk.caravan", {{"topic.kind", "news"}, {"topic.where", "Coldwater"}, {"topic.news", "iron ore is cheap in Coldwater"}}},
        {"talk.ships", {{"topic.kind", "in"}, {"topic.ship", "the Gull"}, {"topic.what", "butter"}, {"topic.is", "is"}}},
        {"talk.ships", {{"topic.kind", "lost"}, {"topic.ship", "the Gull"}}},
        {"talk.harvest", {{"world.harvest", "poor"}}},
        {"talk.event", {{"world.event", "house_wedding"}, {"world.event_words", "the House wedding"}, {"world.event_when", "now"}}},
        {"talk.event", {{"world.event", "seal_hunt"}, {"world.event_words", "the seal hunt"}, {"world.event_when", "soon"}}},
        {"talk.event", {{"world.event", "brewers_glut"}, {"world.event_words", "the brewers' glut"}, {"world.event_when", "past"}}},
        {"talk.event", {{"world.event", "seal_hunt"}, {"world.event_words", "the seal hunt"}, {"world.event_when", "past"}}},
        {"talk.times", {{"world.times", "hard"}, {"world.times_cause", "the Crown's war levy"}}},
        {"talk.times", {{"world.times", "boom"}}},
        {"talk.price_cause", {{"topic.dir", "up"}, {"topic.known", "yes"}, {"topic.who", "Ada"}, {"topic.what", "salt"}, {"topic.is", "is"},
                              {"topic.item", "salt"}, {"topic.price", "fourteen pence the dozen"}}},
    };
    for (const auto& [concept, facts] : topics)
    {
        int said = 0;
        for (const auto& wealth : {"destitute", "poor", "comfortable", "rich"})
            for (std::uint64_t seed = 0; seed < 4; ++seed)
            {
                auto q = moment(concept);
                q.a.facts["wealth"] = wealth;
                q.a.facts["class"] = std::string(wealth) == "rich" ? "upper" : "middle";
                for (const auto& [k, v] : facts)
                    q.facts[k] = v;
                const auto lines = lib.converse(q, seed, None, {}).lines;
                said += !lines.empty();
                for (const auto& [who, line] : lines)
                    expect(line.find('#') == std::string::npos, concept + ": every symbol filled: " + line);
            }
        expect(said >= 12, concept + " (" + facts.begin()->second + ") is talked of: " + std::to_string(said) + " of 16");
    }
    // A ship's move names the ship that made it (topic.ship), never the port's ship of the moment (Phase 8 repair).
    {
        auto q = price("ship_late", "the Heron stormbound", "up");
        q.facts["topic.ship"] = "the Heron";
        q.facts["world.ship_name"] = "the Long Tide";
        for (const auto& line : heard(q, 4))
            expect(line.find("Long Tide") == std::string::npos, "a stormbound ship's move names her: " + line);
    }
    // Nails are never eaten, and no keeper who doesn't sell them speaks of a shelf of them (topic.sort, deals).
    {
        auto q = price("players", "travellers selling", "down");
        q.facts["topic.item"] = q.facts["topic.what"] = "nails";
        q.facts["topic.is"] = "are";
        q.facts["topic.good"] = "nails";
        q.facts["topic.sort"] = "goods";
        q.a.facts["deals"] = q.b.facts["deals"] = "no";
        for (const auto& dir : {"up", "down"})
        {
            q.facts["topic.dir"] = dir;
            for (const auto& line : heard(q, 4))
            {
                const auto low = lowered(line);
                for (const char* food : {" eat", "supper", "hungry", "bowl", "belly", "the cook", "pups", "larder", "on the table"})
                    expect(low.find(food) == std::string::npos, std::string("nails aren't food (") + food + "): " + line);
                for (const char* shelf : {"my nails", "my shelf", "my shelves", "holding a shelf", "i only sell it", "my till"})
                    expect(low.find(shelf) == std::string::npos, std::string("nor a shelf of them (") + shelf + "): " + line);
            }
        }
    }
    // The slice 2 verify's leftovers. A keeper who doesn't sell the good is never glad of its rise as a seller, nor
    // undercut on it (deals): a butcher makes no living off dear planks.
    {
        auto q = price("players", "travellers buying", "up");
        q.facts["topic.item"] = q.facts["topic.what"] = "planks";
        q.facts["topic.is"] = "are";
        q.facts["topic.good"] = "planks";
        q.facts["topic.sort"] = "goods";
        bool glad = false;
        for (const auto& business : {"failing", "steady", "thriving"})
            for (const auto& deals : {"no", "yes"})
            {
                q.a.facts["business"] = q.b.facts["business"] = business;
                q.a.facts["deals"] = q.b.facts["deals"] = deals;
                q.a.facts["sold_out"] = q.b.facts["sold_out"] = "no";
                for (const auto& line : heard(q, 16))
                    for (const char* seller : {"better margin", "if you're selling", "i might make a living"})
                    {
                        const bool said = lowered(line).find(seller) != std::string::npos;
                        glad |= said;
                        expect(!said || std::string(deals) == "yes", std::string("only a keeper of the good is glad of its rise (") + seller + "): " + line);
                    }
            }
        expect(glad, "and one who sells it is");
        auto c = moment("talk.price_cause");
        for (const auto& [k, v] : Facts{{"topic.dir", "down"}, {"topic.known", "yes"}, {"topic.who", "Ada"}, {"topic.what", "planks"}, {"topic.is", "are"},
                                        {"topic.item", "planks"}, {"topic.good", "planks"}, {"topic.sort", "goods"},
                                        {"topic.price", "thirty-one pence the dozen"}})
            c.facts[k] = v;
        for (const auto& deals : {"no", "yes"})
        {
            c.a.facts["deals"] = c.b.facts["deals"] = deals;
            bool undercut = false;
            for (const auto& line : heard(c, 8))
                for (const char* mine : {"undercut the lot of us", "i'd stocked up", "at my price"})
                {
                    const bool said = lowered(line).find(mine) != std::string::npos;
                    undercut |= said;
                    expect(!said || std::string(deals) == "yes", std::string("only a keeper of the good is undercut on it (") + mine + "): " + line);
                }
            expect(std::string(deals) != "yes" || undercut, "and one who sells it is");
        }
        // A keeper says its shelf was cleared only where it was (sold_out): never after a dozen off a full shelf.
        c.facts["topic.dir"] = "up";
        c.a.facts["deals"] = c.b.facts["deals"] = "yes";
        for (const auto& bare : {"no", "yes"})
        {
            c.a.facts["sold_out"] = c.b.facts["sold_out"] = bare;
            bool cleared = false, custom = false;
            for (const auto& line : heard(c, 8))
            {
                const auto low = lowered(line);
                const bool said = low.find("to the last") != std::string::npos || low.find("cleared out") != std::string::npos;
                cleared |= said;
                custom |= low.find("good custom") != std::string::npos || low.find("that kind of morning") != std::string::npos;
                expect(!said || std::string(bare) == "yes", "a shelf is cleared only where none is left: " + line);
            }
            expect(custom && (std::string(bare) != "yes" || cleared), std::string("the keeper speaks of the custom; of a bare shelf where it is: ") + bare);
        }
        // And the townsfolk say a traveller "bought the shelf bare", or "all" of a good, only where that traveller's buying
        // took a shop's last of it (topic.bare; the merge of Phase 9 and the decisions): never after a small purchase.
        c.a.facts["deals"] = c.b.facts["deals"] = "no";
        c.a.facts["sold_out"] = c.b.facts["sold_out"] = "no";
        for (const auto& known : {"yes", "no"})
            for (const auto& bare : {"no", "yes"})
            {
                c.facts["topic.known"] = known;
                c.facts["topic.who"] = std::string(known) == "yes" ? "Ada" : "a grey wolf in a red cloak";
                c.facts["topic.bare"] = bare;
                bool emptied = false;
                int said = 0;
                for (const auto& line : heard(c, 12))
                {
                    ++said;
                    const auto low = lowered(line);
                    const bool claims = low.find("shelf bare") != std::string::npos || low.find("bought up all") != std::string::npos ||
                                        low.find("shop to shop") != std::string::npos || low.find("to the last") != std::string::npos;
                    emptied |= claims;
                    expect(!claims || std::string(bare) == "yes", std::string("a shelf bought bare only where it was (known ") + known + "): " + line);
                    expect(line.find('#') == std::string::npos, "every symbol filled: " + line);
                }
                expect(said >= 100 && (std::string(bare) == "yes") == emptied,
                       std::string("the traveller's buying is talked of, a bare shelf only where there is one (known ") + known + ", bare " + bare +
                           "): " + std::to_string(said) + " lines");
            }
    }
    // A complaint about a price is answered with a line about eating only where the good is food (topic.sort).
    for (const auto& humour : {"dry", "merry"})
        for (const auto& sort : {"goods", "drink", "food"})
        {
            auto q = moment("prices.answer_complaint");
            q.facts["topic.item"] = q.facts["topic.what"] = std::string(sort) == "food" ? "bread" : std::string(sort) == "drink" ? "ale" : "nails";
            q.facts["topic.sort"] = sort;
            q.a.facts["humour"] = humour;
            q.a.facts["wealth"] = "comfortable";
            bool eating = false;
            int said = 0;
            for (std::uint64_t seed = 0; seed < 80; ++seed)
                for (const auto& [who, line] : lib.converse(q, seed, None, {}).lines)
                    if (who == 0)
                    {
                        ++said;
                        const auto low = lowered(line);
                        const bool eats = low.find("eat") != std::string::npos || low.find("thin and") != std::string::npos;
                        eating |= eats;
                        expect(!eats || std::string(sort) == "food", std::string("only food is eaten (") + humour + ", " + sort + "): " + line);
                    }
            expect(said >= 40 && (std::string(sort) != "food" || eating), std::string("a ") + humour + " answer to dear " + sort + ": " + std::to_string(said));
        }
    // The glut over names no drink its market may not sell (its goods are those its shops deal in).
    {
        auto q = moment("talk.event");
        q.facts["world.event"] = "brewers_glut";
        q.facts["world.event_words"] = "the brewers' glut";
        q.facts["world.event_goods"] = "cider and mead";
        q.facts["world.event_when"] = "past";
        int said = 0;
        for (const auto& line : heard(q, 6))
        {
            ++said;
            const auto low = " " + lowered(line) + " ";
            for (const char* drink : {" ale ", " ale,", " ale.", " ale!", " ale?"})
                expect(low.find(drink) == std::string::npos, "the glut over names no ale where none was sold: " + line);
        }
        expect(said >= 20, "the glut over is talked of: " + std::to_string(said));
    }
    // An event's lines: a good dear or cheap only where its price fact says so, and an event over in the past.
    for (const char* kind : {"house_wedding", "works_fire", "good_catch", "orchard_blight", "festival_buying", "lame_ox", "rich_cargo",
                             "mill_stopped", "drovers_in", "salt_short", "brewers_glut"})
        for (const char* when : {"now", "soon", "past"})
        {
            auto q = moment("talk.event");
            q.facts["world.event"] = kind;
            q.facts["world.event_words"] = "the business";
            q.facts["world.event_goods"] = "brandy and wine";   // (Its goods, as World::eventFacts gives them.)
            q.facts["world.event_when"] = when;
            for (const auto& line : heard(q, 3))
            {
                const auto low = lowered(line);
                for (const char* claim : {"'s dear", "s cheap", "dear as", "cheap!", "cheap for", "cheap, too", "dearer", "going for nothing", "cheap mutton", "cheap ale"})
                    expect(low.find(claim) == std::string::npos, std::string(kind) + " " + when + ": no price the list doesn't show (" + claim + "): " + line);
                if (std::string(when) == "past")
                    for (const char* now : {"is on", "'s on", "this week", "till it turns", "today", "on the way", "is due", "coming"})
                        expect(low.find(now) == std::string::npos, std::string(kind) + " over: said in the past (" + now + "): " + line);
            }
        }
    {
        auto q = moment("talk.event");
        q.facts["world.event"] = "brewers_glut";
        q.facts["world.event_words"] = "the brewers' glut";
        q.facts["world.event_when"] = "now";
        q.facts["world.ale_price"] = "down";
        bool cheap = false;
        for (const auto& line : heard(q, 4))
            cheap |= lowered(line).find("cheap") != std::string::npos;
        expect(cheap, "and where the list shows ale cheap, it's said");
    }
    // A failing keeper, asked how it is, never says it can't complain.
    for (std::uint64_t seed = 0; seed < 60; ++seed)
    {
        auto q = moment("keeping.answer");
        q.a.facts["business"] = "failing";
        q.a.facts["wealth"] = "comfortable";
        for (const auto& [who, line] : lib.converse(q, seed, None, {}).lines)
            if (who == 0)
                expect(lowered(line).find("can't complain") == std::string::npos, "a failing keeper complains: " + line);
    }
}

// Never the word for the domestic animal anywhere a player reads talk (the user, 2026-10-14; doc 79's standing rules):
// every file of the library, the troubles' words and the market's lists (ship names, events, causes), by the same check
// the loaders use (market::forbiddenWord: whole words), and no hounds either.
void theForbiddenWord()
{
    namespace fs = std::filesystem;
    std::vector<fs::path> files;
    for (const auto& entry : fs::recursive_directory_iterator(Dir))
        if (entry.is_regular_file())
            files.push_back(entry.path());
    files.push_back(fs::path(RATW_SOURCE_DIR) / "Data/Town/troubles.json");
    files.push_back(fs::path(RATW_SOURCE_DIR) / "Data/Economy/market.json");
    int lines = 0;
    for (const auto& f : files)
    {
        std::ifstream in(f);
        expect(bool(in), "read " + f.string());
        int n = 0;
        for (std::string line; std::getline(in, line);)
        {
            ++n, ++lines;
            // (A rule's or scene's id is never read by a player, and players' heard lists keep them: three older ones
            // named for the animal before the user's rule stay as they are.)
            if (line.rfind("scene ", 0) == 0 || line.rfind("rule ", 0) == 0)
                continue;
            std::string low = line;
            for (auto& c : low)
                c = char(std::tolower(static_cast<unsigned char>(c)));
            // ("Hound" as a whole word: horehound is a herb.)
            bool hound = false;
            for (std::size_t at = low.find("hound"); at != std::string::npos && !hound; at = low.find("hound", at + 1))
                hound = (at == 0 || !std::isalpha(static_cast<unsigned char>(low[at - 1]))) &&
                        (at + 5 >= low.size() || !std::isalpha(static_cast<unsigned char>(low[at + 5])) || low[at + 5] == 's');
            expect(!ratw::market::forbiddenWord(line) && !hound,
                   "the forbidden word in " + f.filename().string() + ":" + std::to_string(n) + ": " + line);
        }
    }
    expect(lines > 20000, "every line read (" + std::to_string(lines) + ")");
}

void theRealLibrary()
{
    Library lib;
    std::string problem;
    expect(lib.load(Dir, problem), "the library loads whole:\n" + problem);
    expect(lib.size() >= 2300, "and is a library (" + std::to_string(lib.size()) + ")");
    for (const auto& c : lib.settings().concepts)
        expect(lib.has(c), "something written for " + c);
    expect(lib.find("lib_gossip_1") && lib.find("lib_gossip_1")->priority == -1, "the library's exchanges: filler");
    expect(lib.find("ev_muse_purpose") != nullptr, "the converted scenes keep their IDs (what players have heard)");
    // Phase 2's facts may be tested (and a misspelt one is refused).
    auto facts = foundation();
    expect(facts.parse(R"(
rule poor_and_hurt
when concept=bark wealth=poor|destitute injured=bad shift=after world.crime=recent here.crowd=busy wealth_days<7 tone=gruff
say Robbed, beaten, and not a penny.
scene kin_boss
when topic=family pair.kin=parent pair.boss=a pair.richer=a mourning=kin
a : Work comes first.
b[working=yes] : I know.
b : Yes.
)", "phase2.talk", problem), "phase 2's facts load: " + problem);
    expect(!facts.parse("rule x\nwhen concept=bark wealth=wealthy\nsay Hm.\n", "x.talk", problem) && problem.find("not a wealth") != std::string::npos,
           "and their values are checked: " + problem);
}

// Doc 83, Phase 4: a Country town may speak another's dialect (settings.json, dialect_of: Fenshore), or its own
// (Saltwell, the oasis's, since 2026-11-15), and every symbol a dialogue tree says is in every dialect.
void theCountrysVoices()
{
    Library lib;
    std::string problem;
    expect(lib.load(Dir, problem), "the library loads: " + problem);
    expect(lib.dialectOf("fenshore") == "mirebridge", "Fenshore speaks as Mirebridge");
    expect(lib.dialectOf("saltwell") == "saltwell", "Saltwell speaks its own, the oasis's (it borrowed Coldwater's before)");
    expect(lib.dialectOf("larkfield") == "larkfield" && lib.dialectOf("") == "", "a town with a dialect speaks its own");
    // A Fenshore wolf, broad (as Game::talkSpeaker makes them: their grammar layer is the dialect, `from` their town).
    Query q = moment("talk.smalltalk");
    q.alone = true;
    q.a.facts["from"] = "fenshore";
    q.a.facts["accent"] = "broad";
    q.a.grammar = {lib.dialectOf("fenshore")};
    std::set<std::string> said;
    for (std::uint64_t seed = 1; seed < 40; ++seed)
    {
        std::string out;
        expect(lib.expand("#greet#", q, 0, seed, out), "a greeting expands");
        said.insert(out);
    }
    expect(said.count("Dry feet") || said.count("Dry feet to you") || said.count("Dry feet, dry feet"),
           "a broad Fenshore wolf greets as the fens do");
    expect(lib.voiceCard(q.a).find("Grew up in Fenshore") != std::string::npos &&
               lib.voiceCard(q.a).find("Dry feet") != std::string::npos,
           "and the model is told where they grew up, with the fens' words: " + lib.voiceCard(q.a));
    // A broad Saltwell wolf greets as the oasis does, with peace, water and shade, and never with the lake's words;
    // the model is told so. With no accent left they speak plainly.
    q.a.facts["from"] = "saltwell";
    q.a.facts["accent"] = "broad";
    q.a.grammar = {lib.dialectOf("saltwell")};
    said.clear();
    for (std::uint64_t seed = 1; seed < 40; ++seed)
    {
        std::string out;
        expect(lib.expand("#greet#", q, 0, seed, out), "a Saltwell greeting expands");
        said.insert(out);
        expect(out.find("Still water") == std::string::npos && out.find("Calm water") == std::string::npos,
               "no Coldwater greeting in Saltwell: " + out);
    }
    expect(std::any_of(said.begin(), said.end(), [](const std::string& s) { return s.rfind("Peace on you", 0) == 0; }),
           "a broad Saltwell wolf greets with peace");
    expect(lib.voiceCard(q.a).find("Grew up in Saltwell") != std::string::npos &&
               lib.voiceCard(q.a).find("Still water") == std::string::npos,
           "and the model hears the oasis's words, not the lake's: " + lib.voiceCard(q.a));
    q.a.facts["accent"] = "none";
    for (std::uint64_t seed = 1; seed < 20; ++seed)
    {
        std::string out;
        lib.expand("#greet#", q, 0, seed, out);
        expect(out.find("Peace") == std::string::npos && out.find("shade") == std::string::npos,
               "no accent, no oasis words: " + out);
    }
    // The dialogue trees' symbols (doc 83's shop_*, work_*, inn_*, watch_*, the Phase 4 words) in every dialect.
    for (const char* symbol : {"shop_offer", "shop_asleep", "shop_bye", "talk_open", "work_offer", "work_declined", "inn_offer",
                               "inn_rest", "inn_helper", "watch_greet", "watch_noted", "watch_bye", "church_greet", "learn_offer",
                               "learn_taken", "news_none", "gossip_none", "trouble_none", "trouble_thanks", "way_ask", "job_busy",
                               "receiver_ask", "bless"})
        for (const auto& g : lib.grammars())
            if (g.rfind("person:", 0) != 0)
                expect(lib.symbols(g).count(symbol) > 0, g + " has #" + symbol + "#");
}

// Doc 84, Phase 1: a value that ends its own sentence before the line's comma or stop gives up its mark (defect 14);
// whether a line may be heard again is the concept asked's, not any of its entry's (defect 4).
void truthFixes()
{
    auto lib = foundation();
    std::string problem;
    expect(lib.parseGrammar(R"(
grammar plain
habit = fine
aside = well

grammar loaves
habit = the loaves are honest.
aside = mind;
)", "loaves.grammar", problem), problem);
    auto q = moment("bark");
    q.alone = true;
    q.b = {};
    q.a.grammar = {"loaves"};
    const auto say = [&](const std::string& text) {
        std::string out;
        lib.expand(text, q, 0, 1, out);
        return out;
    };
    expect(say("Morning to you; #habit#, neighbour.") == "Morning to you; the loaves are honest, neighbour.",
           "the punctuation join drops the value's stop before a comma: " + say("Morning to you; #habit#, neighbour."));
    expect(say("#habit#.") == "The loaves are honest.", "and one of two stops: " + say("#habit#."));
    expect(say("#aside#, now.") == "Mind, now.", "a semicolon too: " + say("#aside#, now."));
    expect(say("#habit# Yes.") == "The loaves are honest. Yes.", "and keeps it where the line goes on: " + say("#habit# Yes."));

    expect(lib.parse(R"(
rule r_also_weather
when concept=talk.smalltalk|talk.weather
say Grey again.

rule r_only_small
when concept=talk.smalltalk
say Quiet today.
)", "repeat.talk", problem), problem);
    const auto heard = [](const std::string& id) { return id == "r_also_weather"; };
    auto small = moment("talk.smalltalk");
    int again = 0;
    for (std::uint64_t seed = 0; seed < 40; ++seed)
        again += lib.pick(small, seed, heard, {}).id == "r_also_weather";
    expect(again == 0, "a line heard is not fresh for small talk because it is also a weather line (" + std::to_string(again) + " of 40)");
    auto weather = moment("talk.weather");
    bool repeats = false;
    for (std::uint64_t seed = 0; seed < 10; ++seed)
        repeats |= lib.pick(weather, seed, heard, {}).id == "r_also_weather";
    expect(repeats, "while asked as weather, it may be said again");
}


// Doc 84, Phase 2: moves and tags. moves.json and what a line says it does (does, owes, invites, tone, asserts, denies,
// gist, binds, {k=v}), refused when wrong; the answer concepts as aliases; entries by move; the dead-end proof; what a
// said line carries into the transcripts; the conversation's own facts (conv.*) and a bound third party (x.*).
void movesAndTags()
{
    // The book itself.
    {
        Moves m;
        std::string problem;
        const std::set<std::string> concepts{"jest.answer", "talk.smalltalk"};
        const auto refused = [&](const std::string& text, const std::string& says) {
            Moves x;
            std::string p;
            expect(!x.read(text, concepts, p) && p.find(says) != std::string::npos, "moves.json refused (" + says + "): " + p);
            expect(x.empty(), "and nothing of it kept");
        };
        const std::string ok = R"({"tones": ["light", "even"], "moves": {
            "greet": {"family": "opening", "first": true, "expects": ["greet.return"]},
            "greet.return": {"family": "opening", "answers": ["greet"], "fallback": ["ack"]},
            "react.news": {"family": "react", "answers": ["greet"], "variants": ["known"], "fallback": ["ack"]},
            "ack": {"family": "ground", "answers": ["*"]},
            "jest.answer": {"alias": "react.news.known"}}})";
        expect(m.read(ok, concepts, problem), "a small book reads: " + problem);
        expect(m.known("react.news.known") && !m.known("react.news.loud") && m.alias("jest.answer") == "react.news.known",
               "variants are moves of their own kind; an alias names a move");
        expect(m.answers("greet.return", "greet") && m.answers("ack", "anything") && !m.answers("greet", "greet.return"),
               "what answers what");
        expect(Moves::meets("react.news.known", "react.news") && !Moves::meets("react.newsy", "react.news"), "a kind meets its move");
        expect(m.chain("greet.return") == std::vector<std::string>({"greet.return", "ack"}), "fallbacks walk down to ack");
        expect(m.toneOf("greet") == "even", "a move with no tone is even");
        refused(R"({"tones": ["even"], "moves": {"greet": {"family": "chat", "first": true}, "ack": {"family": "ground", "answers": ["*"]}}})", "family 'chat'");
        refused(R"({"tones": ["even"], "moves": {"greet": {"family": "opening", "first": true, "expects": ["hello"]}, "ack": {"family": "ground", "answers": ["*"]}}})", "'hello' is not a move");
        refused(R"({"tones": ["even"], "moves": {"greet": {"family": "opening", "first": true, "expects": ["greet2"]}, "greet2": {"family": "opening", "first": true}, "ack": {"family": "ground", "answers": ["*"]}}})", "doesn't answer it");
        refused(R"({"tones": ["even"], "moves": {"greet": {"family": "opening", "first": true, "colour": "red"}, "ack": {"family": "ground", "answers": ["*"]}}})", "no field 'colour'");
        refused(R"({"tones": ["even"], "moves": {"greet": {"family": "opening", "first": true}, "ack": {"family": "ground", "answers": ["*"]}, "wave.answer": {"alias": "greet"}}})", "an alias names a concept");
        refused(R"({"tones": ["even"], "moves": {"greet": {"family": "opening", "first": true}, "ack": {"family": "ground", "answers": ["*"]}, "jest.answer": {"alias": "grin"}}})", "alias 'grin' is not a move");
        refused(R"({"tones": ["even"], "moves": {"greet": {"family": "opening", "first": true, "tone": "loud"}, "ack": {"family": "ground", "answers": ["*"]}}})", "tone 'loud'");
        refused(R"({"tones": ["even"], "moves": {"greet": {"family": "opening", "first": true}}})", "no ack");
        refused(R"({"tones": ["even"], "moves": {"nod": {"family": "ground"}, "ack": {"family": "ground", "answers": ["*"]}}})", "either opens");
    }
    // The real book: every answer concept of the library is a move's alias, so every `then` hands on a move.
    auto lib = foundation();
    expect(!lib.moves().empty() && lib.moves().all().size() >= 85, "the book: about 85 moves or more");
    for (const auto& c : lib.settings().concepts)
        if (c.rfind("talk.", 0) != 0 && c.rfind("player.", 0) != 0 && c.rfind("react.", 0) != 0 && c != "bark")
            expect(!lib.moves().alias(c).empty(), "the answer concept " + c + " is a move's alias");
    expect(lib.moves().alias("jest.answer") == "tease.return", "jest.answer is tease.return (§7.2)");

    // What a line says it does, refused when wrong.
    const auto bad = [&](const std::string& text, const std::string& says) {
        auto l = foundation();
        std::string p;
        expect(!l.parse(text, "bad.talk", p) && p.find(says) != std::string::npos, "refused (" + says + "): " + p);
        expect(l.size() == 0, "and nothing of it loaded");
    };
    bad("rule x\nwhen concept=talk.gossip\ndoes gossiping\nsay Hm.\n", "'gossiping' is not a move");
    bad("rule x\nwhen concept=talk.gossip\ndoes react.news.loud\nsay Hm.\n", "'react.news.loud' is not a move");
    bad("rule x\nwhen concept=talk.gossip\ndoes tell.gossip / react.news\nsay Hm.\n", "a rule is one turn");
    bad("rule x\nwhen concept=talk.gossip\ndoes tell.gossip\ndoes tell.news\nsay Hm.\n", "one `does` line");
    bad("scene x\nwhen topic=smalltalk\ndoes give.opinion / agree\na : Grey.\nb : It is.\na : Very.\n", "does names 2 turns' moves; it has 3");
    bad("rule x\nwhen concept=talk.gossip\ndoes tell.gossip\nowes other: grant\nsay Hm.\n", "owes grant, which doesn't answer tell.gossip");
    bad("rule x\nwhen concept=talk.gossip\ndoes tell.gossip\nowes friend: react.news\nsay Hm.\n", "owes is 'owes other|self|any");
    bad("rule x\nwhen concept=talk.gossip\nowes other: react.news\nsay Hm.\n", "needs the line's own move first");
    bad("rule x\nwhen concept=talk.gossip\ndoes tell.gossip\ninvites other: tease\nsay Hm.\n", "owes tease, which doesn't answer");
    bad("rule x\nwhen concept=talk.gossip\ndoes tell.gossip\ntone loud\nsay Hm.\n", "'loud' is not a tone");
    bad("rule x\nwhen concept=talk.gossip\ndoes tell.gossip\nasserts theft.maybe\nsay Hm.\n", "'theft.maybe' is not a claim kind");
    bad("rule x\nwhen concept=talk.gossip\ndoes tell.gossip\ngist 2\nsay Hm.\n", "a rule is one turn; only a scene's names one");
    bad("scene x\nwhen topic=smalltalk\ndoes give.opinion / agree\ndenies 3\na : Grey.\nb : It is.\n", "names turn 3 of 2");
    bad("rule x\nwhen concept=bark\nbinds y=self.spouse\nsay Hm.\n", "binds is 'binds x=<slot>'");
    bad("rule x\nwhen concept=bark\nbinds x=self.cousin\nsay Hm.\n", "'self.cousin' is not a slot binds may name");
    bad("rule x\nwhen concept=bark\nsay #x.name# snores.\n", "speaks of x with nobody bound");
    bad("rule x\nwhen concept=bark x.stage=adult\nsay Hm.\n", "speaks of x with nobody bound");
    bad("rule x\nwhen concept=bark x.shoe=red\nbinds x=self.spouse\nsay Hm.\n", "unknown fact 'x.shoe'");
    bad("rule x\nwhen concept=bark x.disposition=giddy\nbinds x=self.spouse\nsay Hm.\n", "'giddy' is not a x.disposition");
    bad("rule x\nwhen concept=bark\nsay {Kind=look} Hm.\n", "a tag is {key=value}");
    bad("rule x\nwhen concept=bark\nsay {kind=look Hm.\n", "a tag is {key=value} before the words");
    bad("rule x\nwhen concept=bark\nsay Hm {kind=look}.\n", "{ } hold a tag");
    bad("rule x\nwhen concept=bark\nsay {move=greet} Hm.\n", "conv.move is the engine's own");
    bad("rule x\nwhen concept=bark\nsay {kind=look keep soon} Hm.\n", "keep is a time");
    bad("rule x\nwhen concept=bark conv.move=tell.wibble\nsay Hm.\n", "'tell.wibble' is not a conv.move");
    bad("rule x\nwhen concept=bark conv.Kind=look\nsay Hm.\n", "unknown fact 'conv.Kind'");
    bad("rule x\nwhen concept=talk.gossip\nsay Hm.\nthen other: talk.weather\nrule y\nwhen concept=talk.gossip\nsay Hm.\nthen other: lost.answer\n",
        "is not a concept");

    std::string problem;
    expect(lib.parse(R"(
rule mv_tell
when concept=talk.gossip
does tell.gossip
say {kind=look keep 7d} #topic.subject# tried it.
then other: town.gossip

rule mv_react
when concept=town.gossip
say Never!

rule mv_react_look
when concept=town.gossip conv.kind=look conv.move=tell.gossip conv.beat=1
priority 5
say Look at what?

rule mv_tell_soft
when concept=talk.news
does tell.news
invites other: react.news
tone grave
asserts
say Sad news.

rule mv_x
when concept=bark x.stage=adult x.disposition=warm|fond
binds x=self.spouse
does tell.self
say #x.name# snores. #x.they.capitalize# always did.

scene mv_scene
when topic=smalltalk
does give.opinion / agree / tease
tone even / even / light
gist
a : Grey day.
b : It is.
a : Like your coat.
)", "moves.talk", problem), "lines that say what they do load: " + problem);
    const auto* tell = lib.find("mv_tell");
    const auto* react = lib.find("mv_react");
    expect(tell && tell->does.size() == 1 && tell->does[0] == std::vector<std::string>({"tell.gossip"}) && !tell->aliasMoves,
           "a rule's written move");
    expect(react && react->aliasMoves && react->does[0] == std::vector<std::string>({"react.news"}),
           "a rule with no `does` makes its answer concept's move (town.gossip: react.news)");
    expect(tell->owes.size() == 1 && tell->owes[0].derived && tell->owes[0].moves == std::vector<std::string>({"react.news"}),
           "a `then` hands on its concept's move as what is owed");
    const auto by = [&](const std::string& move, const std::string& id) {
        const auto* list = lib.byMove(move);
        return list && std::any_of(list->begin(), list->end(), [&](std::size_t i) { return lib.entries()[i].id == id; });
    };
    expect(by("tell.gossip", "mv_tell") && by("react.news", "mv_react") && by("give.opinion", "mv_scene") && !by("agree", "mv_scene"),
           "entries by move: a rule by its own, a scene by its first turn's");

    // What a said line carries.
    auto q = moment("talk.gossip");
    q.facts["topic.subject"] = "Wren";
    q.facts["topic.claim_kind"] = "theft";
    const auto said = lib.converse(q, 3, None, {});
    expect(said.chain.size() == 2 && said.chain[0] == "mv_tell" && said.info.size() == 2, "the two beats: " + std::to_string(said.chain.size()));
    const auto& first = said.info[0];
    expect(first.moves == std::vector<std::string>({"tell.gossip"}) && first.owes == std::vector<std::string>({"react.news"}) &&
               first.asserts == std::vector<std::string>({"theft"}) && first.tone == "even" && first.tags.at("kind") == "look" &&
               std::abs(first.keep - 7) < 1e-9 && first.who == 0 && first.to == 1 && first.text == "Wren tried it.",
           "the teller's line: its move, what it owes, the claim it passes on, its tone and its tag");
    expect(said.chain[1] == "mv_react_look" && said.info[1].moves == std::vector<std::string>({"react.news"}) &&
               said.info[1].answers == std::vector<std::string>({"tell.gossip"}),
           "the next beat sees the tag (conv.kind), the move just made (conv.move) and the beat (conv.beat), and answers it: " + said.chain[1]);
    auto news = moment("talk.news");
    const auto sad = lib.pick(news, 1, None, {});
    expect(sad.info.size() == 1 && sad.info[0].tone == "grave" && sad.info[0].invites == std::vector<std::string>({"react.news"}) &&
               sad.info[0].owes.empty() && sad.info[0].asserts == std::vector<std::string>({"topic"}),
           "a soft obligation (invites), a written tone, and asserting with no claim kind: the topic");
    auto small = moment("talk.smalltalk");
    const auto* scene = lib.find("mv_scene");
    const auto talk = lib.render(*scene, small, 1);
    expect(talk.info.size() == 3 && talk.info[1].answers == std::vector<std::string>({"give.opinion"}) && talk.info[2].tone == "light" &&
               talk.info[2].gist && !talk.info[0].gist && talk.info[0].owes.size() == 1 && talk.info[2].owes.empty(),
           "a scene's turns: what each answers, its tone, the gist in the last; its last line owes only what is written");
    // A third party: nobody to bind, the line doesn't fit; the game's binder fills x.*.
    auto bark = moment("bark");
    bark.alone = true;
    const auto* x = lib.find("mv_x");
    expect(x && !lib.fits(*x, bark), "with no binder, a line that binds never fits");
    bark.bind = [](const std::string& slot, int speaker, Facts& f) {
        if (slot != "self.spouse" || speaker != 0)
            return false;
        f = {{"name", "Bram"}, {"sex", "male"}, {"stage", "adult"}, {"disposition", "fond"}};
        return true;
    };
    const auto snores = lib.render(*x, bark, 1);
    expect(lib.fits(*x, bark) && !snores.lines.empty() && snores.lines[0].second == "Bram snores. He always did.",
           "bound, it speaks of them: " + (snores.lines.empty() ? std::string("(nothing)") : snores.lines[0].second));
    bark.bind = [](const std::string&, int, Facts& f) {
        f = {{"name", "Bram"}, {"stage", "adult"}, {"disposition", "cool"}};
        return true;
    };
    expect(!lib.fits(*x, bark), "and its criteria test them (x.disposition)");

    // The dead-end proof.
    auto proof = foundation();
    expect(proof.parse(R"(
rule dead_open
when concept=talk.crime
does tell.news
say A theft.
then other: crime.answer

rule dead_answer_rich
when concept=crime.answer self.wealth=rich
say Flog them.
)", "dead.talk", problem), problem);
    auto dead = proof.deadEnds();
    expect(dead.find("dead end: crime.answer") != std::string::npos && dead.find("react.news") != std::string::npos,
           "a concept handed on to with no plain rule, and a move owed with nothing plain doing it, are dead ends:\n" + dead);
    expect(proof.parse("rule dead_answer_any\nwhen concept=crime.answer\npriority -1\nsay That's a bad business.\n", "fill.talk", problem), problem);
    dead = proof.deadEnds();
    expect(dead.find("crime.answer") == std::string::npos && dead.find("react.news") == std::string::npos,
           "filled, neither is (crime.answer's move, react.news, now has a plain line):\n" + dead);
    expect(proof.parse("rule dead_wave\nwhen concept=talk.smalltalk\ndoes request\nsay Lend me a hand?\n", "wave.talk", problem), problem);
    dead = proof.deadEnds();
    expect(dead.find("grant|decline") != std::string::npos, "what a move expects is owed too: " + dead);
    expect(proof.parse("rule dead_nod\nwhen concept=keeping.nod\nsay Mm.\n", "nod.talk", problem), problem);
    expect(proof.deadEnds().empty(), "and a plain ack answers it, by the fallbacks: " + proof.deadEnds());
}

void turnEngine()
{
    // Doc 84 §8.1-8.4 (Phase 3): the turn engine. A conversation is begun and goes on a line at a time; what a line owes
    // is answered, best fit first and tiers within it; nothing is said twice; a lull closes it with the farewell pair.
    std::string problem;
    auto lib = foundation();
    expect(lib.parse(R"(
rule tw_open
when concept=talk.crime
does tell.news
say Someone broke the bakery window.
then other: crime.answer
rule tw_answer_guard
when concept=crime.answer self.role=guard
say I'll look into it.
rule tw_answer
when concept=crime.answer
say What's the town coming to?
rule mv_news
does react.news
say Is that so?
rule mv_ack
does ack
say I see.
rule mv_pre
does preclose
say Well. Back to it.
rule mv_pre_ok
does accept_preclose
say Go on, then.
rule mv_bye
does farewell
say Farewell.
rule mv_bye_back
does farewell.return
say And you.
)", "turns.talk", problem), problem);
    auto q = moment("talk.crime");
    q.b.facts["role"] = "guard";
    // begin and next, a line at a time; converse is the same, whole.
    Conversation c;
    c.seed = 3;
    const Freshness fresh{None, {}};
    const auto first = lib.begin(c, q, fresh);
    expect(first && first->entry == "tw_open" && first->who == 0 && first->owes == std::vector<std::string>{"react.news"},
           "begin: the opener for the concept, owing its hand-on's move");
    expect(c.open.size() == 1 && c.open[0].to == 1 && c.open[0].concept == "crime.answer", "what it owes is open, to b, by its concept");
    const auto facts = [&](int, int) { return q; };
    const auto second = lib.next(c, facts, fresh);
    expect(second && second->who == 1 && (second->entry == "tw_answer_guard" || second->entry == "tw_answer") &&
               second->fit == 3 && second->best == 3,
           "next: the handed-on concept's own rule answers (fit 3), never a move line that only does the move (fit 1)");
    std::vector<SaidLine> rest;
    while (auto l = lib.next(c, facts, fresh))
        rest.push_back(*l);
    expect(c.phase == Conversation::Phase::Ended && c.ended == "farewell", "a lull closes it with a farewell: " + c.ended);
    expect(!rest.empty() && rest.back().moves == std::vector<std::string>{"farewell.return"} && rest.back().text == "And you.",
           "the farewell pair ends it");
    expect(rest.size() >= 2 && rest[rest.size() - 2].text == "Farewell." && rest[rest.size() - 2].who != rest.back().who,
           "the farewell, answered by the other");
    const auto whole = lib.converse(q, 3, None, {});
    expect(whole.lines.size() == c.lines.size() && whole.ended == "farewell" && whole.id == "tw_open" &&
               whole.chain.front() == "tw_open",
           "converse is begin and next, whole");
    // Fit, then tiers within it: the guard's own answer mostly, anyone's sometimes (no longer only the best tier).
    std::map<std::string, int> by;
    for (std::uint64_t seed = 1; seed <= 300; ++seed)
        ++by[lib.converse(q, seed, None, {}).info.at(1).entry];
    expect(by["tw_answer_guard"] > 180 && by["tw_answer"] > 20 && by.size() == 2,
           "tiers within the best fit: " + std::to_string(by["tw_answer_guard"]) + " and " + std::to_string(by["tw_answer"]));
    // Nothing in the concept holds: a move line doing the owed move answers (fit 1); none of those: its fallback, ack.
    auto none = foundation();
    expect(none.parse("rule o\nwhen concept=talk.crime\ndoes tell.news\nsay A fire at the mill.\nthen other: crime.answer\n"
                      "rule a\nwhen concept=crime.answer self.job=noble\nsay Dreadful.\n"
                      "rule mv_ack\ndoes ack\nsay I see.\n", "fall.talk", problem), problem);
    const auto fell = none.converse(moment("talk.crime"), 1, None, {});
    expect(fell.info.size() >= 2 && fell.info[1].entry == "mv_ack" && fell.info[1].fit == 0,
           "nothing answering the owed move: its fallback, walked down to ack");
    auto noble = moment("talk.crime");
    noble.b.facts["job"] = "noble";
    expect(none.converse(noble, 1, None, {}).info.at(1).entry == "a", "a noble answers as one");
    // Only a talk between residents on a topic closes: a reaction (perhaps to a player) is said, and that is all.
    expect(lib.parse("rule tw_react\nwhen concept=react.customer\nsay Come in, come in.\n", "react.talk", problem), problem);
    auto customer = moment("react.customer");
    const auto reacted = lib.converse(customer, 1, None, {});
    expect(reacted.lines.size() == 1 && reacted.ended == "done", "a reaction gets no farewell pair: " + std::to_string(reacted.lines.size()));
    // A shrug is an ack (§8.4): it owes nothing, whatever its rule hands on, and the talk closes.
    auto shrug = foundation();
    expect(shrug.parse("rule ask\nwhen concept=talk.smalltalk\nsay How's the leg?\nthen other: chat.answer\n"
                       "rule mm\nwhen concept=chat.answer\nsay Mm.\nthen other: health.answer\n"
                       "rule advice\nwhen concept=health.answer\nsay Then see the healer.\n"
                       "rule mv_bye\ndoes farewell\nsay Farewell.\nrule mv_bye_back\ndoes farewell.return\nsay And you.\n",
                       "shrug.talk", problem), problem);
    const auto shrugged = shrug.converse(moment("talk.smalltalk"), 1, None, {});
    expect(shrugged.info.size() == 4 && shrugged.info[1].moves == std::vector<std::string>{"ack"} && shrugged.info[1].owes.empty() &&
               shrugged.info[2].text == "Farewell.",
           "a shrug is an ack: no hand-on, and the talk closes");
    // A preclose when the talk has run long enough and its budget has room; the one at work closes first.
    auto longer = foundation();
    expect(longer.parse("scene s\nwhen topic=work\na : One.\nb : Two.\na : Three.\nb : Four.\n"
                        "rule mv_pre\ndoes preclose\nsay Well. Back to it.\nrule mv_pre_ok\ndoes accept_preclose\nsay Go on, then.\n"
                        "rule mv_bye\ndoes farewell\nsay Farewell.\nrule mv_bye_back\ndoes farewell.return\nsay And you.\n",
                        "long.talk", problem), problem);
    auto work = moment("talk.work");
    work.a.facts["talk"] = "chatty";
    work.b.facts["talk"] = "chatty";
    work.a.facts["working"] = "yes";
    const auto closed = longer.converse(work, 1, None, {});
    expect(closed.info.size() == 8 && closed.info[4].text == "Well. Back to it." && closed.info[4].who == 0 &&
               closed.info[5].text == "Go on, then." && closed.info[6].who == 0 && closed.ended == "farewell",
           "preclose (by the one at work), its answer, the farewell pair");
    work.a.facts["talk"] = "quiet";
    expect(longer.converse(work, 1, None, {}).info.size() == 6, "a quiet one's budget has no room for a preclose: the farewell pair alone");
    // The budget (§8.4): the smaller of the two's range by their talk, +2 for friends and kin, scaled by the frames.
    auto b = moment("talk.work");
    b.facts["pair.band"] = "acquaintances";
    for (std::uint64_t seed = 1; seed < 40; ++seed)
    {
        b.a.facts["talk"] = "chatty";
        b.b.facts["talk"] = "quiet";
        const int n = lib.budgetFor(b, {}, seed);
        expect(n >= 3 && n <= 5, "chatty and quiet: the quiet one's 3-5");
        b.facts["pair.band"] = "friends";
        expect(lib.budgetFor(b, {}, seed) == n + 2, "friends: two more");
        b.facts["pair.band"] = "strangers";
        expect(lib.budgetFor(b, lib.framesFor(b), seed) == std::max(2, int(std::lround(n * .6))), "strangers: 0.6 of it");
        b.facts["pair.band"] = "acquaintances";
    }
    // Frames (§7.4): worked out from the facts, stacked; strangers greet in the opener's breath and never tease.
    auto f = moment("talk.work");
    f.facts["pair.band"] = "strangers";
    f.a.facts["working"] = "yes";
    f.a.facts["shift"] = "during";
    expect(lib.framesFor(f) == std::vector<std::string>{"strangers", "at_work"}, "frames stack: strangers, at work");
    auto greet = foundation();
    expect(greet.parse("rule o\nwhen concept=talk.gossip\ndoes tell.gossip\nsay Word is the miller's wed.\n"
                       "rule mv_greet\ndoes greet\nwhen pair.band=strangers\nsay Good day.\n"
                       "rule mv_tease\ndoes tease\nsay Nice hat.\n", "greet.talk", problem), problem);
    auto g = moment("talk.gossip");
    g.facts["pair.band"] = "strangers";
    const auto greeted = greet.converse(g, 1, None, {});
    expect(greeted.info.at(0).text == "Good day. Word is the miller's wed." &&
               greeted.info[0].moves == std::vector<std::string>{"greet", "tell.gossip"} && greeted.id == "o",
           "a stranger greets in the same breath: " + greeted.info.at(0).text);
    g.facts["pair.band"] = "friends";
    expect(greet.converse(g, 1, None, {}).info.at(0).text == "Word is the miller's wed.", "friends don't");
    Conversation strangers;
    strangers.frames = {"strangers"};
    strangers.people = {"maren", "ivo"};
    Conversation::Open teased;
    teased.moves = {"tease"};
    expect(greet.answer(strangers, teased, g, 1, fresh).id.empty(), "strangers never tease");
    strangers.frames.clear();
    expect(greet.answer(strangers, teased, g, 1, fresh).id == "mv_tease", "acquaintances may");
    auto mourning = g;
    mourning.a.facts["mourning"] = "kin";
    expect(greet.answer(strangers, teased, mourning, 1, fresh).id.empty(), "nobody teases one who mourns");
    // Tone rules (§8.11): light is blocked for two lines after a grave one, unless a bridge came between.
    auto tones = foundation();
    expect(tones.parse("rule mv_laugh\ndoes laugh\nsay Ha! Good one.\n", "tone.talk", problem), problem);
    Conversation grave;
    grave.people = {"maren", "ivo"};
    SaidLine sad;
    sad.who = 0;
    sad.tone = "grave";
    sad.moves = {"tell.news"};
    grave.lines.push_back(sad);
    Conversation::Open laugh;
    laugh.moves = {"laugh"};
    expect(tones.answer(grave, laugh, g, 1, fresh).id.empty(), "no laughing straight after grave news");
    SaidLine between;
    between.who = 1;
    between.moves = {"assess"};
    grave.lines.push_back(between);
    expect(tones.answer(grave, laugh, g, 1, fresh).id == "mv_laugh", "but after a bridge (assess), it may");
    grave.lines.pop_back();
    SaidLine plain;
    plain.who = 1;
    plain.moves = {"ack"};
    grave.lines.push_back(plain);
    grave.lines.push_back(plain);
    expect(tones.answer(grave, laugh, g, 1, fresh).id == "mv_laugh", "and two lines on, it may");
    // Never the same entry, nor the same words, twice in one talk (C6); at most fourteen lines, a farewell included.
    std::string chain = "rule c0\nwhen concept=talk.day\ndoes tell.news\nsay Line 0.\nthen other: event.answer\n";
    for (int i = 1; i < 20; ++i)
        chain += "rule c" + std::to_string(i) + "\nwhen concept=event.answer\npriority " + std::to_string(40 - i) + "\nsay Line " +
                 std::to_string(i) + ".\nthen other: event.answer\n";
    chain += "rule mv_bye\ndoes farewell\nsay Farewell.\nrule mv_bye_back\ndoes farewell.return\nsay And you.\n";
    auto capped = foundation();
    expect(capped.parse(chain, "cap.talk", problem), problem);
    const auto lines = capped.converse(moment("talk.day"), 1, None, {});
    std::set<std::string> entries;
    for (const auto& l : lines.info)
        entries.insert(l.entry);
    expect(lines.info.size() == 14 && entries.size() == 14 && lines.info.back().text == "And you.",
           "fourteen lines at most, each entry once, the last two a farewell: " + std::to_string(lines.info.size()));
    // An invitation (`invites`) is taken up only sometimes; what is owed, always.
    auto invite = foundation();
    expect(invite.parse("rule o\nwhen concept=talk.smalltalk\ndoes tell.self\nsay I've taken up the fiddle.\n"
                        "rule mv_self\ndoes react.self\nsay Have you now.\n", "invite.talk", problem), problem);
    int taken = 0;
    for (std::uint64_t seed = 1; seed <= 200; ++seed)
        taken += invite.converse(moment("talk.smalltalk"), seed, None, {}).lines.size() > 1;
    expect(taken > 60 && taken < 140, "an invitation is taken about half the time: " + std::to_string(taken));
    // frames.json is checked when it loads.
    for (const auto& [text, why] : std::vector<std::pair<std::string, std::string>>{
             {R"({"frames": {"x": {"when": "pair.band=strangers", "colour": "red"}}})", "no field 'colour'"},
             {R"({"frames": {"x": {"when": "pair.band=strangers", "open": ["wave"]}}})", "not a move"},
             {R"({"frames": {"x": {"when": "pair.nonsense=1"}}})", "nonsense"},
             {R"({"frames": {"x": {"open": ["greet"]}}})", "holds when something does"},
             {R"({"frames": {"x": {"when": "pair.band=strangers", "budget": 9}}})", "0.2 to 3"},
             {R"({"frames": {"x": {"when": "pair.band=strangers", "tones": {"jolly": 1}}}})", "tones are moves.json's"},
             {R"({"budget": {"loud": [1, 2]}})", "not a talk"},
             {R"({"budget": {"plain": [5]}})", "[fewest, most]"},
             {R"({"budget": {"least": 1}})", "least is 2"},
             {R"({"invite_taken": 2})", "a chance"},
             {R"({"mood": {}})", "no field 'mood'"},
             // Phase 5's goals (§7.4): a plan of moves that may open, when something holds.
             {R"({"goals": {"x": {"when": "pair.band=strangers", "plan": ["wave"]}}})", "not a move that may open"},
             {R"({"goals": {"x": {"plan": ["greet"]}}})", "holds when something does"},
             {R"({"goals": {"x": {"when": "pair.band=strangers", "plan": ["greet"], "colour": 1}}})", "no field 'colour'"},
             {R"({"goals": {"x": {"when": "pair.band=strangers", "plan": []}}})", "a plan of one move or more"}})
    {
        namespace fs = std::filesystem;
        const auto dir = fs::temp_directory_path() / ("ratw-frames-" + std::to_string(checks));
        fs::remove_all(dir);
        fs::create_directories(dir / "grammar");
        for (const char* f : {"facts.json", "settings.json", "moves.json", "claims.json", "groups.talk"})
            fs::copy_file(fs::path(Dir) / f, dir / f);
        fs::copy_file(fs::path(Dir) / "grammar" / "plain.grammar", dir / "grammar" / "plain.grammar");
        std::ofstream(dir / "frames.json") << text;
        Library bad;
        std::string p;
        expect(!bad.loadFoundation(dir.string(), p) && p.find(why) != std::string::npos, "frames.json refused (" + why + "): " + p);
        fs::remove_all(dir);
    }
    // A library from before moves.json talks as it did (the golden transcripts): each `then` handed on, the best tier,
    // up to eight beats, the same rule again if it comes round.
    {
        namespace fs = std::filesystem;
        const auto dir = fs::temp_directory_path() / "ratw-legacy-talk";
        fs::remove_all(dir);
        fs::create_directories(dir / "grammar");
        for (const char* f : {"facts.json", "settings.json", "groups.talk"})
            fs::copy_file(fs::path(Dir) / f, dir / f);
        fs::copy_file(fs::path(Dir) / "grammar" / "plain.grammar", dir / "grammar" / "plain.grammar");
        Library old;
        std::string p;
        expect(old.loadFoundation(dir.string(), p), "a library without moves.json loads: " + p);
        fs::remove_all(dir);
        expect(old.parse("rule ping\nwhen concept=talk.smalltalk\nsay Ping.\nthen other: talk.day\nrule pong\nwhen concept=talk.day\nsay Pong.\nthen other: talk.smalltalk\n",
                         "loop.talk", p), p);
        const auto pp = old.converse(moment("talk.smalltalk"), 1, None, {});
        expect(pp.lines.size() == 8 && pp.lines[7] == std::pair<int, std::string>{1, "Pong."} && pp.ended == "cap",
               "eight beats at most, turn and turn about, as before");
    }
}

void beatsWhenSpoken()
{
    // Doc 84 §8.2, §8.6, §8.9, §8.10 (Phase 4): each line chosen from the facts as they are when it comes due; a scene's
    // later turn checked again (Valve's rule); breaks; a third voice; what one already knew; a thread returned to; lines
    // written outside the library carried on by the engine.
    std::string problem;
    auto lib = foundation();
    expect(lib.parse(R"(
rule bw_open
when concept=talk.crime
does tell.news
say Someone broke the bakery window.
rule bw_wet
does react.news
when here.weather=rain
say And in this rain, too.
rule bw_dry
does react.news
when here.weather=clear
say On a fine day like this.
rule bw_ack
does ack
say I see.
rule bw_bye
does farewell
say Farewell.
rule bw_bye_back
does farewell.return
say And you.
scene bw_fog
when topic=smalltalk here.weather=fog pair.mem.fogged!=yes
remember pair: fogged=yes 1h
a : Look at this fog.
b : Thick as wool.
rule bw_break_rain
does break
when conv.interrupt=rain
say Rain. Inside, quick.
rule bw_known
does react.news.known
say I'd heard.
rule bw_new
does react.news.new
say Never!
rule bw_defend
does join.defend
when conv.stake=kin
denies
say That's my brother you mean.
rule bw_sorry
does apologise
say I meant nothing by it.
rule bw_forgive
does accept_apology
say No harm done.
rule bw_return
does return
say As I was saying.
)", "beats.talk", problem), problem);
    const Freshness fresh{None, {}};
    // 1. Each beat asks the facts again: the answer is chosen from the moment as it is when it comes due.
    {
        auto q = moment("talk.crime");
        q.facts["here.weather"] = "rain";
        Conversation c;
        c.seed = 5;
        expect(bool(lib.begin(c, q, fresh)), "begun");
        auto now = q;
        now.facts["here.weather"] = "clear";
        int asked = 0;
        const auto facts = [&](int, int) {
            ++asked;
            return now;
        };
        const auto second = lib.next(c, facts, fresh);
        expect(second && second->entry == "bw_dry" && asked > 0, "the reply is chosen from the facts as they are now (dry): " +
                                                                     (second ? second->entry : std::string("none")));
    }
    // 2. Valve's rule: a scene's later turn is checked again when it comes due; the fog lifted, the rest is dropped. What
    // the scene itself remembered (pair.mem) is never checked again.
    {
        auto q = moment("talk.smalltalk");
        q.facts["here.weather"] = "fog";
        q.facts["topic.kind"] = "smalltalk";
        Conversation c;
        c.seed = 1;
        const auto first = lib.begin(c, q, fresh);
        expect(first && first->entry == "bw_fog" && first->turn == 0 && first->variant == 0, "the fog scene opens");
        auto still = q;
        still.facts["pair.mem.fogged"] = "yes";     // (What its own first line remembered.)
        Conversation kept = c;
        const auto stays = lib.next(kept, [&](int, int) { return still; }, fresh);
        expect(stays && stays->text == "Thick as wool." && kept.dropped == 0, "still foggy: its turn is said, its own memory never checked again");
        auto lifted = q;
        lifted.facts["here.weather"] = "clear";
        const auto next = lib.next(c, [&](int, int) { return lifted; }, fresh);
        expect(c.dropped == 1 && (!next || next->text != "Thick as wool."), "the fog lifted: the scene's later turn is dropped");
    }
    // 3. Broken off (§8.10): conv.interrupt, a break line by the one named, and the talk ends; with none fitting, it ends.
    {
        auto q = moment("talk.crime");
        q.facts["here.weather"] = "rain";
        Conversation c;
        c.seed = 2;
        lib.begin(c, q, fresh);
        const auto broke = lib.interrupt(c, "rain", 1, [&](int, int) { return q; }, fresh);
        expect(broke && broke->entry == "bw_break_rain" && broke->who == 1 && broke->to == 0 && c.ended == "break" &&
                   c.phase == Conversation::Phase::Ended && c.conv["interrupt"] == "rain",
               "rain: the one whose turn it was breaks off, and it ends");
        Conversation d;
        d.seed = 2;
        lib.begin(d, q, fresh);
        expect(!lib.interrupt(d, "fight", 1, [&](int, int) { return q; }, fresh) && d.ended == "break" && !lib.next(d, [&](int, int) { return q; }, fresh),
               "no break line fits: it ends all the same");
    }
    // 4. A third voice (§8.9): joins with a line doing a join move, said to the one who spoke last, who owes the answer;
    // the joiner's queries have the speaker as a.
    {
        auto q = moment("talk.crime");
        q.facts["here.weather"] = "rain";
        q.facts["topic.subject"] = "Bram";
        Conversation c;
        c.seed = 3;
        lib.begin(c, q, fresh);
        const auto wren = person("wren", "Wren", "female", "adult", "baker", "civilian");
        const auto query = [&](int speaker, int to) {
            if (speaker < 2 && to < 2)
                return q;
            auto x = q;
            const auto who = [&](int i) { return i == 0 ? q.a : i == 1 ? q.b : wren; };
            x.a = who(speaker);
            x.b = who(to);
            return x;
        };
        c.phase = Conversation::Phase::Closing;
        expect(!lib.join(c, "wren", "kin", {"join.defend"}, query(2, 0), fresh), "nobody joins once the closing has begun");
        c.phase = Conversation::Phase::Body;
        expect(!lib.join(c, "wren", "friend", {"join.defend"}, query(2, 0), fresh) && c.people.size() == 2,
               "nobody joins without a line that fits their stake");
        const auto joined = lib.join(c, "wren", "kin", {"join.defend"}, query(2, 0), fresh);
        expect(joined && joined->entry == "bw_defend" && joined->who == 2 && joined->to == 0 && c.people.size() == 3 &&
                   c.people[2] == "wren" && c.stake == "kin" && joined->denies.size() == 1,
               "a kinswoman cuts in to the one who spoke, denying it");
        expect(c.open.size() == 1 && c.open[0].to == 0 && c.open[0].by == 2, "the one she spoke to owes the answer");
        expect(Library::partner(c, 2) == 0 && Library::partner(c, 0) == 1, "who each is talking with");
        const auto sorry = lib.next(c, query, fresh);
        expect(sorry && sorry->entry == "bw_sorry" && sorry->who == 0 && sorry->to == 2, "the teller apologises to her");
        const auto forgive = lib.next(c, query, fresh);
        expect(forgive && forgive->entry == "bw_forgive" && forgive->who == 2 && forgive->to == 0, "and she accepts it");
        expect(!lib.join(c, "ivo2", "kin", {"join.defend"}, query(2, 0), fresh), "three at most");
        std::vector<SaidLine> rest;
        while (auto l = lib.next(c, query, fresh))
            rest.push_back(*l);
        expect(c.ended == "farewell" && !rest.empty(), "and the talk closes: " + c.ended);
    }
    // 5. What one already knew (§8.6): "I heard." from one who knew, never "Never!"; never "I heard." from one who didn't.
    {
        std::map<std::string, int> knew, fresh2;
        for (std::uint64_t seed = 1; seed <= 60; ++seed)
        {
            auto q = moment("talk.crime");
            q.facts["here.weather"] = "fog";        // (Neither weather answer holds.)
            q.b.facts["knows"] = "yes";
            ++knew[lib.converse(q, seed, None, {}).info.at(1).entry];
            q.b.facts["knows"] = "no";
            ++fresh2[lib.converse(q, seed, None, {}).info.at(1).entry];
        }
        expect(knew["bw_known"] == 60, "one who knew answers that they'd heard, every time");
        expect(fresh2["bw_known"] == 0 && fresh2["bw_new"] > 0, "one who didn't never says they'd heard");
        auto scenes = foundation();
        expect(scenes.parse("scene sc_news\nwhen topic=smalltalk\ndoes tell.gossip / react.news.new\na : Did you hear?\nb : Never!\n"
                            "rule mv_ack\ndoes ack\nsay I see.\n", "news.talk", problem), problem);
        auto q = moment("talk.smalltalk");
        q.facts["topic.kind"] = "smalltalk";
        q.b.facts["knows"] = "yes";
        expect(scenes.pick(q, 1, None, {}).id.empty(), "a scene whose answer says \"Never!\" isn't chosen for one who knew");
        q.b.facts["knows"] = "no";
        expect(scenes.pick(q, 1, None, {}).id == "sc_news", "and is for one who didn't");
    }
    // 6. A thread left unfinished on this topic is returned to in the opener's breath (§8.10).
    {
        auto q = moment("talk.crime");
        q.facts["here.weather"] = "rain";
        q.facts["pair.thread"] = "crime";
        Conversation c;
        c.seed = 4;
        const auto first = lib.begin(c, q, fresh);
        expect(first && first->text.rfind("As I was saying. ", 0) == 0 && first->moves.front() == "return",
               "as I was saying: " + (first ? first->text : std::string()));
        q.facts["pair.thread"] = "prices";
        Conversation d;
        d.seed = 4;
        const auto other = lib.begin(d, q, fresh);
        expect(other && other->text == "Someone broke the bakery window.", "not on another topic");
    }
    // 7. Lines written outside the library (the authored exchange) are said as they are, then the engine closes.
    {
        auto q = moment("talk.crime");
        Said written;
        written.lines = {{0, "A wagon came in."}, {1, "So I saw."}};
        Conversation c;
        c.seed = 1;
        const auto first = lib.beginWith(c, q, written);
        expect(first && first->text == "A wagon came in." && first->who == 0 && first->to == 1, "the first written line");
        std::vector<SaidLine> rest;
        while (auto l = lib.next(c, [&](int, int) { return q; }, fresh))
            rest.push_back(*l);
        expect(rest.size() >= 3 && rest.front().text == "So I saw." && c.ended == "farewell",
               "then the second, and the farewell pair: " + c.ended);
    }
    // 8. frames.json's curt, join and thread are checked when it loads.
    for (const auto& [text, why] : std::vector<std::pair<std::string, std::string>>{
             {R"({"curt": {"chance": 2}})", "curt: chance"},
             {R"({"curt": {"effects": {"them": {"affinity": -1}}}})", "\"self\" or \"other\""},
             {R"({"curt": {"effects": {"other": {"warmth": -1}}}})", "an effect is an axis"},
             {R"({"join": {"people": 4}})", "this build's most"},
             {R"({"join": {"colour": 1}})", "join: no field 'colour'"},
             {R"({"thread": {"days": 99}})", "thread: days"}})
    {
        namespace fs = std::filesystem;
        const auto dir = fs::temp_directory_path() / ("ratw-frames4-" + std::to_string(checks));
        fs::remove_all(dir);
        fs::create_directories(dir / "grammar");
        for (const char* f : {"facts.json", "settings.json", "moves.json", "claims.json", "groups.talk"})
            fs::copy_file(fs::path(Dir) / f, dir / f);
        fs::copy_file(fs::path(Dir) / "grammar" / "plain.grammar", dir / "grammar" / "plain.grammar");
        std::ofstream(dir / "frames.json") << text;
        Library bad;
        std::string p;
        expect(!bad.loadFoundation(dir.string(), p) && p.find(why) != std::string::npos, "frames.json refused (" + why + "): " + p);
        fs::remove_all(dir);
    }
    const auto& b = lib.beats();
    expect(b.curtChance == .5 && b.curtEffects.at("other").at("affinity") == -.5 && b.joinPeople == 3 && b.joinTiles == 3 &&
               b.threadDays == 2,
           "frames.json's Phase 4 settings as written");
}


void agendasAndGoals()
{
    // Doc 84 §7.3, §7.4, §8.5 (Phase 5): topics.json read and checked (today's numbers as the C++ had them), the topic
    // graph's steps, a talk turning in a lull to something on the agenda (a pivot along an edge, a marked change of
    // subject, a return to a thread), at most topics_per_talk topics; goals pursued a step a lull, given up when no line
    // fits, and kept between the two once done; invitations, promises and what talk says of a third party parsed.
    std::string problem;
    // topics.json: today's, read from the file, is the built-in book, number for number.
    {
        std::ifstream in(std::string(RATW_SOURCE_DIR) + "/Tests/fixtures/talk_topics/topics-today.json");
        std::stringstream text;
        text << in.rdbuf();
        expect(ratw::topics::load(text.str(), problem), "topics-today.json loads: " + problem);
        const auto& read = ratw::topics::book();
        const auto& today = ratw::topics::today();
        expect(read.topics.size() == today.topics.size() && !read.tuned() && !today.tuned(), "today's: the same topics, nothing cooled");
        for (const auto& [name, t] : today.topics)
            for (const auto& [kind, sc] : t.kinds)
            {
                const auto& r = read.score(name, kind);
                expect(r.base == sc.base && r.add == sc.add && r.scale == sc.scale && r.per == sc.per && r.step == sc.step &&
                           r.hourly == sc.hourly && r.jitter == sc.jitter && r.jitterScale == sc.jitterScale &&
                           read.news(name) == today.news(name) && read.everyday(name) == today.everyday(name),
                       "today's " + name + "/" + kind + " read as built in");
            }
        for (const auto& [text2, why] : std::vector<std::pair<std::string, std::string>>{
                 {R"({"topics": {}, "colour": 1})", "unknown field 'colour'"},
                 {R"({"topics": {"gossip": {"kinds": {"claim": {"base": 3}}}}})", "they are missing: accent/_"},
                 {R"({"topics": {"gossip": {"kinds": {"claim": {"base": 3, "speed": 1}}}}})", "unknown field 'speed'"},
                 {R"({"topics": {"gossip": {"news": true, "everyday": true, "kinds": {}}}})", "not both"},
                 {R"({"topics": {"gossip": {"next": {"weddings": ""}, "kinds": {}}}})", "which is no topic"},
                 {R"({"topics": {}, "favourites": {"piety=devout": ["sermons"]}})", "which is no topic"},
                 {R"({"topics": {}, "pair_bonus": {"lovers": 1}})", "a pair.band"},
                 {R"({"topics": {}, "pair_novelty": {"factor": 2}})", "factor is a number"}})
        {
            std::string why2;
            expect(!ratw::topics::load(text2, why2) && why2.find(why) != std::string::npos, "topics.json refused (" + why + "): " + why2);
        }
        ratw::topics::reset();
        expect(!ratw::topics::book().tuned(), "reset: today's");
    }
    // The live book loads with the library, and its steps' criteria with it.
    auto lib = foundation();
    expect(ratw::topics::book().tuned() && ratw::topics::book().topicsPerTalk == 3, "the live topics.json is tuned (topics_per_talk 3)");
    const auto& steps = lib.steps("prices");
    expect(std::any_of(steps.begin(), steps.end(), [](const Library::Step& s) { return s.to == "caravan" && !s.when.empty(); }),
           "prices leads to the caravan when the road is the cause");
    expect(lib.parse(R"(
rule ag_open_prices
when concept=talk.prices
does tell.complaint
say Herbs are dear again.
rule ag_agree
does react.complaint
say They are that.
rule ag_open_caravan
when concept=talk.caravan
does tell.news
say The wagon from #topic.where# is late.
rule ag_react_news
does react.news
say Is it now.
rule ag_open_lore
when concept=talk.lore
does tell.story
say My gran swore the river talks.
rule ag_react_story
does react.story
say Did she.
rule ag_bye
does farewell
say Farewell.
rule ag_bye_back
does farewell.return
say And you.
rule ag_intro
does introduce
when conv.goal=learn_name
say I'm #self.first#.
rule ag_intro_back
does introduce.return
say #self.first#. Good to know you.
rule ag_return
does return
say As I was saying.
)", "agenda.talk", problem), problem);
    const Freshness fresh{None, {}};
    const auto base = [&] {
        auto q = moment("talk.prices");
        q.facts["topic.cause"] = "caravan";
        for (auto* s : {&q.a, &q.b})
        {
            s->facts["talk"] = "chatty";
            s->facts["first"] = s->facts["name"];
        }
        return q;
    };
    const auto item = [](const std::string& topic, std::map<std::string, std::string> facts, double urgency, int index) {
        AgendaItem i;
        i.kind = "news";
        i.topic = topic;
        i.claim = topic;
        i.concept = "talk." + topic;
        i.facts = std::move(facts);
        i.urgency = urgency;
        i.index = index;
        return i;
    };
    const auto talk = [&](Conversation& c, const Query& q) {
        std::vector<SaidLine> said;
        if (auto first = lib.begin(c, q, fresh))
            said.push_back(*first);
        for (int i = 0; i < 20; ++i)
            if (auto line = lib.next(c, [&](int, int) { return q; }, fresh))
                said.push_back(*line);
            else
                break;
        return said;
    };
    const auto has = [](const SaidLine& l, const char* m) { return std::find(l.moves.begin(), l.moves.end(), m) != l.moves.end(); };
    // 1. A pivot: prices to the caravan (the road the cause), its own facts over the old topic's, marked as a pivot.
    {
        const auto q = base();
        Conversation c;
        c.seed = 3;
        c.agenda = {{item("caravan", {{"topic.where", "Coldwater"}}, 1.6, 4)}, {}};
        const auto said = talk(c, q);
        const auto turned = std::find_if(said.begin(), said.end(), [&](const SaidLine& l) { return has(l, "pivot"); });
        expect(turned != said.end() && turned->entry == "ag_open_caravan" && turned->moves.front() == "pivot" &&
                   turned->text.find("The wagon from Coldwater is late.") != std::string::npos &&
                   turned->text.find("The wagon") > 0 && turned->topic == "caravan",
               "a pivot to the caravan, its facts, its words after the pivot's");
        expect(c.agendaAt == 4 && c.topics == std::vector<std::string>({"prices", "caravan"}) && c.agenda[0][0].done,
               "the talk is on the agenda's item now");
        const auto after = std::next(turned);
        expect(after != said.end() && after->entry == "ag_react_news" && after->topic == "caravan", "and answered on it");
        expect(said.back().text == "And you." || said.back().text == "Farewell.", "then it closes");
    }
    // 2. No edge: a marked change of subject now and then (lore isn't a step from prices), never unmarked.
    {
        int changed = 0, closed = 0;
        for (std::uint64_t seed = 1; seed <= 40; ++seed)
        {
            const auto q = base();
            Conversation c;
            c.seed = seed;
            c.agenda = {{}, {item("lore", {}, 0.9, 1)}};
            const auto said = talk(c, q);
            const auto turned = std::find_if(said.begin(), said.end(), [](const SaidLine& l) { return l.topic == "lore"; });
            if (turned == said.end())
                ++closed;
            else
            {
                ++changed;
                expect(turned->moves.front() == "change_subject" && turned->who == 1 && turned->entry == "ag_open_lore",
                       "a change of subject by the one whose item it is, marked: " + turned->text);
            }
        }
        expect(changed > 8 && closed > 8, "a change of subject now and then: " + std::to_string(changed) + " of 40");
    }
    // 3. A thread left on the topic (pair.thread) is returned to, whatever the graph says: "As I was saying."
    {
        auto q = base();
        q.facts["pair.thread"] = "lore";
        Conversation c;
        c.seed = 1;
        c.agenda = {{item("lore", {}, 0.5, 2), item("caravan", {{"topic.where", "Larkfield"}}, 1.6, 3)}, {}};
        const auto said = talk(c, q);
        const auto back = std::find_if(said.begin(), said.end(), [&](const SaidLine& l) { return has(l, "return"); });
        expect(back != said.end() && back->text.rfind("As I was saying. My gran", 0) == 0 && back->topic == "lore",
               "a thread is returned to first: " + (back == said.end() ? std::string("none") : back->text));
    }
    // 4. At most topics_per_talk topics (3).
    {
        const auto q = base();
        Conversation c;
        c.seed = 9;
        c.budget = 14;
        c.agenda = {{item("caravan", {{"topic.where", "Coldwater"}}, 1.6, 0), item("lore", {}, 1.5, 1)},
                    {item("lore", {}, 1.4, 2), item("caravan", {{"topic.where", "Frosthollow"}}, 1.3, 3)}};
        talk(c, q);
        expect(c.topics.size() <= 3, "three topics at most: " + std::to_string(c.topics.size()));
    }
    // 5. A goal: learn_name, in a lull, said for it (conv.goal), done and kept; not again once kept between the two.
    {
        auto q = base();
        q.facts["pair.named"] = "no";
        Conversation c;
        c.seed = 4;
        const auto said = talk(c, q);
        const auto intro = std::find_if(said.begin(), said.end(), [](const SaidLine& l) { return l.goal == "learn_name"; });
        expect(intro != said.end() && intro->text == "I'm Maren." && intro->who == 0 && intro->goalDone && intro->goalDays == 30 &&
                   std::next(intro) != said.end() && std::next(intro)->text == "Ivo. Good to know you.",
               "learn_name: the one who didn't speak last introduces themselves, and is answered");
        q.facts["pair.was.goal_learn_name"] = "yes";
        Conversation d;
        d.seed = 4;
        const auto again = talk(d, q);
        expect(std::none_of(again.begin(), again.end(), [](const SaidLine& l) { return !l.goal.empty(); }),
               "learn_name done lately between the two: not again");
    }
    // 6. A goal no line fits (favour: no pre.request here) is given up, and the talk goes on.
    {
        auto q = base();
        q.a.facts["wealth"] = "poor";
        q.b.facts["wealth"] = "poor";
        q.facts["pair.trust"] = "40";
        Conversation c;
        c.seed = 4;
        const auto said = talk(c, q);
        expect(c.conv["goal_favour"] == "off" && said.size() >= 3 && c.ended == "farewell", "favour: no line, given up; the talk closes as ever");
    }
    // 7. Invitations, promises and what talk says of a third party: parsed, carried on the line, checked.
    {
        auto pl = foundation();
        expect(pl.parse(R"(
rule ag_inv
does invite
invite here 19 2
say Come by tomorrow?
rule ag_prom
does accept
promise self: healer 2
say I'll see the healer, I promise.
rule ag_fiddle
when concept=talk.day
binds x=self.neighbour
when x.said.took_up!=hammer
does tell.news
remember x: took_up=fiddle 30d
say #x.name# has taken up the fiddle.
)", "pledges.talk", problem), problem);
        const auto* inv = pl.find("ag_inv");
        const auto* prom = pl.find("ag_prom");
        const auto* fid = pl.find("ag_fiddle");
        expect(inv && inv->invitePlace == "here" && inv->inviteHour == 19 && inv->inviteDays == 2, "an invitation parsed");
        expect(prom && prom->promiseWhat == "healer" && prom->promiseDays == 2, "a promise parsed");
        expect(fid && fid->ledger && fid->remember.size() == 1 && fid->remember[0].scope == "x", "a person fact remembered, a callback (x.said)");
        auto q = moment("talk.day");
        q.bind = [](const std::string& slot, int, Facts& out) {
            if (slot != "self.neighbour")
                return false;
            out = {{"id", "wick"}, {"name", "Wick"}};
            return true;
        };
        const auto said = pl.render(*fid, q, 1);
        expect(!said.effects.empty() && said.effects.front().scope == "said" && said.effects.front().owner == "wick|maren" &&
                   said.effects.front().key == "took_up" && said.effects.front().value == "fiddle" && said.effects.front().days == 30,
               "remember x: kept as what talk said of the one bound, by the speaker");
        q.bind = [](const std::string& slot, int, Facts& out) {
            if (slot != "self.neighbour")
                return false;
            out = {{"id", "wick"}, {"name", "Wick"}, {"said.took_up", "hammer"}};
            return true;
        };
        expect(!pl.fits(*fid, q), "what talk said of them before (x.said.took_up=hammer) holds it back");
        for (const auto& [text, why] : std::vector<std::pair<std::string, std::string>>{
                 {"rule x1\ndoes invite\ninvite tavern 19 2\nsay Come.", "invite is"},
                 {"rule x2\ndoes invite\ninvite here 25 2\nsay Come.", "invite is"},
                 {"rule x3\ndoes accept\npromise other: healer 2\nsay Yes.", "promise is"},
                 {"rule x4\ndoes accept\npromise self: Healer 2\nsay Yes.", "promise is"},
                 {"rule x5\nwhen concept=talk.day\nremember x: a=b 1d\nsay Hm.", "nobody bound"},
                 {"rule x6\nwhen concept=talk.day pair.talked.weather<two\nsay Hm.", "not a number"}})
        {
            auto bad = foundation();
            std::string why2;
            expect(!bad.parse(text, "bad.talk", why2) && why2.find(why) != std::string::npos, "refused (" + why + "): " + why2);
        }
        auto ok = foundation();
        expect(ok.parse("rule y1\nwhen concept=talk.day pair.talked.weather<2 pair.recall.weather=warmed pair.was.carry=declined self.was.asked=yes "
                        "other.told_me=theft other.promised=lapsed pair.last_talk<0.3 pair.last_outcome=soured pair.kept_invite=no\nsay Hm.",
                        "ok.talk", problem),
               "the ledger's facts are criteria: " + problem);
        expect(ok.find("y1") && ok.find("y1")->ledger, "a line reading them is a callback");
        expect(Library::ledgerFact("pair.talked.weather") && Library::ledgerFact("self.was.k") && !Library::ledgerFact("pair.band"),
               "ledgerFact: the ledger's keys only");
    }
}

void opinionsMoodsAccent()
{
    // Doc 84 §7.4-7.6, §8.8, §10.6 (Phase 6): stances from a resident's own facts and the events that drift them, likes
    // held for good, how a trait leans on a move, the town's mood, accommodation, opinions aired and answered as the
    // stances are, vent, and nothing told twice or said over another's words; pronouns by sex.
    std::string problem;
    auto lib = foundation();
    expect(lib.issues().size() >= 10 && lib.issue("watch") && lib.issue("watch")->words == "the Watch", "opinions.json: the issues");
    expect(!lib.likeItems().empty() && !lib.temperaments().empty() && lib.moods().size() == 4 && lib.accommodation().on,
           "likes, temperaments, the four town moods and accommodation load");
    // opinions.json refused: copied beside the foundation's files, one thing wrong at a time.
    {
        namespace fs = std::filesystem;
        const auto tmp = fs::temp_directory_path() / ("ratw-opinions-" + std::to_string(::getpid()));
        fs::remove_all(tmp);
        fs::create_directories(tmp / "grammar");
        for (const char* f : {"facts.json", "settings.json", "claims.json", "moves.json", "frames.json", "topics.json", "temperaments.json",
                              "groups.talk"})
            fs::copy_file(fs::path(Dir) / f, tmp / f);
        for (const auto& g : fs::directory_iterator(fs::path(Dir) / "grammar"))
            fs::copy_file(g.path(), tmp / "grammar" / g.path().filename());
        for (const auto& [text, why] : std::vector<std::pair<std::string, std::string>>{
                 {R"({"issues": {"watch": {"words": "the Watch", "colour": 1}}})", "no field 'colour'"},
                 {R"({"issues": {"watch": {"words": "the Watch", "for": [{"when": "here.weather=rain"}]}}})", "the resident's own facts"},
                 {R"({"issues": {"watch": {"words": "the Watch", "drift": {"robbed_by_moonlight": -1}}}})", "no event the game watches for"},
                 {R"({"issues": {"watch": {"words": "the Watch", "drift": {"robbed_uncaught": -1}}}})", "needs a reason of its own"},
                 {R"({"issues": {"watch": {"words": "the Watch", "for": [{"when": "self.role=guard", "why": "nope"}]}}})", "not among its reasons"},
                 {R"({"issues": {"watch": {"words": "the Watch", "topics": ["weddings"]}}})", "not topics.json's"},
                 {R"({"likes": {"tea": {"kind": "drink", "words": "tea"}}})", "kind is pastime, food, weather or thing"}})
        {
            std::ofstream(tmp / "opinions.json") << text;
            Library l;
            std::string why2;
            expect(!l.loadFoundation(tmp.string(), why2) && why2.find(why) != std::string::npos, "opinions.json refused (" + why + "): " + why2);
        }
        std::ofstream(tmp / "temperaments.json") << R"({"leans": {"temper=short": {"shout": 1.5}}})";
        std::ofstream(tmp / "opinions.json") << "{}";
        Library l;
        std::string why2;
        expect(!l.loadFoundation(tmp.string(), why2) && why2.find("not a move") != std::string::npos, "temperaments.json refused: " + why2);
        std::ofstream(tmp / "temperaments.json") << R"({"leans": {"temper=short": {"take_offence": 2.5}}})";
        expect(!l.loadFoundation(tmp.string(), why2) && why2.find("0.3 to 2") != std::string::npos, "a lean is 0.3 to 2: " + why2);
        fs::remove_all(tmp);
    }
    // Stances: from the resident's own facts; a reason true of them; strength the steps it leans; drift a step more.
    {
        Facts guard{{"id", "g1"}, {"role", "guard"}, {"class", "middle"}, {"honesty", "plain"}, {"quarter", "commons"}};
        lib.stances(guard, {});
        expect(guard["op.watch"] == "for" && guard["op_strength.watch"] == "1" &&
                   guard["op_why.watch"].find("I walk with them myself") != std::string::npos,
               "a guard is for the Watch, and says why: " + guard["op_why.watch"]);
        Facts sly{{"id", "s1"}, {"role", "civilian"}, {"honesty", "sly"}, {"quarter", "sump"}, {"class", "lower"}};
        lib.stances(sly, {});
        expect(sly["op.watch"] == "against" && sly["op_strength.watch"] == "2" && sly["op.sump"] == "for", "a sly wolf of the Sump is against the Watch, 2");
        Facts none{{"id", "n1"}, {"role", "civilian"}};
        lib.stances(none, {});
        expect(none["op.watch"] == "mixed" && none["op_strength.watch"] == "0" && !none.count("op_why.watch") && !none.count("op.sump"),
               "nothing pulling: mixed, no view; no quarter, no view of the Sump at all");
        lib.stances(none, {{"watch", {-1, "robbed_uncaught"}}});
        expect(none["op.watch"] == "against" && none["op_drift.watch"] == "-1" &&
                   none["op_why.watch"] == "they never caught whoever robbed me",
               "a theft never solved drifts it a step, with the event's own reason");
        lib.stances(guard, {{"watch", {-2, "robbed_uncaught"}}});
        expect(guard["op.watch"] == "against", "events can outweigh a lean");
    }
    // Likes: the same every time for one wolf; a list fact holds when the list has it.
    {
        Facts a{{"id", "ivo"}, {"stage", "adult"}, {"homeless", "no"}, {"role", "civilian"}}, b = a;
        lib.likes(a);
        lib.likes(b);
        expect(!a["likes"].empty() && a["likes"] == b["likes"] && a["dislikes"] == b["dislikes"] && !a["food_words"].empty() &&
                   !a["dislike_kind"].empty(),
               "likes held for good: " + a["likes"] + " / " + a["dislikes"]);
        auto l2 = foundation();
        const auto first = a["likes"].substr(0, a["likes"].find(' '));
        expect(l2.parse("rule lk_t\nwhen concept=talk.food self.likes=" + first + "\nsay Mine.\n", "lk.talk", problem), problem);
        auto q = moment("talk.food");
        q.a.facts = a;
        expect(l2.pick(q, 1, None, {}).id == "lk_t", "self.likes=" + first + " holds of one whose list has it");
        q.a.facts["likes"] = "nothing_else";
        expect(l2.pick(q, 1, None, {}).id.empty(), "and not of one whose list hasn't");
    }
    // Temperaments: a chatty wolf pivots more, a quiet one less; the whole lean held to 0.3-2.
    {
        Speaker chatty, quiet, many;
        chatty.facts = {{"talk", "chatty"}};
        quiet.facts = {{"talk", "quiet"}};
        many.facts = {{"talk", "chatty"}, {"drive", "restless"}, {"warmth", "warm"}};
        expect(std::abs(lib.lean(chatty, "pivot") - 1.4) < 1e-9 && std::abs(lib.lean(quiet, "pivot") - .6) < 1e-9, "pivot: chatty 1.4, quiet 0.6");
        expect(lib.lean(many, "change_subject") <= 2.0 && std::abs(lib.lean(chatty, "react.news.doubt") - 1) < 1e-9, "held to 2; no lean, 1");
        Speaker nervy;
        nervy.facts = {{"temper", "nervy"}};
        expect(std::abs(lib.lean(nervy, "react.news.doubt") - 1.4) < 1e-9, "a variant's own lean");
    }
    // The town's mood and accommodation, worked out for a query.
    {
        auto q = moment("talk.smalltalk");
        q.facts["world.death"] = "recent";
        q.facts["world.bandits"] = "near";
        lib.accommodate(q);
        expect(q.facts["world.mood"] == "grief", "a death in town: grief (the first mood that holds)");
        q.facts["world.death"] = "no";
        lib.accommodate(q);
        expect(q.facts["world.mood"] == "uneasy", "bandits near: uneasy");
        q.facts["world.bandits"] = "no";
        lib.accommodate(q);
        expect(!q.facts.count("world.mood"), "no mood");
        const auto speaks = [&](const std::string& accentA, const std::string& fromB, Facts pair, Facts b = {}) {
            auto x = moment("talk.smalltalk");
            x.a.facts["from"] = "coldwater";
            x.a.facts["accent"] = accentA;
            x.a.facts["class"] = "middle";
            x.b.facts["from"] = fromB;
            x.b.facts["accent"] = "light";
            for (const auto& [k, v] : pair)
                x.facts[k] = v;
            for (const auto& [k, v] : b)
                x.b.facts[k] = v;
            lib.accommodate(x);
            return x.a.facts["speaks"];
        };
        expect(speaks("light", "coldwater", {{"pair.kin", "siblings"}}) == "broad", "a step broader to kin of one's own speech");
        expect(speaks("light", "larkfield", {{"pair.kin", "siblings"}, {"pair.band", "family"}}) == "light", "not to kin who speak otherwise");
        expect(speaks("broad", "larkfield", {{"pair.band", "strangers"}}) == "light", "a step plainer to a stranger from elsewhere");
        expect(speaks("light", "coldwater", {{"pair.band", "acquaintances"}}, {{"class", "upper"}}) == "none", "plainer to one a class above");
        expect(speaks("none", "coldwater", {{"pair.band", "friends"}}) == "light" && speaks("broad", "coldwater", {{"pair.band", "friends"}}) == "broad",
               "none rises no higher than light; broad stays broad");
        {
            auto x = moment("talk.smalltalk");
            x.a.facts["from"] = x.b.facts["from"] = "ridgemere";
            x.a.facts["accent"] = "light";
            x.facts["pair.kin"] = "siblings";
            lib.accommodate(x);
            expect(!x.a.facts.count("speaks"), "Ridgemere's plain speech: no dialect to speak more or less of");
        }
        // Spoken in that accent: a broad speaker's dialect words, and the line records the accent it was said in.
        auto l2 = foundation();
        expect(l2.parse("rule acc_t\nwhen concept=talk.smalltalk\nsay #greet#.\n", "acc.talk", problem), problem);
        auto x = moment("talk.smalltalk");
        x.a.facts["from"] = "coldwater";
        x.a.facts["accent"] = "none";
        x.a.grammar = {"coldwater"};
        x.b.facts["from"] = "coldwater";
        x.facts["pair.kin"] = "siblings";
        Conversation c;
        c.seed = 5;
        auto first = l2.begin(c, x, Freshness{None, {}});
        expect(first && first->accent == "light", "the opener records the accent it was spoken in: " + (first ? first->accent : std::string()));
    }
    // An opinion in a lull, on a topic its issue comes up on; the answer as the stances are.
    {
        auto l2 = foundation();
        for (const char* f : {"rules/opinions.talk", "rules/turns.talk"})
        {
            std::ifstream in(Dir + "/" + f);
            std::stringstream text;
            text << in.rdbuf();
            expect(l2.parse(text.str(), f, problem), std::string(f) + ": " + problem);
        }
        expect(l2.parse("rule op_open_crime\nwhen concept=talk.crime\ndoes tell.news\nsay There was a theft by the well.\n"
                        "rule op_react\ndoes react.news\nsay Was there now.\n", "crime.talk", problem), problem);
        const auto run = [&](const std::string& sideB, std::uint64_t seed, const char* band = "friends") {
            auto q = moment("talk.crime");
            q.facts["pair.band"] = band;
            q.a.facts["role"] = "guard";
            q.a.facts["talk"] = "chatty";
            q.a.facts["honesty"] = "frank";
            q.b.facts["talk"] = "chatty";
            if (sideB == "against")
                q.b.facts["honesty"] = "sly", q.b.facts["quarter"] = "sump", q.b.facts["class"] = "lower";
            else
                q.b.facts["role"] = "guard";
            lib.stances(q.a.facts, {});
            lib.stances(q.b.facts, {});
            Conversation c;
            c.seed = seed;
            std::vector<SaidLine> said;
            if (auto f = l2.begin(c, q, Freshness{None, {}}))
                said.push_back(*f);
            for (int i = 0; i < 16; ++i)
                if (auto line = l2.next(c, [&](int, int) { return q; }, Freshness{None, {}}))
                    said.push_back(*line);
                else
                    break;
            return said;
        };
        int aired = 0, answered = 0;
        for (const auto& side : {"for", "against"})
            for (std::uint64_t seed = 1; seed <= 30; ++seed)
            {
                const auto said = run(side, seed);
                const auto given = std::find_if(said.begin(), said.end(), [](const SaidLine& l) {
                    return std::find(l.moves.begin(), l.moves.end(), "give.opinion") != l.moves.end();
                });
                if (given == said.end())
                    continue;
                ++aired;
                std::string lowered = given->text;
                std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
                expect((given->issue == "watch" && lowered.find("watch") != std::string::npos) ||
                           (given->issue == "sump" && lowered.find("sump") != std::string::npos),
                       "an opinion on the Watch or the Sump, on talk of crime: " + given->text);
                const auto reply = std::next(given);
                if (reply == said.end() || reply->issue.empty())
                    continue;
                ++answered;
                const bool agrees = std::find(reply->moves.begin(), reply->moves.end(), "agree") != reply->moves.end();
                const bool differs = std::find(reply->moves.begin(), reply->moves.end(), "disagree") != reply->moves.end();
                if (given->stance == "mixed" || reply->stance == "mixed")
                    continue;                       // (Half: either answer may be had from one with no side.)
                expect(given->stance == reply->stance ? !differs : !agrees,
                       "answered as the stances are (" + given->stance + " / " + reply->stance + "): " + given->entry + " " + given->issue + " who " +
                           std::to_string(given->who) + ": " + given->text + " / " + reply->entry + " who " + std::to_string(reply->who) + ": " + reply->text);
            }
        expect(aired >= 10 && answered >= 8, "opinions aired and answered: " + std::to_string(aired) + " / " + std::to_string(answered));
    }
    // vent: a low mood told to a friend, and sympathy back.
    {
        auto l2 = foundation();
        for (const char* f : {"rules/opinions.talk", "rules/turns.talk"})
        {
            std::ifstream in(Dir + "/" + f);
            std::stringstream text;
            text << in.rdbuf();
            expect(l2.parse(text.str(), f, problem), problem);
        }
        expect(l2.parse("rule v_open\nwhen concept=talk.weather\ndoes tell.self\nsay Grey again.\n"
                        "rule v_react\ndoes react.self\nsay So it is.\n", "v.talk", problem), problem);
        bool vented = false;
        for (std::uint64_t seed = 1; seed <= 20 && !vented; ++seed)
        {
            auto q = moment("talk.weather");
            q.a.facts["talk"] = q.b.facts["talk"] = "chatty";
            q.a.facts["mood"] = "low";
            q.a.facts["mood_why"] = "robbed";
            Conversation c;
            c.seed = seed;
            const auto f = [&](int speaker, int) {
                auto x = q;
                if (speaker == 1)
                    std::swap(x.a, x.b);            // (Each one's own mood: other.mood is the speaker's other.)
                return q;
            };
            std::vector<SaidLine> said;
            if (auto first = l2.begin(c, q, Freshness{None, {}}))
                said.push_back(*first);
            for (int i = 0; i < 12; ++i)
                if (auto line = l2.next(c, f, Freshness{None, {}}))
                    said.push_back(*line);
                else
                    break;
            for (std::size_t i = 0; i + 1 < said.size(); ++i)
                if (said[i].goal == "vent")
                {
                    vented = true;
                    expect(said[i].entry == "md_vent_robbed" && said[i + 1].who == 1, "vent: " + said[i].text);
                    expect(std::find(said[i + 1].moves.begin(), said[i + 1].moves.end(), "sympathise") != said[i + 1].moves.end() ||
                               std::find(said[i + 1].moves.begin(), said[i + 1].moves.end(), "reassure") != said[i + 1].moves.end(),
                           "and the friend sympathises: " + said[i + 1].text);
                }
        }
        expect(vented, "a low mood is vented to a friend");
    }
    // Never told again, never another's words over: the engine refuses them (Freshness::told, ::collides).
    {
        auto l2 = foundation();
        expect(l2.parse("rule t_tell\nwhen concept=talk.gossip\ndoes tell.gossip\nasserts\nsay Osric wed Signe, I hear.\n"
                        "rule t_story\nwhen concept=talk.lore\ndoes tell.story\nsay My gran swore the river talks, and it does.\n"
                        "rule t_story2\nwhen concept=talk.lore\ndoes tell.story\nsay The old mill wheel turns by itself at night.\n",
                        "t.talk", problem), problem);
        auto q = moment("talk.gossip");
        q.facts["topic.claim_kind"] = "marriage";
        Freshness told{None, {}};
        told.told = [](const std::string& listener, const std::string& what) { return listener == "ivo" && what == "claim:signe|wed"; };
        Conversation c;
        c.claim = "signe|wed";
        expect(!l2.begin(c, q, told) && c.ended == "told", "a claim the listener was told is never told them again");
        Conversation c2;
        c2.claim = "signe|other";
        expect(l2.begin(c2, q, told).has_value(), "another claim is");
        auto lore = moment("talk.lore");
        Freshness heard{None, {}};
        heard.told = [](const std::string&, const std::string& what) { return what == "story:t_story"; };
        for (std::uint64_t seed = 1; seed <= 10; ++seed)
        {
            Conversation s;
            s.seed = seed;
            const auto first = l2.begin(s, lore, heard);
            expect(first && first->entry == "t_story2", "a story the listener heard is never told them again");
        }
        Freshness over{None, {}};
        over.collides = [](const std::string& words, const std::string& speaker) {
            return speaker == "maren" && words == "My gran swore the river talks, and it does.";   // (Another said it here.)
        };
        for (std::uint64_t seed = 1; seed <= 10; ++seed)
        {
            Conversation s;
            s.seed = seed;
            const auto first = l2.begin(s, lore, over);
            expect(first && first->entry == "t_story2", "another's distinctive words said here within the hour are passed over");
        }
    }
    // Pronouns by sex: a topic's subject and victim, one's own people.
    {
        auto l2 = foundation();
        expect(l2.parse("rule p_t\nwhen concept=talk.gossip\nsay #topic.subject# lost #topic.their# purse. #topic.victim_they.capitalize# saw it.\n"
                        "rule p_s\nwhen concept=talk.family\nsay #self.spouse# sends #self.spouse_their# best.\n", "p.talk", problem), problem);
        auto q = moment("talk.gossip");
        q.facts["topic.subject"] = "Finn Dripwick";
        q.facts["topic.subject_sex"] = "male";
        q.facts["topic.victim"] = "Signe Sootwell";
        q.facts["topic.victim_sex"] = "female";
        const auto said = l2.pick(q, 1, None, {});
        expect(!said.lines.empty() && said.lines[0].second == "Finn Dripwick lost his purse. She saw it.", "pronouns by sex: " +
               (said.lines.empty() ? std::string() : said.lines[0].second));
        auto f = moment("talk.family");
        f.a.facts["spouse"] = "Osric Wetford";
        f.a.facts["spouse_sex"] = "male";
        const auto s2 = l2.pick(f, 1, None, {});
        expect(!s2.lines.empty() && s2.lines[0].second == "Osric Wetford sends his best.", "one's spouse by sex");
        f.a.facts.erase("spouse_sex");
        expect(l2.pick(f, 1, None, {}).id.empty(), "never 'their' for one wolf: no sex, no line");
    }
}

int main()
{
    try
    {
        theFormat();
        whoSaysWhat();
        parallelAlternatives();
        choosing();
        grammar();
        chainsAndMemory();
        playerAnswers();
        jobs();
        theRealLibrary();
        salience();
        theStagedEconomysTalk();
        theForbiddenWord();
        theCountrysVoices();
        truthFixes();
        movesAndTags();
        turnEngine();
        beatsWhenSpoken();
        agendasAndGoals();
        opinionsMoodsAccent();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Talk tests passed: " << checks << " checks.\n";
    return 0;
}
