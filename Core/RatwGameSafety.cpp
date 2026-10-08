// Mute, block and report (Docs/Design/50-player-card-friends-safety.md, Phase 2; agreed must-haves, doc 48 Part 11).
// A mute stops a wolf's lines reaching the muter's account; a block does that for every character of the blocked
// account, and refuses scenes, parties and challenges between the two, without telling either who the other's other
// characters are. A report sends the Dungeon Master the lines the reporter received from that wolf, as evidence.
#include "RatwGame.h"
#include "RatwNames.h"
#include "RatwStanding.h"

#include <algorithm>
#include <ctime>

namespace ratw::game
{
using json::Value;

namespace
{
constexpr std::size_t MostMutes = 200, MostBlocks = 500, HeardKept = 60, EvidenceKept = 20;
constexpr double HeardSeconds = 1800;
constexpr int ReportsADay = 5;
} // namespace

bool Game::hides(const std::string& listener, const std::string& author) const
{
    if (listener == author)
        return false;
    const auto marks = safety_.find(accountKey(listener));
    if (marks == safety_.end())
        return false;
    const auto theirs = accountKey(author);
    for (const auto& m : marks->second)
        if ((m.kind == "mute" && m.target == author) || (m.kind == "block" && m.target == theirs))
            return true;
    return false;
}

bool Game::blocked(const std::string& a, const std::string& b) const
{
    return blockedAccounts(accountKey(a), accountKey(b));
}

bool Game::blockedAccounts(const std::string& one, const std::string& two) const
{
    if (one.empty() || two.empty() || one == two)
        return false;
    const auto held = [&](const std::string& holder, const std::string& target) {
        const auto marks = safety_.find(holder);
        if (marks == safety_.end())
            return false;
        for (const auto& m : marks->second)
            if (m.kind == "block" && m.target == target)
                return true;
        return false;
    };
    return held(one, two) || held(two, one);
}

void Game::heardLine(const std::string& listener, std::uint64_t seq, const std::string& author, const std::string& channel,
                     const std::string& text)
{
    // A connected player's record of what reached them (doc 50, 7): only ever read for their own reports.
    auto& lines = heard_[listener];
    const double t = now();
    lines.push_back({seq, t, author, channel, text.substr(0, 2000)});
    while (!lines.empty() && (lines.size() > HeardKept || t - lines.front().at > HeardSeconds))
        lines.erase(lines.begin());
}

bool Game::silenced(const std::string& characterId, std::string* until) const
{
    const auto it = people_.find(accountKey(characterId));
    if (it == people_.end() || it->second.silencedUntil <= now())
        return false;
    if (until)
    {
        const auto minutes = int((it->second.silencedUntil - now()) / 60) + 1;
        *until = minutes >= 120 ? std::to_string(minutes / 60) + " hours" : std::to_string(minutes) + " minutes";
    }
    return true;
}

int Game::upheldReportsWithin(const std::string& account, int days) const
{
    int n = 0;
    for (const auto& [id, r] : reportCache_)
        if (r.status == "upheld" && r.reportedAccount == account && r.decidedAt >= 0 && now() - r.decidedAt <= days * 86400.0)
            ++n;
    return n;
}

void Game::sendSafety(Connection* c)
{
    // One's own mutes and blocks: each by the wolf one pointed at, as one knew them then (never their other characters).
    if (!c)
        return;
    auto e = Value::object();
    e.add("type", "safety");
    auto list = Value::array();
    if (const auto marks = safety_.find(accountKey(c)); marks != safety_.end())
        for (const auto& m : marks->second)
        {
            auto row = Value::object();
            row.add("kind", m.kind);
            row.add("character", m.character);
            row.add("label", m.label);
            row.add("at", m.at);
            list.push(row);
        }
    e.add("marks", list);
    send(c, e);
}

bool Game::safetyCommand(Connection* c, const json::Value& j, Result& result)
{
    // {"type": "safety", "verb": "mute" | "unmute" | "block" | "unblock" | "report" | "list", "target": id | "line": seq}.
    const auto& id = c->entityId;
    const auto verb = j.string("verb");
    const auto mine = accountKey(c);
    if (id.empty() || mine.empty())
        return false;
    if (verb == "list")
    {
        sendSafety(c);
        result = {true, {}, {}};
        return true;
    }
    // Who is meant: a wolf by id, or the author of a line this player received (found in their own record, so the page
    // never learns who wrote an anonymous line).
    std::string target = j.string("target"), lineChannel;
    const bool byLine = j.has("line");
    if (byLine)
    {
        target.clear();
        const auto seq = std::uint64_t(j.number("line", -1));
        if (const auto lines = heard_.find(id); lines != heard_.end())
            for (const auto& l : lines->second)
                if (l.seq == seq)
                    target = l.author, lineChannel = l.channel;
    }
    const auto* them = world_.entity(target);
    // (A private message's author may be away: a player character the game keeps is known too. Doc 50, 4.)
    const bool known = (them && !them->npc) || (!them && !target.empty() && characters_.count(target));
    // How the list names them: as this wolf knows them, or by handle for a private message (all one knew of them there).
    const auto labelOf = [&]() {
        const auto handle = handleOf(accountKey(target));
        return lineChannel == "private" && !handle.empty() ? handle : names::capitalised(labelFor(id, target));
    };
    if (verb == "unmute" || verb == "unblock")
    {
        auto& marks = safety_[mine];
        const auto kind = verb == "unmute" ? "mute" : "block";
        const auto before = marks.size();
        marks.erase(std::remove_if(marks.begin(), marks.end(),
                                   [&](const SafetyMark& m) { return m.kind == kind && m.character == j.string("target"); }),
                    marks.end());
        result = marks.size() < before ? Result{true, verb == "unmute" ? "Unmuted." : "Unblocked.", {}}
                                       : Result{false, "They aren't on your list.", {}};
    }
    else if (!target.empty() && target == id)
        result = {false, "Not yourself.", {}};
    else if (verb == "mute" || verb == "block")
    {
        if (!known && !(them && verb == "mute"))
        {
            result = {false, byLine ? "That line can't be traced now." : "Only a wolf you have met.", {}};
            return true;
        }
        auto& marks = safety_[mine];
        const auto count = std::count_if(marks.begin(), marks.end(), [&](const SafetyMark& m) { return m.kind == verb; });
        const auto aim = verb == "mute" ? target : accountKey(target);
        if (aim == mine)
            result = {false, "Not one of your own wolves.", {}};
        else if (std::any_of(marks.begin(), marks.end(), [&](const SafetyMark& m) { return m.kind == verb && m.target == aim; }))
            result = {true, verb == "mute" ? "Already muted." : "Already blocked.", {}};
        else if (std::size_t(count) >= (verb == "mute" ? MostMutes : MostBlocks))
            result = {false, "Your list is full.", {}};
        else
        {
            marks.push_back({verb, aim, target, labelOf(), now()});
            if (verb == "block")
                unfriend(mine, aim);                // (A block ends a friendship, and any request between them.)
            // (A mute isn't told to anyone; nor is a block. Their lines simply stop reaching this account.)
            result = {true, verb == "mute" ? "Muted: you won't see their words." : "Blocked: you won't see their words, and they can't join you.", {}};
        }
    }
    else if (verb == "report")
    {
        const auto kind = j.string("kind", "speech");
        const auto category = j.string("category", "other");
        if (!known)
            result = {false, byLine ? "That line can't be traced now." : "Only a wolf you have met.", {}};
        else if (!reports_)
            result = {false, "Reports can't be kept on this server.", {}};
        else if (kind != "speech" && kind != "profile")
            result = {false, "Report what they said, or their profile.", {}};
        else if (!reports::validCategory(category))
            result = {false, "Choose what kind of trouble it is.", {}};
        else
        {
            int today = 0;
            for (const auto& [rid, r] : reportCache_)
                today += r.reporterAccount == mine && now() - r.created < 86400;
            if (today >= ReportsADay)
            {
                result = {false, "You have made " + std::to_string(ReportsADay) + " reports today; a Dungeon Master will look at them.", {}};
                return true;
            }
            reports::Report r;
            r.id = "rep-" + guid();
            r.created = now();
            r.reporterAccount = mine;
            r.reporterCharacter = id;
            r.reportedAccount = accountKey(target);
            r.reportedCharacter = target;
            r.kind = kind;
            r.category = category;
            r.note = people::clean(j.string("note"), 300);
            if (kind == "profile")
            {
                // A copy of the profile as it stands.
                if (const auto p = profiles_.find(target); p != profiles_.end())
                    r.evidence.push_back({0, now(), "profile", json::dump(people::save(p->second))});
            }
            else if (const auto lines = heard_.find(id); lines != heard_.end())
                for (const auto& l : lines->second)
                    if (l.author == target && r.evidence.size() < EvidenceKept)
                        r.evidence.push_back({l.seq, l.at, l.channel, l.text});
            std::string error;
            if (!reports_->add(r, error))
                result = {false, "The report couldn't be kept: " + error, {}};
            else
            {
                reportCache_[r.id] = r;
                note("info", "RATW_REPORT " + r.id + " " + r.category + " against " + r.reportedCharacter);
                result = {true, "Reported. A Dungeon Master will look at it.", {}};
                if (j.boolean("block"))
                {
                    auto& marks = safety_[mine];
                    if (std::none_of(marks.begin(), marks.end(), [&](const SafetyMark& m) { return m.kind == "block" && m.target == r.reportedAccount; }))
                    {
                        marks.push_back({"block", r.reportedAccount, target, labelOf(), now()});
                        unfriend(mine, r.reportedAccount);
                    }
                    result.message += " And blocked.";
                }
            }
        }
    }
    else
        return false;
    if (result.ok)
    {
        saveSoon();
        sendSafety(c);
    }
    return true;
}

Result Game::decideReport(const std::string& id, const std::string& decision, const std::string& outcome, int hours, const std::string& by)
{
    // A Dungeon Master's decision (doc 50, 7): uphold or dismiss; an upheld one with a note only, a warning the player
    // sees, or a silence of 1, 6, 24 or 72 hours.
    const auto it = reportCache_.find(id);
    if (it == reportCache_.end())
        return {false, "No such report.", {}};
    if (decision != "uphold" && decision != "dismiss")
        return {false, "Uphold or dismiss.", {}};
    if (decision == "uphold" && outcome != "note" && outcome != "warning" && outcome != "silence")
        return {false, "A note, a warning or a silence.", {}};
    if (decision == "uphold" && outcome == "silence" && hours != 1 && hours != 6 && hours != 24 && hours != 72)
        return {false, "A silence is 1, 6, 24 or 72 hours.", {}};
    auto r = it->second;
    r.status = decision == "uphold" ? "upheld" : "dismissed";
    r.outcome = decision == "uphold" ? outcome : "";
    r.silenceHours = decision == "uphold" && outcome == "silence" ? hours : 0;
    r.decidedBy = by;
    r.decidedAt = now();
    if (reports_)
        reports_->update(r);
    it->second = r;
    const auto tell = [&](const std::string& words) {
        for (const auto& cid : r.reportedAccount.rfind("dev:", 0) == 0 ? std::vector<std::string>{r.reportedAccount.substr(4)}
                                                                         : accounts_.characters(r.reportedAccount))
            if (auto* c = clientOf(cid))
                system(c, words);
    };
    if (r.status == "upheld" && outcome == "warning")
        tell("A Dungeon Master has looked at a report about you and warns your account. Please keep to the game's manners.");
    if (r.status == "upheld" && outcome == "silence")
    {
        auto& person = personOf(r.reportedAccount);
        person.silencedUntil = std::max(person.silencedUntil, now() + hours * 3600.0);
        tell("A Dungeon Master has silenced your account for " + std::to_string(hours) + (hours == 1 ? " hour" : " hours") +
             ": you can't speak, in character or out of it, until then.");
    }
    if (accounts_.exists(r.reportedAccount))
        checkUnlocks(r.reportedAccount);           // (An upheld report holds Quickened back: doc 49.)
    if (r.status == "upheld")                       // (And stops mentoring at once: doc 52, 3.)
        mentorOff(r.reportedAccount, "a report about you was upheld.");
    saveSoon();
    return {true, "Report " + id + " " + r.status + (r.outcome.empty() ? "" : ": " + r.outcome) + ".", {}};
}
} // namespace ratw::game
