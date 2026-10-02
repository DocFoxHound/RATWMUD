// NPC-to-NPC scenes (RatwScenes.h; Docs/Design/30-towns-and-talk.md, phase 6): the file format and its checks, who
// says what, blanks, optional turns, the preference for scenes not heard, and the real library loading whole.
#include "RatwScenes.h"

#include <iostream>
#include <stdexcept>
#include <string>

using namespace ratw::scenes;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

Situation moment(const std::string& topic)
{
    Situation s;
    s.topic = topic;
    s.tags = {{"region", "ridgemere"}, {"place", "market"}, {"band", "friends"}, {"time", "day"}, {"season", "autumn"},
              {"day", "market"}, {"weather", "rain"}, {"item", "herbs"}, {"dir", "up"}};
    s.a = {"Maren", "female", "adult", "baker", "merchant"};
    s.b = {"Ivo", "male", "old", "fisher", "civilian"};
    s.blanks = {{"price", "3 pennies"}, {"town", "Ridgemere"}};
    return s;
}

const char* Sample = R"(
group rain_coast = ridgemere saltreach

scene herbs_dear
when topic=prices item=herbs dir=up region=rain_coast place=market|street
weight 2
a : Herbs at {price} now, {b}.
b[merchant] : I pay more, you pay more.
b[old] : In my day a bundle cost less than a smile.
b : Robbery.
a?[rivals] : And I suppose you're pleased.
a?[friends] : Come by tonight; I'll share mine.

scene anywhere_talk
when topic=smalltalk
a : Morning, {b}. | Well met, {b}.
b : Morning.

scene needs_subject
when topic=smalltalk
weight 5
a : Did you hear about {subject}?
b : No.

scene a_bark
when topic=bark
a[merchant] : Fresh bread! Still warm!
a : Mind the puddles.
)";
} // namespace

void theFormat()
{
    Library lib;
    std::string problem;
    expect(lib.parse(Sample, "sample.scene", problem), "the sample parses: " + problem);
    expect(lib.size() == 4 && lib.hasTopic("prices") && lib.hasTopic("bark"), "four scenes, indexed by topic");
    const auto bad = [&](const std::string& text, const std::string& says) {
        Library l;
        std::string p;
        expect(!l.parse(text, "bad.scene", p) && p.find(says) != std::string::npos, "refused (" + says + "): " + p);
        expect(l.size() == 0, "and nothing of it loaded");
    };
    bad("scene x\nwhen topic=smalltalk\na : hi\n", "needs a line b always says");
    bad("scene x\nwhen topic=smalltalk\nb : hi\n", "needs a line a always says");
    bad("scene x\na : hi\nb : yo\n", "has no topic");
    bad("scene x\nwhen topic=gossiping\na : hi\nb : yo\n", "is not a topic");
    bad("scene x\nwhen topic=smalltalk place=moon\na : hi\nb : yo\n", "is not a place");
    bad("scene x\nwhen topic=smalltalk region=atlantis\na : hi\nb : yo\n", "not a region");
    bad("scene x\nwhen topic=smalltalk\na : hello {nobody}\nb : yo\n", "unknown blank {nobody}");
    bad("scene x\nwhen topic=smalltalk\na[wizard] : hi\na : hi\nb : yo\n", "'wizard' is not");
    bad("scene x\nwhen topic=smalltalk\na : hi\nb : yo\nscene x\nwhen topic=smalltalk\na : hi\nb : yo\n", "defined twice");
    bad("scene x\nwhen topic=bark\na : hi\nb : yo\n", "one speaker");
    bad("scene x\nwhen topic=smalltalk\na : " + std::string(170, 'w') + "\nb : yo\n", "160 characters");
    bad("a : hi\n", "expected 'scene <id>' first");
}

void whoSaysWhat()
{
    Library lib;
    std::string problem;
    lib.parse(Sample, "sample.scene", problem);
    const auto& herbs = lib.scenes()[0];
    auto s = moment("prices");
    auto r = lib.render(herbs, s, 1);
    expect(r.lines.size() == 3, "a, b, and a's optional turn for friends");
    expect(r.lines[0].second == "Herbs at 3 pennies now, Ivo.", "blanks filled: " + r.lines[0].second);
    expect(r.lines[1].second.find("In my day") != std::string::npos, "b's first fitting line: b is old, not a merchant");
    expect(r.lines[2].second.find("share mine") != std::string::npos && r.lines[2].first == 0, "friends: the optional turn");
    s.b.role = "merchant";
    expect(lib.render(herbs, s, 1).lines[1].second.find("I pay more") != std::string::npos, "the first tag that holds wins");
    s.tags["band"] = "strangers";
    expect(lib.render(herbs, s, 1).lines.size() == 2, "optional turns that don't hold are left out");
    s.tags["region"] = "saltreach";
    expect(!lib.render(herbs, s, 1).id.empty(), "a group stands for its regions");
    s.tags["region"] = "ser_ferro";
    expect(lib.render(herbs, s, 1).id.empty(), "elsewhere, the scene doesn't fit");
    s.tags["region"] = "ridgemere";
    s.blanks.erase("price");
    expect(lib.render(herbs, s, 1).id.empty(), "a blank that can't be filled: not used");
}

void picking()
{
    Library lib;
    std::string problem;
    lib.parse(Sample, "sample.scene", problem);
    auto s = moment("smalltalk");
    const auto none = [](const std::string&) { return false; };
    for (std::uint64_t seed = 0; seed < 20; ++seed)
        expect(lib.pick(s, seed, none, {}).id == "anywhere_talk", "a scene whose blanks can't be filled is never picked");
    s.blanks["subject"] = "Old Rook";
    int subject = 0;
    for (std::uint64_t seed = 0; seed < 200; ++seed)
        subject += lib.pick(s, seed, none, {}).id == "needs_subject";
    expect(subject > 120, "the heavier scene comes up more often (" + std::to_string(subject) + "/200)");
    const auto heardIt = [](const std::string& id) { return id == "needs_subject"; };
    for (std::uint64_t seed = 0; seed < 50; ++seed)
        expect(lib.pick(s, seed, heardIt, {}).id == "anywhere_talk", "one not heard comes before one heard");
    const auto heardAll = [](const std::string&) { return true; };
    expect(!lib.pick(s, 3, heardAll, {}).id.empty(), "with all heard, one is still said");
    expect(lib.pick(s, 3, none, {"needs_subject", "anywhere_talk"}).id.empty(), "never one used here lately");
    std::set<std::string> said;
    for (std::uint64_t seed = 0; seed < 40; ++seed)
        said.insert(lib.pick(s, seed, none, {}).lines[0].second);
    expect(said.size() >= 3, "the alternatives vary the words");
    auto bark = moment("bark");
    bark.b = {};
    expect(lib.pick(bark, 1, none, {}).lines[0].second == "Fresh bread! Still warm!", "a bark: the merchant's call");
    expect(lib.scenes()[3].always, "barks may be repeated");
}

void jobs()
{
    expect(jobCategory("Master Smith of Westmarch", "", "civilian", 40) == "smith", "smith");
    expect(jobCategory("A lean grey timber wolf, the innkeeper of Westmarch.", "keeps The Last Lantern", "merchant", 40) == "innkeeper", "innkeeper");
    expect(jobCategory("", "fishes the grey coast", "civilian", 30) == "fisher", "fisher");
    expect(jobCategory("", "stands watch", "guard", 30) == "guard", "guard");
    expect(jobCategory("", "plays about the town", "civilian", 9) == "child", "children are children");
    expect(jobCategory("", "keeps Westmarch Mercantile", "merchant", 40) == "merchant", "shopkeeper");
}

void theRealLibrary()
{
    Library lib;
    std::string problem;
    expect(lib.load(std::string(RATW_SOURCE_DIR) + "/Data/Voice/scenes", problem), "the library loads whole:\n" + problem);
    expect(lib.size() >= 100, "and is a library (" + std::to_string(lib.size()) + " scenes)");
    for (const auto& topic : {"smalltalk", "work", "weather", "prices", "caravan", "crime", "life", "festival", "player",
                              "gossip", "news", "quarrel", "friends", "day", "bark"})
        expect(lib.hasTopic(topic), std::string("scenes for ") + topic);
}

int main()
{
    try
    {
        theFormat();
        whoSaysWhat();
        picking();
        jobs();
        theRealLibrary();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Scene tests passed: " << checks << " checks.\n";
    return 0;
}
