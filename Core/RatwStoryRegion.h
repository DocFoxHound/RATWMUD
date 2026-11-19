#pragma once

#include "RatwJsonDoc.h"
#include <cmath>
#include <optional>

namespace ratw::story
{
// Inclusive tile bounds in one cell, shared by movement watches, conditions and world effects.
struct Region
{
    std::string cell;
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;

    bool contains(const std::string& atCell, int x, int y) const
    {
        return cell == atCell && x1 <= x && x <= x2 && y1 <= y && y <= y2;
    }

    static bool tile(const json::Value& n)
    {
        return n.isNumber() && n.asNumber() >= 0 && n.asNumber() <= 4095 && std::floor(n.asNumber()) == n.asNumber();
    }

    static std::optional<Region> read(const json::Value& v)
    {
        if (v.string("cell").empty() || !tile(v["x1"]) || !tile(v["y1"]) || !tile(v["x2"]) || !tile(v["y2"])
            || v.number("x1") > v.number("x2") || v.number("y1") > v.number("y2"))
            return std::nullopt;
        return Region{v.string("cell"), int(v.number("x1")), int(v.number("y1")), int(v.number("x2")), int(v.number("y2"))};
    }
};
} // namespace ratw::story
