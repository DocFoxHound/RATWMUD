// Talk (RatwTalk.h; Docs/Design/62-talk.md): the file formats and their checks, who says what in a scene, rules and
// how one is chosen (tiers with chance, strict priority, filler, not heard first), the grammar (layers, conditions,
// habits, modifiers, pronouns, binding), and the real library loading whole.
#include "RatwTalk.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

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
    auto said = lib.converse(q, 1, None, {});
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

    // A chain ends after 8 beats, whatever its rules say.
    auto loop = foundation();
    expect(loop.parse("rule ping\nwhen concept=talk.smalltalk\nsay Ping.\nthen other: talk.day\nrule pong\nwhen concept=talk.day\nsay Pong.\nthen other: talk.smalltalk\n",
                      "loop.talk", problem), problem);
    const auto lines = loop.converse(moment("talk.smalltalk"), 1, None, {}).lines;
    expect(lines.size() == 8 && lines[7] == std::pair<int, std::string>{1, "Pong."}, "eight beats at most, turn and turn about");

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
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Talk tests passed: " << checks << " checks.\n";
    return 0;
}
