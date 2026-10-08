#pragma once
// The document store (Docs/Design/55-letters-gifts-favours.md, 1): in-world writing (letters now; notices and pacts
// later) kept apart from every ledger, which carry only a document's id. Pure: Game keeps the store and does the
// delivering (RatwGameLetters.cpp). Times are calendar days (World::calendarDays).
#include "RatwJsonDoc.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ratw::documents
{
struct Rules
{
    int textMost = 800, perDay = 10, unreadMost = 100;
    int feeTown = 1, feeBetween = 2, feePerCells = 10, sendOnFee = 1;
    double hoursTown = 1, cellsPerHour = 3, mostHours = 12;
    double scentDays = 14, sightFamiliarity = 20;
};
const Rules& rules();
Rules parse(const std::string& text);

// Courier hours and fee between two towns `cells` road cells apart (0 cells: the same town).
double courierHours(int cells, bool sameTown, const Rules& r = rules());
int courierFee(int cells, bool sameTown, const Rules& r = rules());

// A document. `author` is always kept (for the Dungeon Master and reports); `scent` is the author, or "" when masked.
// A letter goes to `to`'s post town (`postTown`) and is delivered there at `deliverAt`: "travelling" until then,
// "waiting" at that town's inns if its reader wasn't there, "delivered" into the letter case, "read" once opened.
// `viaCourier`: an answer to an anonymous letter, which its writer never learns the author of.
struct Document
{
    std::string id, kind = "letter", author, scent, to, text, sign, fromTown, postTown, state = "travelling", replyTo;
    double written = 0, deliverAt = 0, readAt = -1;
    bool kept = false, viaCourier = false, answered = false;
    // An enclosure (doc 55, 3): one kind of small thing and/or coins, held in the escrow account `escrow`
    // ("letter:<id>@<writer>") from writing to taking; `scented` if it carries the writer's scent.
    std::string escrow, encItem;
    int encQuantity = 0;
    std::int64_t encCoins = 0;
    bool scented = false;
    // A resident's letter (doc 55, 5): what it thanks for (facts, for the resident's briefing), and the work it offers.
    std::string facts, contract;
    std::string occasion;                          // An invitation's occasion (doc 55, 6).
    int answer = 0;                                // Its answer: 1 coming, -1 can't come, 0 not yet.
    // A pact (doc 55, 8): its id (shared by every copy), the other party, and who has sealed it (parties and witnesses).
    std::string pact, party;
    std::vector<std::string> seals, witnesses;
};
json::Value save(const Document& d);
Document load(const json::Value& v);

// The store: by id, with each reader's documents and the courier's queue (by when they arrive).
class Store
{
  public:
    Document& add(Document d);
    Document* find(const std::string& id);
    const Document* find(const std::string& id) const;
    void erase(const std::string& id);
    void clear();
    // A reader's documents, unread first, then newest first.
    std::vector<const Document*> forReader(const std::string& reader) const;
    int unread(const std::string& reader) const;
    int writtenSince(const std::string& author, double day) const;
    std::vector<const Document*> fromAuthor(const std::string& author) const;
    // Those due by `now` (still travelling), in order of arrival.
    std::vector<std::string> due(double now) const;
    void rescheduled(const std::string& id, double was);    // (After deliverAt changed.)
    const std::map<std::string, Document>& all() const { return docs_; }
    std::uint64_t next = 1;

  private:
    std::map<std::string, Document> docs_;
    std::multimap<std::string, std::string> byReader_, byAuthor_;
    std::set<std::pair<double, std::string>> queue_;
};
} // namespace ratw::documents
