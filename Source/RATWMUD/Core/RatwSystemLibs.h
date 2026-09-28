#pragma once
// The system libraries a standalone server needs besides libpq (RatwPg.h), loaded at run time the same way so nothing
// links against the system's C libraries: OpenSSL's libcrypto for account passwords (PBKDF2-HMAC-SHA256, exactly as
// the Unreal runtime's FRatwAccounts derives them) and zlib for the wire (the envelope Unreal's FCompression makes).
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ratw::sys
{
// libcrypto. False if it can't be loaded or the call fails.
bool cryptoAvailable(std::string& error);
bool pbkdf2Sha256(const std::string& password, const std::uint8_t* salt, std::size_t saltLength, int iterations,
                  std::uint8_t* out, std::size_t outLength);
bool sha256(const std::string& data, std::uint8_t out[32]);
bool sha1(const std::string& data, std::uint8_t out[20]);
bool randomBytes(std::uint8_t* out, std::size_t length);
bool sameBytes(const std::uint8_t* a, const std::uint8_t* b, std::size_t length);   // In constant time.
void wipe(void* data, std::size_t length);

std::string hex(const std::uint8_t* bytes, std::size_t count);
bool readHex(const std::string& text, std::uint8_t* out, std::size_t count);

// zlib (the zlib format, as FCompression's NAME_Zlib). False if zlib can't be loaded or the data doesn't fit.
bool zlibAvailable(std::string& error);
bool compress(const std::uint8_t* data, std::size_t length, std::vector<std::uint8_t>& out);
bool uncompress(const std::uint8_t* data, std::size_t length, std::size_t rawLength, std::vector<std::uint8_t>& out);
} // namespace ratw::sys
