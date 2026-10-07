#pragma once
// People (Docs/Design/50-player-card-friends-safety.md): the account as a person (a public handle, experience, played
// time, settings) and each character's roleplay profile on the player card, with who may see which part. Pure: the
// game (RatwGamePeople.cpp) keeps the records, works out what a viewer can perceive, and sends the card. The rules and
// limits are in Data/Social/profile.json.
#include "RatwJsonDoc.h"

#include <map>
#include <string>
#include <vector>

namespace ratw::people
{
// A glance (Total RP 3's "at first glance"): an icon, a title and a line, perceived by one sense.
struct Glance
{
    std::string icon, title, line, sense = "sight";
};

// One character's profile (doc 50, 2). Status and walk-up are its too.
struct Profile
{
    std::string description, currently, pronouns, title, motto;   // What others may see (title and motto: by name).
    std::vector<Glance> glances;
    std::string oocNotes, history, otherLimits;                     // The OOC tab: players only.
    std::map<std::string, int> sliders;                             // "Chaotic/Lawful" -> -10..10 (absent: off).
    std::map<std::string, std::string> consent;                     // "injury" -> "yes", "no" or "ask".
    bool mature = false;
    std::string status = "ic";                                      // "ic", "ooc", "lfs", "storyteller".
    bool walkup = false;
    int revision = 0;                                               // Up by one at each change (the "unread" mark).
};

// The player's own settings, on the account.
struct Settings
{
    bool showMature = false, recaps = true, toasts = true;
};

// An account as a person (doc 50, 1): beside the sign-in record, never holding a verifier.
struct AccountRecord
{
    std::string handle, formerHandle;
    double handleChangedAt = -1;
    std::string experience;                                         // "" until set: profile.json's default.
    double playedSeconds = 0;
    std::string firstCharacter;
    Settings settings;
    double silencedUntil = -1;                                      // A DM's silence (doc 50, 7): real seconds.
};

struct Rules
{
    std::map<std::string, int> limits;
    std::vector<std::string> icons;
    std::map<std::string, double> senseReach;                       // "scent" -> 3 tiles, "sound" -> 15.
    std::vector<std::string> sliders;                               // "Chaotic/Lawful"...
    std::vector<std::string> consent;                               // "injury", "death"...
    std::map<std::string, std::string> statusMarks;                 // "ooc" -> "ooc", "lfs" -> "lfs", "storyteller" -> "quill".
    std::vector<std::string> statuses, approvedOnly, experience;
    std::string defaultExperience = "casual";
    int handleMin = 3, handleMax = 24, handleChangeDays = 30;
    double playedEvery = 60, activeWithin = 300;
    json::Value catalog = json::Value::object();                    // The file as the client is sent it.
};
const Rules& rules();
int limit(const std::string& field);

// Text as a profile keeps it: control characters removed (newlines kept where `multiline`), trimmed, cut to `most`
// characters (never inside a character).
std::string clean(const std::string& text, std::size_t most, bool multiline = false);

// Applies a `profile` command's fields to a profile: each checked and cleaned; unknown fields, icons, senses, sliders
// or consent answers refused. Bumps the revision when anything changed. False, with the reason.
bool applyFields(Profile& p, const json::Value& fields, std::string& error);
// A status a character may take ("storyteller" only when `approvedStoryteller`: doc 58). False, with the reason.
bool validStatus(const std::string& status, bool approvedStoryteller, std::string& error);
bool validExperience(const std::string& experience);
// A handle (doc 50, 1): 3-24 letters, digits, spaces, "_" or "-"; never the sign-in username. Uniqueness is the game's.
bool validHandle(const std::string& handle, const std::string& username, std::string& error);
std::string handleKey(const std::string& handle);                   // Lower case, for uniqueness.

// What a viewer can perceive of the profile's owner (worked out by the game).
struct Viewer
{
    bool self = false;          // Its own profile: everything.
    bool player = true;         // A resident (false) never sees the OOC tab.
    bool knowsName = false;     // Title and motto.
    bool sees = true, smells = false, hears = true;   // Glances by sense.
    bool showMature = true;     // Off: a mature profile's description, history and glances are folded away.
};
// The profile as the card shows it to this viewer: only the parts it may see.
json::Value cardFor(const Profile& p, const Viewer& v);
// What a resident is told of a wolf it can see (doc 50, 2): pronouns, Currently, the glances it perceives and the
// start of the description, marked as the player's own words. Never the OOC tab, title or motto.
std::string residentContext(const Profile& p, const Viewer& v);
// The owner's whole profile, and back (saves and the owner's own view).
json::Value save(const Profile& p);
Profile load(const json::Value& o);
json::Value saveAccount(const AccountRecord& a);
AccountRecord loadAccount(const json::Value& o);
} // namespace ratw::people
