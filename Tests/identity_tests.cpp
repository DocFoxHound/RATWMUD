// A wolf's identity (Core/RatwIdentity.h; Docs/Design/77-central-accounts-and-hosted-worlds.md, 4.1): the shared rules,
// then ratw_identity (central's check) driven the way central drives it, one JSON line in and one out.
//   identity_tests [PATH_TO_RATW_IDENTITY]
#include "RatwIdentity.h"
#include "RatwJsonDoc.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

using namespace ratw;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

json::Value parsed(const std::string& text)
{
    json::Value v;
    std::string error;
    expect(json::parse(text, v, error), "JSON: " + error + " in " + text);
    return v;
}

const std::string Look = R"({"species":"timber","sex":"female","stature":"average","pattern":"solid","baseColor":3,)"
                         R"("gradientColor":1,"markingColor":2,"gradientAmount":0.4,"patternAmount":0.2})";

void theRules()
{
    expect(identity::name("  ash tallow ") == "Ash tallow", "a name is trimmed and capitalised");
    expect(identity::name("A").empty(), "one character is too short");
    expect(identity::name(std::string(33, 'a')).empty(), "33 characters is too long");
    expect(identity::name("Ash\x01").empty(), "control characters are refused");
    expect(identity::age(parsed(R"({"age": 6})")) == 6 && identity::age(parsed(R"({"age": 99})")) == 99, "6 and 99");
    for (const char* bad : {R"({"age": 5})", R"({"age": 100})", R"({"age": 7.5})", R"({"age": "7"})", R"({})"})
        expect(identity::age(parsed(bad)) < 0, std::string("age refused: ") + bad);
    std::string tier, family;
    expect(identity::gift(nullptr, tier, family) && tier == "normal" && family.empty(), "no Gift is Normal");
    const auto gift = [&](const char* text) {
        const auto v = parsed(text);
        return identity::gift(&v, tier, family);
    };
    expect(gift(R"({"tier":"quickened","family":"fire"})") && tier == "quickened" && family == "fire", "Quickened Fire");
    expect(!gift(R"({"tier":"gifted"})"), "a tier needs a family");
    expect(!gift(R"({"tier":"normal","family":"fire"})"), "Normal has no family");
    expect(!gift(R"({"tier":"gifted","family":"death_walker"})"), "Death Walkers are NPCs only");
    expect(!gift(R"({"tier":"gifted","family":"fire","x":"y"})"), "nothing else in a Gift");
    const auto fam = [&](const char* text) {
        const auto v = parsed(text);
        return identity::family(v.find("f"), family);
    };
    expect(fam(R"({})") && family.empty(), "no family");
    expect(fam(R"({"f": null})") && fam(R"({"f": ""})") && family.empty(), "null or empty is none");
    expect(fam(R"({"f": "seer"})") && family == "seer", "a playable family");
    expect(!fam(R"({"f": "death_walker"})") && !fam(R"({"f": "nope"})") && !fam(R"({"f": 3})"), "others refused");
}

// Runs ratw_identity on the lines and returns its replies.
std::vector<json::Value> run(const std::string& command, const std::vector<std::string>& lines)
{
    char in[] = "/tmp/ratw_identity_inXXXXXX";
    const int fd = mkstemp(in);
    expect(fd >= 0, "temp file");
    {
        std::ofstream f(in);
        for (const auto& l : lines)
            f << l << '\n';
    }
    close(fd);
    FILE* p = popen(("'" + command + "' < " + in).c_str(), "r");
    expect(p != nullptr, "ratw_identity starts");
    std::string out;
    char buffer[4096];
    while (std::size_t n = fread(buffer, 1, sizeof buffer, p))
        out.append(buffer, n);
    expect(pclose(p) == 0, "ratw_identity exits 0");
    unlink(in);
    std::vector<json::Value> replies;
    std::size_t start = 0;
    for (std::size_t end; (end = out.find('\n', start)) != std::string::npos; start = end + 1)
        replies.push_back(parsed(out.substr(start, end - start)));
    expect(replies.size() == lines.size(), "one reply per request");
    return replies;
}

void theCommand(const std::string& command)
{
    const auto r = run(command, {
        R"({"check":"version"})",
        R"({"check":"wolf","wolf":{"name":" ash tallow","age":7,"appearance":)" + Look + R"(,"family":"fire"}})",
        R"({"check":"wolf","wolf":{"name":"Ash","age":7,"appearance":)" + Look + "}}",
        R"({"check":"wolf","wolf":{"name":"Ash","age":5,"appearance":)" + Look + "}}",
        R"({"check":"wolf","wolf":{"name":"Ash","age":7,"appearance":{"species":"timber"}}})",
        R"({"check":"wolf","wolf":{"name":"Ash","age":7,"appearance":)" + Look + R"(,"family":"death_walker"}})",
        R"({"check":"wolf","wolf":{"name":"Ash","age":7,"appearance":)" + Look + R"(,"level":40}})",
        R"({"check":"look","appearance":{"species":"timber","sex":"other","stature":"average","pattern":"solid","baseColor":3,"gradientColor":1,"markingColor":2,"gradientAmount":0.4,"patternAmount":0.2}})",
        R"({"check":"look","appearance":)" + Look + "}",
        R"({"check":"profile","fields":{"description":"A grey wolf.","motto":"Onward"}})",
        R"({"check":"profile","fields":{"glances":"no"}})",
        R"({"check":"build","build":{"grades":{"strength":"strong","wisdom":"weak"},"specialty":"tracker"}})",
        R"({"check":"build","build":{"grades":{"strength":"mighty"}}})",
        R"({"check":"build","tier":"gifted","build":{}})",
        "not json",
        R"({"check":"nothing"})",
        R"({"check":"rules"})",
        // The painted look (doc 81): palette ids only.
        R"({"check":"look","appearance":)" + Look.substr(0, Look.size() - 1) + R"(,"build":"lean","fur":{"coat":["russet","tan","cream"],"eyes":"hazel","zones":{"legs":["silver","mist","snow"]}}}})",
        R"({"check":"look","appearance":)" + Look.substr(0, Look.size() - 1) + R"(,"fur":{"coat":["russet","tan","#39ff14"],"eyes":"hazel"}}})",
        R"({"check":"wolf","wolf":{"name":"Ash","age":7,"appearance":)" + Look.substr(0, Look.size() - 1) + R"(,"fur":{"coat":["russet","tan","neon"],"eyes":"hazel"}}}})",
    });
    expect(r[0].boolean("ok") && r[0].number("version") == 1, "version");
    const auto& wolf = r[1]["wolf"];
    expect(r[1].boolean("ok") && wolf.string("name") == "Ash tallow" && wolf.number("age") == 7 && wolf.string("sex") == "female" &&
               wolf.string("family") == "fire" && wolf["appearance"].string("species") == "timber",
           "a good wolf, canonical: " + json::dump(r[1]));
    expect(r[2].boolean("ok") && r[2]["wolf"].string("family").empty(), "no family is fine");
    for (int i : {3, 4, 5, 6})
        expect(!r[i].boolean("ok") && !r[i].string("error").empty(), "refused: " + std::to_string(i) + " " + json::dump(r[i]));
    expect(!r[7].boolean("ok"), "male or female only");
    expect(r[8].boolean("ok") && r[8].string("sex") == "female", "a look");
    expect(r[9].boolean("ok") && r[9]["profile"].string("description") == "A grey wolf." && r[9]["profile"].string("motto") == "Onward",
           "a profile: " + json::dump(r[9]));
    expect(!r[10].boolean("ok"), "a bad profile field");
    expect(r[11].boolean("ok") && r[11]["build"]["grades"].string("strength") == "strong" &&
               r[11]["build"].string("specialty") == "tracker",
           "a build: " + json::dump(r[11]));
    expect(!r[12].boolean("ok"), "an unknown grade");
    expect(!r[13].boolean("ok"), "a tier needs a family");
    expect(!r[14].boolean("ok") && !r[15].boolean("ok"), "not JSON, an unknown check");
    expect(r[17].boolean("ok") && r[17]["appearance"].string("build") == "lean" && r[17]["appearance"]["fur"].string("eyes") == "hazel" &&
               r[17]["appearance"]["fur"]["coat"].items().size() == 3 && r[17]["appearance"]["fur"]["zones"]["legs"].items().size() == 3,
           "a painted look, kept whole: " + json::dump(r[17]));
    expect(!r[18].boolean("ok") && !r[19].boolean("ok"), "a look or a wolf with fur off the palette is refused");
    // The creator's rules for central's GET /rules: the families, as a world's lobby sends them.
    const auto& families = r[16]["gifts"]["families"];
    expect(r[16].boolean("ok") && families.isArray() && families.size() == 8, "the rules: eight families " + json::dump(r[16]).substr(0, 200));
    bool fire = false;
    for (const auto& f : families.items())
        fire = fire || (f.string("id") == "fire" && f["gifted"].isObject() && f["quickened"].isObject());
    expect(fire, "a family with its gifted and quickened sides");
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        theRules();
        if (argc > 1)
            theCommand(argv[1]);
        std::cout << "identity_tests: " << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "identity_tests FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
