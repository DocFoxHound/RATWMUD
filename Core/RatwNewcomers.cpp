#include "RatwNewcomers.h"
#include "RatwJsonDoc.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace ratw::newcomers
{
namespace
{
std::filesystem::path newcomersFile()
{
    // Data/Social: RATW_DATA_DIR, else the working directory or one above it, else the source tree.
    namespace fs = std::filesystem;
    std::error_code ec;
    if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
        return fs::path(dir) / "Social" / "newcomers.json";
    for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
    {
        if (fs::exists(at / "Data" / "Social" / "newcomers.json", ec))
            return at / "Data" / "Social" / "newcomers.json";
        if (at == at.parent_path())
            break;
    }
#ifdef RATW_SOURCE_DIR
    return fs::path(RATW_SOURCE_DIR) / "Data" / "Social" / "newcomers.json";
#else
    return fs::path("Data") / "Social" / "newcomers.json";
#endif
}

std::filesystem::path tiesFile()
{
    namespace fs = std::filesystem;
    return newcomersFile().parent_path() / "ties.json";
}

TieRules buildTies()
{
    std::ifstream in(tiesFile());
    std::stringstream text;
    text << in.rdbuf();
    if (!in)
        std::cerr << "[warn] RATW_TIES no Data/Social/ties.json: no story starters\n";
    return parseTies(text.str());
}

MatchRules buildMatch()
{
    std::ifstream in(newcomersFile().parent_path() / "matchmaking.json");
    std::stringstream text;
    text << in.rdbuf();
    if (!in)
        std::cerr << "[warn] RATW_MATCHMAKING no Data/Social/matchmaking.json: residents point no one at anyone\n";
    return parseMatch(text.str());
}

Rules build()
{
    std::ifstream in(newcomersFile());
    std::stringstream text;
    text << in.rdbuf();
    if (!in)
        std::cerr << "[warn] RATW_NEWCOMERS no Data/Social/newcomers.json: every new character arrives at the spawn\n";
    return parse(text.str());
}
} // namespace

Rules parse(const std::string& text)
{
    Rules r;
    json::Value doc;
    std::string error;
    if (text.empty() || !json::parse(text, doc, error) || !doc.isObject())
        return r;
    for (const auto& s : doc.array("starts"))
    {
        StartTown t;
        t.id = s.string("id");
        t.name = s.string("name", t.id);
        t.line = s.string("line");
        t.cell = s.string("cell");
        t.x = s.number("x", 0);
        t.y = s.number("y", 0);
        if (!t.id.empty())
            r.starts.push_back(t);
    }
    const auto& n = doc.object("newcomer");
    r.hours = n.number("hours", r.hours);
    r.socialLevel = int(n.number("socialLevel", r.socialLevel));
    const auto& s = doc.object("sampling");
    r.sampleEvery = std::max(1.0, s.number("every", r.sampleEvery));
    r.window = std::max(1, int(s.number("window", r.window)));
    r.activeWithin = s.number("activeWithin", r.activeWithin);
    const auto& m = doc.object("mentors");
    r.mentorLevel = int(m.number("socialLevel", r.mentorLevel));
    r.mentorReportDays = int(m.number("reportDays", r.mentorReportDays));
    const auto& ev = doc.object("evenings");
    r.eveningFrom = ev.number("from", r.eveningFrom);
    r.eveningTo = ev.number("to", r.eveningTo);
    r.eveningTries = int(ev.number("tries", r.eveningTries));
    r.eveningLines = int(ev.number("lines", r.eveningLines));
    r.regular = ev.number("regular", r.regular);
    const auto& v = doc.object("vouching");
    auto& w = r.vouching;
    w.trust = v.number("trust", w.trust);
    w.liking = v.number("liking", w.liking);
    w.distrust = v.number("distrust", w.distrust);
    w.trustShare = v.number("trustShare", w.trustShare);
    w.trustMost = v.number("trustMost", w.trustMost);
    w.likingShare = v.number("likingShare", w.likingShare);
    w.likingMost = v.number("likingMost", w.likingMost);
    w.familiarity = v.number("familiarity", w.familiarity);
    w.perVoucher = int(v.number("perVoucher", w.perVoucher));
    w.days = v.number("days", w.days);
    w.householdShare = v.number("householdShare", w.householdShare);
    w.householdMost = int(v.number("householdMost", w.householdMost));
    w.hitTrust = v.number("hitTrust", w.hitTrust);
    w.hitLiking = v.number("hitLiking", w.hitLiking);
    return r;
}

const Rules& rules()
{
    static const Rules r = build();
    return r;
}

void Counts::add(const std::map<std::string, int>& counts, const std::vector<std::string>& towns, int window)
{
    for (const auto& town : towns)
    {
        auto& list = samples_[town];
        const auto it = counts.find(town);
        list.push_back(it == counts.end() ? 0 : it->second);
        while (int(list.size()) > window)
            list.pop_front();
    }
}

double Counts::mean(const std::string& town) const
{
    const auto it = samples_.find(town);
    if (it == samples_.end() || it->second.empty())
        return 0;
    double sum = 0;
    for (const int n : it->second)
        sum += n;
    return sum / double(it->second.size());
}

std::string busiest(const std::vector<std::string>& towns, const Counts& counts)
{
    std::string best;
    double most = -1;
    for (const auto& town : towns)
        if (const double m = counts.mean(town); m > most + 1e-9)
            best = town, most = m;
    return best;
}

bool mayMentor(const MentorCheck& m, std::string& why, const Rules& r)
{
    why.clear();
    if (m.revoked)
        why = "A Dungeon Master has turned mentoring off for your account.";
    else if (m.silenced)
        why = "Not while your account is silenced.";
    else if (m.newcomer)
        why = "You are new to these parts yourself.";
    else if (m.socialLevel < r.mentorLevel)
        why = "Mentors are social level " + std::to_string(r.mentorLevel) + " or more; your account is " + std::to_string(m.socialLevel) + ".";
    else if (m.upheldReports > 0)
        why = "Not with a report upheld against your account in the last " + std::to_string(r.mentorReportDays) + " days.";
    return why.empty();
}

bool graduates(double playedSeconds, int socialLevel, const Rules& r)
{
    return playedSeconds >= r.hours * 3600 || socialLevel >= r.socialLevel;
}

// ------------------------------------------------------------------ Vouching

VouchShare vouchShare(double trustInVoucher, double likingVoucher, const Rules& r)
{
    const auto& w = r.vouching;
    return {std::min(w.trustMost, std::max(0.0, trustInVoucher) * w.trustShare), std::min(w.likingMost, std::max(0.0, likingVoucher) * w.likingShare),
            w.familiarity};
}

bool mayVouch(double trustInVoucher, double likingVoucher, double trustInVouched, int activeVouches, bool already, std::string& why,
              const Rules& r)
{
    const auto& w = r.vouching;
    why.clear();
    if (trustInVoucher < w.trust || likingVoucher < w.liking)
        why = "They don't think well enough of you yet to take your word for someone.";
    else if (trustInVouched <= w.distrust)
        why = "They distrust that wolf too much to take anyone's word for them.";
    else if (already)
        why = "You have vouched for that wolf to them already.";
    else if (activeVouches >= w.perVoucher)
        why = "You stand for " + std::to_string(w.perVoucher) + " wolves already; that's as many as anyone's word will carry.";
    return why.empty();
}

json::Value saveVouch(const Vouch& v)
{
    auto o = json::Value::object();
    o.add("id", v.id);
    o.add("resident", v.resident);
    o.add("voucher", v.voucher);
    o.add("vouched", v.vouched);
    o.add("state", v.state);
    o.add("at", v.at);
    o.add("day", v.day);
    o.add("until", v.until);
    o.add("trust", v.given.trust);
    o.add("liking", v.given.liking);
    o.add("familiarity", v.given.familiarity);
    auto house = json::Value::array();
    for (const auto& g : v.household)
    {
        auto h = json::Value::object();
        h.add("id", g.id);
        h.add("trust", g.trust);
        h.add("liking", g.liking);
        h.add("familiarity", g.familiarity);
        house.push(h);
    }
    o.add("household", house);
    o.add("raised", v.raised);
    return o;
}

Vouch loadVouch(const json::Value& o)
{
    Vouch v;
    v.id = o.string("id").substr(0, 80);
    v.resident = o.string("resident").substr(0, 80);
    v.voucher = o.string("voucher").substr(0, 80);
    v.vouched = o.string("vouched").substr(0, 80);
    v.state = o.string("state", "active");
    if (v.state != "active" && v.state != "ended" && v.state != "broken")
        v.state = "ended";
    v.at = o.number("at", 0);
    v.day = o.number("day", 0);
    v.until = o.number("until", 0);
    v.given = {o.number("trust", 0), o.number("liking", 0), o.number("familiarity", 0)};
    for (const auto& h : o.array("household"))
        if (v.household.size() < 64)
            v.household.push_back({h.string("id").substr(0, 80), h.number("trust", 0), h.number("liking", 0), h.number("familiarity", 0)});
    v.raised = o.boolean("raised", false);
    return v;
}

// ------------------------------------------------------------------ Ties

TieRules parseTies(const std::string& text)
{
    TieRules r;
    json::Value doc;
    std::string error;
    if (text.empty() || !json::parse(text, doc, error) || !doc.isObject())
        return r;
    r.offerSeconds = std::max(5.0, doc.number("offerSeconds", r.offerSeconds));
    r.lapseDays = doc.number("lapseDays", r.lapseDays);
    r.lapseScenes = int(doc.number("lapseScenes", r.lapseScenes));
    r.restSeconds = doc.number("restSeconds", r.restSeconds);
    r.markerSeconds = doc.number("markerSeconds", r.markerSeconds);
    r.nearCells = int(doc.number("nearCells", r.nearCells));
    for (const auto& o : doc.array("starters"))
    {
        Starter s;
        s.id = o.string("id");
        s.newcomer = o.string("newcomer");
        s.other = o.string("other");
        if (s.id.empty() || s.newcomer.empty() || s.other.empty())
            continue;
        s.mentor = o.boolean("mentor", true);
        s.names = o.boolean("names", false);
        s.fallback = o.boolean("fallback", false);
        const auto& res = o.object("resident");
        for (const auto& j : res.array("jobs"))
            if (j.isString())
                s.jobs.push_back(j.asString({}));
        s.minAge = int(res.number("minAge", s.minAge));
        s.maxAge = int(res.number("maxAge", s.maxAge));
        s.need = res.string("need");
        const auto& b = o.object("bond");
        s.affinity = b.number("affinity", 0);
        s.trust = b.number("trust", 0);
        s.familiarity = b.number("familiarity", 0);
        s.respect = b.number("respect", 0);
        s.owed = std::int64_t(o.number("owed", 0));
        r.starters.push_back(s);
    }
    return r;
}

const TieRules& tieRules()
{
    static const TieRules r = buildTies();
    return r;
}

const Starter* starter(const std::string& id, const TieRules& r)
{
    for (const auto& s : r.starters)
        if (s.id == id)
            return &s;
    return nullptr;
}

const Starter* fallbackStarter(const TieRules& r)
{
    for (const auto& s : r.starters)
        if (s.fallback)
            return &s;
    return r.starters.empty() ? nullptr : &r.starters.back();
}

json::Value saveTie(const Tie& t)
{
    auto o = json::Value::object();
    o.add("id", t.id);
    o.add("newcomer", t.newcomer);
    o.add("account", t.account);
    o.add("other", t.other);
    o.add("otherAccount", t.otherAccount);
    o.add("starter", t.starter);
    o.add("state", t.state);
    o.add("town", t.town);
    o.add("arrivalCell", t.arrivalCell);
    o.add("offeredTo", t.offeredTo);
    o.add("offerUntil", t.offerUntil);
    auto asked = json::Value::array();
    for (const auto& a : t.asked)
        asked.push(a);
    o.add("asked", asked);
    o.add("mentorsDone", t.mentorsDone);
    o.add("resident", t.resident);
    o.add("told", t.told);
    o.add("otherTold", t.otherTold);
    o.add("created", t.created);
    o.add("made", t.made);
    o.add("lapsesAt", t.lapsesAt);
    o.add("ended", t.ended);
    o.add("scenesAtStart", t.scenesAtStart);
    o.add("spotCell", t.spotCell);
    o.add("spotPlace", t.spotPlace);
    o.add("spotX", t.spotX);
    o.add("spotY", t.spotY);
    o.add("markerUntil", t.markerUntil);
    return o;
}

Tie loadTie(const json::Value& o)
{
    Tie t;
    t.id = o.string("id").substr(0, 80);
    t.newcomer = o.string("newcomer").substr(0, 80);
    t.account = o.string("account").substr(0, 80);
    t.other = o.string("other").substr(0, 80);
    t.otherAccount = o.string("otherAccount").substr(0, 80);
    t.starter = o.string("starter").substr(0, 40);
    t.state = o.string("state", "seeking");
    if (t.state != "seeking" && t.state != "offered" && t.state != "active" && t.state != "lapsed" && t.state != "ended")
        t.state = "ended";
    t.town = o.string("town").substr(0, 80);
    t.arrivalCell = o.string("arrivalCell").substr(0, 80);
    t.offeredTo = o.string("offeredTo").substr(0, 80);
    t.offerUntil = o.number("offerUntil", 0);
    for (const auto& a : o.array("asked"))
        if (a.isString() && t.asked.size() < 200)
            t.asked.push_back(a.asString({}).substr(0, 80));
    t.mentorsDone = o.boolean("mentorsDone", false);
    t.resident = o.boolean("resident", false);
    t.told = o.boolean("told", false);
    t.otherTold = o.boolean("otherTold", false);
    t.created = o.number("created", 0);
    t.made = o.number("made", 0);
    t.lapsesAt = o.number("lapsesAt", 0);
    t.ended = o.number("ended", 0);
    t.scenesAtStart = int(o.number("scenesAtStart", 0));
    t.spotCell = o.string("spotCell").substr(0, 80);
    t.spotPlace = o.string("spotPlace").substr(0, 120);
    t.spotX = o.number("spotX", 0);
    t.spotY = o.number("spotY", 0);
    t.markerUntil = o.number("markerUntil", 0);
    return t;
}

std::vector<MentorCandidate> mentorOrder(std::vector<MentorCandidate> candidates, std::uint32_t seed)
{
    const auto mix = [seed](const std::string& s) {
        std::uint32_t h = 2166136261u ^ seed;
        for (const unsigned char ch : s)
            h = (h ^ ch) * 16777619u;
        return h;
    };
    std::sort(candidates.begin(), candidates.end(), [&](const MentorCandidate& a, const MentorCandidate& b) {
        if (a.lastTieAt != b.lastTieAt)
            return a.lastTieAt < b.lastTieAt;       // (Never tied, -1, comes first of all.)
        return mix(a.account) < mix(b.account);
    });
    return candidates;
}

bool residentFits(const Starter& s, const std::string& job, int age, bool apprenticePlaceFree)
{
    if (age < s.minAge || age > s.maxAge || job == "child")
        return false;
    if (!s.jobs.empty() && std::find(s.jobs.begin(), s.jobs.end(), job) == s.jobs.end())
        return false;
    return s.need != "apprentice" || apprenticePlaceFree;
}

// ------------------------------------------------------------------ Matchmakers

MatchRules parseMatch(const std::string& text)
{
    MatchRules r;
    json::Value doc;
    std::string error;
    if (text.empty() || !json::parse(text, doc, error) || !doc.isObject())
    {
        r.matchmakers.clear();                      // (No rules: no one matchmakes.)
        return r;
    }
    const auto strings = [](const json::Value& list) {
        std::vector<std::string> out;
        for (const auto& v : list.items())
            if (v.isString())
                out.push_back(v.asString({}));
        return out;
    };
    if (const auto* m = doc.find("matchmakers"); m && m->isArray())
        r.matchmakers = strings(*m);
    if (const auto* o = doc.find("order"); o && o->isArray())
        r.order = strings(*o);
    r.marketMerchant = doc.boolean("marketMerchant", r.marketMerchant);
    r.knowsFamiliarity = doc.number("knowsFamiliarity", r.knowsFamiliarity);
    r.wellKnownScenes = int(doc.number("wellKnownScenes", r.wellKnownScenes));
    r.perPlayerHours = std::max(1, int(doc.number("perPlayerHours", r.perPlayerHours)));
    r.perMatchmakerHour = int(doc.number("perMatchmakerHour", r.perMatchmakerHour));
    r.pointedPerHour = int(doc.number("pointedPerHour", r.pointedPerHour));
    r.sharedRootsDays = doc.number("sharedRootsDays", r.sharedRootsDays);
    for (const auto& [key, v] : doc.object("reasons").fields())
        if (v.isString())
            r.reasons[key] = v.asString({});
    for (const auto& a : doc.array("asks"))
    {
        Ask ask;
        ask.id = a.string("id");
        ask.resident = a.string("resident");
        ask.player = a.string("player");
        ask.meets = a.string("meets");
        if (const auto* p = a.find("patterns"); p && p->isArray())
            ask.patterns = strings(*p);
        if (const auto* j = a.find("jobs"); j && j->isArray())
            ask.jobs = strings(*j);
        if (!ask.id.empty() && !ask.patterns.empty())
            r.asks.push_back(ask);
    }
    r.briefing = doc.string("briefing");
    r.line = doc.string("line");
    r.quiet = doc.string("quiet");
    r.welcome = doc.string("welcome");
    r.pointOut = doc.string("pointOut");
    r.promptPointed = doc.string("promptPointed");
    r.promptNewcomer = doc.string("promptNewcomer");
    for (const auto& [key, v] : doc.object("facts").fields())
        if (v.isString())
            r.facts[key] = v.asString({});
    return r;
}

const MatchRules& matchRules()
{
    static const MatchRules r = buildMatch();
    return r;
}

const Ask* askIn(const std::string& normalised, const MatchRules& r)
{
    for (const auto& a : r.asks)
        for (const auto& p : a.patterns)
            if (!p.empty() && (" " + normalised + " ").find(" " + p + " ") != std::string::npos)
                return &a;
    return nullptr;
}

Pairing pickPairing(const MatchSide& a, const std::vector<MatchCandidate>& candidates, std::uint32_t seed, const MatchRules& r)
{
    for (const auto& reason : r.order)
    {
        std::vector<std::pair<std::string, std::string>> fits;   // (id, the reason's words key)
        for (const auto& c : candidates)
        {
            if (c.excluded)
                continue;
            std::string why;
            if (reason == "newcomer" && c.player)
                why = a.newcomer && c.side.helper ? "newcomerToHelper" : c.side.newcomer && a.helper ? "helperToNewcomer" : "";
            else if (reason == "tie" && c.tiedUnmet)
                why = "tie";
            else if (reason == "looking" && c.player && a.looking && c.side.looking)
                why = "looking";
            else if (reason == "need" && c.meetsAsk)
                why = "need";
            else if (reason == "roots" && c.player && c.sameRoots)
                why = "roots";
            if (!why.empty())
                fits.push_back({c.id, why});
        }
        if (!fits.empty())
        {
            std::sort(fits.begin(), fits.end());
            const auto& chosen = fits[seed % fits.size()];
            return {chosen.first, chosen.second};
        }
    }
    return {};
}

std::string fill(std::string text, const std::map<std::string, std::string>& blanks)
{
    for (const auto& [key, value] : blanks)
        for (std::size_t at; (at = text.find("{" + key + "}")) != std::string::npos;)
            text.replace(at, key.size() + 2, value);
    return text;
}

bool lapsed(const Tie& t, int scenesShared, double now, const TieRules& r)
{
    return t.state == "active" && (now >= t.made + r.lapseDays * 86400 || scenesShared >= r.lapseScenes);
}
} // namespace ratw::newcomers
