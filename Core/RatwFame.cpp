// Fame's record (Docs/Design/56-fame-and-memory.md, 1): see RatwFame.h.
#include "RatwFame.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace ratw::fame
{
using json::Value;

namespace
{
const char* const Weights[] = {"small", "notable", "great", "legendary"};

std::filesystem::path deedsFile()
{
    // Data/Fame: RATW_DATA_DIR, else the working directory or one above it, else the source tree.
    namespace fs = std::filesystem;
    std::error_code ec;
    if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
        return fs::path(dir) / "Fame" / "deeds.json";
    for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
    {
        if (fs::exists(at / "Data" / "Fame" / "deeds.json", ec))
            return at / "Data" / "Fame" / "deeds.json";
        if (at == at.parent_path())
            break;
    }
#ifdef RATW_SOURCE_DIR
    return fs::path(RATW_SOURCE_DIR) / "Data" / "Fame" / "deeds.json";
#else
    return fs::path("Data") / "Fame" / "deeds.json";
#endif
}

std::string replaced(std::string text, const std::string& blank, const std::string& with)
{
    for (auto at = text.find(blank); at != std::string::npos; at = text.find(blank, at + with.size()))
        text.replace(at, blank.size(), with);
    return text;
}

Value strings(const std::vector<std::string>& list)
{
    auto a = Value::array();
    for (const auto& s : list)
        a.push(s);
    return a;
}
std::vector<std::string> strings(const Value& a, std::size_t most)
{
    std::vector<std::string> out;
    for (const auto& s : a.items())
        if (s.isString() && out.size() < most)
            out.push_back(s.asString().substr(0, 128));
    return out;
}
} // namespace

int weightOf(const std::string& name)
{
    for (int i = 0; i < 4; ++i)
        if (name == Weights[i])
            return i;
    return -1;
}

const char* weightName(int weight) { return Weights[std::clamp(weight, 0, 3)]; }

Rules parseRules(const std::string& text)
{
    Rules r;
    Value doc;
    std::string error;
    if (text.empty() || !json::parse(text, doc, error) || !doc.isObject())
        return r;
    for (const auto& k : doc.array("kinds"))
    {
        Kind kind;
        kind.id = k.string("id");
        kind.family = k.string("family");
        kind.phrase = k.string("phrase");
        kind.weight = std::max(0, weightOf(k.string("weight", "small")));
        kind.perSeason = int(k.number("perSeason", 0));
        if (!kind.id.empty() && !kind.phrase.empty())
            r.kinds.push_back(kind);
    }
    const auto& live = doc.object("liveDays");
    for (int i = 0; i < 4; ++i)
        r.liveDays[i] = live.number(Weights[i], r.liveDays[i]);
    r.witnessesMost = std::clamp(int(doc.number("witnessesMost", r.witnessesMost)), 1, 16);
    const auto& word = doc.object("word");
    r.wordStart = word.number("start", r.wordStart);
    r.wordDays = std::max(.01, word.number("days", r.wordDays));
    r.caravanStart = word.number("caravanStart", r.caravanStart);
    r.legendStart = word.number("legendStart", r.legendStart);
    for (int i = 1; i < 3; ++i)
    {
        r.fresh[i] = word.object("fresh").number(Weights[i], r.fresh[i]);
        r.fade[i] = std::max(.01, word.object("fade").number(Weights[i], r.fade[i]));
    }
    r.townMost = std::max(1, int(word.number("most", r.townMost)));
    const auto& ears = word.object("ears");
    r.earMerchant = ears.number("merchant", r.earMerchant);
    r.earGuard = ears.number("guard", r.earGuard);
    r.earClergy = ears.number("clergy", r.earClergy);
    r.earChild = ears.number("child", r.earChild);
    r.greetEveryDays = doc.number("greetEveryDays", r.greetEveryDays);
    r.warmth = doc.number("warmth", r.warmth);
    return r;
}

const Rules& rules()
{
    static const Rules r = [] {
        std::ifstream in(deedsFile());
        std::stringstream text;
        text << in.rdbuf();
        if (!in)
            std::cerr << "[warn] RATW_FAME no Data/Fame/deeds.json: no kinds of deed\n";
        return parseRules(text.str());
    }();
    return r;
}

json::Value dataFile(const std::string& name)
{
    std::ifstream in(deedsFile().parent_path() / name);
    std::stringstream text;
    text << in.rdbuf();
    Value doc;
    std::string error;
    if (!in || !json::parse(text.str(), doc, error))
        return {};
    return doc;
}

std::filesystem::path nicknamesFile()
{
    return deedsFile().parent_path() / "nicknames.json";
}

NicknameRules parseNicknames(const std::string& text)
{
    NicknameRules r;
    Value doc;
    std::string error;
    if (text.empty() || !json::parse(text, doc, error) || !doc.isObject())
        return r;
    const auto list = [](const Value& a) {
        std::vector<std::string> out;
        for (const auto& s : a.items())
            if (s.isString() && !s.asString().empty())
                out.push_back(s.asString());
        return out;
    };
    for (const auto& [family, forms] : doc.object("families").fields())
        r.families[family] = {list(forms["epithets"]), list(forms["possessive"]), list(forms["deed"])};
    r.smallOfAFamily = std::max(1, int(doc.object("when").number("smallOfAFamily", r.smallOfAFamily)));
    r.withinDays = doc.object("when").number("withinDays", r.withinDays);
    r.affinity = doc.object("coiner").number("affinity", r.affinity);
    r.familiarity = doc.object("coiner").number("familiarity", r.familiarity);
    r.innkeeperAtReach = doc.object("coiner").number("innkeeperAtReach", r.innkeeperAtReach);
    r.most = std::max(1, int(doc.number("most", r.most)));
    return r;
}

const NicknameRules& nicknameRules()
{
    static const NicknameRules r = [] {
        std::ifstream in(nicknamesFile());
        std::stringstream text;
        text << in.rdbuf();
        if (!in)
            std::cerr << "[warn] RATW_FAME no Data/Fame/nicknames.json: no nicknames\n";
        return parseNicknames(text.str());
    }();
    return r;
}

std::vector<std::string> nicknameForms(const std::string& family, const std::string& name, const std::string& deedPhrase)
{
    std::vector<std::string> out;
    const auto found = nicknameRules().families.find(family);
    if (found == nicknameRules().families.end())
        return out;
    const auto& f = found->second;
    out.insert(out.end(), f.epithets.begin(), f.epithets.end());
    if (!name.empty())
        for (const auto& p : f.possessive)
            out.push_back(replaced(p, "{name}", name));
    for (const auto& d : f.deed)
        out.push_back(replaced(d, "{deed}", deedPhrase));
    return out;
}

const Kind* kind(const std::string& id)
{
    for (const auto& k : rules().kinds)
        if (k.id == id)
            return &k;
    return nullptr;
}

double reach(const TownWord& w, int weight, double now, const Rules& r)
{
    const bool caravan = w.carrier.rfind("caravan", 0) == 0, legend = w.carrier == "legend";
    const double start = legend ? r.legendStart : caravan ? r.caravanStart : r.wordStart;
    const double days = caravan ? r.wordDays * 2 : r.wordDays;   // (A caravan's word grows at half speed.)
    const double age = std::max(0., now - w.since);
    double grown = std::min(1., start + (1 - start) * age / days);
    const int k = std::clamp(weight, 0, 3);
    if (k == Legendary || r.fresh[k] < 0)
        return grown;
    if (age > r.fresh[k])
        grown *= std::max(0., 1 - (age - r.fresh[k]) / r.fade[k]);
    return grown;
}

std::string phrase(const Deed& d, const std::string& beneficiary)
{
    const auto* k = kind(d.kind);
    std::string text = k ? k->phrase : d.detail;
    text = replaced(text, "{beneficiary}", beneficiary.empty() ? "someone" : beneficiary);
    text = replaced(text, "{place}", d.place.empty() ? "somewhere" : d.place);
    return replaced(text, "{detail}", d.detail.empty() ? "a good turn" : d.detail);
}

Value toJson(const Deed& d)
{
    auto j = Value::object();
    j.add("id", d.id);
    j.add("kind", d.kind);
    j.add("weight", d.weight);
    j.add("doers", strings(d.doers));
    j.add("beneficiary", d.beneficiary);
    j.add("town", d.town);
    j.add("cell", d.cell);
    j.add("place", d.place);
    j.add("detail", d.detail);
    j.add("source", d.source);
    j.add("x", d.x);
    j.add("y", d.y);
    j.add("day", d.day);
    auto witnesses = Value::array();
    for (const auto& w : d.witnesses)
    {
        auto o = Value::object();
        o.add("id", w.id);
        auto as = Value::object();
        for (const auto& [doer, name] : w.as)
            as.add(doer, name);
        o.add("as", as);
        witnesses.push(o);
    }
    j.add("witnesses", witnesses);
    auto looks = Value::object();
    for (const auto& [doer, look] : d.looks)
        looks.add(doer, look);
    j.add("looks", looks);
    auto names = Value::object();
    for (const auto& [doer, list] : d.names)
        names.add(doer, strings(list));
    j.add("names", names);
    auto towns = Value::array();
    for (const auto& t : d.towns)
    {
        auto o = Value::object();
        o.add("town", t.town);
        o.add("carrier", t.carrier);
        o.add("since", t.since);
        o.add("reach", t.reach);
        towns.push(o);
    }
    j.add("towns", towns);
    j.add("warmed", strings(d.warmed));
    if (!d.nickname.empty())
        j.add("nickname", d.nickname);
    if (d.noNickname)
        j.add("noNickname", true);
    if (d.cried)
        j.add("cried", true);
    if (d.revoked)
        j.add("revoked", true);
    return j;
}

Deed fromJson(const Value& j)
{
    Deed d;
    d.id = j.string("id");
    d.kind = j.string("kind");
    d.weight = std::clamp(int(j.number("weight")), 0, 3);
    d.doers = strings(j["doers"], 6);
    d.beneficiary = j.string("beneficiary");
    d.town = j.string("town");
    d.cell = j.string("cell");
    d.place = j.string("place");
    d.detail = j.string("detail").substr(0, 200);
    d.source = j.string("source");
    d.x = j.number("x");
    d.y = j.number("y");
    d.day = j.number("day");
    for (const auto& o : j.array("witnesses"))
    {
        Witness w;
        w.id = o.string("id");
        for (const auto& [doer, name] : o.object("as").fields())
            if (name.isString())
                w.as[doer] = name.asString().substr(0, 64);
        if (!w.id.empty() && d.witnesses.size() < 16)
            d.witnesses.push_back(std::move(w));
    }
    for (const auto& [doer, look] : j.object("looks").fields())
        if (look.isString())
            d.looks[doer] = look.asString().substr(0, 128);
    for (const auto& [doer, list] : j.object("names").fields())
        d.names[doer] = strings(list, 4);
    for (const auto& o : j.array("towns"))
        if (!o.string("town").empty() && d.towns.size() < 64)
            d.towns.push_back({o.string("town"), o.string("carrier"), o.number("since"), std::clamp(o.number("reach"), 0., 1.)});
    d.warmed = strings(j["warmed"], 5000);
    d.nickname = j.string("nickname");
    d.noNickname = j.boolean("noNickname");
    d.cried = j.boolean("cried");
    d.revoked = j.boolean("revoked");
    return d;
}

void Ledger::index(const Deed& d)
{
    for (const auto& doer : d.doers)
        byDoer_[doer].push_back(d.id);
}

Deed& Ledger::record(Deed d)
{
    d.id = "deed-" + std::to_string(next_++);
    index(d);
    return deeds_[d.id] = std::move(d);
}

Deed* Ledger::find(const std::string& id)
{
    const auto found = deeds_.find(id);
    return found == deeds_.end() ? nullptr : &found->second;
}

const Deed* Ledger::find(const std::string& id) const
{
    const auto found = deeds_.find(id);
    return found == deeds_.end() ? nullptr : &found->second;
}

bool Ledger::revoke(const std::string& id)
{
    auto* d = find(id);
    if (!d || d->revoked)
        return false;
    d->revoked = true;
    d->towns.clear();
    return true;
}

std::vector<const Deed*> Ledger::byDoer(const std::string& doer) const
{
    std::vector<const Deed*> out;
    if (const auto found = byDoer_.find(doer); found != byDoer_.end())
        for (const auto& id : found->second)
            if (const auto* d = find(id); d && !d->revoked)
                out.push_back(d);
    std::sort(out.begin(), out.end(), [](const Deed* a, const Deed* b) { return a->day > b->day; });
    return out;
}

bool Ledger::prune(double today)
{
    bool any = false;
    for (auto it = deeds_.begin(); it != deeds_.end();)
    {
        const double life = rules().liveDays[std::clamp(it->second.weight, 0, 3)];
        const bool gone = (it->second.revoked && today - it->second.day > 92) || (life >= 0 && today - it->second.day > life);
        if (!gone)
        {
            ++it;
            continue;
        }
        for (const auto& doer : it->second.doers)
        {
            auto& list = byDoer_[doer];
            list.erase(std::remove(list.begin(), list.end(), it->first), list.end());
        }
        it = deeds_.erase(it);
        any = true;
    }
    return any;
}

Nickname& Ledger::coin(Nickname n)
{
    n.id = "nick-" + std::to_string(nextNickname_++);
    return nicknames_[n.id] = std::move(n);
}

Nickname* Ledger::nickname(const std::string& id)
{
    const auto found = nicknames_.find(id);
    return found == nicknames_.end() ? nullptr : &found->second;
}

std::vector<const Nickname*> Ledger::nicknamesOf(const std::string& wolf) const
{
    std::vector<const Nickname*> out;
    for (const auto& [id, n] : nicknames_)
        if (n.wolf == wolf)
            out.push_back(&n);
    std::sort(out.begin(), out.end(), [](const Nickname* a, const Nickname* b) { return a->day > b->day; });
    return out;
}

Value Ledger::saveNicknames() const
{
    auto list = Value::array();
    for (const auto& [id, n] : nicknames_)
    {
        auto j = Value::object();
        j.add("id", n.id);
        j.add("wolf", n.wolf);
        j.add("text", n.text);
        j.add("family", n.family);
        j.add("deed", n.deed);
        j.add("coinedBy", n.coinedBy);
        j.add("town", n.town);
        j.add("day", n.day);
        j.add("dropped", n.dropped);
        list.push(j);
    }
    return list;
}

void Ledger::loadNicknames(const Value& list)
{
    nicknames_.clear();
    for (const auto& j : list.items())
    {
        Nickname n{j.string("id"), j.string("wolf"), j.string("text").substr(0, 120), j.string("family"), j.string("deed"),
                   j.string("coinedBy"), j.string("town"), j.number("day"), j.boolean("dropped")};
        if (n.id.rfind("nick-", 0) != 0 || n.wolf.empty() || n.text.empty())
            continue;
        nextNickname_ = std::max<std::uint64_t>(nextNickname_, std::strtoull(n.id.c_str() + 5, nullptr, 10) + 1);
        nicknames_[n.id] = std::move(n);
    }
}

Value Ledger::save() const
{
    auto list = Value::array();
    for (const auto& [id, d] : deeds_)
        list.push(toJson(d));
    return list;
}

void Ledger::load(const Value& list)
{
    deeds_.clear();
    byDoer_.clear();
    for (const auto& j : list.items())
    {
        auto d = fromJson(j);
        if (d.id.rfind("deed-", 0) != 0 || d.doers.empty())
            continue;
        next_ = std::max<std::uint64_t>(next_, std::strtoull(d.id.c_str() + 5, nullptr, 10) + 1);
        index(d);
        deeds_[d.id] = std::move(d);
    }
}
} // namespace ratw::fame
