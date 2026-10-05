#include "RatwSocialCore.h"
#include "RatwLevels.h"

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
    // A party's own scene, or the cell's (doc 32, 1.1): they don't merge.
    SocialSession* Scene = nullptr;
    for (auto& Pair : sessions)
        if (Pair.second.cell == post.cell && Pair.second.ended == 0 && Pair.second.party == post.party)
        {
            Scene = &Pair.second;
            break;
        }
    auto Add = [&](SocialSession& S, const SocialPost& P) {
        auto& Member = S.members[P.actor];
        if (Member.left)
            return;                                  // (Stepped out: their words count no more here.)
        if (Member.joined == 0)
            Member.joined = P.at;
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
    // Party mates in earshot of each other open their party's scene at once; no A-B-A needed.
    if (!Scene && !post.party.empty())
    {
        SocialSession S;
        S.id = "scene-" + std::to_string(post.event);
        S.cell = post.cell;
        S.party = post.party;
        S.started = post.at;
        Add(S, post);
        sessions[S.id] = S;
        return 0;
    }
    // Someone outside answering a party's scene (they heard one of it, and it heard them) joins that scene.
    if (post.party.empty())
        for (auto& Pair : sessions)
        {
            auto& S = Pair.second;
            if (S.ended != 0 || S.party.empty() || S.cell != post.cell)
                continue;
            for (const auto& Other : S.members)
                if (Other.first != post.actor && post.at - Other.second.last <= 30 &&
                    std::find(Other.second.lastAudience.begin(), Other.second.lastAudience.end(), post.actor) !=
                        Other.second.lastAudience.end() &&
                    std::find(post.audience.begin(), post.audience.end(), Other.first) != post.audience.end())
                {
                    Add(S, post);
                    return 0;
                }
        }
    if (Scene)
    {
        bool Participating = !post.party.empty();          // (A party mate in earshot: always part of the party's scene.)
        for (const auto& Other : Scene->members)
            if (Other.first != post.actor &&
                std::find(listeners.begin(), listeners.end(), Other.first) != listeners.end())
                Participating = true;
        if (Participating)
            Add(*Scene, post);
        return 0;
    }
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
            Add(S, A);
            Add(S, B);
            Add(S, post);
            sessions[S.id] = S;
            Pending.clear();
            return 0;
        }
    }
    Pending.push_back(post);
    if (Pending.size() > 16)
        Pending.erase(Pending.begin());
    return 0;
}

int SocialLedger::settle(const std::string& id, double now)
{
    auto It = sessions.find(id);
    if (It == sessions.end() || It->second.ended > 0)
        return 0;
    auto& Scene = It->second;
    Scene.ended = now;
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
    for (const auto& Entry : entries)
        if (Entry.actor == Actor && now - Entry.at < 86400)
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
    entries.push_back(Entry);
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
    }
    if (S.ended == 0 && !S.members.count(member))
        S.members[member].joined = now;
}

int SocialLedger::settleFight(const std::string& fight, const std::set<std::string>& fought, double now)
{
    auto It = sessions.find(fightScene(fight));
    if (It == sessions.end() || It->second.ended > 0)
        return 0;
    auto& Scene = It->second;
    Scene.ended = now;
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
        for (const auto& Entry : entries)
            if (Entry.actor == Actor && now - Entry.at < 86400 && Entry.amount > 0)
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
        int Amount = fought.count(Actor) ? FightXP : 0;
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
}

int SocialLedger::level(const std::string& actor) const
{
    const auto it = points.find(actor);
    return levels::levelFor(it == points.end() ? 0 : it->second);
}

int SocialLedger::restedLeft(const std::string& actor, double now) const
{
    // The times it earned (not rested pay), newest first: the latest gap of a day or more between two is time away,
    // and its rested pool runs from the return; what has been paid from it since, used up.
    std::vector<double> times;
    for (const auto& e : entries)
        if (e.actor == actor && e.amount > 0 && e.reason != "rested_bonus")
            times.push_back(e.at);
    times.push_back(now);
    std::sort(times.rbegin(), times.rend());
    for (std::size_t i = 0; i + 1 < times.size(); ++i)
        if (const double gap = times[i] - times[i + 1]; gap >= 86400)
        {
            const double back = times[i];
            int pool = std::min(RestedMost, RestedPerDay * int(gap / 86400));
            for (const auto& e : entries)
                if (e.actor == actor && e.reason == "rested_bonus" && e.at >= back)
                    pool -= e.amount;
            return std::max(0, pool);
        }
    return 0;
}

int SocialLedger::award(const std::string& actor, const std::string& kind, const std::string& source, double now)
{
    // What each kind pays, and the most a rolling day (0: no limit of its own) (doc 44).
    struct Kind
    {
        const char* name;
        int amount, perDay;
    };
    static constexpr Kind Kinds[] = {{"work", 10, 30}, {"practice", 5, 15}, {"milestone", 10, 0}, {"discovery", 5, 25}, {"story", 25, 0}};
    const Kind* k = nullptr;
    for (const auto& c : Kinds)
        if (kind == c.name)
            k = &c;
    if (!k || actor.empty() || source.empty())
        return 0;
    int today = 0;
    for (const auto& e : entries)
    {
        if (e.actor == actor && e.reason == kind && e.session == source)
            return 0;                               // (Once for each source.)
        if (e.actor == actor && e.reason == kind && now - e.at < 86400)
            today += e.amount;
    }
    const int wanted = k->perDay ? std::min(k->amount, std::max(0, k->perDay - today)) : k->amount;
    if (wanted <= 0 || usedToday(actor, now) >= DailyCap)
        return -1;                                  // Not today: no receipt, so it can still be paid another day.
    return pay(actor, "", kind, source, wanted, now, 0);
}

// ------------------------------------------------------------------ Gold Stars and Stories (doc 32, 1.2)

std::string socialTitle(int level)
{
    return level >= 25 ? "Legend" : level >= 18 ? "Renowned" : level >= 12 ? "Notable" : level >= 8 ? "Respected" : level >= 5 ? "Familiar Face"
         : level >= 3 ? "Known" : "Stranger";
}

int SocialLedger::paidFor(const std::string& actor, const std::string& session) const
{
    int n = 0;
    for (const auto& e : entries)
        if (e.actor == actor && e.session == session && e.reason == "qualified_session_settlement")
            n += e.amount;
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
    for (const auto& e : entries)
        if (e.actor == actor && now - e.at < 86400 && e.reason != "rested_bonus")   // (Rested XP is outside the cap.)
            n += e.amount;
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
    // Rested XP (doc 44): back after a day or more, what is earned is paid again from the rested pool, outside the cap.
    const int rested = requested > 0 ? restedLeft(actor, now) : 0;
    int amount = std::max(0, std::min(requested, DailyCap - usedToday(actor, now)));
    amount = std::min(amount, 2147483647 - points[actor]);
    LedgerEntry e;
    e.event = event;
    e.at = now;
    e.actor = actor;
    e.partner = partner;
    e.reason = reason;
    e.amount = amount;
    e.session = source;
    entries.push_back(e);                 // (A zero receipt too: the same source can't pay later.)
    points[actor] += amount;
    if (const int bonus = std::min({rested, amount, 2147483647 - points[actor]}); bonus > 0)
    {
        e.reason = "rested_bonus";
        e.amount = bonus;
        entries.push_back(e);
        points[actor] += bonus;
        amount += bonus;
    }
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
        for (const auto& e : entries)
            if (e.actor == who && e.session == session && e.reason == "qualified_session_settlement")
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
        for (const auto& e : entries)
            if (e.actor == who && e.session == id && e.reason == "story_closure")
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
