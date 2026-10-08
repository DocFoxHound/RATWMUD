#pragma once
// Story books (Docs/Design/51-scenes-and-stars.md, Phase 7): a player-driven Story is a book, an ordered list of scenes
// (its chapters) with a title, a summary, its wolves, who it is shared with, and whether it is finished. Books are the
// one kind of Story (the user, 2026-10-07): an official book is one its wolves agreed to, and it pays as doc 32's
// Stories do (the game drives SocialLedger's Story for it). Pure: the game keeps the books, decides who may read them
// and fills chapters from scenes (RatwGameBooks.cpp). The numbers are in Data/Social/social.json's "books".
#include "RatwJsonDoc.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace ratw::books
{
struct Rules
{
    int title = 80, chapterTitle = 60, summary = 7000, bookSummary = 1200, flavour = 300, linkDays = 7, activeDays = 30,
        quietDays = 3, modelADay = 10, openBooks = 20, chapters = 200, wolves = 100;
};
const Rules& rules();

// A chapter: one scene, with what the book needs of it after the scene itself is let go (eight days on).
struct Chapter
{
    std::string id, session, title, summary, summaryBy;   // summaryBy: a character, or "" for the linker's own words unset.
    bool model = false;                                  // The summary is the model's (from the summariser's recap).
    bool privateScene = false;                           // Its scene was Private: its summary is its wolves' until the book is finished.
    std::string place, linkedBy;
    double at = 0, ended = 0;
    std::vector<std::string> wolves;                     // The characters in the scene.
};

// A book linked to another, once both are finished: "related", "sequel" or "prequel" (the user, 2026-10-07).
struct Link
{
    std::string book, kind;
};

struct Book
{
    std::string id, title, keeper, summary, summaryBy, flavour;
    bool summaryModel = false;
    // Who may read it: "members", "friends", "circle" (`circle`), "chapter" (`chapter`) or "everyone"; and a DM's
    // world storyline it is tied to (the World shelf).
    std::string sharing = "members", circle, chapter, storyline;
    std::vector<std::string> wolves;                     // Its characters, in the order they came in.
    std::set<std::string> hidden;                        // Wolves who hid themselves from its readers.
    std::vector<Chapter> chapters;                       // In the order of the story.
    std::string state = "open";                          // "open", "finishing", "finished".
    double created = 0, last = 0, finishProposed = 0, finished = 0;
    std::set<std::string> agreed, objected;              // To finishing.
    std::string story;                                   // Official: its SocialLedger Story's id.
    std::vector<Link> links;
    std::set<std::string> nextScene;                     // Wolves whose next scene goes into it ("before").
};

bool hasWolf(const Book& b, const std::string& character);
bool hasPrivate(const Book& b);
// The wolves who were in a chapter that ended within the last `days` days, not hidden: those whose word finishes it.
std::vector<std::string> recentlyActive(const Book& b, double now);
// Finishing (agreed, the user, 2026-10-07): a majority of the recently active agree, or `quietDays` pass after it was
// proposed with no objection.
bool finishes(const Book& b, double now);
// The link's other side: a sequel's other is its prequel, and the other way round.
std::string inverse(const std::string& kind);
bool validLink(const std::string& kind);
bool validSharing(const std::string& sharing);

json::Value save(const Book& b);
Book load(const json::Value& o);
} // namespace ratw::books
