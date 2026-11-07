#include "RatwSystemLibs.h"

#if defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>
#endif
#include <cstring>
#include <mutex>

namespace ratw::sys
{
namespace
{
void* open(std::initializer_list<const char*> names)
{
#if defined(__unix__) || defined(__APPLE__)
    for (const char* name : names)
        if (void* library = dlopen(name, RTLD_NOW | RTLD_LOCAL))
            return library;
#endif
    return nullptr;
}
template <class F>
bool bind(void* library, const char* name, F& out)
{
#if defined(__unix__) || defined(__APPLE__)
    out = reinterpret_cast<F>(dlsym(library, name));
#endif
    return out != nullptr;
}

struct Crypto
{
    bool ok = false;
    std::string error;
    int (*pbkdf2)(const char*, int, const unsigned char*, int, int, const void*, int, unsigned char*) = nullptr;
    const void* (*sha256md)() = nullptr;
    unsigned char* (*sha256)(const unsigned char*, std::size_t, unsigned char*) = nullptr;
    unsigned char* (*sha1)(const unsigned char*, std::size_t, unsigned char*) = nullptr;
    int (*rand)(unsigned char*, int) = nullptr;
    int (*memcmp)(const void*, const void*, std::size_t) = nullptr;
    void (*cleanse)(void*, std::size_t) = nullptr;
};
const Crypto& crypto()
{
    static Crypto c;
    static std::once_flag once;
    std::call_once(once, [] {
        void* library = open({"libcrypto.so.3", "libcrypto.so", "libcrypto.3.dylib", "libcrypto.dylib"});
        if (!library)
        {
            c.error = "OpenSSL's libcrypto could not be loaded.";
            return;
        }
        c.ok = bind(library, "PKCS5_PBKDF2_HMAC", c.pbkdf2) && bind(library, "EVP_sha256", c.sha256md) &&
               bind(library, "SHA256", c.sha256) && bind(library, "SHA1", c.sha1) && bind(library, "RAND_bytes", c.rand) &&
               bind(library, "CRYPTO_memcmp", c.memcmp) && bind(library, "OPENSSL_cleanse", c.cleanse);
        if (!c.ok)
            c.error = "libcrypto lacks the functions accounts need.";
    });
    return c;
}

// Ed25519's EVP calls (OpenSSL 1.1.1 and 3.x), bound apart from the rest so password worlds never need them.
struct Ed
{
    bool ok = false;
    std::string error;
    void* (*newRawPublic)(int, void*, const unsigned char*, std::size_t) = nullptr;   // EVP_PKEY_new_raw_public_key
    void* (*newRawPrivate)(int, void*, const unsigned char*, std::size_t) = nullptr;  // EVP_PKEY_new_raw_private_key
    int (*rawPublic)(const void*, unsigned char*, std::size_t*) = nullptr;            // EVP_PKEY_get_raw_public_key
    void (*freeKey)(void*) = nullptr;                                                 // EVP_PKEY_free
    void* (*newCtx)() = nullptr;                                                      // EVP_MD_CTX_new
    void (*freeCtx)(void*) = nullptr;                                                 // EVP_MD_CTX_free
    int (*verifyInit)(void*, void**, const void*, void*, void*) = nullptr;            // EVP_DigestVerifyInit
    int (*verify)(void*, const unsigned char*, std::size_t, const unsigned char*, std::size_t) = nullptr;
    int (*signInit)(void*, void**, const void*, void*, void*) = nullptr;              // EVP_DigestSignInit
    int (*sign)(void*, unsigned char*, std::size_t*, const unsigned char*, std::size_t) = nullptr;
};
constexpr int NidEd25519 = 1087;                   // NID_ED25519, the same in OpenSSL 1.1.1 and 3.x.
const Ed& ed()
{
    static Ed e;
    static std::once_flag once;
    std::call_once(once, [] {
        void* library = open({"libcrypto.so.3", "libcrypto.so", "libcrypto.3.dylib", "libcrypto.dylib"});
        if (!library)
        {
            e.error = "OpenSSL's libcrypto could not be loaded.";
            return;
        }
        e.ok = bind(library, "EVP_PKEY_new_raw_public_key", e.newRawPublic) &&
               bind(library, "EVP_PKEY_new_raw_private_key", e.newRawPrivate) &&
               bind(library, "EVP_PKEY_get_raw_public_key", e.rawPublic) && bind(library, "EVP_PKEY_free", e.freeKey) &&
               bind(library, "EVP_MD_CTX_new", e.newCtx) && bind(library, "EVP_MD_CTX_free", e.freeCtx) &&
               bind(library, "EVP_DigestVerifyInit", e.verifyInit) && bind(library, "EVP_DigestVerify", e.verify) &&
               bind(library, "EVP_DigestSignInit", e.signInit) && bind(library, "EVP_DigestSign", e.sign);
        if (!e.ok)
            e.error = "libcrypto lacks the Ed25519 calls tickets need (OpenSSL 1.1.1 or later).";
    });
    return e;
}

struct Zlib
{
    bool ok = false;
    std::string error;
    int (*compress2)(unsigned char*, unsigned long*, const unsigned char*, unsigned long, int) = nullptr;
    int (*uncompress)(unsigned char*, unsigned long*, const unsigned char*, unsigned long) = nullptr;
    unsigned long (*bound)(unsigned long) = nullptr;
};
const Zlib& zlib()
{
    static Zlib z;
    static std::once_flag once;
    std::call_once(once, [] {
        void* library = open({"libz.so.1", "libz.so", "libz.1.dylib", "libz.dylib"});
        if (!library)
        {
            z.error = "zlib could not be loaded.";
            return;
        }
        z.ok = bind(library, "compress2", z.compress2) && bind(library, "uncompress", z.uncompress) &&
               bind(library, "compressBound", z.bound);
        if (!z.ok)
            z.error = "zlib lacks compress2/uncompress.";
    });
    return z;
}
} // namespace

bool cryptoAvailable(std::string& error)
{
    error = crypto().error;
    return crypto().ok;
}

bool pbkdf2Sha256(const std::string& password, const std::uint8_t* salt, std::size_t saltLength, int iterations,
                  std::uint8_t* out, std::size_t outLength)
{
    const auto& c = crypto();
    return c.ok && c.pbkdf2(password.data(), int(password.size()), salt, int(saltLength), iterations, c.sha256md(), int(outLength), out) == 1;
}

bool sha256(const std::string& data, std::uint8_t out[32])
{
    const auto& c = crypto();
    return c.ok && c.sha256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), out) != nullptr;
}

bool sha1(const std::string& data, std::uint8_t out[20])
{
    const auto& c = crypto();
    return c.ok && c.sha1(reinterpret_cast<const unsigned char*>(data.data()), data.size(), out) != nullptr;
}

bool randomBytes(std::uint8_t* out, std::size_t length)
{
    const auto& c = crypto();
    return c.ok && c.rand(out, int(length)) == 1;
}

bool sameBytes(const std::uint8_t* a, const std::uint8_t* b, std::size_t length)
{
    const auto& c = crypto();
    if (c.ok)
        return c.memcmp(a, b, length) == 0;
    unsigned char diff = 0;
    for (std::size_t i = 0; i < length; ++i)
        diff |= a[i] ^ b[i];
    return diff == 0;
}

void wipe(void* data, std::size_t length)
{
    const auto& c = crypto();
    if (c.ok)
        c.cleanse(data, length);
    else
    {
        volatile unsigned char* p = static_cast<unsigned char*>(data);
        while (length--)
            *p++ = 0;
    }
}

bool ed25519Available(std::string& error)
{
    error = ed().error;
    return ed().ok;
}

bool ed25519Verify(const std::uint8_t publicKey[32], const std::string& message, const std::string& signature)
{
    const auto& e = ed();
    if (!e.ok || signature.size() != 64)
        return false;
    void* key = e.newRawPublic(NidEd25519, nullptr, publicKey, 32);
    if (!key)
        return false;
    void* ctx = e.newCtx();
    bool good = false;
    if (ctx && e.verifyInit(ctx, nullptr, nullptr, nullptr, key) == 1)
        good = e.verify(ctx, reinterpret_cast<const unsigned char*>(signature.data()), signature.size(),
                        reinterpret_cast<const unsigned char*>(message.data()), message.size()) == 1;
    if (ctx)
        e.freeCtx(ctx);
    e.freeKey(key);
    return good;
}

bool ed25519Sign(const std::uint8_t privateKey[32], const std::string& message, std::string& signature)
{
    const auto& e = ed();
    signature.clear();
    if (!e.ok)
        return false;
    void* key = e.newRawPrivate(NidEd25519, nullptr, privateKey, 32);
    if (!key)
        return false;
    void* ctx = e.newCtx();
    unsigned char out[64];
    std::size_t length = sizeof out;
    const bool good = ctx && e.signInit(ctx, nullptr, nullptr, nullptr, key) == 1 &&
                      e.sign(ctx, out, &length, reinterpret_cast<const unsigned char*>(message.data()), message.size()) == 1 &&
                      length == 64;
    if (good)
        signature.assign(reinterpret_cast<const char*>(out), 64);
    if (ctx)
        e.freeCtx(ctx);
    e.freeKey(key);
    return good;
}

bool ed25519PublicKey(const std::uint8_t privateKey[32], std::uint8_t publicKey[32])
{
    const auto& e = ed();
    if (!e.ok)
        return false;
    void* key = e.newRawPrivate(NidEd25519, nullptr, privateKey, 32);
    if (!key)
        return false;
    std::size_t length = 32;
    const bool good = e.rawPublic(key, publicKey, &length) == 1 && length == 32;
    e.freeKey(key);
    return good;
}

std::string hex(const std::uint8_t* bytes, std::size_t count)
{
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(count * 2);
    for (std::size_t i = 0; i < count; ++i)
    {
        out += digits[bytes[i] >> 4];
        out += digits[bytes[i] & 15];
    }
    return out;
}

bool readHex(const std::string& text, std::uint8_t* out, std::size_t count)
{
    if (text.size() != count * 2)
        return false;
    const auto nibble = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
    for (std::size_t i = 0; i < count; ++i)
    {
        const int a = nibble(text[i * 2]), b = nibble(text[i * 2 + 1]);
        if (a < 0 || b < 0)
            return false;
        out[i] = std::uint8_t((a << 4) | b);
    }
    return true;
}

bool zlibAvailable(std::string& error)
{
    error = zlib().error;
    return zlib().ok;
}

bool compress(const std::uint8_t* data, std::size_t length, std::vector<std::uint8_t>& out, int level)
{
    const auto& z = zlib();
    if (!z.ok)
        return false;
    unsigned long size = z.bound(static_cast<unsigned long>(length));
    out.resize(size);
    if (z.compress2(out.data(), &size, data, static_cast<unsigned long>(length), level) != 0)
        return false;
    out.resize(size);
    return true;
}

bool uncompress(const std::uint8_t* data, std::size_t length, std::size_t rawLength, std::vector<std::uint8_t>& out)
{
    const auto& z = zlib();
    if (!z.ok)
        return false;
    out.resize(rawLength);
    unsigned long size = static_cast<unsigned long>(rawLength);
    return z.uncompress(out.data(), &size, data, static_cast<unsigned long>(length)) == 0 && size == rawLength;
}
} // namespace ratw::sys
