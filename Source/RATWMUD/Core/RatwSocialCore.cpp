#include "RatwSocialCore.h"

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
        if (segment.kind != "speech" && segment.kind != "narration")
            continue;
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
                    ++result.words;
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
    SocialSession* Scene = nullptr;
    for (auto& Pair : sessions)
        if (Pair.second.cell == post.cell && Pair.second.ended == 0)
        {
            Scene = &Pair.second;
            break;
        }
    auto Add = [&](SocialSession& S, const SocialPost& P) {
        auto& Member = S.members[P.actor];
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
    if (Scene)
    {
        bool Participating = false;
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
        if (Pair.second.turns >= 2 && Pair.second.words >= 35 && Pair.second.replies >= 1)
            Qualified.push_back(Pair.first);
    if (Qualified.size() < 2)
        return 0;
    std::sort(Qualified.begin(), Qualified.end(), [&](const std::string& A, const std::string& B) {
        return Scene.members[A].joined == Scene.members[B].joined ? A < B
                                                                  : Scene.members[A].joined < Scene.members[B].joined;
    });
    int Total = 0;
    for (std::size_t Index = 0; Index < Qualified.size(); ++Index)
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
                    if (Other.first == id || Other.second.ended <= 0 || now - Other.second.ended >= 86400)
                        continue;
                    auto A = Other.second.members.find(Actor), B = Other.second.members.find(Peer);
                    if (A != Other.second.members.end() && B != Other.second.members.end() && A->second.turns >= 2 &&
                        A->second.words >= 35 && A->second.replies >= 1 && B->second.turns >= 2 &&
                        B->second.words >= 35 && B->second.replies >= 1)
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
        Entry.session = id;
        entries.push_back(Entry);
        points[Actor] += Amount;
        Total += Amount;
    }
    return Total;
}
int SocialLedger::endFor(const std::string& actor, double now)
{
    int Amount = 0;
    for (auto& Pair : sessions)
        if (Pair.second.ended == 0 && Pair.second.members.count(actor))
            Amount += settle(Pair.first, now);
    return Amount;
}
void SocialLedger::tick(double now)
{
    for (auto& Pair : sessions)
        if (Pair.second.ended == 0 && now - Pair.second.last >= 1800)
            settle(Pair.first, now);
}

int SocialLedger::level(const std::string& actor) const
{
    const auto it = points.find(actor);
    return 1 + (it == points.end() ? 0 : it->second / 100);
}
} // namespace ratw
