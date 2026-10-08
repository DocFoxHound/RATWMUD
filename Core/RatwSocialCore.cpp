#include "RatwSocialCore.h"
#include "RatwPractice.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <sstream>

namespace ratw
{
namespace
{
std::string trim(std::string value)
{
    const auto a = value.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
        return {};
    return value.substr(a, value.find_last_not_of(" \t\r\n") - a + 1);
}
std::string brief(const std::string& value, std::size_t limit)
{
    if (value.size() <= limit)
        return value;
    std::size_t end = limit;
    while (end && (static_cast<unsigned char>(value[end]) & 0xC0) == 0x80)
        --end;
    return value.substr(0, end) + "…";
}
// The last `limit` bytes of `value`, starting on a whole UTF-8 character, marked "…" where cut.
std::string tail(const std::string& value, std::size_t limit)
{
    if (value.size() <= limit)
        return value;
    std::size_t start = value.size() - limit;
    while (start < value.size() && (static_cast<unsigned char>(value[start]) & 0xC0) == 0x80)
        ++start;
    return "…" + value.substr(start);
}
void append(ParsedPost& post, const std::string& kind, const std::string& text)
{
    const std::string clean = trim(text);
    if (!clean.empty())
    {
        post.segments.push_back({kind, clean});
        if (kind == "speech")
            post.speech = true;
    }
}
} // namespace

ParsedPost parsePost(const std::string& text)
{
    ParsedPost out;
    if (text.empty() || text.size() > 32768)
    {
        out.error = "Write between 1 and 32,768 UTF-8 bytes.";
        return out;
    }
    const bool quoted = text.find('"') != std::string::npos;
    std::size_t i = 0;
    while (i < text.size())
    {
        if (text[i] == '"')
        {
            ++i;
            std::string speech;
            bool ended = false;
            for (; i < text.size(); ++i)
            {
                if (text[i] == '\\' && i + 1 < text.size())
                {
                    speech += text[++i];
                    continue;
                }
                if (text[i] == '"')
                {
                    ++i;
                    ended = true;
                    break;
                }
                speech += text[i];
            }
            if (!ended)
            {
                out.error = "Close the speech quotation before sending.";
                return out;
            }
            append(out, "speech", speech);
            continue;
        }
        if (text[i] == '/' && (i == 0 || std::isspace(static_cast<unsigned char>(text[i - 1]))))
        {
            if (i + 1 < text.size() && text[i + 1] == '/')
            {
                i += 2;
                std::string escaped = "/";
                while (i < text.size() && !std::isspace(static_cast<unsigned char>(text[i])))
                    escaped += text[i++];
                append(out, quoted ? "narration" : "speech", escaped);
                continue;
            }
            const auto start = ++i;
            while (i < text.size() && std::isalpha(static_cast<unsigned char>(text[i])))
                ++i;
            const auto command = text.substr(start, i - start);
            if (command == "sigh")
            {
                append(out, "action", "sighs softly.");
                continue;
            }
            if (command == "sit" || command == "lay" || command == "stand")
            {
                out.posture = command == "sit" ? "sitting" : command == "lay" ? "lying" : "standing";
                append(out, "state", command == "sit" ? "sits down." : command == "lay" ? "lies down." : "stands up.");
                continue;
            }
            if (command == "action" || command == "pose" || command == "me")
            {
                const auto bodyStart = i;
                while (i < text.size() && text[i] != '"' &&
                       !(text[i] == '/' && i > 0 && std::isspace(static_cast<unsigned char>(text[i - 1]))))
                    ++i;
                auto body = trim(text.substr(bodyStart, i - bodyStart));
                if (body.empty())
                {
                    out.error = "/" + command + " needs a description.";
                    return out;
                }
                if (command == "me")
                {
                    if (body.size() > 512)
                    {
                        out.error = "Current state is limited to 512 UTF-8 bytes.";
                        return out;
                    }
                    out.state = body;
                    out.hasState = true;
                }
                append(out, command == "me" ? "state" : "action", body);
                continue;
            }
            out.error = "Unknown command /" + command + ". Use // to write a literal slash command.";
            return out;
        }
        std::string normal;
        while (i < text.size())
        {
            if (text[i] == '"' || (text[i] == '/' && (i == 0 || std::isspace(static_cast<unsigned char>(text[i - 1])))))
                break;
            if (text[i] == '\\' && i + 1 < text.size())
            {
                normal += text[i + 1];
                i += 2;
            }
            else
                normal += text[i++];
        }
        append(out, quoted ? "narration" : "speech", normal);
    }
    out.ok = !out.segments.empty();
    if (!out.ok)
        out.error = "The post is empty.";
    return out;
}

std::string maskWords(const std::string& text, double clarity, std::uint64_t seed)
{
    if (clarity >= 0.999)
        return text;
    if (clarity <= 0.0)
        return "...";
    std::istringstream in(text);
    std::string word, out;
    bool gap = false;
    while (in >> word)
    {
        seed ^= seed >> 12;
        seed ^= seed << 25;
        seed ^= seed >> 27;
        const double roll = static_cast<double>((seed * 2685821657736338717ULL) >> 11) / 9007199254740992.0;
        const bool heard = roll < clarity;
        if (heard || !gap)
        {
            if (!out.empty())
                out += ' ';
            out += heard ? word : "...";
        }
        gap = !heard;
    }
    return out;
}

SocialEvidence roleplayEvidence(const ParsedPost& post)
{
    SocialEvidence result;
    std::string normalized;
    bool word = false;
    for (const auto& segment : post.segments)
    {
        if (segment.kind != "speech" && segment.kind != "narration" && segment.kind != "action")
            continue;
        const bool acted = segment.kind == "action";       // (Half weight, counted apart: doc 32, 1.1.)
        for (std::size_t i = 0; i < segment.text.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(segment.text[i]);
            const bool alnum = std::isalnum(c) || c >= 128;
            const bool join = (c == '\'' || c == '-') && i > 0 && i + 1 < segment.text.size() &&
                              (std::isalnum(static_cast<unsigned char>(segment.text[i - 1])) ||
                               static_cast<unsigned char>(segment.text[i - 1]) >= 128) &&
                              (std::isalnum(static_cast<unsigned char>(segment.text[i + 1])) ||
                               static_cast<unsigned char>(segment.text[i + 1]) >= 128);
            if (alnum || join)
            {
                if (!word)
                {
                    ++(acted ? result.actionWords : result.words);
                    if (!normalized.empty())
                        normalized += ' ';
                }
                normalized += static_cast<char>(std::tolower(c));
                word = true;
            }
            else
                word = false;
        }
        word = false;
    }
    if (normalized.empty())
        return result;
    result.contentHash = 1469598103934665603ULL;
    for (unsigned char c : normalized)
    {
        result.contentHash ^= c;
        result.contentHash *= 1099511628211ULL;
    }
    // Long-form roleplay remains eligible. Bound its contribution metadata,
    // never truncate the actual prose or reward additional length beyond500.
    result.words = std::min(result.words, 500);
    result.actionWords = std::min(result.actionWords, 500);
    return result;
}

std::vector<Segment> perceivePost(const ParsedPost& post, double hearing, double vision, std::uint64_t seed)
{
    bool perceived = false;
    for (const auto& segment : post.segments)
        if ((segment.kind == "speech" && hearing > 0) || (segment.kind != "speech" && vision > 0) ||
            (segment.text == "sighs softly." && hearing > 0.25))
            perceived = true;
    if (!perceived)
        return {};
    std::vector<Segment> out;
    for (const auto& segment : post.segments)
    {
        if (segment.kind == "speech")
            out.push_back({segment.kind, maskWords(segment.text, hearing, ++seed)});
        else if (segment.text == "sighs softly." && hearing > 0.25)
            out.push_back(segment);
        else
            out.push_back({segment.kind, vision >= 0.45 ? segment.text : "···"});
    }
    return out;
}

void MemoryStore::record(const std::string& npc, const std::string& subject, const MemoryTurn& turn)
{
    for (const auto& summary : summaries)
        if (summary.npc == npc && summary.subject == subject &&
            std::find(summary.sourceEvents.begin(), summary.sourceEvents.end(), turn.event) !=
                summary.sourceEvents.end())
            return;
    const std::string key = npc + "|" + subject;
    auto& memory = active[key];
    if (memory.id.empty())
    {
        memory.id = "memory-" + std::to_string(nextConversation++);
        memory.npc = npc;
        memory.subject = subject;
        memory.started = turn.at;
    }
    if (std::find(memory.olderEvents.begin(), memory.olderEvents.end(), turn.event) != memory.olderEvents.end() ||
        std::any_of(memory.turns.begin(), memory.turns.end(),
                    [&](const MemoryTurn& t) { return t.event == turn.event; }))
        return;
    memory.lastActivity = turn.at;
    memory.turns.push_back(turn);
    if (memory.turns.size() > 32)
    {
        memory.olderContext += memory.turns.front().who + ": “" + brief(memory.turns.front().text, 240) + "” ";
        memory.olderEvents.push_back(memory.turns.front().event);
        memory.turns.erase(memory.turns.begin());
    }
}

int MemoryStore::consolidate(double now)
{
    int count = 0;
    for (auto it = active.begin(); it != active.end();)
    {
        const auto& memory = it->second;
        if (now - memory.lastActivity < InactivitySeconds)
        {
            ++it;
            continue;
        }
        const auto duplicate =
            std::find_if(summaries.begin(), summaries.end(), [&](const MemorySummary& s) { return s.id == memory.id; });
        if (duplicate == summaries.end())
        {
            MemorySummary summary;
            summary.id = memory.id;
            summary.npc = memory.npc;
            summary.subject = memory.subject;
            summary.started = memory.started;
            summary.consolidated = now;
            summary.text = "Conversation record. " + memory.olderContext;
            summary.sourceEvents = memory.olderEvents;
            for (std::size_t i = 0; i < memory.turns.size(); ++i)
            {
                summary.sourceEvents.push_back(memory.turns[i].event);
                summary.text += memory.turns[i].who + " said: “" + brief(memory.turns[i].text, 240) + "” ";
            }
            summaries.push_back(std::move(summary));
            ++count;
        }
        it = active.erase(it);
    }
    return count;
}

std::vector<ActiveMemory> MemoryStore::due(double now) const
{
    std::vector<ActiveMemory> out;
    for (const auto& [key, memory] : active)
        if (now - memory.lastActivity >= InactivitySeconds)
            out.push_back(memory);
    return out;
}

bool MemoryStore::rewrite(const std::string& id, const std::string& text)
{
    if (text.empty())
        return false;
    for (auto& summary : summaries)
        if (summary.id == id)
        {
            summary.text = "Summary. " + brief(text, 1200);
            return true;
        }
    return false;
}

std::string MemoryStore::recall(const std::string& npc, const std::string& subject) const
{
    const auto current = active.find(npc + "|" + subject);
    if (current != active.end())
    {
        for (auto it = current->second.turns.rbegin(); it != current->second.turns.rend(); ++it)
            if (it->who != npc)
                return brief(it->text, 160);
    }
    for (auto it = summaries.rbegin(); it != summaries.rend(); ++it)
        if (it->npc == npc && it->subject == subject)
            return brief(it->text, 400);
    return {};
}

std::string MemoryStore::recallForDialogue(const std::string& npc, const std::string& subject, std::size_t budget) const
{
    std::string current;
    if (const auto found = active.find(npc + "|" + subject); found != active.end())
    {
        const auto& memory = found->second;
        // The newest turns that fit in two thirds of the budget, oldest of them first.
        std::vector<std::string> lines;
        std::size_t used = 0;
        for (auto it = memory.turns.rbegin(); it != memory.turns.rend() && lines.size() < 16; ++it)
        {
            std::string line = (it->who == npc ? std::string("You") : it->who) + ": “" + brief(it->text, 300) + "”\n";
            if (used + line.size() > budget * 2 / 3)
                break;
            used += line.size();
            lines.push_back(std::move(line));
        }
        const bool trimmed = lines.size() < memory.turns.size();
        if (!memory.olderContext.empty() || trimmed)
            current += "Earlier in this conversation: " + tail(memory.olderContext.empty() ? std::string("(more was said)")
                                                                                          : memory.olderContext, 400) + "\n";
        if (!lines.empty())
        {
            current += "This conversation so far:\n";
            for (auto it = lines.rbegin(); it != lines.rend(); ++it)
                current += *it;
        }
    }
    // Earlier conversations with this subject, newest first; a summary's end is its most recent part.
    std::vector<std::string> earlier;
    std::size_t room = budget > current.size() ? budget - current.size() : 0;
    for (auto it = summaries.rbegin(); it != summaries.rend() && earlier.size() < 3; ++it)
    {
        if (it->npc != npc || it->subject != subject)
            continue;
        std::string text = it->text;
        for (const std::string prefix : {"Conversation record. ", "Summary. "})
            if (text.rfind(prefix, 0) == 0)
                text = text.substr(prefix.size());
        const std::string label = "An earlier conversation: ";
        if (room < label.size() + 80)
            break;
        // Room for the label, the "…" marking a cut (three bytes) and the newline.
        std::string entry = label + tail(text, std::min<std::size_t>(700, room - label.size() - 4)) + "\n";
        room -= std::min(room, entry.size());
        earlier.push_back(std::move(entry));
    }
    std::string out;
    for (auto it = earlier.rbegin(); it != earlier.rend(); ++it)
        out += *it;
    return out + current;
}

int SocialLedger::record(SocialPost post, const std::vector<std::string>& listeners)
{
    // Ordinary speech is evidence only. It never grants XP directly.
    if (post.ooc || post.words < 1 || post.words > 500 || acceptedEvents.count(post.event))
        return 0;
    auto prior = recent.find(post.actor);
    if (prior != recent.end() && post.at - prior->second.at < 2)
        return 0;
    if (post.contentHash)
    {
        auto& Hashes = duplicateHashes[post.actor];
        auto It = Hashes.find(post.contentHash);
        if (It != Hashes.end() && post.at - It->second < 60)
            return 0;
        Hashes[post.contentHash] = post.at;
    }
    post.audience = listeners;
    acceptedEvents.insert(post.event);
    recent[post.actor] = post;
    auto Add = [&](SocialSession& S, const SocialPost& P) {
        auto& Member = S.members[P.actor];
        if (Member.left)
            return;                                  // (Stepped out: their words count no more here.)
        if (Member.joined == 0)
        {
            Member.joined = P.at;
            sceneOf_[P.actor].insert(S.id);
        }
        S.last = P.at;
        if (P.words < 5)
            return;
        for (const auto& Other : S.members)
            if (Other.first != P.actor && Other.second.last > 0 && P.at - Other.second.last <= 180 &&
                std::find(Other.second.lastAudience.begin(), Other.second.lastAudience.end(), P.actor) !=
                    Other.second.lastAudience.end() &&
                std::find(P.audience.begin(), P.audience.end(), Other.first) != P.audience.end())
            {
                ++Member.replies;
                break;
            }
        Member.turns = std::min(200, Member.turns + 1);
        Member.words = std::min(10000, Member.words + P.words);
        Member.last = P.at;
        Member.lastAudience = P.audience;
    };
    const auto open = [&](SocialSession& S) {
        sessions[S.id] = S;
        openIn_[S.cell].insert(S.id);
        for (const auto& [who, m] : S.members)
            sceneOf_[who].insert(S.id);
    };
    // Routing (doc 51, §6). 1: the actor's own scene in this lane (their party's, or the room's). It counts only when
    // another of its wolves hears them (or a party mate is near): words said to no one aren't the scene's.
    if (const auto mine = sceneOf_.find(post.actor); mine != sceneOf_.end())
        for (const auto& sid : mine->second)
            if (auto it = sessions.find(sid); it != sessions.end() && it->second.ended == 0 && it->second.cell == post.cell &&
                                              it->second.party == post.party)
            {
                bool heard = !post.party.empty();
                for (const auto& Other : it->second.members)
                    heard = heard || (Other.first != post.actor &&
                                      std::find(listeners.begin(), listeners.end(), Other.first) != listeners.end());
                if (heard)
                    Add(it->second, post);
                return 0;
            }
    const auto here = openIn_.find(post.cell);
    std::vector<SocialSession*> inCell;
    if (here != openIn_.end())
        for (const auto& sid : here->second)
            if (auto it = sessions.find(sid); it != sessions.end() && it->second.ended == 0 && !isFight(it->second))
                inCell.push_back(&it->second);
    // 2: a scene they pressed Join on, or were let in to, in the last two minutes: their line counts at once.
    for (auto* S : inCell)
        if (const auto let = S->admitted.find(post.actor); let != S->admitted.end() && let->second >= post.at)
        {
            S->admitted.erase(let);
            Add(*S, post);
            return 0;
        }
    // A party mate (or a fighter) is always part of their party's (or fight's) scene here; party mates in earshot open
    // their party's scene at once, Private (doc 51, §5), with no A-B-A.
    if (!post.party.empty() && here != openIn_.end())
        for (const auto& sid : here->second)
            if (auto it = sessions.find(sid); it != sessions.end() && it->second.ended == 0 && it->second.party == post.party)
            {
                Add(it->second, post);
                return 0;
            }
    if (!post.party.empty())
    {
        SocialSession S;
        S.id = "scene-" + std::to_string(post.event);
        S.cell = post.cell;
        S.party = post.party;
        S.started = post.at;
        S.openness = "private";
        Add(S, post);
        open(S);
        return 0;
    }
    // 3: an Open scene takes one who joins in. A room scene: when a member hears them. A party's scene, made Open:
    // when they answer a member (heard one in the last 30 s, and are heard by them). Knock and Private scenes never take
    // outsiders by themselves.
    for (auto* S : inCell)
    {
        if (S->openness != "open")
            continue;
        for (const auto& Other : S->members)
        {
            if (Other.first == post.actor || Other.second.left)
                continue;
            const bool hears = std::find(listeners.begin(), listeners.end(), Other.first) != listeners.end();
            const bool answers = post.at - Other.second.last <= 30 &&
                                 std::find(Other.second.lastAudience.begin(), Other.second.lastAudience.end(), post.actor) !=
                                     Other.second.lastAudience.end();
            if (hears && (S->party.empty() || answers))
            {
                Add(*S, post);
                return 0;
            }
        }
    }
    // 4: an A-B-A exchange between those in no scene here makes one (beside any others: several may share a place).
    auto& Pending = candidates[post.cell];
    Pending.erase(
        std::remove_if(Pending.begin(), Pending.end(), [&](const SocialPost& P) { return post.at - P.at > 30; }),
        Pending.end());
    if (Pending.size() >= 2)
    {
        const auto& A = Pending[Pending.size() - 2];
        const auto& B = Pending.back();
        if (A.actor == post.actor && B.actor != post.actor &&
            std::find(A.audience.begin(), A.audience.end(), B.actor) != A.audience.end() &&
            std::find(B.audience.begin(), B.audience.end(), A.actor) != B.audience.end() &&
            std::find(post.audience.begin(), post.audience.end(), B.actor) != post.audience.end())
        {
            SocialSession S;
            S.id = "scene-" + std::to_string(A.event);
            S.cell = post.cell;
            S.started = A.at;
            S.openness = privatePlace && privatePlace(post.cell) ? "private" : "open";
            Add(S, A);
            Add(S, B);
            Add(S, post);
            open(S);
            Pending.clear();
            return 0;
        }
    }
    Pending.push_back(post);
    if (Pending.size() > 16)
        Pending.erase(Pending.begin());
    return 0;
}

void SocialLedger::ended(const SocialSession& scene)
{
    if (const auto it = openIn_.find(scene.cell); it != openIn_.end())
    {
        it->second.erase(scene.id);
        if (it->second.empty())
            openIn_.erase(it);
    }
    for (const auto& [who, m] : scene.members)
    {
        if (const auto it = sceneOf_.find(who); it != sceneOf_.end())
        {
            it->second.erase(scene.id);
            if (it->second.empty())
                sceneOf_.erase(it);
        }
        endedOf_[who] = scene.id;                     // (One who stepped out sees it end too: their stars to give.)
    }
}

const std::set<std::string>& SocialLedger::scenesOf(const std::string& actor) const
{
    static const std::set<std::string> none;
    const auto it = sceneOf_.find(actor);
    return it == sceneOf_.end() ? none : it->second;
}

const std::set<std::string>& SocialLedger::openIn(const std::string& cell) const
{
    static const std::set<std::string> none;
    const auto it = openIn_.find(cell);
    return it == openIn_.end() ? none : it->second;
}

std::string SocialLedger::lastEnded(const std::string& actor) const
{
    const auto it = endedOf_.find(actor);
    return it == endedOf_.end() ? std::string() : it->second;
}

std::string SocialLedger::routeFor(const std::string& actor, const std::string& cell, const std::string& party, double now) const
{
    if (const auto mine = sceneOf_.find(actor); mine != sceneOf_.end())
        for (const auto& lane : {party, std::string()})
            for (const auto& sid : mine->second)
                if (const auto it = sessions.find(sid); it != sessions.end() && it->second.ended == 0 && it->second.cell == cell &&
                                                        it->second.party == lane)
                    return sid;
    if (const auto here = openIn_.find(cell); here != openIn_.end())
        for (const auto& sid : here->second)
            if (const auto it = sessions.find(sid); it != sessions.end() && it->second.ended == 0)
                if (const auto let = it->second.admitted.find(actor); let != it->second.admitted.end() && let->second >= now)
                    return sid;
    return {};
}

void SocialLedger::reindexScenes()
{
    sceneOf_.clear();
    openIn_.clear();
    endedOf_.clear();
    std::map<std::string, double> latest;
    for (const auto& [sid, s] : sessions)
    {
        if (s.ended == 0)
        {
            openIn_[s.cell].insert(sid);
            for (const auto& [who, m] : s.members)
                if (!m.left)
                    sceneOf_[who].insert(sid);
            continue;
        }
        for (const auto& [who, m] : s.members)
            if (s.ended > latest[who])
            {
                latest[who] = s.ended;
                endedOf_[who] = sid;
            }
    }
    // One who stepped out of a scene still going: it ended for them then.
    for (const auto& [sid, s] : sessions)
        if (s.ended == 0)
            for (const auto& [who, m] : s.members)
                if (m.left && m.leftAt > latest[who])
                {
                    latest[who] = m.leftAt;
                    endedOf_[who] = sid;
                }
}

void SocialLedger::moment(const std::string& session, const SceneMoment& m)
{
    const auto it = sessions.find(session);
    if (it == sessions.end())
        return;
    auto& list = it->second.moments;
    if (list.size() >= MostMoments)
        return;
    for (const auto& had : list)
        if (had.kind == m.kind && had.actor == m.actor && had.target == m.target && had.detail == m.detail)
            return;
    list.push_back(m);
}

SocialResult SocialLedger::setOpenness(const std::string& member, const std::string& session, const std::string& value, double now)
{
    const auto it = sessions.find(session);
    if (it == sessions.end() || it->second.ended != 0 || !it->second.members.count(member) || it->second.members.at(member).left)
        return {false, "You aren't in that scene."};
    auto& S = it->second;
    if (isFight(S))
        return {false, "A fight is no scene to open or close."};
    if (value != "open" && value != "knock" && value != "private")
        return {false, "Open, knock or private."};
    if (S.openness == value)
        return {true, {}};
    if (now - S.opennessAt < OpennessSeconds)
        return {false, "The scene's door was changed a moment ago."};
    S.openness = value;
    S.opennessAt = now;
    if (value != "knock")
        S.knocks.clear();
    return {true, {}};
}

SocialResult SocialLedger::join(const std::string& actor, const std::string& session, double now)
{
    const auto it = sessions.find(session);
    if (it == sessions.end() || it->second.ended != 0 || isFight(it->second))
        return {false, "That scene is over."};
    auto& S = it->second;
    if (S.members.count(actor) && !S.members.at(actor).left)
        return {false, "You're in that scene already."};
    if (S.members.count(actor))
        return {false, "You stepped out of that scene."};
    if (S.openness != "open")
        return {false, S.openness == "knock" ? "Knock to join that scene." : "That scene is private."};
    S.admitted[actor] = now + JoinSeconds;
    return {true, {}};
}

SocialResult SocialLedger::knock(const std::string& actor, const std::string& session, double now)
{
    const auto it = sessions.find(session);
    if (it == sessions.end() || it->second.ended != 0 || isFight(it->second))
        return {false, "That scene is over."};
    auto& S = it->second;
    if (S.members.count(actor))
        return {false, S.members.at(actor).left ? "You stepped out of that scene." : "You're in that scene already."};
    if (S.openness == "open")
        return {false, "That scene is open: join it."};
    if (S.openness != "knock")
        return {false, "That scene is private."};
    if (const auto r = S.refused.find(actor); r != S.refused.end() && r->second > now)
        return {false, "They aren't taking anyone just now."};
    if (const auto k = S.knocks.find(actor); k != S.knocks.end() && now - k->second < KnockSeconds)
        return {false, "You've knocked already."};
    S.knocks[actor] = now;
    return {true, {}};
}

SocialResult SocialLedger::admit(const std::string& member, const std::string& session, const std::string& who, double now)
{
    const auto it = sessions.find(session);
    if (it == sessions.end() || it->second.ended != 0 || !it->second.members.count(member) || it->second.members.at(member).left)
        return {false, "You aren't in that scene."};
    auto& S = it->second;
    const auto k = S.knocks.find(who);
    if (k == S.knocks.end() || now - k->second >= KnockSeconds)
        return {false, "No one is knocking."};
    S.knocks.erase(k);
    S.admitted[who] = now + JoinSeconds;
    return {true, {}};
}

SocialResult SocialLedger::refuse(const std::string& member, const std::string& session, const std::string& who, double now)
{
    const auto it = sessions.find(session);
    if (it == sessions.end() || it->second.ended != 0 || !it->second.members.count(member) || it->second.members.at(member).left)
        return {false, "You aren't in that scene."};
    auto& S = it->second;
    if (!S.knocks.erase(who))
        return {false, "No one is knocking."};
    S.refused[who] = now + RefusedSeconds;
    return {true, {}};
}

int SocialLedger::settle(const std::string& id, double now)
{
    auto It = sessions.find(id);
    if (It == sessions.end() || It->second.ended > 0)
        return 0;
    auto& Scene = It->second;
    Scene.ended = now;
    ended(Scene);
    std::vector<std::string> Qualified;
    for (const auto& Pair : Scene.members)
        if (shaped(Pair.second))
            Qualified.push_back(Pair.first);
    if (Qualified.size() < 2)
        return 0;
    std::sort(Qualified.begin(), Qualified.end(), [&](const std::string& A, const std::string& B) {
        return Scene.members[A].joined == Scene.members[B].joined ? A < B
                                                                  : Scene.members[A].joined < Scene.members[B].joined;
    });
    int Total = 0;
    for (std::size_t Index = 0; Index < Qualified.size(); ++Index)
        if (!Scene.members[Qualified[Index]].left)      // (Those who stepped out were paid then.)
            Total += payMember(Scene, Qualified, Index, now);
    return Total;
}
bool SocialLedger::leave(const std::string& actor, const std::string& session, double now, int* paid)
{
    auto It = sessions.find(session);
    if (It == sessions.end() || It->second.ended > 0 || isFight(It->second))
        return false;
    auto& Scene = It->second;
    auto Me = Scene.members.find(actor);
    if (Me == Scene.members.end() || Me->second.left)
        return false;
    Me->second.left = true;
    Me->second.leftAt = now;
    if (const auto mine = sceneOf_.find(actor); mine != sceneOf_.end())
        mine->second.erase(session);
    endedOf_[actor] = session;                      // (It has ended for them: their card, doc 51.)
    std::vector<std::string> Qualified;
    for (const auto& Pair : Scene.members)
        if (shaped(Pair.second))
            Qualified.push_back(Pair.first);
    std::sort(Qualified.begin(), Qualified.end(), [&](const std::string& A, const std::string& B) {
        return Scene.members[A].joined == Scene.members[B].joined ? A < B
                                                                  : Scene.members[A].joined < Scene.members[B].joined;
    });
    const auto Index = std::find(Qualified.begin(), Qualified.end(), actor) - Qualified.begin();
    const int Amount = Qualified.size() >= 2 && Index < static_cast<std::ptrdiff_t>(Qualified.size())
                           ? payMember(Scene, Qualified, static_cast<std::size_t>(Index), now)
                           : 0;
    if (paid)
        *paid = Amount;
    return true;
}
int SocialLedger::payMember(const SocialSession& Scene, const std::vector<std::string>& Qualified, std::size_t Index, double now)
{
    const auto& Actor = Qualified[Index];
    int MostRepeated = 0, Today = 0, Count = 0;
    for (const auto i : receiptsOf(Actor))
        if (const auto& Entry = entries[i]; now - Entry.at < 86400)
        {
            Today += Entry.amount;
            if (Entry.amount > 0)
                ++Count;
        }
    for (const auto& Peer : Qualified)
        if (Peer != Actor)
        {
            int Repeat = 0;
            for (const auto& Other : sessions)
            {
                if (Other.first == Scene.id || Other.second.ended <= 0 || now - Other.second.ended >= 86400)
                    continue;
                auto A = Other.second.members.find(Actor), B = Other.second.members.find(Peer);
                if (A != Other.second.members.end() && B != Other.second.members.end() && shaped(A->second) &&
                    shaped(B->second))
                    ++Repeat;
            }
            MostRepeated = std::max(MostRepeated, Repeat);
        }
    int Amount = MostRepeated >= 4 || Count >= 8 ? 0 : (Index < 4 ? 20 : Index < 8 ? 12 : 5) / (1 << MostRepeated);
    Amount = std::max(0, std::min(Amount, 100 - Today));
    Amount = std::min(Amount, 2147483647 - points[Actor]);
    std::string Partners;
    for (const auto& Peer : Qualified)
        if (Peer != Actor)
        {
            if (!Partners.empty())
                Partners += ",";
            Partners += Peer;
        }
    LedgerEntry Entry;
    Entry.event = static_cast<std::uint64_t>(Scene.started * 1000) + Index;
    Entry.at = now;
    Entry.actor = Actor;
    Entry.partner = Partners;
    Entry.reason = "qualified_session_settlement";
    Entry.amount = Amount;
    Entry.session = Scene.id;
    add(Entry);
    points[Actor] += Amount;
    return Amount;
}
void SocialLedger::joinFight(const std::string& fight, const std::string& cell, const std::string& member, double now)
{
    auto& S = sessions[fightScene(fight)];
    if (S.id.empty())
    {
        S.id = fightScene(fight);
        S.cell = cell;
        S.party = fightTag(fight);
        S.started = now;
        S.last = now;
        S.openness = "";                            // (Joining a fight is doc 33's.)
        openIn_[cell].insert(S.id);
    }
    if (S.ended == 0 && !S.members.count(member))
    {
        S.members[member].joined = now;
        sceneOf_[member].insert(S.id);
    }
}

void SocialLedger::joinWork(const std::string& work, const std::string& cell, const std::string& member, double now)
{
    auto& S = sessions[workScene(work)];
    if (S.id.empty())
    {
        S.id = workScene(work);
        S.cell = cell;
        S.party = workTag(work);
        S.started = now;
        S.last = now;
        S.openness = "open";                        // (Open by default, as any scene: doc 51.)
        openIn_[cell].insert(S.id);
    }
    S.last = std::max(S.last, now);                 // (Alive while the work goes on.)
    if (S.ended == 0 && !S.members.count(member))
    {
        S.members[member].joined = now;
        sceneOf_[member].insert(S.id);
    }
}

int SocialLedger::settleWork(const std::string& work, double now)
{
    // Working isn't roleplay: only those who talked it through are paid (two talkers at least), as for a fight.
    return settleScene(workScene(work), {}, now);
}

int SocialLedger::settleFight(const std::string& fight, const std::set<std::string>& fought, double now)
{
    return settleScene(fightScene(fight), fought, now);
}

int SocialLedger::settleScene(const std::string& sceneId, const std::set<std::string>& fought, double now)
{
    auto It = sessions.find(sceneId);
    if (It == sessions.end() || It->second.ended > 0)
        return 0;
    auto& Scene = It->second;
    Scene.ended = now;
    ended(Scene);
    std::vector<std::string> Talked, Paid;
    for (const auto& [who, c] : Scene.members)
        if (shaped(c))
            Talked.push_back(who);
    if (Talked.size() < 2)
        Talked.clear();                             // (Talking alone is no scene.)
    for (const auto& [who, c] : Scene.members)
        if (fought.count(who) || std::find(Talked.begin(), Talked.end(), who) != Talked.end())
            Paid.push_back(who);
    const auto joined = [&](const std::string& A, const std::string& B) {
        return Scene.members[A].joined == Scene.members[B].joined ? A < B : Scene.members[A].joined < Scene.members[B].joined;
    };
    std::sort(Talked.begin(), Talked.end(), joined);
    std::sort(Paid.begin(), Paid.end(), joined);
    int Total = 0;
    for (std::size_t Index = 0; Index < Paid.size(); ++Index)
    {
        const auto& Actor = Paid[Index];
        int Count = 0;
        for (const auto i : receiptsOf(Actor))
            if (now - entries[i].at < 86400 && entries[i].amount > 0)
                ++Count;
        // The same partners again and again pay less: the most repeated, over the scenes both were paid in today.
        int MostRepeated = 0;
        for (const auto& Peer : Paid)
            if (Peer != Actor)
            {
                int Repeat = 0;
                for (const auto& [sid, Other] : sessions)
                    if (sid != Scene.id && Other.ended > 0 && now - Other.ended < 86400 && paidFor(Actor, sid) > 0 &&
                        paidFor(Peer, sid) > 0)
                        ++Repeat;
                MostRepeated = std::max(MostRepeated, Repeat);
            }
        int Amount = 0;                             // (Fighting itself teaches fighting, not social XP: doc 49.)
        const auto talked = std::find(Talked.begin(), Talked.end(), Actor);
        if (talked != Talked.end())
        {
            const auto at = std::size_t(talked - Talked.begin());
            Amount += FightTalkFactor * (at < 4 ? 20 : at < 8 ? 12 : 5);
        }
        Amount = MostRepeated >= 4 || Count >= 8 ? 0 : Amount / (1 << MostRepeated);
        std::string Partners;
        for (const auto& Peer : Paid)
            if (Peer != Actor)
                Partners += (Partners.empty() ? "" : ",") + Peer;
        // The same receipt as any scene's, so stars and Stories take it as one (a zero one too: no paying twice).
        Total += pay(Actor, Partners, "qualified_session_settlement", Scene.id, Amount, now,
                     static_cast<std::uint64_t>(Scene.started * 1000) + Index);
    }
    return Total;
}

int SocialLedger::endFor(const std::string& actor, double now)
{
    int Amount = 0;
    for (auto& Pair : sessions)
        if (Pair.second.ended == 0 && Pair.second.members.count(actor) && !isFight(Pair.second))
            Amount += settle(Pair.first, now);      // (A fight's scene ends with the fight.)
    return Amount;
}
void SocialLedger::tick(double now)
{
    for (auto& Pair : sessions)
        if (Pair.second.ended == 0 && now - Pair.second.last >= (isFight(Pair.second) ? FightEndSeconds : EndSeconds))
            settle(Pair.first, now);                // (A fight's scene ends with its fight: this only if that was lost.)
    // A Story nobody approved within a day closes without reward.
    for (auto& [id, st] : stories)
        if (st.state == "pending" && now - st.created > 86400)
            st.state = "expired";
    // Scenes ended more than eight days ago go (doc 51; players are told seven, so a day's grace is theirs). Stars and
    // Stories look back a day at most; Story books (Phase 7) take scenes for the seven days players are told.
    for (auto it = sessions.begin(); it != sessions.end();)
        if (it->second.ended > 0 && now - it->second.ended > KeepEndedSeconds)
        {
            for (const auto& [who, m] : it->second.members)
                if (const auto e = endedOf_.find(who); e != endedOf_.end() && e->second == it->first)
                    endedOf_.erase(e);
            it = sessions.erase(it);
        }
        else
            ++it;
    // Knocks past two minutes, and refusals past five, go.
    for (auto& [sid, sc] : sessions)
    {
        for (auto k = sc.knocks.begin(); k != sc.knocks.end();)
            k = now - k->second >= KnockSeconds ? sc.knocks.erase(k) : std::next(k);
        for (auto r = sc.refused.begin(); r != sc.refused.end();)
            r = r->second <= now ? sc.refused.erase(r) : std::next(r);
        for (auto a = sc.admitted.begin(); a != sc.admitted.end();)
            a = a->second < now ? sc.admitted.erase(a) : std::next(a);
    }
    // Stars past two days go (doc 51): duplicates, decay and the daily limit look back a day at most, Story Stars keep
    // their own record (`starred`), and the account-bound record is the star book's.
    stars.erase(std::remove_if(stars.begin(), stars.end(), [&](const SocialStar& s) { return now - s.at > 2 * 86400; }), stars.end());
}

int SocialLedger::level(const std::string& actor) const
{
    const auto it = points.find(actor);
    return practice::levelFor(it == points.end() ? 0 : it->second);
}

int SocialLedger::dailyCap()
{
    return practice::standing().dailyCap;
}

void SocialLedger::reindex()
{
    byActor_.clear();
    for (std::size_t i = 0; i < entries.size(); ++i)
        byActor_[entries[i].actor].push_back(i);
}

const std::vector<std::size_t>& SocialLedger::receiptsOf(const std::string& actor) const
{
    static const std::vector<std::size_t> none;
    const auto it = byActor_.find(actor);
    return it == byActor_.end() ? none : it->second;
}

void SocialLedger::add(const LedgerEntry& e)
{
    byActor_[e.actor].push_back(entries.size());
    entries.push_back(e);
}

// ------------------------------------------------------------------ Gold Stars and Stories (doc 32, 1.2)

std::string socialTitle(int level)
{
    return practice::titleFor(level);               // (Data/Progression/standing.json: doc 49.)
}

int SocialLedger::paidFor(const std::string& actor, const std::string& session) const
{
    int n = 0;
    for (const auto i : receiptsOf(actor))
        if (entries[i].session == session && entries[i].reason == "qualified_session_settlement")
            n += entries[i].amount;
    return n;
}

std::vector<std::string> SocialLedger::paidIn(const std::string& session) const
{
    std::vector<std::string> out;
    for (const auto& e : entries)
        if (e.session == session && e.reason == "qualified_session_settlement" && e.amount > 0 &&
            std::find(out.begin(), out.end(), e.actor) == out.end())
            out.push_back(e.actor);
    return out;
}

int SocialLedger::usedToday(const std::string& actor, double now) const
{
    int n = 0;
    for (const auto i : receiptsOf(actor))
        if (now - entries[i].at < 86400 && practice::socialReason(entries[i].reason))
            n += entries[i].amount;
    return n;
}

double SocialLedger::pairDecay(const std::string& a, const std::string& b, double now) const
{
    // Stars between the same two in a rolling day, either way: 1, 1/2, 1/4, then nothing.
    int n = 0;
    for (const auto& st : stars)
        if (now - st.at < 86400 && ((st.giver == a && st.recipient == b) || (st.giver == b && st.recipient == a)))
            ++n;
    return n >= 3 ? 0 : 1.0 / double(1 << n);
}

int SocialLedger::pay(const std::string& actor, const std::string& partner, const std::string& reason, const std::string& source,
                      int requested, double now, std::uint64_t event)
{
    int amount = std::max(0, std::min(requested, dailyCap() - usedToday(actor, now)));
    amount = std::min(amount, 2147483647 - points[actor]);
    LedgerEntry e;
    e.event = event;
    e.at = now;
    e.actor = actor;
    e.partner = partner;
    e.reason = reason;
    e.amount = amount;
    e.session = source;
    add(e);                               // (A zero receipt too: the same source can't pay later.)
    points[actor] += amount;
    return amount;
}

SocialResult SocialLedger::star(const std::string& giver, const std::string& recipient, const std::string& session, double now)
{
    const auto it = sessions.find(session);
    if (it == sessions.end() || it->second.ended <= 0)
        return {false, "That scene hasn't ended."};
    if (giver == recipient)
        return {false, "Not to yourself."};
    // Both must have qualified in it (a settlement receipt, even one capped to nothing).
    const auto qualified = [&](const std::string& who) {
        for (const auto i : receiptsOf(who))
            if (entries[i].session == session && entries[i].reason == "qualified_session_settlement")
                return true;
        return false;
    };
    if (!qualified(giver))
        return {false, "Only those who took part may give a star."};
    if (!qualified(recipient))
        return {false, "They didn't take part in that scene."};
    const int earned = paidFor(recipient, session);
    for (const auto& st : stars)
        if (st.kind == "gold" && st.source == session && st.giver == giver && st.recipient == recipient)
            return {false, "You've already given them a star for that scene."};   // One to each, as many as took part.
    if (now - it->second.ended > 86400)
        return {false, "That scene was too long ago."};
    int given = 0;
    for (const auto& st : stars)
        given += st.giver == giver && now - st.at < 86400;
    const double decay = given >= 10 ? 0 : pairDecay(giver, recipient, now);
    const int amount = pay(recipient, giver, "gold_star", session, int(std::floor(std::min(2, earned) * decay)), now,
                           std::hash<std::string>{}("gold|" + session + "|" + giver));
    stars.push_back({giver, recipient, session, "gold", now, amount});
    return {true, {}, amount};
}

const SocialStory* SocialLedger::storyOf(const std::string& session) const
{
    for (const auto& [id, st] : stories)
        if (st.state != "expired" && std::find(st.scenes.begin(), st.scenes.end(), session) != st.scenes.end())
            return &st;
    return nullptr;
}

SocialResult SocialLedger::propose(const std::string& owner, const std::string& session, const std::string& name, double now)
{
    const auto it = sessions.find(session);
    if (it == sessions.end() || it->second.ended <= 0)
        return {false, "Only an ended scene can begin a Story."};
    if (name.empty() || name.size() > 60)
        return {false, "Give the Story a name (up to 60 letters)."};
    const auto paid = paidIn(session);
    if (paidFor(owner, session) <= 0 || paid.size() < 2)
        return {false, "A Story begins with a scene that paid you and at least one other."};
    if (storyOf(session))
        return {false, "That scene is already part of a Story."};
    int open = 0;
    for (const auto& [id, st] : stories)
        open += st.members.count(owner) && (st.state == "pending" || st.state == "active");
    if (open >= 8)
        return {false, "You're in too many open Stories."};
    SocialStory st;
    st.id = "story-" + std::to_string(nextStory++);
    st.name = name;
    st.owner = owner;
    st.created = st.last = now;
    st.scenes.push_back(session);
    st.members.insert(paid.begin(), paid.end());
    st.approvals.insert(owner);
    if (st.approvals.size() * 3 >= st.members.size() * 2)
        st.state = "active";
    stories[st.id] = st;
    return {true, st.id};
}

SocialResult SocialLedger::approve(const std::string& member, const std::string& id, double now)
{
    auto it = stories.find(id);
    if (it == stories.end() || it->second.state != "pending")
        return {false, "There's no Story waiting for your word."};
    auto& st = it->second;
    if (!st.members.count(member))
        return {false, "You weren't part of it."};
    st.approvals.insert(member);
    st.last = now;
    if (st.approvals.size() * 3 >= st.members.size() * 2)
        st.state = "active";
    return {true, st.state};
}

SocialResult SocialLedger::extend(const std::string& owner, const std::string& id, const std::string& session, double now)
{
    auto it = stories.find(id);
    if (it == stories.end() || it->second.state != "active")
        return {false, "That Story isn't under way."};
    auto& st = it->second;
    if (st.owner != owner)
        return {false, "Only whoever began it carries it on."};
    const auto s = sessions.find(session);
    if (s == sessions.end() || s->second.ended <= 0)
        return {false, "Only an ended scene can carry a Story on."};
    if (storyOf(session))
        return {false, "That scene is already part of a Story."};
    const auto paid = paidIn(session);
    bool shared = false;
    for (const auto& p : paid)
        shared |= st.members.count(p) > 0;
    if (paid.size() < 2 || !shared)
        return {false, "The scene must have paid two of you, and share someone with the Story."};
    if (st.scenes.size() >= 32 || st.members.size() + paid.size() > 48)
        return {false, "That Story is as long as a Story gets."};
    st.scenes.push_back(session);
    st.members.insert(paid.begin(), paid.end());
    st.last = now;
    return {true, {}};
}

SocialResult SocialLedger::close(const std::string& owner, const std::string& id, double now)
{
    auto it = stories.find(id);
    if (it == stories.end() || it->second.state != "active")
        return {false, "That Story isn't under way."};
    auto& st = it->second;
    if (st.owner != owner)
        return {false, "Only whoever began it closes it."};
    if (st.scenes.size() < 2)
        return {false, "A Story needs at least two scenes."};
    st.state = "closed";
    st.last = now;
    int total = 0;
    for (const auto& m : st.members)
    {
        int paid = 0, scenes = 0;
        for (const auto& sc : st.scenes)
            if (const int p = paidFor(m, sc); p > 0)
            {
                paid += p;
                ++scenes;
            }
        if (scenes < 2)
            continue;
        total += pay(m, "", "story_closure", st.id, paid / 4 + std::min(scenes - 1, 5), now,
                     std::hash<std::string>{}("story|" + st.id + "|" + m));
    }
    return {true, {}, total};
}

SocialResult SocialLedger::storyStar(const std::string& giver, const std::string& recipient, const std::string& id, double now)
{
    auto it = stories.find(id);
    if (it == stories.end() || it->second.state != "closed")
        return {false, "Only a closed Story can be starred."};
    auto& st = it->second;
    const auto closed = [&](const std::string& who) {
        for (const auto i : receiptsOf(who))
            if (entries[i].session == id && entries[i].reason == "story_closure")
                return true;
        return false;
    };
    if (giver == recipient || !closed(giver) || !closed(recipient))
        return {false, "Only between two who saw it through."};
    if (st.starred.count(giver))
        return {false, "You've given your star for this Story."};
    int given = 0;
    for (const auto& s : stars)
        given += s.giver == giver && now - s.at < 86400;
    const double decay = given >= 10 ? 0 : pairDecay(giver, recipient, now);
    const int amount = pay(recipient, giver, "story_star", id, int(std::floor(4 * decay)), now,
                           std::hash<std::string>{}("storystar|" + id + "|" + giver));
    st.starred.insert(giver);
    stars.push_back({giver, recipient, id, "story", now, amount});
    return {true, {}, amount};
}
} // namespace ratw
