#pragma once
#include "CoreMinimal.h"

struct FRatwDialogueContext
{
    FString NpcId, Name, Description, Activity, PlayerName, HeardText, Memory, Scene;
    // Memory is what the language model is given (MemoryStore::recallForDialogue); Recollection is the short form
    // the authored offline replies quote (MemoryStore::recall). Either may be empty.
    FString Recollection;
    FString Environment; // Local authoritative conditions, not inferred from dialogue.
    FString Greeting;    // Authored offline line for world-file residents; may be empty.
    FString Personality, Backstory; // Roster character sheet for the live backend; may be empty.
    // For the NPC Mind (tools/npc_mind.py): the speaker's ID when the NPC recognises them (empty otherwise), how the
    // NPC regards them (Bonds::describe), and how the NPC felt after its last reply.
    FString SubjectId, Relationship, Mood;
};

// A reply as the NPC Mind gives it; from the authored fallback or a text-only provider, just the text. Every field has
// been checked here: the emotion is one of a few words, the nudges are -3..3, the notes are short.
struct FRatwDialogueReply
{
    FString Text;
    bool Generated = false;          // From the provider (false: the authored line).
    FString Emotion;                 // Empty when not given.
    int32 Affinity = 0, Trust = 0;   // How the exchange moves the NPC's liking and trust for the speaker.
    FString Remember;                // A short private note about the speaker, or empty.
    FString PromiseBy, Promise;      // "npc" or "player" and what was promised, or both empty.
};

// Provider-neutral local endpoint protocol. A provider returns prose only;
// authoritative commands are never accepted from its response.
class FRatwDialogueProvider
{
  public:
    void Configure(const FString& Endpoint);
    FString Label() const;
    void Reply(const FRatwDialogueContext& Context, TFunction<void(FString)> Completion);
    // Reply(), with everything the provider said about the exchange (see FRatwDialogueReply).
    void Converse(const FRatwDialogueContext& Context, TFunction<void(const FRatwDialogueReply&)> Completion);
    // A finished conversation summarised from the NPC's point of view (POST .../summarize beside .../dialogue):
    // Completion gets the summary, or an empty string if there is no such service or it failed.
    void Summarize(const FString& NpcName, const TArray<TPair<FString, FString>>& Turns, TFunction<void(FString)> Completion);
    static FString AuthoredReply(const FRatwDialogueContext& Context);

  private:
    FString LocalEndpoint;
};
