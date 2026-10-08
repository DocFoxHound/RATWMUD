#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ratw
{
// What one character feels about another (Docs/Design/26-living-npcs.md, Phase 3). One-sided: Ash may trust Wren
// more than Wren trusts Ash. Moved only by rules on validated events (trading, talking, time spent together, harm,
// help) and by small, clamped nudges a conversation proposes; never by what anyone merely claims.
struct Bond
{
    double affinity = 0;    // Liking, -100..100.
    double trust = 0;       // -100..100.
    double familiarity = 0; // How well known, 0..100.
    double fear = 0;        // 0..100.
    double respect = 0;     // -100..100.
    std::int64_t owed = 0;  // Pennies the other owes this one (negative: this one owes them).
    double lastContact = 0; // Calendar day of the last contact.
};

struct BondChange
{
    double affinity = 0, trust = 0, familiarity = 0, fear = 0, respect = 0;
};

// A bond as saved: who holds it, about whom.
struct SavedBond
{
    std::string holder, other;
    Bond bond;
};

class Bonds
{
  public:
    // The most others one character keeps: about a hundred and fifty acquaintances. Past that the faintest go.
    static constexpr std::size_t MaxPerHolder = 150;

    const Bond* find(const std::string& holder, const std::string& other) const;
    const std::map<std::string, Bond>* of(const std::string& holder) const;
    // Moves holder's feelings about other. Growth slows as a feeling nears its limit (the tenth kind word matters
    // less than the first); every value stays within its range. Creates the bond if need be.
    void change(const std::string& holder, const std::string& other, const BondChange& by, double day);
    // The same change both ways.
    void mutual(const std::string& a, const std::string& b, const BondChange& by, double day);
    void addOwed(const std::string& holder, const std::string& other, std::int64_t pennies, double day);
    // A day's fading: acquaintance fades without contact, strong feelings cool, fear passes. Trust, respect and
    // debts stay. A bond with nothing left in it is forgotten.
    void fade(double day);
    // Characters away (logged out): nobody's regard for them fades while they are gone (doc 56, 10; Principle 7).
    void setAway(const std::string& id, bool away) { if (away) away_.insert(id); else away_.erase(id); }
    bool away(const std::string& id) const { return away_.count(id) > 0; }
    // Someone who no longer exists (a resident who left the world).
    void forget(const std::string& id);
    // A few words for a conversation: how holder regards other, e.g. "You know Ash well, like them, and trust them a
    // little. They owe you 3 pennies." Empty if they are strangers.
    std::string describe(const std::string& holder, const std::string& other, const std::string& otherName) const;
    std::size_t count() const;

    std::vector<SavedBond> save() const;
    // Refuses (and changes nothing) if any saved value is out of range.
    bool restore(const std::vector<SavedBond>& saved);

  private:
    std::set<std::string> away_;
    std::map<std::string, std::map<std::string, Bond>> bonds_;
    void trim(std::map<std::string, Bond>& mine, const std::string& keep);
};
} // namespace ratw
