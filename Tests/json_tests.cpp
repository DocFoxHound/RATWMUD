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
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "JSON tests passed: " << checks << " checks.\n";
    return 0;
}
