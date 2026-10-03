#pragma once
// Delta snapshots (Docs/Design/26-living-npcs.md, Phase 6): the server leaves out the parts of a snapshot a client is
// known to hold (strip), and a client puts its copies back (fill; the browser client's is Client/src/net/sections.ts).
// Keys are the server's own: a client never works one out.
#include "RatwJsonDoc.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ratw::sections
{
using Keys = std::map<std::string, std::string>;

// Recent values of the parts that can be sent as changes (what the wolf can see, the cell's ground and heights), by
// key: the bases for those changes.
struct Bases
{
    std::map<std::string, json::Value> values;
    std::vector<std::string> order;
    // Each part's last value and key: a part sent again as the very same value (its storage: see json::Value::storage)
    // is not hashed again (doc 31, Phase 4). The value is kept, so its storage can't be reused by another.
    std::map<std::string, std::pair<json::Value, std::string>> keys;
    void keep(const std::string& key, const json::Value& value);
    const json::Value* find(const std::string& key) const;
};

// The server's side: gives every part its key (in "sectionKeys"), leaves out those the client holds (`known`, as of
// the snapshot it last acknowledged), and sends each map entry it holds as {"$held": key}. With `bases`, a changed
// visibility, ground or heights is sent as row edits against the one the client holds, {"$delta": key, "edits": [[row, column, text]]},
// when that is much smaller. Returns this snapshot's keys.
Keys strip(json::Value& root, const Keys& known, Bases* bases = nullptr);

// Row edits turning `base` (an array of strings) into `next`, or false where a delta can't or shouldn't be used.
bool rowDelta(const json::Value& base, const json::Value& next, json::Value& edits);
// `base` with `edits` applied; false if the edits don't fit it.
bool applyRowDelta(const json::Value& base, const json::Value& edits, json::Value& out);

// The server's record of one client: what it is known to hold, and what each snapshot not yet acknowledged carried.
struct Held
{
    Keys known;
    std::map<double, Keys> sent;
    Bases bases;
    void reset() { known.clear(); sent.clear(); }
    // A snapshot of this revision is going out with these keys (a second at the same revision keeps the first's).
    void sending(double revision, Keys keys);
    // The client applied that revision (or, missing, lacks a part it was expected to hold: everything again).
    void acknowledged(double revision, bool missing);
};

// The client's side.
struct Cache
{
    std::map<std::string, std::vector<std::pair<std::string, json::Value>>> kept;
    std::map<std::string, json::Value> entries;
    std::vector<std::string> entryOrder;
};
bool fill(json::Value& root, Cache& cache);
} // namespace ratw::sections
