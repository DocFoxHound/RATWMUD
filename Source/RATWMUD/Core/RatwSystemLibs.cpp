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
               bind(library, "SHA256", c.sha256) && bind(library, "RAND_bytes", c.rand) &&
               bind(library, "CRYPTO_memcmp", c.memcmp) && bind(library, "OPENSSL_cleanse", c.cleanse);
        if (!c.ok)
            c.error = "libcrypto lacks the functions accounts need.";
    });
    return c;
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

bool compress(const std::uint8_t* data, std::size_t length, std::vector<std::uint8_t>& out)
{
    const auto& z = zlib();
    if (!z.ok)
        return false;
    unsigned long size = z.bound(static_cast<unsigned long>(length));
    out.resize(size);
    if (z.compress2(out.data(), &size, data, static_cast<unsigned long>(length), 6) != 0)
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
