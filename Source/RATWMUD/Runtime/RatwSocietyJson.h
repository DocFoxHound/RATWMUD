#pragma once
#include "Runtime/RatwJson.h"

namespace ratwjson
{
inline Object Society(const ratw::SocietyState& S)
{
    auto O = New();
    O->SetBoolField(TEXT("enabled"), S.enabled);
    O->SetNumberField(TEXT("minted"), S.minted);
    O->SetNumberField(TEXT("sunk"), S.sunk);
    O->SetNumberField(TEXT("nextEntry"), S.nextEntry);
    O->SetNumberField(TEXT("budgetDay"), S.budgetDay);
    O->SetNumberField(TEXT("exportsRemaining"), S.exportsRemaining);
    O->SetNumberField(TEXT("importsRemaining"), S.importsRemaining);
    O->SetNumberField(TEXT("herbPatch"), S.herbPatch);
    O->SetNumberField(TEXT("decisionRemainder"), S.decisionRemainder);
    auto Accounts = New();
    for (const auto& Pair : S.accounts)
    {
        auto A = New(), Stock = New();
        A->SetNumberField(TEXT("cash"), Pair.second.cash);
        for (const auto& Item : Pair.second.stock) Stock->SetNumberField(F(Item.first), Item.second);
        A->SetObjectField(TEXT("stock"), Stock);
        Accounts->SetObjectField(F(Pair.first), A);
    }
    O->SetObjectField(TEXT("accounts"), Accounts);
    auto Residents = New();
    for (const auto& Pair : S.residents)
    {
        const auto& R = Pair.second;
        auto A = New();
        Text(A, TEXT("role"), R.role); Text(A, TEXT("task"), R.task); Text(A, TEXT("reason"), R.reason);
        Text(A, TEXT("goalCell"), R.goalCell);
        A->SetNumberField(TEXT("hunger"), R.hunger); A->SetNumberField(TEXT("fatigue"), R.fatigue);
        A->SetNumberField(TEXT("progress"), R.progress); A->SetNumberField(TEXT("goalX"), R.goalX);
        A->SetNumberField(TEXT("goalY"), R.goalY); A->SetNumberField(TEXT("wagesToday"), R.wagesToday);
        Text(A, TEXT("homeCell"), R.homeCell); Text(A, TEXT("relocationCell"), R.relocationCell);
        A->SetNumberField(TEXT("homeX"), R.homeX); A->SetNumberField(TEXT("homeY"), R.homeY);
        A->SetNumberField(TEXT("relocationX"), R.relocationX); A->SetNumberField(TEXT("relocationY"), R.relocationY);
        Residents->SetObjectField(F(Pair.first), A);
    }
    O->SetObjectField(TEXT("residents"), Residents);
    Array Ledger;
    for (const auto& E : S.ledger)
    {
        auto A = New();
        A->SetNumberField(TEXT("sequence"), E.sequence); A->SetNumberField(TEXT("day"), E.day);
        A->SetNumberField(TEXT("coins"), E.coins); A->SetNumberField(TEXT("quantity"), E.quantity);
        Text(A, TEXT("kind"), E.kind); Text(A, TEXT("from"), E.from); Text(A, TEXT("to"), E.to); Text(A, TEXT("item"), E.item);
        Ledger.Add(V(A));
    }
    O->SetArrayField(TEXT("ledger"), Ledger);
    // Careers (positions, skill, mourning, estates waiting to be settled): Docs/Design/26-living-npcs.md, Phase 4.
    auto Careers = New(), Positions = New(), Skill = New(), Mourning = New(), Estates = New();
    Careers->SetNumberField(TEXT("day"), static_cast<double>(S.careers.day));
    for (const auto& [Id, P] : S.careers.positions)
    {
        auto A = New();
        Text(A, TEXT("holder"), P.holder); Text(A, TEXT("apprentice"), P.apprentice); Text(A, TEXT("lastHolder"), P.lastHolder);
        A->SetNumberField(TEXT("vacantSince"), P.vacantSince);
        Positions->SetObjectField(F(Id), A);
    }
    for (const auto& [Key, Value] : S.careers.skill) Skill->SetNumberField(F(Key), Value);
    for (const auto& [Id, M] : S.careers.mourning)
    {
        auto A = New();
        A->SetNumberField(TEXT("until"), M.until); Text(A, TEXT("whom"), M.whom);
        Mourning->SetObjectField(F(Id), A);
    }
    for (const auto& [Id, Day] : S.careers.estates) Estates->SetNumberField(F(Id), Day);
    Careers->SetObjectField(TEXT("positions"), Positions); Careers->SetObjectField(TEXT("skill"), Skill);
    Careers->SetObjectField(TEXT("mourning"), Mourning); Careers->SetObjectField(TEXT("estates"), Estates);
    O->SetObjectField(TEXT("careers"), Careers);
    return O;
}
// Careers as saved. Anything unreadable is left out (the society puts founders back in their own jobs for what is
// missing, see Society::reconcileCareers); careers never make a checkpoint unreadable.
inline ratw::CareerState ReadCareers(const Object& O)
{
    ratw::CareerState C;
    if (!O.IsValid())
        return C;
    double Day = -1;
    if (O->TryGetNumberField(TEXT("day"), Day) && FMath::IsFinite(Day) && Day >= -1 && Day < 1e9)
        C.day = static_cast<std::int64_t>(Day);
    if (auto Positions = Child(O, TEXT("positions")); Positions.IsValid())
        for (const auto& Pair : Positions->Values)
            if (auto A = Pair.Value.IsValid() && Pair.Value->Type == EJson::Object ? Pair.Value->AsObject() : Object(); A.IsValid())
            {
                ratw::PositionState P;
                P.holder = S(String(A, TEXT("holder"))); P.apprentice = S(String(A, TEXT("apprentice")));
                P.lastHolder = S(String(A, TEXT("lastHolder")));
                double Since = -1;
                if (A->TryGetNumberField(TEXT("vacantSince"), Since) && FMath::IsFinite(Since))
                    P.vacantSince = Since;
                C.positions[S(Pair.Key)] = P;
            }
    if (auto Skill = Child(O, TEXT("skill")); Skill.IsValid())
        for (const auto& Pair : Skill->Values)
            if (Pair.Value.IsValid() && Pair.Value->Type == EJson::Number)
                C.skill[S(Pair.Key)] = Pair.Value->AsNumber();
    if (auto Mourning = Child(O, TEXT("mourning")); Mourning.IsValid())
        for (const auto& Pair : Mourning->Values)
            if (auto A = Pair.Value.IsValid() && Pair.Value->Type == EJson::Object ? Pair.Value->AsObject() : Object(); A.IsValid())
                C.mourning[S(Pair.Key)] = {Number(A, TEXT("until")), S(String(A, TEXT("whom")))};
    if (auto Estates = Child(O, TEXT("estates")); Estates.IsValid())
        for (const auto& Pair : Estates->Values)
            if (Pair.Value.IsValid() && Pair.Value->Type == EJson::Number)
                C.estates[S(Pair.Key)] = Pair.Value->AsNumber();
    return C;
}
// A malformed subtree invalidates the complete checkpoint. Never silently refill purses.
inline ratw::SocietyState ReadSociety(const Object& O)
{
    ratw::SocietyState S;
    // Twelve fields, and "careers" in saves made since careers (Phase 4).
    bool Valid = O.IsValid() && (O->Values.Num() == 12 || (O->Values.Num() == 13 && O->HasField(TEXT("careers"))));
    auto Integer = [&](const Object& J, const TCHAR* Key, double Max) -> std::int64_t {
        const double N = StrictNumber(J, Key, -1);
        if (N < 0 || N > Max || N != FMath::FloorToDouble(N)) { Valid = false; return 0; }
        return static_cast<std::int64_t>(N);
    };
    auto Real = [&](const Object& J, const TCHAR* Key) { const double N = StrictNumber(J, Key, -1); if (N < 0) Valid = false; return N; };
    auto RequiredText = [&](const Object& J, const TCHAR* Key) {
        FString Value;
        const auto* Field = J.IsValid() ? J->Values.Find(Key) : nullptr;
        if (!Field || !Field->IsValid() || (*Field)->Type != EJson::String || !J->TryGetStringField(Key, Value)) Valid = false;
        return ratwjson::S(Value);
    };
    if (!O.IsValid()) { S.minted = -1; return S; }
    const auto* Flag = O->Values.Find(TEXT("enabled"));
    if (!Flag || !Flag->IsValid() || (*Flag)->Type != EJson::Boolean) Valid = false;
    S.enabled = Bool(O, TEXT("enabled"));
    S.minted = Integer(O, TEXT("minted"), 1e12); S.sunk = Integer(O, TEXT("sunk"), 1e12);
    S.nextEntry = Integer(O, TEXT("nextEntry"), 1e12); S.budgetDay = Integer(O, TEXT("budgetDay"), 365000000);
    S.exportsRemaining = int(Integer(O, TEXT("exportsRemaining"), 8));
    S.importsRemaining = int(Integer(O, TEXT("importsRemaining"), 4));
    S.herbPatch = int(Integer(O, TEXT("herbPatch"), 60)); S.decisionRemainder = Real(O, TEXT("decisionRemainder"));
    auto Accounts = Child(O, TEXT("accounts")), Residents = Child(O, TEXT("residents"));
    if (!Accounts.IsValid() || Accounts->Values.Num() > int32(ratw::MaxAccounts) || !Residents.IsValid() || Residents->Values.Num() > int32(ratw::MaxResidents))
        Valid = false;
    if (Accounts.IsValid() && Accounts->Values.Num() <= 4096)
        for (const auto& Pair : Accounts->Values)
        {
            auto A = Pair.Value.IsValid() && Pair.Value->Type == EJson::Object ? Pair.Value->AsObject() : Object();
            ratw::EconomyAccount Account;
            Account.cash = Integer(A, TEXT("cash"), 1e9);
            auto Stock = Child(A, TEXT("stock"));
            if (!A.IsValid() || A->Values.Num() != 2 || !Stock.IsValid() || Stock->Values.Num() > 2) Valid = false;
            if (Stock.IsValid() && Stock->Values.Num() <= 2)
                for (const auto& Item : Stock->Values) Account.stock[ratwjson::S(Item.Key)] = int(Integer(Stock, *Item.Key, 10000));
            S.accounts[ratwjson::S(Pair.Key)] = Account;
        }
    if (Residents.IsValid() && Residents->Values.Num() <= int32(ratw::MaxResidents))
        for (const auto& Pair : Residents->Values)
        {
            auto A = Pair.Value.IsValid() && Pair.Value->Type == EJson::Object ? Pair.Value->AsObject() : Object();
            ratw::ResidentLife R;
            if (!A.IsValid() || (A->Values.Num() != 10 && A->Values.Num() != 16)) Valid = false;
            R.role = RequiredText(A, TEXT("role")); R.task = RequiredText(A, TEXT("task"));
            R.reason = RequiredText(A, TEXT("reason")); R.goalCell = RequiredText(A, TEXT("goalCell"));
            R.hunger = Real(A, TEXT("hunger")); R.fatigue = Real(A, TEXT("fatigue")); R.progress = Real(A, TEXT("progress"));
            R.goalX = Real(A, TEXT("goalX")); R.goalY = Real(A, TEXT("goalY")); R.wagesToday = int(Integer(A, TEXT("wagesToday"), 3));
            if (A.IsValid() && A->Values.Num() == 16)
            {
                R.homeCell = RequiredText(A, TEXT("homeCell")); R.relocationCell = RequiredText(A, TEXT("relocationCell"));
                R.homeX = Real(A, TEXT("homeX")); R.homeY = Real(A, TEXT("homeY"));
                R.relocationX = Real(A, TEXT("relocationX")); R.relocationY = Real(A, TEXT("relocationY"));
            }
            S.residents[ratwjson::S(Pair.Key)] = R;
        }
    const Array* Ledger = nullptr;
    if (!O->TryGetArrayField(TEXT("ledger"), Ledger) || Ledger->Num() > 128) Valid = false;
    else for (const auto& Value : *Ledger)
    {
        auto A = Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : Object();
        if (!A.IsValid() || A->Values.Num() != 8) Valid = false;
        ratw::EconomyEntry E;
        E.sequence = Integer(A, TEXT("sequence"), 1e12); E.day = Integer(A, TEXT("day"), 365000000);
        E.coins = Integer(A, TEXT("coins"), 1e9); E.quantity = int(Integer(A, TEXT("quantity"), 99));
        E.kind = RequiredText(A, TEXT("kind")); E.from = RequiredText(A, TEXT("from"));
        E.to = RequiredText(A, TEXT("to")); E.item = RequiredText(A, TEXT("item")); S.ledger.push_back(E);
    }
    S.careers = ReadCareers(Child(O, TEXT("careers")));
    if (!Valid) S.minted = -1;
    return S;
}
}
