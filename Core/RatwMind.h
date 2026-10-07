#pragma once
// The NPC Mind's client (tools/npc_mind.py), for both servers. A loopback HTTP endpoint only; requests go on worker threads and their answers are
// handed back on the game thread by poll(). Without an endpoint, or when it fails, NPCs answer with authored lines.
#include "RatwJsonDoc.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace ratw::mind
{
struct Context
{
    std::string npcId, name, description, activity, playerName, heardText, memory, scene;
    std::string recollection;                  // The short form the authored replies quote (MemoryStore::recall).
    std::string environment, greeting, personality, backstory;
    std::string subjectId, relationship, mood;
    std::string seen;                          // What it can see of the speaker, in their player's words (doc 50).
};

// Every field checked: the emotion one of a few words, the nudges -3..3, the notes short.
struct Reply
{
    std::string text;
    bool generated = false;
    std::string emotion;
    int affinity = 0, trust = 0;
    std::string remember, promiseBy, promise;
};

// Two NPCs overheard (the ambient director, Phase 10): who they are, how each sees the other, and the topic's facts.
struct Persona
{
    std::string name, description, personality;
};
struct ExchangeContext
{
    Persona a, b;                              // `a` opens.
    std::string aSeesB, bSeesA, kind, scene;   // kind: "gossip", "news", "quarrel", "friends", "day".
    std::vector<std::string> facts;
    std::string subjectName, claim, news, day; // For the authored lines: the rumour, the news (as told), the day.
};
struct Exchange
{
    std::vector<std::pair<int, std::string>> lines;   // 0: a says, 1: b says.
    bool generated = false;
};

// News kept in the third person ("Sorrel married Rook"), as `teller` says it ("I married Rook").
std::string firstPerson(std::string text, const std::string& teller);

// Text cut to at most `units` characters (UTF-16 units, as the Unreal runtime counts), never inside a character.
std::string left(const std::string& text, std::size_t units);
std::string trim(const std::string& text);
std::string lower(const std::string& text);       // ASCII letters only.

class Client
{
  public:
    Client() = default;
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    ~Client();
    // Only http://127.0.0.1:port/... or http://localhost:port/...; anything else leaves NPCs to authored lines.
    void configure(const std::string& endpoint);
    bool live() const { return !host_.empty(); }
    std::string label() const;
    static std::string authoredReply(const Context& c);
    void converse(const Context& c, std::function<void(const Reply&)> done);
    // A few lines between two NPCs; authored ones when there is no NPC Mind, or it fails.
    static Exchange authoredExchange(const ExchangeContext& c);
    void exchange(const ExchangeContext& c, std::function<void(const Exchange&)> done);
    // The game's own answer put in the NPC's voice by the small model (doc 28): the status (0 without an NPC Mind)
    // and the words, "" when it can't (polishing off, a fact lost, a failure): the game's own words stand then.
    void polish(const std::string& npc, const std::string& personality, const std::string& mood, const std::string& reply,
                std::function<void(int status, const std::string& text)> done);
    // A finished conversation summarised from the NPC's point of view; "" if there is no such service or it failed.
    void summarize(const std::string& npcName, const std::vector<std::pair<std::string, std::string>>& turns,
                   std::function<void(const std::string&)> done);
    // A scene recapped for one player (doc 50, Phase 4) from only the lines its wolf perceived, each by who as it knew
    // them: 2-4 sentences in the second person; "" if there is no such service, it failed, or its budgets are spent.
    void recap(const std::string& place, const std::string& you, int minutes,
               const std::vector<std::pair<std::string, std::string>>& lines, std::function<void(const std::string&)> done);
    // Runs the completions of answers that have arrived (on the caller's thread: the game's).
    void poll();
    // Waits (at most `seconds`) until nothing is on its way, then runs what arrived: for tests and shutdown.
    void settle(double seconds);

  private:
    struct Job
    {
        std::string path, body;
        double timeout = 8;
        std::function<void(int status, const std::string& body)> done;
    };
    std::string host_, path_;
    int port_ = 0;
    std::mutex lock_;
    std::condition_variable wake_;
    std::deque<Job> jobs_;
    std::vector<std::function<void()>> finished_;
    std::vector<std::thread> workers_;
    int busy_ = 0;
    bool stopping_ = false;
    void post(Job job);
    void work();
};

// One HTTP/1.1 POST to a loopback port: the status (0 if it failed) and the body. For the Mind, and tests.
int httpPost(int port, const std::string& path, const std::string& body, double timeoutSeconds, std::string& response);
} // namespace ratw::mind
