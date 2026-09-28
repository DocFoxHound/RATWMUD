#include "Runtime/RatwRemoteLink.h"
#include "Runtime/RatwAccounts.h"
#include "Misc/Compression.h"
#include "IPAddress.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

FRatwRemoteLink::~FRatwRemoteLink()
{
    Close();
}

bool FRatwRemoteLink::Connect(const FString& Address, FString& Error)
{
    Close();
    FString Host, PortText;
    if (!Address.Split(TEXT(":"), &Host, &PortText, ESearchCase::IgnoreCase, ESearchDir::FromEnd) || Host.IsEmpty() ||
        !PortText.IsNumeric())
    {
        Error = TEXT("-RatwServer needs host:port.");
        return false;
    }
    ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!Sockets)
    {
        Error = TEXT("No socket subsystem.");
        return false;
    }
    TSharedPtr<FInternetAddr> Target = Sockets->GetAddressFromString(Host);
    if (!Target.IsValid() || !Target->IsValid())
    {
        FAddressInfoResult Found = Sockets->GetAddressInfo(*Host, nullptr, EAddressInfoFlags::Default, NAME_None, ESocketType::SOCKTYPE_Streaming);
        if (Found.ReturnCode != SE_NO_ERROR || Found.Results.IsEmpty())
        {
            Error = TEXT("Cannot find the server ") + Host + TEXT(".");
            return false;
        }
        Target = Found.Results[0].Address;
    }
    Target->SetPort(FCString::Atoi(*PortText));
    Socket = Sockets->CreateSocket(NAME_Stream, TEXT("RATW standalone server"), Target->GetProtocolType());
    if (!Socket)
    {
        Error = TEXT("Cannot open a socket.");
        return false;
    }
    Socket->SetNoDelay(true);
    if (!Socket->Connect(*Target))
    {
        Error = FString::Printf(TEXT("Cannot connect to %s."), *Address);
        Close();
        return false;
    }
    Socket->SetNonBlocking(true);
    Loopback = FRatwAccounts::IsLoopbackAddress(Target->ToString(false));
    Broken = false;
    return true;
}

void FRatwRemoteLink::Close()
{
    if (Socket)
    {
        Socket->Close();
        if (ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
            Sockets->DestroySocket(Socket);
    }
    Socket = nullptr;
    In.clear();
    Out.clear();
}

void FRatwRemoteLink::SendCommand(const FString& Json)
{
    const FTCHARToUTF8 Utf8(*Json);
    if (!IsOpen() || Utf8.Length() > int32(ratw::link::MaxCommand))
        return;
    ratw::link::appendFrame(Out, ratw::link::Command, Utf8.Get(), size_t(Utf8.Length()));
    Flush();
}

void FRatwRemoteLink::SendAck(double Revision, bool Missing)
{
    if (!IsOpen())
        return;
    char Payload[9];
    FMemory::Memcpy(Payload, &Revision, 8);
    Payload[8] = Missing ? 1 : 0;
    ratw::link::appendFrame(Out, ratw::link::Ack, Payload, 9);
    Flush();
}

void FRatwRemoteLink::Flush()
{
    while (Socket && !Out.empty())
    {
        int32 Sent = 0;
        if (!Socket->Send(reinterpret_cast<const uint8*>(Out.data()), int32(FMath::Min<size_t>(Out.size(), 1 << 20)), Sent))
        {
            if (ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLastErrorCode() != SE_EWOULDBLOCK)
                Broken = true;
            return;
        }
        if (Sent <= 0)
            return;
        Out.erase(0, size_t(Sent));
    }
}

void FRatwRemoteLink::Poll(TFunctionRef<void(ratw::link::Kind Kind, const TArray<uint8>& Raw, int32 WireBytes)> Each)
{
    if (!Socket)
        return;
    Flush();
    uint8 Buffer[65536];
    for (;;)
    {
        int32 Read = 0;
        if (!Socket->Recv(Buffer, sizeof(Buffer), Read))
        {
            if (ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLastErrorCode() != SE_EWOULDBLOCK)
                Broken = true;
            break;
        }
        if (Read <= 0)
        {
            if (Socket->GetConnectionState() != SCS_Connected)
                Broken = true;
            break;
        }
        In.append(reinterpret_cast<const char*>(Buffer), size_t(Read));
    }
    ratw::link::Kind Kind;
    std::string Payload;
    bool Bad = false;
    while (ratw::link::takeFrame(In, Kind, Payload, Bad))
    {
        if (Payload.size() < 4)
            continue;
        uint32 RawLength = 0;
        for (int32 I = 0; I < 4; ++I)
            RawLength |= uint32(uint8(Payload[size_t(I)])) << (8 * I);
        if (RawLength == 0 || RawLength > ratw::link::MaxRaw)
            continue;
        TArray<uint8> Raw;
        Raw.SetNumUninitialized(int32(RawLength));
        if (!FCompression::UncompressMemory(NAME_Zlib, Raw.GetData(), int32(RawLength), Payload.data() + 4, int32(Payload.size() - 4)))
            continue;
        Each(Kind, Raw, int32(Payload.size() + 5));
    }
    if (Bad)
        Broken = true;
}
