#include "Testing/RatwCharacterScenario.h"
#include "Runtime/RatwPlayerController.h"
#include "Runtime/RatwJson.h"
#include "UI/SRatwFrontDoor.h"
#include "UI/SRatwGame.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "UnrealClient.h"

namespace
{
using namespace ratwjson;
Object DecodeCharacterJson(const FString& Text)
{
    Object O;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), O);
    return O;
}
ratw::Appearance Choice(int Index)
{
    ratw::Appearance A;
    const char* Species[] = {"timber", "maned", "arctic", "red", "ethiopian", "timber"};
    A.species = Species[FMath::Clamp(Index, 0, 5)];
    A.sex = Index % 2 ? "male" : "female";
    A.stature = Index == 1 ? "tall" : Index == 3 || Index == 5 ? "short" : "average";
    A.baseColor = Index == 2 ? 0 : Index == 0 ? 7 : Index == 5 ? 5 : 6;
    A.gradientColor = Index == 2 ? 1 : 0;
    A.markingColor = 5;
    A.gradientAmount = .45; A.patternAmount = Index == 2 ? .15 : .6;
    A.pattern = Index == 0 ? "mantle" : Index == 1 ? "solid" : Index == 3 ? "piebald" : "saddle";
    return A;
}
const TCHAR* Names[] = {TEXT("Lark"), TEXT("Sorrel"), TEXT("Frost"), TEXT("Briar"), TEXT("Ember"), TEXT("Flint")};
const int Ages[] = {27, 22, 68, 15, 9, 45};
}

bool TickRatwCharacterScenario(ARatwPlayerController* C, const FString& Scenario, const FString& Role,
                               const FString& Output)
{
    using namespace ratwjson;
    static double Start = FPlatformTime::Seconds(), At = Start;
    static int Step = 0, CreateIndex = 0, CapturedStep = -1;
    static FString FirstId, OtherId;
    static TArray<FString> Checks;
    const double Now = FPlatformTime::Seconds();
    const auto Lobby = C->GetLatestLobby();
    const auto Snapshot = DecodeCharacterJson(C->GetLatestSnapshotJson());
    const auto Self = Child(Snapshot, TEXT("self"));
    const Array* Roster = nullptr;
    if (Lobby) Lobby->TryGetArrayField(TEXT("characters"), Roster);
    const auto Send = [&](const Object& J) { C->SubmitCommand(Encode(J)); };
    const auto Command = [&](const TCHAR* Type) { auto J = New(); J->SetStringField(TEXT("type"), Type); Send(J); };
    const auto Auth = [&](bool Register, const TCHAR* User, bool Correct = true) {
        auto J = New(); J->SetStringField(TEXT("type"), Register ? TEXT("auth_register") : TEXT("auth_login"));
        J->SetStringField(TEXT("username"), User);
        // Synthetic, public fixture credential: never a user's password or command-line secret.
        J->SetStringField(TEXT("password"), Correct ? TEXT("RATW-fixture-only-2026!") : TEXT("Incorrect-fixture-2026!")); Send(J);
    };
    const auto Enter = [&](const FString& Id) { auto J = New(); J->SetStringField(TEXT("type"), TEXT("character_enter")); J->SetStringField(TEXT("id"), Id); Send(J); };
    const auto Create = [&](int Index, bool Invalid = false) {
        auto J = New(); J->SetStringField(TEXT("type"), TEXT("character_create"));
        J->SetStringField(TEXT("name"), Names[Index]); J->SetNumberField(TEXT("age"), Invalid ? 100 : Ages[Index]);
        J->SetStringField(TEXT("commandId"), FString::Printf(TEXT("fixture-create-%s-%d-%s"), *Role, Index, Invalid ? TEXT("invalid") : TEXT("valid")));
        J->SetObjectField(TEXT("appearance"), Appearance(Choice(Index))); Send(J);
    };
    const auto Shot = [&](const TCHAR* Filename) {
        if (FParse::Param(FCommandLine::Get(), TEXT("RatwCaptureCharacters")))
        {
            IFileManager::Get().MakeDirectory(*Output, true);
            FScreenshotRequest::RequestScreenshot(Output / Filename, true, false, false, FIntRect(), true);
        }
    };
    const auto Next = [&](const FString& Check = FString()) {
        if (!Check.IsEmpty()) Checks.Add(Check);
        ++Step; At = Now;
    };
    const auto CaptureBeforeTransition = [&](const TCHAR* Filename) {
        if (!FParse::Param(FCommandLine::Get(), TEXT("RatwCaptureCharacters")) || CapturedStep == Step) return false;
        Shot(Filename); CapturedStep = Step; At = Now;
        // Screenshot requests render at the end of the frame, before a later transition.
        return true;
    };
    const auto Finish = [&](bool Good, const FString& Detail) {
        auto Result = New(); Result->SetBoolField(TEXT("passed"), Good);
        Result->SetStringField(TEXT("scenario"), Scenario); Result->SetStringField(TEXT("role"), Role);
        Result->SetStringField(TEXT("detail"), Detail); Result->SetNumberField(TEXT("step"), Step);
        Result->SetStringField(TEXT("firstId"), FirstId);
        Array Items; for (const auto& Check : Checks) Items.Add(V(Check)); Result->SetArrayField(TEXT("checks"), Items);
        if (Snapshot) Result->SetObjectField(TEXT("lastSnapshot"), Snapshot);
        if (Lobby) Result->SetObjectField(TEXT("lastLobby"), Lobby);
        IFileManager::Get().MakeDirectory(*Output, true);
        FFileHelper::SaveStringToFile(Encode(Result), *(Output / (Scenario + TEXT("-") + Role + TEXT(".json"))), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        UE_LOG(LogTemp, Display, TEXT("RATW_CHARACTERS %s step=%d %s"), Good ? TEXT("PASS") : TEXT("FAIL"), Step, *Detail);
        FPlatformMisc::RequestExitWithStatus(false, Good ? 0 : 1);
        return false;
    };
    if (Now - Start > 100) return Finish(false, TEXT("Character flow timed out"));
    if (Now - At < .8) return true;
    if (Scenario != TEXT("characters-create"))
    {
        if (Step == 0) { Auth(false, Role == TEXT("birch") ? TEXT("portrait_b") : TEXT("portrait_a")); Next(); }
        else if (Step == 1 && Roster && Roster->Num() > 0 && Bool(Lobby, TEXT("ok")))
        {
            FirstId = String((*Roster)[0]->AsObject(), TEXT("id")); Enter(FirstId); Next(TEXT("Saved owned roster restored after process restart"));
        }
        else if (Step == 2 && Scenario == TEXT("characters-duplicate"))
        {
            if (Self || Bool(Lobby, TEXT("ok"))) return Finish(false, TEXT("Duplicate active ownership was accepted"));
            return Finish(true, TEXT("A second session cannot occupy an already-connected character"));
        }
        else if (Step == 2 && Self)
        {
            const auto A = ReadAppearance(Child(Self, TEXT("appearance")));
            if (!ratw::validAppearance(A) || A.species != (Role == TEXT("birch") ? "arctic" : "timber"))
                return Finish(false, TEXT("Owned appearance was lost on restart"));
            Next(TEXT("Persisted avatar restored in authoritative self snapshot"));
        }
        else if (Step == 3 && Scenario == TEXT("characters-hold"))
        {
            if (Now - At > 48) return Finish(true, TEXT("Persisted account character held for independent peer inspections"));
        }
        else if (Step == 3)
        {
            const Array* Entities = nullptr;
            if (!Snapshot->TryGetArrayField(TEXT("entities"), Entities)) return true;
            for (const auto& Item : *Entities)
            {
                const auto E = Item->AsObject();
                if (!Bool(E, TEXT("npc")) && String(E, TEXT("id")) != String(Self, TEXT("id")))
                {
                    OtherId = String(E, TEXT("id"));
                    auto J = New(); J->SetStringField(TEXT("type"), TEXT("action"));
                    J->SetStringField(TEXT("action"), TEXT("inspect")); J->SetStringField(TEXT("target"), OtherId); Send(J);
                    Next(TEXT("Independent client sees the other player's public appearance")); break;
                }
            }
        }
        else if (Step == 4)
        {
            for (const auto& Text : C->GetReceivedEvents())
            {
                const auto E = DecodeCharacterJson(Text);
                if (String(E, TEXT("type")) == TEXT("inspect"))
                {
                    if (!ratw::validAppearance(ReadAppearance(Child(E, TEXT("appearance")))) || E->HasField(TEXT("age")))
                        return Finish(false, TEXT("Inspection appearance missing or private exact age leaked"));
                    Shot(TEXT("39-other-player-inspection.png")); Next(TEXT("Inspection delivers appearance + public life stage without exact age")); break;
                }
            }
        }
        else if (Step == 5) return Finish(true, TEXT("Network login, persisted avatar, and other-player inspection passed"));
        return true;
    }
    if (Step == 0)
    {
        if (CaptureBeforeTransition(TEXT("33-character-login.png"))) return true;
        auto J = New(); J->SetStringField(TEXT("type"), TEXT("hello")); J->SetStringField(TEXT("identity"), TEXT("ash")); Send(J);
        Command(TEXT("move")); Next();
    }
    else if (Step == 1)
    {
        if (Self) return Finish(false, TEXT("Unauthenticated command obtained a world entity"));
        Auth(true, TEXT("portrait_a")); Next(TEXT("Default login rejects unauthenticated development identity and movement"));
    }
    else if (Step == 2 && Roster && Roster->Num() == 0 && Bool(Lobby, TEXT("ok")))
    {
        if (auto UI = C->GetFrontDoorWidget()) UI->SetCharacterDraft(TEXT("Lark"), Ages[0], Appearance(Choice(0)));
        Next(TEXT("Local account registration persisted before empty roster acknowledgement"));
    }
    else if (Step == 3) { if (CaptureBeforeTransition(TEXT("34-character-creator.png"))) return true; Create(0, true); Next(); }
    else if (Step == 4)
    {
        if (!Roster || Roster->Num() || Bool(Lobby, TEXT("ok"))) return Finish(false, TEXT("Invalid creation was accepted"));
        Create(CreateIndex); Next(TEXT("Invalid age is rejected without creating a character"));
    }
    else if (Step == 5 && Roster && Roster->Num() == CreateIndex + 1 && Bool(Lobby, TEXT("ok")))
    {
        if (CreateIndex == 0) FirstId = String((*Roster)[0]->AsObject(), TEXT("id"));
        ++CreateIndex;
        if (CreateIndex < 6) { Create(CreateIndex); At = Now; }
        else { Shot(TEXT("35-character-roster.png")); Create(0); Next(TEXT("All five species and all four age stages saved across six owned slots")); }
    }
    else if (Step == 6)
    {
        if (!Roster || Roster->Num() != 6 || !Bool(Lobby, TEXT("ok"))) return Finish(false, TEXT("Idempotent creation did not replay safely"));
        auto J = New(); J->SetStringField(TEXT("type"), TEXT("character_create")); J->SetStringField(TEXT("name"), TEXT("Overflow"));
        J->SetNumberField(TEXT("age"), 18); J->SetObjectField(TEXT("appearance"), Appearance(Choice(0))); Send(J);
        Next(TEXT("Retrying a creation request does not duplicate the character"));
    }
    else if (Step == 7)
    {
        if (!Roster || Roster->Num() != 6 || Bool(Lobby, TEXT("ok"))) return Finish(false, TEXT("Slot limit not enforced"));
        Enter(FirstId); Next(TEXT("Server rejects a seventh character"));
    }
    else if (Step == 8 && Self)
    {
        if (ReadAppearance(Child(Self, TEXT("appearance"))).species != "timber" || Number(Self, TEXT("age")) != Ages[0])
            return Finish(false, TEXT("Selected character mismatched authoritative appearance/age"));
        if (auto UI = C->GetGameWidget()) UI->SetPresentationPage(TEXT("character"));
        Next(TEXT("Character selection enters the correct owned character"));
    }
    else if (Step == 9) { if (CaptureBeforeTransition(TEXT("36-player-character-card.png"))) return true; Command(TEXT("character_leave")); Next(); }
    else if (Step == 10)
    {
        if (Self || !Roster || Roster->Num() != 6) return Finish(false, TEXT("Leaving did not clear gameplay and restore roster"));
        if (auto UI = C->GetFrontDoorWidget()) UI->SetCharacterDraft(TEXT("Frost"), Ages[2], Appearance(Choice(2)));
        Next(TEXT("Leave returns to owned roster and clears prior gameplay snapshot"));
    }
    else if (Step == 11) { if (CaptureBeforeTransition(TEXT("37-old-arctic-preview.png"))) return true; Command(TEXT("auth_logout")); Next(); }
    else if (Step == 12) { Auth(true, TEXT("portrait_b")); Next(); }
    else if (Step == 13 && Bool(Lobby, TEXT("ok"))) { Enter(FirstId); Next(); }
    else if (Step == 14)
    {
        if (Self || Bool(Lobby, TEXT("ok"))) return Finish(false, TEXT("Other-account character ownership bypass"));
        Create(2); Next(TEXT("Another account cannot enter a character it does not own"));
    }
    else if (Step == 15 && Roster && Roster->Num() == 1) { Command(TEXT("auth_logout")); Next(); }
    else if (Step == 16) { Auth(false, TEXT("portrait_a"), false); Next(); }
    else if (Step == 17)
    {
        if (Bool(Lobby, TEXT("ok")) || String(Lobby, TEXT("stage")) != TEXT("login")) return Finish(false, TEXT("Wrong password accepted"));
        Auth(false, TEXT("portrait_a")); Next(TEXT("Wrong password rejected without revealing an owned roster"));
    }
    else if (Step == 18 && Roster && Roster->Num() == 6 && Bool(Lobby, TEXT("ok")))
    {
        if (auto UI = C->GetFrontDoorWidget()) UI->SetCharacterDraft(TEXT("Sorrel"), Ages[1], Appearance(Choice(1)));
        Next(TEXT("Correct password restores the same six owned characters"));
    }
    else if (Step == 19) { Shot(TEXT("38-maned-wolf-preview.png")); Next(); }
    else if (Step == 20) return Finish(true, TEXT("Native login, registration, live creator, six-slot roster, ownership, replay safety, and character-card flow passed"));
    return true;
}
