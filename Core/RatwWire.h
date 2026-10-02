#pragma once
// The game's JSON shapes, portable (Docs/Design/26-living-npcs.md, Phase 6): what a snapshot says of a character, a
// map cell, the weather and the calendar; how a save writes characters and the society, and reads them back strictly.
// The browser client reads these (Client/src/game/state.ts); Tests/wire_tests.cpp checks what they may and may not
// carry.
#include "RatwJsonDoc.h"
#include "RatwWorld.h"

namespace ratw::wire
{
using json::Value;

// A number field that must be a (finite) number, else `fallback` (a missing or mistyped field).
double strictNumber(const Value& o, const std::string& key, double fallback);
// A number field if it is one and finite, else `fallback`.
double number(const Value& o, const std::string& key, double fallback = 0);

Value appearance(const Appearance& a);
// A provided appearance is complete and strict: no coercion, unknown keys, partial presets or clamping.
bool readAppearance(const Value& o, Appearance& out);
Appearance readAppearance(const Value& o);          // Invalid: an empty species, which rejects the whole save.

// What anyone may see of a character (for the observer's own view, sight-sanitized actors, and saves).
Value entity(const Entity& e, double time);
// A character as a save keeps it: everything entity() says, and the body's numbers.
Value persistEntity(const Entity& e, double time);
Entity readEntity(const Value& o);
// The self view's pace and body numbers (added to the observer's own entity).
void privatePace(Value& o, const Entity& e);

Value mapCell(const MapCell& m, bool includeGlyphs = true);
Value travel(const TravelState& t);
Value wind(const Wind& w);
Wind readWind(const Value& o);
Value environment(const Environment& e);
Value lighting(const Lighting& l);
Lighting readLighting(const Value& o);
std::string environmentDescription(const Cell& cell, const Environment& e);
double readClockOffset(const Value& o);
// A tile height (half steps, -16..16) as one printable character: '0' is -16, 'P' is level ground, 'p' is +16.
char heightChar(double height);
double heightFromChar(char c);
Weather readWeather(const Value& v);
Value senses(const Snapshot& snapshot);
Result paceCommand(World& world, const std::string& id, const Value& o);
Result environmentCommand(World& world, const std::string& cellId, const std::string& type, const std::string& value,
                          bool devTools);

Value society(const SocietyState& s);
// Parts of society(), as it writes them: for the journal (RatwJournal.h).
Value economyAccount(const EconomyAccount& a);
Value economyLedger(const std::vector<EconomyEntry>& entries);
Value careerPosition(const PositionState& p);
CareerState readCareers(const Value& o);
// A malformed subtree invalidates the complete checkpoint (minted -1). Never silently refill purses.
SocietyState readSociety(const Value& o);
} // namespace ratw::wire
