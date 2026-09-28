#pragma once
// The Unreal client's side of the standalone server's wire (Core/RatwLink.h): -RatwServer=host:port makes the game a
// client of Server/ratw_server instead of an Unreal server. One TCP connection; commands and snapshot
// acknowledgements go out, events, snapshots and motion frames come in, and the player controller handles them as
// if they had arrived by RPC.
#include "CoreMinimal.h"
#include "Core/RatwLink.h"

class FSocket;

class FRatwRemoteLink
{
  public:
    ~FRatwRemoteLink();
    // "host:port" (host an IPv4 address or a name). Blocks for at most a few seconds.
    bool Connect(const FString& Address, FString& Error);
    void Close();
    bool IsOpen() const { return Socket != nullptr && !Broken; }
    bool IsLoopback() const { return Loopback; }
    void SendCommand(const FString& Json);
    void SendAck(double Revision, bool Missing);
    // Everything that has arrived, decompressed: each frame's kind, bytes (UTF-8 JSON, or a binary motion frame), and
    // how many bytes it took on the wire.
    void Poll(TFunctionRef<void(ratw::link::Kind Kind, const TArray<uint8>& Raw, int32 WireBytes)> Each);

  private:
    FSocket* Socket = nullptr;
    bool Broken = false, Loopback = false;
    std::string In, Out;
    void Flush();
};
