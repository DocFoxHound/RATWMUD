#include "RatwWire.h"
#include "RatwOrchestratorJson.h"
#include "RatwItems.h"
#include "RatwCalendar.h"
#include "RatwGifts.h"
#include "RatwStep.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace ratw::wire
{
double strictNumber(const Value& o, const std::string& key, double fallback)
{
    const auto* v = o.find(key);
    return v && v->isNumber() && std::isfinite(v->asNumber()) ? v->asNumber() : fallback;
}

double number(const Value& o, const std::string& key, double fallback) { return strictNumber(o, key, fallback); }

Value appearance(const Appearance& a)
{
    auto o = Value::object();
    o.add("species", a.species);
    o.add("sex", a.sex);
    o.add("stature", a.stature);
    o.add("pattern", a.pattern);
    o.add("baseColor", a.baseColor);
    o.add("gradientColor", a.gradientColor);
    o.add("markingColor", a.markingColor);
    o.add("gradientAmount", a.gradientAmount);
    o.add("patternAmount", a.patternAmount);
    // Phase 9's choices, only when made (an older appearance keeps its nine fields).
    for (const auto& [key, value] : {std::pair<const char*, const std::string*>{"coat", &a.coat}, {"gradientTint", &a.gradientTint},
                                     {"markingTint", &a.markingTint}, {"eyes", &a.eyes}, {"build", &a.build}})
        if (!value->empty())
            o.add(key, *value);
    if (!a.markings.empty())
    {
        auto list = Value::array();
        for (const auto& m : a.markings)
        {
            auto j = Value::object();
            j.add("mask", m.mask);
            j.add("color", m.color);
            j.add("opacity", m.opacity);
            list.push(j);
        }
        o.add("markings", list);
    }
    return o;
}

bool readAppearance(const Value& o, Appearance& out)
{
    // The nine fields every appearance has, and phase 9's optional ones; nothing else.
    static const std::set<std::string> optional{"coat", "gradientTint", "markingTint", "eyes", "build", "markings"};
    if (!o.isObject() || o.size() < 9 || o.size() > 9 + optional.size())
        return false;
    for (const auto& [key, value] : o.fields())
        if (optional.count(key) == 0 && key != "species" && key != "sex" && key != "stature" && key != "pattern" && key != "baseColor" &&
            key != "gradientColor" && key != "markingColor" && key != "gradientAmount" && key != "patternAmount")
            return false;
    Appearance a;
    for (const auto& [key, into] : {std::pair<const char*, std::string*>{"coat", &a.coat}, {"gradientTint", &a.gradientTint},
                                    {"markingTint", &a.markingTint}, {"eyes", &a.eyes}, {"build", &a.build}})
        if (const auto* v = o.find(key))
        {
            if (!v->isString() || v->asString().size() > 16)
                return false;
            *into = v->asString();
        }
    if (const auto* list = o.find("markings"))
    {
        if (!list->isArray() || list->items().size() > MaxMarkings)
            return false;
        for (const auto& m : list->items())
        {
            if (!m.isObject() || m.size() != 3 || !m["mask"].isString() || !m["color"].isString())
                return false;
            Marking mark{m.string("mask"), m.string("color"), strictNumber(m, "opacity", -1)};
            a.markings.push_back(mark);
        }
    }
    const auto id = [&](const char* key, std::string& into) {
        const auto* v = o.find(key);
        if (!v || !v->isString() || v->asString().empty() || v->asString().size() > 16)
            return false;
        for (char c : v->asString())
            if (c < 'a' || c > 'z')
                return false;
        into = v->asString();
        return true;
    };
    if (!id("species", a.species) || !id("sex", a.sex) || !id("stature", a.stature) || !id("pattern", a.pattern))
        return false;
    const auto color = [&](const char* key, int& into) {
        const double n = strictNumber(o, key, -1);
        if (n < 0 || n >= CoatColorCount || n != std::floor(n))
            return false;
        into = int(n);
        return true;
    };
    if (!color("baseColor", a.baseColor) || !color("gradientColor", a.gradientColor) || !color("markingColor", a.markingColor))
        return false;
    a.gradientAmount = strictNumber(o, "gradientAmount", -1);
    a.patternAmount = strictNumber(o, "patternAmount", -1);
    if (!validAppearance(a))
        return false;
    out = a;
    return true;
}

Appearance readAppearance(const Value& o)
{
    Appearance a;
    if (!readAppearance(o, a))
        a.species.clear();
    return a;
}

Value entity(const Entity& e, double time)
{
    auto o = Value::object();
    o.add("id", e.id);
    o.add("name", e.name);
    o.add("cell", e.cellId);
    if (e.dead)
        o.add("dead", true);
    o.add("x", e.position.x);
    o.add("y", e.position.y);
    o.add("facing", e.facing);
    o.add("turning", e.turning);
    o.add("moving", std::abs(e.velocity.x) + std::abs(e.velocity.y) > .001 || std::abs(e.input.x) + std::abs(e.input.y) > .001 ||
                        !e.path.empty());
    o.add("postureRemaining", e.postureRemaining);
    o.add("postureTarget", e.postureTarget);
    o.add("npc", e.npc);
    o.add("appearance", appearance(e.appearance));
    o.add("lifeStage", lifeStageName(lifeStage(e.age)));
    o.add("shoulderHeightCm", shoulderHeightCm(e.appearance, e.age));
    o.add("color", e.speakingColor);
    o.add("posture", e.posture);
    o.add("state", e.state);
    o.add("description", e.description);
    o.add("activity", e.activity);
    o.add("typing", e.typing);
    o.add("speaking", e.speakingUntil > time);
    o.add("speakingRemaining", std::max(0.0, e.speakingUntil - time));
    o.add("transitioned", e.transitioned);
    return o;
}

Value persistEntity(const Entity& e, double time)
{
    auto o = entity(e, time);
    o.add("age", e.age);
    o.add("strength", e.strength);
    o.add("wisdom", e.wisdom);
    o.add("lastBirthdayDay", e.lastBirthdayDay);
    o.add("ageNoticePending", e.ageNoticePending);
    o.add("hearing", e.hearing);
    o.add("vision", e.vision);
    o.add("earHealth", e.earHealth);
    o.add("eyeHealth", e.eyeHealth);
    o.add("sneakSkill", e.sneakSkill);
    o.add("hearingSkill", e.hearingSkill);
    o.add("smell", e.smell);
    o.add("noseHealth", e.noseHealth);
    o.add("scentSkill", e.scentSkill);
    o.add("dexterity", e.dexterity);
    o.add("stamina", e.stamina);
    o.add("pace", e.pace);
    o.add("exhausted", e.exhausted);
    if (e.hurt > 0)
        o.add("hurt", e.hurt);
    if (e.downedLeft > 0)
        o.add("downedLeft", e.downedLeft);
    if (e.recoveryUsed >= 0)
        o.add("recoveryUsed", e.recoveryUsed);
    if (e.downsSinceRest > 0)
        o.add("downsSinceRest", double(e.downsSinceRest));
    if (e.restRun > 0)
        o.add("restRun", e.restRun);
    if (e.bedRun > 0)
        o.add("bedRun", e.bedRun);
    if (e.fullRestDay >= 0)
        o.add("fullRestDay", e.fullRestDay);
    if (e.awaySince >= 0)
    {
        o.add("awaySince", e.awaySince);
        o.add("awayInBed", e.awayInBed);
    }
    if (!e.mouth.empty())
        o.add("mouth", e.mouth);
    if (!e.swordKind.empty())
        o.add("swordKind", e.swordKind);
    if (e.scentMaskedUntil > 0)
        o.add("scentMaskedUntil", e.scentMaskedUntil);
    if (e.noPvp)
        o.add("noPvp", true);
    if (e.noHuntPartners)
        o.add("noHuntPartners", true);              // (Doc 53: missing means on.)
    if (e.noWorkPartners)
        o.add("noWorkPartners", true);
    if (!e.wear.empty())
    {
        auto wear = Value::object();
        for (const auto& [item, used] : e.wear)
            wear.add(item, used);
        o.add("wear", wear);
    }
    if (!e.worn.empty())
    {
        auto worn = Value::object();
        for (const auto& [slot, item] : e.worn)
            worn.add(slot, item);
        o.add("worn", worn);
    }
    if (!e.injuries.empty())
    {
        // Acute and lasting injuries (doc 38).
        auto injuries = Value::array();
        for (const auto& i : e.injuries)
        {
            auto j = Value::object();
            j.add("id", i.id);
            j.add("kind", i.kind);
            j.add("type", i.type);
            if (!i.side.empty())
                j.add("side", i.side);
            j.add("cause", i.cause);
            if (!i.from.empty())
                j.add("from", i.from);
            j.add("severity", i.severity);
            if (i.kind == "acute")
            {
                j.add("restLeft", i.restLeft);
                j.add("restFull", i.restFull);
            }
            j.add("gotDay", i.gotDay);
            if (i.cleaned)
                j.add("cleaned", true);             // (Licked clean: doc 55.)
            injuries.push(j);
        }
        o.add("injuries", injuries);
    }
    if (!e.jewellery.empty())
    {
        auto jewellery = Value::array();
        for (const auto& [spot, item] : e.jewellery)
        {
            auto piece = Value::array();
            piece.push(spot);
            piece.push(item);
            jewellery.push(piece);
        }
        o.add("jewellery", jewellery);
    }
    if (!e.gift.empty())
    {
        o.add("gift", e.gift);
        o.add("quickened", e.quickened);
        o.add("mana", e.mana);
    }
    if (e.wardenAttention > 0)
        o.add("wardenAttention", e.wardenAttention);    // Quickened magic others saw (doc 43).
    if (!e.witnessed.empty())
    {
        auto w = Value::object();                      // Keeping the secret (doc 53, 4): IDs and days only.
        for (const auto& [wolf, day] : e.witnessed)
            w.add(wolf, day);
        o.add("witnessed", w);
    }
    if (!e.toldWardens.empty())
    {
        auto t = Value::array();
        for (const auto& wolf : e.toldWardens)
            t.push(wolf);
        o.add("toldWardens", t);
    }
    if (e.vouchedDay > -1e8)
        o.add("vouchedDay", e.vouchedDay);
    if (!e.vouchedFor.empty())
        o.add("vouchedFor", e.vouchedFor);
    if (e.wardenStanding != 0)
        o.add("wardenStanding", e.wardenStanding);
    if (!e.postTown.empty())
        o.add("postTown", e.postTown);              // (Where its letters wait: doc 55.)
    if (e.leftAt > 0)
        o.add("leftAt", e.leftAt);                 // (When its player last left: doc 56, 10.)
    if (!e.lore.empty())
    {
        auto lore = Value::array();                // The journal (doc 54, 7).
        for (const auto& id : e.lore)
            lore.push(id);
        o.add("lore", lore);
    }
    if (!e.bestiary.empty())
    {
        auto b = Value::object();
        for (const auto& [species, seen] : e.bestiary)
        {
            auto s = Value::array();
            s.push(seen.first);
            s.push(seen.second);
            b.add(species, s);
        }
        o.add("bestiary", b);
    }
    if (!e.herbarium.empty())
    {
        auto h = Value::object();
        for (const auto& [item, found] : e.herbarium)
        {
            auto f = Value::array();
            f.push(found.first);
            f.push(found.second);
            h.add(item, f);
        }
        o.add("herbarium", h);
    }
    if (!e.places.empty())
    {
        auto p = Value::array();
        for (const auto& cell : e.places)
            p.push(cell);
        o.add("places", p);
    }
    if (!e.gameSkills.empty())
    {
        auto skills = Value::object();             // Tavern games (doc 54, 5).
        for (const auto& [game, skill] : e.gameSkills)
            skills.add(game, skill);
        o.add("gameSkills", skills);
    }
    if (e.groomedUntil > 0)
    {
        o.add("groomedUntil", e.groomedUntil);     // Grooming (doc 55, 7).
        o.add("groomScentUntil", e.groomScentUntil);
        o.add("groomHalf", e.groomHalf);
        o.add("groomedBy", e.groomedBy);
    }
    if (e.groomedOtherDay > -1e8)
        o.add("groomedOtherDay", e.groomedOtherDay);
    if (e.fedUntil > 0)
        o.add("fedUntil", e.fedUntil), o.add("ateAt", e.ateAt);   // (Meals: doc 55, 6.)
    if (e.awayAtInn)
        o.add("awayAtInn", true);                   // (Away at an inn: doc 54, 1.)
    if (e.groomedSelfDay > -1e8)
        o.add("groomedSelfDay", e.groomedSelfDay);
    if (!e.scents.empty())
    {
        auto list = Value::array();                // Whose scent its goods carry (doc 55): ids and days.
        for (const auto& r : e.scents)
        {
            auto s = Value::object();
            s.add("item", r.item);
            if (!r.maker.empty())
                s.add("maker", r.maker), s.add("madeDay", r.madeDay);
            if (!r.giver.empty())
                s.add("giver", r.giver), s.add("givenDay", r.givenDay);
            s.add("count", r.count);
            list.push(s);
        }
        o.add("scents", list);
    }
    if (e.fightingSkill != 50)
        o.add("fightingSkill", e.fightingSkill);
    if (e.dungeonMaster)
        o.add("dungeonMaster", true);
    // Practice (doc 49): grades, specialty, trade skills, stamina the attribute, migrations had, and the day's practice.
    if (!e.grades.empty())
    {
        auto grades = Value::object();
        for (const auto& [attribute, grade] : e.grades)
            grades.add(attribute, grade);
        o.add("grades", grades);
    }
    if (!e.specialty.empty())
        o.add("specialty", e.specialty);
    if (!e.skills.empty())
    {
        auto skills = Value::object();
        for (const auto& [skill, value] : e.skills)
            skills.add(skill, value);
        o.add("skills", skills);
    }
    if (e.endurance != 50)
        o.add("endurance", e.endurance);
    if (e.progressVersion > 0)
        o.add("progressVersion", double(e.progressVersion));
    if (!e.practice->days.empty() || e.practice->rested > 0 || e.practice->lastGainAt >= 0)
    {
        auto practice = Value::object();
        auto days = Value::object();
        for (const auto& [skill, day] : e.practice->days)
        {
            auto row = Value::array();
            row.push(day.start);
            row.push(day.gained);
            days.add(skill, row);
        }
        practice.add("days", days);
        if (e.practice->rested > 0)
            practice.add("rested", e.practice->rested);
        if (e.practice->lastGainAt >= 0)
            practice.add("lastGainAt", e.practice->lastGainAt);
        o.add("practice", practice);
    }
    return o;
}

Entity readEntity(const Value& o)
{
    Entity e;
    e.id = o.string("id");
    e.name = o.string("name");
    e.cellId = o.string("cell");
    e.position = {number(o, "x"), number(o, "y")};
    e.facing = number(o, "facing");
    e.npc = o.boolean("npc");
    // Legacy records omit appearance; malformed provided objects must not become the valid default.
    if (o.has("appearance"))
        e.appearance = readAppearance(o["appearance"]);
    e.speakingColor = int(std::clamp(number(o, "color"), 0.0, 31.0));
    e.posture = o.string("posture", "standing");
    e.state = o.string("state");
    e.description = o.string("description");
    e.activity = o.string("activity");
    e.hearing = number(o, "hearing", 1);
    e.vision = number(o, "vision", 1);
    e.earHealth = number(o, "earHealth", 1);
    e.eyeHealth = number(o, "eyeHealth", 1);
    const auto marked = [&](const char* key, double absent) { return number(o, key, o.has(key) ? -1.0 : absent); };
    e.sneakSkill = marked("sneakSkill", 0);
    e.hearingSkill = marked("hearingSkill", 0);
    e.smell = marked("smell", 1);
    e.noseHealth = marked("noseHealth", 1);
    e.scentSkill = marked("scentSkill", 0);
    e.dexterity = strictNumber(o, "dexterity", o.has("dexterity") ? -1.0 : 50.0);
    const auto aging = [&](const char* key, double absent) { return strictNumber(o, key, o.has(key) ? -2.0 : absent); };
    const double age = aging("age", 18), notices = aging("ageNoticePending", 0);
    e.age = age >= 0 && age <= 10000 && age == std::floor(age) ? int(age) : -1;
    e.ageNoticePending = notices >= 0 && notices <= 10000 && notices == std::floor(notices) ? int(notices) : -1;
    e.strength = aging("strength", 50);
    e.wisdom = aging("wisdom", 30);
    e.lastBirthdayDay = aging("lastBirthdayDay", -1);
    e.stamina = strictNumber(o, "stamina", o.has("stamina") ? -1.0 : 100.0);
    const double pace = strictNumber(o, "pace", o.has("pace") ? -1.0 : 0.0);
    e.pace = pace >= 0 && pace <= 10 && pace == std::floor(pace) ? int(pace) : -1;
    e.exhausted = o.boolean("exhausted");
    e.dead = o.boolean("dead");
    if (o.has("exhausted") && !o["exhausted"].isBool())
        e.stamina = -1;                            // Malformed fatigue state rejects the complete save.
    if (const double hurt = strictNumber(o, "hurt", o.has("hurt") ? -1.0 : 0.0); hurt >= 0 && hurt <= 100)
        e.hurt = hurt;
    else
        e.stamina = -1;                            // So does a malformed injury.
    if (const double left = strictNumber(o, "downedLeft", o.has("downedLeft") ? -1.0 : 0.0); left >= 0 && left <= 86400)
        e.downedLeft = left;
    else
        e.stamina = -1;
    e.recoveryUsed = strictNumber(o, "recoveryUsed", -1.0);
    e.downsSinceRest = std::clamp(int(strictNumber(o, "downsSinceRest", 0.0)), 0, 99);
    e.restRun = std::clamp(strictNumber(o, "restRun", 0.0), 0.0, 1e6);
    e.bedRun = std::clamp(strictNumber(o, "bedRun", 0.0), 0.0, 1e6);
    e.fullRestDay = strictNumber(o, "fullRestDay", -1.0);
    e.awaySince = strictNumber(o, "awaySince", -1.0);
    e.awayInBed = o.boolean("awayInBed");
    e.mouth = o.string("mouth");
    // What is worn (doc 35): only what fits where it is said to be; whether the purse still has it is checked on joining.
    for (const auto& [slot, item] : o.object("worn").fields())
        if (const auto* piece = items::wearable(item.asString({})); piece && items::wearSlot(slot))
        {
            const auto fits = items::slotsFor(*piece);
            if (std::find(fits.begin(), fits.end(), slot) != fits.end())
                e.worn[slot] = item.asString({});       // (As saved: a masterwork keeps its maker's mark.)
        }
    for (const auto& piece : o.array("jewellery"))
        if (piece.isArray() && piece.items().size() == 2 && e.jewellery.size() < items::MaxJewellery)
            if (const auto* item = items::wearable(piece.items()[1].asString({})); item && items::spotAllowed(*item, piece.items()[0].asString({})))
                e.jewellery.emplace_back(piece.items()[0].asString({}), piece.items()[1].asString({}));
    if (!e.mouth.empty() && e.mouth != "sword")
        e.mouth.clear();
    // The sword's kind, if it is one; the wear on gear, for goods that exist, within bounds.
    if (const auto kind = o.string("swordKind"); e.mouth == "sword" && items::blade(kind))
        e.swordKind = kind;
    e.scentMaskedUntil = std::max(0.0, strictNumber(o, "scentMaskedUntil", 0.0));
    e.noPvp = !e.npc && o.boolean("noPvp");
    e.noHuntPartners = !e.npc && o.boolean("noHuntPartners");
    e.noWorkPartners = !e.npc && o.boolean("noWorkPartners");
    for (const auto& [item, used] : o.object("wear").fields())
        if (items::good(item) && used.asNumber(-1) >= 0 && used.asNumber(-1) <= 100000 && e.wear.size() < 64)
            e.wear[item] = used.asNumber(0);
    // Injuries (doc 38): only known kinds, within bounds; anything else is dropped rather than refusing the save.
    for (const auto& j : o.array("injuries"))
    {
        if (!j.isObject() || e.injuries.size() >= injury::MostKept)
            continue;
        Injury i;
        i.id = j.string("id").substr(0, 96);
        i.type = j.string("type");
        i.kind = injury::lasting(i.type) ? "lasting" : "acute";
        i.side = j.string("side") == "left" || j.string("side") == "right" ? j.string("side") : "";
        i.cause = j.string("cause").substr(0, 16);
        i.from = j.string("from").substr(0, 80);
        i.severity = std::clamp(int(strictNumber(j, "severity", 1)), 1, 3);
        i.restFull = std::clamp(strictNumber(j, "restFull", 0), 0.0, 24.0 * 60);
        i.restLeft = std::clamp(strictNumber(j, "restLeft", 0), 0.0, i.restFull);
        i.gotDay = std::max(0.0, strictNumber(j, "gotDay", 0));
        i.cleaned = j.boolean("cleaned");
        if (i.id.empty() || !injury::known(i.type) || j.string("kind") != i.kind || (i.kind == "acute" && i.restLeft <= 0))
            continue;
        e.injuries.push_back(std::move(i));
    }
    e.gift = o.string("gift");
    if (!e.gift.empty() && !gifts::known(e.gift))         // (A family in Data/Gifts: doc 43.)
        e.gift.clear();
    e.quickened = o.boolean("quickened");
    e.wardenAttention = std::clamp(strictNumber(o, "wardenAttention", 0.0), 0.0, 1e9);
    for (const auto& [wolf, day] : o.object("witnessed").fields())
        if (day.isNumber() && wolf.size() <= 128 && e.witnessed.size() < 500)
            e.witnessed[wolf] = day.asNumber(0);
    for (const auto& wolf : o.array("toldWardens"))
        if (wolf.isString() && e.toldWardens.size() < 500)
            e.toldWardens.insert(wolf.asString());
    e.vouchedDay = strictNumber(o, "vouchedDay", -1e9);
    e.vouchedFor = o.string("vouchedFor").substr(0, 128);
    e.wardenStanding = std::clamp(strictNumber(o, "wardenStanding", 0.0), -100.0, 100.0);
    e.postTown = o.string("postTown").substr(0, 128);
    e.leftAt = std::max(0.0, strictNumber(o, "leftAt", 0));
    for (const auto& id : o.array("lore"))
        if (id.isString() && e.lore.size() < 500)
            e.lore.push_back(id.asString().substr(0, 64));
    for (const auto& [species, seen] : o.object("bestiary").fields())
        if (seen.isArray() && seen.items().size() == 2 && e.bestiary.size() < 200)
            e.bestiary[species.substr(0, 64)] = {seen.items()[0].asNumber(), int(seen.items()[1].asNumber())};
    for (const auto& [item, found] : o.object("herbarium").fields())
        if (found.isArray() && found.items().size() == 2 && e.herbarium.size() < 200)
            e.herbarium[item.substr(0, 64)] = {found.items()[0].asNumber(), found.items()[1].asString(std::string()).substr(0, 96)};
    for (const auto& cell : o.array("places"))
        if (cell.isString() && e.places.size() < 5000)
            e.places.insert(cell.asString().substr(0, 128));
    for (const auto& [game, skill] : o.object("gameSkills").fields())
        if (skill.isNumber() && e.gameSkills.size() < 8 && game.size() <= 32)
            e.gameSkills[game] = std::clamp(skill.asNumber(), 0., 100.);
    e.groomedUntil = strictNumber(o, "groomedUntil", -1);
    e.groomScentUntil = strictNumber(o, "groomScentUntil", -1);
    e.groomHalf = o.boolean("groomHalf");
    e.groomedBy = o.string("groomedBy").substr(0, 128);
    e.groomedOtherDay = strictNumber(o, "groomedOtherDay", -1e9);
    e.groomedSelfDay = strictNumber(o, "groomedSelfDay", -1e9);
    e.fedUntil = strictNumber(o, "fedUntil", -1);
    e.awayAtInn = o.boolean("awayAtInn");
    e.ateAt = strictNumber(o, "ateAt", -1);
    for (const auto& s : o.array("scents"))
        if (s.isObject() && e.scents.size() < 60 && !s.string("item").empty() && s.number("count") > 0)
            e.scents.push_back({s.string("item").substr(0, 128), s.string("maker").substr(0, 128), s.string("giver").substr(0, 128),
                                std::clamp(int(s.number("count")), 1, 9999), s.number("madeDay", -1), s.number("givenDay", -1)});
    e.dungeonMaster = !e.npc && o.boolean("dungeonMaster");
    e.mana = std::clamp(strictNumber(o, "mana", 0.0), 0.0, 100.0);
    e.fightingSkill = std::clamp(strictNumber(o, "fightingSkill", 50.0), 0.0, 100.0);
    // Practice (doc 49): only known attributes and trade skills, within bounds; what doesn't fit is dropped rather than
    // refusing the save.
    for (const auto& [attribute, grade] : o.object("grades").fields())
        if (const auto* s = practice::skill(attribute); s && s->attribute && (grade.asString({}) == "weak" || grade.asString({}) == "strong"))
            e.grades[attribute] = grade.asString({});
    e.specialty = o.string("specialty").substr(0, 32);
    for (const auto& [skill, value] : o.object("skills").fields())
        if (const auto* s = practice::skill(skill); s && !s->attribute && s->field.empty() && value.isNumber())
            e.skills[skill] = std::clamp(value.asNumber(0), 0.0, 100.0);
    e.endurance = std::clamp(strictNumber(o, "endurance", 50.0), 0.0, 100.0);
    e.progressVersion = std::clamp(int(strictNumber(o, "progressVersion", 0.0)), 0, 1000);
    const auto& practised = o.object("practice");
    for (const auto& [skill, day] : practised.object("days").fields())
        if (practice::skill(skill) && day.isArray() && day.items().size() == 2 && day.items()[0].isNumber() && day.items()[1].isNumber())
            e.practice->days[skill] = {day.items()[0].asNumber(0), std::clamp(day.items()[1].asNumber(0), 0.0, 1e6)};
    e.practice->rested = std::clamp(strictNumber(practised, "rested", 0.0), 0.0, 1e6);
    e.practice->lastGainAt = strictNumber(practised, "lastGainAt", -1.0);
    e.postureTarget = o.string("postureTarget");
    e.postureRemaining = number(o, "postureRemaining");
    e.turnTarget = e.facing;                       // Input, paths and manual turn intents are never reloaded.
    return e;
}

void practiceView(Value& o, const Entity& e, double now)
{
    auto attributes = Value::array(), skills = Value::array();
    const auto& rules = practice::rules();
    for (const auto& s : practice::skills())
    {
        auto row = Value::object();
        row.add("id", s.id);
        row.add("name", s.name);
        const double value = World::practiceValue(e, s), cap = World::practiceCap(e, s);
        row.add("value", value);
        row.add("cap", cap);
        if (s.percent)
            row.add("percent", true);
        if (s.attribute)
        {
            row.add("short", s.shortName);
            const auto grade = e.grades.find(s.id);
            row.add("grade", grade == e.grades.end() ? std::string("plain") : grade->second);
            attributes.push(row);
            continue;
        }
        if (const auto* sp = practice::specialty(e.specialty); sp && sp->skill == s.id)
            row.add("specialty", true);
        // Easing off: today's gains have reached the soft limit (and the rolling day hasn't come round).
        if (const auto day = e.practice->days.find(s.id); day != e.practice->days.end() && now - day->second.start < rules.softDay &&
                                                          now >= day->second.start && day->second.gained >= s.softPerDay)
            row.add("easing", true);
        skills.push(row);
    }
    o.set("attributes", attributes);
    o.set("skills", skills);
    o.set("restedPractice", e.practice->rested);
}

void privatePace(Value& o, const Entity& e)
{
    o.set("age", e.age);
    o.set("strength", e.strength);
    o.set("wisdom", e.wisdom);
    o.set("effectiveDexterity", effectiveDexterity(e));
    o.set("dexterity", e.dexterity);
    o.set("stamina", e.stamina);
    o.set("pace", e.pace);
    o.set("effectivePace", effectivePace(e));
    o.set("paceName", paceName(effectivePace(e)));
    o.set("staminaRate", e.staminaRate);
    o.set("staminaRecovery", step::StaminaRecovery * (e.npc ? 1.0 : practice::staminaRecovery(e.endurance)));   // (A second: doc 49.)
    o.set("exhausted", e.exhausted);
    auto sprint = e;
    sprint.pace = 10;
    sprint.exhausted = false;
    sprint.posture = "standing";
    o.set("topSpeed", paceSpeed(sprint));
    o.set("currentSpeed", std::sqrt(e.velocity.x * e.velocity.x + e.velocity.y * e.velocity.y));
}

Value mapCell(const MapCell& m, bool includeGlyphs)
{
    auto o = Value::object();
    o.add("id", m.id);
    o.add("name", m.name);
    o.add("width", m.width);
    o.add("height", m.height);
    o.add("x", m.worldX);
    o.add("y", m.worldY);
    o.add("z", m.worldZ);
    o.add("knowledge", knowledgeName(m.knowledge));
    o.add("current", m.current);
    o.add("visible", m.visible);
    if (includeGlyphs)
        o.add("glyphs", std::string(m.rememberedGlyphs.begin(), m.rememberedGlyphs.end()));
    return o;
}

Value travel(const TravelState& t)
{
    auto o = Value::object();
    o.add("active", t.active);
    o.add("paused", t.paused);
    o.add("destination", t.destination);
    o.add("status", t.status);
    o.add("nextDoor", t.nextDoor);
    auto route = Value::array();
    for (const auto& id : t.route)
        route.push(id);
    o.add("route", route);
    return o;
}

Value wind(const Wind& w)
{
    auto o = Value::object();
    o.add("direction", w.direction);
    o.add("strength", w.strength);
    o.add("variable", w.variable);
    return o;
}

Wind readWind(const Value& o)
{
    Wind w;
    w.direction = number(o, "direction");
    w.strength = number(o, "strength", -1);
    w.variable = o.boolean("variable");
    if (!o.isObject() || !o["direction"].isNumber() || !o["variable"].isBool())
        w.strength = -1;                           // Core restore rejects the entire invalid record.
    return w;
}

Value environment(const Environment& e)
{
    auto o = Value::object(), date = Value::object();
    date.add("absoluteDays", e.date.absoluteDays);
    date.add("year", e.date.year);
    date.add("dayOfYear", e.date.dayOfYear);
    date.add("dayOfSeason", e.date.dayOfSeason);
    date.add("season", calendar::seasonName(e.date.season));
    date.add("weekday", calendar::weekdayName(calendar::weekdayOf(e.date.absoluteDays)));
    date.add("moonName", e.date.moonName);
    date.add("moonPhase", e.date.moonPhase);
    date.add("moonIllumination", e.date.moonIllumination);
    o.add("calendar", date);
    o.add("hour", e.hour);
    o.add("phase", e.phase);
    o.add("daylight", e.daylight);
    o.add("illumination", e.illumination);
    o.add("sight", e.sight);
    o.add("hearing", e.hearing);
    o.add("scent", e.scent);
    o.add("movement", e.movement);
    o.add("artificialLight", e.artificialLight);
    o.add("daylightAccess", e.daylightAccess);
    o.add("glowStrength", e.glowStrength);
    o.add("lightingTone", e.lightingTone);
    o.add("lightSource", e.lightSource);
    return o;
}

Value lighting(const Lighting& l)
{
    auto o = Value::object();
    o.add("artificial", l.artificial);
    o.add("daylightAccess", l.daylightAccess);
    o.add("tone", l.tone);
    return o;
}

Lighting readLighting(const Value& o)
{
    Lighting l;
    l.artificial = strictNumber(o, "artificial", -1);
    l.daylightAccess = strictNumber(o, "daylightAccess", -1);
    l.tone = o.string("tone");
    if (!o.isObject() || o.size() != 3)
        l.artificial = -1;
    return l;                                      // Core validation rejects the entire record if malformed.
}

std::string environmentDescription(const Cell& cell, const Environment& e)
{
    const std::string time = e.phase;
    if (!cell.outdoors)
    {
        const char* light = e.lightSource == "dark" ? "The room is unlit and dark; ears and nose remain useful."
                            : e.illumination < .6   ? "The room is dimly lit, limiting distant vision."
                            : e.glowStrength > .1 && e.lightingTone == "warm" ? "Warm artificial light keeps the room clearly visible."
                            : e.lightSource == "daylight" || e.glowStrength < .1 ? "Daylight keeps the room clearly visible."
                                                                                 : "Artificial light keeps the room clearly visible.";
        return "It is " + time + ". " + light + " This room is sheltered from outdoor weather.";
    }
    const char* conditions;
    // The weather where it was asked for (doc 29, phase 7), else the cell's.
    const Weather kind = e.weather != Weather::Clear || e.intensity > 0 ? e.weather : cell.weather;
    switch (kind)
    {
    case Weather::Rain: conditions = "Rain blurs the distance, masks quieter sounds, scatters airborne scent, and slows the footing."; break;
    case Weather::Snow: conditions = "Snow veils the distance, muffles sound, weakens airborne scent, and makes travel slower."; break;
    case Weather::Fog: conditions = "Fog conceals the distance. Ears and nose can still find what the eyes cannot."; break;
    case Weather::Overcast: conditions = "Low cloud flattens the light; the distance is grey but plain to see."; break;
    case Weather::Storm:
        conditions = "A storm lashes the ground. Thunder drowns quiet sounds, rain strips the air of scent, and the footing is treacherous.";
        break;
    case Weather::Sandstorm: conditions = "Driven sand blinds the eyes, clogs the nose and hisses over every quieter sound."; break;
    default:
        conditions = e.phase == "night" ? "The sky is clear, but darkness conceals distant movement."
                                        : "The sky is clear; the changing light shapes what you can see.";
    }
    std::string strength;
    if (kind != Weather::Clear && kind != Weather::Overcast && e.intensity > 0)
        strength = e.intensity < .35 ? " Here it is only light, at the edge of it." : e.intensity > .8 ? " Here it is at its heaviest." : "";
    return "It is " + time + ". " + conditions + strength;
}

double readClockOffset(const Value& o)
{
    return o.has("clockOffsetHours") ? strictNumber(o, "clockOffsetHours", -1) : 12.0;
}

char heightChar(double height)
{
    return char('0' + std::clamp(int(std::lround(height * 2.0)) + 32, 0, 64));
}

double heightFromChar(char c)
{
    return c >= '0' && c <= 'p' ? (int(c - '0') - 32) / 2.0 : 0.0;
}

Weather readWeather(const Value& v)
{
    const double n = v.isNumber() ? v.asNumber() : -1;
    if (!std::isfinite(n) || n < 0 || n >= WeatherKinds || std::floor(n) != n)
        return static_cast<Weather>(-1);
    return static_cast<Weather>(int(n));
}

Value senses(const Snapshot& snapshot)
{
    auto o = Value::object(), cues = Value::array();
    for (const auto& cue : snapshot.scentCues)
    {
        auto j = Value::object();
        j.add("sector", cue.sector);
        j.add("strength", cue.strength);
        j.add("windborne", cue.windborne);
        cues.push(j);
    }
    o.add("scentCues", cues);
    o.add("movementHeard", snapshot.movementHeard);
    return o;
}

Result paceCommand(World& world, const std::string& id, const Value& o)
{
    const double pace = strictNumber(o, "pace", -1);
    if (pace < 0 || pace > 10 || pace != std::floor(pace))
        return {false, "Pace must be a whole step from 0 to 10.", {}};
    return world.setPace(id, int(pace));
}

Result environmentCommand(World& world, const std::string& cellId, const std::string& type, const std::string& value, bool devTools)
{
    if (!devTools)
        return {false, "Environment controls are available only in development sessions.", cellId};
    if (type == "calendar")
    {
        if (value == "season")
        {
            // To the first day of the next season (farm work's seasons, doc 53, are tried this way).
            const auto now = calendar::calendarAt(world.calendarDays()).season;
            Result step{false, "Unknown calendar step.", cellId};
            for (int day = 0; day < 120 && calendar::calendarAt(world.calendarDays()).season == now; ++day)
                step = world.advanceCalendar(1);
            return step;
        }
        return value == "day" ? world.advanceCalendar(1) : value == "year" ? world.advanceCalendar(365)
                                                                            : Result{false, "Unknown calendar step.", cellId};
    }
    if (type == "weather" && value == "seasonal")
        return world.useSeasonalWeather(cellId);
    if (type == "lighting")
    {
        if (value != "warm" && value != "unlit" && value != "daylit" && value != "cool")
            return {false, "Unknown lighting preset.", cellId};
        return world.setLighting(cellId, value == "unlit" || value == "daylit" ? 0.0 : 1.0, value == "unlit" ? 0.0 : 1.0,
                                 value == "cool" ? "cool" : "warm");
    }
    if (type == "time")
    {
        if (value != "dawn" && value != "day" && value != "dusk" && value != "night")
            return {false, "Unknown time-of-day preset.", cellId};
        return world.setTimeOfDay(value == "dawn" ? 6.0 : value == "day" ? 12.0 : value == "dusk" ? 18.0 : 0.0);
    }
    Weather weather = Weather::Clear;
    if (type != "weather" || !world.cell(cellId) || !parseWeather(value, weather))
        return {false, "Unknown weather preset or cell.", cellId};
    world.setWeather(cellId, weather);
    return {true, "Weather updated.", cellId};
}

// --------------------------------------------------------------------------- The society

Value economyAccount(const EconomyAccount& a)
{
    auto j = Value::object(), stock = Value::object();
    j.add("cash", a.cash);
    for (const auto& [item, n] : a.stock)
        stock.add(item, n);
    j.add("stock", stock);
    return j;
}

Value economyLedger(const std::vector<EconomyEntry>& entries)
{
    auto ledger = Value::array();
    for (const auto& e : entries)
    {
        auto j = Value::object();
        j.add("sequence", e.sequence); j.add("day", e.day); j.add("coins", e.coins); j.add("quantity", e.quantity);
        j.add("kind", e.kind); j.add("from", e.from); j.add("to", e.to); j.add("item", e.item);
        ledger.push(j);
    }
    return ledger;
}

Value careerPosition(const PositionState& p)
{
    auto j = Value::object();
    j.add("holder", p.holder); j.add("apprentice", p.apprentice); j.add("lastHolder", p.lastHolder);
    j.add("vacantSince", p.vacantSince); j.add("newcomerAsked", p.newcomerAsked);
    return j;
}

Value monthBooks(const MonthBooks& b)
{
    auto o = Value::object(), start = Value::object(), unearned = Value::object();
    o.add("month", double(b.month));
    o.add("revision", double(b.revision));
    for (const auto& [id, cash] : b.start)
        start.add(id, double(cash));
    for (const auto& [id, cash] : b.unearned)
        unearned.add(id, double(cash));
    o.add("start", start);
    o.add("unearned", unearned);
    return o;
}

Value economyMemory(const EconomyMemory& m)
{
    auto o = Value::object(), unpaid = Value::object(), outgoing = Value::object();
    o.add("revision", double(m.revision));
    for (const auto& [id, day] : m.unpaidSince)
        unpaid.add(id, day);
    for (const auto& [id, avg] : m.outgoing)
        outgoing.add(id, avg);
    auto condition = Value::object();
    for (const auto& [id, c] : m.condition)
        condition.add(id, c);
    o.add("condition", condition);
    o.add("purses", double(m.purses));
    o.add("tills", double(m.tills));
    auto loans = Value::object();
    for (const auto& [till, loan] : m.loans)
    {
        auto pair = Value::array();
        pair.push(double(loan.first));
        pair.push(double(loan.second));
        loans.add(till, pair);
    }
    o.add("loans", loans);
    auto keeper = Value::object(), comfort = Value::object(), toWork = Value::array(), improved = Value::object();
    for (const auto& [home, who] : m.keeper)
        keeper.add(home, who);
    for (const auto& [home, days] : m.comfort)
        comfort.add(home, double(days));
    for (const auto& home : m.toWork)
        toWork.push(home);
    for (const auto& [pid, level] : m.improved)
        improved.add(pid, double(level));
    auto neglected = Value::object();
    for (const auto& [pid, weeks] : m.neglected)
        neglected.add(pid, double(weeks));
    o.add("neglected", neglected);
    auto lodging = Value::object();
    for (const auto& [who, home] : m.lodging)
        lodging.add(who, home);
    o.add("lodging", lodging);
    o.add("keeper", keeper);
    o.add("comfort", comfort);
    o.add("toWork", toWork);
    o.add("improved", improved);
    o.add("unpaidSince", unpaid);
    o.add("outgoing", outgoing);
    return o;
}

EconomyMemory readEconomyMemory(const Value& o)
{
    EconomyMemory m;
    if (!o.isObject())
        return m;
    if (const auto* r = o.find("revision"); r && r->isNumber() && r->asNumber() >= 0 && r->asNumber() < 1e15)
        m.revision = std::int64_t(r->asNumber());
    for (const auto& [id, v] : o.object("unpaidSince").fields())
        if (v.isNumber() && v.asNumber() >= 0 && v.asNumber() < 1e9)
            m.unpaidSince[id] = v.asNumber();
    for (const auto& [id, v] : o.object("outgoing").fields())
        if (v.isNumber() && v.asNumber() >= 0 && v.asNumber() < 1e12)
            m.outgoing[id] = v.asNumber();
    for (const auto& [id, v] : o.object("condition").fields())
        if (v.isNumber() && v.asNumber() >= 0 && v.asNumber() <= 100)
            m.condition[id] = v.asNumber();
    for (const auto& [till, v] : o.object("loans").fields())
        if (v.isArray() && v.items().size() == 2 && v.items()[0].isNumber() && v.items()[1].isNumber() && v.items()[0].asNumber() > 0 &&
            v.items()[0].asNumber() < 1e9)
            m.loans[till] = {std::int64_t(v.items()[0].asNumber()), std::int64_t(v.items()[1].asNumber())};
    if (const auto* t = o.find("tills"); t && t->isNumber() && t->asNumber() >= 0 && t->asNumber() <= 100)
        m.tills = int(t->asNumber());            // (Saved before doc 46's tills: 0, so an older world founds them once.)
    if (const auto* p = o.find("purses"); p && p->isNumber() && p->asNumber() >= 0 && p->asNumber() <= 100)
        m.purses = int(p->asNumber());           // (Saved before starting money: 0, so an older world gets it once.)
    for (const auto& [home, v] : o.object("keeper").fields())
        if (v.isString())
            m.keeper[home] = v.asString();
    for (const auto& [home, v] : o.object("comfort").fields())
        if (v.isNumber() && std::abs(v.asNumber()) < 1e6)
            m.comfort[home] = int(v.asNumber());
    for (const auto& v : o.array("toWork"))
        if (v.isString())
            m.toWork.insert(v.asString());
    for (const auto& [pid, v] : o.object("improved").fields())
        if (v.isNumber() && v.asNumber() >= 0 && v.asNumber() <= 10)
            m.improved[pid] = int(v.asNumber());
    for (const auto& [pid, v] : o.object("neglected").fields())
        if (v.isNumber() && v.asNumber() >= 0 && v.asNumber() <= 100)
            m.neglected[pid] = int(v.asNumber());
    for (const auto& [who, v] : o.object("lodging").fields())
        if (v.isString() && v.asString().size() < 256)
            m.lodging[who] = v.asString();
    return m;
}

Value houseState(const HouseState& h)
{
    auto o = Value::object(), owner = Value::object(), propped = Value::object();
    o.add("founded", h.founded);
    o.add("day", double(h.day));
    o.add("revision", double(h.revision));
    for (const auto& [pid, house] : h.owner)
        owner.add(pid, house);
    for (const auto& [pid, days] : h.propped)
    {
        auto list = Value::array();
        for (const double d : days)
            list.push(d);
        propped.add(pid, list);
    }
    o.add("owner", owner);
    o.add("propped", propped);
    return o;
}

HouseState readHouseState(const Value& o)
{
    HouseState h;
    if (!o.isObject())
        return h;
    if (o["founded"].isBool())
        h.founded = o["founded"].asBool();
    if (const auto* d = o.find("day"); d && d->isNumber() && d->asNumber() >= -1 && d->asNumber() < 1e9)
        h.day = std::int64_t(d->asNumber());
    if (const auto* r = o.find("revision"); r && r->isNumber() && r->asNumber() >= 0 && r->asNumber() < 1e15)
        h.revision = std::int64_t(r->asNumber());
    for (const auto& [pid, v] : o.object("owner").fields())
        if (v.isString() && v.asString().rfind("house:", 0) == 0)
            h.owner[pid] = v.asString();
    for (const auto& [pid, list] : o.object("propped").fields())
        for (const auto& d : list.items())
            if (d.isNumber() && h.propped[pid].size() < 64)
                h.propped[pid].push_back(d.asNumber());
    return h;
}

MonthBooks readMonthBooks(const Value& o)
{
    MonthBooks b;
    if (!o.isObject())
        return b;
    if (const auto* m = o.find("month"); m && m->isNumber() && m->asNumber() >= -1 && m->asNumber() < 1e8)
        b.month = std::int64_t(m->asNumber());
    if (const auto* r = o.find("revision"); r && r->isNumber() && r->asNumber() >= 0 && r->asNumber() < 1e15)
        b.revision = std::int64_t(r->asNumber());
    for (const auto& [id, v] : o.object("start").fields())
        if (v.isNumber() && std::abs(v.asNumber()) < 1e12) b.start[id] = std::int64_t(v.asNumber());
    for (const auto& [id, v] : o.object("unearned").fields())
        if (v.isNumber() && std::abs(v.asNumber()) < 1e12) b.unearned[id] = std::int64_t(v.asNumber());
    return b;
}

Value society(const SocietyState& s)
{
    auto o = Value::object();
    o.add("enabled", s.enabled);
    o.add("minted", s.minted);
    o.add("sunk", s.sunk);
    o.add("nextEntry", s.nextEntry);
    o.add("budgetDay", s.budgetDay);
    o.add("exportsRemaining", s.exportsRemaining);
    o.add("importsRemaining", s.importsRemaining);
    o.add("herbPatch", s.herbPatch);
    o.add("decisionRemainder", s.decisionRemainder);
    o.add("craftingStocked", s.craftingStocked);
    auto accounts = Value::object();
    for (const auto& [id, a] : s.accounts)
        accounts.add(id, economyAccount(a));
    o.add("accounts", accounts);
    auto residents = Value::object();
    for (const auto& [id, r] : s.residents)
    {
        auto j = Value::object();
        j.add("role", r.role); j.add("task", r.task); j.add("reason", r.reason); j.add("goalCell", r.goalCell);
        j.add("hunger", r.hunger); j.add("fatigue", r.fatigue); j.add("progress", r.progress); j.add("goalX", r.goalX);
        j.add("goalY", r.goalY); j.add("wagesToday", r.wagesToday);
        j.add("homeCell", r.homeCell); j.add("relocationCell", r.relocationCell);
        j.add("homeX", r.homeX); j.add("homeY", r.homeY); j.add("relocationX", r.relocationX); j.add("relocationY", r.relocationY);
        residents.add(id, j);
    }
    o.add("residents", residents);
    o.add("ledger", economyLedger(s.ledger));
    auto careers = Value::object(), positions = Value::object(), skill = Value::object(), mourning = Value::object(),
         estates = Value::object();
    careers.add("day", double(s.careers.day));
    for (const auto& [id, p] : s.careers.positions)
        positions.add(id, careerPosition(p));
    for (const auto& [key, v] : s.careers.skill)
        skill.add(key, v);
    for (const auto& [id, m] : s.careers.mourning)
    {
        auto j = Value::object();
        j.add("until", m.until); j.add("whom", m.whom);
        mourning.add(id, j);
    }
    for (const auto& [id, day] : s.careers.estates)
        estates.add(id, day);
    careers.add("positions", positions); careers.add("skill", skill); careers.add("mourning", mourning); careers.add("estates", estates);
    auto spouses = Value::object(), parents = Value::object(), lastBirth = Value::object(), births = Value::object();
    for (const auto& [id, other] : s.careers.spouses)
        spouses.add(id, other);
    for (const auto& [child, of] : s.careers.parents)
    {
        auto list = Value::array();
        for (const auto& p : of)
            list.push(p);
        parents.add(child, list);
    }
    for (const auto& [key, day] : s.careers.lastBirth)
        lastBirth.add(key, day);
    for (const auto& [key, n] : s.careers.births)
        births.add(key, n);
    careers.add("spouses", spouses); careers.add("parents", parents); careers.add("lastBirth", lastBirth); careers.add("births", births);
    o.add("careers", careers);
    o.add("books", monthBooks(s.books));
    o.add("houses", houseState(s.houses));
    o.add("memory", economyMemory(s.memory));
    o.add("orchestrator", orchestra::stateJson(s.orchestrator));   // (Doc 46.)
    return o;
}

CareerState readCareers(const Value& o)
{
    CareerState c;
    if (!o.isObject())
        return c;
    if (const auto* day = o.find("day"); day && day->isNumber() && day->asNumber() >= -1 && day->asNumber() < 1e9)
        c.day = std::int64_t(day->asNumber());
    for (const auto& [id, a] : o.object("positions").fields())
        if (a.isObject())
        {
            PositionState p;
            p.holder = a.string("holder"); p.apprentice = a.string("apprentice"); p.lastHolder = a.string("lastHolder");
            if (a["vacantSince"].isNumber()) p.vacantSince = a["vacantSince"].asNumber();
            if (a["newcomerAsked"].isBool()) p.newcomerAsked = a["newcomerAsked"].asBool();
            c.positions[id] = p;
        }
    for (const auto& [key, v] : o.object("skill").fields())
        if (v.isNumber()) c.skill[key] = v.asNumber();
    for (const auto& [id, a] : o.object("mourning").fields())
        if (a.isObject()) c.mourning[id] = {number(a, "until"), a.string("whom")};
    for (const auto& [id, v] : o.object("estates").fields())
        if (v.isNumber()) c.estates[id] = v.asNumber();
    for (const auto& [id, v] : o.object("spouses").fields())
        if (v.isString()) c.spouses[id] = v.asString();
    for (const auto& [child, list] : o.object("parents").fields())
        for (const auto& p : list.items())
            if (p.isString()) c.parents[child].push_back(p.asString());
    for (const auto& [key, v] : o.object("lastBirth").fields())
        if (v.isNumber()) c.lastBirth[key] = v.asNumber();
    for (const auto& [key, v] : o.object("births").fields())
        if (v.isNumber()) c.births[key] = std::clamp(int(v.asNumber()), 0, 3);
    return c;
}

SocietyState readSociety(const Value& o)
{
    SocietyState s;
    if (!o.isObject())
    {
        s.minted = -1;
        return s;
    }
    // Twelve fields; careers since Phase 4; craftingStocked since crafting (doc 35, Phase 5).
    const std::size_t fields = o.size() - (o.has("careers") ? 1 : 0) - (o.has("craftingStocked") ? 1 : 0) - (o.has("books") ? 1 : 0) - (o.has("houses") ? 1 : 0) - (o.has("memory") ? 1 : 0) -
                              (o.has("orchestrator") ? 1 : 0);
    bool valid = fields == 12;
    const auto integer = [&](const Value& j, const char* key, double max) -> std::int64_t {
        const double n = strictNumber(j, key, -1);
        if (n < 0 || n > max || n != std::floor(n)) { valid = false; return 0; }
        return std::int64_t(n);
    };
    const auto real = [&](const Value& j, const char* key) { const double n = strictNumber(j, key, -1); if (n < 0) valid = false; return n; };
    const auto text = [&](const Value& j, const char* key) { const auto* v = j.find(key); if (!v || !v->isString()) valid = false; return v ? v->asString("") : std::string(); };
    if (!o["enabled"].isBool()) valid = false;
    s.enabled = o.boolean("enabled");
    s.minted = integer(o, "minted", 1e12); s.sunk = integer(o, "sunk", 1e12);
    s.nextEntry = integer(o, "nextEntry", 1e12); s.budgetDay = integer(o, "budgetDay", 365000000);
    s.exportsRemaining = int(integer(o, "exportsRemaining", 8));
    s.importsRemaining = int(integer(o, "importsRemaining", 4));
    s.herbPatch = int(integer(o, "herbPatch", 60)); s.decisionRemainder = real(o, "decisionRemainder");
    s.craftingStocked = o.has("craftingStocked") ? int(integer(o, "craftingStocked", 100)) : 0;   // Saved before crafting: 0.
    s.books = readMonthBooks(o["books"]);
    s.houses = readHouseState(o["houses"]);       // Saved before doc 42's houses: none founded yet.
    s.memory = readEconomyMemory(o["memory"]);         // Saved before doc 42: no books yet (they open at the next new day).
    s.orchestrator = orchestra::readState(o["orchestrator"]);   // Saved before doc 46: the orchestrator starts afresh.
    const auto& accounts = o["accounts"];
    const auto& residents = o["residents"];
    if (!accounts.isObject() || accounts.size() > MaxAccounts || !residents.isObject() || residents.size() > MaxResidents)
        valid = false;
    if (accounts.isObject() && accounts.size() <= 4096)
        for (const auto& [id, a] : accounts.fields())
        {
            EconomyAccount account;
            account.cash = integer(a, "cash", 1e9);
            const auto& stock = a["stock"];
            if (!a.isObject() || a.size() != 2 || !stock.isObject() || stock.size() > MaxGoodsKinds) valid = false;
            if (stock.isObject() && stock.size() <= MaxGoodsKinds)
                for (const auto& [item, _] : stock.fields())
                    account.stock[item] = int(integer(stock, item.c_str(), 10000));
            s.accounts[id] = account;
        }
    if (residents.isObject() && residents.size() <= MaxResidents)
        for (const auto& [id, a] : residents.fields())
        {
            ResidentLife r;
            if (!a.isObject() || (a.size() != 10 && a.size() != 16)) valid = false;
            r.role = text(a, "role"); r.task = text(a, "task"); r.reason = text(a, "reason"); r.goalCell = text(a, "goalCell");
            r.hunger = real(a, "hunger"); r.fatigue = real(a, "fatigue"); r.progress = real(a, "progress");
            r.goalX = real(a, "goalX"); r.goalY = real(a, "goalY"); r.wagesToday = int(integer(a, "wagesToday", Society::PaidSpells));
            if (a.size() == 16)
            {
                r.homeCell = text(a, "homeCell"); r.relocationCell = text(a, "relocationCell");
                r.homeX = real(a, "homeX"); r.homeY = real(a, "homeY");
                r.relocationX = real(a, "relocationX"); r.relocationY = real(a, "relocationY");
            }
            s.residents[id] = r;
        }
    const auto* ledger = o.find("ledger");
    if (!ledger || !ledger->isArray() || ledger->size() > 128)
        valid = false;
    else
        for (const auto& a : ledger->items())
        {
            if (!a.isObject() || a.size() != 8) valid = false;
            EconomyEntry e;
            e.sequence = integer(a, "sequence", 1e12); e.day = integer(a, "day", 365000000);
            e.coins = integer(a, "coins", 1e9); e.quantity = int(integer(a, "quantity", 99));
            e.kind = text(a, "kind"); e.from = text(a, "from"); e.to = text(a, "to"); e.item = text(a, "item");
            s.ledger.push_back(e);
        }
    s.careers = readCareers(o["careers"]);
    if (!valid)
        s.minted = -1;
    return s;
}
} // namespace ratw::wire
