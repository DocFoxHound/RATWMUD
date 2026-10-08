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
    std::string description, currently, title, motto;   // What others may see (title and motto: by name).
    // From the character's sex, she/her or he/him (the user: characters are male or female, and nothing else is
    // offered): filled in by the game for a card or a resident's context, never set by the player or saved.
    std::string pronouns;
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
    bool messages = true;                                           // Private messages from friends (doc 50, 4).
    bool matchmaking = true;                                        // Residents may point others to me, and me to others (doc 52, 5).
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
    bool graduated = false;                                         // No longer a newcomer, for good (doc 52, 2).
    // Mentoring (doc 52, 3): opted in, available (else busy), turned off by a Dungeon Master until restored; resting
    // after a tie until (real seconds), the tie it is on, ties that ended with a shared scene, the last tie's time.
    struct Mentor
    {
        bool on = false, available = true, revoked = false;
        double restingUntil = -1, lastTieAt = -1;
        std::string tie;
        int guided = 0;
    } mentor;
};

// Friends (doc 50, 4): mutual, by account. Each side keeps its own link: when it began, and whether this side shows the
// other which character it is playing.
struct FriendLink
{
    double since = 0;
    bool shares = true;
};
// A request waiting for an answer: from one account to another, and the wolf it was made from in person (a card), if any.
struct FriendRequest
{
    std::string from, to, character;
    double at = 0;
};
// A private message kept for an offline friend until they next come (doc 50, 4: 50 a recipient, 14 days).
struct PrivateMessage
{
    std::string id, to, from, fromCharacter, text;
    double at = 0;
};

// One wolf a character has met (doc 50, 5): when first and last (real seconds, and the game's calendar day), where
// last, scenes shared, this character's tag and private note, a tie's story starter (doc 52), and the profile revision
// it last read.
struct KnownWolf
{
    double firstMet = 0, lastMet = 0, lastMetDay = -1;
    std::string lastPlace, label;     // Where last; and how this character knew them then (shown while they're away).
    int scenes = 0;
    std::string tag, note, tie;
    bool customTag = false;          // `tag` is one the player named, not one of the rules' own.
    int readRevision = 0;
    bool resident = false;
};
// A scene recapped for one character (doc 50, 5): what it perceived, by the model, or written from the ledger.
struct Recap
{
    std::string id, session, place, text;
    double at = 0;
    int minutes = 0;
    std::vector<std::string> others;   // The players it was with.
    bool model = false;
};

// A circle (doc 50, 6): an out-of-character group of accounts, with a keeper, officers, members (each choosing whether
// the circle sees which wolf they're playing), invitations waiting, and planned nights.
struct CircleMember
{
    std::string role = "member";      // "keeper", "officer" or "member".
    bool shares = false;
    double joined = 0;
};
struct CircleNight
{
    std::string id, place, line, by;
    double at = 0;                    // Real seconds (each viewer sees it in their own time).
};
struct Circle
{
    std::string id, name;
    double created = 0;
    std::map<std::string, CircleMember> members;          // By account.
    std::map<std::string, std::pair<std::string, double>> invited;   // Account -> (who invited them, when).
    std::vector<CircleNight> nights;
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
    int friendsMost = 200, requestsWaiting = 20, requestDays = 14;
    bool shareByDefault = true;
    int messageMost = 2000, inboxMost = 50, inboxDays = 14;
    int knownPlayers = 300, knownResidents = 100, noteMost = 500, customTagMost = 24;
    double metEvery = 600;
    std::vector<std::string> tags;
    int recapsPerWolf = 3, recapsPerCharacter = 150, bufferLines = 120, bufferCharacters = 6000, modelLines = 6,
        modelADay = 10, recapMost = 7000;
    double modelMinutes = 5;
    int circleName = 32, circlesPerAccount = 10, circleMembers = 50, circleNights = 10, nightPlace = 80, nightLine = 160,
        circleInviteDays = 14;
    bool circleShareByDefault = false;
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
json::Value saveMessage(const PrivateMessage& m);
PrivateMessage loadMessage(const json::Value& o);
json::Value saveKnown(const KnownWolf& k);
KnownWolf loadKnown(const json::Value& o);
json::Value saveRecap(const Recap& r);
Recap loadRecap(const json::Value& o);
json::Value saveCircle(const Circle& c);
Circle loadCircle(const json::Value& o);
// A circle's name (doc 50, 6): 1-32 letters, cleaned. Uniqueness is the game's. False, with the reason.
bool validCircleName(const std::string& name, std::string& cleaned, std::string& error);

// A tag (Total RP 3's): "" for none, one of the rules' own, or (custom) one the player names. The cleaned tag, or false.
bool validTag(const std::string& tag, bool custom, std::string& cleaned);
// One character's list past its caps (300 players, 100 residents): the oldest met without a note, tag or recap go
// first, then the oldest. Returns the ids dropped.
std::vector<std::string> trimKnown(std::map<std::string, KnownWolf>& list, const std::vector<Recap>& recaps);
// One character's recaps: those no known wolf would show (each shows its newest 3) go, then the oldest past 150.
void trimRecaps(std::vector<Recap>& recaps);
} // namespace ratw::people
