#include "RatwSocialCore.h"
#include <cstdlib>
#include <iostream>
#include <string>

int Checks = 0;
void Check(bool Good, const char* Message)
{
    ++Checks;
    if (!Good)
    {
        std::cerr << "FAIL: " << Message << '\n';
        std::exit(1);
    }
}

int main()
{
    using namespace ratw;
    auto Post = parsePost("\"I have never heard of that.\" Jason said. /sigh \"And I don't want to.\"");
    Check(Post.ok && Post.segments.size() == 4 && Post.speech, "mixed speech/action/narration parsed in order");
    Check(Post.segments[1].kind == "narration" && Post.segments[2].kind == "action",
          "segment kinds preserve narration and action");
    Check(!parsePost("/delete all").ok, "unknown command rejected");
    Check(parsePost("//delete is literal").ok, "escaped slash supported");
    Check(!parsePost("\"unterminated").ok, "unterminated speech rejected");
    Check(parsePost("/me resting beside the hearth").hasState, "declared state recognized");
    Check(parsePost("/sit").posture == "sitting", "posture is state");
    Check(!parsePost(std::string(32769, 'x')).ok, "long message bound");
    Check(roleplayEvidence(parsePost("/action stretches slowly beside the warming fire")).words == 0,
          "slash action excluded from reward evidence per source policy");
    Check(roleplayEvidence(parsePost("/me resting beside a warming fire")).words == 0,
          "persistent state excluded from reward evidence");
    Check(roleplayEvidence(parsePost("Don't lose the rain-soaked satchel")).words == 5,
          "apostrophes and hyphens remain internal to words");
    Check(roleplayEvidence(parsePost("HELLO   there friend")).contentHash ==
              roleplayEvidence(parsePost("hello there friend")).contentHash,
          "duplicate hash normalizes case and whitespace");
    std::string LongRoleplay;
    for (int I = 0; I < 600; ++I)
        LongRoleplay += "story ";
    Check(roleplayEvidence(parsePost(LongRoleplay)).words == 500,
          "long-form prose remains eligible with capped contribution metadata");
    const auto Mask = maskWords("one two three four five six seven eight nine ten", 0.5, 1234);
    Check(Mask == maskWords("one two three four five six seven eight nine ten", 0.5, 1234),
          "perception masking deterministic");
    Check(maskWords("a secret", 0, 9) == "...", "lost speech marker");
    Check(perceivePost(Post, 0, 0, 12).empty(), "wholly unperceived event omitted");
    auto Heard = perceivePost(Post, 1, 0, 12);
    Check(Heard[1].text == "···", "unseen action uses visual marker");

    MemoryStore M;
    M.record("rowan", "ash", {1, 1000, "ash", "I promise to bring back the blue scarf."});
    Check(M.consolidate(4599) == 0, "memory not consolidated before one hour idle");
    M.record("rowan", "ash", {2, 4599, "ash", "We are still talking."});
    Check(M.consolidate(4600) == 0, "new activity resets inactivity timer");
    Check(M.consolidate(8199) == 1, "exactly one hour inactivity consolidates");
    Check(M.summaries.size() == 1 && M.active.empty(), "active memory becomes permanent summary");
    Check(M.consolidate(9000) == 0 && M.summaries.size() == 1, "consolidation idempotent");
    Check(M.recall("rowan", "ash").find("blue scarf") != std::string::npos, "earlier promise remembered");
    for (std::uint64_t I = 0; I < 80; ++I)
        M.record("rowan", "birch",
                 {100 + I, 10000 + double(I), "birch",
                  I == 0 ? "I promise to return the amber bead." : "Another detail of our conversation."});
    Check(M.active["rowan|birch"].turns.size() == 32, "active full-detail window bounded");
    Check(M.consolidate(13679) == 1, "large conversation consolidates after inactivity");
    Check(M.summaries.back().sourceEvents.size() == 80, "all source event IDs retained across compaction");
    Check(M.summaries.back().text.find("amber bead") != std::string::npos,
          "early commitment survives long conversation");
    M.record("rowan", "birch", {100, 10000, "birch", "duplicate old event"});
    Check(M.active.empty(), "consolidated source event cannot be replayed into new memory");
    auto Copy = M;
    Check(Copy.consolidate(999999) == 0 && Copy.summaries.size() == 2, "restored summaries never expire");

    // What an NPC brings to a reply: both sides of the conversation, and the ends of earlier ones.
    MemoryStore D;
    D.record("moss", "fen", {1, 100, "Fen", "Do you still bake rye on market days?"});
    D.record("moss", "fen", {2, 101, "moss", "Every market day, before dawn."});
    D.record("moss", "fen", {3, 102, "Fen", "Save me a loaf. I will pay you back on Thursday."});
    Check(D.consolidate(10000) == 1, "first conversation consolidates");
    D.record("moss", "fen", {4, 20000, "Fen", "It is Thursday. Here is what I owe."});
    D.record("moss", "fen", {5, 20001, "moss", "I kept the loaf by the oven."});
    const auto Recalled = D.recallForDialogue("moss", "fen");
    Check(Recalled.find("You: “I kept the loaf by the oven.”") != std::string::npos, "the NPC's own words are recalled");
    Check(Recalled.find("Fen: “It is Thursday.") != std::string::npos, "the subject's words are recalled");
    Check(Recalled.find("pay you back on Thursday") != std::string::npos, "the end of an earlier conversation is recalled");
    Check(Recalled.find("Conversation record.") == std::string::npos, "summary boilerplate is left out");
    Check(Recalled.find("An earlier conversation") < Recalled.find("This conversation so far"),
          "earlier conversations come before the current one");
    Check(D.recallForDialogue("ash", "fen").empty(), "another NPC recalls nothing of it");
    MemoryStore Long;
    for (std::uint64_t I = 0; I < 40; ++I)
        Long.record("moss", "fen", {I + 1, 100 + double(I), I % 2 ? "moss" : "Fen", std::string(250, char('a' + I % 26))});
    for (int Pass = 0; Pass < 5; ++Pass)
    {
        Long.consolidate(1e9);
        for (std::uint64_t I = 0; I < 40; ++I)
            Long.record("moss", "fen", {1000 * (Pass + 2) + I, 1e9 + Pass * 1e4 + double(I), I % 2 ? "moss" : "Fen",
                                        std::string(250, char('a' + I % 26)) + " ünïcödé"});
    }
    MemoryStore S;
    S.record("wren", "ash", {1, 100, "Ash", "I promise to bring flour on Thursday."});
    Check(S.due(3699).empty() && S.due(3700).size() == 1 && S.due(3700)[0].turns.size() == 1,
          "the conversations about to close are known beforehand");
    const auto Closing = S.due(3700)[0].id;
    Check(S.consolidate(3700) == 1 && S.rewrite(Closing, "Ash promised flour for Thursday."),
          "a closed conversation's summary can be rewritten");
    Check(!S.rewrite("memory-none", "x") && !S.rewrite(Closing, ""), "only a real summary, and not with nothing");
    Check(S.summaries[0].sourceEvents.size() == 1, "rewriting keeps the sources");
    const auto Better = S.recallForDialogue("wren", "ash");
    Check(Better.find("Ash promised flour for Thursday.") != std::string::npos && Better.find("Summary.") == std::string::npos,
          "the better summary is what is recalled");
    const auto Bounded = Long.recallForDialogue("moss", "fen", 3600);
    Check(Bounded.size() <= 3600, "dialogue recall stays within its budget");
    Check(Bounded.find("Earlier in this conversation") != std::string::npos, "a long conversation notes what came before");

    SocialLedger L;
    Check(L.record({1, 1000, "a", "tavern", 20, false, 100}, {"a", "b"}) == 0, "speech earns no direct XP");
    L.record({2, 1003, "b", "tavern", 20, false, 101}, {"a", "b"});
    L.record({3, 1006, "a", "tavern", 20, false, 102}, {"a", "b"});
    Check(L.sessions.size() == 1 && L.points.empty(), "A-B-A forms scene without award");
    L.record({4, 1009, "b", "tavern", 20, false, 103}, {"a", "b"});
    Check(L.endFor("a", 1010) == 40, "two qualified contributors settle 20 XP each");
    Check(L.points["a"] == 20 && L.points["b"] == 20, "account totals authoritative");
    Check(L.endFor("a", 1011) == 0 && L.entries.size() == 2, "settlement replay safe");
    L.record({5, 1020, "a", "tavern", 20, true, 104}, {"a", "b"});
    Check(L.points["a"] == 20, "OOC excluded");
    L.record({6, 1100, "a", "tavern", 20, false, 105}, {"a", "b"});
    L.record({7, 1103, "b", "tavern", 20, false, 106}, {"a", "b"});
    L.record({8, 1106, "a", "tavern", 20, false, 107}, {"a", "b"});
    L.record({9, 1109, "b", "tavern", 20, false, 108}, {"a", "b"});
    Check(L.endFor("b", 1110) == 20, "repeated pair settlement halves reward");
    SocialLedger Solo;
    for (int I = 0; I < 10; ++I)
        Solo.record({std::uint64_t(I + 1), double(2000 + I * 3), "solo", "tavern", 100, false, std::uint64_t(I + 1)},
                    {"solo"});
    Check(Solo.sessions.empty() && Solo.entries.empty(), "solo/NPC-only activity cannot qualify");
    SocialLedger Duplicate;
    Duplicate.record({1, 1000, "a", "tavern", 40, false, 123}, {"a", "b"});
    Duplicate.record({2, 1003, "a", "tavern", 40, false, 123}, {"a", "b"});
    Check(Duplicate.recent["a"].event == 1, "duplicate evidence suppressed");
    SocialLedger Asymmetric;
    Asymmetric.record({1, 1000, "a", "tavern", 20, false, 1}, {"a"});
    Asymmetric.record({2, 1003, "b", "tavern", 20, false, 2}, {"a", "b"});
    Asymmetric.record({3, 1006, "a", "tavern", 20, false, 3}, {"a", "b"});
    Check(Asymmetric.sessions.empty(), "later listeners cannot retroactively hear old candidate turn");
    SocialLedger Reverse;
    Reverse.record({1, 1000, "a", "tavern", 20, false, 1}, {"a", "b"});
    Reverse.record({2, 1003, "b", "tavern", 20, false, 2}, {"b"});
    Reverse.record({3, 1006, "a", "tavern", 20, false, 3}, {"a", "b"});
    Check(Reverse.sessions.empty(), "reply requires current author to have heard previous speaker");
    SocialLedger Disconnect;
    Disconnect.record({1, 1000, "a", "tavern", 20, false, 1}, {"a", "b"});
    Disconnect.record({2, 1003, "b", "tavern", 20, false, 2}, {"a", "b"});
    Disconnect.record({3, 1006, "a", "tavern", 20, false, 3}, {"a"});
    Check(Disconnect.sessions.empty(), "disconnected counterpart cannot complete automatic scene");
    SocialLedger Replies;
    Replies.record({1, 1000, "a", "tavern", 20, false, 1}, {"a", "b"});
    Replies.record({2, 1003, "b", "tavern", 20, false, 2}, {"a", "b"});
    Replies.record({3, 1006, "a", "tavern", 20, false, 3}, {"a", "b"});
    Replies.record({4, 1009, "b", "tavern", 20, false, 4}, {"b"});
    Check(Replies.sessions.begin()->second.members["b"].turns == 1,
          "alone post does not create eligible scene evidence");
    std::cout << "Runtime core: " << Checks << " checks passed\n";
}
