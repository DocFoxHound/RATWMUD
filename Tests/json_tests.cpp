// Portable JSON (RatwJsonDoc.h): exact round trips, strictness, and objects that keep their order.
#include "RatwJsonDoc.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace ratw::json;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
Value read(const std::string& text)
{
    Value v;
    std::string error;
    expect(parse(text, v, error), "parses: " + text + " (" + error + ")");
    return v;
}
bool refused(const std::string& text)
{
    Value v;
    std::string error;
    return !parse(text, v, error) && !error.empty();
}
} // namespace

int main()
{
    try
    {
        const std::string text = R"({"b":1,"a":[true,false,null,-2.5,1e3,"x\"\\\n\u00e9\ud83d\udc3a"],"c":{"d":{}}})";
        const auto v = read(text);
        expect(v.fields()[0].first == "b" && v.fields()[1].first == "a", "fields keep their order");
        expect(v["a"].items()[5].asString() == "x\"\\\n\xc3\xa9\xf0\x9f\x90\xba", "escapes and surrogate pairs become UTF-8");
        expect(v["a"].items()[4].asNumber() == 1000, "exponents");
        expect(read(dump(v)) == v, "what is written reads back the same");
        expect(dump(read("[0.1,123456789012,-0,1.7976931348623157e308,5e-324]")) == "[0.1,123456789012,0,1.7976931348623157e+308,5e-324]",
               "numbers in the shortest exact form: " + dump(read("[0.1,123456789012,-0,1.7976931348623157e308,5e-324]")));
        for (const char* bad : {"", "{", "[1,]", "{\"a\":1,}", "01", "1.", ".5", "+1", "\"\\x\"", "\"\x01\"", "\"\\ud800\"",
                                "nul", "[1] 2", "{'a':1}", "1e999"})
            expect(refused(bad), std::string("refused: ") + bad);
        std::string deep(300, '[');
        expect(refused(deep + std::string(300, ']')), "nesting is bounded");
        // Building: set replaces in place, add appends, copies don't share changes.
        auto o = Value::object();
        o.set("x", 1);
        o.set("y", "two");
        o.set("x", 3);
        expect(dump(o) == R"({"x":3,"y":"two"})", "set replaces in place: " + dump(o));
        auto copy = o;
        copy.set("z", Value::array());
        copy.find("z")->push(1);
        expect(!o.has("z") && copy["z"].size() == 1, "a copy changes alone");
        for (int i = 0; i < 100; ++i)
            o.add("k" + std::to_string(i), i);
        expect(o.number("k77") == 77 && o.size() == 102, "large objects find their fields");
        expect(o.erase("k5") && !o.has("k5") && o.number("k6") == 6, "and lose them");
        expect(read(R"({"a":1,"b":2})") == read(R"({"b":2,"a":1})"), "equality ignores field order");
        expect(read(R"({"a":1,"a":2})").number("a") == 2, "a repeated key keeps the last value");
        expect(o.string("missing", "fallback") == "fallback" && o["nothing"].isNull(), "missing fields read as fallbacks");
        // UTF-8 as Postgres takes it (doc 77, Phase 5: a reason cut inside a character stopped every save).
        expect(validUtf8("") && validUtf8("plain") && validUtf8("\xc3\xa9t\xc3\xa9 \xe2\x80\x94 \xf0\x9f\x90\xba \xf4\x8f\xbf\xbf"),
               "ASCII, two, three and four bytes, up to U+10FFFF");
        for (const char* bad : {"\xc3", "ab\xc3", "\xa9", "\xe2\x80", "\xc0\xaf", "\xc1\xbf", "\xe0\x80\xaf", "\xed\xa0\x80",
                                "\xf0\x80\x80\xaf", "\xf4\x90\x80\x80", "\xf5\x80\x80\x80", "\xff", "\xc3\x28"})
            expect(!validUtf8(bad), std::string("not UTF-8: ") + bad);
        const std::string mixed = "a\xc3\xa9\xe2\x80\x94\xf0\x9f\x90\xba\xc3";
        expect(utf8Length(mixed, 0) == 1 && utf8Length(mixed, 1) == 2 && utf8Length(mixed, 3) == 3 && utf8Length(mixed, 6) == 4 &&
                   utf8Length(mixed, 2) == 0 && utf8Length(mixed, 10) == 0 && utf8Length(mixed, 11) == 0,
               "each character's length, 0 inside one, at a cut one and past the end");
        // A cut by bytes falls before the character it would split (the saved text's limits).
        const std::string euros = "\xe2\x82\xac\xe2\x82\xac\xe2\x82\xac";
        expect(utf8Prefix(euros, 9) == euros && utf8Prefix(euros, 20) == euros && utf8Prefix(euros, 8) == euros.substr(0, 6) &&
                   utf8Prefix(euros, 7) == euros.substr(0, 6) && utf8Prefix(euros, 6) == euros.substr(0, 6) && utf8Prefix(euros, 2).empty() &&
                   utf8Prefix("plain", 3) == "pla" && utf8Prefix(mixed, 6) == "a\xc3\xa9\xe2\x80\x94" && utf8Prefix(mixed, 9) == "a\xc3\xa9\xe2\x80\x94",
               "a cut between characters, never in one");
        for (std::size_t most = 0; most <= mixed.size(); ++most)
            expect(validUtf8(utf8Prefix(mixed.substr(0, 10), most)), "every cut is UTF-8: " + std::to_string(most));
        // And whatever a string holds, its JSON is UTF-8: a byte that is no character is written as U+FFFD, so one broken
        // field can't have Postgres refuse a whole save.
        Value broken = Value::object();
        broken.set("cut", std::string("ok \xe2\x82"));
        broken.set("whole", euros + "\xf0\x9f\x90\xba");
        broken.set("stray", std::string("\xff\xc0\xaf\xed\xa0\x80"));
        const auto written = dump(broken);
        expect(validUtf8(written) && written == "{\"cut\":\"ok \xef\xbf\xbd\xef\xbf\xbd\",\"whole\":\"" + euros + "\xf0\x9f\x90\xba\"," +
                                                     "\"stray\":\""
                                                     "\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd\"}",
               "broken bytes repaired, characters kept: " + written);
        Value back;
        std::string problem;
        expect(parse(written, back, problem) && back.string("whole") == euros + "\xf0\x9f\x90\xba", "and it reads back");
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "JSON tests passed: " << checks << " checks.\n";
    return 0;
}
