// A realm founded at ladder.json's foundingDay (Docs/Design/64-economy-migration.md, Phase 13: 60p), for the tests of
// what follows the day: while one of these stands, a world or a society made (a test's village, under
// WorldMoneyResidents) founds its going rate and its amounts at the founding's day, as a realm's are; its money is
// still a village's. `ratio` is the founding's day over the catalogue's (5 at 60p): every amount written at a 12p day
// is that many times what it was.
#pragma once

#include "RatwItems.h"
#include "RatwSociety.h"

struct FoundedAtTheDay
{
    FoundedAtTheDay() { ratw::Society::setFoundAnyLand(true); }
    ~FoundedAtTheDay() { ratw::Society::setFoundAnyLand(false); }
    FoundedAtTheDay(const FoundedAtTheDay&) = delete;
    FoundedAtTheDay& operator=(const FoundedAtTheDay&) = delete;
    static double day() { return ratw::items::ladder().foundingDay; }
    static double ratio() { return ratw::items::ladder().foundingDay / ratw::items::ladder().catalogueDay; }
    // An amount written in pennies at a 12p day, in the founded realm's pennies.
    static std::int64_t at(std::int64_t pennies) { return ratw::Society::pennies(double(pennies) * ratio()); }
};
