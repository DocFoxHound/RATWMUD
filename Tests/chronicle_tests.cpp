// The chronicle (Docs/Design/56-fame-and-memory.md, 8; Phase 5), pure, on rows: the first arrival, told once; meetings
// by the names the owner knows (a wolf never introduced, by look); deeds, nicknames and Stories; the season's round
// folded into one line; nothing names anyone the owner wasn't told; dated by the game's calendar.
#include "RatwChronicle.h"

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

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
bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

void lines()
{
    // What the owner (Ash) knows: Wren by name; the miller only by look; Bo's true name, never told, is "Bo".
    const std::map<std::string, std::string> known{{"wren", "Wren"}, {"miller", "the miller"}, {"bo", "a dun wolf"}};
    chronicle::Lens lens;
    lens.name = [&](const std::string& id) { return known.count(id) ? known.at(id) : std::string("someone"); };
    lens.place = [](const std::string& cell) { return cell == "c1" ? std::string("the Market") : cell; };
    std::vector<chronicle::Row> rows = {
        {0.2, "character created", "ash"},
        {0.3, "arrival", "ash", {}, "c1"},
        {0.4, "arrival", "ash", {}, "c1"},
        {1.0, "introduced", "ash", "wren", "c1", {}, "Ash"},
        {1.1, "introduced", "ash", "wren", "c1", {}, "Ash"},
        {1.2, "introduced", "bo", "ash", "c1", {}, "Bo"},
        {2.0, "deed", "ash", "town:upper_accord", "c1", "broke_camp", "drove the bandits off the road at the ford (deed-1)"},
        {2.5, "nickname", "ash", "wren", "c1", {}, "the Lantern (nick-1)"},
        {3.0, "story told", "ash", {}, {}, {}, "The Long Night"},
        {4.0, "conversation", "ash", "wren"},
        {4.1, "conversation", "miller", "ash"},
        {4.2, "conversation", "ash", "wren"},
        {5.0, "hunted", "ash", "hare"},
        {5.5, "arrival", "ash", {}, "c2"},
        {6.0, "festival won", "ash", {}, "c1", "race", "the Greening"},
        {7.0, "theft", "fennel", "wren"},           // (Not theirs: left out.)
    };
    const auto out = chronicle::compile("ash", rows, lens);
    std::string all;
    for (const auto& e : out)
        all += e.text + "\n";
    expect(contains(all, "You came into the world.") && contains(all, "You first set foot in the Market."), "the first arrival: " + all);
    std::size_t at = 0;
    int arrivals = 0;
    while ((at = all.find("first set foot", at)) != std::string::npos)
        ++arrivals, ++at;
    expect(arrivals == 1, "told once");
    expect(contains(all, "You told Wren your name.") && all.find("You told Wren your name.") == all.rfind("You told Wren your name."),
           "a meeting, once, by the name she knows");
    expect(contains(all, "A dun wolf told you their name.") && !contains(all, "Bo told"), "a wolf whose name she wasn't given, by look");
    expect(contains(all, "You drove the bandits off the road at the ford.") && !contains(all, "deed-1"), "the deed, without its id");
    expect(contains(all, "Wren first called you the Lantern."), "the nickname, and who first said it");
    expect(contains(all, "\"The Long Night\" was told, with you in it."), "a Story told");
    expect(contains(all, "You won the race at the Greening."), "a festival won");
    expect(contains(all, "talked with 2 wolves") && contains(all, "brought down 1 beast") && contains(all, "came to 1 new place"),
           "the season's round, folded: " + all);
    expect(!contains(all, "fennel") && !contains(all, "stole"), "nothing that isn't theirs");
    expect(contains(out.front().text, "Spring 1, Year 1:"), "dated by the game's calendar: " + out.front().text);
    expect(chronicle::compile("nobody", rows, lens).empty(), "someone with no rows has no chronicle yet");
}
} // namespace

int main()
{
    try
    {
        lines();
    }
    catch (const std::exception& error)
    {
        std::cerr << "chronicle_tests failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "chronicle_tests passed (" << checks << " checks)\n";
    return 0;
}
