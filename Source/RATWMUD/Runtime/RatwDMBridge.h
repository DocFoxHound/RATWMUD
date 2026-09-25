#pragma once

#include "Runtime/RatwSocietyJson.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include <filesystem>
#if PLATFORM_LINUX
#include <sys/stat.h>
#include <unistd.h>
#endif

// Opt-in, same-OS-user operator channel. It is never routed through a character
// command or replicated snapshot. See Docs/DM_BRIDGE_CONTRACT.md.
class FRatwDMBridge
{
    using Object = ratwjson::Object;
    using Array = ratwjson::Array;
    FString Directory;
    FString WorldId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    Array Receipts;
    double Accumulator = 2;
    bool Healthy = true;

    static FString Text(const Object& O, const TCHAR* Key)
    {
        const auto* V = O.IsValid() ? O->Values.Find(Key) : nullptr;
        return V && V->IsValid() && (*V)->Type == EJson::String ? (*V)->AsString() : FString();
    }
    static bool Keys(const Object& O, std::initializer_list<const TCHAR*> Allowed)
    {
        if (!O.IsValid()) return false;
        for (const auto& Field : O->Values)
        {
            bool Found = false;
            for (const auto* Key : Allowed) Found |= Field.Key == Key;
            if (!Found || !Field.Value.IsValid()) return false;
        }
        return true;
    }
    static bool StringField(const Object& O, const TCHAR* Key)
    {
        const auto* Field = O.IsValid() ? O->Values.Find(Key) : nullptr;
        return Field && Field->IsValid() && (*Field)->Type == EJson::String;
    }
    static bool SafeId(const FString& Id)
    {
        if (Id.IsEmpty() || Id.Len() > 80) return false;
        for (TCHAR C : Id) if (!((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') ||
            (C >= '0' && C <= '9') || C == '-' || C == '_')) return false;
        return true;
    }
    static FString Digest(const FString& Raw)
    {
        FTCHARToUTF8 Bytes(*Raw);
        uint8 Hash[20]; FSHA1::HashBuffer(Bytes.Get(), Bytes.Length(), Hash);
        return BytesToHex(Hash, 20);
    }
    bool Write(const FString& Path, const Object& Value)
    {
        const FString Temp = Path + TEXT(".") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".tmp");
        if (!FFileHelper::SaveStringToFile(ratwjson::Encode(Value), *Temp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) return false;
#if PLATFORM_LINUX
        if (chmod(TCHAR_TO_UTF8(*Temp), 0600) != 0) { IFileManager::Get().Delete(*Temp); return false; }
#endif
        if (!IFileManager::Get().Move(*Path, *Temp, true, true, false, true))
        { IFileManager::Get().Delete(*Temp); return false; }
        return true;
    }
    Object Result(const FString& Id, bool Ok, const FString& Detail, double Now) const
    {
        auto R = ratwjson::New();
        R->SetNumberField(TEXT("version"), 1); R->SetStringField(TEXT("id"), Id);
        R->SetStringField(TEXT("worldId"), WorldId); R->SetBoolField(TEXT("ok"), Ok);
        R->SetStringField(TEXT("detail"), Detail); R->SetNumberField(TEXT("appliedAtUnix"), Now);
        return R;
    }

  public:
    using NoticeSender = TFunction<void(const std::set<std::string>&, const FString&)>;
    bool Enabled() const { return !Directory.IsEmpty() && Healthy; }
    const FString& Id() const { return WorldId; }
    bool Configure(const FString& Path)
    {
        if (Path.IsEmpty()) return true;
        if (FPaths::IsRelative(Path) || Path.Len() > 2048) return false;
        namespace fs = std::filesystem;
        std::error_code Error;
        const fs::path Root(ratwjson::S(Path));
        if (Root == Root.root_path() || fs::is_symlink(Root, Error)) return false;
        Error.clear();
        const bool Created = fs::create_directories(Root, Error);
        if (Error || !fs::is_directory(Root, Error)) return false;
#if PLATFORM_LINUX
        if (Created && chmod(Root.c_str(), 0700) != 0) return false;
        struct stat Info{};
        if (stat(Root.c_str(), &Info) != 0 || Info.st_uid != getuid() || (Info.st_mode & 0077)) return false;
#else
        (void)Created;
#endif
        for (const char* Child : {"outbox", "inbox"})
        {
            const auto Sub = Root / Child;
            if (fs::is_symlink(Sub, Error)) return false;
            Error.clear(); const bool Made = fs::create_directory(Sub, Error);
            if (Error || !fs::is_directory(Sub, Error)) return false;
#if PLATFORM_LINUX
            if (Made && chmod(Sub.c_str(), 0700) != 0) return false;
            if (stat(Sub.c_str(), &Info) != 0 || Info.st_uid != getuid() || (Info.st_mode & 0077)) return false;
#else
            (void)Made;
#endif
        }
        Directory = FPaths::ConvertRelativePathToFull(Path);
        return true;
    }
    Object State() const
    {
        auto O = ratwjson::New(); O->SetStringField(TEXT("worldId"), WorldId);
        O->SetArrayField(TEXT("receipts"), Receipts); return O;
    }
    bool Restore(const Object& O)
    {
        if (!O.IsValid()) return false;
        const FString Identity = Text(O, TEXT("worldId"));
        FGuid Guid;
        const Array* Saved = nullptr;
        if (!Keys(O, {TEXT("worldId"), TEXT("receipts")}) || !FGuid::ParseExact(Identity, EGuidFormats::DigitsWithHyphens, Guid) ||
            !O->TryGetArrayField(TEXT("receipts"), Saved) || Saved->Num() > 512) return false;
        std::set<FString> Unique;
        for (const auto& Value : *Saved)
        {
            auto R = Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : Object();
            auto Reply = ratwjson::Child(R, TEXT("result"));
            const auto* Ok = Reply.IsValid() ? Reply->Values.Find(TEXT("ok")) : nullptr;
            const auto Hash = Text(R, TEXT("fingerprint"));
            bool Hex = Hash.Len() == 40;
            for (TCHAR C : Hash) Hex &= (C >= '0' && C <= '9') || (C >= 'A' && C <= 'F') || (C >= 'a' && C <= 'f');
            if (!Keys(R, {TEXT("id"), TEXT("fingerprint"), TEXT("result")}) || R->Values.Num() != 3 || !SafeId(Text(R, TEXT("id"))) ||
                !Unique.insert(Text(R, TEXT("id"))).second || Text(R, TEXT("fingerprint")).Len() != 40 ||
                !Hex || !Keys(Reply, {TEXT("version"), TEXT("id"), TEXT("worldId"), TEXT("ok"), TEXT("detail"), TEXT("appliedAtUnix")}) ||
                Reply->Values.Num() != 6 || ratwjson::StrictNumber(Reply, TEXT("version"), -1) != 1 || !StringField(Reply, TEXT("detail")) ||
                !Ok || !Ok->IsValid() || (*Ok)->Type != EJson::Boolean || Text(Reply, TEXT("worldId")) != Identity ||
                Text(Reply, TEXT("id")) != Text(R, TEXT("id")) || Text(Reply, TEXT("detail")).Len() > 8192 ||
                ratwjson::StrictNumber(Reply, TEXT("appliedAtUnix"), -1) < 0) return false;
        }
        WorldId = Identity; Receipts = *Saved; return true;
    }
    Object Snapshot(const ratw::World& World, const std::map<std::string, ratw::Entity>& Characters,
                    const std::set<std::string>& Online, const std::map<std::string, double>& Activity,
                    uint64 Revision, double Now) const
    {
        using namespace ratwjson;
        auto O = New(); O->SetNumberField(TEXT("version"), 1); O->SetStringField(TEXT("worldId"), WorldId);
        O->SetNumberField(TEXT("sequence"), Revision); O->SetNumberField(TEXT("generatedAtUnix"), Now);
        O->SetNumberField(TEXT("calendarDays"), World.calendarDays());
        Array Capabilities, Cells, Factions, Chapters, Actors, Accounts;
        for (const auto* Capability : {TEXT("notice"), TEXT("weather"), TEXT("npc_relocate"), TEXT("economy_transfer")}) Capabilities.Add(V(FString(Capability)));
        O->SetArrayField(TEXT("capabilities"), Capabilities);
        for (const auto& Pair : World.cells())
        {
            const auto& C = Pair.second; auto J = New(), Territory = New();
            ratwjson::Text(J, TEXT("id"), C.id); ratwjson::Text(J, TEXT("name"), C.name);
            J->SetNumberField(TEXT("x"), C.worldX); J->SetNumberField(TEXT("y"), C.worldY); J->SetNumberField(TEXT("z"), C.worldZ);
            J->SetNumberField(TEXT("width"), C.width); J->SetNumberField(TEXT("height"), C.height);
            J->SetBoolField(TEXT("outdoors"), C.outdoors); ratwjson::Text(J, TEXT("weather"), ratw::weatherName(C.weather));
            ratwjson::Text(Territory, TEXT("region"), C.region); ratwjson::Text(Territory, TEXT("chapter"), C.chapter);
            Array Claims, Rows;
            for (const auto& Claim : C.factionClaims) Claims.Add(V(F(Claim)));
            Territory->SetArrayField(TEXT("claims"), Claims); J->SetObjectField(TEXT("territory"), Territory);
            for (int Y = 0; Y < C.height; ++Y)
            { FString Row; for (int X = 0; X < C.width; ++X) Row += TCHAR(C.tile(X, Y)->glyph); Rows.Add(V(Row)); }
            J->SetArrayField(TEXT("terrain"), Rows); Cells.Add(V(J));
        }
        for (const auto& Pair : World.factions())
        { auto J = New(); ratwjson::Text(J, TEXT("id"), Pair.first); ratwjson::Text(J, TEXT("name"), Pair.second.name); ratwjson::Text(J, TEXT("color"), Pair.second.color); Factions.Add(V(J)); }
        for (const auto& Pair : World.chapters())
        { auto J = New(); ratwjson::Text(J, TEXT("id"), Pair.first); ratwjson::Text(J, TEXT("name"), Pair.second.name); Chapters.Add(V(J)); }
        auto All = Characters;
        for (const auto& Pair : World.entities()) All[Pair.first] = Pair.second;
        for (const auto& Pair : All)
        {
            const auto& E = Pair.second; auto J = New(), Stock = New();
            ratwjson::Text(J, TEXT("id"), E.id); ratwjson::Text(J, TEXT("name"), E.name); ratwjson::Text(J, TEXT("cell"), E.cellId);
            J->SetBoolField(TEXT("npc"), E.npc); J->SetBoolField(TEXT("online"), E.npc || Online.count(E.id));
            const auto Seen = Activity.find(E.id); const double Last = Seen == Activity.end() ? 0 : Seen->second;
            J->SetBoolField(TEXT("active"), !E.npc && Online.count(E.id) && Last > 0 && Now >= Last && Now - Last <= 300);
            J->SetNumberField(TEXT("lastActiveAtUnix"), Last); J->SetNumberField(TEXT("x"), E.position.x); J->SetNumberField(TEXT("y"), E.position.y);
            J->SetNumberField(TEXT("age"), E.age); ratwjson::Text(J, TEXT("activity"), E.activity);
            const auto* Life = World.society().resident(E.id); const auto* Account = World.society().account(E.id);
            ratwjson::Text(J, TEXT("role"), Life ? Life->role : "player");
            J->SetBoolField(TEXT("recruited"), !E.leaderId.empty() || E.state == "following");
            ratwjson::Text(J, TEXT("homeCell"), Life ? Life->homeCell : E.cellId);
            J->SetNumberField(TEXT("homeX"), Life ? Life->homeX : E.position.x); J->SetNumberField(TEXT("homeY"), Life ? Life->homeY : E.position.y);
            J->SetBoolField(TEXT("relocating"), Life && !Life->relocationCell.empty());
            ratwjson::Text(J, TEXT("relocationTarget"), Life ? Life->relocationCell : "");
            J->SetNumberField(TEXT("cash"), Account ? Account->cash : 0);
            if (Account) for (const auto& Item : Account->stock) Stock->SetNumberField(F(Item.first), Item.second);
            J->SetObjectField(TEXT("stock"), Stock); Actors.Add(V(J));
        }
        for (const auto& Pair : World.society().state().accounts)
        {
            auto J = New(), Stock = New(); ratwjson::Text(J, TEXT("id"), Pair.first); J->SetNumberField(TEXT("cash"), Pair.second.cash);
            for (const auto& Item : Pair.second.stock) Stock->SetNumberField(F(Item.first), Item.second);
            J->SetObjectField(TEXT("stock"), Stock); Accounts.Add(V(J));
        }
        auto Economy = New(), Full = ratwjson::Society(World.society().state());
        Economy->SetNumberField(TEXT("minted"), World.society().state().minted); Economy->SetNumberField(TEXT("sunk"), World.society().state().sunk);
        Economy->SetArrayField(TEXT("ledger"), Items(Full, TEXT("ledger")));
        O->SetArrayField(TEXT("cells"), Cells); O->SetArrayField(TEXT("factions"), Factions); O->SetArrayField(TEXT("chapters"), Chapters);
        O->SetArrayField(TEXT("characters"), Actors); O->SetArrayField(TEXT("accounts"), Accounts); O->SetObjectField(TEXT("economy"), Economy);
        return O;
    }

    Object Execute(const Object& Request, const FString& ExpectedId, const FString& Fingerprint,
                   ratw::World& World, const std::set<std::string>& Online, double Now,
                   const TFunction<bool()>& Commit, const NoticeSender& SendNotice)
    {
        using namespace ratwjson;
        if (!SafeId(ExpectedId) || !Keys(Request, {TEXT("version"), TEXT("id"), TEXT("worldId"), TEXT("createdAtUnix"), TEXT("expiresAtUnix"), TEXT("kind"), TEXT("payload")}) ||
            StrictNumber(Request, TEXT("version"), -1) != 1 || Text(Request, TEXT("id")) != ExpectedId ||
            Text(Request, TEXT("worldId")) != WorldId) return Result(ExpectedId, false, TEXT("Invalid or cross-world operator envelope."), Now);
        for (const auto& Saved : Receipts)
        {
            auto R = Saved->AsObject();
            if (Text(R, TEXT("id")) == ExpectedId) return Text(R, TEXT("fingerprint")) == Fingerprint
                ? Child(R, TEXT("result")) : Result(ExpectedId, false, TEXT("Request ID was reused with different content."), Now);
        }
        const double Created = StrictNumber(Request, TEXT("createdAtUnix"), -1), Expires = StrictNumber(Request, TEXT("expiresAtUnix"), -1);
        if (Created < 0 || Created > Now + 5 || Expires < Now || Expires <= Created || Expires - Created > 300)
            return Result(ExpectedId, false, TEXT("Operator request expired or has an invalid clock window."), Now);
        const auto P = Child(Request, TEXT("payload")); const auto Kind = Text(Request, TEXT("kind"));
        // A failed checkpoint must not cancel unrelated paths, typing or turns.
        // Portable save records intentionally strip those transient details.
        const auto Before = World;
        ratw::Result Applied{false, "Unsupported or malformed operator action.", {}};
        std::set<std::string> Recipients; FString Notice;
        if (Kind == TEXT("weather") && Keys(P, {TEXT("cell"), TEXT("preset")}) && P->Values.Num() == 2)
            Applied = EnvironmentCommand(World, S(Text(P, TEXT("cell"))), TEXT("weather"), Text(P, TEXT("preset")), true);
        else if (Kind == TEXT("npc_relocate") && Keys(P, {TEXT("npc"), TEXT("cell"), TEXT("x"), TEXT("y")}) && P->Values.Num() == 4)
            Applied = World.relocateResident(S(Text(P, TEXT("npc"))), S(Text(P, TEXT("cell"))),
                StrictNumber(P, TEXT("x"), -1), StrictNumber(P, TEXT("y"), -1));
        else if (Kind == TEXT("economy_transfer") && Keys(P, {TEXT("from"), TEXT("to"), TEXT("item"), TEXT("quantity"), TEXT("coins")}) && P->Values.Num() == 5)
        {
            const double Qty = StrictNumber(P, TEXT("quantity"), -1), Coins = StrictNumber(P, TEXT("coins"), -1);
            if (StringField(P, TEXT("item")) && Qty >= 0 && Qty <= 99 && Coins >= 0 && Coins <= 1000000 && Qty == FMath::FloorToDouble(Qty) && Coins == FMath::FloorToDouble(Coins))
            {
                const auto A = World.society().operatorTransfer(S(Text(P, TEXT("from"))), S(Text(P, TEXT("to"))),
                    S(Text(P, TEXT("item"))), int(Qty), int64(Coins)); Applied = {A.ok, A.message, {}};
            }
        }
        else if (Kind == TEXT("notice") && Keys(P, {TEXT("scope"), TEXT("target"), TEXT("targets"), TEXT("text")}))
        {
            Notice = Text(P, TEXT("text")); const auto Scope = Text(P, TEXT("scope")), Target = Text(P, TEXT("target"));
            bool Valid = !Notice.TrimStartAndEnd().IsEmpty() && Notice.Len() <= 8192;
            if (Scope == TEXT("world"))
            { Valid &= !P->HasField(TEXT("targets")) && (!P->HasField(TEXT("target")) || (StringField(P, TEXT("target")) && Target.IsEmpty())); Recipients = Online; }
            else if (Scope == TEXT("player"))
            { Valid &= StringField(P, TEXT("target")) && SafeId(Target) && !P->HasField(TEXT("targets")); if (Online.count(S(Target))) Recipients.insert(S(Target)); }
            else if (Scope == TEXT("cell"))
            { Valid &= StringField(P, TEXT("target")) && World.cell(S(Target)) && !P->HasField(TEXT("targets"));
              for (const auto& Id : Online) if (World.entity(Id) && World.entity(Id)->cellId == S(Target)) Recipients.insert(Id); }
            else if (Scope == TEXT("players"))
            {
                Valid &= !P->HasField(TEXT("target"));
                const Array* Targets = nullptr;
                if (!P->TryGetArrayField(TEXT("targets"), Targets) || Targets->IsEmpty() || Targets->Num() > 128) Valid = false;
                else for (const auto& V : *Targets)
                { if (!V.IsValid() || V->Type != EJson::String || !SafeId(V->AsString())) Valid = false;
                  else if (Online.count(S(V->AsString()))) Recipients.insert(S(V->AsString())); }
            }
            else Valid = false;
            if (Valid && !Recipients.empty()) Applied = {true, "Operator announcement queued for " + std::to_string(Recipients.size()) + " connected character(s).", {}};
            else Applied = {false, "Invalid announcement or no matching connected recipients.", {}};
        }
        auto Reply = Result(ExpectedId, Applied.ok, F(Applied.message), Now);
        auto Receipt = New(); Receipt->SetStringField(TEXT("id"), ExpectedId); Receipt->SetStringField(TEXT("fingerprint"), Fingerprint); Receipt->SetObjectField(TEXT("result"), Reply);
        const Array Previous = Receipts;
        Receipts.Add(V(Receipt)); if (Receipts.Num() > 512) Receipts.RemoveAt(0);
        if (!Commit())
        {
            if (Applied.ok) World = Before;
            Receipts = Previous; Healthy = false;
            return Result(ExpectedId, false, TEXT("Checkpoint failed; operator channel halted without committing the requested effect."), Now);
        }
        if (Applied.ok && !Notice.IsEmpty()) SendNotice(Recipients, Notice);
        return Reply;
    }

    void Tick(double Dt, ratw::World& World, const std::map<std::string, ratw::Entity>& Characters,
              const std::set<std::string>& Online, const std::map<std::string, double>& Activity,
              uint64 Revision, const TFunction<bool()>& Commit, const NoticeSender& SendNotice)
    {
        if (!Enabled()) return;
        Accumulator += Dt; if (Accumulator < 2) return; Accumulator = 0;
        const double Now = double(FDateTime::UtcNow().ToUnixTimestamp());
        namespace fs = std::filesystem; std::error_code Error;
        int Count = 0;
        for (fs::directory_iterator It(fs::path(ratwjson::S(Directory)) / "outbox", Error), End; !Error && It != End && Count < 16; It.increment(Error))
        {
            const auto Path = It->path();
            if (Path.extension() != ".json" || It->is_symlink(Error) || !It->is_regular_file(Error)) continue;
            const FString Id = ratwjson::F(Path.stem().string()); if (!SafeId(Id)) continue;
            ++Count; const auto Size = It->file_size(Error); if (Error) break;
            Object Reply;
            if (Size > 65536) Reply = Result(Id, false, TEXT("Operator request exceeds 64 KiB."), Now);
            else
            {
                FString Raw;
                if (!FFileHelper::LoadFileToString(Raw, *ratwjson::F(Path.string())))
                { Healthy = false; UE_LOG(LogTemp, Error, TEXT("RATW operator request cannot be read; channel halted.")); return; }
                Reply = Execute(ratwjson::Decode(Raw), Id, Digest(Raw), World, Online, Now, Commit, SendNotice);
            }
            if (!Write(Directory / TEXT("inbox") / (Id + TEXT(".json")), Reply))
            { Healthy = false; UE_LOG(LogTemp, Error, TEXT("RATW operator receipt cannot be published; channel halted.")); return; }
            fs::remove(Path, Error);
            if (!Healthy) return;
        }
        if (Error)
        { Healthy = false; UE_LOG(LogTemp, Error, TEXT("RATW operator queue filesystem failure; channel halted.")); return; }
        if (!Write(Directory / TEXT("snapshot.json"), Snapshot(World, Characters, Online, Activity, Revision, Now)))
        { Healthy = false; UE_LOG(LogTemp, Error, TEXT("RATW operator snapshot failed; channel halted.")); }
    }
};
