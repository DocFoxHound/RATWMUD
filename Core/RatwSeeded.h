// Seeded lots and plain word lists for the staged trade's shows (doc 79, Phases 4-5): a wagon, a cart and a ship are as
// fixed as their seed (a name and a number: "ship:east" and its day), so the same world shows the same ones after a
// restart; and "a, b and c" for their notices. One copy for the roads' show (RatwTradeShow.cpp) and the port's
// (RatwPort.cpp).
#pragma once

#include <cctype>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ratw::seeded
{
inline std::uint64_t lot(const std::string& a, std::int64_t b)
{
    std::uint64_t h = std::hash<std::string>{}(a) * 1099511628211ULL + std::uint64_t(b) * 2654435761ULL;
    h ^= h >> 29;
    h *= 0xBF58476D1CE4E5B9ULL;
    h ^= h >> 32;
    return h;
}
// The same lot as a number in [0, 1).
inline double unit(const std::string& a, std::int64_t b)
{
    return double(lot(a, b) % 100000) / 100000.0;
}
inline std::string lowered(std::string s)
{
    for (auto& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
// "a", "a and b", "a, b and c".
inline std::string listed(const std::vector<std::string>& words)
{
    std::string out;
    for (std::size_t i = 0; i < words.size(); ++i)
        out += (i == 0 ? "" : i + 1 == words.size() ? " and " : ", ") + words[i];
    return out;
}
} // namespace ratw::seeded
