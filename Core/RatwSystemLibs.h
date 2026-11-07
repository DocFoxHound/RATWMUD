#pragma once
// The system libraries the server needs besides libpq (RatwPg.h), loaded at run time the same way so nothing links
// against the system's C libraries: OpenSSL's libcrypto for account passwords (PBKDF2-HMAC-SHA256), SHA-1 for the
// WebSocket handshake and Ed25519 for central's tickets (Docs/Design/77, 5), and zlib for the game's messages.
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

// Ed25519 (RFC 8032) through libcrypto's EVP calls, bound only when first asked for, so a libcrypto without them
// still serves password worlds. Checking a ticket (doc 77, 5): a 32-byte public key, the message, a 64-byte signature.
bool ed25519Available(std::string& error);
bool ed25519Verify(const std::uint8_t publicKey[32], const std::string& message, const std::string& signature);
// Signing and the public half of a private key: central signs in Python; these are for tests and tools.
bool ed25519Sign(const std::uint8_t privateKey[32], const std::string& message, std::string& signature);
bool ed25519PublicKey(const std::uint8_t privateKey[32], std::uint8_t publicKey[32]);

std::string hex(const std::uint8_t* bytes, std::size_t count);
bool readHex(const std::string& text, std::uint8_t* out, std::size_t count);

// zlib (the zlib format, as FCompression's NAME_Zlib). False if zlib can't be loaded or the data doesn't fit.
bool zlibAvailable(std::string& error);
// zlib at `level` (1 fastest ... 9 smallest; 6, zlib's own default, for saves).
bool compress(const std::uint8_t* data, std::size_t length, std::vector<std::uint8_t>& out, int level = 6);
bool uncompress(const std::uint8_t* data, std::size_t length, std::size_t rawLength, std::vector<std::uint8_t>& out);
} // namespace ratw::sys
