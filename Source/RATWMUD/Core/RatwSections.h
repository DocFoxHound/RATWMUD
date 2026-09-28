#pragma once
// Delta snapshots, portable: the protocol of Source/RATWMUD/Runtime/RatwSnapshotSections.h (see there), on the
// portable JSON, for a standalone server (strip) and for tests of a client (fill). Keys are the server's own: a client
// never works one out, so the two servers' JSON may be written differently and clients of either still agree.
#include "RatwJsonDoc.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ratw::sections
{
using Keys = std::map<std::string, std::string>;

// The server's side: gives every part its key (in "sectionKeys"), leaves out those the client holds (`known`, as of
// the snapshot it last acknowledged), and sends each map entry it holds as {"$held": key}. Returns this snapshot's keys.
Keys strip(json::Value& root, const Keys& known);

// The server's record of one client: what it is known to hold, and what each snapshot not yet acknowledged carried.
struct Held
{
    Keys known;
    std::map<double, Keys> sent;
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
