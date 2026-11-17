#pragma once
// The talk ledger (Docs/Design/84 §8.7, Phase 5): what residents have talked of, with whom, and how it went; owned by the
// World (so a harness world has it) and saved in the checkpoint. It is what makes "as you told me yesterday" true:
//
//   pair notes   what two talked of (topics, and what about: the director's keys), how it went, the claims passed, the
//                tags kept ({k=v keep 7d}: pair.was.k), an open thread, who began it; or, written by World::gossip once a
//                day for talk out of anyone's hearing, only that they talked of a claim (no words). 8 heard a pair, 30
//                days; and the last 2 days' out of hearing beside them, never in their place (Slice 2's review: a daily
//                note out of hearing pushed every heard one out of a close pair's 8 within 8 days).
//   told         who told a listener which claim (or their own news), and when: a claim is told to a listener once
//                (the scorecard's K3), a second source corroborates once (World::told), other.told_me. 256 a listener
//                (64 until Slice 2's review: a tavern regular overhears that many claims in a week, and was told one again).
//   person facts what talk has said of a third party (remember x: took_up=fiddle 30d), by whom, until when: x.said.k.
//                16 a subject.
//   heard        the distinctive entries (stories, teases) a resident has heard: told once per listener. 64 a resident.
//   kept         a speaker's own kept tags (self.was.k), as long as they keep.
//   places       a place's last talks (topic, news or not) and today's count of each thing talked of there: the
//                director's everyday floor and saturation (§8.5).
//   invitations  an invitation the world checks (invite <place> <hour> <days>): kept or missed (pair.kept_invite).
//   promises     a talk-only promise (promise self: <thing> <days>), never a world promise; lapsed, it is a callback.
//   drift        (Phase 6, §7.6) the steps events moved a resident's stance on an issue (a theft never solved: the Watch
//                a step down), and the events applied, once each: self.op.<issue> stands on it. For good.
//   moods        (Phase 6, §8.8) a resident's recent events as talk saw them (a quarrel, praise, a warm talk): self.mood.
//                8 a resident, 7 days.
//
// At most 30,000 records in all (places aside): past it, the notes out of hearing go first, the oldest first, then whole
// record sets, the least recently used first (prune, each game day). A set is used when talk in someone's hearing writes
// or reads it; gossip out of hearing never makes one look used (Slice 2's review: World::gossip's daily writes made every
// set heard talk had touched the least recently used). Drift and moods of the dead go with them.
#include "RatwJsonDoc.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace ratw
{
struct TalkNote
{
    double day = 0;
    std::string opener;                                      // Who began it.
    std::vector<std::string> topics, keys;                   // Talked of, and what about, in order.
    std::string outcome;                                     // warmed, soured, reassured, conceded, quarrelled, broken, curt, even
    std::string souredBy;                                    // Soured or quarrelled: who made the sharp move (self.soured)
    std::vector<std::string> told;                           // Entries told (stories, teases: once per listener).
    std::vector<std::string> claims;                         // "teller|key": claims passed, each way.
    std::map<std::string, std::pair<std::string, double>> kept;   // {k=v keep Nd}: k -> (v, until).
    std::string thread;                                      // A topic left before its point was made ("" none)...
    double threadUntil = 0;                                  // ...kept until.
    bool unheard = false;                                    // Out of anyone's hearing (World::gossip): no words.
};
struct TalkTold
{
    std::string teller, key, kind;                           // kind: the claim kind passed (other.told_me).
    double day = 0;
    bool counted = false;                                    // Counted as a second source (World::told's corroboration).
};
struct TalkSaid
{
    std::string key, value, by;
    double until = 0;
};
struct TalkInvite
{
    std::string by, to, cell, place;                         // place: in words ("the Glass Lantern").
    int hour = 0;
    double from = 0, until = 0;                              // The days it stands for.
    std::string state = "open";                              // open, kept, missed
};
struct TalkPromise
{
    std::string by, to, thing;
    double day = 0, until = 0;
    std::string state = "open";                              // open, lapsed, called (brought up since)
};
struct TalkDrift
{
    int steps = 0;                                           // For (+) or against (-), at most 2 either way
    std::string event;                                       // The latest event that moved it (its reason's key)
    double day = 0;
};

class TalkLedger
{
  public:
    static constexpr std::size_t NotesPerPair = 8, UnheardPerPair = 2, ToldPerResident = 256, SaidPerSubject = 16,
                                 HeardPerResident = 64, PlaceLast = 16, Records = 30000, AppliedPerResident = 32,
                                 MoodsPerResident = 8;
    static constexpr double MoodDays = 7;
    static constexpr double NoteDays = 30;
    // Off in fast economy sims (--no-talk-ledger): nothing is written, so nothing is read.
    bool enabled = true;

    static std::string pairKey(const std::string& a, const std::string& b);

    // Pair notes. `note` begins a new one (the oldest heard past 8 dropped; `unheard`, World::gossip's: the oldest of
    // those past 2).
    TalkNote& note(const std::string& a, const std::string& b, double day, bool unheard = false);
    const std::deque<TalkNote>* notes(const std::string& a, const std::string& b) const;
    TalkNote* latest(const std::string& a, const std::string& b, bool heardOnly = false);
    const TalkNote* latest(const std::string& a, const std::string& b, bool heardOnly = false) const;
    // The latest note in which the two talked of `topic` (the newest first), or null.
    const TalkNote* recall(const std::string& a, const std::string& b, const std::string& topic) const;
    // Whether the two talked of `topic` and the same `key` since `since` (the director's novelty).
    bool talkedOf(const std::string& a, const std::string& b, const std::string& topic, const std::string& key, double since) const;
    // An open thread the two left, unexpired at `day` ("" none).
    std::string thread(const std::string& a, const std::string& b, double day) const;
    void closeThread(const std::string& a, const std::string& b);

    // The told set.
    // (`heard` false: told out of anyone's hearing, World::gossip: the set isn't made to look used.)
    void told(const std::string& listener, const std::string& teller, const std::string& key, const std::string& kind, double day,
              bool counted = false, bool heard = true);
    bool wasTold(const std::string& listener, const std::string& key) const;
    bool toldBy(const std::string& listener, const std::string& teller, const std::string& key) const;
    // Whether `teller` has been counted as a source of the claim for `listener` (corroboration: once a source).
    bool counted(const std::string& listener, const std::string& teller, const std::string& key) const;
    // The claim kind `teller` last passed `listener` ("" none).
    std::string toldMe(const std::string& listener, const std::string& teller) const;

    // Person facts: what talk has said of a third party.
    void said(const std::string& subject, const std::string& key, const std::string& value, const std::string& by, double until);
    std::map<std::string, std::string> saidOf(const std::string& subject, double day) const;

    // Distinctive entries heard.
    void heard(const std::string& resident, const std::string& entry);
    bool hasHeard(const std::string& resident, const std::string& entry) const;

    // A speaker's own kept tags (self.was.k).
    void keep(const std::string& resident, const std::string& key, const std::string& value, double until);
    std::map<std::string, std::string> kept(const std::string& resident, double day) const;
    // The pair's kept tags, newest first (pair.was.k).
    std::map<std::string, std::string> pairKept(const std::string& a, const std::string& b, double day) const;

    // Drift (Phase 6): an event moves a stance a step (never past 2 either way), once for each event id.
    void drift(const std::string& resident, const std::string& issue, int steps, const std::string& event,
               const std::string& eventId, double day);
    bool applied(const std::string& resident, const std::string& eventId) const;
    // issue -> (steps, the event that last moved it).
    std::map<std::string, std::pair<int, std::string>> drifts(const std::string& resident) const;
    // Moods (Phase 6): what happened to a resident in talk lately (quarrel, praised, warmed...), newest last.
    void moodEvent(const std::string& resident, const std::string& kind, double day);
    std::vector<std::pair<double, std::string>> moodEvents(const std::string& resident, double since) const;

    // Places.
    void placeTalk(const std::string& cell, const std::string& topic, const std::string& key, bool news, double day);
    int todayCount(const std::string& cell, const std::string& key, double day) const;
    // The news share of a place's last `window` talks (-1 with none).
    double newsShare(const std::string& cell, int window) const;

    // Invitations and promises.
    std::vector<TalkInvite>& invites() { return invites_; }
    const std::vector<TalkInvite>& invites() const { return invites_; }
    std::vector<TalkPromise>& promises() { return promises_; }
    const std::vector<TalkPromise>& promises() const { return promises_; }
    // The latest invitation between the two that was checked (kept or missed), or null.
    const TalkInvite* lastInvite(const std::string& a, const std::string& b) const;
    // The latest talk-only promise `by` made to `to`, or null.
    const TalkPromise* lastPromise(const std::string& by, const std::string& to) const;

    // Expired records dropped, and the drift, moods and kept tags of residents `gone` says are gone (dead, or no longer in
    // the world); then, past 30,000, the notes out of hearing, then the least recently used sets.
    void prune(double day, const std::function<bool(const std::string&)>& gone = {});
    std::size_t size() const;
    void clear();
    json::Value save() const;
    void load(const json::Value& v);

  private:
    struct Place
    {
        std::deque<std::pair<std::string, bool>> last;   // (topic, news), newest last
        std::int64_t day = -1;
        std::map<std::string, int> today;
    };
    std::map<std::string, std::deque<TalkNote>> notes_;     // By pair key.
    std::map<std::string, std::deque<TalkTold>> told_;      // By listener.
    std::map<std::string, std::deque<TalkSaid>> said_;      // By subject.
    std::map<std::string, std::deque<std::string>> heard_;  // By resident.
    std::map<std::string, std::map<std::string, std::pair<std::string, double>>> kept_;   // By resident.
    std::map<std::string, Place> places_;                   // By cell.
    std::vector<TalkInvite> invites_;
    std::vector<TalkPromise> promises_;
    std::map<std::string, std::map<std::string, TalkDrift>> drift_;   // By resident, then issue.
    std::map<std::string, std::deque<std::string>> applied_;           // By resident: event ids applied.
    std::map<std::string, std::deque<std::pair<double, std::string>>> moods_;   // By resident.
    // When each record set was last used (written or read), for the least-recently-used cap: "n:pair", "t:id"...
    mutable std::map<std::string, double> touched_;
    double now_ = 0;                                        // The latest day written (what reads are stamped with).
    void touch(const std::string& key) const { touched_[key] = now_; }
    // Stamped once, and before any set heard talk used (a million days back): a set only gossip out of hearing ever wrote
    // goes before every one that was heard, the oldest first; heard talk using it later makes it recent.
    static constexpr double QuietBias = 1e6;
    void touchQuiet(const std::string& key) const { touched_.emplace(key, now_ - QuietBias); }
    void trimNotes(std::deque<TalkNote>& list) const;
};
} // namespace ratw
