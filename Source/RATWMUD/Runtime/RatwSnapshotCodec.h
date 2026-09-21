#pragma once

#include "CoreMinimal.h"
#include "Misc/Compression.h"

// Snapshots are already observer-filtered before encoding. A bounded compressed
// UTF-8 envelope stays below Unreal's 64 KiB RPC bunch limit; never raise that
// global engine limit to accommodate verbose JSON.
namespace ratwwire
{
constexpr int32 MaxRawBytes = 1024 * 1024;
constexpr int32 MaxCompressedBytes = 48 * 1024;

inline bool Encode(const FString& Json, TArray<uint8>& Compressed, int32& RawBytes)
{
    const FTCHARToUTF8 Utf8(*Json);
    RawBytes = Utf8.Length();
    if (RawBytes <= 0 || RawBytes > MaxRawBytes)
        return false;
    int32 Size = FCompression::CompressMemoryBound(NAME_Zlib, RawBytes);
    Compressed.SetNumUninitialized(Size);
    if (!FCompression::CompressMemory(NAME_Zlib, Compressed.GetData(), Size, Utf8.Get(), RawBytes))
        return false;
    Compressed.SetNum(Size);
    return Size > 0 && Size <= MaxCompressedBytes;
}

inline bool Decode(const TArray<uint8>& Compressed, int32 RawBytes, FString& Json)
{
    if (RawBytes <= 0 || RawBytes > MaxRawBytes || Compressed.IsEmpty() || Compressed.Num() > MaxCompressedBytes)
        return false;
    TArray<uint8> Raw;
    Raw.SetNumZeroed(RawBytes + 1);
    if (!FCompression::UncompressMemory(NAME_Zlib, Raw.GetData(), RawBytes, Compressed.GetData(), Compressed.Num()))
        return false;
    Json = UTF8_TO_TCHAR(reinterpret_cast<const char*>(Raw.GetData()));
    return !Json.IsEmpty();
}
} // namespace ratwwire
