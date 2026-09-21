#pragma once
#include "CoreMinimal.h"
#include "SQLiteDatabase.h"

// One transaction holds the complete internally consistent MVP state. The
// payload is versioned JSON; SQLite provides atomic commits, not loose files.
class FRatwPersistence
{
  public:
    ~FRatwPersistence();
    bool Open(const FString& Path);
    FString Load();
    bool Save(const FString& Payload, uint64 Revision);
    FString Error() const;

  private:
    FSQLiteDatabase Database;
};
